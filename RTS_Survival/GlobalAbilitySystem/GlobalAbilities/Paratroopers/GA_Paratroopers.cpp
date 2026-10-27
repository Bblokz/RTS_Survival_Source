// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_Paratroopers.h"

#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace ParatroopersConstants
{
	constexpr int32 PlayerOwner = 1;
	// Hidden squads wait on a small ring around the target until their soldiers jump.
	constexpr float SquadSpawnRingRadius = 200.0f;
	// Spacing of the landing spiral: soldier N lands about sqrt(N) * this far from the target.
	constexpr float LandingSpiralSpacing = 150.0f;
	constexpr float GoldenAngleRadians = 2.39996323f;
	constexpr float JumpBehindAircraftOffset = 200.0f;
	constexpr float LandscapeTraceDepth = 200000.0f;
	const FVector NavProjectionExtentScale(2.0f, 2.0f, 2.0f);
}

void UGA_Paratroopers::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	StartDrop(TargetLocation, TOptional<FVector>());
}

void UGA_Paratroopers::ExecuteCinematicDrop(const FVector& TargetLocation, const FVector& AircraftStartLocation)
{
	StartDrop(TargetLocation, AircraftStartLocation);
}

void UGA_Paratroopers::StartDrop(const FVector& TargetLocation, const TOptional<FVector>& AircraftStartOverride)
{
	ResetAbilityEndedBroadcast();
	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}

	const TSoftClassPtr<AAircraftMaster> AircraftClassToLoad = M_ParatroopersSettings.AircraftClass;
	if (AircraftClassToLoad.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Paratroopers has no aircraft class configured."));
		BroadcastParatroopersFinishedIfNeeded();
		return;
	}

	FParatrooperDropRun NewRun;
	NewRun.RunId = M_NextRunId++;
	NewRun.TargetLocation = TargetLocation;
	NewRun.AircraftStartOverride = AircraftStartOverride;
	const int32 RunId = NewRun.RunId;
	M_DropRuns.Add(MoveTemp(NewRun));
	if (AircraftStartOverride.IsSet())
	{
		// A cinematic must have its aircraft ready on the first visible frame.
		M_ParatroopersSettings.AircraftClass.LoadSynchronous();
		OnAircraftClassLoaded(RunId);
		return;
	}

	TWeakObjectPtr<UGA_Paratroopers> WeakThis(this);
	M_AircraftClassLoadHandles.Add(UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AircraftClassToLoad.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakThis, RunId]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnAircraftClassLoaded(RunId);
		})));
	Super::ExecuteAbilityAtLocation(TargetLocation);
}

void UGA_Paratroopers::BeginDestroy()
{
	M_AircraftClassLoadHandles.Reset();
	if (UWorld* World = GetWorld())
	{
		for (FParatrooperDropRun& Run : M_DropRuns)
		{
			World->GetTimerManager().ClearTimer(Run.JumpTimerHandle);
		}
	}
	M_DropRuns.Reset();
	Super::BeginDestroy();
}

AAircraftMaster* UGA_Paratroopers::GetActiveDropAircraft() const
{
	for (const FParatrooperDropRun& Run : M_DropRuns)
	{
		if (AAircraftMaster* Aircraft = Run.Aircraft.Get())
		{
			return Aircraft;
		}
	}
	return nullptr;
}

TArray<TWeakObjectPtr<AAircraftMaster>> UGA_Paratroopers::GetActiveDropAircrafts() const
{
	TArray<TWeakObjectPtr<AAircraftMaster>> AircraftFormation;
	for (const FParatrooperDropRun& Run : M_DropRuns)
	{
		if (Run.Aircraft.IsValid())
		{
			AircraftFormation.Add(Run.Aircraft);
		}
	}
	return AircraftFormation;
}

void UGA_Paratroopers::SetCinematicLandingTrailLength(const float TrailLength)
{
	M_CinematicLandingTrailLength = FMath::Max(0.f, TrailLength);
}

void UGA_Paratroopers::SetCinematicJumpInterval(const float IntervalSeconds)
{
	if (IntervalSeconds <= 0.f)
	{
		return;
	}
	constexpr float MinimumJumpIntervalSeconds = 0.05f;
	M_ParatroopersSettings.DropIntervalPerUnit = FMath::Max(MinimumJumpIntervalSeconds, IntervalSeconds);
}

void UGA_Paratroopers::SetCinematicSquadSubtypes(const TArray<ESquadSubtype>& SquadSubtypes)
{
	if (SquadSubtypes.IsEmpty())
	{
		return;
	}
	M_ParatroopersSettings.SquadSubtypes = SquadSubtypes;
}

TArray<TWeakObjectPtr<ASquadController>> UGA_Paratroopers::GetActiveDropSquads() const
{
	TArray<TWeakObjectPtr<ASquadController>> Squads;
	for (const FParatrooperDropRun& Run : M_DropRuns)
	{
		for (const FParatrooperSquadInDrop& SquadInDrop : Run.SquadsInDrop)
		{
			Squads.AddUnique(SquadInDrop.Squad);
		}
	}
	return Squads;
}

bool UGA_Paratroopers::GetHasActiveDrop() const
{
	return not M_DropRuns.IsEmpty();
}

void UGA_Paratroopers::CancelActiveDrops()
{
	// Remove the runs before destroying aircraft: their destruction delegates can re-enter this ability.
	TArray<FParatrooperDropRun> CancelledRuns = MoveTemp(M_DropRuns);
	M_DropRuns.Reset();
	for (const FParatrooperDropRun& Run : CancelledRuns)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle JumpTimerHandle = Run.JumpTimerHandle;
			World->GetTimerManager().ClearTimer(JumpTimerHandle);
		}
		if (AAircraftMaster* Aircraft = Run.Aircraft.Get())
		{
			Aircraft->Destroy();
		}
	}
	M_AircraftClassLoadHandles.Reset();
}

UGA_Paratroopers::FParatrooperDropRun* UGA_Paratroopers::FindRun(const int32 RunId)
{
	return M_DropRuns.FindByPredicate([RunId](const FParatrooperDropRun& Run)
	{
		return Run.RunId == RunId;
	});
}

void UGA_Paratroopers::OnAircraftClassLoaded(const int32 RunId)
{
	UClass* AircraftClass = M_ParatroopersSettings.AircraftClass.Get();
	FParatrooperDropRun* Run = FindRun(RunId);
	if (not Run || not IsValid(AircraftClass) || not GetIsValidGlobalAbilityManager())
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Paratroopers failed to async load a valid aircraft class."));
		FinishRunIfDone(RunId);
		return;
	}

	const FVector StartLocation = Run->AircraftStartOverride.IsSet()
		? Run->AircraftStartOverride.GetValue()
		: GetGlobalAbilityManager()->GetAircraftBombingSpawnLocation(this, Run->TargetLocation);
	Run->PassOverLocation = FVector(Run->TargetLocation.X, Run->TargetLocation.Y, StartLocation.Z);
	const FVector EndLocation = BuildRunEndLocation(StartLocation, Run->PassOverLocation);
	Run->RunDirection = (EndLocation - StartLocation).GetSafeNormal2D();

	AAircraftMaster* SpawnedAircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(this, AircraftClass, StartLocation);
	if (not IsValid(SpawnedAircraft))
	{
		FinishRunIfDone(RunId);
		return;
	}

	Run->Aircraft = SpawnedAircraft;
	SpawnedAircraft->SetActorRotation(Run->RunDirection.Rotation());
	SpawnedAircraft->SetUnitSelectable(false);
	SpawnedAircraft->OnDestroyed.AddUniqueDynamic(this, &UGA_Paratroopers::OnSpawnedAircraftDestroyed);
	SpawnedAircraft->OnStraightRunPassedPoint.AddWeakLambda(this, [this, RunId](AAircraftMaster*)
	{
		OnAircraftOverTarget(RunId);
	});
	QueueStraightRunForNextFrame(SpawnedAircraft, RunId, EndLocation);
}

void UGA_Paratroopers::QueueStraightRunForNextFrame(
	AAircraftMaster* SpawnedAircraft,
	const int32 RunId,
	const FVector& EndLocation)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	TWeakObjectPtr<UGA_Paratroopers> WeakThis(this);
	TWeakObjectPtr<AAircraftMaster> WeakAircraft(SpawnedAircraft);
	// Same as the other aircraft abilities: the aircraft finishes its BeginPlay setup before it gets an order.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda(
		[WeakThis, WeakAircraft, RunId, EndLocation]()
		{
			if (not WeakThis.IsValid() || not WeakAircraft.IsValid())
			{
				return;
			}
			const FParatrooperDropRun* Run = WeakThis->FindRun(RunId);
			if (not Run)
			{
				return;
			}
			WeakAircraft->OrderStraightFlightRun(Run->PassOverLocation, EndLocation);
			WeakAircraft->SetStraightRunHoldAtEnd(true);
		}));
}

FVector UGA_Paratroopers::BuildRunEndLocation(const FVector& StartLocation, const FVector& PassOverLocation) const
{
	const float ExitDistance = M_ParatroopersSettings.ExitDistanceAfterTarget > 0.0f
		                           ? M_ParatroopersSettings.ExitDistanceAfterTarget
		                           : FVector::Dist2D(StartLocation, PassOverLocation);
	return GlobalAbilityHelpers::BuildAircraftRunEndLocationFromDirection(
		PassOverLocation,
		StartLocation,
		PassOverLocation,
		ExitDistance);
}

void UGA_Paratroopers::OnAircraftOverTarget(const int32 RunId)
{
	FParatrooperDropRun* Run = FindRun(RunId);
	if (not Run || Run->bM_HasReachedTarget)
	{
		return;
	}
	Run->bM_HasReachedTarget = true;

	const TArray<ESquadSubtype> SquadSubtypes = M_ParatroopersSettings.SquadSubtypes;
	for (int32 SquadIndex = 0; SquadIndex < SquadSubtypes.Num(); ++SquadIndex)
	{
		SpawnSquadForRun(RunId, SquadSubtypes[SquadIndex], SquadIndex);
	}

	UWorld* World = GetWorld();
	Run = FindRun(RunId);
	if (not Run || not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<UGA_Paratroopers> WeakThis(this);
	World->GetTimerManager().SetTimer(
		Run->JumpTimerHandle,
		FTimerDelegate::CreateLambda([WeakThis, RunId]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnJumpTimer(RunId);
		}),
		M_ParatroopersSettings.DropIntervalPerUnit,
		true);
}

void UGA_Paratroopers::SpawnSquadForRun(const int32 RunId, const ESquadSubtype SquadSubtype, const int32 SquadIndex)
{
	FParatrooperDropRun* Run = FindRun(RunId);
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not Run || SquadSubtype == ESquadSubtype::Squad_None)
	{
		return;
	}
	if (not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Paratroopers failed to find ARTSAsyncSpawner for a squad drop."));
		return;
	}

	const FVector SpawnLocation = GetProjectedSquadSpawnLocation(
		Run->TargetLocation,
		SquadIndex,
		M_ParatroopersSettings.SquadSubtypes.Num());
	++Run->PendingSquadSpawns;
	TWeakObjectPtr<UGA_Paratroopers> WeakThis(this);
	const bool bRequested = AsyncSpawner->AsyncSpawnOptionAtLocation(
		FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype)),
		SpawnLocation,
		this,
		SquadIndex,
		[WeakThis, RunId](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnSquadSpawned(RunId, SpawnedActor);
		},
		FRotator::ZeroRotator);
	if (bRequested)
	{
		return;
	}
	Run = FindRun(RunId);
	if (Run)
	{
		Run->PendingSquadSpawns = FMath::Max(0, Run->PendingSquadSpawns - 1);
	}
}

FVector UGA_Paratroopers::GetProjectedSquadSpawnLocation(
	const FVector& TargetLocation,
	const int32 SquadIndex,
	const int32 SquadCount) const
{
	const float Angle = static_cast<float>(SquadIndex) * UE_TWO_PI / FMath::Max(1, SquadCount);
	const FVector DesiredLocation = TargetLocation + FVector(
		FMath::Cos(Angle) * ParatroopersConstants::SquadSpawnRingRadius,
		FMath::Sin(Angle) * ParatroopersConstants::SquadSpawnRingRadius,
		0.0f);
	bool bWasProjected = false;
	return URTSBlueprintFunctionLibrary::RTSProjectLocationToNavigation(
		this,
		DesiredLocation,
		ParatroopersConstants::NavProjectionExtentScale,
		bWasProjected);
}

void UGA_Paratroopers::OnSquadSpawned(const int32 RunId, AActor* SpawnedActor)
{
	FParatrooperDropRun* Run = FindRun(RunId);
	if (not Run)
	{
		return;
	}
	Run->PendingSquadSpawns = FMath::Max(0, Run->PendingSquadSpawns - 1);
	ASquadController* Squad = Cast<ASquadController>(SpawnedActor);
	if (not IsValid(Squad))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_Paratroopers spawned something that is not a squad."));
		FinishRunIfDone(RunId);
		return;
	}

	// Hide at once: the spawner has just placed and shown the squad on the ground.
	Squad->BeginParachuteDrop();
	FParatrooperSquadInDrop SquadInDrop;
	SquadInDrop.Squad = Squad;
	Run->SquadsInDrop.Add(SquadInDrop);
	if (Squad->GetIsSquadFullyLoadedAndInitialized())
	{
		OnSquadReadyToJump(RunId, Squad);
		return;
	}
	Squad->OnSquadFullyLoaded.AddWeakLambda(this, [this, RunId](ASquadController* LoadedSquad)
	{
		OnSquadReadyToJump(RunId, LoadedSquad);
	});
}

void UGA_Paratroopers::OnSquadReadyToJump(const int32 RunId, ASquadController* Squad)
{
	FParatrooperDropRun* Run = FindRun(RunId);
	if (not Run || not IsValid(Squad))
	{
		return;
	}
	FParatrooperSquadInDrop* SquadInDrop = Run->SquadsInDrop.FindByPredicate(
		[Squad](const FParatrooperSquadInDrop& Entry)
		{
			return Entry.Squad.Get() == Squad;
		});
	if (not SquadInDrop || SquadInDrop->bM_HasQueuedUnits)
	{
		return;
	}

	SquadInDrop->bM_HasQueuedUnits = true;
	for (ASquadUnit* SquadUnit : Squad->GetSquadUnitsChecked())
	{
		FParatrooperWaitingToJump WaitingUnit;
		WaitingUnit.SquadUnit = SquadUnit;
		WaitingUnit.Squad = Squad;
		Run->UnitsWaitingToJump.Add(WaitingUnit);
		++Run->QueuedUnitCount;
		++SquadInDrop->UnitsNotLanded;
	}
}

void UGA_Paratroopers::OnJumpTimer(const int32 RunId)
{
	FParatrooperDropRun* Run = FindRun(RunId);
	if (not Run)
	{
		return;
	}
	if (not Run->UnitsWaitingToJump.IsEmpty())
	{
		JumpNextUnit(*Run);
	}
	ReleaseAircraftHoldIfAllJumped(*Run);
	FinishRunIfDone(RunId);
}

void UGA_Paratroopers::JumpNextUnit(FParatrooperDropRun& Run)
{
	const FParatrooperWaitingToJump WaitingUnit = Run.UnitsWaitingToJump[0];
	Run.UnitsWaitingToJump.RemoveAt(0);
	ASquadUnit* SquadUnit = WaitingUnit.SquadUnit.Get();
	if (not IsValid(SquadUnit) || not SquadUnit->IsUnitAlive())
	{
		MarkUnitDone(Run, WaitingUnit.Squad);
		return;
	}

	UParachuteDescentComponent* DescentComponent = NewObject<UParachuteDescentComponent>(SquadUnit);
	DescentComponent->RegisterComponent();
	const int32 RunId = Run.RunId;
	const TWeakObjectPtr<ASquadController> WeakSquad = WaitingUnit.Squad;
	DescentComponent->OnDescentFinished.BindWeakLambda(this, [this, RunId, WeakSquad](ASquadUnit*, bool)
	{
		OnParatrooperDescentFinished(RunId, WeakSquad);
	});
	const FVector LandingLocation = GetLandingLocation(Run, SquadUnit, Run.JumpedUnitCount);
	const FVector StartLocation = GetJumpStartLocation(Run);
	const AAircraftMaster* Aircraft = Run.Aircraft.Get();
	UE_LOG(LogTemp, Display, TEXT("RTS_PARATROOPER_JUMP run=%d index=%d unit=%s source=%s past_target_cm=%.0f start=%s landing=%s"),
		Run.RunId, Run.JumpedUnitCount, *SquadUnit->GetName(), IsValid(Aircraft) ? TEXT("aircraft") : TEXT("fallback"),
		IsValid(Aircraft)
			? FVector::DotProduct(Aircraft->GetActorLocation() - Run.PassOverLocation, Run.RunDirection)
			: 0.0f, *StartLocation.ToString(), *LandingLocation.ToString());
	++Run.JumpedUnitCount;
	// May finish synchronously (invalid owner) and re-enter through OnParatrooperDescentFinished: Run is not used after.
	DescentComponent->StartDescent(
		M_ParatroopersSettings.DescentSettings,
		StartLocation,
		LandingLocation,
		Run.RunDirection.Rotation().Yaw);
}

void UGA_Paratroopers::ReleaseAircraftHoldIfAllJumped(const FParatrooperDropRun& Run) const
{
	AAircraftMaster* Aircraft = Run.Aircraft.Get();
	if (not IsValid(Aircraft) || not GetHasEverySoldierJumped(Run))
	{
		return;
	}
	Aircraft->SetStraightRunHoldAtEnd(false);
}

bool UGA_Paratroopers::GetHasEverySoldierJumped(const FParatrooperDropRun& Run) const
{
	if (not Run.bM_HasReachedTarget || Run.PendingSquadSpawns > 0 || not Run.UnitsWaitingToJump.IsEmpty())
	{
		return false;
	}
	for (const FParatrooperSquadInDrop& SquadInDrop : Run.SquadsInDrop)
	{
		// A squad that is still loading its soldiers has not queued them yet.
		if (SquadInDrop.Squad.IsValid() && not SquadInDrop.bM_HasQueuedUnits)
		{
			return false;
		}
	}
	return true;
}

FVector UGA_Paratroopers::GetJumpStartLocation(const FParatrooperDropRun& Run) const
{
	const FVector BelowOffset(0.0f, 0.0f, M_ParatroopersSettings.JumpBelowAircraftOffset);
	AAircraftMaster* Aircraft = Run.Aircraft.Get();
	if (not IsValid(Aircraft) || not Aircraft->IsUnitAlive())
	{
		return Run.PassOverLocation - BelowOffset;
	}
	return Aircraft->GetActorLocation() - BelowOffset
		- Run.RunDirection * ParatroopersConstants::JumpBehindAircraftOffset;
}

FVector UGA_Paratroopers::GetLandingLocation(
	const FParatrooperDropRun& Run,
	const ASquadUnit* SquadUnit,
	const int32 JumpIndex) const
{
	const float SpiralRadius = FMath::Min(
		M_ParatroopersSettings.LandingSpreadRadius,
		ParatroopersConstants::LandingSpiralSpacing * FMath::Sqrt(static_cast<float>(JumpIndex) + 0.5f));
	const float SpiralAngle = static_cast<float>(JumpIndex) * ParatroopersConstants::GoldenAngleRadians;
	const FVector DesiredLocation = GetLandingTrailOrigin(Run, JumpIndex) +
		FVector(FMath::Cos(SpiralAngle), FMath::Sin(SpiralAngle), 0.0f) * SpiralRadius;
	bool bWasProjected = false;
	FVector GroundLocation = URTSBlueprintFunctionLibrary::RTSProjectLocationToNavigation(
		this,
		DesiredLocation,
		ParatroopersConstants::NavProjectionExtentScale,
		bWasProjected);

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		const FVector TraceStart(GroundLocation.X, GroundLocation.Y, Run.PassOverLocation.Z);
		const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, ParatroopersConstants::LandscapeTraceDepth);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ParatrooperLandingTrace), false);
		QueryParams.AddIgnoredActor(SquadUnit);
		FHitResult LandscapeHit;
		if (World->LineTraceSingleByChannel(LandscapeHit, TraceStart, TraceEnd, COLLISION_TRACE_LANDSCAPE, QueryParams))
		{
			GroundLocation = LandscapeHit.ImpactPoint;
		}
	}

	const UCapsuleComponent* Capsule = SquadUnit->GetCapsuleComponent();
	const float HalfHeight = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;
	return GroundLocation + FVector(0.0f, 0.0f, HalfHeight);
}

FVector UGA_Paratroopers::GetLandingTrailOrigin(const FParatrooperDropRun& Run, const int32 JumpIndex) const
{
	if (M_CinematicLandingTrailLength <= 0.f)
	{
		return Run.TargetLocation;
	}
	const float TrailFraction = static_cast<float>(JumpIndex) / FMath::Max(1, Run.QueuedUnitCount - 1);
	return Run.TargetLocation + Run.RunDirection * M_CinematicLandingTrailLength * TrailFraction;
}

void UGA_Paratroopers::OnParatrooperDescentFinished(const int32 RunId, TWeakObjectPtr<ASquadController> Squad)
{
	FParatrooperDropRun* Run = FindRun(RunId);
	if (not Run)
	{
		return;
	}
	MarkUnitDone(*Run, Squad);
}

void UGA_Paratroopers::MarkUnitDone(FParatrooperDropRun& Run, const TWeakObjectPtr<ASquadController>& Squad)
{
	for (FParatrooperSquadInDrop& SquadInDrop : Run.SquadsInDrop)
	{
		if (SquadInDrop.Squad == Squad)
		{
			SquadInDrop.UnitsNotLanded = FMath::Max(0, SquadInDrop.UnitsNotLanded - 1);
			break;
		}
	}
	ReleaseFinishedSquads(Run);
}

void UGA_Paratroopers::ReleaseFinishedSquads(FParatrooperDropRun& Run) const
{
	// Collect first: releasing a squad sets it idle, which runs squad callbacks outside this ability.
	TArray<ASquadController*> SquadsToRelease;
	for (int32 Index = Run.SquadsInDrop.Num() - 1; Index >= 0; --Index)
	{
		const FParatrooperSquadInDrop& SquadInDrop = Run.SquadsInDrop[Index];
		ASquadController* Squad = SquadInDrop.Squad.Get();
		if (not IsValid(Squad))
		{
			Run.SquadsInDrop.RemoveAt(Index);
			continue;
		}
		if (SquadInDrop.bM_HasQueuedUnits && SquadInDrop.UnitsNotLanded <= 0)
		{
			SquadsToRelease.Add(Squad);
			Run.SquadsInDrop.RemoveAt(Index);
		}
	}
	for (ASquadController* Squad : SquadsToRelease)
	{
		ReleaseSquad(Squad);
	}
}

void UGA_Paratroopers::ReleaseSquad(ASquadController* Squad) const
{
	if (not IsValid(Squad))
	{
		return;
	}
	Squad->EndParachuteDrop();
	if (GetOwningPlayer() == ParatroopersConstants::PlayerOwner)
	{
		return;
	}
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	if (not IsValid(EnemyController))
	{
		return;
	}
	if (UEnemyStrategicAIComponent* StrategicAI = EnemyController->GetEnemyStrategicAIComponent())
	{
		StrategicAI->RegisterDroppedUnitWithBlackboardWhenReady(Squad);
	}
}

bool UGA_Paratroopers::GetIsRunFinished(const FParatrooperDropRun& Run) const
{
	const bool bAircraftGone = not Run.Aircraft.IsValid();
	return bAircraftGone
		&& Run.PendingSquadSpawns <= 0
		&& Run.UnitsWaitingToJump.IsEmpty()
		&& Run.SquadsInDrop.IsEmpty();
}

void UGA_Paratroopers::FinishRunIfDone(const int32 RunId)
{
	const int32 RunIndex = M_DropRuns.IndexOfByPredicate([RunId](const FParatrooperDropRun& Run)
	{
		return Run.RunId == RunId;
	});
	if (not M_DropRuns.IsValidIndex(RunIndex))
	{
		BroadcastParatroopersFinishedIfNeeded();
		return;
	}
	ReleaseFinishedSquads(M_DropRuns[RunIndex]);
	if (not GetIsRunFinished(M_DropRuns[RunIndex]))
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_DropRuns[RunIndex].JumpTimerHandle);
	}
	M_DropRuns.RemoveAt(RunIndex);
	BroadcastParatroopersFinishedIfNeeded();
}

void UGA_Paratroopers::OnSpawnedAircraftDestroyed(AActor* DestroyedActor)
{
	TArray<int32> RunIdsOfAircraft;
	for (const FParatrooperDropRun& Run : M_DropRuns)
	{
		if (Run.Aircraft.Get() == DestroyedActor)
		{
			RunIdsOfAircraft.Add(Run.RunId);
		}
	}
	for (const int32 RunId : RunIdsOfAircraft)
	{
		if (FParatrooperDropRun* Run = FindRun(RunId))
		{
			// The weak pointer still resolves during OnDestroyed; a run shot down before the target ends here.
			Run->Aircraft.Reset();
		}
		FinishRunIfDone(RunId);
	}
}

void UGA_Paratroopers::BroadcastParatroopersFinishedIfNeeded()
{
	if (not M_DropRuns.IsEmpty())
	{
		return;
	}
	BroadcastAbilityEnded();
}
