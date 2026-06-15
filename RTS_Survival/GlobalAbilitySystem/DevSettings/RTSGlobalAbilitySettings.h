#pragma once

#include "CoreMinimal.h"
#include "GlobalAbilitySetDataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTSGlobalAbilitySettings.generated.h"

UCLASS(Config = Game, DefaultConfig)
class RTS_SURVIVAL_API URTSGlobalAbilitySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	TSoftObjectPtr<URTSGlobalAbilitySettings> GetGlobalAbilityOfType(const EGlobalAbility Type);

private:
	// Do not use directly; always copy uobjects.
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Global Abilities")
	TMap<EGlobalAbility, TObjectPtr<UGlobalAbility>> M_AbilityTemplatesByType;
};
