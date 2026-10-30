// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_SurvivalArmySelection.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/SurvivalMode/Army/SurvivalArmyLibrary.h"
#include "RTS_Survival/SurvivalMode/Data/SurvivalModeSettings.h"
#include "RTS_Survival/SurvivalMode/Director/SurvivalModeDirector.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Units/Variants/UnitVariantSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Slate/SlateBrushAsset.h"
#include "SurvivalArmyDragOperation.h"
#include "SurvivalUiLayout.h"
#include "SurvivalUiStyle.h"
#include "TimerManager.h"
#include "W_SurvivalArmyTab.h"
#include "W_SurvivalRosterEntry.h"

namespace SurvivalArmySelectionPrivate
{
	constexpr int32 HQSlotIndex = 0;
	// The game state keeps per-player unit data; the human player is player 1 everywhere in the project.
	constexpr int32 HumanPlayerIndex = 1;
	// Unit type and subtype give every unit its own stable wear seed (same scheme as the faction unit browser).
	constexpr int32 SubtypesPerUnitType = 256;
	// A slot area change smaller than this is layout noise, not a resize.
	constexpr float SlotAreaChangeTolerance = 0.5f;

	const TArray<ESurvivalRosterCategory> TabOrder = {
		ESurvivalRosterCategory::All, ESurvivalRosterCategory::Infantry, ESurvivalRosterCategory::Vehicles,
		ESurvivalRosterCategory::Nomadic, ESurvivalRosterCategory::Aircraft
	};

	void ApplyBrushIfSet(UBorder* Border, const FSlateBrush& Brush)
	{
		if (IsValid(Border) and IsValid(Brush.GetResourceObject()))
		{
			Border->SetBrush(Brush);
		}
	}

	void ApplyButtonStyleIfSet(UButton* Button, const FButtonStyle& ButtonStyle)
	{
		if (IsValid(Button) and IsValid(ButtonStyle.Normal.GetResourceObject()))
		{
			Button->SetStyle(ButtonStyle);
		}
	}

	void ApplyText(UTextBlock* Text, const FSlateFontInfo& Font, const FSlateColor& Color)
	{
		if (IsValid(Text))
		{
			UnitVariantSelectionWidgets::ApplyTextOnMetal(Text, Font, Color);
		}
	}

	void SetTextIfBound(UTextBlock* Text, const FText& Value)
	{
		if (IsValid(Text))
		{
			Text->SetText(Value);
		}
	}
}

// ====================================================================================================== Lifecycle

void UW_SurvivalArmySelection::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	ApplyStyleToBoundWidgets();
	ApplyStaticTexts();
	BindButtons();
}

void UW_SurvivalArmySelection::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (not IsDesignTime())
	{
		return;
	}
	ApplyStyleToBoundWidgets();
	ApplyStaticTexts();
	BuildDesignPreview();
}

void UW_SurvivalArmySelection::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (M_Slots.IsEmpty())
	{
		return;
	}
	const FVector2D AreaSize = GetSlotAreaSize();
	if (not AreaSize.Equals(M_LastSlotAreaSize, SurvivalArmySelectionPrivate::SlotAreaChangeTolerance))
	{
		LayoutSlotGrid();
	}
}

void UW_SurvivalArmySelection::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FeedbackTimerHandle);
	}
	Super::NativeDestruct();
}

FReply UW_SurvivalArmySelection::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Enter || Key == EKeys::Gamepad_Special_Right)
	{
		ConfirmArmy();
		return FReply::Handled();
	}
	if (Key == EKeys::Delete || Key == EKeys::BackSpace)
	{
		ClearArmy();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UW_SurvivalArmySelection::InitArmySelection(ASurvivalModeDirector* Director)
{
	M_Director = Director;
	if (not GetIsValidDirector())
	{
		return;
	}
	const USurvivalModeSettings* Settings = Director->GetSettings();
	if (not IsValid(Settings))
	{
		RTSFunctionLibrary::ReportError(TEXT("Survival army selection: the director has no settings asset."));
		return;
	}
	M_SupplyBudget = Settings->Army.SupplyBudget;
	M_TrainingOptionsTable = UUnitVariantPresentationLibrary::LoadTrainingOptionsTable();
	M_VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
	BuildRoster(*Director);
	RebuildTabs();
	RebuildRosterEntries();
	RebuildSlots(Settings->Army.SlotCount);
	SurvivalArmySelectionPrivate::SetTextIfBound(
		M_FactionText, UEnum::GetDisplayValueAsText(Director->GetPlayerFaction()).ToUpper());
	HandleTabClicked(ESurvivalRosterCategory::All);
	RefreshArmyViews();
	ClearFeedback();
}

// ====================================================================================================== Setup

void UW_SurvivalArmySelection::ApplyStyleToBoundWidgets()
{
	using namespace SurvivalUiLayout;
	using namespace SurvivalArmySelectionPrivate;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_StyleAsset))
	{
		return;
	}
	const FSurvivalUiStyle& Style = M_StyleAsset->Style;
	ApplyBrushIfSet(M_ScreenShade, Style.ScreenShade);
	ApplyBrushIfSet(M_HeaderPlate, Style.HeaderPlate);
	ApplyBrushIfSet(M_RosterPanel, Style.RosterPanel);
	ApplyBrushIfSet(M_ArmyPanel, Style.ArmyPanel);
	ApplyButtonStyleIfSet(M_ConfirmButton, Style.ConfirmButton);
	ApplyButtonStyleIfSet(M_ClearButton, Style.BackButton);
	if (IsValid(M_SupplyBar) and IsValid(Style.SupplyBar.FillImage.GetResourceObject()))
	{
		M_SupplyBar->SetWidgetStyle(Style.SupplyBar);
	}
	if (IsValid(Style.ScrollBar.VerticalBackgroundImage.GetResourceObject()))
	{
		for (UScrollBox* const Scroll : {M_RosterScroll.Get(), M_SlotScroll.Get()})
		{
			if (IsValid(Scroll))
			{
				Scroll->SetWidgetBarStyle(Style.ScrollBar);
			}
		}
	}
	ApplyText(M_TitleText, MakeFont(Style.HeadingFont, TitleFontSize, TitleLetterSpacing), Style.TextColor);
	ApplyText(M_FactionText, MakeFont(Style.HeadingFont, SubtitleFontSize, LabelLetterSpacing), Style.AccentColor);
	for (UTextBlock* const Label : {M_RosterTitleText.Get(), M_ArmyTitleText.Get(), M_SupplyLabelText.Get()})
	{
		ApplyText(Label, MakeFont(Style.HeadingFont, LabelFontSize, LabelLetterSpacing), Style.TextColor);
	}
	ApplyText(M_SlotCountText, MakeFont(Style.BodyFont, BodyFontSize), Style.MutedTextColor);
	ApplyText(M_SupplyText, MakeFont(Style.HeadingFont, LabelFontSize), Style.TextColor);
	ApplyText(M_FeedbackText, MakeFont(Style.BodyFont, BodyFontSize), Style.WarningColor);
	ApplyText(M_HintText, MakeFont(Style.BodyFont, BodyFontSize), Style.MutedTextColor);
	ApplyText(M_ConfirmText, MakeFont(Style.HeadingFont, ActionButtonFontSize, ActionButtonLetterSpacing),
	          Style.TextColor);
	ApplyText(M_ClearText, MakeFont(Style.HeadingFont, ActionButtonFontSize, ActionButtonLetterSpacing),
	          Style.TextColor);
}

void UW_SurvivalArmySelection::ApplyStaticTexts()
{
	using namespace SurvivalArmySelectionPrivate;
	SetTextIfBound(M_TitleText, NSLOCTEXT("SurvivalArmySelection", "Title", "STARTING ARMY"));
	SetTextIfBound(M_RosterTitleText, NSLOCTEXT("SurvivalArmySelection", "RosterTitle", "AVAILABLE UNITS"));
	SetTextIfBound(M_ArmyTitleText, NSLOCTEXT("SurvivalArmySelection", "ArmyTitle", "YOUR ARMY"));
	SetTextIfBound(M_SupplyLabelText, NSLOCTEXT("SurvivalArmySelection", "SupplyBarLabel", "SUPPLY"));
	SetTextIfBound(M_ConfirmText, NSLOCTEXT("SurvivalArmySelection", "Confirm", "CONFIRM"));
	SetTextIfBound(M_ClearText, NSLOCTEXT("SurvivalArmySelection", "Clear", "CLEAR"));
	// The Blueprint carries a sample refusal line for layout checks; it never shows in game.
	SetTextIfBound(M_FeedbackText, FText::GetEmpty());
	SetTextIfBound(M_HintText, NSLOCTEXT("SurvivalArmySelection", "Hint",
	                                     "Drag units into slots. Double-click adds, right-click removes."));
}

void UW_SurvivalArmySelection::BindButtons()
{
	if (IsValid(M_ConfirmButton))
	{
		M_ConfirmButton->OnClicked.RemoveAll(this);
		M_ConfirmButton->OnClicked.AddDynamic(this, &UW_SurvivalArmySelection::OnConfirmClicked);
	}
	if (IsValid(M_ClearButton))
	{
		M_ClearButton->OnClicked.RemoveAll(this);
		M_ClearButton->OnClicked.AddDynamic(this, &UW_SurvivalArmySelection::OnClearClicked);
	}
}

void UW_SurvivalArmySelection::BuildRoster(const ASurvivalModeDirector& Director)
{
	const USurvivalModeSettings* Settings = Director.GetSettings();
	const ERTSFaction Faction = Director.GetPlayerFaction();
	const TArray<FSurvivalArmyEntry> Entries = USurvivalArmyLibrary::BuildArmyRoster(
		Settings, Faction, Director.FindAsyncSpawner());
	M_Roster.Reset();
	bool bFoundHQ = false;
	for (const FSurvivalArmyEntry& Entry : Entries)
	{
		if (Entry.bIsHQ)
		{
			if (not bFoundHQ)
			{
				M_HQItem = MakeRosterItem(Entry);
				bFoundHQ = true;
			}
			continue;
		}
		M_Roster.Add(MakeRosterItem(Entry));
	}
	if (not bFoundHQ)
	{
		M_HQItem = MakeRosterItem(USurvivalArmyLibrary::BuildHQEntry(Settings, Faction));
	}
	// Grouped like the tabs, cheapest first inside a group, so the ALL list reads the same as the filtered ones.
	M_Roster.StableSort([](const FSurvivalRosterItem& Left, const FSurvivalRosterItem& Right)
	{
		if (Left.Category != Right.Category)
		{
			return static_cast<uint8>(Left.Category) < static_cast<uint8>(Right.Category);
		}
		return Left.Entry.SupplyCost < Right.Entry.SupplyCost;
	});
}

FSurvivalRosterItem UW_SurvivalArmySelection::MakeRosterItem(const FSurvivalArmyEntry& Entry)
{
	FSurvivalRosterItem Item;
	Item.Entry = Entry;
	if (Item.Entry.DisplayName.IsEmpty())
	{
		const FText TableTitle = UUnitVariantPresentationLibrary::FindTrainingTableTitle(
			M_TrainingOptionsTable, Entry.Unit);
		Item.Entry.DisplayName = TableTitle.IsEmpty()
			                         ? UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(Entry.Unit)
			                         : TableTitle;
	}
	Item.Icon = MakeIconBrush(Entry.Unit, Entry.Variant);
	Item.VariantName = MakeVariantName(Entry.Unit, Entry.Variant);
	Item.Tooltip = MakeTooltip(Item.Entry, Item.VariantName);
	Item.Category = SurvivalArmyUi::GetRosterCategory(Entry.Unit.UnitType);
	Item.WearSeed = static_cast<float>(static_cast<int32>(Entry.Unit.UnitType)
		* SurvivalArmySelectionPrivate::SubtypesPerUnitType + Entry.Unit.SubtypeValue);
	return Item;
}

FSlateBrush UW_SurvivalArmySelection::MakeIconBrush(const FTrainingOption& Unit, const EUnitVariant Variant)
{
	// Same order the training menu and the variant menu use: the variant's training brush override, then the
	// variant's selection icon, then the unit's DT_TrainingOptions picture.
	const FUnitVariantDefinition* Definition = nullptr;
	if (Variant != EUnitVariant::Base and IsValid(M_VariantDataAsset))
	{
		Definition = M_VariantDataAsset->FindVariantDefinition(UPlayerUnitVariantManager::GetSpawnerKey(Unit),
		                                                        Variant);
	}
	if (Definition and Definition->bOverrideTrainingImageBrush and IsValid(Definition->TrainingImageBrush))
	{
		return Definition->TrainingImageBrush->Brush;
	}
	if (Definition and not Definition->Icon.IsNull())
	{
		UTexture2D* const IconTexture = Definition->Icon.LoadSynchronous();
		if (IsValid(IconTexture))
		{
			M_LoadedIcons.AddUnique(IconTexture);
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(IconTexture);
			IconBrush.ImageSize = FVector2D(SurvivalUiLayout::SlotIconSize, SurvivalUiLayout::SlotIconSize);
			return IconBrush;
		}
	}
	const USlateBrushAsset* TableBrush = UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(
		M_TrainingOptionsTable, Unit);
	return IsValid(TableBrush) ? TableBrush->Brush : FSlateBrush();
}

FText UW_SurvivalArmySelection::MakeVariantName(const FTrainingOption& Unit, const EUnitVariant Variant) const
{
	if (Variant == EUnitVariant::Base)
	{
		return FText::GetEmpty();
	}
	const FUnitVariantDefinition* Definition = IsValid(M_VariantDataAsset)
		                                           ? M_VariantDataAsset->FindVariantDefinition(
			                                           UPlayerUnitVariantManager::GetSpawnerKey(Unit), Variant)
		                                           : nullptr;
	if (Definition and not Definition->DisplayName.IsEmpty())
	{
		return Definition->DisplayName;
	}
	return UEnum::GetDisplayValueAsText(Variant);
}

FText UW_SurvivalArmySelection::MakeTooltip(const FSurvivalArmyEntry& Entry, const FText& VariantName) const
{
	FString Tooltip = Entry.DisplayName.ToString();
	if (not VariantName.IsEmpty())
	{
		Tooltip += FString::Printf(TEXT("  [%s]"), *VariantName.ToString());
		const FUnitVariantDefinition* Definition = IsValid(M_VariantDataAsset)
			                                           ? M_VariantDataAsset->FindVariantDefinition(
				                                           UPlayerUnitVariantManager::GetSpawnerKey(Entry.Unit),
				                                           Entry.Variant)
			                                           : nullptr;
		if (Definition and not Definition->Description.IsEmpty())
		{
			Tooltip += TEXT("\n") + Definition->Description.ToString();
		}
	}
	const FText StatLine = MakeStatLine(Entry.Unit);
	if (not StatLine.IsEmpty())
	{
		Tooltip += TEXT("\n") + StatLine.ToString();
	}
	Tooltip += FText::Format(NSLOCTEXT("SurvivalArmySelection", "TooltipSupply", "\nSupply {0}"),
	                         FText::AsNumber(Entry.SupplyCost)).ToString();
	if (Entry.MaxCopies > 0)
	{
		Tooltip += FText::Format(NSLOCTEXT("SurvivalArmySelection", "TooltipMax", "  (max {0})"),
		                         FText::AsNumber(Entry.MaxCopies)).ToString();
	}
	return FText::FromString(Tooltip);
}

FText UW_SurvivalArmySelection::MakeStatLine(const FTrainingOption& Unit) const
{
	using namespace SurvivalArmySelectionPrivate;
	const ACPPGameState* GameState = FRTS_Statics::GetGameState(this);
	if (not IsValid(GameState))
	{
		return FText::GetEmpty();
	}
	FNumberFormattingOptions WholeNumbers;
	WholeNumbers.MaximumFractionalDigits = 0;
	if (Unit.UnitType == EAllUnitType::UNType_Tank || Unit.UnitType == EAllUnitType::UNType_Harvester)
	{
		const FTankData TankData = GameState->GetTankDataOfPlayer(
			HumanPlayerIndex, static_cast<ETankSubtype>(Unit.SubtypeValue));
		return FText::Format(NSLOCTEXT("SurvivalArmySelection", "TankStats", "Health {0} | {1} km/h"),
		                     FText::AsNumber(TankData.MaxHealth, &WholeNumbers),
		                     FText::AsNumber(TankData.VehicleMaxSpeedKmh, &WholeNumbers));
	}
	if (Unit.UnitType == EAllUnitType::UNType_Squad)
	{
		const FSquadData SquadData = GameState->GetSquadDataOfPlayer(
			HumanPlayerIndex, static_cast<ESquadSubtype>(Unit.SubtypeValue));
		return FText::Format(NSLOCTEXT("SurvivalArmySelection", "SquadStats", "Health {0} per soldier"),
		                     FText::AsNumber(SquadData.MaxHealth, &WholeNumbers));
	}
	return FText::GetEmpty();
}

void UW_SurvivalArmySelection::RebuildTabs()
{
	using namespace SurvivalUiLayout;
	M_Tabs.Reset();
	if (not IsValid(M_TabRow))
	{
		return;
	}
	M_TabRow->ClearChildren();
	for (const ESurvivalRosterCategory Category : SurvivalArmySelectionPrivate::TabOrder)
	{
		const bool bHasUnits = Category == ESurvivalRosterCategory::All
			                       || M_Roster.ContainsByPredicate([Category](const FSurvivalRosterItem& Item)
			                       {
				                       return Item.Category == Category;
			                       });
		if (not bHasUnits)
		{
			continue;
		}
		UW_SurvivalArmyTab* const Tab = CreateWidget<UW_SurvivalArmyTab>(this, UW_SurvivalArmyTab::StaticClass());
		if (not IsValid(Tab))
		{
			continue;
		}
		Tab->SetupTab(this, Category, M_StyleAsset);
		UHorizontalBoxSlot* const TabSlot = M_TabRow->AddChildToHorizontalBox(Tab);
		TabSlot->SetPadding(FMargin(M_Tabs.IsEmpty() ? 0.f : TabSpacing, 0.f, 0.f, 0.f));
		M_Tabs.Add(Tab);
	}
}

void UW_SurvivalArmySelection::RebuildRosterEntries()
{
	using namespace SurvivalUiLayout;
	M_RosterEntries.Reset();
	if (not GetIsValidRosterScroll())
	{
		return;
	}
	M_RosterScroll->ClearChildren();
	for (int32 RosterIndex = 0; RosterIndex < M_Roster.Num(); ++RosterIndex)
	{
		UW_SurvivalRosterEntry* const Entry = CreateWidget<UW_SurvivalRosterEntry>(
			this, UW_SurvivalRosterEntry::StaticClass());
		if (not IsValid(Entry))
		{
			continue;
		}
		Entry->SetupEntry(this, RosterIndex, M_Roster[RosterIndex], M_StyleAsset);
		if (UScrollBoxSlot* const EntrySlot = Cast<UScrollBoxSlot>(M_RosterScroll->AddChild(Entry)))
		{
			EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, RosterEntrySpacing));
			EntrySlot->SetHorizontalAlignment(HAlign_Center);
		}
		M_RosterEntries.Add(Entry);
	}
}

void UW_SurvivalArmySelection::RebuildSlots(const int32 SlotCount)
{
	using namespace SurvivalUiLayout;
	M_Slots.Reset();
	M_SlotRosterIndices.Init(INDEX_NONE, FMath::Max(SlotCount, 1));
	if (not GetIsValidSlotGrid())
	{
		return;
	}
	M_SlotGrid->ClearChildren();
	M_SlotGrid->SetSlotPadding(FMargin(SlotGap * 0.5f));
	// The grid sits centred in the scroll box, so a small army forms a tidy block instead of hugging the left.
	if (UScrollBoxSlot* const GridSlot = Cast<UScrollBoxSlot>(M_SlotGrid->Slot))
	{
		GridSlot->SetHorizontalAlignment(HAlign_Center);
	}
	for (int32 SlotIndex = 0; SlotIndex < M_SlotRosterIndices.Num(); ++SlotIndex)
	{
		UW_SurvivalArmySlot* const ArmySlot = CreateWidget<UW_SurvivalArmySlot>(
			this, UW_SurvivalArmySlot::StaticClass());
		if (not IsValid(ArmySlot))
		{
			continue;
		}
		ArmySlot->SetupSlot(this, SlotIndex, M_StyleAsset);
		M_SlotGrid->AddChildToUniformGrid(ArmySlot, 0, SlotIndex);
		M_Slots.Add(ArmySlot);
	}
	LayoutSlotGrid();
}

void UW_SurvivalArmySelection::BuildDesignPreview()
{
	// Placeholder data so the designer and analyze_widget_layout see a filled screen; never runs in game.
	M_Roster.Reset();
	const TArray<FString> PreviewNames = {
		TEXT("Panzer IV H"), TEXT("Panzergrenadiers"), TEXT("Sd.Kfz. 251"), TEXT("Tiger H1"),
		TEXT("Pak 38 Team"), TEXT("Puma"), TEXT("Bf 109"), TEXT("Harvester")
	};
	for (int32 Index = 0; Index < M_DesignPreviewRosterCount; ++Index)
	{
		FSurvivalRosterItem Item;
		Item.Entry.DisplayName = FText::FromString(PreviewNames[Index % PreviewNames.Num()]);
		Item.Entry.SupplyCost = 4 + Index * 3;
		Item.VariantName = Index % 3 == 0 ? FText::FromString(TEXT("Variant A")) : FText::GetEmpty();
		M_Roster.Add(Item);
	}
	M_HQItem = FSurvivalRosterItem();
	M_HQItem.Entry.DisplayName = FText::FromString(TEXT("Command Vehicle"));
	M_HQItem.Entry.bIsHQ = true;
	M_SupplyBudget = 100;
	M_Tabs.Reset();
	if (IsValid(M_TabRow))
	{
		M_TabRow->ClearChildren();
		for (const ESurvivalRosterCategory Category : SurvivalArmySelectionPrivate::TabOrder)
		{
			UW_SurvivalArmyTab* const Tab = CreateWidget<UW_SurvivalArmyTab>(this, UW_SurvivalArmyTab::StaticClass());
			if (not IsValid(Tab))
			{
				continue;
			}
			Tab->SetupTab(this, Category, M_StyleAsset);
			Tab->SetIsSelected(Category == ESurvivalRosterCategory::All);
			M_TabRow->AddChildToHorizontalBox(Tab)->SetPadding(
				FMargin(M_Tabs.IsEmpty() ? 0.f : SurvivalUiLayout::TabSpacing, 0.f, 0.f, 0.f));
			M_Tabs.Add(Tab);
		}
	}
	RebuildRosterEntries();
	RebuildSlots(M_DesignPreviewSlotCount);
	// Fill a third of the slots so filled and empty looks are both visible.
	for (int32 SlotIndex = 1; SlotIndex < M_SlotRosterIndices.Num() and not M_Roster.IsEmpty(); SlotIndex += 3)
	{
		M_SlotRosterIndices[SlotIndex] = SlotIndex % M_Roster.Num();
	}
	RefreshArmyViews();
}

// ====================================================================================================== Slot grid

FVector2D UW_SurvivalArmySelection::GetSlotAreaSize() const
{
	using namespace SurvivalUiLayout;
	const FVector2D Measured = IsValid(M_SlotScroll)
		                           ? M_SlotScroll->GetCachedGeometry().GetLocalSize()
		                           : FVector2D::ZeroVector;
	// Not laid out yet (first frame, or the widget designer): the reference layout's area.
	if (Measured.X < SlotMinCellWidth || Measured.Y < SlotMinCellWidth)
	{
		return FVector2D(DefaultSlotAreaWidth, DefaultSlotAreaHeight);
	}
	return Measured;
}

void UW_SurvivalArmySelection::LayoutSlotGrid()
{
	using namespace SurvivalUiLayout;
	if (not GetIsValidSlotGrid())
	{
		return;
	}
	M_LastSlotAreaSize = GetSlotAreaSize();
	// The uniform grid puts half a gap around every cell, so one gap of the area is padding, not cells.
	const float GridWidth = M_LastSlotAreaSize.X - SlotScrollBarReserve - SlotGap;
	const float GridHeight = M_LastSlotAreaSize.Y - SlotGap;
	const FSlotGridLayout GridLayout = ComputeSlotGridLayout(M_Slots.Num(), GridWidth, GridHeight);
	M_CellWidth = GridLayout.CellWidth;
	M_SlotGrid->SetMinDesiredSlotWidth(GridLayout.CellWidth);
	M_SlotGrid->SetMinDesiredSlotHeight(GridLayout.CellHeight);
	for (int32 SlotIndex = 0; SlotIndex < M_Slots.Num(); ++SlotIndex)
	{
		UW_SurvivalArmySlot* const ArmySlot = M_Slots[SlotIndex];
		if (not IsValid(ArmySlot))
		{
			continue;
		}
		ArmySlot->SetCellWidth(GridLayout.CellWidth);
		if (UUniformGridSlot* const GridSlot = Cast<UUniformGridSlot>(ArmySlot->Slot))
		{
			GridSlot->SetRow(SlotIndex / GridLayout.Columns);
			GridSlot->SetColumn(SlotIndex % GridLayout.Columns);
		}
	}
}

// ====================================================================================================== Army rules

bool UW_SurvivalArmySelection::GetCanPlaceRosterItem(const int32 RosterIndex, const int32 SlotIndex,
                                                     FText& OutReason) const
{
	using namespace SurvivalArmySelectionPrivate;
	if (not M_Roster.IsValidIndex(RosterIndex) || not M_SlotRosterIndices.IsValidIndex(SlotIndex))
	{
		return false;
	}
	if (SlotIndex == HQSlotIndex)
	{
		OutReason = NSLOCTEXT("SurvivalArmySelection", "HqLockedReason", "The first slot always holds your HQ.");
		return false;
	}
	const FSurvivalRosterItem& Item = M_Roster[RosterIndex];
	const int32 ReplacedRosterIndex = M_SlotRosterIndices[SlotIndex];
	const int32 OtherCopies = GetCopiesInArmy(RosterIndex) - (ReplacedRosterIndex == RosterIndex ? 1 : 0);
	if (Item.Entry.MaxCopies > 0 and OtherCopies >= Item.Entry.MaxCopies)
	{
		OutReason = FText::Format(NSLOCTEXT("SurvivalArmySelection", "MaxCopiesReason",
		                                    "Your army can hold at most {0} x {1}."),
		                          FText::AsNumber(Item.Entry.MaxCopies), Item.Entry.DisplayName);
		return false;
	}
	const int32 SupplyAfter = GetUsedSupply() - GetSupplyOfSlot(SlotIndex) + Item.Entry.SupplyCost;
	if (SupplyAfter > M_SupplyBudget)
	{
		const int32 FreeSupply = M_SupplyBudget - GetUsedSupply() + GetSupplyOfSlot(SlotIndex);
		OutReason = FText::Format(NSLOCTEXT("SurvivalArmySelection", "SupplyReason",
		                                    "Not enough supply: {0} costs {1}, {2} left."),
		                          Item.Entry.DisplayName, FText::AsNumber(Item.Entry.SupplyCost),
		                          FText::AsNumber(FMath::Max(0, FreeSupply)));
		return false;
	}
	return true;
}

bool UW_SurvivalArmySelection::GetCanMoveSlot(const int32 SourceSlotIndex, const int32 TargetSlotIndex,
                                              FText& OutReason) const
{
	using namespace SurvivalArmySelectionPrivate;
	if (not M_SlotRosterIndices.IsValidIndex(SourceSlotIndex) || not M_SlotRosterIndices.IsValidIndex(TargetSlotIndex))
	{
		return false;
	}
	if (SourceSlotIndex == HQSlotIndex || TargetSlotIndex == HQSlotIndex)
	{
		OutReason = NSLOCTEXT("SurvivalArmySelection", "HqLockedReason", "The first slot always holds your HQ.");
		return false;
	}
	// A move or swap keeps the same units, so supply and copies cannot change.
	return true;
}

int32 UW_SurvivalArmySelection::GetCopiesInArmy(const int32 RosterIndex) const
{
	int32 Copies = 0;
	for (const int32 SlotRosterIndex : M_SlotRosterIndices)
	{
		Copies += SlotRosterIndex == RosterIndex ? 1 : 0;
	}
	return Copies;
}

int32 UW_SurvivalArmySelection::GetSupplyOfSlot(const int32 SlotIndex) const
{
	if (not M_SlotRosterIndices.IsValidIndex(SlotIndex))
	{
		return 0;
	}
	const int32 RosterIndex = M_SlotRosterIndices[SlotIndex];
	return M_Roster.IsValidIndex(RosterIndex) ? M_Roster[RosterIndex].Entry.SupplyCost : 0;
}

int32 UW_SurvivalArmySelection::GetUsedSupply() const
{
	int32 UsedSupply = 0;
	for (int32 SlotIndex = 0; SlotIndex < M_SlotRosterIndices.Num(); ++SlotIndex)
	{
		UsedSupply += GetSupplyOfSlot(SlotIndex);
	}
	return UsedSupply;
}

int32 UW_SurvivalArmySelection::FindFirstEmptySlot() const
{
	for (int32 SlotIndex = SurvivalArmySelectionPrivate::HQSlotIndex + 1; SlotIndex < M_SlotRosterIndices.Num();
	     ++SlotIndex)
	{
		if (M_SlotRosterIndices[SlotIndex] == INDEX_NONE)
		{
			return SlotIndex;
		}
	}
	return INDEX_NONE;
}

TArray<FSurvivalArmyPick> UW_SurvivalArmySelection::BuildPicks() const
{
	using namespace SurvivalArmySelectionPrivate;
	TArray<FSurvivalArmyPick> Picks;
	FSurvivalArmyPick HQPick;
	HQPick.SlotIndex = HQSlotIndex;
	HQPick.Entry = M_HQItem.Entry;
	Picks.Add(HQPick);
	for (int32 SlotIndex = HQSlotIndex + 1; SlotIndex < M_SlotRosterIndices.Num(); ++SlotIndex)
	{
		const int32 RosterIndex = M_SlotRosterIndices[SlotIndex];
		if (not M_Roster.IsValidIndex(RosterIndex))
		{
			continue;
		}
		FSurvivalArmyPick Pick;
		Pick.SlotIndex = SlotIndex;
		Pick.Entry = M_Roster[RosterIndex].Entry;
		Picks.Add(Pick);
	}
	return Picks;
}

// ====================================================================================================== Child input

void UW_SurvivalArmySelection::HandleTabClicked(const ESurvivalRosterCategory Category)
{
	M_SelectedCategory = Category;
	for (UW_SurvivalArmyTab* const Tab : M_Tabs)
	{
		if (IsValid(Tab))
		{
			Tab->SetIsSelected(Tab->GetCategory() == Category);
		}
	}
	ApplyRosterFilter();
	if (IsValid(M_RosterScroll))
	{
		M_RosterScroll->ScrollToStart();
	}
}

void UW_SurvivalArmySelection::HandleRosterEntryActivated(const int32 RosterIndex)
{
	const int32 SlotIndex = FindFirstEmptySlot();
	if (SlotIndex == INDEX_NONE)
	{
		ShowFeedback(FText::Format(NSLOCTEXT("SurvivalArmySelection", "SlotsFullReason",
		                                     "All {0} slots are full. Remove a unit first."),
		                           FText::AsNumber(M_SlotRosterIndices.Num())), true);
		return;
	}
	FText Reason;
	if (not GetCanPlaceRosterItem(RosterIndex, SlotIndex, Reason))
	{
		ShowFeedback(Reason, true);
		return;
	}
	M_SlotRosterIndices[SlotIndex] = RosterIndex;
	ClearFeedback();
	RefreshArmyViews();
}

void UW_SurvivalArmySelection::HandleSlotRemoveRequested(const int32 SlotIndex)
{
	if (SlotIndex == SurvivalArmySelectionPrivate::HQSlotIndex || not M_SlotRosterIndices.IsValidIndex(SlotIndex))
	{
		return;
	}
	M_SlotRosterIndices[SlotIndex] = INDEX_NONE;
	RefreshArmyViews();
}

void UW_SurvivalArmySelection::HandleDropOnSlot(const USurvivalArmyDragOperation* Operation, const int32 SlotIndex)
{
	if (not IsValid(Operation))
	{
		return;
	}
	FText Reason;
	if (Operation->GetIsFromSlot())
	{
		if (GetCanMoveSlot(Operation->SourceSlotIndex, SlotIndex, Reason))
		{
			M_SlotRosterIndices.Swap(Operation->SourceSlotIndex, SlotIndex);
			ClearFeedback();
		}
		else
		{
			ShowFeedback(Reason, true);
		}
		// Also restores the faded source slot.
		RefreshArmyViews();
		return;
	}
	if (GetCanPlaceRosterItem(Operation->RosterIndex, SlotIndex, Reason))
	{
		M_SlotRosterIndices[SlotIndex] = Operation->RosterIndex;
		ClearFeedback();
	}
	else
	{
		ShowFeedback(Reason, true);
	}
	RefreshArmyViews();
}

ESurvivalSlotDropPreview UW_SurvivalArmySelection::GetDropPreview(const USurvivalArmyDragOperation* Operation,
                                                                  const int32 SlotIndex) const
{
	if (not IsValid(Operation))
	{
		return ESurvivalSlotDropPreview::None;
	}
	FText UnusedReason;
	const bool bAccepted = Operation->GetIsFromSlot()
		                       ? GetCanMoveSlot(Operation->SourceSlotIndex, SlotIndex, UnusedReason)
		                       : GetCanPlaceRosterItem(Operation->RosterIndex, SlotIndex, UnusedReason);
	return bAccepted ? ESurvivalSlotDropPreview::Accept : ESurvivalSlotDropPreview::Refuse;
}

UDragDropOperation* UW_SurvivalArmySelection::CreateDragOperationFromRoster(const int32 RosterIndex)
{
	if (not M_Roster.IsValidIndex(RosterIndex))
	{
		return nullptr;
	}
	USurvivalArmyDragOperation* const Operation = NewObject<USurvivalArmyDragOperation>(this);
	Operation->RosterIndex = RosterIndex;
	Operation->SourceSlotIndex = INDEX_NONE;
	Operation->Pivot = EDragPivot::CenterCenter;
	UW_SurvivalArmySlot* const Visual = CreateWidget<UW_SurvivalArmySlot>(this, UW_SurvivalArmySlot::StaticClass());
	if (IsValid(Visual))
	{
		Visual->SetupSlot(nullptr, INDEX_NONE, M_StyleAsset);
		Visual->SetSlotContent(&M_Roster[RosterIndex], false);
		Visual->SetCellWidth(M_CellWidth > 0.f ? M_CellWidth : SurvivalUiLayout::SlotNativeWidth);
		Visual->SetIsDragVisual();
		Operation->DefaultDragVisual = Visual;
	}
	return Operation;
}

UDragDropOperation* UW_SurvivalArmySelection::CreateDragOperationFromSlot(const int32 SlotIndex)
{
	if (SlotIndex == SurvivalArmySelectionPrivate::HQSlotIndex || not M_SlotRosterIndices.IsValidIndex(SlotIndex))
	{
		return nullptr;
	}
	UDragDropOperation* const Operation = CreateDragOperationFromRoster(M_SlotRosterIndices[SlotIndex]);
	if (USurvivalArmyDragOperation* const ArmyOperation = Cast<USurvivalArmyDragOperation>(Operation))
	{
		ArmyOperation->SourceSlotIndex = SlotIndex;
	}
	return Operation;
}

// ====================================================================================================== Actions

void UW_SurvivalArmySelection::ClearArmy()
{
	for (int32 SlotIndex = SurvivalArmySelectionPrivate::HQSlotIndex + 1; SlotIndex < M_SlotRosterIndices.Num();
	     ++SlotIndex)
	{
		M_SlotRosterIndices[SlotIndex] = INDEX_NONE;
	}
	ClearFeedback();
	RefreshArmyViews();
}

void UW_SurvivalArmySelection::ConfirmArmy()
{
	if (bM_IsConfirmPending || not GetIsValidDirector())
	{
		return;
	}
	FText Reason;
	if (not M_Director->ConfirmArmy(BuildPicks(), Reason))
	{
		ShowFeedback(Reason.IsEmpty()
			             ? NSLOCTEXT("SurvivalArmySelection", "ConfirmRefused", "This army cannot be deployed.")
			             : Reason, true);
		return;
	}
	// The director moves on and removes this widget; ignore further clicks until it does.
	bM_IsConfirmPending = true;
	if (IsValid(M_ConfirmButton))
	{
		M_ConfirmButton->SetIsEnabled(false);
	}
}

void UW_SurvivalArmySelection::OnConfirmClicked()
{
	ConfirmArmy();
}

void UW_SurvivalArmySelection::OnClearClicked()
{
	ClearArmy();
}

// ====================================================================================================== Views

void UW_SurvivalArmySelection::RefreshArmyViews()
{
	RefreshSlots();
	RefreshSupply();
	RefreshRosterStates();
}

void UW_SurvivalArmySelection::RefreshSlots()
{
	int32 FilledSlots = 0;
	for (int32 SlotIndex = 0; SlotIndex < M_Slots.Num(); ++SlotIndex)
	{
		UW_SurvivalArmySlot* const ArmySlot = M_Slots[SlotIndex];
		if (not IsValid(ArmySlot) || not M_SlotRosterIndices.IsValidIndex(SlotIndex))
		{
			continue;
		}
		const bool bIsHQSlot = SlotIndex == SurvivalArmySelectionPrivate::HQSlotIndex;
		const int32 RosterIndex = M_SlotRosterIndices[SlotIndex];
		const FSurvivalRosterItem* const Item = bIsHQSlot
			                                        ? &M_HQItem
			                                        : (M_Roster.IsValidIndex(RosterIndex)
				                                           ? &M_Roster[RosterIndex]
				                                           : nullptr);
		ArmySlot->SetSlotContent(Item, bIsHQSlot);
		FilledSlots += Item ? 1 : 0;
	}
	SurvivalArmySelectionPrivate::SetTextIfBound(
		M_SlotCountText, FText::Format(NSLOCTEXT("SurvivalArmySelection", "SlotCount", "{0} / {1} SLOTS"),
		                               FText::AsNumber(FilledSlots), FText::AsNumber(M_SlotRosterIndices.Num())));
}

void UW_SurvivalArmySelection::RefreshSupply()
{
	const int32 UsedSupply = GetUsedSupply();
	const bool bIsFull = M_SupplyBudget > 0 and UsedSupply >= M_SupplyBudget;
	const FSurvivalUiStyle& Style = GetStyle();
	if (IsValid(M_SupplyBar))
	{
		M_SupplyBar->SetPercent(M_SupplyBudget > 0
			                        ? FMath::Clamp(static_cast<float>(UsedSupply) / M_SupplyBudget, 0.f, 1.f)
			                        : 0.f);
		M_SupplyBar->SetFillColorAndOpacity(bIsFull ? Style.WarningColor.GetSpecifiedColor() : FLinearColor::White);
	}
	if (IsValid(M_SupplyText))
	{
		M_SupplyText->SetText(FText::Format(NSLOCTEXT("SurvivalArmySelection", "SupplyValue", "{0} / {1}"),
		                                    FText::AsNumber(UsedSupply), FText::AsNumber(M_SupplyBudget)));
		M_SupplyText->SetColorAndOpacity(bIsFull ? Style.WarningColor : Style.TextColor);
	}
}

void UW_SurvivalArmySelection::RefreshRosterStates()
{
	const int32 FreeSupply = M_SupplyBudget - GetUsedSupply();
	const bool bHasEmptySlot = FindFirstEmptySlot() != INDEX_NONE;
	for (UW_SurvivalRosterEntry* const Entry : M_RosterEntries)
	{
		if (not IsValid(Entry) || not M_Roster.IsValidIndex(Entry->GetRosterIndex()))
		{
			continue;
		}
		const FSurvivalArmyEntry& ArmyEntry = M_Roster[Entry->GetRosterIndex()].Entry;
		const int32 Copies = GetCopiesInArmy(Entry->GetRosterIndex());
		const bool bAtMax = ArmyEntry.MaxCopies > 0 and Copies >= ArmyEntry.MaxCopies;
		Entry->SetArmyState(Copies, bHasEmptySlot and not bAtMax and ArmyEntry.SupplyCost <= FreeSupply);
	}
}

void UW_SurvivalArmySelection::ApplyRosterFilter()
{
	for (UW_SurvivalRosterEntry* const Entry : M_RosterEntries)
	{
		if (not IsValid(Entry) || not M_Roster.IsValidIndex(Entry->GetRosterIndex()))
		{
			continue;
		}
		const bool bShown = M_SelectedCategory == ESurvivalRosterCategory::All
			|| M_Roster[Entry->GetRosterIndex()].Category == M_SelectedCategory;
		Entry->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UW_SurvivalArmySelection::ShowFeedback(const FText& Message, const bool bIsWarning)
{
	if (not IsValid(M_FeedbackText))
	{
		return;
	}
	const FSurvivalUiStyle& Style = GetStyle();
	M_FeedbackText->SetText(Message);
	M_FeedbackText->SetColorAndOpacity(bIsWarning ? Style.WarningColor : Style.TextColor);
	M_FeedbackText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UWorld* const World = GetWorld();
	if (not World)
	{
		return;
	}
	TWeakObjectPtr<UW_SurvivalArmySelection> WeakThis(this);
	World->GetTimerManager().SetTimer(M_FeedbackTimerHandle, [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->ClearFeedback();
		}
	}, SurvivalUiLayout::FeedbackSeconds, false);
}

void UW_SurvivalArmySelection::ClearFeedback()
{
	if (IsValid(M_FeedbackText))
	{
		M_FeedbackText->SetText(FText::GetEmpty());
	}
}

// ====================================================================================================== Validators

const FSurvivalUiStyle& UW_SurvivalArmySelection::GetStyle() const
{
	static const FSurvivalUiStyle DefaultStyle;
	return IsValid(M_StyleAsset) ? M_StyleAsset->Style : DefaultStyle;
}

bool UW_SurvivalArmySelection::GetIsValidDirector() const
{
	if (M_Director.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Director", "GetIsValidDirector", this);
	return false;
}

bool UW_SurvivalArmySelection::GetIsValidSlotGrid() const
{
	if (IsValid(M_SlotGrid))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_SlotGrid", "GetIsValidSlotGrid", this);
	return false;
}

bool UW_SurvivalArmySelection::GetIsValidRosterScroll() const
{
	if (IsValid(M_RosterScroll))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_RosterScroll", "GetIsValidRosterScroll",
	                                                             this);
	return false;
}
