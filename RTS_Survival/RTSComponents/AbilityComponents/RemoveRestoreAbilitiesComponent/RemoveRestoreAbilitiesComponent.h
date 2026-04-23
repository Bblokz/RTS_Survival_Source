// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "RemoveRestoreAbilitiesComponent.generated.h"

class ICommands;

USTRUCT(BlueprintType)
struct FRemoveRestoreCachedAbility
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FUnitAbilityEntry AbilityEntry;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RemovedFromIndex = INDEX_NONE;
};

/**
 * @brief Removes configured abilities from the owning ICommands actor on BeginPlay and caches them for later restoration.
 * @note RestoreAbility: call in blueprint to restore one cached ability by id.
 * @note RestoreAllCachedAbilities: call in blueprint to restore all currently cached abilities.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API URemoveRestoreAbilitiesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URemoveRestoreAbilitiesComponent();

	UFUNCTION(BlueprintCallable)
	void RestoreAbility(EAbilityID AbilityIdToRestore);

	UFUNCTION(BlueprintCallable)
	void RestoreAllCachedAbilities();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TArray<EAbilityID> M_AbilitiesToRemoveOnBeginPlay;

	// We cache the full removed entry together with its original slot to keep subtype and UI order intact when restoring.
	UPROPERTY()
	TArray<FRemoveRestoreCachedAbility> M_CachedRemovedAbilities;

	UPROPERTY()
	TScriptInterface<ICommands> M_OwnerCommandsInterface;

	bool GetIsValidOwnerCommandsInterface() const;
	void BeginPlay_InitOwnerCommandsInterface();
	void BeginPlay_RemoveConfiguredAbilities();
	bool RemoveAndCacheAbility(const EAbilityID AbilityToRemove);
	bool TryGetOwnerAbilityEntry(const EAbilityID AbilityToFind, FUnitAbilityEntry& OutAbilityEntry,
		int32& OutAbilityIndex) const;
	int32 GetCachedAbilityIndex(const EAbilityID AbilityIdToFind) const;
	bool RestoreCachedAbilityByCacheIndex(const int32 CachedAbilityIndex);
};
