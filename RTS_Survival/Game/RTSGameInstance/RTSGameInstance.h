#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/Audio/Settings/RTSAudioType.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTSGameInstance.generated.h"


enum class ERTSFaction : uint8;
class URTSMusicManager;
class USoundClass;
class USoundMix;

/**
 * @brief Game instance that wires startup systems and exposes global managers to Blueprint.
 *
 * @note On Faction and Campaign generation.
 * the AFactionPlayerController::HandleWorldGenerationSettingsGenerated handles providing the :
 * @note - FCampaignGenerationSettings after interaction with last widget
 * @note - ERTSFaction selected by the player
 * @note - FRTSGameDifficulty selected by the player
 * @note at the begin play of the world player controller these settings are retrieved from the game instance and
 * provided to the world generator.
 *
 * Handles early audio setup and keeps persistent managers alive between map transitions.
 * @note SetupMusicManager: implement in Blueprint to finish music manager setup.
 */
UCLASS()
class RTS_SURVIVAL_API URTSGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    /** Blueprint can grab this and call InitMusicManager(...) on it. */
    UFUNCTION(BlueprintCallable, Category="Music")
    URTSMusicManager* GetMusicManager() const { return MusicManager; }

	void SetCampaignGenerationSettings(const FCampaignGenerationSettings& Settings);
	FCampaignGenerationSettings GetCampaignGenerationSettings() const;
	void SetSelectedGameDifficulty(const FRTSGameDifficulty& Difficulty) { M_SelectedGameDifficulty = Difficulty; }
	FRTSGameDifficulty GetSelectedGameDifficulty() const { return M_SelectedGameDifficulty; }
	void SetPlayerFaction(const ERTSFaction Faction) { M_PlayerFaction = Faction; }
	ERTSFaction GetPlayerFaction() const { return M_PlayerFaction; }

protected:
    // Called on init when the music manager is setup and needs to be initialized.
    UFUNCTION(BlueprintImplementableEvent, Category="Music")
    void SetupMusicManager();

    virtual void Shutdown() override;

private:
	/** Our persistent music player. */
	UPROPERTY()
	URTSMusicManager* MusicManager;

	/** @brief Loads developer audio settings and pushes the configured sound mix for runtime volume control. */
	void InitializeAudioSettings();

	/**
	 * @brief Ensures every audio type maps to an adjuster so per-channel volumes can be applied safely.
	 * @param SettingsSoundMix Sound mix that should include adjusters for every configured sound class.
	 * @param SoundClassesByType Map of audio type entries supplied by RTSAudioSettings.
	 */
	void EnsureSoundMixHasAudioClasses(
		USoundMix* SettingsSoundMix,
		const TMap<ERTSAudioType, TSoftObjectPtr<USoundClass>>& SoundClassesByType);

    void BeginLoadingScreen(const FString& MapName);
    void EndLoadingScreen(UWorld* LoadedWorld);

    void StopLoadingScreen();

    FTimerHandle LoadingScreenTimerHandle;

    // This is the map we start at, do not wait the extra seconds.
    bool bIsLoadingProfileMap;

	FCampaignGenerationSettings M_CampaignGenerationSettings;
	FRTSGameDifficulty M_SelectedGameDifficulty;
	ERTSFaction M_PlayerFaction;

};
