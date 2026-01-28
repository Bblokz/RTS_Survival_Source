#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Scalability.h"

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
 * @brief Stores and persists runtime settings so the escape menu can restore user choices every session.
 *
 * Designers read and tweak the values through the settings UI while this class keeps them synced to GameUserSettings.ini.
 */
UCLASS(config=GameUserSettings)
class RTS_SURVIVAL_API URTSGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	/**
	 * @brief Pulls settings from config and normalizes them before the UI reads them.
	 * @param bForceReload Forces a fresh read from config instead of cached values.
	 */
	virtual void LoadSettings(bool bForceReload = false) override;

	/**
	 * @brief Applies runtime values so subsystems can safely consume them during play.
	 * @param bCheckForCommandLineOverrides Honors command line overrides when applying values.
	 */
	virtual void ApplySettings(bool bCheckForCommandLineOverrides) override;

	/**
	 * @brief Saves the current values so menu changes persist between sessions.
	 */
	virtual void SaveSettings() override;

	/** @brief Returns the volume used for the master audio channel in the settings menu. */
	float GetMasterVolume() const;

	/** @brief Returns the volume used for the music audio channel in the settings menu. */
	float GetMusicVolume() const;

	/** @brief Returns the volume used for the SFX audio channel in the settings menu. */
	float GetSfxAndWeaponsVolume() const;

	/** @brief Returns the volume used for the voicelines audio channel in the settings menu. */
	float GetVoicelinesVolume() const;

	/** @brief Returns the volume used for the announcer audio channel in the settings menu. */
	float GetAnnouncerVolume() const;

	/** @brief Returns the volume used for the UI audio channel in the settings menu. */
	float GetUiVolume() const;

	/** @brief Returns the mouse sensitivity multiplier applied by the settings menu. */
	float GetMouseSensitivity() const;

	/** @brief Returns whether the settings menu should invert the vertical look direction. */
	bool GetInvertYAxis() const;

	/** @brief Writes the master volume value before clamping and saving. */
	void SetMasterVolume(const float NewMasterVolume);

	/** @brief Writes the music volume value before clamping and saving. */
	void SetMusicVolume(const float NewMusicVolume);

	/** @brief Writes the SFX volume value before clamping and saving. */
	void SetSfxAndWeaponsVolume(const float NewSfxAndWeaponsVolume);

	/** @brief Writes the voicelines volume value before clamping and saving. */
	void SetVoicelinesVolume(const float NewVoicelinesVolume);

	/** @brief Writes the announcer volume value before clamping and saving. */
	void SetAnnouncerVolume(const float NewAnnouncerVolume);

	/** @brief Writes the UI volume value before clamping and saving. */
	void SetUiVolume(const float NewUiVolume);

	/** @brief Writes the mouse sensitivity multiplier before clamping and saving. */
	void SetMouseSensitivity(const float NewMouseSensitivity);

	/** @brief Writes the invert setting so input code can flip vertical control direction. */
	void SetInvertYAxis(const bool bNewInvertYAxis);

	/**
	 * @brief Returns the current scalability group values so the settings menu can mirror the live engine state.
	 */
	Scalability::FQualityLevels GetQualityLevels() const;

	/**
	 * @brief Pushes new scalability values so the engine applies group-level quality changes.
	 * @param NewQualityLevels Group quality levels built from the menu selection.
	 */
	void SetQualityLevels(const Scalability::FQualityLevels& NewQualityLevels);

private:
	void ApplyCustomSettingClamps();

	UPROPERTY(config)
	float M_MasterVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_MusicVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_SfxAndWeaponsVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_VoicelinesVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_AnnouncerVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_UiVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(config)
	float M_MouseSensitivity = RTSGameUserSettingsRanges::DefaultMouseSensitivity;

	UPROPERTY(config)
	bool bM_InvertYAxis = false;
};
