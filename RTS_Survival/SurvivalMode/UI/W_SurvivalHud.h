// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/SurvivalMode/Core/SurvivalModeTypes.h"

#include "W_SurvivalHud.generated.h"

class ASurvivalModeDirector;
class UBorder;
class UButton;
class USurvivalUiStyleAsset;
class UTextBlock;
class UWidget;

/**
 * @brief In-match survival overlay: grace countdown, survival clock, wave counter, next-wave warning and the
 * defeat result (time survived, waves, best time). Polls the director four times a second; the root never takes
 * the mouse except the defeat panel. The Blueprint child holds the layout (Docs/ui_specs/W_SurvivalHud.json).
 * @note InitHud: called by the director when deployment starts.
 * @note ShowDefeat: called by the director on defeat.
 * @note M_StyleAsset: set to DA_SurvivalUiStyle in the Blueprint class defaults.
 *
 * Bound widgets (names in the Blueprint must match):
 *   required: M_TimerText (TextBlock), M_DefeatPanel (any widget, collapsed until defeat),
 *             M_RetryButton, M_MainMenuButton (Button)
 *   optional: M_StatusPlate, M_BannerPlate, M_ResultPanel (Border); M_StatusPanel, M_BannerPanel (any widget);
 *             M_PhaseText, M_WaveText, M_BannerTitleText, M_BannerText, M_DefeatTitleText, M_TimeSurvivedLabel,
 *             M_TimeSurvivedText, M_WavesLabel, M_WavesText, M_BestTimeLabel, M_BestTimeText, M_NewRecordText,
 *             M_RetryText, M_MainMenuText (TextBlock)
 */
UCLASS()
class RTS_SURVIVAL_API UW_SurvivalHud : public UUserWidget
{
	GENERATED_BODY()

public:
	/** @brief Called by the director when deployment starts; the HUD polls the director for its values. */
	virtual void InitHud(ASurvivalModeDirector* Director);

	/** @brief Called by the director on defeat. */
	virtual void ShowDefeat(float SecondsSurvived, int32 WavesLaunched, float BestSeconds, bool bIsNewBest);

	/**
	 * @brief The wave director calls this when it launches (or is about to launch) a wave, e.g.
	 * Director->GetHudWidget()->ShowIncomingWaveBanner(WaveIndex + 1, Zone->GetDisplayName(), 0.f).
	 * @param WaveNumber 1-based wave number shown in the title.
	 * @param EdgeLabel Where the wave comes from ("North-East", "river crossing"; shown as "from the {0}");
	 * empty shows no direction.
	 * Use GetCompassDirectionText when the edge zone has no name.
	 * @param DisplaySeconds How long the banner stays; 0 or less uses the default (6 s).
	 */
	UFUNCTION(BlueprintCallable, Category = "Survival|HUD")
	void ShowIncomingWaveBanner(int32 WaveNumber, const FText& EdgeLabel, float DisplaySeconds = 0.f);

	UFUNCTION(BlueprintCallable, Category = "Survival|HUD")
	void HideIncomingWaveBanner();

	/**
	 * @brief Eight-way compass word for the banner: where the wave enters, seen from the player's base.
	 * @param WaveEntryLocation Where the wave spawns (the edge zone).
	 * @param BaseLocation ASurvivalModeDirector::GetCurrentPlayerBaseLocation().
	 * @return "North", "North-East", ...; +X is north.
	 */
	UFUNCTION(BlueprintPure, Category = "Survival|HUD")
	static FText GetCompassDirectionText(const FVector& WaveEntryLocation, const FVector& BaseLocation);

	/** @brief Reloads the survival map; the game instance keeps faction, variants and difficulty. */
	UFUNCTION(BlueprintCallable, Category = "Survival|HUD")
	void RetrySurvival();

	/** @brief Opens the project's default map, which leads to the main menu (as the escape menu's exit does). */
	UFUNCTION(BlueprintCallable, Category = "Survival|HUD")
	void ReturnToMainMenu();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Style")
	TObjectPtr<USurvivalUiStyleAsset> M_StyleAsset = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> M_TimerText = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> M_DefeatPanel = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_RetryButton = nullptr;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> M_MainMenuButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> M_StatusPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_StatusPlate = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_PhaseText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_WaveText = nullptr;

	// "12 ENEMIES REMAINING" during a wave; created next to M_WaveText when the Blueprint has none.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_EnemiesText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> M_BannerPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_BannerPlate = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_BannerTitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_BannerText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> M_ResultPanel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_DefeatTitleText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_TimeSurvivedLabel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_TimeSurvivedText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_WavesLabel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_WavesText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_BestTimeLabel = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_BestTimeText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_NewRecordText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_RetryText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> M_MainMenuText = nullptr;

private:
	TWeakObjectPtr<ASurvivalModeDirector> M_Director;

	FDelegateHandle M_PhaseChangedHandle;
	FTimerHandle M_RefreshTimerHandle;
	FTimerHandle M_BannerTimerHandle;
	bool bM_IsDefeatShown = false;

	void ApplyStyleToBoundWidgets();
	void EnsureEnemiesText();
	void ApplyStaticTexts();
	void BindButtons();
	void StartRefreshTimer();
	void RefreshStatus();
	void RefreshStatusForPhase(const ASurvivalModeDirector& Director, const ESurvivalPhase Phase);
	void OnPhaseChanged(ESurvivalPhase OldPhase, ESurvivalPhase NewPhase);
	void SetDefeatInputMode();

	UFUNCTION()
	void OnRetryClicked();

	UFUNCTION()
	void OnMainMenuClicked();

	bool GetIsValidDirector() const;
};
