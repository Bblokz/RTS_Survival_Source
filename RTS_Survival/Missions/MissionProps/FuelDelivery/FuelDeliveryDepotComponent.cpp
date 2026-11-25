// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FuelDeliveryDepotComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "FuelDeliveryNodeComponent.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Missions/MissionProps/FuelCrane/MissionFuelCrane.h"
#include "RTS_Survival/Missions/MissionProps/FuelTruck/MissionFuelTruckSetup.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace FuelDeliveryDepotPrivate
{
	constexpr float UpdateIntervalSeconds = 0.5f;
	// The approach point only has to be passed, not hit.
	constexpr float ApproachPassTolerance = 450.f;
	// After this many orders for one leg a stopped truck near its goal counts as arrived.
	constexpr int32 OrdersBeforeLooseArrival = 3;
	constexpr float LooseHomeArrivalFactor = 3.f;
	// A driving truck must cover this much ground per window, or the window counts as stuck.
	constexpr double StuckWindowSeconds = 8.0;
	constexpr float StuckProgressCm = 150.f;
	// One-shot unload refused (truck out of the crane's reach): park again this often before giving up.
	constexpr int32 MaxParkRetries = 2;
	constexpr float SpawnGroundTraceCm = 5000.f;
	constexpr float SpawnLiftCm = 50.f;
	// No async spawner yet: try again this often, and report it only after this many tries.
	constexpr float SpawnerRetrySeconds = 3.f;
	constexpr int32 SpawnerWaitsBeforeError = 20;

}

UFuelDeliveryDepotComponent::UFuelDeliveryDepotComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	TruckHullHitboxMesh = FMissionFuelTruckSetup::GetDefaultZilHullHitboxMesh();
	TruckWreckDeath = FMissionFuelTruckSetup::MakeDefaultZilWreckDeath();
}

AMissionFuelCrane* UFuelDeliveryDepotComponent::GetCrane() const
{
	return M_Crane.Get();
}

// ---------------------------------------------------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------------------------------------------------

void UFuelDeliveryDepotComponent::BeginPlay()
{
	Super::BeginPlay();
	if (not bEnabled)
	{
		return;
	}
	if (not BeginPlay_FindNodes())
	{
		return;
	}
	BeginPlay_SpawnCrane();
	ScheduleNextTruck(FirstTruckDelaySeconds);
	StartUpdates();
}

void UFuelDeliveryDepotComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UpdateTimer);
	}
	if (M_Crane.IsValid())
	{
		M_Crane->OnTruckCargoEmptied.Remove(M_CraneEmptiedHandle);
	}
	// The crane is its own destructible prop and stays. A truck still out drives home on its own.
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		HandTruckHomeAfterDepotRemoved();
	}
	Super::EndPlay(EndPlayReason);
}

bool UFuelDeliveryDepotComponent::GetIsValidNodes() const
{
	return M_SpawnNode.IsValid() and M_ParkNode.IsValid() and M_CraneNode.IsValid();
}

bool UFuelDeliveryDepotComponent::BeginPlay_FindNodes()
{
	const AActor* const Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return false;
	}
	TInlineComponentArray<UFuelDeliveryNodeComponent*> Nodes(Owner);
	for (UFuelDeliveryNodeComponent* const Node : Nodes)
	{
		if (not IsValid(Node))
		{
			continue;
		}
		switch (Node->GetNodeRole())
		{
		case EFuelDeliveryNodeRole::TruckSpawn:
			M_SpawnNode = Node;
			break;
		case EFuelDeliveryNodeRole::TruckPark:
			M_ParkNode = Node;
			break;
		case EFuelDeliveryNodeRole::Crane:
			M_CraneNode = Node;
			break;
		default:
			break;
		}
	}
	if (GetIsValidNodes())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("FuelDeliveryDepot on %s needs one UFuelDeliveryNodeComponent per role (spawn %d, park %d, crane %d); "
			"no deliveries."), *Owner->GetName(), M_SpawnNode.IsValid(), M_ParkNode.IsValid(), M_CraneNode.IsValid()));
	SetState(EFuelDeliveryState::Stopped);
	return false;
}

void UFuelDeliveryDepotComponent::BeginPlay_SpawnCrane()
{
	UWorld* const World = GetWorld();
	UClass* const LoadedCraneClass = CraneClass.LoadSynchronous();
	if (World == nullptr || LoadedCraneClass == nullptr)
	{
		RTSFunctionLibrary::ReportError("FuelDeliveryDepot on " + GetNameSafe(GetOwner())
			+ " has no crane class; trucks will turn back without unloading.");
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform CraneTransform(FRotator(0.f, M_CraneNode->GetComponentRotation().Yaw, 0.f),
	                                M_CraneNode->GetComponentLocation());
	M_Crane = World->SpawnActor<AMissionFuelCrane>(LoadedCraneClass, CraneTransform, SpawnParameters);
	if (not M_Crane.IsValid())
	{
		RTSFunctionLibrary::ReportError("FuelDeliveryDepot on " + GetNameSafe(GetOwner())
			+ " could not spawn its crane.");
		return;
	}
	bM_HasSpawnedCrane = true;
	M_CraneEmptiedHandle = M_Crane->OnTruckCargoEmptied.AddUObject(
		this, &UFuelDeliveryDepotComponent::OnCraneEmptiedTruck);
}

void UFuelDeliveryDepotComponent::StartUpdates()
{
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<UFuelDeliveryDepotComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(M_UpdateTimer, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->Update();
		}
	}), FuelDeliveryDepotPrivate::UpdateIntervalSeconds, true);
}

// ---------------------------------------------------------------------------------------------------------------------
// Loop
// ---------------------------------------------------------------------------------------------------------------------

void UFuelDeliveryDepotComponent::Update()
{
	if (M_State != EFuelDeliveryState::Stopped and GetIsDepotLost())
	{
		OnDepotLost();
	}
	const bool bHasTruckPhase = M_State == EFuelDeliveryState::DrivingToApproach
		or M_State == EFuelDeliveryState::DrivingToPark or M_State == EFuelDeliveryState::Unloading
		or M_State == EFuelDeliveryState::DrivingHome;
	if (bHasTruckPhase and not GetIsTruckAlive())
	{
		OnTruckLost();
		return;
	}
	switch (M_State)
	{
	case EFuelDeliveryState::WaitingForTruck:
		if (const UWorld* const World = GetWorld(); World != nullptr and World->GetTimeSeconds() >= M_NextTruckSeconds)
		{
			SpawnTruck();
		}
		break;
	case EFuelDeliveryState::DrivingToApproach:
	case EFuelDeliveryState::DrivingToPark:
	case EFuelDeliveryState::DrivingHome:
		UpdateDriving();
		break;
	case EFuelDeliveryState::Unloading:
		UpdateUnloading();
		break;
	case EFuelDeliveryState::Stopped:
		// A truck that turned back still has to get home and disappear.
		if (M_Truck.IsValid() and GetIsTruckAlive())
		{
			UpdateDriving();
		}
		break;
	case EFuelDeliveryState::SpawningTruck:
	default:
		break;
	}
}

bool UFuelDeliveryDepotComponent::GetIsDepotLost() const
{
	if (not RTSFunctionLibrary::RTSIsValid(GetOwner()))
	{
		return true;
	}
	// A crane that never spawned is reported at BeginPlay; trucks then turn back at the park node.
	return bStopWhenCraneDestroyed and bM_HasSpawnedCrane and not RTSFunctionLibrary::RTSIsValidWeak(M_Crane);
}

void UFuelDeliveryDepotComponent::OnDepotLost()
{
	const bool bTruckOut = GetIsTruckAlive();
	if (M_State == EFuelDeliveryState::Unloading and RTSFunctionLibrary::RTSIsValidWeak(M_Crane))
	{
		M_Crane->StopUnloading();
	}
	SetState(EFuelDeliveryState::Stopped);
	if (bTruckOut)
	{
		DriveHome();
		// DriveHome sets the driving state; deliveries are over, so keep the stopped state for the way home.
		M_State = EFuelDeliveryState::Stopped;
	}
}

bool UFuelDeliveryDepotComponent::GetIsTruckAlive() const
{
	return RTSFunctionLibrary::RTSIsValidWeak(M_Truck);
}

void UFuelDeliveryDepotComponent::OnTruckLost()
{
	AActor* const LostTruck = M_Truck.Get();
	M_Truck.Reset();
	if (M_State == EFuelDeliveryState::Stopped)
	{
		return;
	}
	OnTruckDestroyed.Broadcast(LostTruck);
	ScheduleNextTruck(RespawnAfterDestroyedSeconds);
}

void UFuelDeliveryDepotComponent::SetState(const EFuelDeliveryState NewState)
{
	M_State = NewState;
	M_OrdersThisLeg = 0;
	bM_HasOrder = false;
	M_StuckWindowStartSeconds = -1.0;
	if (const UWorld* const World = GetWorld())
	{
		M_LastProgressSeconds = World->GetTimeSeconds();
		M_LegStartSeconds = World->GetTimeSeconds();
	}
}

void UFuelDeliveryDepotComponent::ScheduleNextTruck(const float DelaySeconds)
{
	SetState(EFuelDeliveryState::WaitingForTruck);
	const UWorld* const World = GetWorld();
	M_NextTruckSeconds = (World != nullptr ? World->GetTimeSeconds() : 0.0) + FMath::Max(0.f, DelaySeconds);
}

void UFuelDeliveryDepotComponent::SpawnTruck()
{
	using namespace FuelDeliveryDepotPrivate;
	ARTSAsyncSpawner* const Spawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(Spawner) and not TruckClass.IsNull() and M_SpawnerWaitCount < SpawnerWaitsBeforeError)
	{
		// A depot placed on the map begins play before the player's async spawner exists: wait for it quietly.
		++M_SpawnerWaitCount;
		ScheduleNextTruck(SpawnerRetrySeconds);
		return;
	}
	if (TruckClass.IsNull() or not IsValid(Spawner) or not M_SpawnNode.IsValid())
	{
		RTSFunctionLibrary::ReportError("FuelDeliveryDepot on " + GetNameSafe(GetOwner())
			+ " cannot spawn a truck (no truck class, async spawner or spawn node); trying again later.");
		ScheduleNextTruck(RespawnAfterDestroyedSeconds);
		return;
	}
	SetState(EFuelDeliveryState::SpawningTruck);
	// Nodes are dragged around by hand: put the truck on the ground under (or over) the spawn node.
	FVector SpawnLocation = M_SpawnNode->GetComponentLocation();
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FuelDeliverySpawnGround), false, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, SpawnLocation + FVector(0.f, 0.f, SpawnGroundTraceCm),
	                                         SpawnLocation - FVector(0.f, 0.f, SpawnGroundTraceCm),
	                                         COLLISION_TRACE_LANDSCAPE, QueryParams))
	{
		SpawnLocation.Z = GroundHit.ImpactPoint.Z + SpawnLiftCm;
	}
	const int32 OwningPlayer = FRTSPlayers::GetOwningPlayerOfActor(GetOwner());
	const FRotator SpawnRotation(0.f, M_SpawnNode->GetComponentRotation().Yaw, 0.f);
	TWeakObjectPtr<UFuelDeliveryDepotComponent> WeakThis(this);
	const bool bStarted = Spawner->AsyncSpawnClassAtLocation(
		TruckClass, FTrainingOption(), SpawnLocation, this, 0,
		[WeakThis](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnTruckSpawnedCallback(SpawnedActor);
			}
			else if (IsValid(SpawnedActor))
			{
				// The depot went away while the truck loaded: nobody drives it.
				SpawnedActor->Destroy();
			}
		}, SpawnRotation, FRTSUnitOwnerSpawning::GetOwnerOverrideForAIPlayer(OwningPlayer));
	if (not bStarted and M_State == EFuelDeliveryState::SpawningTruck)
	{
		ScheduleNextTruck(RespawnAfterDestroyedSeconds);
	}
}

void UFuelDeliveryDepotComponent::OnTruckSpawnedCallback(AActor* SpawnedTruck)
{
	if (M_State != EFuelDeliveryState::SpawningTruck)
	{
		// The depot was lost while the truck loaded.
		if (IsValid(SpawnedTruck))
		{
			SpawnedTruck->Destroy();
		}
		return;
	}
	if (not IsValid(SpawnedTruck))
	{
		RTSFunctionLibrary::ReportError("FuelDeliveryDepot on " + GetNameSafe(GetOwner())
			+ ": the fuel truck failed to spawn; trying again later.");
		ScheduleNextTruck(RespawnAfterDestroyedSeconds);
		return;
	}
	M_Truck = SpawnedTruck;
	M_SpawnerWaitCount = 0;
	const FString Context = "FuelDeliveryDepot on " + GetNameSafe(GetOwner());
	FMissionFuelTruckSetup::ApplyHitboxAndWreckDeath(SpawnedTruck, TruckHullHitboxMesh, TruckWreckDeath,
	                                                 M_PreloadedTruckDeathAssets, Context);
	if (not bSinkTruckWreck)
	{
		if (ATankMaster* const Tank = Cast<ATankMaster>(SpawnedTruck))
		{
			Tank->SetWreckSpawnedHook(nullptr);
		}
	}
	SetState(ApproachDistance > 0.f ? EFuelDeliveryState::DrivingToApproach : EFuelDeliveryState::DrivingToPark);
	OnTruckSpawned.Broadcast(SpawnedTruck);
}

// ---------------------------------------------------------------------------------------------------------------------
// Driving
// ---------------------------------------------------------------------------------------------------------------------

FVector UFuelDeliveryDepotComponent::GetApproachLocation() const
{
	return M_ParkNode->GetComponentLocation() - M_ParkNode->GetForwardVector().GetSafeNormal2D() * ApproachDistance;
}

FVector UFuelDeliveryDepotComponent::GetParkOrderLocation() const
{
	return M_ParkNode->GetComponentLocation()
		+ M_ParkNode->GetForwardVector().GetSafeNormal2D() * ParkStopShortDistance;
}

void UFuelDeliveryDepotComponent::UpdateDriving()
{
	if (not GetIsValidNodes() or not M_Truck.IsValid())
	{
		return;
	}
	if (GetHasReachedGoal())
	{
		OnGoalReached();
		return;
	}
	// GetIsUnitIdle is not const on the interface.
	ICommands* const Commands = Cast<ICommands>(M_Truck.Get());
	if (Commands == nullptr)
	{
		return;
	}
	if (not bM_HasOrder or Commands->GetIsUnitIdle())
	{
		IssueOrderForState();
	}
	const UWorld* const World = GetWorld();
	if (World != nullptr and World->GetTimeSeconds() - M_LegStartSeconds > MaxLegSeconds)
	{
		RecycleTruck(TEXT("did not finish its drive within MaxLegSeconds"));
		return;
	}
	UpdateStuckWatch();
}

void UFuelDeliveryDepotComponent::IssueOrderForState()
{
	const FRotator ParkRotation(0.f, M_ParkNode->GetComponentRotation().Yaw, 0.f);
	switch (M_State)
	{
	case EFuelDeliveryState::DrivingToApproach:
		IssueMoveOrder(GetApproachLocation(), ParkRotation, false);
		break;
	case EFuelDeliveryState::DrivingToPark:
		// The crane works along the bed: the heading matters, also after a reversing re-order.
		IssueMoveOrder(GetParkOrderLocation(), ParkRotation, true);
		break;
	default:
		IssueMoveOrder(M_SpawnNode->GetComponentLocation(),
		               FRotator(0.f, M_SpawnNode->GetComponentRotation().Yaw + 180.f, 0.f), false);
		break;
	}
}

bool UFuelDeliveryDepotComponent::GetHasReachedGoal() const
{
	using namespace FuelDeliveryDepotPrivate;
	AActor* const Truck = M_Truck.Get();
	ICommands* const Commands = Cast<ICommands>(Truck);
	const bool bIsIdle = Commands != nullptr and Commands->GetIsUnitIdle();
	const bool bIsStoppedAfterRetries = M_OrdersThisLeg >= OrdersBeforeLooseArrival and bIsIdle;
	const FVector TruckLocation = Truck->GetActorLocation();
	switch (M_State)
	{
	case EFuelDeliveryState::DrivingToApproach:
		return FVector::Dist2D(TruckLocation, GetApproachLocation()) <= ApproachPassTolerance
			or bIsStoppedAfterRetries;
	case EFuelDeliveryState::DrivingToPark:
		{
			// The order aims past the park node; what counts is how close the stopped truck is to the node itself.
			const float DistanceToPark = FVector::Dist2D(TruckLocation, M_ParkNode->GetComponentLocation());
			return bIsIdle and bM_HasOrder
				and DistanceToPark <= (bIsStoppedAfterRetries ? LooseParkTolerance : ParkTolerance);
		}
	default:
		{
			const float DistanceHome = FVector::Dist2D(TruckLocation, M_SpawnNode->GetComponentLocation());
			return DistanceHome <= (bIsStoppedAfterRetries
				                        ? HomeArrivalTolerance * LooseHomeArrivalFactor
				                        : HomeArrivalTolerance);
		}
	}
}

void UFuelDeliveryDepotComponent::OnGoalReached()
{
	switch (M_State)
	{
	case EFuelDeliveryState::DrivingToApproach:
		SetState(EFuelDeliveryState::DrivingToPark);
		break;
	case EFuelDeliveryState::DrivingToPark:
		StartUnloading();
		break;
	case EFuelDeliveryState::DrivingHome:
	case EFuelDeliveryState::Stopped:
		DespawnTruckAtHome();
		break;
	default:
		break;
	}
}

void UFuelDeliveryDepotComponent::IssueMoveOrder(const FVector& Goal, const FRotator& FinalRotation,
                                                 const bool bForceFinalRotation)
{
	ICommands* const Commands = Cast<ICommands>(M_Truck.Get());
	if (Commands == nullptr)
	{
		return;
	}
	constexpr bool bSetUnitToIdle = true;
	const ECommandQueueError Error = Commands->MoveToLocation(Goal, bSetUnitToIdle, FinalRotation,
	                                                          bForceFinalRotation);
	// A refused order (abilities not set up right after spawn) is simply given again on the next update.
	bM_HasOrder = Error == ECommandQueueError::NoError;
	if (bM_HasOrder)
	{
		++M_OrdersThisLeg;
	}
}

void UFuelDeliveryDepotComponent::UpdateStuckWatch()
{
	using namespace FuelDeliveryDepotPrivate;
	const UWorld* const World = GetWorld();
	if (World == nullptr or not M_Truck.IsValid())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVector TruckLocation = M_Truck->GetActorLocation();
	if (M_StuckWindowStartSeconds < 0.0)
	{
		M_StuckWindowStartSeconds = Now;
		M_StuckWindowStartLocation = TruckLocation;
		M_LastProgressSeconds = Now;
		return;
	}
	if (Now - M_StuckWindowStartSeconds >= StuckWindowSeconds)
	{
		if (FVector::Dist2D(TruckLocation, M_StuckWindowStartLocation) >= StuckProgressCm)
		{
			M_LastProgressSeconds = Now;
		}
		else
		{
			// Wedged while still "moving": the next update gives the order again.
			bM_HasOrder = false;
		}
		M_StuckWindowStartSeconds = Now;
		M_StuckWindowStartLocation = TruckLocation;
	}
	if (Now - M_LastProgressSeconds >= StuckSecondsBeforeRecycle)
	{
		RecycleTruck(TEXT("made no headway"));
	}
}

void UFuelDeliveryDepotComponent::RecycleTruck(const FString& Reason)
{
	RTSFunctionLibrary::ReportError("FuelDeliveryDepot on " + GetNameSafe(GetOwner()) + ": the fuel truck " + Reason
		+ " and was removed; check the nodes and the navmesh between them.");
	const bool bWasStopped = M_State == EFuelDeliveryState::Stopped;
	if (M_Truck.IsValid())
	{
		M_Truck->Destroy();
	}
	M_Truck.Reset();
	if (bWasStopped)
	{
		return;
	}
	ScheduleNextTruck(RefillSeconds);
}

// ---------------------------------------------------------------------------------------------------------------------
// Unloading
// ---------------------------------------------------------------------------------------------------------------------

void UFuelDeliveryDepotComponent::StartUnloading()
{
	using namespace FuelDeliveryDepotPrivate;
	AMissionFuelCrane* const Crane = M_Crane.Get();
	if (not RTSFunctionLibrary::RTSIsValid(Crane))
	{
		// No crane (never spawned): nothing to unload with.
		DriveHome();
		return;
	}
	M_CraneCountAtUnloadStart = Crane->GetUnloadedCount();
	if (Crane->StartUnloadingTruckOnce(M_Truck.Get()))
	{
		SetState(EFuelDeliveryState::Unloading);
		if (const UWorld* const World = GetWorld())
		{
			M_UnloadStartSeconds = World->GetTimeSeconds();
		}
		return;
	}
	// Parked out of the crane's reach: line up again a couple of times, then give up.
	const int32 ParkOrders = M_OrdersThisLeg;
	if (ParkOrders <= MaxParkRetries + OrdersBeforeLooseArrival)
	{
		bM_HasOrder = false;
		return;
	}
	RTSFunctionLibrary::ReportError("FuelDeliveryDepot on " + GetNameSafe(GetOwner())
		+ ": the truck parks out of the crane's reach; move the park node closer to the crane (bed parallel to "
		"the jib's reach, within ~11 m of the mast).");
	DriveHome();
}

void UFuelDeliveryDepotComponent::UpdateUnloading()
{
	const UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	if (not RTSFunctionLibrary::RTSIsValidWeak(M_Crane) or not M_Crane->GetIsUnloading())
	{
		// The crane stopped without reporting an empty truck (crane lost or the truck rolled out of reach).
		DriveHome();
		return;
	}
	if (World->GetTimeSeconds() - M_UnloadStartSeconds > MaxUnloadSeconds)
	{
		RTSFunctionLibrary::ReportError("FuelDeliveryDepot on " + GetNameSafe(GetOwner())
			+ ": unloading took longer than MaxUnloadSeconds; the truck drives home without granting points.");
		M_Crane->StopUnloading();
		DriveHome();
	}
}

void UFuelDeliveryDepotComponent::OnCraneEmptiedTruck(AActor* Truck)
{
	if (M_State != EFuelDeliveryState::Unloading or Truck == nullptr or Truck != M_Truck.Get())
	{
		return;
	}
	const int32 Lifted = M_Crane.IsValid() ? M_Crane->GetUnloadedCount() - M_CraneCountAtUnloadStart : 0;
	if (Lifted > 0)
	{
		GrantTrainingPoints();
	}
	DriveHome();
}

void UFuelDeliveryDepotComponent::GrantTrainingPoints()
{
	++M_CompletedDeliveries;
	int32 Granted = 0;
	if (TrainingPointsPerDelivery > 0)
	{
		const AEnemyController* const EnemyController = FRTS_Statics::GetEnemyControllerOfUnit(GetOwner());
		UEnemyStrategicAIComponent* const StrategicAI = IsValid(EnemyController)
			                                                ? EnemyController->GetEnemyStrategicAIComponent()
			                                                : nullptr;
		if (IsValid(StrategicAI))
		{
			StrategicAI->AddBonusTrainingPoints(TrainingPointsPerDelivery);
			Granted = TrainingPointsPerDelivery;
		}
		else
		{
			UE_LOG(LogRTS, Warning, TEXT("FuelDeliveryDepot on %s: no AI controller for player %d; delivery grants no "
				       "training points."), *GetNameSafe(GetOwner()), FRTSPlayers::GetOwningPlayerOfActor(GetOwner()));
		}
	}
	M_TotalTrainingPointsGranted += Granted;
	UE_LOG(LogRTS, Log, TEXT("FUEL_DELIVERY %s unloaded truck %s: +%d training points (%d deliveries)."),
	       *GetNameSafe(GetOwner()), *GetNameSafe(M_Truck.Get()), Granted, M_CompletedDeliveries);
	OnTruckUnloaded.Broadcast(M_Truck.Get(), Granted);
}

void UFuelDeliveryDepotComponent::HandTruckHomeAfterDepotRemoved()
{
	using namespace FuelDeliveryDepotPrivate;
	UWorld* const World = GetWorld();
	AActor* const Truck = M_Truck.Get();
	if (World == nullptr or not GetIsTruckAlive() or not M_SpawnNode.IsValid())
	{
		return;
	}
	const FVector Home = M_SpawnNode->GetComponentLocation();
	const float ArrivalTolerance = HomeArrivalTolerance;
	const double GiveUpSeconds = World->GetTimeSeconds() + MaxLegSeconds;
	const TWeakObjectPtr<AActor> WeakTruck(Truck);
	// The depot is gone, so the timer is bound to the truck: it ends when the truck is removed or destroyed.
	const TSharedRef<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();
	World->GetTimerManager().SetTimer(*TimerHandle, FTimerDelegate::CreateWeakLambda(
		                                  Truck, [WeakTruck, Home, ArrivalTolerance, GiveUpSeconds, TimerHandle]()
		                                  {
			                                  DriveOrphanedTruckHome(WeakTruck, Home, ArrivalTolerance,
			                                                         GiveUpSeconds, *TimerHandle);
		                                  }), UpdateIntervalSeconds, true);
	M_Truck.Reset();
}

void UFuelDeliveryDepotComponent::DriveOrphanedTruckHome(const TWeakObjectPtr<AActor>& WeakTruck, const FVector& Home,
                                                         const float ArrivalTolerance, const double GiveUpSeconds,
                                                         FTimerHandle& TimerHandle)
{
	AActor* const Truck = WeakTruck.Get();
	UWorld* const World = IsValid(Truck) ? Truck->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return;
	}
	if (not Truck->IsUnitAlive() or World->GetTimeSeconds() > GiveUpSeconds)
	{
		// Dead (its wreck takes over) or stuck for good: leave it as an ordinary unit.
		World->GetTimerManager().ClearTimer(TimerHandle);
		return;
	}
	if (FVector::Dist2D(Truck->GetActorLocation(), Home) <= ArrivalTolerance)
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
		Truck->Destroy();
		return;
	}
	ICommands* const Commands = Cast<ICommands>(Truck);
	if (Commands != nullptr and Commands->GetIsUnitIdle())
	{
		Commands->MoveToLocation(Home, true, Truck->GetActorRotation(), false);
	}
}

void UFuelDeliveryDepotComponent::DriveHome()
{
	SetState(EFuelDeliveryState::DrivingHome);
}

void UFuelDeliveryDepotComponent::DespawnTruckAtHome()
{
	AActor* const Truck = M_Truck.Get();
	const bool bWasStopped = M_State == EFuelDeliveryState::Stopped;
	M_Truck.Reset();
	OnTruckDespawned.Broadcast(Truck);
	if (IsValid(Truck))
	{
		// Removed, not killed: no wreck, no death effects.
		Truck->Destroy();
	}
	if (bWasStopped)
	{
		return;
	}
	ScheduleNextTruck(RefillSeconds);
}

#if RTS_WITH_MAP_TESTS
void UFuelDeliveryDepotComponent::MapTest_OverrideTimings(const float FirstTruckDelay, const float Refill,
                                                          const float RespawnAfterDestroyed)
{
	FirstTruckDelaySeconds = FirstTruckDelay;
	RefillSeconds = Refill;
	RespawnAfterDestroyedSeconds = RespawnAfterDestroyed;
	if (M_State == EFuelDeliveryState::WaitingForTruck)
	{
		ScheduleNextTruck(FirstTruckDelay);
	}
}

const UFuelDeliveryNodeComponent* UFuelDeliveryDepotComponent::MapTest_GetNode(const EFuelDeliveryNodeRole Role) const
{
	switch (Role)
	{
	case EFuelDeliveryNodeRole::TruckPark:
		return M_ParkNode.Get();
	case EFuelDeliveryNodeRole::Crane:
		return M_CraneNode.Get();
	case EFuelDeliveryNodeRole::TruckSpawn:
	default:
		return M_SpawnNode.Get();
	}
}
#endif
