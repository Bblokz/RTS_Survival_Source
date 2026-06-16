#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "GlobalAbilitySetDataAsset.generated.h"

UCLASS(BlueprintType)
class RTS_SURVIVAL_API UGlobalAbilitySetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	const TArray<TObjectPtr<UGlobalAbility>>& GetAbilityTemplates() const;

private:
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Global Abilities")
	TArray<TObjectPtr<UGlobalAbility>> M_AbilityTemplates;
};
