// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ConvoyPressureDirector.h"

#include "DormantDefence/DormantDefenceSlot.h"
#include "RTS_Survival/Enemy/AirDrop/EnemyAirDrop.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRouteActor/EnemyConvoyRoute.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawner.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Enemy/Strongpoint/EnemyStrongpoint.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace ConvoyPressureDirectorConstants
{
	// Amber: an alarm that grows; set apart from the other placed enemy actors.
	const FColor DirectorColor(255, 150, 30);
	constexpr float LinkBrightness = 0.55f;
	constexpr float LinkDashSize = 120.f;
	constexpr float SpawnPointRadius = 150.f;

	const TArray<int32> DefaultStoppedConvoysPerTier = {1, 3, 5};
	constexpr float DefaultAmmoSupplySecondsPerTruck = 60.f;
	constexpr int32 DefaultFuelPlacementActorsPerConvoy = 1;
	constexpr int32 DefaultConcreteDefencesPerTruck = 1;
}

AConvoyPressureDirector::AConvoyPressureDirector()
{
	using namespace ConvoyPressureDirectorConstants;
	PrimaryActorTick.bCanEverTick = false;
	StoppedConvoysPerTier = DefaultStoppedConvoysPerTier;
	TierResponses.SetNum(DefaultStoppedConvoysPerTier.Num());

	FConvoyPressureCargoResponse FuelResponse;
	FuelResponse.CargoType = EConvoyCargoType::Fuel;
	FuelResponse.PlacementActorsPerEscapedConvoy = DefaultFuelPlacementActorsPerConvoy;
	CargoResponses.Add(FuelResponse);

	FConvoyPressureCargoResponse AmmoResponse;
	AmmoResponse.CargoType = EConvoyCargoType::Ammo;
	AmmoResponse.AmmoSupplySecondsPerEscapedTruck = DefaultAmmoSupplySecondsPerTruck;
	CargoResponses.Add(AmmoResponse);

	FConvoyPressureCargoResponse ConcreteResponse;
	ConcreteResponse.CargoType = EConvoyCargoType::Concrete;
	ConcreteResponse.DormantDefencesPerEscapedTruck = DefaultConcreteDefencesPerTruck;
	CargoResponses.Add(ConcreteResponse);
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

void AConvoyPressureDirector::ForceAlertTier(const int32 NewTier)
{
	const int32 ClampedTier = FMath::Clamp(NewTier, 0, GetMaxAlertTier());
	if (ClampedTier > M_AlertTier)
	{
		StepAlertTierUpTo(ClampedTier);
		return;
	}
	if (ClampedTier == M_AlertTier)
	{
		return;
	}
	const int32 OldTier = M_AlertTier;
	M_AlertTier = ClampedTier;
	// A stepping loop that is running (this call came from its broadcast) stops at the lowered tier.
	M_TargetAlertTier = ClampedTier;
	OnAlertTierChanged.Broadcast(ClampedTier, OldTier);
}

void AConvoyPressureDirector::AddStoppedConvoy()
{
	M_StoppedConvoyCount++;
	StepAlertTierUpTo(GetTierForStoppedCount(M_StoppedConvoyCount));
}

void AConvoyPressureDirector::ApplyEscapedCargo(const EConvoyCargoType Cargo, const int32 Trucks,
                                                const int32 ConvoyIndex)
{
	M_EscapedConvoyCount++;
	ApplyEscapedCargoType(Cargo, FMath::Max(Trucks, 1), ConvoyIndex);
}

FColor AConvoyPressureDirector::GetPlacementColor() const
{
	return ConvoyPressureDirectorConstants::DirectorColor;
}

FString AConvoyPressureDirector::GetConfigurationProblem() const
{
	if (WatchedRoutes.IsEmpty())
	{
		return TEXT("no watched routes");
	}
	for (int32 TierIndex = 1; TierIndex < StoppedConvoysPerTier.Num(); ++TierIndex)
	{
		if (StoppedConvoysPerTier[TierIndex] <= StoppedConvoysPerTier[TierIndex - 1])
		{
			return TEXT("tier thresholds must rise");
		}
	}
	if (TierResponses.Num() > StoppedConvoysPerTier.Num())
	{
		return TEXT("more tier responses than tiers");
	}
	for (const FConvoyPressureTierResponse& Response : TierResponses)
	{
		const FConvoyPressureTargetedAttack& Attack = Response.TargetedAttack;
		if (Attack.bEnabled && (Attack.Targets.IsEmpty() || Attack.Units.IsEmpty() || Attack.SpawnPoints.IsEmpty()))
		{
			return TEXT("targeted attack needs targets, units and spawn points");
		}
	}
	return FString();
}

// ------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------

void AConvoyPressureDirector::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_BindWatchedRoutes();
}

void AConvoyPressureDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindWatchedRoutes();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TargetedAttackRetryTimerHandle);
	}
	M_PendingTargetedAttacks.Empty();
	Super::EndPlay(EndPlayReason);
}

void AConvoyPressureDirector::BeginPlay_BindWatchedRoutes()
{
	for (AEnemyConvoyRoute* Route : WatchedRoutes)
	{
		if (not IsValid(Route))
		{
			RTSFunctionLibrary::ReportError("Convoy pressure director " + GetName() + " has an empty watched route.");
			continue;
		}
		if (M_WatchedRoutes.Contains(Route))
		{
			// A route listed twice would count each of its convoys twice.
			continue;
		}
		M_WatchedRoutes.Add(Route);
		Route->OnConvoyCargoOutcome.AddUniqueDynamic(this, &AConvoyPressureDirector::HandleRouteConvoyCargoOutcome);
	}
}

void AConvoyPressureDirector::UnbindWatchedRoutes()
{
	for (const TWeakObjectPtr<AEnemyConvoyRoute>& WeakRoute : M_WatchedRoutes)
	{
		AEnemyConvoyRoute* Route = WeakRoute.Get();
		if (not IsValid(Route))
		{
			continue;
		}
		Route->OnConvoyCargoOutcome.RemoveDynamic(this, &AConvoyPressureDirector::HandleRouteConvoyCargoOutcome);
	}
	M_WatchedRoutes.Empty();
}

// ------------------------------------------------------------------
// Route callbacks
// ------------------------------------------------------------------

void AConvoyPressureDirector::HandleRouteConvoyCargoOutcome(AEnemyConvoyRoute* Route, AConvoy* Convoy,
                                                            const FConvoyCargoOutcome& Outcome)
{
	if (Outcome.Outcome == EConvoyOutcome::Escaped)
	{
		HandleConvoyEscaped(Outcome);
		return;
	}
	if (GetIsStoppedOutcome(Outcome.Outcome))
	{
		AddStoppedConvoy();
	}
}

bool AConvoyPressureDirector::GetIsStoppedOutcome(const EConvoyOutcome Outcome) const
{
	if (Outcome == EConvoyOutcome::TrucksStopped)
	{
		return true;
	}
	return Outcome == EConvoyOutcome::TrucksDestroyed && bCountDestroyedConvoysAsStopped;
}

void AConvoyPressureDirector::HandleConvoyEscaped(const FConvoyCargoOutcome& Outcome)
{
	M_EscapedConvoyCount++;
	// Copied before anything is applied: the responses below can run Blueprint code.
	const TArray<FConvoyCargoTypeOutcome> EscapedByCargo = GetEscapedTrucksByCargoType(Outcome);
	for (const FConvoyCargoTypeOutcome& CargoOutcome : EscapedByCargo)
	{
		ApplyEscapedCargoType(CargoOutcome.CargoType, CargoOutcome.EscapedTrucks, Outcome.ConvoyIndex);
	}
}

TArray<FConvoyCargoTypeOutcome> AConvoyPressureDirector::GetEscapedTrucksByCargoType(const FConvoyCargoOutcome& Outcome)
{
	TArray<FConvoyCargoTypeOutcome> EscapedByCargo;
	// Mixed columns carry several cargo types; each escaped type strengthens the enemy on its own.
	for (const FConvoyCargoTypeOutcome& CargoOutcome : Outcome.ByCargoType)
	{
		if (CargoOutcome.EscapedTrucks > 0)
		{
			EscapedByCargo.Add(CargoOutcome);
		}
	}
	if (not EscapedByCargo.IsEmpty())
	{
		return EscapedByCargo;
	}
	// No per-type tally: the definition's cargo with the convoy's escaped trucks.
	FConvoyCargoTypeOutcome& Fallback = EscapedByCargo.AddDefaulted_GetRef();
	Fallback.CargoType = Outcome.CargoType;
	Fallback.EscapedTrucks = FMath::Max(Outcome.EscapedTrucks, 1);
	return EscapedByCargo;
}

void AConvoyPressureDirector::ApplyEscapedCargoType(const EConvoyCargoType Cargo, const int32 Trucks,
                                                    const int32 ConvoyIndex)
{
	M_EscapedTruckCount += Trucks;
	UE_LOG(LogConvoy, Log, TEXT("Convoy pressure %s: convoy %d escaped with %d %s trucks (%d escaped convoys)."),
	       *GetName(), ConvoyIndex, Trucks, *ConvoyNames::GetCargoName(Cargo), M_EscapedConvoyCount);
	float AmmoSeconds = 0.f;
	for (const FConvoyPressureCargoResponse& Response : CargoResponses)
	{
		if (Response.CargoType != Cargo)
		{
			continue;
		}
		ApplyCargoResponse(Response, Trucks);
		AmmoSeconds += Response.AmmoSupplySecondsPerEscapedTruck * Trucks;
	}
	OnEscapedCargo.Broadcast(Cargo, Trucks, ConvoyIndex);
	if (AmmoSeconds > 0.f)
	{
		OnEscapedAmmoSeconds.Broadcast(AmmoSeconds);
	}
}

// ------------------------------------------------------------------
// Alert tiers
// ------------------------------------------------------------------

int32 AConvoyPressureDirector::GetMaxAlertTier() const
{
	return StoppedConvoysPerTier.Num();
}

int32 AConvoyPressureDirector::GetTierForStoppedCount(const int32 StoppedCount) const
{
	int32 Tier = 0;
	for (const int32 Threshold : StoppedConvoysPerTier)
	{
		if (StoppedCount >= Threshold)
		{
			Tier++;
		}
	}
	return Tier;
}

void AConvoyPressureDirector::StepAlertTierUpTo(const int32 WantedTier)
{
	M_TargetAlertTier = FMath::Max(M_TargetAlertTier, FMath::Clamp(WantedTier, 0, GetMaxAlertTier()));
	if (bM_IsSteppingAlertTier)
	{
		// Called from a handler of the loop below; the loop climbs to the raised target.
		return;
	}
	TGuardValue<bool> SteppingGuard(bM_IsSteppingAlertTier, true);
	// One tier at a time, so a jump (two convoys in one frame, ForceAlertTier) still runs every tier's response.
	while (M_AlertTier < M_TargetAlertTier)
	{
		const int32 OldTier = M_AlertTier;
		M_AlertTier = OldTier + 1;
		RunTierResponseOnce(M_AlertTier);
		OnAlertTierChanged.Broadcast(M_AlertTier, OldTier);
	}
}

void AConvoyPressureDirector::RunTierResponseOnce(const int32 Tier)
{
	if (M_TiersWithResponseRun.Contains(Tier))
	{
		return;
	}
	M_TiersWithResponseRun.Add(Tier);
	const int32 TierIndex = Tier - 1;
	if (not TierResponses.IsValidIndex(TierIndex))
	{
		UE_LOG(LogConvoy, Log, TEXT("Convoy pressure %s: alert tier %d has no response."), *GetName(), Tier);
		return;
	}
	UE_LOG(LogConvoy, Log, TEXT("Convoy pressure %s: alert tier %d (%s) after %d stopped convoys."), *GetName(), Tier,
	       *TierResponses[TierIndex].TierName.ToString(), M_StoppedConvoyCount);
	// Copied: starting a placement actor can run Blueprint code before the loop ends.
	const TArray<TObjectPtr<AEnemyPlacementActor>> PlacementActors = TierResponses[TierIndex].PlacementActorsToStart;
	for (AEnemyPlacementActor* PlacementActor : PlacementActors)
	{
		StartPlacementActorOnce(PlacementActor);
	}
	if (not TierResponses[TierIndex].TargetedAttack.bEnabled)
	{
		return;
	}
	if (TryLaunchTargetedAttack(TierIndex) == EConvoyPressureAttackLaunch::NoTargetYet)
	{
		QueueTargetedAttackRetry(TierIndex);
	}
}

// ------------------------------------------------------------------
// Targeted attacks
// ------------------------------------------------------------------

EConvoyPressureAttackLaunch AConvoyPressureDirector::TryLaunchTargetedAttack(const int32 TierIndex)
{
	if (not TierResponses.IsValidIndex(TierIndex))
	{
		return EConvoyPressureAttackLaunch::Failed;
	}
	const FConvoyPressureTargetedAttack& Attack = TierResponses[TierIndex].TargetedAttack;
	const TArray<FAttackWaveElement> Elements = BuildTargetedAttackElements(Attack);
	if (Elements.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Convoy pressure director " + GetName() + ": the targeted attack of tier "
			+ FString::FromInt(TierIndex + 1) + " has no units; it does not start.");
		return EConvoyPressureAttackLaunch::Failed;
	}
	FVector StartLocation = FVector::ZeroVector;
	for (const FAttackWaveElement& Element : Elements)
	{
		StartLocation += Element.SpawnLocation;
	}
	StartLocation /= Elements.Num();
	const TArray<FVector> Waypoints = BuildTargetedAttackWaypoints(Attack, StartLocation);
	if (Waypoints.IsEmpty())
	{
		UE_LOG(LogConvoy, Log, TEXT("Convoy pressure %s: tier %d attack waits; no target is alive and hostile."),
		       *GetName(), TierIndex + 1);
		return EConvoyPressureAttackLaunch::NoTargetYet;
	}
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyControllerForPlayer(this, GetEnemyPlayer());
	if (not IsValid(EnemyController))
	{
		RTSFunctionLibrary::ReportError("Convoy pressure director " + GetName()
			+ " found no enemy controller for its enemy player; the targeted attack does not start.");
		return EConvoyPressureAttackLaunch::Failed;
	}
	FAttackWaveRequest Request;
	Request.WaveType = EEnemyWaveType::Wave_NoOwningBuilding;
	Request.WaveElements = Elements;
	Request.IterationCount = 1;
	Request.FirstIterationDelay = 0.f;
	FillTargetedAttackMovement(Attack, Waypoints, StartLocation, Request);
	const int32 WaveId = EnemyController->StartWaveFromRequest(Request);
	if (WaveId == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Convoy pressure director " + GetName()
			+ " could not start the targeted attack of tier " + FString::FromInt(TierIndex + 1) + "; see above.");
		return EConvoyPressureAttackLaunch::Failed;
	}
	UE_LOG(LogConvoy, Log, TEXT("Convoy pressure %s: tier %d attack started (%d units, %d targets)."), *GetName(),
	       TierIndex + 1, Elements.Num(), Waypoints.Num());
	return EConvoyPressureAttackLaunch::Launched;
}

TArray<FAttackWaveElement> AConvoyPressureDirector::BuildTargetedAttackElements(
	const FConvoyPressureTargetedAttack& Attack) const
{
	TArray<FAttackWaveElement> Elements;
	const TArray<FVector> WorldSpawnPoints = GetWorldPoints(Attack.SpawnPoints);
	if (WorldSpawnPoints.IsEmpty())
	{
		return Elements;
	}
	UWorld* World = GetWorld();
	TArray<int32> UsesPerSpawnPoint;
	UsesPerSpawnPoint.Init(0, WorldSpawnPoints.Num());
	int32 NextSpreadPoint = 0;
	for (const FEnemyWaveSpawnerUnitEntry& Entry : Attack.Units)
	{
		const TArray<FTrainingOption> Options = EnemyPlacement::ToTrainingOptions(Entry.UnitOptions);
		if (Options.IsEmpty())
		{
			continue;
		}
		for (int32 UnitIndex = 0; UnitIndex < Entry.Count; ++UnitIndex)
		{
			const bool bHasFixedPoint = WorldSpawnPoints.IsValidIndex(Entry.SpawnPointIndex);
			const int32 PointIndex = bHasFixedPoint ? Entry.SpawnPointIndex : NextSpreadPoint++ % WorldSpawnPoints.Num();
			const FVector Spread = EnemyPlacement::GetRingSpreadOffset(UsesPerSpawnPoint[PointIndex]++);
			FAttackWaveElement& Element = Elements.AddDefaulted_GetRef();
			Element.SpawnLocation = EnemyPlacement::ProjectToNavigationIfPossible(
				World, WorldSpawnPoints[PointIndex] + Spread);
			Element.UnitOptions = Options;
		}
	}
	return Elements;
}

TArray<FVector> AConvoyPressureDirector::BuildTargetedAttackWaypoints(const FConvoyPressureTargetedAttack& Attack,
                                                                      const FVector& StartLocation) const
{
	TArray<FVector> Remaining;
	for (AActor* Target : Attack.Targets)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Target))
		{
			continue;
		}
		if (Attack.bOnlyHostileTargets && not FRTSPlayers::IsActorHostileToPlayer(Target, GetEnemyPlayer()))
		{
			continue;
		}
		Remaining.Add(Target->GetActorLocation());
	}
	UWorld* World = GetWorld();
	TArray<FVector> Waypoints;
	FVector From = StartLocation;
	while (not Remaining.IsEmpty())
	{
		const int32 NearestIndex = GetNearestPointIndex2D(From, Remaining);
		From = Remaining[NearestIndex];
		Remaining.RemoveAtSwap(NearestIndex);
		Waypoints.Add(EnemyPlacement::ProjectToNavigationIfPossible(World, From));
	}
	return Waypoints;
}

int32 AConvoyPressureDirector::GetNearestPointIndex2D(const FVector& From, const TArray<FVector>& Points)
{
	int32 NearestIndex = 0;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		if (FVector::DistSquared2D(From, Points[Index]) < FVector::DistSquared2D(From, Points[NearestIndex]))
		{
			NearestIndex = Index;
		}
	}
	return NearestIndex;
}

void AConvoyPressureDirector::FillTargetedAttackMovement(const FConvoyPressureTargetedAttack& Attack,
                                                         const TArray<FVector>& Waypoints,
                                                         const FVector& StartLocation,
                                                         FAttackWaveRequest& OutRequest) const
{
	const FEnemyWaveSpawnerAttackSettings& Settings = Attack.AttackSettings;
	OutRequest.Movement = EAttackWaveMovement::AttackMove;
	OutRequest.Waypoints = Waypoints;
	OutRequest.MaxFormationWidth = Settings.Formation.MaxFormationWidth;
	OutRequest.FormationOffsetMultiplier = Settings.Formation.FormationOffsetMultiplier;
	OutRequest.AttackMoveSettings = Settings.CombatSupport;
	OutRequest.FinalWaypointDirection = Settings.FinalWaypointDirection;
	if (not Settings.bFaceAlongLastLeg || Waypoints.IsEmpty())
	{
		return;
	}
	const FVector LegStart = Waypoints.Num() >= 2 ? Waypoints[Waypoints.Num() - 2] : StartLocation;
	OutRequest.FinalWaypointDirection = FRotator(0.f, (Waypoints.Last() - LegStart).Rotation().Yaw, 0.f);
}

void AConvoyPressureDirector::QueueTargetedAttackRetry(const int32 TierIndex)
{
	if (TargetedAttackRetrySeconds <= 0.f)
	{
		return;
	}
	M_PendingTargetedAttacks.AddUnique(TierIndex);
	UWorld* World = GetWorld();
	if (not IsValid(World) || World->GetTimerManager().IsTimerActive(M_TargetedAttackRetryTimerHandle))
	{
		return;
	}
	TWeakObjectPtr<AConvoyPressureDirector> WeakThis(this);
	World->GetTimerManager().SetTimer(M_TargetedAttackRetryTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->RetryPendingTargetedAttacks();
		}
	}), TargetedAttackRetrySeconds, true);
}

void AConvoyPressureDirector::RetryPendingTargetedAttacks()
{
	// Copied: a launch can run Blueprint code that raises the alert and queues another attack.
	const TArray<int32> Pending = M_PendingTargetedAttacks;
	for (const int32 TierIndex : Pending)
	{
		if (TryLaunchTargetedAttack(TierIndex) != EConvoyPressureAttackLaunch::NoTargetYet)
		{
			M_PendingTargetedAttacks.Remove(TierIndex);
		}
	}
	if (not M_PendingTargetedAttacks.IsEmpty())
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TargetedAttackRetryTimerHandle);
	}
}

// ------------------------------------------------------------------
// Escaped cargo
// ------------------------------------------------------------------

void AConvoyPressureDirector::ApplyCargoResponse(const FConvoyPressureCargoResponse& Response, const int32 Trucks)
{
	if (Response.PlacementActorsPerEscapedConvoy > 0)
	{
		const int32 Started = StartNextPlacementActors(Response.PlacementActorsToStart,
		                                               Response.PlacementActorsPerEscapedConvoy);
		if (Started < Response.PlacementActorsPerEscapedConvoy)
		{
			UE_LOG(LogConvoy, Log, TEXT("Convoy pressure %s: %s cargo started %d of %d placement actors; list used up."),
			       *GetName(), *ConvoyNames::GetCargoName(Response.CargoType), Started,
			       Response.PlacementActorsPerEscapedConvoy);
		}
	}
	const int32 WantedDefences = Response.DormantDefencesPerEscapedTruck * Trucks;
	if (WantedDefences <= 0)
	{
		return;
	}
	const int32 Activated = ActivateNextDormantDefences(Response.DormantDefences, WantedDefences);
	if (Activated < WantedDefences)
	{
		UE_LOG(LogConvoy, Log, TEXT("Convoy pressure %s: %s cargo switched on %d of %d dormant defences; list used up."),
		       *GetName(), *ConvoyNames::GetCargoName(Response.CargoType), Activated, WantedDefences);
	}
}

int32 AConvoyPressureDirector::StartNextPlacementActors(const TArray<TObjectPtr<AEnemyPlacementActor>>& Candidates,
                                                        const int32 Count)
{
	// Copied: the list lives in a settings struct that a started actor's Blueprint code could touch.
	const TArray<TObjectPtr<AEnemyPlacementActor>> CandidatesCopy = Candidates;
	int32 Started = 0;
	for (AEnemyPlacementActor* Candidate : CandidatesCopy)
	{
		if (Started >= Count)
		{
			break;
		}
		if (StartPlacementActorOnce(Candidate))
		{
			Started++;
		}
	}
	return Started;
}

int32 AConvoyPressureDirector::ActivateNextDormantDefences(const TArray<TObjectPtr<ADormantDefenceSlot>>& Candidates,
                                                           const int32 Count)
{
	const TArray<TObjectPtr<ADormantDefenceSlot>> CandidatesCopy = Candidates;
	int32 Activated = 0;
	for (ADormantDefenceSlot* Slot : CandidatesCopy)
	{
		if (Activated >= Count)
		{
			break;
		}
		if (not IsValid(Slot) || Slot->GetIsActivated())
		{
			continue;
		}
		// A failed spawn is reported by the slot; the next slot takes its place.
		if (ActivateDormantDefence(*Slot))
		{
			Activated++;
		}
	}
	return Activated;
}

bool AConvoyPressureDirector::ActivateDormantDefence(ADormantDefenceSlot& Slot)
{
	AActor* Defence = Slot.Activate();
	if (not IsValid(Defence))
	{
		return false;
	}
	OnDormantDefenceActivated.Broadcast(Defence);
	return true;
}

// ------------------------------------------------------------------
// Placement actors
// ------------------------------------------------------------------

bool AConvoyPressureDirector::StartPlacementActorOnce(AEnemyPlacementActor* PlacementActor)
{
	if (not IsValid(PlacementActor) || GetWasPlacementActorStarted(PlacementActor))
	{
		return false;
	}
	// Recorded first: an actor of an unknown kind is reported once, not on every escaped convoy.
	M_StartedPlacementActors.Add(PlacementActor);
	return TryStartPlacementActor(*PlacementActor);
}

bool AConvoyPressureDirector::GetWasPlacementActorStarted(const AEnemyPlacementActor* PlacementActor) const
{
	return M_StartedPlacementActors.ContainsByPredicate(
		[PlacementActor](const TWeakObjectPtr<AEnemyPlacementActor>& Started)
		{
			return Started.Get() == PlacementActor;
		});
}

bool AConvoyPressureDirector::TryStartPlacementActor(AEnemyPlacementActor& PlacementActor)
{
	if (AEnemyWaveSpawner* Spawner = Cast<AEnemyWaveSpawner>(&PlacementActor))
	{
		Spawner->StartSpawner();
		return true;
	}
	if (AEnemyConvoyRoute* Route = Cast<AEnemyConvoyRoute>(&PlacementActor))
	{
		Route->StartRoute();
		return true;
	}
	if (AEnemyStrongpoint* Strongpoint = Cast<AEnemyStrongpoint>(&PlacementActor))
	{
		Strongpoint->SpawnGarrison();
		return true;
	}
	if (AEnemyAirDrop* AirDrop = Cast<AEnemyAirDrop>(&PlacementActor))
	{
		AirDrop->StartAirDrop();
		return true;
	}
	if (ADormantDefenceSlot* Slot = Cast<ADormantDefenceSlot>(&PlacementActor))
	{
		return ActivateDormantDefence(*Slot);
	}
	RTSFunctionLibrary::ReportError("Convoy pressure: " + PlacementActor.GetName()
		+ " is not a wave spawner, convoy route, strongpoint, air drop or dormant defence; it cannot be started.");
	return false;
}

// ------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------

void AConvoyPressureDirector::BuildEditorShapes(FEnemyPlacementShapes& OutSummary,
                                                FEnemyPlacementShapes& OutDetail) const
{
	using namespace ConvoyPressureDirectorConstants;
	const FColor LinkColor = EnemyPlacementColors::GetDimmed(DirectorColor, LinkBrightness);
	const FVector Origin = GetActorLocation();
	for (const AEnemyConvoyRoute* Route : WatchedRoutes)
	{
		if (IsValid(Route))
		{
			OutDetail.DashedLines.Emplace(Origin, Route->GetActorLocation(), LinkColor, LinkDashSize);
		}
	}
	for (const FConvoyPressureTierResponse& Response : TierResponses)
	{
		AddTargetedAttackShapes(Response.TargetedAttack, OutSummary, OutDetail);
	}
	for (const FConvoyPressureCargoResponse& Response : CargoResponses)
	{
		AddDormantDefenceLinkShapes(Response.DormantDefences, OutDetail);
	}
}

void AConvoyPressureDirector::AddTargetedAttackShapes(const FConvoyPressureTargetedAttack& Attack,
                                                      FEnemyPlacementShapes& OutSummary,
                                                      FEnemyPlacementShapes& OutDetail) const
{
	using namespace ConvoyPressureDirectorConstants;
	if (not Attack.bEnabled)
	{
		return;
	}
	const TArray<FVector> WorldSpawnPoints = GetWorldPoints(Attack.SpawnPoints);
	for (const FVector& SpawnPoint : WorldSpawnPoints)
	{
		OutSummary.Spheres.Emplace(SpawnPointRadius, SpawnPoint, FLinearColor(DirectorColor));
	}
	const FVector Origin = GetActorLocation();
	for (const AActor* Target : Attack.Targets)
	{
		if (IsValid(Target))
		{
			OutDetail.ArrowLines.Emplace(Origin, Target->GetActorLocation(), DirectorColor);
		}
	}
}

void AConvoyPressureDirector::AddDormantDefenceLinkShapes(const TArray<TObjectPtr<ADormantDefenceSlot>>& Slots,
                                                          FEnemyPlacementShapes& OutDetail) const
{
	using namespace ConvoyPressureDirectorConstants;
	const FColor LinkColor = EnemyPlacementColors::GetDimmed(DirectorColor, LinkBrightness);
	const FVector Origin = GetActorLocation();
	for (const ADormantDefenceSlot* Slot : Slots)
	{
		if (IsValid(Slot))
		{
			OutDetail.DashedLines.Emplace(Origin, Slot->GetActorLocation(), LinkColor, LinkDashSize);
		}
	}
}
