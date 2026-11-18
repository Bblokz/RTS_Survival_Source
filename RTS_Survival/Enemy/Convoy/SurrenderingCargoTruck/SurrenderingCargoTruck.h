// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SupplyPoints/SupplyPointTypes.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"

#include "SurrenderingCargoTruck.generated.h"

class AConvoy;
class ASurrenderingCargoTruck;
class USupplyTruckComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurrenderingTruckSurrendered, ASurrenderingCargoTruck*, Truck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSurrenderingTruckDelivered, ASurrenderingCargoTruck*, Truck,
                                             const FSupplyTruckCargo&, DeliveredCargo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurrenderingTruckLost, ASurrenderingCargoTruck*, Truck);

/** What makes a convoy truck of this class give up. */
UENUM(BlueprintType)
enum class ESurrenderingTruckTrigger : uint8
{
	// Every escort of its convoy is dead.
	AllEscortsDestroyed,
	// No living escort within EscortProtectionRadius of the truck (also true once every escort is dead).
	NoEscortNearby
};

UENUM(BlueprintType)
enum class ESurrenderingTruckState : uint8
{
	// Drives as a convoy truck and may surrender.
	CanSurrender,
	// Left its convoy without surrendering (exit, convoy ended) or never was in one; only SurrenderNow captures it.
	NoConvoy,
	// Gave up: belongs to its captor and stands still for SurrenderStandSeconds.
	Surrendered,
	// Drives itself to its drop-off; the player cannot steer it.
	DrivingToDropOff,
	// Paid its cargo at the drop-off; the truck is removed.
	Delivered,
	// Destroyed (or removed) after it surrendered; the cargo is lost.
	Lost
};

/** When a truck of this class surrenders and to whom. */
USTRUCT(BlueprintType)
struct FSurrenderingTruckSurrenderSettings
{
	GENERATED_BODY()

	// Off: this truck never surrenders by itself (SurrenderNow still works).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
	bool bCanSurrender = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
	ESurrenderingTruckTrigger Trigger = ESurrenderingTruckTrigger::AllEscortsDestroyed;

	// NoEscortNearby: a living escort this close (cm, 2D) keeps the truck from surrendering.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "0",
		EditCondition = "Trigger == ESurrenderingTruckTrigger::NoEscortNearby"))
	float EscortProtectionRadius = 3500.f;

	// The truck only gives up to a captor in sight: a unit of CapturingPlayer must be within CaptorSearchRadius.
	// Also keeps trucks of a convoy without escorts from surrendering the moment the column starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
	bool bRequireCaptorNearby = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "100",
		EditCondition = "bRequireCaptorNearby"))
	float CaptorSearchRadius = 4500.f;

	// A truck below this health fraction does not surrender (0 = any living truck surrenders).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "0", ClampMax = "1"))
	float MinHealthFractionToSurrender = 0.f;

	// The player the truck surrenders to and drives for afterwards.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "1", ClampMax = "4"))
	int32 CapturingPlayer = 1;

	// Seconds the truck stands still after surrendering (white flag) before it drives to the drop-off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "0"))
	float SurrenderStandSeconds = 3.f;

	// Seconds between two surrender checks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "0.1"))
	float CheckIntervalSeconds = 0.5f;
};

/** Where a captured truck drives and what it pays there. */
USTRUCT(BlueprintType)
struct FSurrenderingTruckDeliverySettings
{
	GENERATED_BODY()

	// The truck drives to the nearest actor with this tag (the mine adit drop-off; put the tag on a marker on the
	// road there). SetDropOffTarget overrides it. With neither, the truck drives to its captor's base (HQ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	FName DropOffTag = TEXT("CapturedTruckDropOff");

	// Multiplies the convoy's per-truck cargo (what this truck's cache would pay) for the captured payout.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery", meta = (ClampMin = "0"))
	float CapturedCargoMultiplier = 1.f;

	// On: pay FixedCargo instead of the convoy's cargo (also used when the truck has no convoy).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	bool bUseFixedCargo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery", meta = (EditCondition = "bUseFixedCargo"))
	FSupplyTruckCargo FixedCargo;
};

/**
 * @brief Convoy truck that can surrender: point FConvoyTruckSpec::TruckClass at a Blueprint child of this class.
 * When its convoy's escort is gone and a captor is near, it leaves the convoy as captured, changes owner to the
 * captor and drives itself (through USupplyTruckComponent) to the drop-off, where it pays the cargo on arrival.
 * Only trucks of this class surrender; every other convoy truck behaves as before.
 * @note InitTrackedTank / SetupTurret: set up in the Blueprint child exactly as for any tracked tank master.
 */
UCLASS()
class RTS_SURVIVAL_API ASurrenderingCargoTruck : public ATrackedTankMaster
{
	GENERATED_BODY()

public:
	ASurrenderingCargoTruck(const FObjectInitializer& ObjectInitializer);

	/** @brief Mission scripting: the drop-off this truck drives to once captured; wins over the drop-off tag. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Surrendering Truck")
	void SetDropOffTarget(AActor* DropOffTarget);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Surrendering Truck")
	void SetCanSurrender(const bool bNewCanSurrender);

	/**
	 * @brief Mission scripting: surrenders now, skipping the trigger, captor and health checks.
	 * @return True when the truck surrendered; false when it is dead or already surrendered.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Surrendering Truck")
	bool SurrenderNow();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Surrendering Truck")
	ESurrenderingTruckState GetSurrenderState() const { return M_SurrenderState; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Surrendering Truck")
	bool GetHasSurrendered() const;

	// What the truck pays on arrival; empty until it surrenders.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Surrendering Truck")
	const FSupplyTruckCargo& GetCapturedCargo() const { return M_CapturedCargo; }

	// The truck surrendered; it already belongs to its captor.
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck")
	FOnSurrenderingTruckSurrendered OnSurrendered;

	// The captured truck reached its drop-off and paid its cargo.
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck")
	FOnSurrenderingTruckDelivered OnCapturedCargoDelivered;

	// The captured truck was destroyed (or removed) before it delivered; the cargo is lost.
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck")
	FOnSurrenderingTruckLost OnCapturedTruckLost;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Show the surrender here: white flag, voice line, minimap icon.
	UFUNCTION(BlueprintImplementableEvent, Category = "Surrendering Truck")
	void BP_OnSurrendered(int32 NewOwningPlayer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Surrendering Truck")
	void BP_OnDriveToDropOffStarted(AActor* DropOffTarget);

	UFUNCTION(BlueprintImplementableEvent, Category = "Surrendering Truck")
	void BP_OnCapturedCargoDelivered(const FSupplyTruckCargo& DeliveredCargo);

	UFUNCTION(BlueprintImplementableEvent, Category = "Surrendering Truck")
	void BP_OnCapturedTruckLost();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrendering Truck")
	FSurrenderingTruckSurrenderSettings SurrenderSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrendering Truck")
	FSurrenderingTruckDeliverySettings DeliverySettings;

private:
	void BeginPlay_StartSurrenderChecks();

	void CheckSurrender();
	/** @return The convoy this truck drives in; nullptr before the convoy registered it or after it left. */
	AConvoy* GetConvoyOfTruck();
	bool GetIsConvoyReadyForSurrender(const AConvoy& Convoy) const;
	bool GetIsSurrenderTriggerMet(const AConvoy& Convoy) const;
	bool GetIsHealthEnoughToSurrender() const;
	bool GetIsCaptorNearby() const;
	// Non-const: the engine's dying flag has no const getter.
	bool GetIsTruckAlive();

	void Surrender();
	void Surrender_LeaveConvoy();
	void Surrender_ChangeOwnerToCaptor();
	bool Surrender_SetupSupplyTruck();
	void ScheduleDriveToDropOff();

	FSupplyTruckCargo BuildCapturedCargo() const;
	void StartDriveToDropOff();
	AActor* FindDropOffTarget() const;
	FTransform GetDeliveryTransformOfDropOff(const AActor& DropOff) const;

	void OnSupplyRunEnded(USupplyTruckComponent* SupplyTruck, bool bDelivered);
	void StopTimers();
	bool GetIsValidSupplyTruckComponent() const;

	ESurrenderingTruckState M_SurrenderState = ESurrenderingTruckState::CanSurrender;

	// Found lazily: the convoy registers the truck right after it spawned.
	UPROPERTY()
	TWeakObjectPtr<AConvoy> M_Convoy;

	bool bM_HasFoundConvoy = false;
	double M_ConvoySearchStartSeconds = -1.0;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_DropOffOverride;

	// Added (or found) when the truck surrenders; drives the truck and pays the cargo on arrival.
	UPROPERTY()
	TObjectPtr<USupplyTruckComponent> M_SupplyTruckComponent;

	FDelegateHandle M_SupplyRunEndedHandle;

	FSupplyTruckCargo M_CapturedCargo;

	FTimerHandle M_SurrenderCheckTimerHandle;
	FTimerHandle M_DriveToDropOffTimerHandle;
};
