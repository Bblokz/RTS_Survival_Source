// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Behaviours/Derived/BehaviourWeapon/BehaviourWeapon.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"

#include "BehWeaponRemoveAbilities.generated.h"

class ICommands;

USTRUCT()
struct FBehWeaponRemovedAbility
{
	GENERATED_BODY()

	UPROPERTY()
	FUnitAbilityEntry AbilityEntry;

	UPROPERTY()
	int32 AbilityIndex = INDEX_NONE;
};

/**
 * @brief Weapon behaviour used to temporarily strip command abilities from the owning unit.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API UBehWeaponRemoveAbilities : public UBehaviourWeapon
{
	GENERATED_BODY()

public:
	UBehWeaponRemoveAbilities();

protected:
	virtual void OnAdded(AActor* BehaviourOwner) override;
	virtual void OnRemoved(AActor* BehaviourOwner) override;

private:
	void CacheRemovedAbilities(ICommands* CommandsInterface);
	void RestoreRemovedAbilities(ICommands* CommandsInterface);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour|Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<EAbilityID> M_AbilitiesToRemove;

	// Stores removed ability entries so they can be restored at their original indices.
	UPROPERTY()
	TArray<FBehWeaponRemovedAbility> M_RemovedAbilityEntries;
};
