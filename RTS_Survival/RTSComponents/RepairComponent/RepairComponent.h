// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "RepairState.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RepairComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URepairComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	URepairComponent();

	/**
	 * @brief Sets the owner squad unit.
	 * @param OwnerUnit Pointer to the squad unit owner.
	 */
	void SetSquadOwner(ASquadUnit* OwnerUnit);

	void ExecuteRepair(AActor* RepairableActor,
		const FVector& RepairOffset);
	
	void TerminateRepair();

	void OnFinishedMovementForRepairAbility();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// The equipment that the Repair Unit uses.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Repair")
	TObjectPtr<UStaticMesh> RepairEquipment;

	// The socket to which the Repair Equipment can be attached.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Repair")
	FName RepairEquipmentSocketName;

	// The radius within which the repair unit can accept repair requests.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Repair")
	float RepairAcceptanceRadius = 300.0f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Repair|Stats")
	float RepairMlt = 1.0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UNiagaraSystem> RepairEffect;

	// The name of the socket on the equipment mesh to attach the effect to.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Effect")
	FName RepairEffectSocketName = "RepairEffectSocket";

private:
	ERepairState M_RepairState = ERepairState::None;

	UPROPERTY()
	TObjectPtr<AActor> M_RepairTarget = nullptr;

	UPROPERTY()
	FVector M_RepairOffset = FVector::ZeroVector;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_RepairEquipment = nullptr;

	// The spawned Niagara effect component for repair
    UPROPERTY()
    TObjectPtr<UNiagaraComponent> M_RepairEffectComponent = nullptr;
    
    // Adds the repair equipment mesh and effect, hides the unit's weapon
    void AddRepairEquipment();

	void PlayRepairAnimation() const;
	void StopRepairAnimation() const;
    
    // Removes the repair equipment mesh and effect, shows the unit's weapon
    void RemoveRepairEquipment();

	
	UPROPERTY()
	TObjectPtr<ASquadUnit> M_OwnerSquadUnit = nullptr;

	void MoveToRepairableActor_SetStatus(AActor* RepairableActor);
	void MoveToRepairLocation_SetStatus();

	// Checks whether the owner squad unit pointer is valid.
	bool GetIsValidOwnerSquadUnit() const;

	void OnRepairUnitNotValidForRepairs() const;
	FString CheckRepairOffsetValidText() const;
	float EnsureValidAcceptanceRadius();
	FVector GetRepairLocation(bool& OutValidPosition)const;

	void StartRepairTarget();

	FTimerHandle M_RepairTickHandle;
	void RepairTick();

	bool GetIsRepairTargetInRange() const;

	void OnRepairTargetOutOfRange();

	void OnRepairTargetFullyRepaired();
	void StopRepairTimer();

	void SetSquadUnitWeaponDisabled(const bool bDisabled) const;
		
	
};
