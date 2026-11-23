// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Environment/TankTrails/TankTrailSubsystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsEvasion, Log, All);

namespace RTSShortsEvasionPrivate
{
	constexpr int32 CycleEveryFrames = 6;
	// Hull within this many degrees of the threat counts as facing it.
	constexpr float FacingToleranceDegrees = 12.f;
	constexpr float TurnOrderEverySeconds = 1.5f;
	constexpr float StationarySpeedCm = 40.f;
	// A nearby flanker is more urgent than a far one at the same angle; this much score at point-blank range.
	constexpr float ClosenessScore = 40.f;
	constexpr float ClosenessRangeCm = 4000.f;
	// An attacker the unit has turned this close to facing goes round it again, this far off the unit's nose.
	constexpr float FacedByUnitDegrees = 65.f;
	constexpr float ReflankBearingDegrees = 110.f;
	constexpr float ReflankArrivedCm = 500.f;
	constexpr float ReflankTimeoutSeconds = 9.f;
	constexpr float ReflankStaggerSeconds = 0.7f;
	// Directions to the attackers sum to less than this when they stand all round the unit.
	constexpr float SurroundedSpread = 0.5f;
	// A team of defenders spreads its fire (user 2026-11-13): an attacker another living defender already has as its
	// threat scores this much less per such defender, so two only share a target that is that much more dangerous, or
	// when there are fewer attackers than defenders.
	constexpr float SharedThreatPenalty = 90.f;
	// User 2026-11-15: against six IS-1s the E-75s drove forward instead of firing. Their threats were the outermost
	// attackers, beyond gun range, and a turret told to engage a target out of range has its tank drive into range. An
	// attacker within this share of the defender's gun range outscores every one outside it, and the turret is only
	// put on a threat within range (otherwise it auto-engages whatever is in range, which never drives the tank).
	constexpr float InRangeShare = 0.98f;
	constexpr float InRangeBonus = 1000.f;

	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}

	float GetDegreesOffNose(const AActor& Unit, const AActor& Other)
	{
		const FVector Forward = Unit.GetActorForwardVector().GetSafeNormal2D();
		const FVector ToOther = (Other.GetActorLocation() - Unit.GetActorLocation()).GetSafeNormal2D();
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, ToOther), -1.f, 1.f)));
	}

	/** The attacker furthest round the unit's side is the one aiming at thinner armour; ties go to the nearer one. */
	float GetFlankScore(const AActor& Unit, const AActor& Attacker)
	{
		const float Distance = FVector::Dist2D(Unit.GetActorLocation(), Attacker.GetActorLocation());
		return GetDegreesOffNose(Unit, Attacker) + (1.f - FMath::Clamp(Distance / ClosenessRangeCm, 0.f, 1.f)) * ClosenessScore;
	}

	bool GetIsWithinRange(const AActor& Unit, const AActor& Target, const float RangeCm)
	{
		return RangeCm <= 0.f || FVector::Dist2D(Unit.GetActorLocation(), Target.GetActorLocation()) <= RangeCm * InRangeShare;
	}

	/** Turrets still on a target beyond their reach go back to auto-engage, which holds fire instead of driving. */
	void ReleaseOutOfRangeTurrets(const AActor* Unit, const float RangeCm)
	{
		const ATankMaster* Tank = Cast<ATankMaster>(Unit);
		if (Tank == nullptr)
		{
			return;
		}
		for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			const AActor* Target = IsValid(Turret) ? Turret->GetCurrentTargetActor() : nullptr;
			if (IsValid(Target) && not GetIsWithinRange(*Unit, *Target, RangeCm))
			{
				Turret->SetAutoEngageTargets(false);
			}
		}
	}

	/** Backing away from the group keeps them all towards the front; when they surround it, from the threat alone. */
	FVector GetDirectionAwayFromAttackers(const AActor& Unit, const AActor& Threat, const TArray<TWeakObjectPtr<AActor>>& Attackers)
	{
		FVector TowardsAttackers = FVector::ZeroVector;
		for (const TWeakObjectPtr<AActor>& Attacker : Attackers)
		{
			if (GetIsAlive(Attacker))
			{
				TowardsAttackers += (Attacker->GetActorLocation() - Unit.GetActorLocation()).GetSafeNormal2D();
			}
		}
		if (TowardsAttackers.Size2D() < SurroundedSpread)
		{
			TowardsAttackers = Threat.GetActorLocation() - Unit.GetActorLocation();
		}
		return -TowardsAttackers.GetSafeNormal2D();
	}

	/** Every turret of the tank faced to its hull's front, idle sweeps off; how many turrets it has. */
	int32 SnapTurretsToHullFront(const AActor* Unit)
	{
		const ATankMaster* Tank = Cast<ATankMaster>(Unit);
		if (Tank == nullptr)
		{
			return 0;
		}
		int32 Turrets = 0;
		for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			if (IsValid(Turret))
			{
				Turret->SnapToHullFrontAndHoldIdle();
				++Turrets;
			}
		}
		return Turrets;
	}

	const UWeaponState* GetMainGun(const AActor* Unit)
	{
		const ATankMaster* Tank = Cast<ATankMaster>(Unit);
		if (Tank == nullptr)
		{
			return nullptr;
		}
		const UWeaponState* MainGun = nullptr;
		int32 LargestCalibre = -1;
		for (const ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			const UWeaponState* Weapon = IsValid(Turret) ? Turret->GetLargestCalibreWeapon() : nullptr;
			if (IsValid(Weapon) && Weapon->GetRawWeaponData().WeaponCalibre > LargestCalibre)
			{
				LargestCalibre = Weapon->GetRawWeaponData().WeaponCalibre;
				MainGun = Weapon;
			}
		}
		return MainGun;
	}

	void AimTurretsAt(const AActor* Unit, AActor* Target)
	{
		const ATankMaster* Tank = Cast<ATankMaster>(Unit);
		if (Tank == nullptr || not IsValid(Target))
		{
			return;
		}
		for (ACPPTurretsMaster* Turret : Tank->GetTurrets())
		{
			if (IsValid(Turret))
			{
				Turret->SetEngageSpecificTarget(Target);
			}
		}
	}
}


void ARTSShortsDirector::BeginSoloEvasion(const FRTSShortsSegment& Segment, AActor* MainUnit)
{
	if (not Segment.BattleSoloEvasion.bEnabled || Cast<ATankMaster>(MainUnit) == nullptr || not GetIsValidCamera())
	{
		return;
	}
	FRTSShortsSoloEvasionState& Evasion = M_Battle.Evasion;
	Evasion.bIsActive = true;
	// The segment's unit first, then its tank allies: a team of defenders each runs the solo vehicle's cycle.
	TArray<AActor*> Defenders = {MainUnit};
	for (const TWeakObjectPtr<AActor>& Ally : M_Battle.PlayerUnits)
	{
		if (Cast<ATankMaster>(Ally.Get()) != nullptr && Ally.Get() != MainUnit)
		{
			Defenders.Add(Ally.Get());
		}
	}
	// The round's own vehicle waited off stage since the render began and its idle sweeps could leave its gun facing
	// back (user 2026-11-15: it then had to turn 180 degrees before it could fight). Both sides start with their guns
	// on their hulls' fronts and no idle sweeps; a vehicle whose turrets are not up yet gets the same a moment later.
	TArray<TWeakObjectPtr<AActor>> LateTurrets;
	int32 Snapped = 0;
	TArray<AActor*> RoundVehicles = Defenders;
	for (const TWeakObjectPtr<AActor>& Enemy : M_Battle.EnemyUnits)
	{
		RoundVehicles.Add(Enemy.Get());
	}
	for (AActor* Vehicle : RoundVehicles)
	{
		const int32 Turrets = RTSShortsEvasionPrivate::SnapTurretsToHullFront(Vehicle);
		Snapped += Turrets;
		if (Turrets == 0 && Cast<ATankMaster>(Vehicle) != nullptr)
		{
			LateTurrets.Add(Vehicle);
		}
	}
	if (not LateTurrets.IsEmpty())
	{
		FTimerHandle LateHandle;
		GetWorld()->GetTimerManager().SetTimer(LateHandle, FTimerDelegate::CreateLambda([LateTurrets]()
		{
			for (const TWeakObjectPtr<AActor>& Vehicle : LateTurrets)
			{
				RTSShortsEvasionPrivate::SnapTurretsToHullFront(Vehicle.Get());
			}
		}), 0.3f, false);
	}
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_TURRETS_FRONT turrets=%d late=%d"), Snapped, LateTurrets.Num());
	for (AActor* Defender : Defenders)
	{
		FRTSShortsEvaderState& Evader = Evasion.Evaders.AddDefaulted_GetRef();
		Evader.Unit = Defender;
		Evader.StartLocation = Defender->GetActorLocation();
	}
	// The camera was framed on the segment's unit; from the first frame it frames the defenders' centre instead.
	Evasion.StartLocation = MainUnit->GetActorLocation();
	Evasion.CameraAnchor = Evasion.StartLocation;
	GetEvadersCentre(Evasion.CameraAnchor);
	Evasion.CameraStartLocation = M_Camera->GetActorLocation();
	Evasion.EffectIdsAtBegin = CollectActiveEffectIds();
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_EVASION_BEGIN defenders=%d centre=%s"), Evasion.Evaders.Num(),
	       *Evasion.CameraAnchor.ToCompactString());
}

void ARTSShortsDirector::TickSoloEvasion(const int32 Frame)
{
	using namespace RTSShortsEvasionPrivate;
	if (not M_Battle.Evasion.bIsActive)
	{
		return;
	}
	const FRTSShortsSoloEvasion& Settings = M_Job.Segments[M_CurrentSegment].BattleSoloEvasion;
	UpdateSoloFollowCamera(Settings);
	FVector Centre;
	if (M_Battle.DecidedFrame >= 0 || not GetEvadersCentre(Centre) || Frame % CycleEveryFrames != 0)
	{
		return;
	}
	if (not bM_FireOrdered)
	{
		// Before the first order the turrets would auto-engage, every defender on the same nearest attacker: give each
		// one its own threat from the first frame.
		for (FRTSShortsEvaderState& Evader : M_Battle.Evasion.Evaders)
		{
			if (GetIsAlive(Evader.Unit))
			{
				if (not GetIsAlive(Evader.Threat) || not GetIsThreatInRange(Evader))
				{
					PickSoloThreat(Evader);
				}
				AimSoloAtThreat(Evader);
			}
		}
		return;
	}
	const float SegmentSeconds = static_cast<float>(Frame - M_SegmentStartFrames[M_CurrentSegment]) / M_Job.Fps;
	TickSoloEvasionCycle(Settings, SegmentSeconds);
	TickBattleReflanks(Settings, SegmentSeconds);
}

void ARTSShortsDirector::TickSoloEvasionCycle(const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	for (FRTSShortsEvaderState& Evader : M_Battle.Evasion.Evaders)
	{
		if (GetIsAlive(Evader.Unit))
		{
			TickEvaderCycle(Evader, Settings, SegmentSeconds);
		}
	}
}

void ARTSShortsDirector::TickEvaderCycle(FRTSShortsEvaderState& Evader, const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	if (not GetIsAlive(Evader.Threat) || not GetIsThreatInRange(Evader))
	{
		PickSoloThreat(Evader);
	}
	AimSoloAtThreat(Evader);
	const UWeaponState* MainGun = GetMainGun(Evader.Unit.Get());
	const bool bReloading = IsValid(MainGun) && MainGun->GetIsReloading();
	const float PhaseSeconds = SegmentSeconds - Evader.PhaseStartSeconds;
	switch (Evader.Phase)
	{
	case ERTSShortsSoloEvasionPhase::WaitingForFireOrder:
		EnterSoloHold(Evader, SegmentSeconds);
		break;
	case ERTSShortsSoloEvasionPhase::Holding:
		if ((bReloading && PhaseSeconds >= Settings.HoldSeconds) || PhaseSeconds >= Settings.MaxHoldSeconds)
		{
			EnterSoloReverse(Evader, Settings, SegmentSeconds);
		}
		break;
	case ERTSShortsSoloEvasionPhase::Reversing:
		if (not bReloading && PhaseSeconds >= Settings.HoldSeconds)
		{
			EnterSoloHold(Evader, SegmentSeconds);
			break;
		}
		TurnSoloTowardsThreat(Evader, SegmentSeconds);
		break;
	}
}

void ARTSShortsDirector::EnterSoloHold(FRTSShortsEvaderState& Evader, const float SegmentSeconds)
{
	Evader.Phase = ERTSShortsSoloEvasionPhase::Holding;
	Evader.PhaseStartSeconds = SegmentSeconds;
	Evader.LastTurnOrderSeconds = SegmentSeconds - RTSShortsEvasionPrivate::TurnOrderEverySeconds;
	if (ICommands* Commands = Cast<ICommands>(Evader.Unit.Get()))
	{
		Commands->StopCommand();
	}
	TurnSoloTowardsThreat(Evader, SegmentSeconds);
	AimSoloAtThreat(Evader);
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_EVASION_HOLD at=%.2f unit=%s threat=%s"), SegmentSeconds,
	       *GetNameSafe(Evader.Unit.Get()), *GetNameSafe(Evader.Threat.Get()));
}

void ARTSShortsDirector::EnterSoloReverse(FRTSShortsEvaderState& Evader, const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	Evader.Phase = ERTSShortsSoloEvasionPhase::Reversing;
	Evader.PhaseStartSeconds = SegmentSeconds;
	AActor* Unit = Evader.Unit.Get();
	const AActor* Threat = Evader.Threat.Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr || Threat == nullptr)
	{
		return;
	}
	const FVector Away = RTSShortsEvasionPrivate::GetDirectionAwayFromAttackers(*Unit, *Threat, M_Battle.EnemyUnits);
	const FVector Destination = GetBattleGroundPoint(Unit->GetActorLocation() + Away * Settings.ReverseLegCm);
	const bool bInsideRetreat = FVector::Dist2D(Destination, Evader.StartLocation) <= Settings.RetreatRadiusCm;
	if (bInsideRetreat)
	{
		Commands->ReverseUnitToLocation(Destination, true);
	}
	AimSoloAtThreat(Evader);
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_EVASION_REVERSE at=%.2f unit=%s threat=%s reversing=%d to=%s"), SegmentSeconds,
	       *Unit->GetName(), *GetNameSafe(Threat), bInsideRetreat ? 1 : 0, *Destination.ToCompactString());
}

void ARTSShortsDirector::TurnSoloTowardsThreat(FRTSShortsEvaderState& Evader, const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	AActor* Unit = Evader.Unit.Get();
	const AActor* Threat = Evader.Threat.Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr || Threat == nullptr || SegmentSeconds - Evader.LastTurnOrderSeconds < TurnOrderEverySeconds)
	{
		return;
	}
	// A rotate order would end a reverse leg that is still running; that leg is turning the front round already.
	const bool bStationary = Unit->GetVelocity().Size2D() < StationarySpeedCm;
	if (not bStationary || GetDegreesOffNose(*Unit, *Threat) <= FacingToleranceDegrees)
	{
		return;
	}
	Evader.LastTurnOrderSeconds = SegmentSeconds;
	Commands->RotateTowards((Threat->GetActorLocation() - Unit->GetActorLocation()).GetSafeNormal2D().Rotation(), true);
}

void ARTSShortsDirector::PickSoloThreat(FRTSShortsEvaderState& Evader)
{
	using namespace RTSShortsEvasionPrivate;
	const AActor* Unit = Evader.Unit.Get();
	if (Unit == nullptr)
	{
		return;
	}
	const float RangeCm = GetEvaderRange(Unit);
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Lowest();
	for (const TWeakObjectPtr<AActor>& Attacker : M_Battle.EnemyUnits)
	{
		if (not GetIsAlive(Attacker))
		{
			continue;
		}
		int32 Sharing = 0;
		for (const FRTSShortsEvaderState& Other : M_Battle.Evasion.Evaders)
		{
			Sharing += &Other != &Evader && GetIsAlive(Other.Unit) && Other.Threat == Attacker ? 1 : 0;
		}
		const float Score = GetFlankScore(*Unit, *Attacker) - SharedThreatPenalty * Sharing
			+ (GetIsWithinRange(*Unit, *Attacker, RangeCm) ? InRangeBonus : 0.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Attacker.Get();
		}
	}
	if (Evader.Threat.Get() == Best)
	{
		return;
	}
	Evader.Threat = Best;
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_EVASION_THREAT unit=%s threat=%s inRange=%d"), *Unit->GetName(),
	       *GetNameSafe(Best), Best != nullptr && GetIsWithinRange(*Unit, *Best, RangeCm) ? 1 : 0);
}

void ARTSShortsDirector::AimSoloAtThreat(const FRTSShortsEvaderState& Evader) const
{
	using namespace RTSShortsEvasionPrivate;
	if (GetIsThreatInRange(Evader))
	{
		AimTurretsAt(Evader.Unit.Get(), Evader.Threat.Get());
		return;
	}
	ReleaseOutOfRangeTurrets(Evader.Unit.Get(), GetEvaderRange(Evader.Unit.Get()));
}

float ARTSShortsDirector::GetEvaderRange(const AActor* Unit) const
{
	const float Shortest = ReadShortestTurretRange(Unit);
	return Shortest > 0.f ? Shortest : ReadLargestWeaponRange(Unit);
}

bool ARTSShortsDirector::GetIsThreatInRange(const FRTSShortsEvaderState& Evader) const
{
	const AActor* Unit = Evader.Unit.Get();
	const AActor* Threat = Evader.Threat.Get();
	return Unit != nullptr && Threat != nullptr && RTSShortsEvasionPrivate::GetIsWithinRange(*Unit, *Threat, GetEvaderRange(Unit));
}

AActor* ARTSShortsDirector::GetNearestEvader(const AActor& From) const
{
	using namespace RTSShortsEvasionPrivate;
	AActor* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (const FRTSShortsEvaderState& Evader : M_Battle.Evasion.Evaders)
	{
		if (not GetIsAlive(Evader.Unit))
		{
			continue;
		}
		const float Distance = FVector::DistSquared2D(From.GetActorLocation(), Evader.Unit->GetActorLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Evader.Unit.Get();
		}
	}
	return Nearest;
}

bool ARTSShortsDirector::GetIsEvader(const AActor* Unit) const
{
	return M_Battle.Evasion.bIsActive && Unit != nullptr
		&& M_Battle.Evasion.Evaders.ContainsByPredicate([Unit](const FRTSShortsEvaderState& Evader) { return Evader.Unit.Get() == Unit; });
}

bool ARTSShortsDirector::GetEvadersCentre(FVector& OutCentre) const
{
	using namespace RTSShortsEvasionPrivate;
	FVector Sum = FVector::ZeroVector;
	int32 Alive = 0;
	for (const FRTSShortsEvaderState& Evader : M_Battle.Evasion.Evaders)
	{
		if (GetIsAlive(Evader.Unit))
		{
			Sum += Evader.Unit->GetActorLocation();
			++Alive;
		}
	}
	if (Alive == 0)
	{
		return false;
	}
	OutCentre = Sum / Alive;
	return true;
}

void ARTSShortsDirector::ApplyTrackTrailArea() const
{
	UTankTrailSubsystem* Trails = GetWorld()->GetSubsystem<UTankTrailSubsystem>();
	if (M_Job.TrackTrailAreaHalfSizeCm <= 0.f || Trails == nullptr || not GetIsValidStage())
	{
		return;
	}
	const FVector2D HalfSize(M_Job.TrackTrailAreaHalfSizeCm, M_Job.TrackTrailAreaHalfSizeCm);
	Trails->SetTrailArea(FVector2D(M_Stage->GetActorLocation()) - HalfSize, HalfSize * 2.f);
}

void ARTSShortsDirector::SkipSegmentsAfterRunning(const int32 Frame)
{
	bM_LaterSegmentsSkipped = true;
	bM_TimelineShortened = true;
	for (int32 Index = M_CurrentSegment + 1; Index < M_Manifest.Segments.Num(); ++Index)
	{
		FRTSShortsManifestSegment& Skipped = M_Manifest.Segments[Index];
		Skipped.bSkipped = true;
		Skipped.SkipReason = TEXT("the unit was destroyed in an earlier round");
		Skipped.StartFrame = Frame;
		Skipped.EndFrame = Frame;
	}
	for (int32 Index = M_CurrentSegment + 1; Index < M_SegmentStartFrames.Num(); ++Index)
	{
		M_SegmentStartFrames[Index] = Frame;
	}
	M_Manifest.TotalFrames = Frame;
	WriteManifest();
}


void ARTSShortsDirector::UpdateSoloFollowCamera(const FRTSShortsSoloEvasion& Settings)
{
	using namespace RTSShortsEvasionPrivate;
	FRTSShortsSoloEvasionState& Evasion = M_Battle.Evasion;
	if (Settings.CameraFollowSpeed <= 0.f || not GetIsValidCamera())
	{
		return;
	}
	// The camera follows the living defenders' centre; once all are dead it settles where it stopped, so the last
	// wreck stays in frame for the result.
	FVector Centre;
	if (GetEvadersCentre(Centre))
	{
		const float Blend = 1.f - FMath::Exp(-Settings.CameraFollowSpeed / static_cast<float>(M_Job.Fps));
		Evasion.CameraAnchor = FMath::Lerp(Evasion.CameraAnchor, Centre, Blend);
	}
	FVector Offset = Evasion.CameraAnchor - Evasion.StartLocation;
	Offset.Z = 0.f;
	M_Camera->SetActorLocation(Evasion.CameraStartLocation + Offset);
}

void ARTSShortsDirector::TickBattleReflanks(const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	for (int32 Index = 0; Index < M_Battle.EnemyUnits.Num(); ++Index)
	{
		const TWeakObjectPtr<AActor> Attacker = M_Battle.EnemyUnits[Index];
		const bool bOnFlankRoute = M_Battle.Flanks.ContainsByPredicate([&Attacker](const FRTSShortsBattleFlankState& Flank)
		{
			return not Flank.bFinished && Flank.Unit == Attacker;
		});
		// Soldiers of Segment.BattleSquadTactics squads move with their squad, which picks its own flank.
		const bool bTacticSoldier = not M_Battle.SquadTactics.IsEmpty() && Cast<ASquadUnit>(Attacker.Get()) != nullptr;
		if (bOnFlankRoute || bTacticSoldier || not GetIsAlive(Attacker))
		{
			continue;
		}
		// Each attacker works round the defender nearest to it; with one defender that is always the same unit.
		AActor* Defender = GetNearestEvader(*Attacker);
		if (Defender == nullptr)
		{
			return;
		}
		FRTSShortsReflankState& Reflank = M_Battle.Reflanks.FindOrAdd(Attacker);
		if (not Reflank.bDriving)
		{
			if (SegmentSeconds >= Reflank.NextAllowedSeconds && GetDegreesOffNose(*Defender, *Attacker) < FacedByUnitDegrees)
			{
				OrderBattleReflank(*Attacker, *Defender, Reflank, Settings, SegmentSeconds);
			}
			continue;
		}
		const bool bArrived = FVector::Dist2D(Attacker->GetActorLocation(), Reflank.Destination) < ReflankArrivedCm;
		if (not bArrived && SegmentSeconds - Reflank.OrderedAtSeconds < ReflankTimeoutSeconds)
		{
			continue;
		}
		Reflank.bDriving = false;
		// Staggered, or every attacker would set off together.
		Reflank.NextAllowedSeconds = SegmentSeconds + Settings.ReflankIntervalSeconds + ReflankStaggerSeconds * Index;
		if (ICommands* Commands = Cast<ICommands>(Attacker.Get()))
		{
			Commands->AttackActor(Defender, true);
			M_Battle.OrderedTargets.Add(Attacker, Defender);
		}
	}
}

void ARTSShortsDirector::OrderBattleReflank(AActor& Attacker, AActor& Defender, FRTSShortsReflankState& Reflank,
                                            const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	ICommands* Commands = Cast<ICommands>(&Attacker);
	if (Commands == nullptr)
	{
		return;
	}
	const FVector Forward = Defender.GetActorForwardVector().GetSafeNormal2D();
	const FVector ToAttacker = (Attacker.GetActorLocation() - Defender.GetActorLocation()).GetSafeNormal2D();
	// It keeps going round the side it is already on.
	const float Side = FVector::CrossProduct(Forward, ToAttacker).Z >= 0.f ? 1.f : -1.f;
	const FVector Bearing = Forward.RotateAngleAxis(Side * ReflankBearingDegrees, FVector::UpVector);
	const FVector Destination = GetBattleGroundPoint(Defender.GetActorLocation() + Bearing * Settings.ReflankRadiusCm);
	const FRotator Facing = (Defender.GetActorLocation() - Destination).GetSafeNormal2D().Rotation();
	const ECommandQueueError Result = Commands->MoveToLocation(Destination, true, Facing, false);
	AimTurretsAt(&Attacker, &Defender);
	Reflank.bDriving = true;
	Reflank.OrderedAtSeconds = SegmentSeconds;
	Reflank.Destination = Destination;
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_REFLANK at=%.2f unit=%s round=%s result=%d to=%s"), SegmentSeconds,
	       *Attacker.GetName(), *Defender.GetName(), static_cast<int32>(Result), *Destination.ToCompactString());
}
