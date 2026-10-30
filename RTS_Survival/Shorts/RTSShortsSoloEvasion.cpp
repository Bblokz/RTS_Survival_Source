// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "Engine/World.h"
#include "RTS_Survival/Environment/TankTrails/TankTrailSubsystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
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
	Evasion.Unit = MainUnit;
	Evasion.StartLocation = MainUnit->GetActorLocation();
	Evasion.CameraAnchor = Evasion.StartLocation;
	Evasion.CameraStartLocation = M_Camera->GetActorLocation();
	Evasion.EffectIdsAtBegin = CollectActiveEffectIds();
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
	if (not bM_FireOrdered || M_Battle.DecidedFrame >= 0 || not GetIsAlive(M_Battle.Evasion.Unit) || Frame % CycleEveryFrames != 0)
	{
		return;
	}
	const float SegmentSeconds = static_cast<float>(Frame - M_SegmentStartFrames[M_CurrentSegment]) / M_Job.Fps;
	TickSoloEvasionCycle(Settings, SegmentSeconds);
	TickBattleReflanks(Settings, SegmentSeconds);
}

void ARTSShortsDirector::TickSoloEvasionCycle(const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	FRTSShortsSoloEvasionState& Evasion = M_Battle.Evasion;
	if (not GetIsAlive(Evasion.Threat))
	{
		PickSoloThreat();
	}
	AimSoloAtThreat();
	const UWeaponState* MainGun = GetMainGun(Evasion.Unit.Get());
	const bool bReloading = IsValid(MainGun) && MainGun->GetIsReloading();
	const float PhaseSeconds = SegmentSeconds - Evasion.PhaseStartSeconds;
	switch (Evasion.Phase)
	{
	case ERTSShortsSoloEvasionPhase::WaitingForFireOrder:
		EnterSoloHold(SegmentSeconds);
		break;
	case ERTSShortsSoloEvasionPhase::Holding:
		if ((bReloading && PhaseSeconds >= Settings.HoldSeconds) || PhaseSeconds >= Settings.MaxHoldSeconds)
		{
			EnterSoloReverse(Settings, SegmentSeconds);
		}
		break;
	case ERTSShortsSoloEvasionPhase::Reversing:
		if (not bReloading && PhaseSeconds >= Settings.HoldSeconds)
		{
			EnterSoloHold(SegmentSeconds);
			break;
		}
		TurnSoloTowardsThreat(SegmentSeconds);
		break;
	}
}

void ARTSShortsDirector::EnterSoloHold(const float SegmentSeconds)
{
	FRTSShortsSoloEvasionState& Evasion = M_Battle.Evasion;
	Evasion.Phase = ERTSShortsSoloEvasionPhase::Holding;
	Evasion.PhaseStartSeconds = SegmentSeconds;
	Evasion.LastTurnOrderSeconds = SegmentSeconds - RTSShortsEvasionPrivate::TurnOrderEverySeconds;
	if (ICommands* Commands = Cast<ICommands>(Evasion.Unit.Get()))
	{
		Commands->StopCommand();
	}
	TurnSoloTowardsThreat(SegmentSeconds);
	AimSoloAtThreat();
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_EVASION_HOLD at=%.2f threat=%s"), SegmentSeconds, *GetNameSafe(Evasion.Threat.Get()));
}

void ARTSShortsDirector::EnterSoloReverse(const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	FRTSShortsSoloEvasionState& Evasion = M_Battle.Evasion;
	Evasion.Phase = ERTSShortsSoloEvasionPhase::Reversing;
	Evasion.PhaseStartSeconds = SegmentSeconds;
	AActor* Unit = Evasion.Unit.Get();
	const AActor* Threat = Evasion.Threat.Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr || Threat == nullptr)
	{
		return;
	}
	const FVector Away = RTSShortsEvasionPrivate::GetDirectionAwayFromAttackers(*Unit, *Threat, M_Battle.EnemyUnits);
	const FVector Destination = GetBattleGroundPoint(Unit->GetActorLocation() + Away * Settings.ReverseLegCm);
	const bool bInsideRetreat = FVector::Dist2D(Destination, Evasion.StartLocation) <= Settings.RetreatRadiusCm;
	if (bInsideRetreat)
	{
		Commands->ReverseUnitToLocation(Destination, true);
	}
	AimSoloAtThreat();
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_EVASION_REVERSE at=%.2f threat=%s reversing=%d to=%s"), SegmentSeconds, *GetNameSafe(Threat),
	       bInsideRetreat ? 1 : 0, *Destination.ToCompactString());
}

void ARTSShortsDirector::TurnSoloTowardsThreat(const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	FRTSShortsSoloEvasionState& Evasion = M_Battle.Evasion;
	AActor* Unit = Evasion.Unit.Get();
	const AActor* Threat = Evasion.Threat.Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr || Threat == nullptr || SegmentSeconds - Evasion.LastTurnOrderSeconds < TurnOrderEverySeconds)
	{
		return;
	}
	// A rotate order would end a reverse leg that is still running; that leg is turning the front round already.
	const bool bStationary = Unit->GetVelocity().Size2D() < StationarySpeedCm;
	if (not bStationary || GetDegreesOffNose(*Unit, *Threat) <= FacingToleranceDegrees)
	{
		return;
	}
	Evasion.LastTurnOrderSeconds = SegmentSeconds;
	Commands->RotateTowards((Threat->GetActorLocation() - Unit->GetActorLocation()).GetSafeNormal2D().Rotation(), true);
}

void ARTSShortsDirector::PickSoloThreat()
{
	using namespace RTSShortsEvasionPrivate;
	FRTSShortsSoloEvasionState& Evasion = M_Battle.Evasion;
	const AActor* Unit = Evasion.Unit.Get();
	if (Unit == nullptr)
	{
		return;
	}
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Lowest();
	for (const TWeakObjectPtr<AActor>& Attacker : M_Battle.EnemyUnits)
	{
		if (not GetIsAlive(Attacker))
		{
			continue;
		}
		const float Score = GetFlankScore(*Unit, *Attacker);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Attacker.Get();
		}
	}
	Evasion.Threat = Best;
}

void ARTSShortsDirector::AimSoloAtThreat() const
{
	RTSShortsEvasionPrivate::AimTurretsAt(M_Battle.Evasion.Unit.Get(), M_Battle.Evasion.Threat.Get());
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
	// Once the unit is dead the camera settles where it stopped: its wreck stays in frame for the result.
	if (GetIsAlive(Evasion.Unit))
	{
		const float Blend = 1.f - FMath::Exp(-Settings.CameraFollowSpeed / static_cast<float>(M_Job.Fps));
		Evasion.CameraAnchor = FMath::Lerp(Evasion.CameraAnchor, Evasion.Unit->GetActorLocation(), Blend);
	}
	FVector Offset = Evasion.CameraAnchor - Evasion.StartLocation;
	Offset.Z = 0.f;
	M_Camera->SetActorLocation(Evasion.CameraStartLocation + Offset);
}

void ARTSShortsDirector::TickBattleReflanks(const FRTSShortsSoloEvasion& Settings, const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	AActor* Unit = M_Battle.Evasion.Unit.Get();
	for (int32 Index = 0; Index < M_Battle.EnemyUnits.Num(); ++Index)
	{
		const TWeakObjectPtr<AActor> Attacker = M_Battle.EnemyUnits[Index];
		const bool bOnFlankRoute = M_Battle.Flanks.ContainsByPredicate([&Attacker](const FRTSShortsBattleFlankState& Flank)
		{
			return not Flank.bFinished && Flank.Unit == Attacker;
		});
		if (bOnFlankRoute || not GetIsAlive(Attacker))
		{
			continue;
		}
		FRTSShortsReflankState& Reflank = M_Battle.Reflanks.FindOrAdd(Attacker);
		if (not Reflank.bDriving)
		{
			if (SegmentSeconds >= Reflank.NextAllowedSeconds && GetDegreesOffNose(*Unit, *Attacker) < FacedByUnitDegrees)
			{
				OrderBattleReflank(*Attacker, Reflank, Settings, SegmentSeconds);
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
			Commands->AttackActor(Unit, true);
			M_Battle.OrderedTargets.Add(Attacker, Unit);
		}
	}
}

void ARTSShortsDirector::OrderBattleReflank(AActor& Attacker, FRTSShortsReflankState& Reflank, const FRTSShortsSoloEvasion& Settings,
                                            const float SegmentSeconds)
{
	using namespace RTSShortsEvasionPrivate;
	AActor* Unit = M_Battle.Evasion.Unit.Get();
	ICommands* Commands = Cast<ICommands>(&Attacker);
	if (Unit == nullptr || Commands == nullptr)
	{
		return;
	}
	const FVector Forward = Unit->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToAttacker = (Attacker.GetActorLocation() - Unit->GetActorLocation()).GetSafeNormal2D();
	// It keeps going round the side it is already on.
	const float Side = FVector::CrossProduct(Forward, ToAttacker).Z >= 0.f ? 1.f : -1.f;
	const FVector Bearing = Forward.RotateAngleAxis(Side * ReflankBearingDegrees, FVector::UpVector);
	const FVector Destination = GetBattleGroundPoint(Unit->GetActorLocation() + Bearing * Settings.ReflankRadiusCm);
	const FRotator Facing = (Unit->GetActorLocation() - Destination).GetSafeNormal2D().Rotation();
	const ECommandQueueError Result = Commands->MoveToLocation(Destination, true, Facing, false);
	AimTurretsAt(&Attacker, Unit);
	Reflank.bDriving = true;
	Reflank.OrderedAtSeconds = SegmentSeconds;
	Reflank.Destination = Destination;
	UE_LOG(LogRTSShortsEvasion, Display, TEXT("RTS_SHORTS_REFLANK at=%.2f unit=%s result=%d to=%s"), SegmentSeconds, *Attacker.GetName(),
	       static_cast<int32>(Result), *Destination.ToCompactString());
}
