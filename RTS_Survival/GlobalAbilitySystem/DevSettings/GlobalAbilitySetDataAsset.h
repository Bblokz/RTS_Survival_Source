#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GlobalAbilitySetDataAsset.generated.h"

enum class EGlobalAbility : uint8;

UCLASS(BlueprintType)
class RTS_SURVIVAL_API UGlobalAbilitySetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	TObjectPtr<UGlobalAbility> GetGLobalAbilityForTypeAsCopyFromTemplate(const EGlobalAbility Type);

	/**
	 * @brief Read-only template lookup for UI that lists commander kits before every template exists: a missing
	 * template is a normal case here, so it reports no error. Never execute the template; copy it instead.
	 * @param Type Ability to look up.
	 * @return The template, or nullptr when the data asset has none for this type.
	 */
	const UGlobalAbility* FindAbilityTemplate(const EGlobalAbility Type) const;

private:
	// Do not use directly; always copy uobjects on it.
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Global Abilities")
	TMap<EGlobalAbility, TObjectPtr<UGlobalAbility>> M_AbilityTemplatesByType;
};
