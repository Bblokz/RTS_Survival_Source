// RTSAudioSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/Audio/Settings/RTSAudioType.h"

#include "RTSAudioDeveloperSettings.generated.h"

class USoundClass;
class USoundMix;

/**
 * @brief Central developer settings asset that drives runtime audio mix routing for the settings menu.
 *
 * @details Designers must assign the settings sound mix and every sound class entry in the map
 * before play so the runtime menu can safely apply volume overrides without null references.
 * User volume choices are saved through URTSGameUserSettings, then URTSSettingsMenuSubsystem
 * reloads them and applies overrides per ERTSAudioType using the sound mix and mapped classes.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Audio Settings"))
class RTS_SURVIVAL_API URTSAudioDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSAudioDeveloperSettings();

	/** Sound mix that receives class overrides for all menu-driven volume changes. */
	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TSoftObjectPtr<USoundMix> UserSettingsSoundMix;

	/** Sound classes mapped by channel type so the settings menu can target each class. */
	UPROPERTY(Config, EditAnywhere, Category="Audio")
	TMap<ERTSAudioType, TSoftObjectPtr<USoundClass>> SoundClassesByType;
};
