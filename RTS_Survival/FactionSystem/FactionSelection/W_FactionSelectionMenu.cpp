// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionSelectionMenu.h"

#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/GameUI/Portrait/PortraitWidget/W_Portrait.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_FactionUnit.h"

void UW_FactionSelectionMenu::SetFactionPlayerController(AFactionPlayerController* FactionPlayerController)
{
	M_FactionPlayerController = FactionPlayerController;
}

void UW_FactionSelectionMenu::HandleUnitSelected(const FTrainingOption& TrainingOption)
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_FactionPlayerController->SpawnPreviewForTrainingOption(TrainingOption);
}

void UW_FactionSelectionMenu::HandleAnnouncementFinished()
{
	HidePortrait();
}

void UW_FactionSelectionMenu::NativeConstruct()
{
	Super::NativeConstruct();
	SetupFactionPlayerController();
	SetupButtonBindings();
	ApplyFactionAvailability();
	SelectGermanBreakthrough(true);
}

void UW_FactionSelectionMenu::SetupFactionPlayerController()
{
	M_FactionPlayerController = Cast<AFactionPlayerController>(GetOwningPlayer());
}

void UW_FactionSelectionMenu::HandleGerBreakthroughClicked()
{
	SelectGermanBreakthrough(false);
}

void UW_FactionSelectionMenu::HandleGerBreakthroughAudioClicked()
{
	SelectGermanBreakthrough(true);
}

void UW_FactionSelectionMenu::HandleGerThermoKorpsClicked()
{
	SelectGermanThermoKorps(false);
}

void UW_FactionSelectionMenu::HandleGerThermoKorpsAudioClicked()
{
	SelectGermanThermoKorps(true);
}

void UW_FactionSelectionMenu::HandleGerItalianGameClicked()
{
	SelectGermanItalianGame(false);
}

void UW_FactionSelectionMenu::HandleGerItalianGameAudioClicked()
{
	SelectGermanItalianGame(true);
}

void UW_FactionSelectionMenu::HandleLaunchCampaignClicked()
{
	LaunchCampaign();
}

void UW_FactionSelectionMenu::HandleSortArmoredCarsClicked()
{
	ApplyUnitFilter(M_UnitWidgets.ArmoredCars);
}

void UW_FactionSelectionMenu::HandleSortLightTanksClicked()
{
	ApplyUnitFilter(M_UnitWidgets.LightTanks);
}

void UW_FactionSelectionMenu::HandleSortMediumTanksClicked()
{
	ApplyUnitFilter(M_UnitWidgets.MediumTanks);
}

void UW_FactionSelectionMenu::HandleSortHeavyTanksClicked()
{
	ApplyUnitFilter(M_UnitWidgets.HeavyTanks);
}

void UW_FactionSelectionMenu::HandleShowAllUnitsClicked()
{
	ShowAllUnitWidgets();
}

void UW_FactionSelectionMenu::SetupButtonBindings()
{
	if (GetIsValidGermanBreakthroughButton())
	{
		M_GerBreakthrough->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleGerBreakthroughClicked);
	}

	if (GetIsValidGermanBreakthroughAudioButton())
	{
		M_GerBreakthroughAudio->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleGerBreakthroughAudioClicked);
	}

	if (GetIsValidGermanThermoKorpsButton())
	{
		M_GerThermoKorps->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleGerThermoKorpsClicked);
	}

	if (GetIsValidGermanThermoKorpsAudioButton())
	{
		M_GerThermoKorpsAudio->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleGerThermoKorpsAudioClicked);
	}

	if (GetIsValidGermanItalianGameButton())
	{
		M_GerItalianGame->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleGerItalianGameClicked);
	}

	if (GetIsValidGermanItalianGameAudioButton())
	{
		M_GerItalianGameAudio->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleGerItalianGameAudioClicked);
	}

	if (GetIsValidLaunchCampaignButton())
	{
		M_LaunchCampaign->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleLaunchCampaignClicked);
	}

	if (GetIsValidSortArmoredCarsButton())
	{
		SortArmoredCars->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleSortArmoredCarsClicked);
	}

	if (GetIsValidSortLightTanksButton())
	{
		SortLightTanks->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleSortLightTanksClicked);
	}

	if (GetIsValidSortMediumTanksButton())
	{
		SortMediumTanks->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleSortMediumTanksClicked);
	}

	if (GetIsValidSortHeavyTanksButton())
	{
		SortHeavyTanks->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleSortHeavyTanksClicked);
	}

	if (GetIsValidShowAllUnitsButton())
	{
		ShowAllUnits->OnClicked.AddDynamic(this, &UW_FactionSelectionMenu::HandleShowAllUnitsClicked);
	}
}

void UW_FactionSelectionMenu::ApplyFactionAvailability()
{
	if (GetIsValidGermanBreakthroughButton())
	{
		M_GerBreakthrough->SetIsEnabled(M_GerBreakthroughSetting.bIsEnabled);
	}

	if (GetIsValidGermanBreakthroughAudioButton())
	{
		M_GerBreakthroughAudio->SetIsEnabled(M_GerBreakthroughSetting.bIsEnabled);
	}

	if (GetIsValidGermanThermoKorpsButton())
	{
		M_GerThermoKorps->SetIsEnabled(M_GerThermoKorpsSetting.bIsEnabled);
	}

	if (GetIsValidGermanThermoKorpsAudioButton())
	{
		M_GerThermoKorpsAudio->SetIsEnabled(M_GerThermoKorpsSetting.bIsEnabled);
	}

	if (GetIsValidGermanItalianGameButton())
	{
		M_GerItalianGame->SetIsEnabled(M_GerItalianGameSetting.bIsEnabled);
	}

	if (GetIsValidGermanItalianGameAudioButton())
	{
		M_GerItalianGameAudio->SetIsEnabled(M_GerItalianGameSetting.bIsEnabled);
	}
}

void UW_FactionSelectionMenu::SelectGermanBreakthrough(const bool bForceAudio)
{
	SelectFaction(M_GerBreakthroughSetting, bForceAudio, bM_HasPlayedGerBreakthroughAudio);
}

void UW_FactionSelectionMenu::SelectGermanThermoKorps(const bool bForceAudio)
{
	SelectFaction(M_GerThermoKorpsSetting, bForceAudio, bM_HasPlayedGerThermoKorpsAudio);
}

void UW_FactionSelectionMenu::SelectGermanItalianGame(const bool bForceAudio)
{
	SelectFaction(M_GerItalianGameSetting, bForceAudio, bM_HasPlayedGerItalianGameAudio);
}

void UW_FactionSelectionMenu::SelectFaction(
	const FFactionMenuSetting& FactionSetting,
	const bool bForceAudio,
	bool& bHasPlayedAudio)
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_FactionPlayerController->StopAnnouncementSound();

	const bool bShouldPlayAudio = bForceAudio || not bHasPlayedAudio;
	if (bShouldPlayAudio)
	{
		UpdatePortraitForAnnouncement();
		PlayFactionAudio(FactionSetting);
		bHasPlayedAudio = true;
	}
	else
	{
		HidePortrait();
	}

	PopulateFactionUnits(FactionSetting);
}

void UW_FactionSelectionMenu::PopulateFactionUnits(const FFactionMenuSetting& FactionSetting)
{
	if (not GetIsValidScrollBox())
	{
		return;
	}

	if (not GetIsValidFactionUnitWidgetClass())
	{
		return;
	}

	ResetUnitWidgets();

	AddTankUnitWidgets(FactionSetting.ArmoredCars, M_UnitWidgets.ArmoredCars);
	AddTankUnitWidgets(FactionSetting.LightTanks, M_UnitWidgets.LightTanks);
	AddTankUnitWidgets(FactionSetting.MediumTanks, M_UnitWidgets.MediumTanks);
	AddTankUnitWidgets(FactionSetting.HeavyTanks, M_UnitWidgets.HeavyTanks);
	AddSquadUnitWidgets(FactionSetting.Infantry);
	AddAircraftUnitWidgets(FactionSetting.Aircraft);
	AddBuildingExpansionUnitWidgets(FactionSetting.BuildingExpansions);

	ShowAllUnitWidgets();
}

void UW_FactionSelectionMenu::AddTankUnitWidgets(
	const TArray<FFactionMenuTankOption>& TankOptions,
	TArray<TObjectPtr<UW_FactionUnit>>& OutUnitWidgets)
{
	for (const FFactionMenuTankOption& TankOption : TankOptions)
	{
		if (TankOption.TankSubtype == ETankSubtype::Tank_None)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Tank,
			static_cast<uint8>(TankOption.TankSubtype));
		AddUnitWidgetFromTrainingOption(TrainingOption, &OutUnitWidgets);
	}
}

void UW_FactionSelectionMenu::AddSquadUnitWidgets(const TArray<FFactionMenuSquadOption>& SquadOptions)
{
	for (const FFactionMenuSquadOption& SquadOption : SquadOptions)
	{
		if (SquadOption.SquadSubtype == ESquadSubtype::Squad_None)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Squad,
			static_cast<uint8>(SquadOption.SquadSubtype));
		AddUnitWidgetFromTrainingOption(TrainingOption, nullptr);
	}
}

void UW_FactionSelectionMenu::AddAircraftUnitWidgets(const TArray<FFactionMenuAircraftOption>& AircraftOptions)
{
	for (const FFactionMenuAircraftOption& AircraftOption : AircraftOptions)
	{
		if (AircraftOption.AircraftSubtype == EAircraftSubtype::Aircarft_None)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_Aircraft,
			static_cast<uint8>(AircraftOption.AircraftSubtype));
		AddUnitWidgetFromTrainingOption(TrainingOption, nullptr);
	}
}

void UW_FactionSelectionMenu::AddBuildingExpansionUnitWidgets(
	const TArray<FFactionMenuBuildingExpansionOption>& BuildingExpansionOptions)
{
	for (const FFactionMenuBuildingExpansionOption& BuildingExpansionOption : BuildingExpansionOptions)
	{
		if (BuildingExpansionOption.BuildingExpansionType == EBuildingExpansionType::BXT_Invalid)
		{
			continue;
		}

		const FTrainingOption TrainingOption(
			EAllUnitType::UNType_BuildingExpansion,
			static_cast<uint8>(BuildingExpansionOption.BuildingExpansionType));
		AddUnitWidgetFromTrainingOption(TrainingOption, nullptr);
	}
}

void UW_FactionSelectionMenu::AddUnitWidgetFromTrainingOption(
	const FTrainingOption& TrainingOption,
	TArray<TObjectPtr<UW_FactionUnit>>* OptionalBucket)
{
	UW_FactionUnit* FactionUnitWidget = CreateWidget<UW_FactionUnit>(this, M_FactionUnitWidgetClass);
	if (not IsValid(FactionUnitWidget))
	{
		return;
	}

	FactionUnitWidget->SetFactionSelectionMenu(this);
	FactionUnitWidget->SetTrainingOption(TrainingOption);
	M_ScrollBox->AddChild(FactionUnitWidget);
	M_UnitWidgets.AllUnits.Add(FactionUnitWidget);

	if (OptionalBucket != nullptr)
	{
		OptionalBucket->Add(FactionUnitWidget);
	}
}

void UW_FactionSelectionMenu::ApplyUnitFilter(const TArray<TObjectPtr<UW_FactionUnit>>& VisibleUnits)
{
	for (const TObjectPtr<UW_FactionUnit>& FactionUnitWidget : M_UnitWidgets.AllUnits)
	{
		if (not IsValid(FactionUnitWidget))
		{
			continue;
		}

		FactionUnitWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	for (const TObjectPtr<UW_FactionUnit>& FactionUnitWidget : VisibleUnits)
	{
		if (not IsValid(FactionUnitWidget))
		{
			continue;
		}

		FactionUnitWidget->SetVisibility(ESlateVisibility::Visible);
	}

	for (const TObjectPtr<UW_FactionUnit>& FactionUnitWidget : VisibleUnits)
	{
		if (not IsValid(FactionUnitWidget))
		{
			continue;
		}

		FactionUnitWidget->SimulateUnitButtonClick();
		break;
	}
}

void UW_FactionSelectionMenu::ShowAllUnitWidgets()
{
	ApplyUnitFilter(M_UnitWidgets.AllUnits);
}

void UW_FactionSelectionMenu::ResetUnitWidgets()
{
	M_ScrollBox->ClearChildren();
	M_UnitWidgets.AllUnits.Reset();
	M_UnitWidgets.ArmoredCars.Reset();
	M_UnitWidgets.LightTanks.Reset();
	M_UnitWidgets.MediumTanks.Reset();
	M_UnitWidgets.HeavyTanks.Reset();
}

void UW_FactionSelectionMenu::PlayFactionAudio(const FFactionMenuSetting& FactionSetting)
{
	if (not GetIsValidPlayerController())
	{
		return;
	}

	if (FactionSetting.FactionDescription == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"FactionDescription",
			"PlayFactionAudio",
			this
		);
		return;
	}

	M_FactionPlayerController->PlayAnnouncementSound(FactionSetting.FactionDescription);
}

void UW_FactionSelectionMenu::UpdatePortraitForAnnouncement()
{
	if (not GetIsValidPortrait())
	{
		return;
	}

	M_Portrait->UpdatePortrait(ERTSPortraitTypes::GermanAnnouncer);
}

void UW_FactionSelectionMenu::HidePortrait()
{
	if (not GetIsValidPortrait())
	{
		return;
	}

	M_Portrait->HidePortrait();
}

void UW_FactionSelectionMenu::LaunchCampaign()
{
	if (M_CampaignWorld.IsNull())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_CampaignWorld",
			"LaunchCampaign",
			this
		);
		return;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, M_CampaignWorld);
}

bool UW_FactionSelectionMenu::GetIsValidPlayerController() const
{
	if (not M_FactionPlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_FactionPlayerController",
			"GetIsValidPlayerController",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidScrollBox() const
{
	if (not IsValid(M_ScrollBox))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_ScrollBox",
			"GetIsValidScrollBox",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidPortrait() const
{
	if (not IsValid(M_Portrait))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_Portrait",
			"GetIsValidPortrait",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidFactionUnitWidgetClass() const
{
	if (M_FactionUnitWidgetClass == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_FactionUnitWidgetClass",
			"GetIsValidFactionUnitWidgetClass",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidGermanBreakthroughButton() const
{
	if (not IsValid(M_GerBreakthrough))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_GerBreakthrough",
			"GetIsValidGermanBreakthroughButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidGermanBreakthroughAudioButton() const
{
	if (not IsValid(M_GerBreakthroughAudio))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_GerBreakthroughAudio",
			"GetIsValidGermanBreakthroughAudioButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidGermanThermoKorpsButton() const
{
	if (not IsValid(M_GerThermoKorps))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_GerThermoKorps",
			"GetIsValidGermanThermoKorpsButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidGermanThermoKorpsAudioButton() const
{
	if (not IsValid(M_GerThermoKorpsAudio))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_GerThermoKorpsAudio",
			"GetIsValidGermanThermoKorpsAudioButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidGermanItalianGameButton() const
{
	if (not IsValid(M_GerItalianGame))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_GerItalianGame",
			"GetIsValidGermanItalianGameButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidGermanItalianGameAudioButton() const
{
	if (not IsValid(M_GerItalianGameAudio))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_GerItalianGameAudio",
			"GetIsValidGermanItalianGameAudioButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidLaunchCampaignButton() const
{
	if (not IsValid(M_LaunchCampaign))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_LaunchCampaign",
			"GetIsValidLaunchCampaignButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidSortArmoredCarsButton() const
{
	if (not IsValid(SortArmoredCars))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"SortArmoredCars",
			"GetIsValidSortArmoredCarsButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidSortLightTanksButton() const
{
	if (not IsValid(SortLightTanks))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"SortLightTanks",
			"GetIsValidSortLightTanksButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidSortMediumTanksButton() const
{
	if (not IsValid(SortMediumTanks))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"SortMediumTanks",
			"GetIsValidSortMediumTanksButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidSortHeavyTanksButton() const
{
	if (not IsValid(SortHeavyTanks))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"SortHeavyTanks",
			"GetIsValidSortHeavyTanksButton",
			this
		);
		return false;
	}

	return true;
}

bool UW_FactionSelectionMenu::GetIsValidShowAllUnitsButton() const
{
	if (not IsValid(ShowAllUnits))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"ShowAllUnits",
			"GetIsValidShowAllUnitsButton",
			this
		);
		return false;
	}

	return true;
}
