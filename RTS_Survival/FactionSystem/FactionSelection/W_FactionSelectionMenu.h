// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "W_FactionSelectionMenu.generated.h"

class AFactionPlayerController;
class UButton;
class UScrollBox;
class USoundBase;
class UWorld;
class UW_FactionUnit;
class UW_Portrait;

USTRUCT(BlueprintType)
struct FFactionMenuTankOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	ETankSubtype TankSubtype = ETankSubtype::Tank_None;
};

USTRUCT(BlueprintType)
struct FFactionMenuSquadOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	ESquadSubtype SquadSubtype = ESquadSubtype::Squad_None;
};

USTRUCT(BlueprintType)
struct FFactionMenuAircraftOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EAircraftSubtype AircraftSubtype = EAircraftSubtype::Aircarft_None;
};

USTRUCT(BlueprintType)
struct FFactionMenuBuildingExpansionOption
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	EBuildingExpansionType BuildingExpansionType = EBuildingExpansionType::BXT_Invalid;
};

USTRUCT(BlueprintType)
struct FFactionMenuSetting
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Armored Cars")
	TArray<FFactionMenuTankOption> ArmoredCars;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Light")
	TArray<FFactionMenuTankOption> LightTanks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Medium")
	TArray<FFactionMenuTankOption> MediumTanks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Faction Selection|Training|Tanks|Heavy")
	TArray<FFactionMenuTankOption> HeavyTanks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Faction Selection|Training|Infantry")
	TArray<FFactionMenuSquadOption> Infantry;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Faction Selection|Training|Aircraft")
	TArray<FFactionMenuAircraftOption> Aircraft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Faction Selection|Training|Building Expansion")
	TArray<FFactionMenuBuildingExpansionOption> BuildingExpansions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bIsEnabled = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TObjectPtr<USoundBase> FactionDescription = nullptr;
};

USTRUCT()
struct FFactionSelectionUnitWidgets
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionUnit>> AllUnits;

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionUnit>> ArmoredCars;

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionUnit>> LightTanks;

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionUnit>> MediumTanks;

	UPROPERTY()
	TArray<TObjectPtr<UW_FactionUnit>> HeavyTanks;
};

/**
 * @brief Widget used to select a faction and preview its units before launching the campaign.
 */
UCLASS()
class RTS_SURVIVAL_API UW_FactionSelectionMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetFactionPlayerController(AFactionPlayerController* FactionPlayerController);
	void HandleUnitSelected(const FTrainingOption& TrainingOption);
	void HandleAnnouncementFinished();

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
	UScrollBox* M_ScrollBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UW_Portrait* M_Portrait = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SortArmoredCars = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SortLightTanks = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SortMediumTanks = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* SortHeavyTanks = nullptr;

	UPROPERTY(meta = (BindWidget))
	UButton* ShowAllUnits = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSubclassOf<UW_FactionUnit> M_FactionUnitWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FFactionMenuSetting M_GerBreakthroughSetting;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FFactionMenuSetting M_GerThermoKorpsSetting;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	FFactionMenuSetting M_GerItalianGameSetting;

	UPROPERTY(EditDefaultsOnly, Category = "Faction Selection")
	TSoftObjectPtr<UWorld> M_CampaignWorld;

private:
	UPROPERTY()
	TWeakObjectPtr<AFactionPlayerController> M_FactionPlayerController;

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

	UFUNCTION()
	void HandleSortArmoredCarsClicked();

	UFUNCTION()
	void HandleSortLightTanksClicked();

	UFUNCTION()
	void HandleSortMediumTanksClicked();

	UFUNCTION()
	void HandleSortHeavyTanksClicked();

	UFUNCTION()
	void HandleShowAllUnitsClicked();

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
	void PopulateFactionUnits(const FFactionMenuSetting& FactionSetting);
	void AddTankUnitWidgets(
		const TArray<FFactionMenuTankOption>& TankOptions,
		TArray<TObjectPtr<UW_FactionUnit>>& OutUnitWidgets);
	void AddSquadUnitWidgets(const TArray<FFactionMenuSquadOption>& SquadOptions);
	void AddAircraftUnitWidgets(const TArray<FFactionMenuAircraftOption>& AircraftOptions);
	void AddBuildingExpansionUnitWidgets(const TArray<FFactionMenuBuildingExpansionOption>& BuildingExpansionOptions);
	void AddUnitWidgetFromTrainingOption(
		const FTrainingOption& TrainingOption,
		TArray<TObjectPtr<UW_FactionUnit>>* OptionalBucket);
	void ApplyUnitFilter(const TArray<TObjectPtr<UW_FactionUnit>>& VisibleUnits);
	void ShowAllUnitWidgets();
	void ResetUnitWidgets();
	void PlayFactionAudio(const FFactionMenuSetting& FactionSetting);
	void UpdatePortraitForAnnouncement();
	void HidePortrait();
	void LaunchCampaign();

	bool GetIsValidPlayerController() const;
	bool GetIsValidScrollBox() const;
	bool GetIsValidPortrait() const;
	bool GetIsValidFactionUnitWidgetClass() const;
	bool GetIsValidGermanBreakthroughButton() const;
	bool GetIsValidGermanBreakthroughAudioButton() const;
	bool GetIsValidGermanThermoKorpsButton() const;
	bool GetIsValidGermanThermoKorpsAudioButton() const;
	bool GetIsValidGermanItalianGameButton() const;
	bool GetIsValidGermanItalianGameAudioButton() const;
	bool GetIsValidLaunchCampaignButton() const;
	bool GetIsValidSortArmoredCarsButton() const;
	bool GetIsValidSortLightTanksButton() const;
	bool GetIsValidSortMediumTanksButton() const;
	bool GetIsValidSortHeavyTanksButton() const;
	bool GetIsValidShowAllUnitsButton() const;

	// Tracks widget groups used to filter the scroll list without destroying widgets.
	UPROPERTY()
	FFactionSelectionUnitWidgets M_UnitWidgets;
};
