#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"

#include "RTSGameUserSettings.generated.h"

namespace RTSGameUserSettingsRanges
{
	constexpr float MinVolume = 0.0f;
	constexpr float MaxVolume = 1.0f;
	constexpr float DefaultVolume = 1.0f;

	constexpr float MinMouseSensitivity = 0.1f;
	constexpr float MaxMouseSensitivity = 5.0f;
	constexpr float DefaultMouseSensitivity = 1.0f;
}

/**
 * @brief Stores runtime-changeable settings in GameUserSettings.ini so the settings menu can load and persist user preferences.
 *
 * This class extends UGameUserSettings with audio and controls data that can be applied without restarting the game.
 */
UCLASS(config=GameUserSettings)
class RTS_SURVIVAL_API URTSGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;
	virtual void SaveSettings() override;

	float GetMasterVolume() const;
	float GetMusicVolume() const;
	float GetSfxVolume() const;
	float GetMouseSensitivity() const;
	bool GetInvertYAxis() const;

	void SetMasterVolume(const float NewMasterVolume);
	void SetMusicVolume(const float NewMusicVolume);
	void SetSfxVolume(const float NewSfxVolume);
	void SetMouseSensitivity(const float NewMouseSensitivity);
	void SetInvertYAxis(const bool bNewInvertYAxis);

private:
	void ApplyCustomSettingClamps();

	UPROPERTY(config)
	float M_MasterVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_MusicVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_SfxVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_MouseSensitivity = RTSGameUserSettingsRanges::DefaultMouseSensitivity;

	UPROPERTY(config)
	bool bM_InvertYAxis = false;
};
