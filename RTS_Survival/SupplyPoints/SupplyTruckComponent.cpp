// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SupplyTruckComponent.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"
#include "RTS_Survival/SupplyPoints/SupplyPointSubsystem.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SupplyTruckPrivate
{
	constexpr float SupplyRunUpdateInterval = 0.5f;
	// Waypoints the truck only passes (approach, exit) count as reached this far out, so it does not stop on them.
	constexpr float PassWaypointTolerance = 400.f;
	// Vehicles stop up to about 2 m short of a move goal.
	constexpr float ParkTolerance = 250.f;
	constexpr float BaseArrivalTolerance = 500.f;
	// After this many orders for one leg a truck that stopped near its goal is accepted as arrived.
	constexpr int32 OrdersBeforeLooseArrival = 3;
	constexpr float LooseParkTolerance = 700.f;
	constexpr float LooseBaseArrivalTolerance = 1800.f;
	// The base moved (nomadic HQ) further than this: drive to the new place.
	constexpr float GoalMovedReorderDistance = 300.f;
	// A driving truck must cover this much ground per window, or it counts as stuck.
	constexpr double StuckWindowSeconds = 8.0;
	constexpr float StuckProgressCm = 150.f;
	// Stuck windows in a row before the run is given up and the truck removed (about a minute and a half).
	constexpr int32 StuckWindowsBeforeAbandon = 11;
}

USupplyTruckComponent::USupplyTruckComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USupplyTruckComponent::GetIsValidTruck() const
{
	if (M_Truck.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Truck", "GetIsValidTruck", GetOwner());
	return false;
}

void USupplyTruckComponent::BeginPlay()
{
	Super::BeginPlay();
	// The cargo struct defaults to a full load for the supply point settings; a truck starts empty.
	M_Cargo.Reset();
	M_Truck = Cast<ATankMaster>(GetOwner());
	if (not GetIsValidTruck())
	{
		return;
	}
	BeginPlay_MakeUncontrollable();
	BeginPlay_BindTruckDeath();
	BeginPlay_AddMiniMapIcon();
	BeginPlay_StartSupplyRunUpdates();
}

void USupplyTruckComponent::BeginPlay_MakeUncontrollable()
{
	// Marquee, click and control-group selection all respect this flag.
	M_Truck->SetUnitSelectable(false);
}

void USupplyTruckComponent::BeginPlay_BindTruckDeath()
{
	M_TruckDeathHandle = M_Truck->OnUnitDies.AddUObject(this, &USupplyTruckComponent::OnTruckDied);
}

void USupplyTruckComponent::BeginPlay_AddMiniMapIcon()
{
	// Custom icons ignore the fog of war, so only the player's own trucks get one.
	if (GetTruckOwningPlayer() != SupplyPointConstants::PlayerOwner)
	{
		return;
	}
	AFowManager* FowManager = FRTS_Statics::GetFowManager(GetOwner());
	if (not IsValid(FowManager))
	{
		return;
	}
	const FName IconId(*FString::Printf(TEXT("SupplyTruck_%s"), *GetOwner()->GetName()));
	// The icon is an upright side view of the truck, so it does not turn with the truck.
	constexpr bool bUseStaticRotation = true;
	M_MiniMapIconId = FowManager->AddCustomMiniMapIconAttachedToActor(
		IconId, EMinimapIconType::SupplyTruck, M_Truck->GetActorLocation(), M_Truck.Get(), FRotator::ZeroRotator,
		bUseStaticRotation, FMinimapIconTextPayload());
}

void USupplyTruckComponent::BeginPlay_StartSupplyRunUpdates()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<USupplyTruckComponent> WeakThis(this);
	World->GetTimerManager().SetTimer(M_SupplyRunTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->UpdateSupplyRun();
		}
	}), SupplyTruckPrivate::SupplyRunUpdateInterval, true);
}

void USupplyTruckComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SupplyRunTimerHandle);
	}
	RemoveMiniMapIcon();
	if (not bM_HasRunEnded)
	{
		// Removed without dying or delivering (level change, script): the run still has to end for its supply point.
		bM_HasRunEnded = true;
		OnSupplyTruckRunEnded.Broadcast(this, false);
	}
	Super::EndPlay(EndPlayReason);
}

void USupplyTruckComponent::OnTruckDied()
{
	RemoveMiniMapIcon();
	if (bM_HasRunEnded)
	{
		return;
	}
	bM_HasRunEnded = true;
	// The cargo is lost with the truck.
	M_Cargo.Reset();
	if (M_SupplyPoint.IsValid())
	{
		M_SupplyPoint->OnSupplyTruckLeftBay(this);
	}
	OnSupplyTruckRunEnded.Broadcast(this, false);
}

uint8 USupplyTruckComponent::GetTruckOwningPlayer() const
{
	const URTSComponent* TruckRTSComponent = M_Truck.IsValid() ? M_Truck->GetRTSComponent() : nullptr;
	return IsValid(TruckRTSComponent) ? TruckRTSComponent->GetOwningPlayer() : SupplyPointConstants::NeutralOwner;
}

void USupplyTruckComponent::StartSupplyRun(ASupplyPoint* SupplyPoint)
{
	if (not IsValid(SupplyPoint))
	{
		RTSFunctionLibrary::ReportError("Supply truck got no supply point to drive to: " + GetNameSafe(GetOwner()));
		return;
	}
	M_SupplyPoint = SupplyPoint;
	SetSupplyTruckState(ESupplyTruckState::DrivingToApproach);
}

void USupplyTruckComponent::ReturnToBase()
{
	if (bM_HasRunEnded or M_SupplyTruckState == ESupplyTruckState::DrivingToBase)
	{
		return;
	}
	const bool bIsInOrNearBay = M_SupplyTruckState == ESupplyTruckState::DrivingToPark
		or M_SupplyTruckState == ESupplyTruckState::WaitingForSupplies
		or M_SupplyTruckState == ESupplyTruckState::BeingLoaded;
	if (M_SupplyPoint.IsValid())
	{
		M_SupplyPoint->OnSupplyTruckLeftBay(this);
	}
	// From the bay the truck first drives out forwards; anywhere else it turns for home at once.
	SetSupplyTruckState(bIsInOrNearBay and M_SupplyPoint.IsValid()
		                    ? ESupplyTruckState::DrivingToExit
		                    : ESupplyTruckState::DrivingToBase);
}

void USupplyTruckComponent::OnLoadingStarted(const float Duration)
{
	SetSupplyTruckState(ESupplyTruckState::BeingLoaded);
	OnLoadingStartedBP.Broadcast(Duration);
}

void USupplyTruckComponent::OnLoadingFinished(const FSupplyTruckCargo& LoadedCargo)
{
	M_Cargo = LoadedCargo;
	OnCargoLoadedBP.Broadcast(M_Cargo);
	SetSupplyTruckState(ESupplyTruckState::DrivingToExit);
	OnSupplyTruckLoaded.Broadcast(this);
}

void USupplyTruckComponent::SetSupplyTruckState(const ESupplyTruckState NewState)
{
	M_SupplyTruckState = NewState;
	M_DriveState = FSupplyTruckDriveState();
}

void USupplyTruckComponent::UpdateSupplyRun()
{
	if (bM_HasRunEnded or not M_Truck.IsValid())
	{
		return;
	}
	switch (M_SupplyTruckState)
	{
	case ESupplyTruckState::DrivingToApproach:
	case ESupplyTruckState::DrivingToPark:
	case ESupplyTruckState::WaitingForSupplies:
	case ESupplyTruckState::BeingLoaded:
	case ESupplyTruckState::DrivingToExit:
		UpdateDriveToPoint();
		break;
	case ESupplyTruckState::DrivingToBase:
		UpdateDriveToBase();
		break;
	default:
		break;
	}
	UpdateStuckWatch();
}

bool USupplyTruckComponent::GetIsDrivingState() const
{
	return M_SupplyTruckState == ESupplyTruckState::DrivingToApproach
		or M_SupplyTruckState == ESupplyTruckState::DrivingToPark
		or M_SupplyTruckState == ESupplyTruckState::DrivingToExit
		or M_SupplyTruckState == ESupplyTruckState::DrivingToBase;
}

void USupplyTruckComponent::UpdateStuckWatch()
{
	using namespace SupplyTruckPrivate;
	const UWorld* World = GetWorld();
	if (World == nullptr or bM_HasRunEnded or not M_Truck.IsValid() or not GetIsDrivingState()
		or not M_DriveState.bHasOrder)
	{
		M_DriveState.WindowStartSeconds = -1.0;
		return;
	}
	const double Now = World->GetTimeSeconds();
	const FVector TruckLocation = M_Truck->GetActorLocation();
	if (M_DriveState.WindowStartSeconds < 0.0)
	{
		M_DriveState.WindowStartSeconds = Now;
		M_DriveState.WindowStartLocation = TruckLocation;
		return;
	}
	if (Now - M_DriveState.WindowStartSeconds < StuckWindowSeconds)
	{
		return;
	}
	const bool bMadeProgress = FVector::Dist2D(TruckLocation, M_DriveState.WindowStartLocation) >= StuckProgressCm;
	M_DriveState.WindowStartSeconds = Now;
	M_DriveState.WindowStartLocation = TruckLocation;
	if (bMadeProgress)
	{
		M_DriveState.StuckWindows = 0;
		return;
	}
	++M_DriveState.StuckWindows;
	if (M_DriveState.StuckWindows >= StuckWindowsBeforeAbandon)
	{
		AbandonSupplyRun();
		return;
	}
	// Wedged while still "moving": the next update gives the order again (and counts it towards a loose arrival).
	M_DriveState.bHasOrder = false;
}

void USupplyTruckComponent::AbandonSupplyRun()
{
	RTSFunctionLibrary::ReportError(
		"Supply truck could not reach its goal and was removed (cargo lost): " + GetNameSafe(GetOwner()));
	bM_HasRunEnded = true;
	M_Cargo.Reset();
	if (M_SupplyPoint.IsValid())
	{
		M_SupplyPoint->OnSupplyTruckLeftBay(this);
	}
	OnSupplyTruckRunEnded.Broadcast(this, false);
	RemoveMiniMapIcon();
	if (M_Truck.IsValid())
	{
		M_Truck->OnUnitDies.Remove(M_TruckDeathHandle);
		M_Truck->Destroy();
	}
}

void USupplyTruckComponent::UpdateDriveToPoint()
{
	const bool bIsLeavingBay = M_SupplyTruckState == ESupplyTruckState::DrivingToExit;
	if (not M_SupplyPoint.IsValid())
	{
		// The point is gone: nothing to load and no bay to leave through.
		SetSupplyTruckState(ESupplyTruckState::DrivingToBase);
		return;
	}
	if (not bIsLeavingBay and M_SupplyPoint->GetOwningPlayer() != GetTruckOwningPlayer())
	{
		ReturnToBase();
		return;
	}
	FVector Goal = FVector::ZeroVector;
	FRotator FinalRotation = FRotator::ZeroRotator;
	if (not GetGoalOfState(Goal, FinalRotation))
	{
		// Waiting or being loaded.
		return;
	}
	if (GetHasReachedGoal(Goal))
	{
		OnGoalReached();
		return;
	}
	if (not M_DriveState.bHasOrder or M_Truck->GetIsUnitIdle())
	{
		IssueMoveOrder(Goal, FinalRotation);
	}
}

void USupplyTruckComponent::UpdateDriveToBase()
{
	USupplyPointSubsystem* SupplySubsystem = USupplyPointSubsystem::Get(this);
	FTransform BaseTransform;
	if (SupplySubsystem == nullptr or not SupplySubsystem->GetSupplyBaseTransform(GetTruckOwningPlayer(), BaseTransform))
	{
		// No base right now (HQ destroyed or not delivered yet): wait where we are.
		M_DriveState.bHasOrder = false;
		return;
	}
	const FVector Goal = BaseTransform.GetLocation();
	if (GetHasReachedGoal(Goal))
	{
		DeliverCargoAndLeave();
		return;
	}
	const bool bGoalMoved = FVector::Dist2D(Goal, M_DriveState.LastOrderedGoal)
		> SupplyTruckPrivate::GoalMovedReorderDistance;
	if (not M_DriveState.bHasOrder or bGoalMoved or M_Truck->GetIsUnitIdle())
	{
		IssueMoveOrder(Goal, BaseTransform.Rotator());
	}
}

bool USupplyTruckComponent::GetGoalOfState(FVector& OutGoal, FRotator& OutFinalRotation) const
{
	if (not M_SupplyPoint.IsValid())
	{
		return false;
	}
	const FTransform ParkTransform = M_SupplyPoint->GetTruckParkTransform();
	OutFinalRotation = ParkTransform.Rotator();
	switch (M_SupplyTruckState)
	{
	case ESupplyTruckState::DrivingToApproach:
		OutGoal = M_SupplyPoint->GetTruckApproachLocation();
		return true;
	case ESupplyTruckState::DrivingToPark:
		OutGoal = M_SupplyPoint->GetTruckParkOrderLocation();
		return true;
	case ESupplyTruckState::DrivingToExit:
		OutGoal = M_SupplyPoint->GetTruckExitLocation();
		return true;
	default:
		return false;
	}
}

bool USupplyTruckComponent::GetHasReachedGoal(const FVector& Goal) const
{
	using namespace SupplyTruckPrivate;
	const float DistanceToGoal = FVector::Dist2D(M_Truck->GetActorLocation(), Goal);
	const bool bIsStoppedAfterRetries = M_DriveState.OrdersThisLeg >= OrdersBeforeLooseArrival and M_Truck->GetIsUnitIdle();
	switch (M_SupplyTruckState)
	{
	case ESupplyTruckState::DrivingToPark:
		{
			// The order aims past the park point; what counts is how close the stopped truck is to the point itself.
			// The truck must have stopped: the crane works on a standing truck.
			const float DistanceToPark = M_SupplyPoint.IsValid()
				                             ? FVector::Dist2D(M_Truck->GetActorLocation(),
				                                               M_SupplyPoint->GetTruckParkTransform().GetLocation())
				                             : DistanceToGoal;
			return M_Truck->GetIsUnitIdle() and M_DriveState.bHasOrder
				and DistanceToPark <= (bIsStoppedAfterRetries ? LooseParkTolerance : ParkTolerance);
		}
	case ESupplyTruckState::DrivingToBase:
		return DistanceToGoal <= (bIsStoppedAfterRetries ? LooseBaseArrivalTolerance : BaseArrivalTolerance);
	default:
		return DistanceToGoal <= PassWaypointTolerance or bIsStoppedAfterRetries;
	}
}

void USupplyTruckComponent::OnGoalReached()
{
	switch (M_SupplyTruckState)
	{
	case ESupplyTruckState::DrivingToApproach:
		SetSupplyTruckState(ESupplyTruckState::DrivingToPark);
		break;
	case ESupplyTruckState::DrivingToPark:
		OnParkedInBay();
		break;
	case ESupplyTruckState::DrivingToExit:
		SetSupplyTruckState(ESupplyTruckState::DrivingToBase);
		break;
	default:
		break;
	}
}

void USupplyTruckComponent::OnParkedInBay()
{
	// Set before telling the point: ready supplies start the loading (and its state) inside that call.
	SetSupplyTruckState(ESupplyTruckState::WaitingForSupplies);
	if (not M_SupplyPoint.IsValid() or not M_SupplyPoint->OnSupplyTruckParked(this))
	{
		ReturnToBase();
	}
}

void USupplyTruckComponent::IssueMoveOrder(const FVector& Goal, const FRotator& FinalRotation)
{
	constexpr bool bSetUnitToIdle = true;
	// In the bay the heading matters (the crane works along the bed), also when a re-order ended in a reverse move.
	const bool bForceFinalRotation = M_SupplyTruckState == ESupplyTruckState::DrivingToPark;
	const ECommandQueueError Error = M_Truck->MoveToLocation(Goal, bSetUnitToIdle, FinalRotation,
	                                                         bForceFinalRotation);
	// A refused order (ability array not filled yet right after spawn) is simply given again on the next update.
	M_DriveState.bHasOrder = Error == ECommandQueueError::NoError;
	M_DriveState.LastOrderedGoal = Goal;
	if (M_DriveState.bHasOrder)
	{
		++M_DriveState.OrdersThisLeg;
	}
}

void USupplyTruckComponent::DeliverCargoAndLeave()
{
	bM_HasRunEnded = true;
	SetSupplyTruckState(ESupplyTruckState::Delivered);
	if (USupplyPointSubsystem* SupplySubsystem = USupplyPointSubsystem::Get(this))
	{
		SupplySubsystem->DeliverCargo(GetTruckOwningPlayer(), M_Cargo);
	}
	M_Cargo.Reset();
	OnSupplyTruckRunEnded.Broadcast(this, true);
	RemoveMiniMapIcon();
	if (M_Truck.IsValid())
	{
		M_Truck->OnUnitDies.Remove(M_TruckDeathHandle);
		M_Truck->Destroy();
	}
}

void USupplyTruckComponent::RemoveMiniMapIcon()
{
	if (M_MiniMapIconId.IsNone())
	{
		return;
	}
	if (AFowManager* FowManager = FRTS_Statics::GetFowManager(GetOwner()))
	{
		FowManager->RemoveCustomMiniMapIcon(M_MiniMapIconId);
	}
	M_MiniMapIconId = NAME_None;
}
