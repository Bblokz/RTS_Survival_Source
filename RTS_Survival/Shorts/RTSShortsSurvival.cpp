// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

// Segment.Survival ("How long can they survive?"): the segment's unit and its allies hold a hilltop; Soviet waves of
// rising tiers spawn behind a ridge in front of them, out of the camera's sight, and come over its crest. Every unit of
// the short gets Survival.RangeMultiplier on its weapons' range. The battle ends only when the last defender dies.

#include "RTSShortsDirector.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "RTSShortsRangeBehaviour.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/FStrafeAircraftSettings/FStrafeAircraftSettings.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "RTS_Survival/Weapons/Projectile/Tests/ProjectileMapTestHooks.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsSurvival, Log, All);

namespace RTSShortsSurvivalPrivate
{
	// A spawn spot is not reused for this long: the unit spawned there is still driving off it.
	constexpr float SpotReuseSeconds = 4.f;
	// Seen-check height over a unit's origin (about a tank's turret roof or a standing soldier's head).
	constexpr float UnitTopCm = 300.f;
	constexpr int32 ReleaseCheckEveryFrames = 10;
	constexpr int32 SightCheckEveryFrames = 6;
	// Aircraft are removed this long after they spawned: by then their run is over and they have flown off.
	constexpr float AircraftLifeSeconds = 40.f;
	// Defender orders: a target must be within this fraction of the defender's longest weapon range.
	constexpr float DefenderReachFraction = 0.97f;

	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}

	bool GetIsDefenderAlive(const TWeakObjectPtr<AActor>& Defender)
	{
		ASquadController* Squad = Cast<ASquadController>(Defender.Get());
		if (Squad == nullptr)
		{
			return GetIsAlive(Defender);
		}
		if (not IsValid(Squad))
		{
			return false;
		}
		const TArray<ASquadUnit*> Soldiers = Squad->GetSquadUnitsChecked();
		// A squad whose soldiers have not spawned yet still stands.
		if (Soldiers.Num() == 0)
		{
			return true;
		}
		for (ASquadUnit* Soldier : Soldiers)
		{
			if (IsValid(Soldier) && Soldier->IsUnitAlive())
			{
				return true;
			}
		}
		return false;
	}

	/** The longest range of any weapon on a vehicle, squad or building expansion (cm), 0 when none is known yet. */
	float ReadReach(const AActor* Unit)
	{
		TArray<UWeaponState*> Weapons;
		if (ASquadController* Squad = Cast<ASquadController>(const_cast<AActor*>(Unit)))
		{
			Weapons = Squad->GetWeaponsOfSquad();
		}
		else if (const ABuildingExpansion* Expansion = Cast<ABuildingExpansion>(Unit))
		{
			Weapons = Expansion->GetAllWeapons();
		}
		else if (const ATankMaster* Tank = Cast<ATankMaster>(Unit))
		{
			Weapons = FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
		}
		float Reach = 0.f;
		for (const UWeaponState* Weapon : Weapons)
		{
			if (IsValid(Weapon))
			{
				Reach = FMath::Max(Reach, Weapon->GetRange());
			}
		}
		return Reach;
	}

	FVector GetLiveLocation(const AActor* Unit)
	{
		if (const ASquadController* Squad = Cast<ASquadController>(Unit))
		{
			FVector Sum = FVector::ZeroVector;
			int32 Count = 0;
			for (ASquadUnit* Soldier : const_cast<ASquadController*>(Squad)->GetSquadUnitsChecked())
			{
				if (IsValid(Soldier) && Soldier->IsUnitAlive())
				{
					Sum += Soldier->GetActorLocation();
					++Count;
				}
			}
			if (Count > 0)
			{
				return Sum / Count;
			}
		}
		return Unit->GetActorLocation();
	}
}

void ARTSShortsDirector::BeginSurvival(const int32 SegmentIndex)
{
	if (not M_Job.Segments.IsValidIndex(SegmentIndex) || not M_Job.Segments[SegmentIndex].Survival.bEnabled || not GetIsValidStage())
	{
		return;
	}
	const FRTSShortsSurvival& Settings = M_Job.Segments[SegmentIndex].Survival;
	M_Survival = FRTSShortsSurvivalState();
	M_Survival.bIsActive = true;
	M_Survival.BeginFrame = GetTimelineFrame();
	M_Survival.SpawnUsedFrame.Init(TNumericLimits<int32>::Lowest() / 2, Settings.SpawnOffsets.Num());
	URTSShortsRangeBehaviour::SetRangeMultiplier(Settings.RangeMultiplier);
	M_Survival.Defenders = M_Battle.PlayerUnits;
	M_Survival.Defenders.Append(M_Battle.PlayerSquads);
	for (const TWeakObjectPtr<AActor>& Defender : M_Survival.Defenders)
	{
		AActor* Unit = Defender.Get();
		if (not IsValid(Unit))
		{
			continue;
		}
		ApplySurvivalRange(Unit);
		// Defenders never leave the hill: squads hold their spot, vehicles only take orders on targets in reach.
		if (const ASquadController* Squad = Cast<ASquadController>(Unit))
		{
			Squad->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
		}
		else if (ATankMaster* Tank = Cast<ATankMaster>(Unit))
		{
			Tank->SetAggroStance(ERTSAggroBehaviour::Stance_HoldPosition);
		}
	}
	M_Survival.DefendersAlive = M_Survival.Defenders.Num();
	M_Manifest.Segments[SegmentIndex].SurvivalDefenders = M_Survival.Defenders.Num();
#if RTS_WITH_MAP_TESTS
	M_Survival.LaunchHandle = FProjectileMapTestHooks::OnShellLaunched().AddLambda(
		[this](const FMapTestShellLaunchReport& Report)
		{
			using namespace RTSShortsSurvivalPrivate;
			constexpr int32 ShellsToLog = 24;
			if (not M_Survival.bIsActive)
			{
				return;
			}
			if (M_Manifest.Segments.IsValidIndex(M_CurrentSegment))
			{
				TArray<int32>& ShotFrames = M_Manifest.Segments[M_CurrentSegment].SurvivalShotFrames;
				const int32 Frame = GetTimelineFrame();
				if (ShotFrames.IsEmpty() || ShotFrames.Last() != Frame)
				{
					ShotFrames.Add(Frame);
				}
			}
			if (M_Survival.PlayerShellsLogged >= ShellsToLog && M_Survival.EnemyShellsLogged >= ShellsToLog)
			{
				return;
			}
			const UActorComponent* Component = Cast<UActorComponent>(Report.WeaponOwner.Get());
			AActor* Shooter = Component != nullptr ? Component->GetOwner() : Cast<AActor>(Report.WeaponOwner.Get());
			while (IsValid(Shooter) && Shooter->GetAttachParentActor() != nullptr)
			{
				Shooter = Shooter->GetAttachParentActor();
			}
			const URTSComponent* ShooterRTS = IsValid(Shooter) ? Shooter->FindComponentByClass<URTSComponent>() : nullptr;
			if (ShooterRTS == nullptr)
			{
				return;
			}
			const bool bPlayer = ShooterRTS->GetOwningPlayer() == M_Job.UnitOwningPlayer;
			int32& Logged = bPlayer ? M_Survival.PlayerShellsLogged : M_Survival.EnemyShellsLogged;
			if (Logged >= ShellsToLog)
			{
				return;
			}
			++Logged;
			float Nearest = TNumericLimits<float>::Max();
			for (const TWeakObjectPtr<AActor>& Other : bPlayer ? M_Battle.EnemyUnits : M_Battle.PlayerUnits)
			{
				if (GetIsAlive(Other))
				{
					Nearest = FMath::Min(Nearest, FVector::Dist2D(Shooter->GetActorLocation(), Other->GetActorLocation()));
				}
			}
			UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_SHOT side=%s shooter=%s nearestOther=%.0f reach=%.0f"),
			       bPlayer ? TEXT("player") : TEXT("enemy"), *Shooter->GetName(), Nearest, ReadReach(Shooter));
		});
#endif
	UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_BEGIN segment=%d defenders=%d waves=%d spawns=%d range=%.2f"),
	       SegmentIndex, M_Survival.Defenders.Num(), Settings.Waves.Num(), Settings.SpawnOffsets.Num(), Settings.RangeMultiplier);
}

void ARTSShortsDirector::ApplySurvivalRange(AActor* Unit) const
{
	if (not IsValid(Unit) || Cast<ASquadUnit>(Unit) != nullptr)
	{
		return;
	}
	UBehaviourComp* BehaviourComponent = Unit->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		UE_LOG(LogRTSShortsSurvival, Warning, TEXT("RTS_SHORTS_SURVIVAL_NO_BEHAVIOURS unit=%s"), *Unit->GetName());
		return;
	}
	if (BehaviourComponent->GetBehaviourByClass(URTSShortsRangeBehaviour::StaticClass()) != nullptr)
	{
		return;
	}
	BehaviourComponent->AddBehaviour(URTSShortsRangeBehaviour::StaticClass());
	UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_RANGE unit=%s reachBefore=%.0f"), *Unit->GetName(),
	       RTSShortsSurvivalPrivate::ReadReach(Unit));
}

void ARTSShortsDirector::TickSurvival(const int32 Frame)
{
	using namespace RTSShortsSurvivalPrivate;
	if (not M_Survival.bIsActive || not M_Job.Segments.IsValidIndex(M_CurrentSegment))
	{
		return;
	}
	const FRTSShortsSurvival& Settings = M_Job.Segments[M_CurrentSegment].Survival;
	FRTSShortsManifestSegment& Manifest = M_Manifest.Segments[M_CurrentSegment];
	const float SegmentSeconds = static_cast<float>(Frame - M_SegmentStartFrames[M_CurrentSegment]) / M_Job.Fps;
	int32 Alive = 0;
	for (const TWeakObjectPtr<AActor>& Defender : M_Survival.Defenders)
	{
		Alive += GetIsDefenderAlive(Defender) ? 1 : 0;
	}
	for (int32 Lost = Alive; Lost < M_Survival.DefendersAlive; ++Lost)
	{
		Manifest.SurvivalDefenderDeathFrames.Add(Frame);
		UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_DEFENDER_LOST frame=%d alive=%d"), Frame, Alive);
	}
	M_Survival.DefendersAlive = Alive;
	if (Alive == 0)
	{
		if (Manifest.SurvivalEndFrame < 0)
		{
			Manifest.SurvivalEndFrame = Frame;
			UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_END frame=%d seconds=%.1f waves=%d"), Frame,
			       SegmentSeconds, M_Survival.NextWave);
		}
	}
	else
	{
		while (Settings.Waves.IsValidIndex(M_Survival.NextWave) && SegmentSeconds >= Settings.Waves[M_Survival.NextWave].AtSeconds)
		{
			SpawnSurvivalWave(M_Survival.NextWave, Frame);
			++M_Survival.NextWave;
		}
	}
	if (Frame % ReleaseCheckEveryFrames == 0)
	{
		ReleaseSurvivalUnits(Frame);
	}
	if (Frame % SightCheckEveryFrames == 0)
	{
		RecordSurvivalSightings(Frame);
	}
	// Aircraft that flew their run are gone from the frame: remove them before they loiter in a later shot.
	for (int32 Index = M_Survival.Aircraft.Num() - 1; Index >= 0; --Index)
	{
		AActor* Plane = M_Survival.Aircraft[Index].Get();
		if (Plane == nullptr || static_cast<float>(Frame - M_Survival.AircraftFrames[Index]) / M_Job.Fps > AircraftLifeSeconds)
		{
			if (IsValid(Plane))
			{
				Plane->Destroy();
			}
			M_Survival.Aircraft.RemoveAt(Index);
			M_Survival.AircraftFrames.RemoveAt(Index);
		}
	}
	int32 Killed = 0;
	for (const FRTSShortsBattleEvent& Event : Manifest.BattleEvents)
	{
		Killed += not Event.bPlayerSide && Event.Kind == TEXT("Kill") ? 1 : 0;
	}
	Manifest.SurvivalEnemiesKilled = Killed;
	Manifest.SurvivalEnemiesSpawned = M_Survival.EnemiesSpawned;
}

bool ARTSShortsDirector::PickSurvivalSpawn(FVector& OutWorldLocation, const int32 Frame)
{
	using namespace RTSShortsSurvivalPrivate;
	const FRTSShortsSurvival& Settings = M_Job.Segments[M_CurrentSegment].Survival;
	const int32 Count = Settings.SpawnOffsets.Num();
	if (Count == 0)
	{
		return false;
	}
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const int32 ReuseFrames = FMath::RoundToInt(SpotReuseSeconds * M_Job.Fps);
	for (int32 Attempt = 0; Attempt < Count; ++Attempt)
	{
		const int32 Index = (M_Survival.NextSpawn + Attempt) % Count;
		if (Frame - M_Survival.SpawnUsedFrame[Index] < ReuseFrames)
		{
			continue;
		}
		const FVector Ground = GetBattleGroundPoint(StageTransform.TransformPosition(Settings.SpawnOffsets[Index]));
		if (GetIsPointInCameraView(Ground + FVector(0.f, 0.f, UnitTopCm)))
		{
			UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_SPOT_VISIBLE index=%d"), Index);
			continue;
		}
		M_Survival.SpawnUsedFrame[Index] = Frame;
		M_Survival.NextSpawn = (Index + 1) % Count;
		OutWorldLocation = Ground;
		return true;
	}
	return false;
}

void ARTSShortsDirector::SpawnSurvivalWave(const int32 WaveIndex, const int32 Frame)
{
#if RTS_WITH_MAP_TESTS
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	const FRTSShortsSurvivalWave& Wave = Segment.Survival.Waves[WaveIndex];
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector Rally = GetBattleGroundPoint(StageTransform.TransformPosition(Segment.Survival.RallyOffset));
	M_Survival.WaveUnits.SetNum(FMath::Max(M_Survival.WaveUnits.Num(), WaveIndex + 1));
	int32 Spawned = 0;
	for (const FString& ClassPath : Wave.ClassPaths)
	{
		FVector Location;
		if (not PickSurvivalSpawn(Location, Frame))
		{
			UE_LOG(LogRTSShortsSurvival, Warning, TEXT("RTS_SHORTS_SURVIVAL_NO_SPOT wave=%d class=%s"), Wave.Number, *ClassPath);
			continue;
		}
		const FRotator Facing(0.f, (Rally - Location).Rotation().Yaw, 0.f);
		AActor* Unit = SpawnBattleParticipant(ClassPath, FTransform(Facing, Location), static_cast<uint8>(Segment.TargetOwningPlayer));
		if (not IsValid(Unit))
		{
			UE_LOG(LogRTSShortsSurvival, Warning, TEXT("RTS_SHORTS_SURVIVAL_SPAWN_FAILED wave=%d class=%s"), Wave.Number, *ClassPath);
			continue;
		}
		const bool bSquad = Cast<ASquadController>(Unit) != nullptr;
		(bSquad ? M_Battle.EnemySquads : M_Battle.EnemyUnits).Add(Unit);
		// Held: no battle order sends it at the hill (or a defender at it) until it is within reach; it attack-moves in.
		M_Battle.HeldUnits.Add(Unit);
		if (UFowComp* Fow = Unit->FindComponentByClass<UFowComp>())
		{
			Fow->StopFowParticipation();
		}
		UHealthComponent* Health = Unit->FindComponentByClass<UHealthComponent>();
		if (Health != nullptr && not FMath::IsNearlyEqual(Segment.BattleHealthScale, 1.f))
		{
			Health->SetMaxHealth(Health->GetMaxHealth() * Segment.BattleHealthScale);
		}
		AddBattleEnemyBehaviours(Segment, Unit);
		ApplySurvivalRange(Unit);
		ICommands* Commands = Cast<ICommands>(Unit);
		FRTSShortsSurvivalPending Pending;
		Pending.Unit = Unit;
		Pending.SpawnFrame = Frame;
		Pending.WaveIndex = WaveIndex;
		ECommandQueueError Result = ECommandQueueError::NoError;
		if (Segment.Survival.AdvanceOffsetX > 0.f)
		{
			// A plain move: attack-move would park it at the edge of its own (tripled) range, out of most defenders' reach.
			const FVector LocalSpawn = StageTransform.InverseTransformPosition(Location);
			const float HalfWidth = FMath::Max(0.f, Segment.Survival.AdvanceHalfWidthCm);
			Pending.bAdvancing = true;
			Pending.AdvancePoint = GetBattleGroundPoint(StageTransform.TransformPosition(
				FVector(Segment.Survival.AdvanceOffsetX, FMath::Clamp(LocalSpawn.Y, -HalfWidth, HalfWidth), 0.f)));
			const FRotator AdvanceFacing(0.f, (Rally - Pending.AdvancePoint).Rotation().Yaw, 0.f);
			Result = Commands != nullptr ? Commands->MoveToLocation(Pending.AdvancePoint, true, AdvanceFacing, false) : Result;
		}
		else
		{
			Result = Commands != nullptr ? Commands->AttackMoveToLocation(Rally, true, Facing, false) : Result;
		}
		M_Survival.Pending.Add(Pending);
		M_Survival.WaveUnits[WaveIndex].Add(Unit);
		++M_Survival.EnemiesSpawned;
		++Spawned;
		UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_SPAWN frame=%d wave=%d tier=%d unit=%s at=%s order=%d"),
		       Frame, Wave.Number, Wave.Tier, *Unit->GetName(), *Location.ToCompactString(), static_cast<int32>(Result));
	}
	for (const FString& ClassPath : Wave.AircraftClassPaths)
	{
		SpawnSurvivalAircraft(ClassPath, Frame);
	}
	// Spawned after the job's units: switch the distance optimisation off again so it covers this wave too.
	DisableUnitOptimisation();
	FRTSShortsManifestSegment& Manifest = M_Manifest.Segments[M_CurrentSegment];
	Manifest.SurvivalWaveFrames.Add(Frame);
	Manifest.SurvivalWaveTiers.Add(Wave.Tier);
	Manifest.SurvivalWaveNumbers.Add(Wave.Number);
	Manifest.SurvivalWaveSeenFrames.Add(-1);
	UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_WAVE frame=%d wave=%d tier=%d units=%d/%d aircraft=%d"),
	       Frame, Wave.Number, Wave.Tier, Spawned, Wave.ClassPaths.Num(), Wave.AircraftClassPaths.Num());
#endif
}

AActor* ARTSShortsDirector::SpawnSurvivalAircraft(const FString& ClassPath, const int32 Frame)
{
	using namespace RTSShortsSurvivalPrivate;
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	UClass* AircraftClass = LoadClass<AActor>(nullptr, *ClassPath);
	TArray<AActor*> Targets;
	for (const TWeakObjectPtr<AActor>& Defender : M_Survival.Defenders)
	{
		if (GetIsDefenderAlive(Defender))
		{
			Targets.Add(Defender.Get());
		}
	}
	if (not IsValid(AircraftClass) || Targets.IsEmpty())
	{
		UE_LOG(LogRTSShortsSurvival, Warning, TEXT("RTS_SHORTS_SURVIVAL_AIRCRAFT_SKIPPED class=%s"), *ClassPath);
		return nullptr;
	}
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector Forward = StageTransform.GetRotation().GetForwardVector();
	const FVector TargetLocation = GetLiveLocation(Targets[FMath::RandRange(0, Targets.Num() - 1)]);
	const float Lateral = FMath::FRandRange(-12000.f, 12000.f);
	FVector Start = StageTransform.TransformPosition(FVector(Segment.Survival.AircraftStartDistanceCm, Lateral, 0.f));
	Start.Z = TargetLocation.Z + Segment.Survival.AircraftHeightCm;
	const FTransform SpawnTransform(FRotator(0.f, (TargetLocation - Start).Rotation().Yaw, 0.f), Start);
	AActor* Spawned = GetWorld()->SpawnActorDeferred<AActor>(AircraftClass, SpawnTransform, nullptr, nullptr,
	                                                         ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Spawned))
	{
		return nullptr;
	}
	// The owner must be set before BeginPlay, so the aircraft registers as the enemy's unit.
	if (URTSComponent* RTSComponent = Spawned->FindComponentByClass<URTSComponent>())
	{
		RTSComponent->SetOwningPlayerRuntime(static_cast<uint8>(Segment.TargetOwningPlayer));
	}
	Spawned->FinishSpawning(SpawnTransform);
	AAircraftMaster* Aircraft = Cast<AAircraftMaster>(Spawned);
	if (not IsValid(Aircraft))
	{
		Spawned->Destroy();
		return nullptr;
	}
	if (Aircraft->GetController() == nullptr)
	{
		Aircraft->SpawnDefaultController();
	}
	if (UFowComp* Fow = Aircraft->FindComponentByClass<UFowComp>())
	{
		Fow->StopFowParticipation();
	}
	ApplySurvivalRange(Aircraft);
	M_Survival.Aircraft.Add(Aircraft);
	M_Survival.AircraftFrames.Add(Frame);
	const FVector RunDirection = (TargetLocation - Start).GetSafeNormal2D();
	// Out past the hill behind the camera: the run ends off screen.
	const FVector Retreat = StageTransform.TransformPosition(FVector(-90000.f, -Lateral, 0.f)) + FVector(0.f, 0.f, Start.Z - StageTransform.GetLocation().Z);
	const bool bBomber = Aircraft->FindComponentByClass<UBombComponent>() != nullptr;
	const TWeakObjectPtr<AAircraftMaster> WeakAircraft = Aircraft;
	// Orders go out on the next tick, after BeginPlay has set the aircraft's state machines up (as the abilities do).
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakAircraft, bBomber, TargetLocation, RunDirection, Retreat]()
		{
			AAircraftMaster* Plane = WeakAircraft.Get();
			if (Plane == nullptr)
			{
				return;
			}
			if (bBomber)
			{
				Plane->CarpetBombing(TargetLocation - RunDirection * 1500.f, TargetLocation + RunDirection * 2500.f, Retreat);
				return;
			}
			const FStrafeAircraftSettings Strafe(TargetLocation - RunDirection * 1200.f, TargetLocation + RunDirection * 1800.f,
			                                     2.f, 7.f, Retreat);
			Plane->StrafeLocation(Strafe, FAircraftAttackMoveSettings());
		}));
	UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_AIRCRAFT frame=%d unit=%s bomber=%d start=%s target=%s"),
	       Frame, *Aircraft->GetName(), bBomber ? 1 : 0, *Start.ToCompactString(), *TargetLocation.ToCompactString());
	return Aircraft;
}

void ARTSShortsDirector::ReleaseSurvivalUnits(const int32 Frame)
{
	using namespace RTSShortsSurvivalPrivate;
	const FRTSShortsSurvival& Settings = M_Job.Segments[M_CurrentSegment].Survival;
	for (int32 Index = M_Survival.Pending.Num() - 1; Index >= 0; --Index)
	{
		const FRTSShortsSurvivalPending& Pending = M_Survival.Pending[Index];
		AActor* Unit = Pending.Unit.Get();
		const bool bSquad = Cast<ASquadController>(Unit) != nullptr;
		const bool bGone = Unit == nullptr || (bSquad ? not GetIsDefenderAlive(Pending.Unit) : not Unit->IsUnitAlive());
		if (bGone)
		{
			M_Battle.HeldUnits.Remove(Pending.Unit);
			M_Survival.Pending.RemoveAt(Index);
			continue;
		}
		const float Reach = ReadReach(Unit) * Settings.ReleaseRangeFraction;
		const FVector From = GetLiveLocation(Unit);
		float Nearest = TNumericLimits<float>::Max();
		for (const TWeakObjectPtr<AActor>& Defender : M_Survival.Defenders)
		{
			if (GetIsDefenderAlive(Defender))
			{
				Nearest = FMath::Min(Nearest, FVector::Dist2D(From, GetLiveLocation(Defender.Get())));
			}
		}
		const float AgeSeconds = static_cast<float>(Frame - Pending.SpawnFrame) / M_Job.Fps;
		constexpr float ArrivedCm = 1500.f;
		const bool bInReach = Pending.bAdvancing
			? FVector::Dist2D(From, Pending.AdvancePoint) <= ArrivedCm
			: Reach > 0.f && Nearest <= Reach;
		if (not bInReach && AgeSeconds < Settings.ReleaseAfterSeconds)
		{
			continue;
		}
		M_Battle.HeldUnits.Remove(Pending.Unit);
		UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_RELEASED frame=%d unit=%s reach=%.0f nearest=%.0f age=%.1f"),
		       Frame, *Unit->GetName(), Reach, Nearest, AgeSeconds);
		M_Survival.Pending.RemoveAt(Index);
	}
}

void ARTSShortsDirector::RecordSurvivalSightings(const int32 Frame)
{
	using namespace RTSShortsSurvivalPrivate;
	FRTSShortsManifestSegment& Manifest = M_Manifest.Segments[M_CurrentSegment];
	for (int32 Wave = 0; Wave < M_Survival.WaveUnits.Num() && Wave < Manifest.SurvivalWaveSeenFrames.Num(); ++Wave)
	{
		if (Manifest.SurvivalWaveSeenFrames[Wave] >= 0)
		{
			continue;
		}
		for (const TWeakObjectPtr<AActor>& Unit : M_Survival.WaveUnits[Wave])
		{
			if (Unit.IsValid() && GetIsPointInCameraView(GetLiveLocation(Unit.Get()) + FVector(0.f, 0.f, UnitTopCm)))
			{
				Manifest.SurvivalWaveSeenFrames[Wave] = Frame;
				UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_WAVE_SEEN frame=%d wave=%d afterSeconds=%.1f unit=%s"),
				       Frame, Manifest.SurvivalWaveNumbers[Wave],
				       static_cast<float>(Frame - Manifest.SurvivalWaveFrames[Wave]) / M_Job.Fps, *Unit->GetName());
				break;
			}
		}
	}
}

bool ARTSShortsDirector::GetIsPointInCameraView(const FVector& Point) const
{
	if (not GetIsValidCamera())
	{
		return false;
	}
	const UCineCameraComponent* Cine = M_Camera->GetCineCameraComponent();
	const FVector Eye = M_Camera->GetActorLocation();
	const FRotator View = M_Camera->GetActorRotation();
	const FVector Local = View.UnrotateVector(Point - Eye);
	if (Local.X <= 0.f)
	{
		return false;
	}
	// The render is portrait: the horizontal field of view is the narrow one, the vertical follows from the aspect.
	const float HalfHorizontal = FMath::DegreesToRadians((IsValid(Cine) ? Cine->FieldOfView : 60.f) * 0.5f);
	const float Aspect = static_cast<float>(FMath::Max(M_Job.ResolutionY, 1)) / static_cast<float>(FMath::Max(M_Job.ResolutionX, 1));
	const float HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) * Aspect);
	if (FMath::Abs(FMath::Atan2(Local.Y, Local.X)) > HalfHorizontal || FMath::Abs(FMath::Atan2(Local.Z, Local.X)) > HalfVertical)
	{
		return false;
	}
	// Only the ground hides a unit from the camera here: a hit on anything that is not a landscape does not count.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RTSShortsSurvivalSight), false);
	TArray<FHitResult> Hits;
	GetWorld()->LineTraceMultiByChannel(Hits, Eye, Point, ECC_WorldStatic, Params);
	for (const FHitResult& Hit : Hits)
	{
		if (Cast<ALandscapeProxy>(Hit.GetActor()) != nullptr)
		{
			return false;
		}
	}
	return true;
}

bool ARTSShortsDirector::GetIsSurvivalTargetOutOfReach(const AActor* Attacker, const AActor* Target) const
{
	using namespace RTSShortsSurvivalPrivate;
	if (not M_Survival.bIsActive || not IsValid(Attacker) || not IsValid(Target))
	{
		return false;
	}
	if (not M_Survival.Defenders.Contains(TWeakObjectPtr<AActor>(const_cast<AActor*>(Attacker))))
	{
		return false;
	}
	const float Reach = ReadReach(Attacker);
	if (Reach <= 0.f)
	{
		// Unknown reach: only a building, which cannot drive off the hill, may take the order.
		return Cast<ABuildingExpansion>(Attacker) == nullptr;
	}
	return FVector::Dist2D(GetLiveLocation(Attacker), GetLiveLocation(Target)) > Reach * DefenderReachFraction;
}

void ARTSShortsDirector::EndSurvival()
{
	if (not M_Survival.bIsActive)
	{
		return;
	}
	for (const TWeakObjectPtr<AActor>& Plane : M_Survival.Aircraft)
	{
		if (Plane.IsValid())
		{
			Plane->Destroy();
		}
	}
#if RTS_WITH_MAP_TESTS
	FProjectileMapTestHooks::OnShellLaunched().Remove(M_Survival.LaunchHandle);
#endif
	UE_LOG(LogRTSShortsSurvival, Display, TEXT("RTS_SHORTS_SURVIVAL_DONE waves=%d enemies=%d defendersAlive=%d"), M_Survival.NextWave,
	       M_Survival.EnemiesSpawned, M_Survival.DefendersAlive);
	M_Survival = FRTSShortsSurvivalState();
	URTSShortsRangeBehaviour::SetRangeMultiplier(3.f);
}
