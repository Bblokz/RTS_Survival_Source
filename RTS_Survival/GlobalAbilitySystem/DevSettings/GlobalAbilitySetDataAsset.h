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
	TObjectPtr<UObject> GetGLobalAbilityForTypeAsCopyFromTemplate(const EGlobalAbility Type);

private:
	// Do not use directly; always copy uobjects on it.
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Global Abilities")
	TMap<EGlobalAbility, TObjectPtr<UGlobalAbility>> M_AbilityTemplatesByType;
};
