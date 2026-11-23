// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/SurrenderingCargoTruck/W_SurrenderingTruckCountdown.h"
#include "RTS_Survival/SupplyPoints/SupplyPointTypes.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"

#include "SurrenderingCargoTruck.generated.h"

class AConvoy;
class ASurrenderingCargoTruck;
class UNiagaraSystem;
class USoundBase;
class USupplyTruckComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurrenderingTruckSurrendered, ASurrenderingCargoTruck*, Truck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSurrenderingTruckDelivered, ASurrenderingCargoTruck*, Truck,
                                             const FSupplyTruckCargo&, DeliveredCargo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurrenderingTruckLost, ASurrenderingCargoTruck*, Truck);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSurrenderingTruckScuttleCountdown, ASurrenderingCargoTruck*, Truck,
                                             float, CountdownSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSurrenderingTruckScuttled, ASurrenderingCargoTruck*, Truck);

/** What makes a convoy truck of this class give up. */
UENUM(BlueprintType)
enum class ESurrenderingTruckTrigger : uint8
{
	// Every escort of its convoy is dead.
	AllEscortsDestroyed,
	// No living escort within EscortProtectionRadius of the truck (also true once every escort is dead).
	NoEscortNearby
};

/** Which units of the capturing player can take a truck. */
UENUM(BlueprintType)
enum class ESurrenderingTruckCaptors : uint8
{
	// Only infantry: the crew gives up to soldiers on foot.
	SquadsOnly,
	SquadsAndVehicles
};

UENUM(BlueprintType)
enum class ESurrenderingTruckState : uint8
{
	// Drives as a convoy truck; may start its scuttle countdown or surrender.
	CanSurrender,
	// Left its convoy without surrendering (exit, convoy ended) or never was in one; only SurrenderNow captures it.
	NoConvoy,
	// The escort is gone: the crew waits for a captor and blows the truck up when the countdown runs out.
	ScuttleCountdown,
	// Gave up: belongs to its captor and stands still for SurrenderStandSeconds.
	Surrendered,
	// Drives itself to its drop-off; the player cannot steer it.
	DrivingToDropOff,
	// Paid its cargo at the drop-off; the truck is removed.
	Delivered,
	// Destroyed (or removed) after it surrendered; the cargo is lost.
	Lost,
	// The crew blew it up; it died as a convoy truck (the convoy drops its cargo cache).
	Scuttled,
	// Destroyed before it surrendered; it died as a convoy truck.
	Destroyed
};

/** When a truck of this class surrenders and to whom. */
USTRUCT(BlueprintType)
struct FSurrenderingTruckSurrenderSettings
{
	GENERATED_BODY()

	// Off: this truck never surrenders or scuttles by itself (SurrenderNow still works).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
	bool bCanSurrender = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
	ESurrenderingTruckTrigger Trigger = ESurrenderingTruckTrigger::AllEscortsDestroyed;

	// NoEscortNearby: a living escort this close (cm, 2D) keeps the truck from giving up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "0",
		EditCondition = "Trigger == ESurrenderingTruckTrigger::NoEscortNearby"))
	float EscortProtectionRadius = 3500.f;

	// The truck only gives up to a captor right beside it; off = it surrenders the moment the trigger is met.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender")
	bool bRequireCaptorNearby = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (EditCondition = "bRequireCaptorNearby"))
	ESurrenderingTruckCaptors Captors = ESurrenderingTruckCaptors::SquadsOnly;

	// A captor must come this close (cm, 2D) to take the truck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "100",
		EditCondition = "bRequireCaptorNearby"))
	float CaptorSearchRadius = 2000.f;

	// A truck below this health fraction cannot be taken: it only scuttles on its countdown (0 = any living truck).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrender", meta = (ClampMin = "0", ClampMax = "1"))
	float MinHealthFractionToSurrender = 0.6f;

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

/** The crew blows the truck up when nobody takes it in time; the convoy then drops its normal cargo cache. */
USTRUCT(BlueprintType)
struct FSurrenderingTruckScuttleSettings
{
	GENERATED_BODY()

	// Off: once the trigger is met the truck waits for a captor as long as it takes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle")
	bool bScuttleWhenNotCaptured = true;

	// Seconds from the trigger (last escort dead) until the crew blows the truck up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle", meta = (ClampMin = "0.5",
		EditCondition = "bScuttleWhenNotCaptured"))
	float CountdownSeconds = 12.f;

	// On: the truck stops for the countdown, so the capture window is a fixed place.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle")
	bool bStopDuringCountdown = true;

	// A convoy that never had an escort: the countdown starts once a unit of the capturing player comes this close
	// (cm, 2D, any unit kind) or the truck takes damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle", meta = (ClampMin = "100"))
	float UnescortedAlertRadius = 3000.f;

	// The blast kills the truck through the normal damage path with this damage type (picks the death).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle")
	ERTSDamageType ScuttleDamageType = ERTSDamageType::Kinetic;

	// Optional blast effect and sound at the truck; skipped while the fog of war hides the truck.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle")
	TObjectPtr<UNiagaraSystem> ScuttleExplosionEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle", meta = (ClampMin = "0.01"))
	float ScuttleExplosionEffectScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle")
	TObjectPtr<USoundBase> ScuttleExplosionSound = nullptr;

	// Screen-space countdown over the truck; the native class needs no asset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle|Countdown UI")
	TSubclassOf<UW_SurrenderingTruckCountdown> CountdownWidgetClass = UW_SurrenderingTruckCountdown::StaticClass();

	// Where the countdown hangs, relative to the truck's root.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle|Countdown UI")
	FVector CountdownWidgetOffset = FVector(0.f, 0.f, 450.f);

	// {0} = whole seconds left.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle|Countdown UI")
	FText CountdownTextFormat = NSLOCTEXT("SurrenderingCargoTruck", "ScuttleCountdown", "SCUTTLING {0}");

	// Seconds between two countdown UI refreshes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scuttle|Countdown UI", meta = (ClampMin = "0.02"))
	float CountdownRefreshSeconds = 0.1f;
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
	float CapturedCargoMultiplier = 2.5f;

	// On: pay FixedCargo instead of the convoy's cargo (also used when the truck has no convoy).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery")
	bool bUseFixedCargo = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Delivery", meta = (EditCondition = "bUseFixedCargo"))
	FSupplyTruckCargo FixedCargo;
};

/** Runtime state of the scuttle countdown. */
USTRUCT()
struct FSurrenderingTruckScuttleState
{
	GENERATED_BODY()

	FTimerHandle ScuttleTimerHandle;
	FTimerHandle RefreshTimerHandle;

	// Created when the countdown starts, destroyed when it ends.
	UPROPERTY()
	TObjectPtr<UWidgetComponent> CountdownWidgetComponent = nullptr;

	// Most escorts seen alive at once: zero means the convoy never had an escort (the countdown then needs an alert).
	int32 MostAliveEscortsSeen = 0;
	bool bWasDamaged = false;
	// The truck holds itself in place with a mobility restriction of its own.
	bool bIsHeldInPlace = false;
};

/**
 * @brief Point FConvoyTruckSpec::TruckClass at a Blueprint child: once its escort is gone the crew waits a short
 * countdown for infantry to take the truck, then blows it up. A taken truck changes owner and drives itself
 * (USupplyTruckComponent) to the drop-off, paying its cargo on arrival. Only trucks of this class do this.
 * @note InitTrackedTank: call in the Blueprint child, as for any tracked tank master.
 * @note SetupTurret: call in the Blueprint child for each turret, as for any tracked tank master.
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
	 * @brief Mission scripting: surrenders now, skipping the trigger, captor and health checks (also mid-countdown).
	 * @return True when the truck surrendered; false when it is dead, scuttled or already surrendered.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Surrendering Truck")
	bool SurrenderNow();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Surrendering Truck")
	ESurrenderingTruckState GetSurrenderState() const { return M_SurrenderState; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Surrendering Truck")
	bool GetHasSurrendered() const;

	/** @return Seconds until the crew blows the truck up; negative when no countdown runs. */
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Surrendering Truck")
	float GetScuttleSecondsLeft() const;

	// What the truck pays on arrival; empty until it surrenders.
	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Surrendering Truck")
	const FSupplyTruckCargo& GetCapturedCargo() const { return M_CapturedCargo; }

	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	// The escort is gone: the scuttle countdown started.
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck")
	FOnSurrenderingTruckScuttleCountdown OnScuttleCountdownStarted;

	// Nobody took the truck in time: the crew blows it up (fired just before it dies).
	UPROPERTY(BlueprintAssignable, Category = "Surrendering Truck")
	FOnSurrenderingTruckScuttled OnScuttled;

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
	virtual void UnitDies(const ERTSDeathType DeathType) override;

	// Crew warning (shout, flare, smoke) as the countdown starts.
	UFUNCTION(BlueprintImplementableEvent, Category = "Surrendering Truck")
	void BP_OnScuttleCountdownStarted(float CountdownSeconds);

	// The blast: called just before the truck dies; ScuttleExplosionEffect / Sound play beside it when set.
	UFUNCTION(BlueprintImplementableEvent, Category = "Surrendering Truck")
	void BP_OnScuttled();

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
	FSurrenderingTruckScuttleSettings ScuttleSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surrendering Truck")
	FSurrenderingTruckDeliverySettings DeliverySettings;

private:
	void BeginPlay_StartSurrenderChecks();

	void CheckSurrender();
	void CheckSurrender_InConvoy();
	void CheckSurrender_DuringCountdown();
	/** @return The convoy this truck drives in; nullptr before the convoy registered it or after it left. */
	AConvoy* GetConvoyOfTruck();
	bool GetIsConvoyReadyForSurrender(const AConvoy& Convoy) const;
	bool GetIsSurrenderTriggerMet(const AConvoy& Convoy) const;
	/** @return True when an escort-less convoy's truck has a reason to notice the captor (unit close or damage). */
	bool GetIsUnescortedTruckAlerted() const;
	bool GetCanBeCapturedNow() const;
	bool GetIsHealthEnoughToSurrender() const;
	/**
	 * @brief Looks for units of the capturing player around the truck; trucks that surrendered earlier never count.
	 * @param Radius 2D search radius in cm.
	 * @param bIncludeVehicles Also count the captor's tanks and vehicles, not only soldiers.
	 * @return True when one is in range.
	 */
	bool GetIsCaptorUnitWithin(float Radius, bool bIncludeVehicles) const;
	// Non-const: the engine's dying flag has no const getter.
	bool GetIsTruckAlive();

	void StartScuttleCountdown();
	void RefreshCountdownWidget() const;
	void ShowCountdownWidget();
	void RemoveCountdownWidget();
	void HoldInPlace();
	void ReleaseHoldInPlace();
	void Scuttle();
	void PlayScuttleExplosion() const;
	void EndScuttleCountdown();

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

	UPROPERTY()
	FSurrenderingTruckScuttleState M_ScuttleState;

	FTimerHandle M_SurrenderCheckTimerHandle;
	FTimerHandle M_DriveToDropOffTimerHandle;
};
