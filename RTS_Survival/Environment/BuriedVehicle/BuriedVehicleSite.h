// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "BuriedVehicleSite.generated.h"

class UBoxComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuriedVehicleRecovered, AActor*, Vehicle);

/**
 * @brief A vehicle half-buried in a sand mound that a squad scavenges; the squad is consumed (it takes command) and the
 * real vehicle spawns for the squad's player on the spot. The buried parts hide, the mound stays as a low plateau.
 * Child Blueprints set the sand mesh (with Scav_ sockets at the foot), the parts under Pose and RecoveredVehicleClass.
 * @note ProgressBarWidget: set its widget class to a UTimeProgressBarWidget Blueprint in the parent Blueprint.
 */
UCLASS()
class RTS_SURVIVAL_API ABuriedVehicleSite : public AScavengeableObject
{
	GENERATED_BODY()

public:
	ABuriedVehicleSite(const FObjectInitializer& ObjectInitializer);

	// Fires once when scavenging spawned the vehicle; the vehicle belongs to the scavenging squad's player.
	UPROPERTY(BlueprintAssignable, Category="BuriedVehicle")
	FOnBuriedVehicleRecovered OnVehicleRecovered;

	/** @return The recovered vehicle, or null before recovery (or once it is destroyed). */
	UFUNCTION(BlueprintPure, Category="BuriedVehicle")
	AActor* GetRecoveredVehicle() const;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void OnUnitDies(const ERTSDeathType DeathType) override;
	virtual void OnScavengeCompletedBySquad(ASquadController& ScavengingSquad) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	UFUNCTION(BlueprintImplementableEvent, Category="BuriedVehicle")
	void BP_OnVehicleRecovered(AActor* Vehicle);

	// The vehicle Blueprint that is spawned for the scavenging player.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BuriedVehicle")
	TSubclassOf<AActor> RecoveredVehicleClass;

	// Spawn offset in actor space (yaw only, the burial tilt is dropped); Z lifts the vehicle onto the ground.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BuriedVehicle")
	FVector VehicleSpawnOffset = FVector(0.0, 0.0, 60.0);

	// Off by default: a stray shell must not cost the player the reward. When on, the site dies as a plain wreck.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BuriedVehicle")
	bool bCanBeDestroyedByDamage = false;

	// Root: the sand mound and the scavenge mesh (its Scav_ sockets are where the squad stands).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BuriedVehicle")
	TObjectPtr<UStaticMeshComponent> Sand;

	// Burial pitch/roll/sink; the hull, turret and weapon meshes sit under it at their vehicle Blueprint transforms.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BuriedVehicle")
	TObjectPtr<USceneComponent> Pose;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BuriedVehicle")
	TObjectPtr<UWidgetComponent> ProgressBarWidget;

	// Dynamic nav obstacle (null area) over the hull and the high sand; removed on recovery so the vehicle can leave.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BuriedVehicle")
	TObjectPtr<UBoxComponent> NavBlocker;

private:
	void PostInitializeComponents_SetupSandCollision() const;
	void PostInitializeComponents_SetupBuriedPartCollision() const;
	void BeginPlay_InitProgressBar();

	void RecoverVehicle();
	void HideBuriedParts() const;
	void ClearMoundBlocking();
	AActor* SpawnRecoveredVehicle() const;
	int32 GetRecoveringPlayerOrHuman() const;

	bool GetIsValidSand() const;
	bool GetIsValidPose() const;
	bool GetIsValidProgressBarWidget() const;
	bool GetIsValidNavBlocker() const;

	// Owner of the squad that finished scavenging, read before the squad is consumed.
	int32 M_RecoveringPlayer = INDEX_NONE;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_RecoveredVehicle;
};
