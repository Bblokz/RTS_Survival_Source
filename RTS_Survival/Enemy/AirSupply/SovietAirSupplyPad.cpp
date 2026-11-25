// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SovietAirSupplyPad.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/ArrowComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace SovietAirSupplyPadPrivate
{
	// Mesh socket at the cargo door of an aircraft (same name AAircraftMaster uses for squads walking out).
	const FName CargoDoorSocket = TEXT("CargoDoor");
	constexpr float GroundTraceUp = 400.f;
	constexpr float GroundTraceDown = 1500.f;
	constexpr float ArriveDistance = 8.f;
	constexpr int32 ItemsPerPileRow = 2;
	constexpr int32 MaxItemsPerCargoType = 24;
	// A worker stops this short of an item, on the side it comes from, instead of standing in it.
	constexpr float StopShortOfItemDistance = 60.f;
	// Items land on the pile slightly turned, so the pile does not look machine-placed.
	constexpr float PileYawJitterDegrees = 25.f;
	// The aircraft's own stay-landed timer only catches a stall: it runs this much longer than MaxUnloadSeconds.
	constexpr float StayLandedCapMarginSeconds = 30.f;
	constexpr float MinTimerDelaySeconds = 0.01f;

	void SetupDecorPrimitive(UPrimitiveComponent* Component)
	{
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetGenerateOverlapEvents(false);
	}
}

ASovietAirSupplyPad::ASovietAirSupplyPad()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	Constructor_CreateDecorMeshes();
	Constructor_CreateLayoutPoints();
	Constructor_CreateWorkers();
	Constructor_CreateCargoPiles();
}

void ASovietAirSupplyPad::Constructor_CreateDecorMeshes()
{
	PadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PadMesh"));
	PadMesh->SetupAttachment(SceneRoot);
	RadioMastMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RadioMastMesh"));
	RadioMastMesh->SetupAttachment(SceneRoot);
	FuelStorageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FuelStorageMesh"));
	FuelStorageMesh->SetupAttachment(SceneRoot);
	MunitionStorageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MunitionStorageMesh"));
	MunitionStorageMesh->SetupAttachment(SceneRoot);
}

void ASovietAirSupplyPad::Constructor_CreateLayoutPoints()
{
	const auto CreatePoint = [this](const TCHAR* Name, const FVector& RelativeLocation)
	{
		USceneComponent* Point = CreateDefaultSubobject<USceneComponent>(Name);
		Point->SetupAttachment(SceneRoot);
		Point->SetRelativeLocation(RelativeLocation);
		return Point;
	};
	// Defaults only; the Blueprint places them around its meshes.
	LandingPoint = CreatePoint(TEXT("LandingPoint"), FVector::ZeroVector);
	FuelStoragePoint = CreatePoint(TEXT("FuelStoragePoint"), FVector(-700.f, 1560.f, 0.f));
	MunitionStoragePoint = CreatePoint(TEXT("MunitionStoragePoint"), FVector(700.f, 1560.f, 0.f));
	WorkerPostA = CreatePoint(TEXT("WorkerPostA"), FVector(-180.f, 1380.f, 0.f));
	WorkerPostB = CreatePoint(TEXT("WorkerPostB"), FVector(180.f, 1380.f, 0.f));

#if WITH_EDITORONLY_DATA
	LandingArrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("LandingArrow"));
	if (not LandingArrow)
	{
		return;
	}
	LandingArrow->SetupAttachment(LandingPoint);
	LandingArrow->ArrowColor = FColor::Red;
	LandingArrow->ArrowSize = 8.f;
	LandingArrow->ArrowLength = 300.f;
	LandingArrow->bIsScreenSizeScaled = false;
	LandingArrow->SetHiddenInGame(true);
#endif
}

void ASovietAirSupplyPad::Constructor_CreateWorkers()
{
	using namespace SovietAirSupplyPadPrivate;
	WorkerMeshA = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WorkerMeshA"));
	WorkerMeshA->SetupAttachment(SceneRoot);
	WorkerMeshB = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WorkerMeshB"));
	WorkerMeshB->SetupAttachment(SceneRoot);
	for (USkeletalMeshComponent* Worker : {WorkerMeshA.Get(), WorkerMeshB.Get()})
	{
		SetupDecorPrimitive(Worker);
		Worker->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		// Scripted scenery: the pose only matters where someone looks.
		Worker->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	}

	CarriedCargoA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarriedCargoA"));
	CarriedCargoA->SetupAttachment(WorkerMeshA);
	CarriedCargoB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarriedCargoB"));
	CarriedCargoB->SetupAttachment(WorkerMeshB);
	for (UStaticMeshComponent* Carried : {CarriedCargoA.Get(), CarriedCargoB.Get()})
	{
		SetupDecorPrimitive(Carried);
		Carried->SetVisibility(false);
	}
}

void ASovietAirSupplyPad::Constructor_CreateCargoPiles()
{
	using namespace SovietAirSupplyPadPrivate;
	FuelCargoPile = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("FuelCargoPile"));
	FuelCargoPile->SetupAttachment(SceneRoot);
	MunitionCargoPile = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MunitionCargoPile"));
	MunitionCargoPile->SetupAttachment(SceneRoot);
	for (UInstancedStaticMeshComponent* Pile : {FuelCargoPile.Get(), MunitionCargoPile.Get()})
	{
		SetupDecorPrimitive(Pile);
		// Instances are placed in world space at landing time.
		Pile->SetUsingAbsoluteLocation(true);
		Pile->SetUsingAbsoluteRotation(true);
		Pile->SetUsingAbsoluteScale(true);
	}
}

void ASovietAirSupplyPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Workers stand at their posts in the editor too, so the layout reads before play.
	for (const TPair<USkeletalMeshComponent*, USceneComponent*>& Pair :
	     {TPair<USkeletalMeshComponent*, USceneComponent*>(WorkerMeshA, WorkerPostA),
	      TPair<USkeletalMeshComponent*, USceneComponent*>(WorkerMeshB, WorkerPostB)})
	{
		if (IsValid(Pair.Key) && IsValid(Pair.Value))
		{
			const FRotator PostRotation = Pair.Value->GetComponentRotation();
			Pair.Key->SetWorldLocationAndRotation(
				Pair.Value->GetComponentLocation(),
				FRotator(0.f, PostRotation.Yaw + WorkerVisuals.MeshYawOffset, 0.f));
		}
	}
}

void ASovietAirSupplyPad::BeginPlay()
{
	Super::BeginPlay();
	InitWorkers();
	ClearCargo();
	if (Settings.bEnabled)
	{
		SetDeliveriesEnabled(true);
	}
}

void ASovietAirSupplyPad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLoopTimers();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UnloadItemTimerHandle);
		World->GetTimerManager().ClearTimer(M_TakeOffTimerHandle);
	}
	if (AAircraftMaster* Plane = M_Plane.Get())
	{
		// A landed plane does not wait for workers that are gone.
		Plane->EndAircraftDropStayLanded();
	}
	UnbindPlane();
	if (M_AircraftClassLoadHandle.IsValid())
	{
		M_AircraftClassLoadHandle->CancelHandle();
		M_AircraftClassLoadHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------------------------------------------------------
// Loop
// ------------------------------------------------------------------------------------------------------------------

void ASovietAirSupplyPad::SetDeliveriesEnabled(const bool bNewEnabled)
{
	if (bM_DeliveriesEnabled == bNewEnabled)
	{
		return;
	}
	bM_DeliveriesEnabled = bNewEnabled;
	if (not bNewEnabled)
	{
		// A plane that is already up finishes its delivery; only the countdown stops.
		if (M_State == ESovietAirSupplyState::WaitingForNextPlane)
		{
			ClearLoopTimers();
			SetState(ESovietAirSupplyState::Disabled);
		}
		return;
	}
	if (M_State == ESovietAirSupplyState::Disabled)
	{
		ScheduleNextPlane(Settings.FirstPlaneDelaySeconds);
	}
}

void ASovietAirSupplyPad::SetState(const ESovietAirSupplyState NewState)
{
	M_State = NewState;
	// Only workers and cargo move; nothing to tick while the pad is empty and the workers stand at their posts.
	const bool bNeedsTick = NewState == ESovietAirSupplyState::Unloading || not GetAreAllWorkersAtPost();
	SetActorTickEnabled(bNeedsTick);
}

void ASovietAirSupplyPad::ScheduleNextPlane(const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	if (not bM_DeliveriesEnabled)
	{
		SetState(ESovietAirSupplyState::Disabled);
		return;
	}
	SetState(ESovietAirSupplyState::WaitingForNextPlane);
	World->GetTimerManager().SetTimer(M_NextPlaneTimerHandle, this, &ASovietAirSupplyPad::LoadAircraftClass,
	                                  FMath::Max(SovietAirSupplyPadPrivate::MinTimerDelaySeconds, DelaySeconds), false);
}

void ASovietAirSupplyPad::ClearLoopTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_NextPlaneTimerHandle);
		World->GetTimerManager().ClearTimer(M_SafetyTimerHandle);
	}
}

void ASovietAirSupplyPad::LoadAircraftClass()
{
	if (IsValid(M_LoadedAircraftClass))
	{
		LaunchPlane();
		return;
	}
	if (Settings.AircraftClass.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("ASovietAirSupplyPad has no AircraftClass; no cargo plane will come. Pad: ")
			+ GetName());
		SetState(ESovietAirSupplyState::Disabled);
		return;
	}
	TWeakObjectPtr<ASovietAirSupplyPad> WeakThis(this);
	M_AircraftClassLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Settings.AircraftClass.ToSoftObjectPath(),
		FStreamableDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnAircraftClassLoaded();
			}
		}));
	if (not M_AircraftClassLoadHandle.IsValid())
	{
		// Already loaded: no handle, no callback.
		OnAircraftClassLoaded();
	}
}

void ASovietAirSupplyPad::OnAircraftClassLoaded()
{
	M_AircraftClassLoadHandle.Reset();
	M_LoadedAircraftClass = Settings.AircraftClass.Get();
	if (not IsValid(M_LoadedAircraftClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("ASovietAirSupplyPad could not load its AircraftClass. Pad: ") + GetName());
		SetState(ESovietAirSupplyState::Disabled);
		return;
	}
	LaunchPlane();
}

FVector ASovietAirSupplyPad::GetFlightDirection() const
{
	if (not GetIsValidLandingPoint())
	{
		return FVector::ForwardVector;
	}
	FVector Direction = LandingPoint->GetForwardVector();
	Direction.Z = 0.f;
	return Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
}

FSovietAirSupplyFlightPlan ASovietAirSupplyPad::BuildFlightPlan() const
{
	FSovietAirSupplyFlightPlan Plan;
	Plan.Direction = GetFlightDirection();
	Plan.LandingLocation = GetIsValidLandingPoint() ? LandingPoint->GetComponentLocation() : GetActorLocation();
	Plan.LandingLocation.Z = GetGroundZ(Plan.LandingLocation);
	const float CruiseZ = Plan.LandingLocation.Z + Settings.CruiseHeightAbovePad;
	Plan.SpawnLocation = Plan.LandingLocation - Plan.Direction * Settings.ApproachDistance;
	Plan.SpawnLocation.Z = CruiseZ;
	Plan.RetreatLocation = Plan.LandingLocation + Plan.Direction * Settings.DepartureDistance;
	Plan.RetreatLocation.Z = CruiseZ;
	return Plan;
}

void ASovietAirSupplyPad::LaunchPlane()
{
	if (not bM_DeliveriesEnabled)
	{
		SetState(ESovietAirSupplyState::Disabled);
		return;
	}
	++M_RunId;
	ClearCargo();
	const FSovietAirSupplyFlightPlan Plan = BuildFlightPlan();
	AAircraftMaster* Plane = SpawnPlane(Plan);
	if (not IsValid(Plane))
	{
		ScheduleNextPlane(Settings.SecondsBetweenPlanes);
		return;
	}
	BindPlane(Plane);
	OrderPlaneToPad(*Plane, Plan);
	SetState(ESovietAirSupplyState::PlaneApproaching);
	StartSafetyTimer(Settings.MaxApproachSeconds, &ASovietAirSupplyPad::OnApproachTimedOut);
	UE_LOG(LogRTS, Log, TEXT("AIR_SUPPLY %s: plane %s launched (run %d)."), *GetName(), *Plane->GetName(), M_RunId);
	OnPlaneLaunched.Broadcast(this);
}

AAircraftMaster* ASovietAirSupplyPad::SpawnPlane(const FSovietAirSupplyFlightPlan& Plan) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(M_LoadedAircraftClass))
	{
		return nullptr;
	}
	// Owner set before BeginPlay, so the plane registers, collides and is targeted as the AI player's aircraft.
	AActor* SpawnedActor = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, M_LoadedAircraftClass, FTransform(Plan.Direction.Rotation(), Plan.SpawnLocation),
		static_cast<uint8>(Settings.EnemyPlayer));
	AAircraftMaster* Plane = Cast<AAircraftMaster>(SpawnedActor);
	if (not IsValid(Plane))
	{
		if (IsValid(SpawnedActor))
		{
			SpawnedActor->Destroy();
		}
		RTSFunctionLibrary::ReportError(TEXT("ASovietAirSupplyPad failed to spawn its cargo plane. Pad: ") + GetName());
		return nullptr;
	}
	Plane->SpawnDefaultController();
	Plane->SetUnitSelectable(false);
	return Plane;
}

void ASovietAirSupplyPad::OrderPlaneToPad(AAircraftMaster& Plane, const FSovietAirSupplyFlightPlan& Plan) const
{
	using namespace SovietAirSupplyPadPrivate;
	FAircraftDropRequest Request;
	Request.PayloadType = EAircraftDropPayloadType::None;
	Request.OwningPlayer = Settings.EnemyPlayer;
	Request.ExecuteLocation = Plan.LandingLocation;
	Request.RetreatLocation = Plan.RetreatLocation;
	// The pad releases the plane once the cargo is stored; this only caps a stall.
	Request.HowLongStayLanded = Settings.MaxUnloadSeconds + StayLandedCapMarginSeconds;
	Request.bHandPayloadToEnemyAI = false;
	Plane.OrderAircraftDrop(Request);
}

void ASovietAirSupplyPad::StartSafetyTimer(const float Seconds, void (ASovietAirSupplyPad::*OnTimedOut)(int32))
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const int32 RunId = M_RunId;
	TWeakObjectPtr<ASovietAirSupplyPad> WeakThis(this);
	World->GetTimerManager().SetTimer(M_SafetyTimerHandle, FTimerDelegate::CreateLambda([WeakThis, RunId, OnTimedOut]()
	{
		if (WeakThis.IsValid())
		{
			(WeakThis.Get()->*OnTimedOut)(RunId);
		}
	}), Seconds, false);
}

void ASovietAirSupplyPad::BindPlane(AAircraftMaster* Plane)
{
	UnbindPlane();
	M_Plane = Plane;
	const int32 RunId = M_RunId;
	TWeakObjectPtr<ASovietAirSupplyPad> WeakThis(this);
	M_PlaneLandedHandle = Plane->OnAircraftDropLanded.AddLambda([WeakThis, RunId](AAircraftMaster* LandedPlane)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnPlaneLanded(LandedPlane, RunId);
		}
	});
	M_PlaneDiesHandle = Plane->OnUnitDies.AddLambda([WeakThis, RunId]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnPlaneDies(RunId);
		}
	});
	Plane->OnDestroyed.AddUniqueDynamic(this, &ASovietAirSupplyPad::OnPlaneDestroyed);
}

void ASovietAirSupplyPad::UnbindPlane()
{
	AAircraftMaster* Plane = M_Plane.Get();
	if (IsValid(Plane))
	{
		Plane->OnAircraftDropLanded.Remove(M_PlaneLandedHandle);
		Plane->OnUnitDies.Remove(M_PlaneDiesHandle);
		Plane->OnDestroyed.RemoveDynamic(this, &ASovietAirSupplyPad::OnPlaneDestroyed);
	}
	M_PlaneLandedHandle.Reset();
	M_PlaneDiesHandle.Reset();
	M_Plane.Reset();
}

void ASovietAirSupplyPad::OnPlaneLanded(AAircraftMaster* Plane, const int32 RunId)
{
	if (RunId != M_RunId || M_State != ESovietAirSupplyState::PlaneApproaching || not IsValid(Plane))
	{
		return;
	}
	StartSafetyTimer(Settings.MaxUnloadSeconds, &ASovietAirSupplyPad::OnUnloadTimedOut);
	BuildCargoPile(*Plane);
	SetState(ESovietAirSupplyState::Unloading);
	UE_LOG(LogRTS, Log, TEXT("AIR_SUPPLY %s: plane landed, %d items to store."), *GetName(), M_CargoItems.Num());
	if (M_CargoItems.IsEmpty())
	{
		CompleteDelivery();
		return;
	}
	UnloadNextItem();
	for (FWorker& Worker : M_Workers)
	{
		SendWorkerToNextItem(Worker);
	}
}

void ASovietAirSupplyPad::OnPlaneDies(const int32 RunId)
{
	if (RunId != M_RunId)
	{
		return;
	}
	if (M_State == ESovietAirSupplyState::PlaneApproaching || M_State == ESovietAirSupplyState::Unloading)
	{
		LoseDelivery(TEXT("plane shot down"));
		return;
	}
	if (M_State == ESovietAirSupplyState::PlaneDeparting)
	{
		// Delivered already: the points stay. The wreck may fall for a while; the countdown starts now.
		UnbindPlane();
		ScheduleNextPlane(Settings.SecondsBetweenPlanes);
	}
}

void ASovietAirSupplyPad::OnPlaneDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor != M_Plane.Get())
	{
		return;
	}
	if (M_State == ESovietAirSupplyState::PlaneApproaching || M_State == ESovietAirSupplyState::Unloading)
	{
		// Removed without dying (another system destroyed it): the cargo never arrived.
		LoseDelivery(TEXT("plane removed"));
		return;
	}
	UnbindPlane();
	if (M_State == ESovietAirSupplyState::PlaneDeparting)
	{
		// Reached the end of its departure and left the map.
		ScheduleNextPlane(Settings.SecondsBetweenPlanes);
	}
}

void ASovietAirSupplyPad::OnApproachTimedOut(const int32 RunId)
{
	if (RunId != M_RunId || M_State != ESovietAirSupplyState::PlaneApproaching)
	{
		return;
	}
	UE_LOG(LogRTS, Warning, TEXT("AIR_SUPPLY %s: plane did not land within %.0f s; removed."), *GetName(),
	       Settings.MaxApproachSeconds);
	AAircraftMaster* Plane = M_Plane.Get();
	LoseDelivery(TEXT("approach timeout"));
	if (IsValid(Plane))
	{
		Plane->Destroy();
	}
}

void ASovietAirSupplyPad::OnUnloadTimedOut(const int32 RunId)
{
	if (RunId != M_RunId || M_State != ESovietAirSupplyState::Unloading)
	{
		return;
	}
	UE_LOG(LogRTS, Warning, TEXT("AIR_SUPPLY %s: cargo not stored within %.0f s; the plane leaves without points."),
	       *GetName(), Settings.MaxUnloadSeconds);
	AAircraftMaster* Plane = M_Plane.Get();
	LoseDelivery(TEXT("unload timeout"));
	if (IsValid(Plane))
	{
		Plane->EndAircraftDropStayLanded();
	}
}

void ASovietAirSupplyPad::LoseDelivery(const TCHAR* Reason)
{
	++M_LostDeliveries;
	UE_LOG(LogRTS, Log, TEXT("AIR_SUPPLY %s: delivery lost (%s), no points (%d lost)."), *GetName(), Reason,
	       M_LostDeliveries);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SafetyTimerHandle);
		World->GetTimerManager().ClearTimer(M_TakeOffTimerHandle);
	}
	UnbindPlane();
	ClearCargo();
	SendAllWorkersHome();
	OnDeliveryLost.Broadcast(this);
	ScheduleNextPlane(Settings.SecondsBetweenPlanes);
}

void ASovietAirSupplyPad::CompleteDelivery()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().ClearTimer(M_SafetyTimerHandle);
	++M_CompletedDeliveries;
	int32 Training = 0;
	int32 Building = 0;
	GrantPoints(Training, Building);
	M_TotalTrainingPointsGranted += Training;
	M_TotalBuildingPointsGranted += Building;
	UE_LOG(LogRTS, Log, TEXT("AIR_SUPPLY %s: delivery %d stored: +%d training, +%d building points."), *GetName(),
	       M_CompletedDeliveries, Training, Building);
	ClearCargo();
	SendAllWorkersHome();
	SetState(ESovietAirSupplyState::PlaneDeparting);
	OnDeliveryCompleted.Broadcast(this, Training, Building);
	World->GetTimerManager().SetTimer(M_TakeOffTimerHandle, this, &ASovietAirSupplyPad::ReleasePlaneForTakeOff,
	                                  FMath::Max(SovietAirSupplyPadPrivate::MinTimerDelaySeconds, Settings.TakeOffDelaySeconds), false);
}

void ASovietAirSupplyPad::ReleasePlaneForTakeOff()
{
	if (AAircraftMaster* Plane = M_Plane.Get())
	{
		Plane->EndAircraftDropStayLanded();
	}
}

void ASovietAirSupplyPad::GrantPoints(int32& OutTraining, int32& OutBuilding) const
{
	OutTraining = 0;
	OutBuilding = 0;
	AEnemyController* EnemyController = Settings.EnemyPlayer == RTSPlayers::FirstAIPlayer
		                                    ? FRTS_Statics::GetEnemyController(this)
		                                    : FRTS_Statics::GetEnemyControllerForPlayer(this, Settings.EnemyPlayer);
	UEnemyStrategicAIComponent* StrategicAI = IsValid(EnemyController)
		                                          ? EnemyController->GetEnemyStrategicAIComponent()
		                                          : nullptr;
	if (not IsValid(StrategicAI))
	{
		UE_LOG(LogRTS, Warning, TEXT("AIR_SUPPLY %s: no enemy controller for player %d; the delivery gives no points."),
		       *GetName(), Settings.EnemyPlayer);
		return;
	}
	StrategicAI->AddBonusTrainingPoints(Settings.TrainingPointsPerDelivery);
	StrategicAI->AddBuildingPoints(Settings.BuildingPointsPerDelivery);
	OutTraining = FMath::Max(0, Settings.TrainingPointsPerDelivery);
	OutBuilding = FMath::Max(0, Settings.BuildingPointsPerDelivery);
}

// ------------------------------------------------------------------------------------------------------------------
// Cargo
// ------------------------------------------------------------------------------------------------------------------

void ASovietAirSupplyPad::BuildCargoPile(const AAircraftMaster& Plane)
{
	using namespace SovietAirSupplyPadPrivate;
	ClearCargo();
	FRotator PlaneYaw = Plane.GetActorRotation();
	PlaneYaw.Pitch = 0.f;
	PlaneYaw.Roll = 0.f;
	FVector Door = Plane.GetActorLocation();
	const USkeletalMeshComponent* PlaneMesh = Plane.FindComponentByClass<USkeletalMeshComponent>();
	if (IsValid(PlaneMesh) && PlaneMesh->DoesSocketExist(CargoDoorSocket))
	{
		Door = PlaneMesh->GetSocketLocation(CargoDoorSocket);
	}
	const FVector PileOrigin = Door + PlaneYaw.RotateVector(WorkerVisuals.CargoPileOffset);
	const FVector RowDirection = PlaneYaw.RotateVector(FVector(-1.f, 0.f, 0.f));
	const FVector ColumnDirection = PlaneYaw.RotateVector(FVector(0.f, 1.f, 0.f));

	const int32 FuelCount = FMath::Clamp(Settings.FuelBarrelsPerPlane, 0, MaxItemsPerCargoType);
	const int32 MunitionCount = FMath::Clamp(Settings.MunitionCratesPerPlane, 0, MaxItemsPerCargoType);
	const int32 Total = FuelCount + MunitionCount;
	int32 FuelPlaced = 0;
	int32 MunitionPlaced = 0;
	for (int32 Index = 0; Index < Total; ++Index)
	{
		// Alternate the types while both last, so both storages get traffic from the start.
		const bool bWantFuel = Index % 2 == 0;
		const bool bIsFuel = MunitionPlaced >= MunitionCount || (bWantFuel && FuelPlaced < FuelCount);
		FuelPlaced += bIsFuel ? 1 : 0;
		MunitionPlaced += bIsFuel ? 0 : 1;
		FCargoItem& Item = M_CargoItems.AddDefaulted_GetRef();
		Item.Type = bIsFuel ? ESovietAirSupplyCargo::Fuel : ESovietAirSupplyCargo::Munition;
		const int32 Row = Index / ItemsPerPileRow;
		const int32 Column = Index % ItemsPerPileRow;
		FVector Location = PileOrigin + RowDirection * (Row * WorkerVisuals.CargoPileSpacing)
			+ ColumnDirection * (Column * WorkerVisuals.CargoPileSpacing);
		Location.Z = GetGroundZ(Location);
		Item.Location = Location;
	}
}

void ASovietAirSupplyPad::UnloadNextItem()
{
	if (M_State != ESovietAirSupplyState::Unloading || M_AppearedItemCount >= M_CargoItems.Num())
	{
		return;
	}
	FCargoItem& Item = M_CargoItems[M_AppearedItemCount];
	UInstancedStaticMeshComponent* Pile = GetPile(Item.Type);
	if (IsValid(Pile))
	{
		const FRotator RandomYaw(0.f, FMath::FRandRange(-SovietAirSupplyPadPrivate::PileYawJitterDegrees, SovietAirSupplyPadPrivate::PileYawJitterDegrees) + GetActorRotation().Yaw, 0.f);
		Item.InstanceIndex = Pile->AddInstance(FTransform(RandomYaw, Item.Location), /*bWorldSpace*/ true);
	}
	Item.bAppeared = true;
	++M_AppearedItemCount;

	// Waiting workers grab the item that just came out.
	for (FWorker& Worker : M_Workers)
	{
		if (Worker.State == EWorkerState::WaitingForCargo)
		{
			SendWorkerToNextItem(Worker);
		}
	}
	if (M_AppearedItemCount < M_CargoItems.Num())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(M_UnloadItemTimerHandle, this, &ASovietAirSupplyPad::UnloadNextItem,
			                                  FMath::Max(SovietAirSupplyPadPrivate::MinTimerDelaySeconds, WorkerVisuals.SecondsBetweenUnloadedItems), false);
		}
	}
}

void ASovietAirSupplyPad::ClearCargo()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_UnloadItemTimerHandle);
	}
	M_CargoItems.Reset();
	M_AppearedItemCount = 0;
	M_StoredItemCount = 0;
	for (const ESovietAirSupplyCargo Type : {ESovietAirSupplyCargo::Fuel, ESovietAirSupplyCargo::Munition})
	{
		if (UInstancedStaticMeshComponent* Pile = GetPile(Type))
		{
			Pile->ClearInstances();
		}
	}
}

UInstancedStaticMeshComponent* ASovietAirSupplyPad::GetPile(const ESovietAirSupplyCargo Type) const
{
	if (not GetIsValidPile(Type))
	{
		return nullptr;
	}
	return Type == ESovietAirSupplyCargo::Fuel ? FuelCargoPile.Get() : MunitionCargoPile.Get();
}

FVector ASovietAirSupplyPad::GetStorageLocation(const ESovietAirSupplyCargo Type) const
{
	if (not GetIsValidStoragePoint(Type))
	{
		return GetActorLocation();
	}
	const USceneComponent* Point = Type == ESovietAirSupplyCargo::Fuel ? FuelStoragePoint.Get()
		                               : MunitionStoragePoint.Get();
	return Point->GetComponentLocation();
}

bool ASovietAirSupplyPad::GetIsValidLandingPoint() const
{
	if (IsValid(LandingPoint))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "LandingPoint", "GetIsValidLandingPoint", this);
	return false;
}

bool ASovietAirSupplyPad::GetIsValidStoragePoint(const ESovietAirSupplyCargo Type) const
{
	const bool bIsFuel = Type == ESovietAirSupplyCargo::Fuel;
	if (IsValid(bIsFuel ? FuelStoragePoint.Get() : MunitionStoragePoint.Get()))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, bIsFuel ? "FuelStoragePoint" : "MunitionStoragePoint", "GetIsValidStoragePoint", this);
	return false;
}

bool ASovietAirSupplyPad::GetIsValidPile(const ESovietAirSupplyCargo Type) const
{
	const bool bIsFuel = Type == ESovietAirSupplyCargo::Fuel;
	if (IsValid(bIsFuel ? FuelCargoPile.Get() : MunitionCargoPile.Get()))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, bIsFuel ? "FuelCargoPile" : "MunitionCargoPile", "GetIsValidPile", this);
	return false;
}

int32 ASovietAirSupplyPad::ClaimNextItem()
{
	for (int32 Index = 0; Index < M_AppearedItemCount; ++Index)
	{
		if (not M_CargoItems[Index].bClaimed)
		{
			M_CargoItems[Index].bClaimed = true;
			return Index;
		}
	}
	return INDEX_NONE;
}

// ------------------------------------------------------------------------------------------------------------------
// Workers
// ------------------------------------------------------------------------------------------------------------------

void ASovietAirSupplyPad::InitWorkers()
{
	M_Workers.Reset();
	const TArray<TTuple<USkeletalMeshComponent*, UStaticMeshComponent*, USceneComponent*>> Setups = {
		MakeTuple(WorkerMeshA.Get(), CarriedCargoA.Get(), WorkerPostA.Get()),
		MakeTuple(WorkerMeshB.Get(), CarriedCargoB.Get(), WorkerPostB.Get())
	};
	float LaneSign = 1.f;
	for (const auto& Setup : Setups)
	{
		USkeletalMeshComponent* Mesh = Setup.Get<0>();
		if (not IsValid(Mesh) || not IsValid(Mesh->GetSkeletalMeshAsset()))
		{
			continue;
		}
		FWorker& Worker = M_Workers.AddDefaulted_GetRef();
		Worker.Mesh = Mesh;
		Worker.Carried = Setup.Get<1>();
		Worker.Post = Setup.Get<2>();
		Worker.LaneSign = LaneSign;
		LaneSign = -LaneSign;
		// Moved by hand in world space every frame.
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);
		if (UStaticMeshComponent* Carried = Worker.Carried.Get())
		{
			Carried->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform,
			                           WorkerVisuals.CarrySocketName);
			Carried->SetVisibility(false);
		}
		PlaceWorkerAtPost(Worker);
		SetWorkerState(Worker, EWorkerState::AtPost);
	}
}

void ASovietAirSupplyPad::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (FWorker& Worker : M_Workers)
	{
		TickWorker(Worker, DeltaSeconds);
	}
	if (M_State != ESovietAirSupplyState::Unloading && GetAreAllWorkersAtPost())
	{
		SetActorTickEnabled(false);
	}
}

void ASovietAirSupplyPad::TickWorker(FWorker& Worker, const float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const float Now = IsValid(World) ? World->GetTimeSeconds() : 0.f;
	switch (Worker.State)
	{
	case EWorkerState::WalkingToCargo:
		if (MoveWorker(Worker, WorkerVisuals.WalkSpeed, DeltaSeconds))
		{
			SetWorkerState(Worker, EWorkerState::PickingUp);
		}
		break;
	case EWorkerState::PickingUp:
		TickWorkerPickingUp(Worker, Now);
		break;
	case EWorkerState::CarryingToStorage:
		if (MoveWorker(Worker, WorkerVisuals.CarrySpeed, DeltaSeconds))
		{
			SetWorkerState(Worker, EWorkerState::PuttingDown);
		}
		break;
	case EWorkerState::PuttingDown:
		TickWorkerPuttingDown(Worker, Now);
		break;
	case EWorkerState::WalkingToPost:
		if (MoveWorker(Worker, WorkerVisuals.WalkSpeed, DeltaSeconds))
		{
			PlaceWorkerAtPost(Worker);
			SetWorkerState(Worker, EWorkerState::AtPost);
		}
		break;
	default:
		break;
	}
}

void ASovietAirSupplyPad::TickWorkerPickingUp(FWorker& Worker, const float NowSeconds)
{
	if (not M_CargoItems.IsValidIndex(Worker.ItemIndex))
	{
		SendWorkerToNextItem(Worker);
		return;
	}
	if (not Worker.bHandedOver && NowSeconds >= GetHandOverSeconds(Worker))
	{
		Worker.bHandedOver = true;
		AttachCarriedItem(Worker);
	}
	if (NowSeconds < Worker.StateEndSeconds)
	{
		return;
	}
	const FVector Storage = GetStorageLocation(M_CargoItems[Worker.ItemIndex].Type);
	Worker.Target = GetLaneTarget(Worker, Worker.Mesh->GetComponentLocation(), Storage);
	SetWorkerState(Worker, EWorkerState::CarryingToStorage);
}

void ASovietAirSupplyPad::TickWorkerPuttingDown(FWorker& Worker, const float NowSeconds)
{
	if (not Worker.bHandedOver && NowSeconds >= GetHandOverSeconds(Worker))
	{
		Worker.bHandedOver = true;
		DropCarriedItem(Worker);
	}
	if (NowSeconds < Worker.StateEndSeconds)
	{
		return;
	}
	DropCarriedItem(Worker);
	Worker.ItemIndex = INDEX_NONE;
	++M_StoredItemCount;
	if (M_State == ESovietAirSupplyState::Unloading && M_StoredItemCount >= M_CargoItems.Num())
	{
		CompleteDelivery();
		return;
	}
	SendWorkerToNextItem(Worker);
}

float ASovietAirSupplyPad::GetHandOverSeconds(const FWorker& Worker) const
{
	return Worker.StateStartSeconds + WorkerVisuals.HandleCargoSeconds * WorkerVisuals.HandOverFraction;
}

bool ASovietAirSupplyPad::MoveWorker(FWorker& Worker, const float Speed, const float DeltaSeconds) const
{
	USkeletalMeshComponent* Mesh = Worker.Mesh.Get();
	if (not IsValid(Mesh))
	{
		return true;
	}
	const FVector Location = Mesh->GetComponentLocation();
	FVector ToTarget = Worker.Target - Location;
	ToTarget.Z = 0.f;
	const float Distance = ToTarget.Size();
	const float Step = Speed * DeltaSeconds;
	const bool bArrived = Distance <= FMath::Max(Step, SovietAirSupplyPadPrivate::ArriveDistance);
	FVector NewLocation = bArrived ? Worker.Target : Location + ToTarget / Distance * Step;
	NewLocation.Z = GetGroundZ(NewLocation);
	const float Yaw = Distance > KINDA_SMALL_NUMBER ? ToTarget.Rotation().Yaw : Mesh->GetComponentRotation().Yaw
		- WorkerVisuals.MeshYawOffset;
	Mesh->SetWorldLocationAndRotation(NewLocation, FRotator(0.f, Yaw + WorkerVisuals.MeshYawOffset, 0.f));
	return bArrived;
}

void ASovietAirSupplyPad::SetWorkerState(FWorker& Worker, const EWorkerState NewState)
{
	Worker.State = NewState;
	const UWorld* World = GetWorld();
	const float Now = IsValid(World) ? World->GetTimeSeconds() : 0.f;
	Worker.StateStartSeconds = Now;
	Worker.StateEndSeconds = Now + WorkerVisuals.HandleCargoSeconds;
	Worker.bHandedOver = false;
	UAnimSequenceBase* Walk = WorkerVisuals.WalkAnim;
	UAnimSequenceBase* Idle = WorkerVisuals.IdleAnim;
	UAnimSequenceBase* Carry = WorkerVisuals.CarryAnim ? WorkerVisuals.CarryAnim.Get() : Walk;
	switch (NewState)
	{
	case EWorkerState::AtPost:
	case EWorkerState::WaitingForCargo:
		PlayWorkerAnim(Worker, Idle, /*bLooping*/ true);
		break;
	case EWorkerState::WalkingToCargo:
	case EWorkerState::WalkingToPost:
		PlayWorkerAnim(Worker, Walk, /*bLooping*/ true);
		break;
	case EWorkerState::CarryingToStorage:
		PlayWorkerAnim(Worker, Carry, /*bLooping*/ true);
		break;
	case EWorkerState::PickingUp:
		PlayWorkerAnim(Worker, WorkerVisuals.PickUpAnim ? WorkerVisuals.PickUpAnim.Get() : Idle,
		               WorkerVisuals.PickUpAnim == nullptr);
		break;
	case EWorkerState::PuttingDown:
		PlayWorkerAnim(Worker, WorkerVisuals.PutDownAnim ? WorkerVisuals.PutDownAnim.Get() : Idle,
		               WorkerVisuals.PutDownAnim == nullptr);
		break;
	}
	if (NewState != EWorkerState::AtPost)
	{
		SetActorTickEnabled(true);
	}
}

void ASovietAirSupplyPad::SendWorkerToNextItem(FWorker& Worker)
{
	if (M_State != ESovietAirSupplyState::Unloading)
	{
		return;
	}
	const int32 ItemIndex = ClaimNextItem();
	if (ItemIndex == INDEX_NONE)
	{
		// Everything out so far is taken: wait (or go home when nothing more comes).
		const bool bMoreComing = M_AppearedItemCount < M_CargoItems.Num();
		if (bMoreComing)
		{
			SetWorkerState(Worker, EWorkerState::WaitingForCargo);
		}
		else
		{
			Worker.Target = Worker.Post.IsValid() ? Worker.Post->GetComponentLocation()
				                : Worker.Mesh->GetComponentLocation();
			SetWorkerState(Worker, EWorkerState::WalkingToPost);
		}
		return;
	}
	Worker.ItemIndex = ItemIndex;
	const FVector ItemLocation = M_CargoItems[ItemIndex].Location;
	FVector ToItem = ItemLocation - Worker.Mesh->GetComponentLocation();
	ToItem.Z = 0.f;
	// Stop just short of the item, on the side the worker comes from.
	Worker.Target = ItemLocation - ToItem.GetSafeNormal() * SovietAirSupplyPadPrivate::StopShortOfItemDistance;
	SetWorkerState(Worker, EWorkerState::WalkingToCargo);
}

void ASovietAirSupplyPad::PlayWorkerAnim(FWorker& Worker, UAnimSequenceBase* Anim, const bool bLooping) const
{
	USkeletalMeshComponent* Mesh = Worker.Mesh.Get();
	if (not IsValid(Mesh) || not IsValid(Anim))
	{
		return;
	}
	// A loop already playing keeps its phase; a one-shot always restarts.
	if (bLooping && Worker.PlayingAnim.Get() == Anim)
	{
		return;
	}
	Worker.PlayingAnim = bLooping ? Anim : nullptr;
	Mesh->PlayAnimation(Anim, bLooping);
	// One-shots (pick up, put down) are fitted to the handling time.
	const float PlayLength = Anim->GetPlayLength();
	const bool bFitToHandleTime = not bLooping && PlayLength > KINDA_SMALL_NUMBER
		&& WorkerVisuals.HandleCargoSeconds > KINDA_SMALL_NUMBER;
	Mesh->SetPlayRate(bFitToHandleTime ? PlayLength / WorkerVisuals.HandleCargoSeconds : 1.f);
}

void ASovietAirSupplyPad::PlaceWorkerAtPost(FWorker& Worker) const
{
	USkeletalMeshComponent* Mesh = Worker.Mesh.Get();
	const USceneComponent* Post = Worker.Post.Get();
	if (not IsValid(Mesh) || not IsValid(Post))
	{
		return;
	}
	FVector Location = Post->GetComponentLocation();
	Location.Z = GetGroundZ(Location);
	Mesh->SetWorldLocationAndRotation(
		Location, FRotator(0.f, Post->GetComponentRotation().Yaw + WorkerVisuals.MeshYawOffset, 0.f));
}

void ASovietAirSupplyPad::AttachCarriedItem(FWorker& Worker) const
{
	if (Worker.ItemIndex == INDEX_NONE || not M_CargoItems.IsValidIndex(Worker.ItemIndex))
	{
		return;
	}
	const FCargoItem& Item = M_CargoItems[Worker.ItemIndex];
	UInstancedStaticMeshComponent* Pile = GetPile(Item.Type);
	if (IsValid(Pile) && Item.InstanceIndex != INDEX_NONE)
	{
		// Shrunk to nothing instead of removed: removing would renumber the other items' instances.
		FTransform Hidden(FRotator::ZeroRotator, Item.Location, FVector(KINDA_SMALL_NUMBER));
		Pile->UpdateInstanceTransform(Item.InstanceIndex, Hidden, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ true);
	}
	UStaticMeshComponent* Carried = Worker.Carried.Get();
	if (not IsValid(Carried))
	{
		return;
	}
	Carried->SetStaticMesh(IsValid(Pile) ? Pile->GetStaticMesh() : nullptr);
	Carried->SetRelativeTransform(Item.Type == ESovietAirSupplyCargo::Fuel
		                              ? WorkerVisuals.FuelCarryTransform
		                              : WorkerVisuals.MunitionCarryTransform);
	Carried->SetVisibility(true);
}

void ASovietAirSupplyPad::DropCarriedItem(FWorker& Worker) const
{
	if (UStaticMeshComponent* Carried = Worker.Carried.Get())
	{
		Carried->SetVisibility(false);
	}
}

void ASovietAirSupplyPad::SendAllWorkersHome()
{
	for (FWorker& Worker : M_Workers)
	{
		DropCarriedItem(Worker);
		Worker.ItemIndex = INDEX_NONE;
		if (Worker.State == EWorkerState::AtPost || not Worker.Mesh.IsValid())
		{
			continue;
		}
		Worker.Target = Worker.Post.IsValid() ? Worker.Post->GetComponentLocation()
			                : Worker.Mesh->GetComponentLocation();
		SetWorkerState(Worker, EWorkerState::WalkingToPost);
	}
}

FVector ASovietAirSupplyPad::GetLaneTarget(const FWorker& Worker, const FVector& From, const FVector& To) const
{
	FVector Direction = To - From;
	Direction.Z = 0.f;
	const FVector Side = FVector::CrossProduct(Direction.GetSafeNormal(), FVector::UpVector);
	return To + Side * (WorkerVisuals.LaneOffset * Worker.LaneSign);
}

float ASovietAirSupplyPad::GetGroundZ(const FVector& Location) const
{
	using namespace SovietAirSupplyPadPrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return Location.Z;
	}
	// World static only: the pad and the landscape, not the landed plane or units walking past.
	FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SovietAirSupplyGround), /*bTraceComplex*/ false);
	FHitResult Hit;
	const FVector Start(Location.X, Location.Y, Location.Z + GroundTraceUp);
	const FVector End(Location.X, Location.Y, Location.Z - GroundTraceDown);
	if (World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams))
	{
		return Hit.ImpactPoint.Z;
	}
	return Location.Z;
}

bool ASovietAirSupplyPad::GetAreAllWorkersAtPost() const
{
	for (const FWorker& Worker : M_Workers)
	{
		if (Worker.State != EWorkerState::AtPost)
		{
			return false;
		}
	}
	return true;
}

#if RTS_WITH_MAP_TESTS
FVector ASovietAirSupplyPad::MapTest_GetLandingLocation() const
{
	return GetIsValidLandingPoint() ? LandingPoint->GetComponentLocation() : GetActorLocation();
}

int32 ASovietAirSupplyPad::MapTest_GetItemsOnPile() const
{
	int32 Count = 0;
	for (const FCargoItem& Item : M_CargoItems)
	{
		Count += Item.bAppeared && not Item.bClaimed ? 1 : 0;
	}
	return Count;
}

bool ASovietAirSupplyPad::MapTest_GetIsAnyWorkerCarrying() const
{
	for (const FWorker& Worker : M_Workers)
	{
		if (Worker.State == EWorkerState::CarryingToStorage)
		{
			return true;
		}
	}
	return false;
}

FVector ASovietAirSupplyPad::MapTest_GetWorkerLocation(const int32 WorkerIndex) const
{
	return M_Workers.IsValidIndex(WorkerIndex) && M_Workers[WorkerIndex].Mesh.IsValid()
		       ? M_Workers[WorkerIndex].Mesh->GetComponentLocation()
		       : FVector::ZeroVector;
}
#endif
