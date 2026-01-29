#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Scalability.h"
#include "RTS_Survival/Game/UserSettings/GameplaySettings/HealthbarVisibilityStrategy/HealthBarVisibilityStrategy.h"

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
	 * @brief Returns the live RTS user settings singleton currently used by the engine.
	 * This does not force a disk reload; it returns the in-memory instance.
	 */
	static const URTSGameUserSettings* Get();

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

	/** @brief Returns the saved player health bar visibility override. */
	ERTSPlayerHealthBarVisibilityStrategy GetOverwriteAllPlayerHpBarStrat() const;

	/** @brief Returns the saved player tank health bar visibility strategy. */
	ERTSPlayerHealthBarVisibilityStrategy GetPlayerTankHpBarStrat() const;

	/** @brief Returns the saved player squad health bar visibility strategy. */
	ERTSPlayerHealthBarVisibilityStrategy GetPlayerSquadHpBarStrat() const;

	/** @brief Returns the saved player nomadic health bar visibility strategy. */
	ERTSPlayerHealthBarVisibilityStrategy GetPlayerNomadicHpBarStrat() const;

	/** @brief Returns the saved player building expansion health bar visibility strategy. */
	ERTSPlayerHealthBarVisibilityStrategy GetPlayerBxpHpBarStrat() const;

	/** @brief Returns the saved player aircraft health bar visibility strategy. */
	ERTSPlayerHealthBarVisibilityStrategy GetPlayerAircraftHpBarStrat() const;

	/** @brief Returns the saved enemy health bar visibility override. */
	ERTSEnemyHealthBarVisibilityStrategy GetOverwriteAllEnemyHpBarStrat() const;

	/** @brief Returns the saved enemy tank health bar visibility strategy. */
	ERTSEnemyHealthBarVisibilityStrategy GetEnemyTankHpBarStrat() const;

	/** @brief Returns the saved enemy squad health bar visibility strategy. */
	ERTSEnemyHealthBarVisibilityStrategy GetEnemySquadHpBarStrat() const;

	/** @brief Returns the saved enemy nomadic health bar visibility strategy. */
	ERTSEnemyHealthBarVisibilityStrategy GetEnemyNomadicHpBarStrat() const;

	/** @brief Returns the saved enemy building expansion health bar visibility strategy. */
	ERTSEnemyHealthBarVisibilityStrategy GetEnemyBxpHpBarStrat() const;

	/** @brief Returns the saved enemy aircraft health bar visibility strategy. */
	ERTSEnemyHealthBarVisibilityStrategy GetEnemyAircraftHpBarStrat() const;

	/** @brief Stores the player health bar visibility override. */
	void SetOverwriteAllPlayerHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the player tank health bar visibility strategy. */
	void SetPlayerTankHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the player squad health bar visibility strategy. */
	void SetPlayerSquadHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the player nomadic health bar visibility strategy. */
	void SetPlayerNomadicHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the player building expansion health bar visibility strategy. */
	void SetPlayerBxpHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the player aircraft health bar visibility strategy. */
	void SetPlayerAircraftHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the enemy health bar visibility override. */
	void SetOverwriteAllEnemyHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the enemy tank health bar visibility strategy. */
	void SetEnemyTankHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the enemy squad health bar visibility strategy. */
	void SetEnemySquadHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the enemy nomadic health bar visibility strategy. */
	void SetEnemyNomadicHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the enemy building expansion health bar visibility strategy. */
	void SetEnemyBxpHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	/** @brief Stores the enemy aircraft health bar visibility strategy. */
	void SetEnemyAircraftHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

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

	UPROPERTY(config)
	ERTSPlayerHealthBarVisibilityStrategy M_OverwriteAllPlayerHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::NotInitialized;

	UPROPERTY(config)
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerTankHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerSquadHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerNomadicHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerBxpHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerAircraftHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSEnemyHealthBarVisibilityStrategy M_OverwriteAllEnemyHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::NotInitialized;

	UPROPERTY(config)
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyTankHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSEnemyHealthBarVisibilityStrategy M_EnemySquadHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyNomadicHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyBxpHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(config)
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyAircraftHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;
};
