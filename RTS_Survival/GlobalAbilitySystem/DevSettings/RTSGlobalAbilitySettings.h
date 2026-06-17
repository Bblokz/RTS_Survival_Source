#pragma once

#include "CoreMinimal.h"
#include "GlobalAbilitySetDataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "RTSGlobalAbilitySettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "GlobalAbilitySettings"))
class RTS_SURVIVAL_API URTSGlobalAbilitySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSGlobalAbilitySettings();
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category="Data")
	TSoftObjectPtr<UGlobalAbilitySetDataAsset> GlobalAbilityDataAsset;

};
