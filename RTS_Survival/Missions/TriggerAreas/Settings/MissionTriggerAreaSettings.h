#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MissionTriggerAreaSettings.generated.h"

class ATriggerArea;

/**
 * @brief Designer-configured trigger area classes used by mission-driven volume spawning.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Mission Trigger Areas"))
class RTS_SURVIVAL_API UMissionTriggerAreaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMissionTriggerAreaSettings();

	UPROPERTY(EditAnywhere, Config, Category="Mission|Trigger Areas")
	TSubclassOf<ATriggerArea> M_RectangleTriggerAreaClass;

	UPROPERTY(EditAnywhere, Config, Category="Mission|Trigger Areas")
	TSubclassOf<ATriggerArea> M_SphereTriggerAreaClass;
};
