// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "VehicleModuleTypes.h"

#include "VehicleModuleBehaviour.generated.h"

class UArmorCalculation;
class UMeshComponent;

/** @brief Module identity and state supplied to a module behaviour before OnAdded. */
USTRUCT(BlueprintType)
struct FVehicleModuleBehaviourContext
{
	GENERATED_BODY()

	// Source key of the owning module; INDEX_NONE means the behaviour is not owned by a module slot.
	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	int32 ModuleId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	EVehicleModuleTypes Type = EVehicleModuleTypes::None;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	EVehicleModuleState State = EVehicleModuleState::Healthy;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	float CurrentHp = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Vehicle Modules")
	float MaxHp = 0.f;

	// Turret or gun mesh restricted by the behaviour; unset for unbound module types.
	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> BoundMesh;

	UPROPERTY()
	TWeakObjectPtr<UArmorCalculation> ArmorCalculation;
};

/**
 * @brief Optional effect of one vehicle module state, assigned per type/state in the shared module data asset.
 * UBehaviourComp owns exactly one instance per module slot and supplies the module context before OnAdded;
 * a same-class state change updates the context in place through OnModuleContextUpdated.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UVehicleModuleBehaviour : public UBehaviour
{
	GENERATED_BODY()

public:
	/** @brief Called by UBehaviourComp before OnAdded; establishes the module source identity. */
	void InitializeModuleContext(const FVehicleModuleBehaviourContext& NewContext);

	/** @brief Called by UBehaviourComp when the module changes state but keeps this behaviour class. */
	void UpdateModuleContext(const FVehicleModuleBehaviourContext& NewContext);

	const FVehicleModuleBehaviourContext& GetModuleContext() const { return M_ModuleContext; }

	/** @return True when a module slot owns this instance; generic class-based operations must skip it. */
	bool GetIsModuleOwned() const { return M_ModuleContext.ModuleId != INDEX_NONE; }

protected:
	/** @brief Native reaction to a state change within the same behaviour class; default forwards to Blueprint. */
	virtual void OnModuleContextUpdated(const FVehicleModuleBehaviourContext& PreviousContext);

	UFUNCTION(BlueprintImplementableEvent, Category="Vehicle Modules")
	void BP_OnModuleContextUpdated(const FVehicleModuleBehaviourContext& NewContext);

	UFUNCTION(BlueprintPure, Category="Vehicle Modules")
	FVehicleModuleBehaviourContext GetVehicleModuleContext() const { return M_ModuleContext; }

	UFUNCTION(BlueprintPure, Category="Vehicle Modules")
	UMeshComponent* GetBoundMesh() const;

	UArmorCalculation* GetModuleArmorCalculation() const;

private:
	UPROPERTY()
	FVehicleModuleBehaviourContext M_ModuleContext;
};
