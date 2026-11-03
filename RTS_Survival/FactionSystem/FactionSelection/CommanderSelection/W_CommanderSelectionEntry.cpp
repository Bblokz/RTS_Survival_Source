// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CommanderSelectionEntry.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionButton.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_CommanderSelectionMenu.h"

// Same measures as the unit entries of the variant screen (Custom_VariantUnitEntry), whose button art it reuses.
namespace CommanderSelectionEntryLayout
{
	constexpr float EntryWidth = 352.f;
	constexpr float EntryHeight = 80.f;
	constexpr float PortraitWellSize = 72.f;
	constexpr float PortraitSize = 64.f;
	const FMargin PortraitWellOffset(16.f, 4.f, 0.f, 0.f);
	const FMargin PortraitWellPadding(4.f);
	const FMargin TextOffset(102.f, 18.f, 24.f, 0.f);
	const FMargin DetailPadding(0.f, 2.f, 0.f, 0.f);
	// Smaller than the variant screen's unit names so "LUFTWAFFE LIAISON" clears the selection chevron.
	constexpr int32 TitleFontSize = 16;
	constexpr int32 DetailFontSize = 13;
	constexpr int32 DetailLetterSpacing = 50;
	// The portraits are 1024x1280 renders with the head left of centre; this 717 px square sits on it.
	const FBox2d PortraitHeadUVRegion(FVector2d(0.08, 0.03), FVector2d(0.78, 0.59));
}

void UW_CommanderSelectionEntry::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_CommanderSelectionEntry::BuildWidgetTree()
{
	using namespace CommanderSelectionEntryLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(EntryWidth);
	SizeBox->SetHeightOverride(EntryHeight);
	WidgetTree->RootWidget = SizeBox;

	// Never takes keyboard focus, so the menu keeps the arrow keys for its navigation.
	M_Button = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
	M_Button->OnClicked.AddDynamic(this, &UW_CommanderSelectionEntry::OnEntryClicked);
	SizeBox->SetContent(M_Button);

	UOverlay* const EntryOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// The button centres its content by default; the overlay must fill it so the offsets hold.
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_Button->SetContent(EntryOverlay)))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	BuildPortraitWell(EntryOverlay);
	BuildTexts(EntryOverlay);
}

void UW_CommanderSelectionEntry::BuildPortraitWell(UOverlay* EntryOverlay)
{
	using namespace CommanderSelectionEntryLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(PortraitWellSize);
	WellBox->SetHeightOverride(PortraitWellSize);
	UOverlaySlot* const WellSlot = EntryOverlay->AddChildToOverlay(WellBox);
	WellSlot->SetHorizontalAlignment(HAlign_Left);
	WellSlot->SetVerticalAlignment(VAlign_Top);
	WellSlot->SetPadding(PortraitWellOffset);

	M_PortraitWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_PortraitWell->SetPadding(PortraitWellPadding);
	M_PortraitWell->SetVisibility(ESlateVisibility::HitTestInvisible);
	WellBox->SetContent(M_PortraitWell);
	M_PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_PortraitImage->SetVisibility(ESlateVisibility::Hidden);
	M_PortraitWell->SetContent(M_PortraitImage);
}

void UW_CommanderSelectionEntry::BuildTexts(UOverlay* EntryOverlay)
{
	using namespace CommanderSelectionEntryLayout;
	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TextColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const TextSlot = EntryOverlay->AddChildToOverlay(TextColumn);
	TextSlot->SetHorizontalAlignment(HAlign_Fill);
	TextSlot->SetVerticalAlignment(VAlign_Top);
	TextSlot->SetPadding(TextOffset);

	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TitleText->SetClipping(EWidgetClipping::ClipToBounds);
	TextColumn->AddChildToVerticalBox(M_TitleText);
	M_KitTiersText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TextColumn->AddChildToVerticalBox(M_KitTiersText)->SetPadding(DetailPadding);
}

void UW_CommanderSelectionEntry::SetupEntry(UW_CommanderSelectionMenu* Menu, const int32 EntryIndex,
                                            const FUnitVariantSelectionStyle& Style,
                                            const FCommanderSelectionEntryData& CommanderEntry)
{
	using namespace CommanderSelectionEntryLayout;
	M_Menu = Menu;
	M_EntryIndex = EntryIndex;
	M_Style = Style;
	if (not GetIsValidButton())
	{
		return;
	}
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_TitleText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TitleFontSize), Style.TextColor);
	M_TitleText->SetText(CommanderEntry.Title.ToUpper());
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_KitTiersText, UnitVariantSelectionWidgets::MakeFont(Style.BodyFont, DetailFontSize, DetailLetterSpacing),
		Style.MutedTextColor);
	M_KitTiersText->SetText(MakeKitTiersText(CommanderEntry));
	ApplySelectionLook();
}

void UW_CommanderSelectionEntry::SetPortrait(UTexture2D* Portrait)
{
	using namespace CommanderSelectionEntryLayout;
	if (not GetIsValidButton())
	{
		return;
	}
	if (not IsValid(Portrait))
	{
		// An empty brush would draw Slate's default white box.
		M_PortraitImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}
	// The image's brush is a UPROPERTY, so it keeps the portrait loaded.
	FSlateBrush PortraitBrush;
	PortraitBrush.SetResourceObject(Portrait);
	PortraitBrush.ImageSize = FVector2D(PortraitSize, PortraitSize);
	PortraitBrush.SetUVRegion(PortraitHeadUVRegion);
	M_PortraitImage->SetBrush(PortraitBrush);
	M_PortraitImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_CommanderSelectionEntry::SetIsSelected(const bool bIsSelected)
{
	bM_IsSelected = bIsSelected;
	ApplySelectionLook();
}

void UW_CommanderSelectionEntry::ApplySelectionLook()
{
	if (not GetIsValidButton())
	{
		return;
	}
	M_Button->SetStyle(bM_IsSelected ? M_Style.UnitEntrySelected : M_Style.UnitEntry);
	M_PortraitWell->SetBrush(bM_IsSelected ? M_Style.UnitIconWellSelected : M_Style.UnitIconWell);
	M_TitleText->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedTextColor : M_Style.TextColor);
	M_KitTiersText->SetColorAndOpacity(bM_IsSelected ? M_Style.SelectedMutedTextColor : M_Style.MutedTextColor);
}

FText UW_CommanderSelectionEntry::MakeKitTiersText(const FCommanderSelectionEntryData& CommanderEntry)
{
	TArray<FText> TierNumerals;
	for (const FCommanderSelectionAbilitySlot& AbilitySlot : CommanderEntry.AbilitySlots)
	{
		TierNumerals.Add(AbilitySlot.bHasTemplate
			                 ? CommanderSelectionData::GetTierNumeral(AbilitySlot.Tier)
			                 : NSLOCTEXT("CommanderSelection", "EntryTierPending", "?"));
	}
	return FText::Format(NSLOCTEXT("CommanderSelection", "EntryKitTiers", "TIERS  {0}"),
	                     FText::Join(NSLOCTEXT("CommanderSelection", "EntryTierSeparator", " · "), TierNumerals));
}

void UW_CommanderSelectionEntry::OnEntryClicked()
{
	if (not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleCommanderEntryClicked(M_EntryIndex);
}

bool UW_CommanderSelectionEntry::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Menu", "GetIsValidMenu", this);
	return false;
}

bool UW_CommanderSelectionEntry::GetIsValidButton() const
{
	if (IsValid(M_Button))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Button", "GetIsValidButton", this);
	return false;
}
