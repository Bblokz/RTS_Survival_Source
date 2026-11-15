// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/SupplyPoints/SupplyPointTypes.h"
#include "SupplyTruckComponent.generated.h"

class ASupplyPoint;
class ATankMaster;
class USupplyTruckComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSupplyTruckLoaded, USupplyTruckComponent* /*SupplyTruck*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSupplyTruckRunEnded, USupplyTruckComponent* /*SupplyTruck*/,
                                     bool /*bDelivered*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSupplyTruckLoadingStartedBP, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSupplyTruckCargoLoadedBP, const FSupplyTruckCargo&, LoadedCargo);

/** The order the truck is following and how often it had to be given again. */
USTRUCT()
struct FSupplyTruckDriveState
{
	GENERATED_BODY()

	FVector LastOrderedGoal = FVector::ZeroVector;
	bool bHasOrder = false;
	// Orders given for the current leg without the truck getting there; loosens the arrival check when it is stuck.
	int32 OrdersThisLeg = 0;
	// No-progress watch: where the truck was when the current window started (negative time = no window).
	FVector WindowStartLocation = FVector::ZeroVector;
	double WindowStartSeconds = -1.0;
	int32 StuckWindows = 0;
};

/**
 * @brief Turns a tank master vehicle into an uncontrollable supply truck: USupplyPointSubsystem spawns the truck
 * Blueprint set in Project Settings > RTS Supply Points and steers it through this component into the truck bay
 * of a captured supply point and back to the base. The cargo only reaches the owner when the truck arrives; a
 * destroyed truck loses it.
 * Add the component in the truck Blueprint to react to its events; a truck without one gets it when it is sent.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API USupplyTruckComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USupplyTruckComponent();

	/** Sends the truck to the truck bay of this supply point. */
	void StartSupplyRun(ASupplyPoint* SupplyPoint);

	/** Sends the truck home with whatever it carries; used when its supply point is lost. */
	void ReturnToBase();

	// Called by the supply point while the truck stands in the bay.
	void OnLoadingStarted(float Duration);
	void OnLoadingFinished(const FSupplyTruckCargo& LoadedCargo);

	uint8 GetTruckOwningPlayer() const;

	UFUNCTION(BlueprintPure, Category = "Supply Truck")
	ESupplyTruckState GetSupplyTruckState() const { return M_SupplyTruckState; }

	UFUNCTION(BlueprintPure, Category = "Supply Truck")
	const FSupplyTruckCargo& GetCargo() const { return M_Cargo; }

	UFUNCTION(BlueprintPure, Category = "Supply Truck")
	ATankMaster* GetTruck() const { return M_Truck.Get(); }

	ASupplyPoint* GetSupplyPoint() const { return M_SupplyPoint.Get(); }

	// Fired when the loaded truck leaves the bay.
	FOnSupplyTruckLoaded OnSupplyTruckLoaded;
	// Fired once: the truck delivered its cargo (true) or was destroyed on the way (false).
	FOnSupplyTruckRunEnded OnSupplyTruckRunEnded;

	// The crane starts loading; play bed/tarp animations here.
	UPROPERTY(BlueprintAssignable, Category = "Supply Truck")
	FOnSupplyTruckLoadingStartedBP OnLoadingStartedBP;

	// The truck is loaded; show the cargo in the bed (kinds with amount zero stay hidden).
	UPROPERTY(BlueprintAssignable, Category = "Supply Truck")
	FOnSupplyTruckCargoLoadedBP OnCargoLoadedBP;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BeginPlay_MakeUncontrollable();
	void BeginPlay_BindTruckDeath();
	void BeginPlay_AddMiniMapIcon();
	void BeginPlay_StartSupplyRunUpdates();

	bool GetIsValidTruck() const;

	void OnTruckDied();
	// Runs the drive state machine: checks arrival, repeats refused or dropped orders.
	void UpdateSupplyRun();
	void UpdateDriveToPoint();
	void UpdateDriveToBase();
	// A truck that should drive but makes no progress gets its order again; after too many tries the run is given up.
	void UpdateStuckWatch();
	bool GetIsDrivingState() const;
	void AbandonSupplyRun();
	bool GetGoalOfState(FVector& OutGoal, FRotator& OutFinalRotation) const;
	bool GetHasReachedGoal(const FVector& Goal) const;
	void OnGoalReached();
	void OnParkedInBay();
	void IssueMoveOrder(const FVector& Goal, const FRotator& FinalRotation);
	void SetSupplyTruckState(ESupplyTruckState NewState);
	void DeliverCargoAndLeave();
	void RemoveMiniMapIcon();

	ESupplyTruckState M_SupplyTruckState = ESupplyTruckState::Idle;

	FSupplyTruckCargo M_Cargo;

	FSupplyTruckDriveState M_DriveState;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_Truck;

	UPROPERTY()
	TWeakObjectPtr<ASupplyPoint> M_SupplyPoint;

	FTimerHandle M_SupplyRunTimerHandle;

	FDelegateHandle M_TruckDeathHandle;

	FName M_MiniMapIconId = NAME_None;

	// Set when the run ended (delivered or destroyed) so the end is reported once.
	bool bM_HasRunEnded = false;
};
