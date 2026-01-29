// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/EnergyComponent/LowEnergyStrategy.h"
#include "EnergyComp.generated.h"

class UBehaviourComp;
class ULowEnergyBehaviour;
class UMaterialInterface;
class UMeshComponent;

USTRUCT()
struct FEnergyMeshMaterialCache
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> MeshComponent;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
};


/**
 * @brief Manages the energy supply of an actor, including registration with the PlayerResourceManager.
 *
 * This component represents the energy-producing or consuming capability of an actor within the game. It tracks the energy amount supplied or consumed, manages its enabled state, and handles registration with the `UPlayerResourceManager` to update the overall energy resources.
 *
 * @note 1) When enabled, the component registers itself with the `UPlayerResourceManager`, contributing its energy amount to the player's total energy supply.
 * @note 2) When disabled or destroyed, it deregisters itself from the `UPlayerResourceManager`, removing its energy contribution.
 * @note 3) The energy amount can be initialized and updated, and any changes are communicated to the `UPlayerResourceManager` if the component is enabled.
 * @note 4) This component is intended to be attached to actors that produce or consume energy, such as power plants or energy consumers.
 * @note 5) Usage Workflow:
 * - Initialization: Call `InitEnergyComponent()` to set the initial energy amount.
 * - Enabling/Disabling: Use `SetEnabled()` to enable or disable the component, which registers or deregisters it with the `UPlayerResourceManager`.
 * - Updating Energy: To change the energy amount, use `UpdateEnergySupplied()`. If the component is enabled, the `UPlayerResourceManager` will be notified of the change.
 * @note 6) Ensure that the `UPlayerResourceManager` is available in the game context, as this component depends on it for managing energy resources.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEnergyComp : public UActorComponent
{
	GENERATED_BODY()

public:
	/**
	 * @brief Constructs the energy component, setting default values.
	 *
	 * Initializes the component by disabling ticking and preparing it for use within the game.
	 */
	UEnergyComp(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Enables or disables the energy component, registering or deregistering it with the PlayerResourceManager.
	 *
	 * When enabled, the component registers itself with the `UPlayerResourceManager`, contributing its energy to the player's total energy supply. When disabled, it deregisters itself, removing its contribution.
	 *
	 * @param bEnabled A boolean flag indicating whether to enable (`true`) or disable (`false`) the component; used to manage registration with the `UPlayerResourceManager`.
	 * @note If the `UPlayerResourceManager` cannot be obtained, an error is reported.
	 * @pre The component must have a valid owner and be properly initialized.
	 */
	void SetEnabled(const bool bEnabled);

	/**
	 * @brief Notifies this component about changes in the base energy state.
	 * @param bIsLowPower Whether the base is currently in a low power state.
	 * @param bSuppressErrorsOnJustEnabledEnergyComp
	 */
	void OnBaseEnergyChange(const bool bIsLowPower, const bool bSuppressErrorsOnJustEnabledEnergyComp);

	/**
	 * @brief Retrieves the amount of energy supplied by this component.
	 *
	 * Returns the current energy amount that this component contributes to the player's total energy supply can be negitive when the component consumes energy.
	 *
	 * @return The energy amount supplied by the component.
	 */
	int32 GetEnergySupplied() const { return M_Energy; }
	void UpgradeEnergy(const int32 Amount);

	/**
	 * @brief Initializes the energy component with the specified energy amount.
	 *
	 * Sets the energy supplied by this component and updates the `UPlayerResourceManager` if the component is enabled.
	 *
	 * @param Energy The initial energy amount to be supplied by this component; used to set `M_Energy` and update the resource manager.
	 * @pre The component should be properly initialized.
	 * @note If the component is enabled, the `UPlayerResourceManager` will be notified of the new energy amount.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Energy")
	void InitEnergyComponent(const int32 Energy);

protected:
	/**
	 * @brief Called when the game starts; performs necessary initialization.
	 *
	 * If `bStartEnabled` is `true`, the component is enabled and registers with the `UPlayerResourceManager`.
	 *
	 * @note Overrides the base class implementation.
	 * @pre The component must be properly initialized.
	 */
	virtual void BeginPlay() override;

	/**
	 * @brief Called when the component is being destroyed; performs cleanup.
	 *
	 * If the component is enabled, it deregisters itself from the `UPlayerResourceManager` before destruction.
	 *
	 * @note Overrides the base class implementation.
	 */
	virtual void BeginDestroy() override;

	/** Determines whether the component starts enabled. If true, the component will enable itself and register with the PlayerResourceManager at game start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStartEnabled = false;

	/** Override the auto-detected low energy strategy. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Energy")
	ELowEnergyStrategy OverwriteLowEnergyStrategy = ELowEnergyStrategy::LES_Invalid;



	/** Cache owner-specific mesh/material data for low energy visuals. */
	virtual void CacheOwnerMaterials();

	void CacheMeshComponentMaterials(UMeshComponent* MeshComponent);
	void CacheMeshesFromActor(AActor* ActorToCache);
	void ApplyLowEnergyMaterials();
	void RestoreOriginalMaterials();
	void ApplyMaterialsForCurrentEnergyState();
	bool GetDoesStrategyUseMaterials() const;
	bool GetDoesStrategyUseBehaviour() const;
	virtual bool GetShouldApplyMaterialChanges() const;
	bool GetIsShuttingDown() const;

private:
	bool bM_IsEnabled = false;
	bool bM_IsLowEnergy = false;
	bool bM_IsShuttingDown = false;

	int32 M_Energy = 0;

	UPROPERTY()
	TArray<FEnergyMeshMaterialCache> M_CachedMeshMaterials;

	UPROPERTY()
	TWeakObjectPtr<UBehaviourComp> M_OwnerBehaviourComponent;

	UPROPERTY()
	TSubclassOf<ULowEnergyBehaviour> M_LowEnergyBehaviourClass;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> M_LowEnergyMaterial = nullptr;

	bool bM_HasAppliedLowEnergyBehaviour = false;

	ELowEnergyStrategy M_LowEnergyStrategy = ELowEnergyStrategy::LES_Invalid;

	/**
	 * @brief Updates the energy amount supplied by this component.
	 *
	 * Changes the energy amount and notifies the `UPlayerResourceManager` of the update if the component is enabled.
	 *
	 * @param NewEnergySupplied The new energy amount to be supplied by this component; used to update `M_Energy` and the resource manager.
	 * @note The previous energy amount is provided to the `UPlayerResourceManager` to adjust the total supply accurately.
	 * @pre The component must be enabled to notify the `UPlayerResourceManager`.
	 * @post `M_Energy` is updated with the new value.
	 */
	void UpdateEnergySupplied(const int32 NewEnergySupplied);

	void BeginPlay_CacheLowEnergySettings();
	void BeginPlay_InitializeLowEnergyStrategy();
	void BeginPlay_CheckInitialLowEnergyState();
	void DetermineLowEnergyStrategy();
	void CacheOwnerBehaviourComponent();
	void ApplyLowEnergyBehaviour();
	void RemoveLowEnergyBehaviour();

	bool GetIsOwnerBehaviourComponentValid() const;
	bool GetIsValidLowEnergyMaterial() const;
	bool GetIsValidLowEnergyBehaviourClass() const;

	void ReportInvalidMaterialCache(const UMeshComponent* MeshComponent, const int32 CachedMaterialCount,
	                                const int32 CurrentMaterialCount, const FString& Context) const;
	void ReportNoCachedMeshes(const FString& Context) const;
	void ReportDuplicateEnergyState(const bool bAttemptedLowEnergy, const bool bSuppress) const;
};
