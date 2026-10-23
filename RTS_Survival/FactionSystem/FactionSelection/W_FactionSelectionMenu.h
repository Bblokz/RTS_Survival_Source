// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "W_FactionSelectionMenu.generated.h"

class AFactionPlayerController;
class UButton;
class USoundBase;
struct FFactionUnitBrowserGroup;
struct FSlateBrush;
class UW_FactionUnitBrowser;
class UW_Portrait;

USTRUCT(BlueprintType)
struct FFactionMenuTankOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;
};

USTRUCT(BlueprintType)
struct FFactionMenuSquadOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;
};

USTRUCT(BlueprintType)
struct FFactionMenuAircraftOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAircraftSubtype AircraftSubtype = EAircraftSubtype::Aircarft_None;
};

USTRUCT(BlueprintType)
struct FFactionMenuBuildingExpansionOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBuildingExpansionType BuildingExpansionType = EBuildingExpansionType::BXT_Invalid;
};

USTRUCT(BlueprintType)
struct FFactionMenuSetting
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Armored Cars")
	TArray<FFactionMenuTankOption> ArmoredCars;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Light")
	TArray<FFactionMenuTankOption> LightTanks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Medium")
	TArray<FFactionMenuTankOption> MediumTanks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Heavy")
	TArray<FFactionMenuTankOption> HeavyTanks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Infantry")
	TArray<FFactionMenuSquadOption> Infantry;

	// Team weapon crews (anti-tank guns, howitzers, flak, the Mörser); shown under their own filter.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Support Weapons")
	TArray<FFactionMenuSquadOption> SupportWeapons;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Aircraft")
	TArray<FFactionMenuAircraftOption> Aircraft;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Faction Selection|Training|Building Expansion")
	TArray<FFactionMenuBuildingExpansionOption> BuildingExpansions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> FactionDescription = nullptr;
};

/**
 * @brief Widget used to select a faction and preview its units before launching the campaign.
 * Creates the unit browser (M_UnitBrowserClass) on its canvas and fills it with the selected faction's units.
 * @note M_UnitBrowserClass: set to BP_W_FactionUnitBrowser in the class defaults.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionSelectionMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetFactionPlayerController(AFactionPlayerController* FactionPlayerController);
	void HandleUnitSelected(const FTrainingOption& TrainingOption);
	void HandleAnnouncementFinished();

	/** @return Every unit the faction's menu setting lists, in list order; the variant selection filters these. */
	TArray<FTrainingOption> GetFactionTrainingOptions(const ERTSFaction Faction) const;

	/** Selects a faction without forcing its announcement, e.g. when the player comes back from variant selection. */
	void ShowFaction(const ERTSFaction Faction);

	/** Test access for the faction menu capture. */
	UW_FactionUnitBrowser* GetUnitBrowser() const { return M_UnitBrowser; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UButton* M_GerBreakthrough = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* M_GerBreakthroughAudio = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* M_GerThermoKorps = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* M_GerThermoKorpsAudio = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* M_GerItalianGame = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* M_GerItalianGameAudio = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* M_LaunchCampaign = nullptr;

	UPROPERTY(meta = (BindWidget))
	UW_Portrait* M_Portrait = nullptr;

	// Built in code and placed at the bottom centre of the root canvas.
	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSubclassOf<UW_FactionUnitBrowser> M_UnitBrowserClass;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FFactionMenuSetting M_GerBreakthroughSetting;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FFactionMenuSetting M_GerThermoKorpsSetting;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FFactionMenuSetting M_GerItalianGameSetting;

private:
	UPROPERTY()
	TWeakObjectPtr<AFactionPlayerController> M_FactionPlayerController;
	void SetupFactionPlayerController();

	UPROPERTY()
	TObjectPtr<UW_FactionUnitBrowser> M_UnitBrowser;
	void SetupUnitBrowser();

	ERTSFaction M_SelectedFaction = ERTSFaction::GerBreakthroughDoctrine;

	bool bM_HasPlayedGerBreakthroughAudio = false;
	bool bM_HasPlayedGerThermoKorpsAudio = false;
	bool bM_HasPlayedGerItalianGameAudio = false;

	UFUNCTION()
	void HandleGerBreakthroughClicked();

	UFUNCTION()
	void HandleGerBreakthroughAudioClicked();

	UFUNCTION()
	void HandleGerThermoKorpsClicked();

	UFUNCTION()
	void HandleGerThermoKorpsAudioClicked();

	UFUNCTION()
	void HandleGerItalianGameClicked();

	UFUNCTION()
	void HandleGerItalianGameAudioClicked();

	UFUNCTION()
	void HandleLaunchCampaignClicked();

	void SetupButtonBindings();
	void ApplyFactionAvailability();
	void SelectGermanBreakthrough(const bool bForceAudio);
	void SelectGermanThermoKorps(const bool bForceAudio);
	void SelectGermanItalianGame(const bool bForceAudio);
	/**
	 * @brief Applies a faction setting while handling audio playback rules and preview list refresh.
	 * @param FactionSetting The setting data used to populate the menu.
	 * @param bForceAudio Whether to force audio playback for this selection.
	 * @param bHasPlayedAudio Tracks whether this faction has already played its announcer audio.
	 */
	void SelectFaction(const FFactionMenuSetting& FactionSetting, const bool bForceAudio, bool& bHasPlayedAudio);
	void UpdateFactionSelectionVisuals();
	/**
	 * @brief Keeps the chosen faction visible after hover ends, including selections from the audio buttons.
	 * @param FactionButton Validated faction button whose skin receives the selection glint.
	 * @param GlassWidgetName Matching glass image in the widget tree, layered beneath the label.
	 * @param bIsSelected Whether this button represents the current faction.
	 */
	void UpdateFactionButtonSelection(UButton* FactionButton, const FName GlassWidgetName,
	                                  const bool bIsSelected);
	/**
	 * @brief Gives each button its own selection glint without changing shared skin instances or hover sweeps.
	 * @param Brush Skinned button brush that retains its dynamic material between selections.
	 * @param bIsSelected Whether to enable the persistent selection glint.
	 */
	void UpdateFactionSelectionBrush(FSlateBrush& Brush, const bool bIsSelected);
	void PopulateFactionUnits(const FFactionMenuSetting& FactionSetting);
	const FFactionMenuSetting* FindFactionSetting(const ERTSFaction Faction) const;
	/** @return The browser tabs in order: ALL first, then each unit class; the browser hides empty ones. */
	static TArray<FFactionUnitBrowserGroup> BuildUnitBrowserGroups(const FFactionMenuSetting& FactionSetting);
	static TArray<FTrainingOption> ToTrainingOptions(const TArray<FFactionMenuTankOption>& TankOptions);
	static TArray<FTrainingOption> ToTrainingOptions(const TArray<FFactionMenuSquadOption>& SquadOptions);
	static TArray<FTrainingOption> ToTrainingOptions(const TArray<FFactionMenuAircraftOption>& AircraftOptions);
	static TArray<FTrainingOption> ToTrainingOptions(
		const TArray<FFactionMenuBuildingExpansionOption>& BuildingExpansionOptions);
	void PlayFactionAudio(const FFactionMenuSetting& FactionSetting);
	void UpdatePortraitForAnnouncement();
	void HidePortrait();
	void LaunchCampaign();

	bool GetIsValidPlayerController() const;
	bool GetIsValidUnitBrowser() const;
	bool GetIsValidPortrait() const;
	bool GetIsValidGermanBreakthroughButton() const;
	bool GetIsValidGermanBreakthroughAudioButton() const;
	bool GetIsValidGermanThermoKorpsButton() const;
	bool GetIsValidGermanThermoKorpsAudioButton() const;
	bool GetIsValidGermanItalianGameButton() const;
	bool GetIsValidGermanItalianGameAudioButton() const;
	bool GetIsValidLaunchCampaignButton() const;
};
