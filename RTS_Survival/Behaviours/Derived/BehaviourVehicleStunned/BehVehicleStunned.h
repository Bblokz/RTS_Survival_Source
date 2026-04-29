// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Behaviour.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"

#include "BehVehicleStunned.generated.h"

class ICommands;
class ATankMaster;
class ACPPTurretsMaster;

USTRUCT()
struct FBehVehicleStunnedRemovedAbility
{
	GENERATED_BODY()

	UPROPERTY()
	FUnitAbilityEntry AbilityEntry;

	UPROPERTY()
	int32 AbilityIndex = INDEX_NONE;
};

/**
 * @brief Temporarily disables core vehicle combat/mobility to model a stun state.
 * Apply this behaviour on tanks to stop movement, disable mounted weapons,
 * reduce turret rotation speed, and remove movement-related command card abilities.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehVehicleStunned : public UBehaviour
{
	GENERATED_BODY()

public:
	UBehVehicleStunned();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;

private:
	void CacheOwnerReferences(AActor* BehaviourOwner);
	void SetOwnerToIdleAndStopMovement() const;
	void ApplyTurretRotationPenalty();
	void RestoreTurretRotation();
	void DisableMountedWeapons() const;
	void EnableMountedWeapons() const;
	void CacheAndRemoveAbilities();
	void RestoreRemovedAbilities();
	void ResetCachedState();
	ICommands* GetCommandsInterface() const;
	bool GetIsValidTankMaster() const;
	bool GetIsValidCommandsOwnerActor() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour|VehicleStunned", meta = (AllowPrivateAccess = "true"))
	float M_TurretRotationSpeedMultiplier = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour|VehicleStunned", meta = (AllowPrivateAccess = "true"))
	TArray<EAbilityID> M_AbilitiesToRemove ;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_CommandsOwnerActor;

	// Caches turret base speed while stunned so all affected turrets restore exact original values.
	UPROPERTY()
	TMap<TWeakObjectPtr<ACPPTurretsMaster>, float> M_CachedTurretRotationSpeeds;

	// Stores removed command entries with original indices so command-card ordering is restored on stun end.
	UPROPERTY()
	TArray<FBehVehicleStunnedRemovedAbility> M_RemovedAbilityEntries;
};
