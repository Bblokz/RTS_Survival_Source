// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_GameDifficultyPicker.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Game/Difficulty/GameDifficultySettings/RTSGameDifficultySettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace VehicleBalanceColumnLayout
{
	// Row of the picker Blueprints that holds the difficulty buttons and the details panel.
	const FName DifficultyColumnsName = TEXT("DifficultyColumns");
	constexpr float ColumnWidth = 280.f;
	constexpr float OptionHeight = 76.f;
	constexpr float OptionSpacing = 12.f;
	constexpr float ColumnSpacing = 24.f;
	constexpr float NoteWidth = 320.f;
	constexpr float NoteSpacing = 16.f;
	const FMargin OptionTextPadding(20.f, 14.f);
}

void UW_GameDifficultyPicker::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (not GetIsValidNewToRTSDifficultyButton()
		or not GetIsValidNormalDifficultyButton()
		or not GetIsValidHardDifficultyButton()
		or not GetIsValidBrutalDifficultyButton()
		or not GetIsValidIronmanDifficultyButton()
		or not GetIsValidDifficultyInfoBorder())
	{
		return;
	}

	M_NewToRTSDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNewToRTSDifficultyHovered);
	M_NormalDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNormalDifficultyHovered);
	M_HardDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnHardDifficultyHovered);
	M_BrutalDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnBrutalDifficultyHovered);
	M_IronmanDifficultyButton->OnHovered.AddDynamic(this, &UW_GameDifficultyPicker::OnIronmanDifficultyHovered);

	M_NewToRTSDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNewToRTSDifficultyUnhovered);
	M_NormalDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnNormalDifficultyUnhovered);
	M_HardDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnHardDifficultyUnhovered);
	M_BrutalDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnBrutalDifficultyUnhovered);
	M_IronmanDifficultyButton->OnUnhovered.AddDynamic(this, &UW_GameDifficultyPicker::OnIronmanDifficultyUnhovered);

	M_NewToRTSDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnNewToRTSDifficultyClicked);
	M_NormalDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnNormalDifficultyClicked);
	M_HardDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnHardDifficultyClicked);
	M_BrutalDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnBrutalDifficultyClicked);
	M_IronmanDifficultyButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnIronmanDifficultyClicked);

	SetDifficultyInfoBorderVisible(false);
	NativeOnInitialized_BuildVehicleBalanceColumn();
}

void UW_GameDifficultyPicker::NativeOnInitialized_BuildVehicleBalanceColumn()
{
	using namespace VehicleBalanceColumnLayout;
	UHorizontalBox* const DifficultyColumns = IsValid(WidgetTree)
		                                          ? WidgetTree->FindWidget<UHorizontalBox>(DifficultyColumnsName)
		                                          : nullptr;
	const URichTextBlock* const TemplateText = Cast<URichTextBlock>(M_NormalDifficultyButton->GetContent());
	if (not IsValid(DifficultyColumns) || not IsValid(TemplateText))
	{
		RTSFunctionLibrary::ReportError(
			"Difficulty picker could not build the vehicle balance toggle: it needs a HorizontalBox named "
			"DifficultyColumns and a RichTextBlock inside M_NormalDifficultyButton.");
		return;
	}

	USizeBox* const ColumnWidthBox = WidgetTree->ConstructWidget<USizeBox>();
	ColumnWidthBox->SetWidthOverride(ColumnWidth);
	ColumnWidthBox->SetContent(CreateVehicleBalanceColumn(*TemplateText));
	USizeBox* const NoteWidthBox = WidgetTree->ConstructWidget<USizeBox>();
	NoteWidthBox->SetWidthOverride(NoteWidth);
	NoteWidthBox->SetContent(CreateVehicleBalanceNote(*TemplateText));

	UHorizontalBox* const BalanceGroup = WidgetTree->ConstructWidget<UHorizontalBox>();
	BalanceGroup->AddChildToHorizontalBox(ColumnWidthBox)->SetVerticalAlignment(VAlign_Top);
	UHorizontalBoxSlot* const NoteSlot = BalanceGroup->AddChildToHorizontalBox(NoteWidthBox);
	NoteSlot->SetPadding(FMargin(NoteSpacing, 0.f, 0.f, 0.f));
	NoteSlot->SetVerticalAlignment(VAlign_Top);

	// Right of the difficulty buttons, before the difficulty details panel.
	constexpr int32 RightOfDifficultyButtonsIndex = 1;
	UHorizontalBoxSlot* const GroupSlot = Cast<UHorizontalBoxSlot>(
		DifficultyColumns->InsertChildAt(RightOfDifficultyButtonsIndex, BalanceGroup));
	if (IsValid(GroupSlot))
	{
		GroupSlot->SetPadding(FMargin(ColumnSpacing, 0.f, ColumnSpacing, 0.f));
		GroupSlot->SetVerticalAlignment(VAlign_Top);
	}

	M_VehicleBalanceWidgets.ArcadeButton->OnClicked.AddDynamic(this, &UW_GameDifficultyPicker::OnArcadeBalanceClicked);
	M_VehicleBalanceWidgets.RealisticButton->OnClicked.AddDynamic(
		this, &UW_GameDifficultyPicker::OnRealisticBalanceClicked);
}

UVerticalBox* UW_GameDifficultyPicker::CreateVehicleBalanceColumn(const URichTextBlock& TemplateText)
{
	using namespace VehicleBalanceColumnLayout;
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>();

	URichTextBlock* const Header = CreateVehicleBalanceText(TemplateText);
	Header->SetText(FText::FromString(TEXT("<Text_Title>Vehicle Balance</>")));
	UVerticalBoxSlot* const HeaderSlot = Column->AddChildToVerticalBox(Header);
	HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, OptionSpacing));
	HeaderSlot->SetHorizontalAlignment(HAlign_Center);

	M_VehicleBalanceWidgets.ArcadeButton = AddVehicleBalanceOption(
		*Column, TemplateText, M_VehicleBalanceWidgets.ArcadeLabel);
	M_VehicleBalanceWidgets.ArcadeLabel->SetText(FText::FromString(TEXT("<Text_NewGood>Arcade</>")));
	M_VehicleBalanceWidgets.RealisticButton = AddVehicleBalanceOption(
		*Column, TemplateText, M_VehicleBalanceWidgets.RealisticLabel);
	M_VehicleBalanceWidgets.RealisticLabel->SetText(FText::FromString(TEXT("<Text_NewGood>Realistic</>")));

	M_VehicleBalanceWidgets.PickBothHint = CreateVehicleBalanceText(TemplateText);
	M_VehicleBalanceWidgets.PickBothHint->SetAutoWrapText(true);
	M_VehicleBalanceWidgets.PickBothHint->SetText(FText::FromString(
		TEXT("<Text_DescriptionHelper>Pick a difficulty and a vehicle balance to start.</>")));
	Column->AddChildToVerticalBox(M_VehicleBalanceWidgets.PickBothHint);
	return Column;
}

UBorder* UW_GameDifficultyPicker::CreateVehicleBalanceNote(const URichTextBlock& TemplateText)
{
	using namespace VehicleBalanceColumnLayout;
	// Same panel look as the difficulty details panel; it opens once a balance is picked.
	UBorder* const NoteBorder = WidgetTree->ConstructWidget<UBorder>();
	NoteBorder->SetBrush(M_DifficultyInfoBorder->Background);
	NoteBorder->SetPadding(M_DifficultyInfoBorder->GetPadding());
	NoteBorder->SetVisibility(ESlateVisibility::Hidden);

	UVerticalBox* const NoteContent = WidgetTree->ConstructWidget<UVerticalBox>();
	M_VehicleBalanceWidgets.NoteTitle = CreateVehicleBalanceText(TemplateText);
	NoteContent->AddChildToVerticalBox(M_VehicleBalanceWidgets.NoteTitle)->SetPadding(
		FMargin(0.f, 0.f, 0.f, OptionSpacing));
	M_VehicleBalanceWidgets.NoteBody = CreateVehicleBalanceText(TemplateText);
	M_VehicleBalanceWidgets.NoteBody->SetAutoWrapText(true);
	NoteContent->AddChildToVerticalBox(M_VehicleBalanceWidgets.NoteBody);
	NoteBorder->SetContent(NoteContent);
	M_VehicleBalanceWidgets.NoteBorder = NoteBorder;
	return NoteBorder;
}

URichTextBlock* UW_GameDifficultyPicker::CreateVehicleBalanceText(const URichTextBlock& TemplateText)
{
	URichTextBlock* const Text = WidgetTree->ConstructWidget<URichTextBlock>();
	Text->SetTextStyleSet(TemplateText.GetTextStyleSet());
	Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Text;
}

UButton* UW_GameDifficultyPicker::AddVehicleBalanceOption(UVerticalBox& Column, const URichTextBlock& TemplateText,
                                                          TObjectPtr<URichTextBlock>& OutLabel)
{
	using namespace VehicleBalanceColumnLayout;
	UButton* const OptionButton = WidgetTree->ConstructWidget<UButton>();
	OptionButton->SetStyle(M_NormalDifficultyButton->GetStyle());
	OutLabel = CreateVehicleBalanceText(TemplateText);
	UPanelSlot* const LabelSlot = OptionButton->AddChild(OutLabel);
	if (UButtonSlot* const ButtonSlot = Cast<UButtonSlot>(LabelSlot))
	{
		ButtonSlot->SetPadding(OptionTextPadding);
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* const OptionRow = WidgetTree->ConstructWidget<USizeBox>();
	OptionRow->SetHeightOverride(OptionHeight);
	OptionRow->SetContent(OptionButton);
	UVerticalBoxSlot* const RowSlot = Column.AddChildToVerticalBox(OptionRow);
	RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, OptionSpacing));
	return OptionButton;
}

void UW_GameDifficultyPicker::OnRealisticBalanceClicked()
{
	SelectVehicleBalance(ERTSVehicleBalance::Realistic);
}

void UW_GameDifficultyPicker::OnArcadeBalanceClicked()
{
	SelectVehicleBalance(ERTSVehicleBalance::Arcade);
}

void UW_GameDifficultyPicker::SelectVehicleBalance(const ERTSVehicleBalance VehicleBalance)
{
	URTSGameInstance* const GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (IsValid(GameInstance))
	{
		GameInstance->SetVehicleBalance(VehicleBalance);
	}
	// The mission picker opens after placed vehicles began play; the game state rescales them in place.
	const UWorld* const World = GetWorld();
	if (ACPPGameState* const GameState = IsValid(World) ? World->GetGameState<ACPPGameState>() : nullptr)
	{
		GameState->SetVehicleBalance(VehicleBalance);
	}
	RefreshVehicleBalanceWidgets(VehicleBalance);
	M_PendingChoice.bHasVehicleBalance = true;
	TryContinueWithPendingChoice();
}

void UW_GameDifficultyPicker::RefreshVehicleBalanceWidgets(const ERTSVehicleBalance VehicleBalance) const
{
	if (not GetAreVehicleBalanceWidgetsValid())
	{
		return;
	}
	const bool bIsArcade = VehicleBalance == ERTSVehicleBalance::Arcade;
	// The option not picked is faded out.
	constexpr float SelectedOpacity = 1.f;
	constexpr float UnselectedOpacity = 0.4f;
	M_VehicleBalanceWidgets.ArcadeButton->SetRenderOpacity(bIsArcade ? SelectedOpacity : UnselectedOpacity);
	M_VehicleBalanceWidgets.RealisticButton->SetRenderOpacity(bIsArcade ? UnselectedOpacity : SelectedOpacity);

	M_VehicleBalanceWidgets.NoteTitle->SetText(FText::FromString(bIsArcade
		? TEXT("<Text_Title>Arcade</>")
		: TEXT("<Text_Title>Realistic</>")));
	M_VehicleBalanceWidgets.NoteBody->SetText(FText::FromString(bIsArcade
		? TEXT("<Text_DescriptionHelper>Dev's note: I personally find this more fun, as vehicles have health pools "
			"and take multiple shots to be taken out. Combat is a bit slower and you have more time to react to what "
			"happens on the battlefield.</>")
		: TEXT("<Text_DescriptionHelper>Vehicle combat can be a lot quicker. High velocity shells can overpower armor, "
			"destroying vehicles in single shots. Fuel and ammo detonations are more likely when their modules are "
			"hit. This can make the game a lot harder, but it is also easier to one-shot enemy vehicles with precise "
			"shooting.</>")));
	M_VehicleBalanceWidgets.NoteBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

bool UW_GameDifficultyPicker::GetAreVehicleBalanceWidgetsValid() const
{
	if (IsValid(M_VehicleBalanceWidgets.ArcadeButton) && IsValid(M_VehicleBalanceWidgets.RealisticButton)
		&& IsValid(M_VehicleBalanceWidgets.ArcadeLabel) && IsValid(M_VehicleBalanceWidgets.RealisticLabel)
		&& IsValid(M_VehicleBalanceWidgets.PickBothHint) && IsValid(M_VehicleBalanceWidgets.NoteBorder)
		&& IsValid(M_VehicleBalanceWidgets.NoteTitle) && IsValid(M_VehicleBalanceWidgets.NoteBody))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_VehicleBalanceWidgets", "UW_GameDifficultyPicker::GetAreVehicleBalanceWidgetsValid", this);
	return false;
}

void UW_GameDifficultyPicker::RefreshDifficultySelection() const
{
	if (not M_PendingChoice.bHasDifficulty)
	{
		return;
	}
	constexpr float SelectedOpacity = 1.f;
	constexpr float UnselectedOpacity = 0.4f;
	const TPair<UButton*, ERTSGameDifficulty> DifficultyButtons[] = {
		{M_NewToRTSDifficultyButton, ERTSGameDifficulty::NewToRTS},
		{M_NormalDifficultyButton, ERTSGameDifficulty::Normal},
		{M_HardDifficultyButton, ERTSGameDifficulty::Hard},
		{M_BrutalDifficultyButton, ERTSGameDifficulty::Brutal},
		{M_IronmanDifficultyButton, ERTSGameDifficulty::Ironman}
	};
	for (const TPair<UButton*, ERTSGameDifficulty>& DifficultyButton : DifficultyButtons)
	{
		if (IsValid(DifficultyButton.Key))
		{
			DifficultyButton.Key->SetRenderOpacity(DifficultyButton.Value == M_PendingChoice.Difficulty
				                                       ? SelectedOpacity
				                                       : UnselectedOpacity);
		}
	}
}

void UW_GameDifficultyPicker::TryContinueWithPendingChoice()
{
	if (not M_PendingChoice.bHasDifficulty || not M_PendingChoice.bHasVehicleBalance || M_PendingChoice.bHasContinued)
	{
		return;
	}
	M_PendingChoice.bHasContinued = true;
	OnDifficultyChosen(M_PendingChoice.DifficultyPercentage, M_PendingChoice.Difficulty);
}

void UW_GameDifficultyPicker::ConfigureForRestartCurrentMap()
{
	M_SelectionAction = EDifficultySelectionAction::RestartCurrentMap;
}

void UW_GameDifficultyPicker::OnNewToRTSDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::NewToRTS);
}

void UW_GameDifficultyPicker::OnNormalDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Normal);
}

void UW_GameDifficultyPicker::OnHardDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Hard);
}

void UW_GameDifficultyPicker::OnBrutalDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Brutal);
}

void UW_GameDifficultyPicker::OnIronmanDifficultyHovered()
{
	HandleDifficultyHovered(ERTSGameDifficulty::Ironman);
}

void UW_GameDifficultyPicker::OnNewToRTSDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnNormalDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnHardDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnBrutalDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnIronmanDifficultyUnhovered()
{
	HandleDifficultyUnhovered();
}

void UW_GameDifficultyPicker::OnNewToRTSDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::NewToRTS);
}

void UW_GameDifficultyPicker::OnNormalDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Normal);
}

void UW_GameDifficultyPicker::OnHardDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Hard);
}

void UW_GameDifficultyPicker::OnBrutalDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Brutal);
}

void UW_GameDifficultyPicker::OnIronmanDifficultyClicked()
{
	HandleDifficultyClicked(ERTSGameDifficulty::Ironman);
}

void UW_GameDifficultyPicker::HandleDifficultyHovered(const ERTSGameDifficulty HoveredDifficulty)
{
	SetDifficultyInfoBorderVisible(true);
	BP_OnDifficultyHovered(HoveredDifficulty);
}

void UW_GameDifficultyPicker::HandleDifficultyUnhovered()
{
	if (GetIsAnyDifficultyButtonHovered())
	{
		return;
	}

	SetDifficultyInfoBorderVisible(false);
}

void UW_GameDifficultyPicker::HandleDifficultyClicked(const ERTSGameDifficulty SelectedDifficulty)
{
	const URTSGameDifficultySettings* DifficultySettings = GetDefault<URTSGameDifficultySettings>();
	if (DifficultySettings == nullptr)
	{
		RTSFunctionLibrary::ReportError("Failed to get RTS game difficulty settings for selection.");
		return;
	}

	const int32* DifficultyPercentage = DifficultySettings->DifficultyPercentageByLevel.Find(SelectedDifficulty);
	if (DifficultyPercentage == nullptr)
	{
		const FString DifficultyAsString = UEnum::GetValueAsString(SelectedDifficulty);
		RTSFunctionLibrary::ReportError(
			"No difficulty percentage configured for: " + DifficultyAsString);
		return;
	}

	M_PendingChoice.bHasDifficulty = true;
	M_PendingChoice.Difficulty = SelectedDifficulty;
	M_PendingChoice.DifficultyPercentage = *DifficultyPercentage;
	RefreshDifficultySelection();
	// Unattended runs (map tests, offscreen captures) only click a difficulty; they keep the game instance's balance.
	if (FApp::IsUnattended())
	{
		M_PendingChoice.bHasVehicleBalance = true;
	}
	TryContinueWithPendingChoice();
}

void UW_GameDifficultyPicker::OnDifficultyChosen(const int32 DifficultyPercentage, const ERTSGameDifficulty SelectedDifficulty)
{
	switch (M_SelectionAction)
	{
	case EDifficultySelectionAction::SetCurrentMission:
		ApplyDifficultyToCurrentMission(DifficultyPercentage, SelectedDifficulty);
		return;
	case EDifficultySelectionAction::RestartCurrentMap:
		RestartCurrentMapWithDifficulty(DifficultyPercentage, SelectedDifficulty);
		return;
	}

	RTSFunctionLibrary::ReportError("Difficulty picker has an unsupported selection action.");
}

void UW_GameDifficultyPicker::ApplyDifficultyToCurrentMission(
	const int32 DifficultyPercentage,
	const ERTSGameDifficulty SelectedDifficulty)
{
	AMissionManager* MissionManager = FRTS_Statics::GetGameMissionManager(this);
	if (not MissionManager)
	{
		return;
	}

	MissionManager->SetMissionDifficulty(DifficultyPercentage, SelectedDifficulty);
	RemoveFromParent();
	ConditionalBeginDestroy();
}

void UW_GameDifficultyPicker::RestartCurrentMapWithDifficulty(
	const int32 DifficultyPercentage,
	const ERTSGameDifficulty SelectedDifficulty)
{
	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (CurrentLevelName.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Failed to restart with a new difficulty because the current level name was empty.");
		return;
	}

	URTSGameInstance* const GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not IsValid(GameInstance))
	{
		return;
	}

	FRTSGameDifficulty NewDifficulty;
	NewDifficulty.DifficultyLevel = SelectedDifficulty;
	NewDifficulty.DifficultyPercentage = DifficultyPercentage;
	NewDifficulty.bIsInitialized = true;
	GameInstance->SetSelectedGameDifficultyForRestart(NewDifficulty);

	RemoveFromParent();
	UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
}

void UW_GameDifficultyPicker::SetDifficultyInfoBorderVisible(const bool bIsVisible)
{
	if (not GetIsValidDifficultyInfoBorder())
	{
		return;
	}

	M_DifficultyInfoBorder->SetVisibility(bIsVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
}

bool UW_GameDifficultyPicker::GetIsAnyDifficultyButtonHovered() const
{
	if (not GetIsValidNewToRTSDifficultyButton()
		or not GetIsValidNormalDifficultyButton()
		or not GetIsValidHardDifficultyButton()
		or not GetIsValidBrutalDifficultyButton()
		or not GetIsValidIronmanDifficultyButton())
	{
		return false;
	}

	return M_NewToRTSDifficultyButton->IsHovered()
		|| M_NormalDifficultyButton->IsHovered()
		|| M_HardDifficultyButton->IsHovered()
		|| M_BrutalDifficultyButton->IsHovered()
		|| M_IronmanDifficultyButton->IsHovered();
}

bool UW_GameDifficultyPicker::GetIsValidNewToRTSDifficultyButton() const
{
	if (IsValid(M_NewToRTSDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_NewToRTSDifficultyButton", "UW_GameDifficultyPicker::GetIsValidNewToRTSDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidNormalDifficultyButton() const
{
	if (IsValid(M_NormalDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_NormalDifficultyButton", "UW_GameDifficultyPicker::GetIsValidNormalDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidHardDifficultyButton() const
{
	if (IsValid(M_HardDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_HardDifficultyButton", "UW_GameDifficultyPicker::GetIsValidHardDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidBrutalDifficultyButton() const
{
	if (IsValid(M_BrutalDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_BrutalDifficultyButton", "UW_GameDifficultyPicker::GetIsValidBrutalDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidIronmanDifficultyButton() const
{
	if (IsValid(M_IronmanDifficultyButton))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_IronmanDifficultyButton", "UW_GameDifficultyPicker::GetIsValidIronmanDifficultyButton", this);
	return false;
}

bool UW_GameDifficultyPicker::GetIsValidDifficultyInfoBorder() const
{
	if (IsValid(M_DifficultyInfoBorder))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_DifficultyInfoBorder", "UW_GameDifficultyPicker::GetIsValidDifficultyInfoBorder", this);
	return false;
}
