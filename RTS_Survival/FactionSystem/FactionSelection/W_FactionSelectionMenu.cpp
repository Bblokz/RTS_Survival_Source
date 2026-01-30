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

	SetupButtonBindings();
	ApplyFactionAvailability();
	SelectGermanBreakthrough(true);
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

	PopulateFactionUnits(FactionSetting.UnitOptions);
}

void UW_FactionSelectionMenu::PopulateFactionUnits(const TArray<FTrainingOption>& UnitOptions)
{
	if (not GetIsValidScrollBox())
	{
		return;
	}

	if (not GetIsValidFactionUnitWidgetClass())
	{
		return;
	}

	M_ScrollBox->ClearChildren();

	UW_FactionUnit* FirstFactionUnitWidget = nullptr;
	for (const FTrainingOption& TrainingOption : UnitOptions)
	{
		UW_FactionUnit* FactionUnitWidget = CreateWidget<UW_FactionUnit>(this, M_FactionUnitWidgetClass);
		if (not IsValid(FactionUnitWidget))
		{
			continue;
		}

		if (FirstFactionUnitWidget == nullptr)
		{
			FirstFactionUnitWidget = FactionUnitWidget;
		}

		FactionUnitWidget->SetFactionSelectionMenu(this);
		FactionUnitWidget->SetTrainingOption(TrainingOption);
		M_ScrollBox->AddChild(FactionUnitWidget);
	}

	if (not IsValid(FirstFactionUnitWidget))
	{
		return;
	}

	FirstFactionUnitWidget->SimulateUnitButtonClick();
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
