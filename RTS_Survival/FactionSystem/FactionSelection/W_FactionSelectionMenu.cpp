// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionSelectionMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/GameUI/Portrait/PortraitWidget/W_Portrait.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UnitBrowser/FactionUnitBrowserLayout.h"
#include "UnitBrowser/W_FactionUnitBrowser.h"

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
	SetupUnitBrowser();
	SetupButtonBindings();
	ApplyFactionAvailability();
	SelectGermanBreakthrough(true);
}

void UW_FactionSelectionMenu::SetupFactionPlayerController()
{
	M_FactionPlayerController = Cast<AFactionPlayerController>(GetOwningPlayer());
}

void UW_FactionSelectionMenu::SetupUnitBrowser()
{
	using namespace FactionUnitBrowserLayout;
	if (IsValid(M_UnitBrowser))
	{
		return;
	}
	UCanvasPanel* const RootCanvas = IsValid(WidgetTree) ? Cast<UCanvasPanel>(WidgetTree->RootWidget) : nullptr;
	if (not IsValid(RootCanvas) || M_UnitBrowserClass == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("Faction selection menu needs a root canvas and M_UnitBrowserClass "
			"(BP_W_FactionUnitBrowser) to show the faction units."));
		return;
	}
	M_UnitBrowser = CreateWidget<UW_FactionUnitBrowser>(this, M_UnitBrowserClass);
	if (not IsValid(M_UnitBrowser))
	{
		return;
	}
	M_UnitBrowser->SetupBrowser(this);
	UCanvasPanelSlot* const BrowserSlot = RootCanvas->AddChildToCanvas(M_UnitBrowser);
	const FVector2D BottomCentre(0.5f, 1.f);
	BrowserSlot->SetAnchors(FAnchors(BottomCentre.X, BottomCentre.Y));
	BrowserSlot->SetAlignment(BottomCentre);
	BrowserSlot->SetPosition(FVector2D(0.f, -BottomMargin));
	BrowserSlot->SetAutoSize(true);
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
	M_SelectedFaction = ERTSFaction::GerBreakthroughDoctrine;
	SelectFaction(M_GerBreakthroughSetting, bForceAudio, bM_HasPlayedGerBreakthroughAudio);
}

void UW_FactionSelectionMenu::SelectGermanThermoKorps(const bool bForceAudio)
{
	M_SelectedFaction = ERTSFaction::GerStrikeDivision;
	SelectFaction(M_GerThermoKorpsSetting, bForceAudio, bM_HasPlayedGerThermoKorpsAudio);
}

void UW_FactionSelectionMenu::SelectGermanItalianGame(const bool bForceAudio)
{
	M_SelectedFaction = ERTSFaction::GerItalianFaction;
	SelectFaction(M_GerItalianGameSetting, bForceAudio, bM_HasPlayedGerItalianGameAudio);
}

void UW_FactionSelectionMenu::SelectFaction(
	const FFactionMenuSetting& FactionSetting,
	const bool bForceAudio,
	bool& bHasPlayedAudio)
{
	UpdateFactionSelectionVisuals();

	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_FactionPlayerController->HandleFactionSelectionChanged(M_SelectedFaction);
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

void UW_FactionSelectionMenu::UpdateFactionSelectionVisuals()
{
	if (GetIsValidGermanBreakthroughButton())
	{
		UpdateFactionButtonSelection(M_GerBreakthrough, TEXT("M_GlassGerBreakthrough"),
			M_SelectedFaction == ERTSFaction::GerBreakthroughDoctrine);
	}
	if (GetIsValidGermanThermoKorpsButton())
	{
		UpdateFactionButtonSelection(M_GerThermoKorps, TEXT("M_GlassGerThermoKorps"),
			M_SelectedFaction == ERTSFaction::GerStrikeDivision);
	}
	if (GetIsValidGermanItalianGameButton())
	{
		UpdateFactionButtonSelection(M_GerItalianGame, TEXT("M_GlassGerItalianGame"),
			M_SelectedFaction == ERTSFaction::GerItalianFaction);
	}
}

void UW_FactionSelectionMenu::UpdateFactionButtonSelection(UButton* const FactionButton,
	const FName GlassWidgetName, const bool bIsSelected)
{
	FButtonStyle ButtonStyle = FactionButton->GetStyle();
	UpdateFactionSelectionBrush(ButtonStyle.Normal, bIsSelected);
	UpdateFactionSelectionBrush(ButtonStyle.Hovered, bIsSelected);
	FactionButton->SetStyle(ButtonStyle);
	const FLinearColor SelectedButtonTint(1.0f, 0.82f, 0.50f, 1.0f);
	FactionButton->SetBackgroundColor(bIsSelected ? SelectedButtonTint : FLinearColor::White);

	UImage* const GlassPane = Cast<UImage>(GetWidgetFromName(GlassWidgetName));
	if (not IsValid(GlassPane))
	{
		RTSFunctionLibrary::ReportError(TEXT("Faction selection menu is missing its glass image: ")
			+ GlassWidgetName.ToString());
		return;
	}
	UMaterialInstanceDynamic* const GlassMaterial = GlassPane->GetDynamicMaterial();
	if (not IsValid(GlassMaterial))
	{
		RTSFunctionLibrary::ReportError(TEXT("Faction glass image needs a glass pane material: ")
			+ GlassWidgetName.ToString());
		return;
	}

	const FName RimParameter(TEXT("RimIntensity"));
	const FName StreakParameter(TEXT("StreakIntensity"));
	const FName TintParameter(TEXT("EdgeTint"));
	constexpr float SelectedRimIntensity = 0.85f;
	constexpr float UnselectedRimIntensity = 0.26f;
	constexpr float SelectedStreakIntensity = 0.10f;
	constexpr float UnselectedStreakIntensity = 0.045f;
	const FLinearColor SelectedGlassTint(1.0f, 0.58f, 0.16f, 0.38f);
	const FLinearColor UnselectedGlassTint(0.80f, 0.90f, 1.0f, 0.06f);
	GlassMaterial->SetScalarParameterValue(RimParameter,
		bIsSelected ? SelectedRimIntensity : UnselectedRimIntensity);
	GlassMaterial->SetScalarParameterValue(StreakParameter,
		bIsSelected ? SelectedStreakIntensity : UnselectedStreakIntensity);
	GlassMaterial->SetVectorParameterValue(TintParameter,
		bIsSelected ? SelectedGlassTint : UnselectedGlassTint);
}

void UW_FactionSelectionMenu::UpdateFactionSelectionBrush(FSlateBrush& Brush, const bool bIsSelected)
{
	UMaterialInterface* const SourceMaterial = Cast<UMaterialInterface>(Brush.GetResourceObject());
	if (not IsValid(SourceMaterial))
	{
		RTSFunctionLibrary::ReportError(TEXT("Faction selection button needs a skinned UI material brush."));
		return;
	}
	UMaterialInstanceDynamic* ButtonMaterial = Cast<UMaterialInstanceDynamic>(SourceMaterial);
	if (not IsValid(ButtonMaterial))
	{
		ButtonMaterial = UMaterialInstanceDynamic::Create(SourceMaterial, this);
	}
	if (not IsValid(ButtonMaterial))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to create the faction button selection material."));
		return;
	}
	Brush.SetResourceObject(ButtonMaterial);

	const FName EffectsParameter(TEXT("Fx"));
	constexpr float SelectedGlintIntensity = 0.75f;
	constexpr float UnselectedGlintIntensity = 0.0f;
	FLinearColor Effects = ButtonMaterial->K2_GetVectorParameterValue(EffectsParameter);
	Effects.B = bIsSelected ? SelectedGlintIntensity : UnselectedGlintIntensity;
	ButtonMaterial->SetVectorParameterValue(EffectsParameter, Effects);
}

TArray<FTrainingOption> UW_FactionSelectionMenu::GetFactionTrainingOptions(const ERTSFaction Faction) const
{
	TArray<FTrainingOption> TrainingOptions;
	const FFactionMenuSetting* FactionSetting = FindFactionSetting(Faction);
	if (not FactionSetting)
	{
		return TrainingOptions;
	}
	TrainingOptions.Append(ToTrainingOptions(FactionSetting->ArmoredCars));
	TrainingOptions.Append(ToTrainingOptions(FactionSetting->LightTanks));
	TrainingOptions.Append(ToTrainingOptions(FactionSetting->MediumTanks));
	TrainingOptions.Append(ToTrainingOptions(FactionSetting->HeavyTanks));
	TrainingOptions.Append(ToTrainingOptions(FactionSetting->Infantry));
	TrainingOptions.Append(ToTrainingOptions(FactionSetting->SupportWeapons));
	TrainingOptions.Append(ToTrainingOptions(FactionSetting->BuildingExpansions));
	return TrainingOptions;
}

void UW_FactionSelectionMenu::ShowFaction(const ERTSFaction Faction)
{
	switch (Faction)
	{
	case ERTSFaction::GerBreakthroughDoctrine:
		SelectGermanBreakthrough(false);
		return;
	case ERTSFaction::GerStrikeDivision:
		SelectGermanThermoKorps(false);
		return;
	case ERTSFaction::GerItalianFaction:
		SelectGermanItalianGame(false);
		return;
	default:
		return;
	}
}

const FFactionMenuSetting* UW_FactionSelectionMenu::FindFactionSetting(const ERTSFaction Faction) const
{
	switch (Faction)
	{
	case ERTSFaction::GerBreakthroughDoctrine:
		return &M_GerBreakthroughSetting;
	case ERTSFaction::GerStrikeDivision:
		return &M_GerThermoKorpsSetting;
	case ERTSFaction::GerItalianFaction:
		return &M_GerItalianGameSetting;
	default:
		return nullptr;
	}
}

void UW_FactionSelectionMenu::PopulateFactionUnits(const FFactionMenuSetting& FactionSetting)
{
	if (not GetIsValidUnitBrowser())
	{
		return;
	}
	M_UnitBrowser->SetUnitGroups(BuildUnitBrowserGroups(FactionSetting));
}

TArray<FFactionUnitBrowserGroup> UW_FactionSelectionMenu::BuildUnitBrowserGroups(
	const FFactionMenuSetting& FactionSetting)
{
	auto MakeGroup = [](const FText& Label, TArray<FTrainingOption>&& Units)
	{
		FFactionUnitBrowserGroup Group;
		Group.Label = Label;
		Group.Units = MoveTemp(Units);
		return Group;
	};
	TArray<FFactionUnitBrowserGroup> Groups;
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabAll", "All"), {}));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabRecon", "Recon"),
	                     ToTrainingOptions(FactionSetting.ArmoredCars)));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabLight", "Light"),
	                     ToTrainingOptions(FactionSetting.LightTanks)));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabMedium", "Medium"),
	                     ToTrainingOptions(FactionSetting.MediumTanks)));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabHeavy", "Heavy"),
	                     ToTrainingOptions(FactionSetting.HeavyTanks)));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabInfantry", "Infantry"),
	                     ToTrainingOptions(FactionSetting.Infantry)));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabSupport", "Support"),
	                     ToTrainingOptions(FactionSetting.SupportWeapons)));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabAircraft", "Aircraft"),
	                     ToTrainingOptions(FactionSetting.Aircraft)));
	Groups.Add(MakeGroup(NSLOCTEXT("FactionSelectionMenu", "TabDefences", "Defences"),
	                     ToTrainingOptions(FactionSetting.BuildingExpansions)));
	// ALL lists every other group in tab order.
	for (int32 GroupIndex = 1; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		Groups[0].Units.Append(Groups[GroupIndex].Units);
	}
	return Groups;
}

TArray<FTrainingOption> UW_FactionSelectionMenu::ToTrainingOptions(const TArray<FFactionMenuTankOption>& TankOptions)
{
	TArray<FTrainingOption> TrainingOptions;
	for (const FFactionMenuTankOption& TankOption : TankOptions)
	{
		if (TankOption.TankSubtype != ETankSubtype::Tank_None)
		{
			TrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankOption.TankSubtype)));
		}
	}
	return TrainingOptions;
}

TArray<FTrainingOption> UW_FactionSelectionMenu::ToTrainingOptions(const TArray<FFactionMenuSquadOption>& SquadOptions)
{
	TArray<FTrainingOption> TrainingOptions;
	for (const FFactionMenuSquadOption& SquadOption : SquadOptions)
	{
		if (SquadOption.SquadSubtype != ESquadSubtype::Squad_None)
		{
			TrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_Squad,
			                                    static_cast<uint8>(SquadOption.SquadSubtype)));
		}
	}
	return TrainingOptions;
}

TArray<FTrainingOption> UW_FactionSelectionMenu::ToTrainingOptions(
	const TArray<FFactionMenuAircraftOption>& AircraftOptions)
{
	TArray<FTrainingOption> TrainingOptions;
	for (const FFactionMenuAircraftOption& AircraftOption : AircraftOptions)
	{
		if (AircraftOption.AircraftSubtype != EAircraftSubtype::Aircarft_None)
		{
			TrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_Aircraft,
			                                    static_cast<uint8>(AircraftOption.AircraftSubtype)));
		}
	}
	return TrainingOptions;
}

TArray<FTrainingOption> UW_FactionSelectionMenu::ToTrainingOptions(
	const TArray<FFactionMenuBuildingExpansionOption>& BuildingExpansionOptions)
{
	TArray<FTrainingOption> TrainingOptions;
	for (const FFactionMenuBuildingExpansionOption& BxpOption : BuildingExpansionOptions)
	{
		if (BxpOption.BuildingExpansionType != EBuildingExpansionType::BXT_Invalid)
		{
			TrainingOptions.Add(FTrainingOption(EAllUnitType::UNType_BuildingExpansion,
			                                    static_cast<uint8>(BxpOption.BuildingExpansionType)));
		}
	}
	return TrainingOptions;
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
	if (not GetIsValidPlayerController())
	{
		return;
	}

	M_FactionPlayerController->HandleLaunchCampaignRequested(M_SelectedFaction);
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

bool UW_FactionSelectionMenu::GetIsValidUnitBrowser() const
{
	if (not IsValid(M_UnitBrowser))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			"M_UnitBrowser",
			"GetIsValidUnitBrowser",
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
