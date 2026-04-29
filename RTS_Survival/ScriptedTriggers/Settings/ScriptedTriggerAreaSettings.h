#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ScriptedTriggerAreaSettings.generated.h"

class ATriggerArea;

/**
 * @brief Designer-configured trigger area classes used by standalone scripted trigger actors.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Scripted Trigger Areas"))
class RTS_SURVIVAL_API UScriptedTriggerAreaSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UScriptedTriggerAreaSettings();

	UPROPERTY(EditAnywhere, Config, Category="ScriptedTrigger|Trigger Areas")
	TSubclassOf<ATriggerArea> M_RectangleTriggerAreaClass;

	UPROPERTY(EditAnywhere, Config, Category="ScriptedTrigger|Trigger Areas")
	TSubclassOf<ATriggerArea> M_SphereTriggerAreaClass;
};
