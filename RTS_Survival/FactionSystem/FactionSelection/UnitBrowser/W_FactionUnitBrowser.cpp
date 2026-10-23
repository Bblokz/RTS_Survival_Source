// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_FactionUnitBrowser.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "FactionUnitBrowserLayout.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/FactionSystem/FactionSelection/W_FactionSelectionMenu.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"
#include "W_FactionUnitTab.h"
#include "W_FactionUnitTile.h"

void UW_FactionUnitBrowser::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	M_TrainingOptionsTable = UUnitVariantPresentationLibrary::LoadTrainingOptionsTable();
	BuildWidgetTree();
}

void UW_FactionUnitBrowser::BuildWidgetTree()
{
	using namespace FactionUnitBrowserLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(BrowserWidth);
	SizeBox->SetHeightOverride(BrowserHeight);
	WidgetTree->RootWidget = SizeBox;

	UBorder* const Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrush(M_Style.Panel);
	Panel->SetPadding(FMargin(0.f));
	Panel->SetHorizontalAlignment(HAlign_Fill);
	Panel->SetVerticalAlignment(VAlign_Fill);
	SizeBox->SetContent(Panel);

	UOverlay* const PanelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Panel->SetContent(PanelOverlay);
	BuildHeader(PanelOverlay);
	BuildTabRow(PanelOverlay);
	BuildTileScroll(PanelOverlay);
}

void UW_FactionUnitBrowser::BuildHeader(UOverlay* PanelOverlay)
{
	using namespace FactionUnitBrowserLayout;
	USizeBox* const TitleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	TitleBox->SetHeightOverride(HeaderHeight);
	TitleBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const TitleSlot = PanelOverlay->AddChildToOverlay(TitleBox);
	TitleSlot->SetHorizontalAlignment(HAlign_Left);
	TitleSlot->SetVerticalAlignment(VAlign_Top);
	TitleSlot->SetPadding(TitleOffset);
	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.HeadingFont, TitleFontSize, TitleLetterSpacing),
		M_Style.TextColor);
	M_TitleText->SetText(NSLOCTEXT("FactionUnitBrowser", "Title", "FACTION UNITS"));
	TitleBox->SetContent(M_TitleText);
	if (USizeBoxSlot* const TitleTextSlot = Cast<USizeBoxSlot>(M_TitleText->Slot))
	{
		TitleTextSlot->SetVerticalAlignment(VAlign_Center);
	}

}

void UW_FactionUnitBrowser::BuildTabRow(UOverlay* PanelOverlay)
{
	using namespace FactionUnitBrowserLayout;
	M_TabRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UOverlaySlot* const TabRowSlot = PanelOverlay->AddChildToOverlay(M_TabRow);
	TabRowSlot->SetHorizontalAlignment(HAlign_Left);
	TabRowSlot->SetVerticalAlignment(VAlign_Top);
	TabRowSlot->SetPadding(TabRowOffset);
}

void UW_FactionUnitBrowser::BuildTileScroll(UOverlay* PanelOverlay)
{
	using namespace FactionUnitBrowserLayout;
	// Smoked glass sheet behind the tiles; it hides the panel's metal grain where no tile covers it.
	USizeBox* const GlassBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	GlassBox->SetWidthOverride(GlassWidth);
	GlassBox->SetHeightOverride(GlassHeight);
	GlassBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const GlassSlot = PanelOverlay->AddChildToOverlay(GlassBox);
	GlassSlot->SetHorizontalAlignment(HAlign_Left);
	GlassSlot->SetVerticalAlignment(VAlign_Top);
	GlassSlot->SetPadding(GlassOffset);
	UImage* const Glass = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Glass->SetBrush(M_Style.TileRowGlass);
	GlassBox->SetContent(Glass);

	USizeBox* const RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RowBox->SetWidthOverride(TileRowWidth);
	RowBox->SetHeightOverride(TileRowHeight);
	UOverlaySlot* const RowSlot = PanelOverlay->AddChildToOverlay(RowBox);
	RowSlot->SetHorizontalAlignment(HAlign_Left);
	RowSlot->SetVerticalAlignment(VAlign_Top);
	RowSlot->SetPadding(TileRowOffset);

	M_TileScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	M_TileScroll->SetOrientation(Orient_Horizontal);
	M_TileScroll->SetWidgetStyle(M_Style.TileRow);
	M_TileScroll->SetWidgetBarStyle(M_Style.TileRowScrollBar);
	M_TileScroll->SetScrollbarThickness(FVector2D(ScrollBarThickness, ScrollBarThickness));
	M_TileScroll->SetScrollbarPadding(ScrollBarPadding);
	// Slate hides the bar on its own when every tile fits.
	M_TileScroll->SetScrollBarVisibility(ESlateVisibility::Visible);
	M_TileScroll->SetAnimateWheelScrolling(true);
	M_TileScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	RowBox->SetContent(M_TileScroll);
}

void UW_FactionUnitBrowser::SetupBrowser(UW_FactionSelectionMenu* Menu)
{
	M_Menu = Menu;
}

void UW_FactionUnitBrowser::SetUnitGroups(const TArray<FFactionUnitBrowserGroup>& Groups)
{
	M_Groups.Reset();
	for (const FFactionUnitBrowserGroup& Group : Groups)
	{
		if (not Group.Units.IsEmpty())
		{
			M_Groups.Add(Group);
		}
	}
	RebuildTabs();
	HandleTabClicked(0);
}

void UW_FactionUnitBrowser::HandleTabClicked(const int32 TabIndex)
{
	if (not M_Groups.IsValidIndex(TabIndex))
	{
		M_SelectedTabIndex = INDEX_NONE;
		M_ShownUnits.Reset();
		RebuildTiles();
		return;
	}
	M_SelectedTabIndex = TabIndex;
	for (int32 Index = 0; Index < M_Tabs.Num(); ++Index)
	{
		if (IsValid(M_Tabs[Index]))
		{
			M_Tabs[Index]->SetIsSelected(Index == TabIndex);
		}
	}
	M_ShownUnits = M_Groups[TabIndex].Units;
	RebuildTiles();
	// Like the old filter buttons: switching the filter previews its first unit.
	HandleTileClicked(0);
}

void UW_FactionUnitBrowser::HandleTileClicked(const int32 TileIndex)
{
	if (not M_ShownUnits.IsValidIndex(TileIndex))
	{
		return;
	}
	SelectTile(TileIndex);
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleUnitSelected(M_ShownUnits[TileIndex]);
}

FText UW_FactionUnitBrowser::GetTabLabel(const int32 TabIndex) const
{
	return M_Groups.IsValidIndex(TabIndex) ? M_Groups[TabIndex].Label : FText::GetEmpty();
}

UW_FactionUnitTab* UW_FactionUnitBrowser::GetTab(const int32 TabIndex) const
{
	return M_Tabs.IsValidIndex(TabIndex) ? M_Tabs[TabIndex].Get() : nullptr;
}

UW_FactionUnitTile* UW_FactionUnitBrowser::GetTile(const int32 TileIndex) const
{
	return M_Tiles.IsValidIndex(TileIndex) ? M_Tiles[TileIndex].Get() : nullptr;
}

void UW_FactionUnitBrowser::RebuildTabs()
{
	using namespace FactionUnitBrowserLayout;
	if (not GetIsValidTabRow())
	{
		return;
	}
	M_TabRow->ClearChildren();
	M_Tabs.Reset();
	for (int32 GroupIndex = 0; GroupIndex < M_Groups.Num(); ++GroupIndex)
	{
		UW_FactionUnitTab* const Tab = CreateWidget<UW_FactionUnitTab>(this, UW_FactionUnitTab::StaticClass());
		if (not IsValid(Tab))
		{
			continue;
		}
		Tab->SetupTab(this, GroupIndex, M_Style, M_Groups[GroupIndex].Label);
		UHorizontalBoxSlot* const TabSlot = M_TabRow->AddChildToHorizontalBox(Tab);
		TabSlot->SetPadding(FMargin(GroupIndex == 0 ? 0.f : TabSpacing, 0.f, 0.f, 0.f));
		M_Tabs.Add(Tab);
	}
}

void UW_FactionUnitBrowser::RebuildTiles()
{
	if (not GetIsValidTileScroll())
	{
		return;
	}
	M_TileScroll->ClearChildren();
	M_TileScroll->SetScrollOffset(0.f);
	M_Tiles.Reset();
	M_SelectedTileIndex = INDEX_NONE;
	for (int32 TileIndex = 0; TileIndex < M_ShownUnits.Num(); ++TileIndex)
	{
		AddTile(TileIndex, M_ShownUnits[TileIndex]);
	}
}

void UW_FactionUnitBrowser::AddTile(const int32 TileIndex, const FTrainingOption& Unit)
{
	using namespace FactionUnitBrowserLayout;
	UW_FactionUnitTile* const Tile = CreateWidget<UW_FactionUnitTile>(this, UW_FactionUnitTile::StaticClass());
	if (not IsValid(Tile))
	{
		return;
	}
	Tile->SetupTile(this, TileIndex, M_Style, GetUnitIconBrush(Unit), GetUnitName(Unit), GetUnitWearSeed(Unit));
	UScrollBoxSlot* const TileSlot = Cast<UScrollBoxSlot>(M_TileScroll->AddChild(Tile));
	if (IsValid(TileSlot))
	{
		TileSlot->SetPadding(FMargin(TileIndex == 0 ? 0.f : TileSpacing, 0.f, 0.f, 0.f));
		TileSlot->SetVerticalAlignment(VAlign_Top);
	}
	M_Tiles.Add(Tile);
}

void UW_FactionUnitBrowser::SelectTile(const int32 TileIndex)
{
	M_SelectedTileIndex = TileIndex;
	for (int32 Index = 0; Index < M_Tiles.Num(); ++Index)
	{
		if (IsValid(M_Tiles[Index]))
		{
			M_Tiles[Index]->SetIsSelected(Index == TileIndex);
		}
	}
	if (GetIsValidTileScroll() && M_Tiles.IsValidIndex(TileIndex) && IsValid(M_Tiles[TileIndex]))
	{
		M_TileScroll->ScrollWidgetIntoView(M_Tiles[TileIndex], true, EDescendantScrollDestination::IntoView);
	}
}

FSlateBrush UW_FactionUnitBrowser::GetUnitIconBrush(const FTrainingOption& Unit) const
{
	const USlateBrushAsset* TableBrush = UUnitVariantPresentationLibrary::FindTrainingTableImageBrush(
		M_TrainingOptionsTable, Unit);
	return IsValid(TableBrush) ? TableBrush->Brush : FSlateBrush();
}

float UW_FactionUnitBrowser::GetUnitWearSeed(const FTrainingOption& Unit)
{
	// Unit type and subtype rather than the tile's slot, so a unit keeps its wear across tabs and scrolling.
	constexpr int32 SubtypesPerUnitType = 256;
	return static_cast<float>(static_cast<int32>(Unit.UnitType) * SubtypesPerUnitType + Unit.SubtypeValue);
}

FText UW_FactionUnitBrowser::GetUnitName(const FTrainingOption& Unit) const
{
	const FText TableTitle = UUnitVariantPresentationLibrary::FindTrainingTableTitle(M_TrainingOptionsTable, Unit);
	if (not TableTitle.IsEmpty())
	{
		return TableTitle;
	}
	return UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(Unit);
}

bool UW_FactionUnitBrowser::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Menu", "GetIsValidMenu", this);
	return false;
}

bool UW_FactionUnitBrowser::GetIsValidTabRow() const
{
	if (IsValid(M_TabRow))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_TabRow", "GetIsValidTabRow", this);
	return false;
}

bool UW_FactionUnitBrowser::GetIsValidTileScroll() const
{
	if (IsValid(M_TileScroll))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_TileScroll", "GetIsValidTileScroll", this);
	return false;
}
