#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/UnitData/UnitAbilityEntry.h"
#include "TowedActor.generated.h"

class UVehicleTowComponent;

USTRUCT(BlueprintType)
struct FTowedActorSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FVector AttachOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FRotator AttachRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<EAbilityID> AbilitiesToRemoveWhileTowed = {EAbilityID::IdAttack, EAbilityID::IdMove};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 PreferredDetachAbilityIndex = 0;
};

USTRUCT()
struct FTowedActorRemovedAbilityCache
{
	GENERATED_BODY()

	UPROPERTY()
	int32 M_AbilityIndex = INDEX_NONE;

	UPROPERTY()
	FUnitAbilityEntry M_AbilityEntry;
};

/**
 * @brief Stores tow attach settings and runtime towing state for towable actors.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTowedActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTowedActorComponent();

	bool IsTowFree() const;
	bool GetIsValidTowingActor() const;
	bool GetIsValidTowingVehicleTowComp() const;

	void SetTowRelationship(AActor* TowingActor, UVehicleTowComponent* TowingVehicleTowComp);
	void ClearTowRelationship();

	const FTowedActorSettings& GetTowedSettings() const { return M_TowedSettings; }
	AActor* GetTowingActor() const;
	UVehicleTowComponent* GetTowingVehicleTowComp() const;

	void RemoveAbilitiesWhileTowed();
	void RestoreAbilitiesAfterTow();

private:
	bool GetIsValidCommandsOwner() const;

	UPROPERTY(EditDefaultsOnly, Category="Tow")
	FTowedActorSettings M_TowedSettings;

	UPROPERTY()
	TArray<FTowedActorRemovedAbilityCache> M_CachedRemovedAbilities;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_TowingActor;

	UPROPERTY()
	TWeakObjectPtr<UVehicleTowComponent> M_TowingVehicleTowComp;
};
