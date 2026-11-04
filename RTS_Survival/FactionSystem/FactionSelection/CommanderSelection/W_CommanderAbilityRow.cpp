// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CommanderAbilityRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

// Sized for the 440 unit wide body of the 480 wide kit panel (1920x1080 reference).
namespace CommanderAbilityRowLayout
{
	constexpr float IconWellSize = 72.f;
	constexpr float IconSize = 64.f;
	const FMargin IconWellPadding(4.f);
	const FMargin TextColumnPadding(14.f, 2.f, 0.f, 0.f);
	const FMargin DescriptionPadding(0.f, 4.f, 0.f, 0.f);
	const FMargin TierBadgePadding(8.f, 2.f, 8.f, 3.f);
	const FMargin TierBadgeSlotPadding(8.f, 0.f, 0.f, 0.f);
	constexpr float TierBadgeMinWidth = 74.f;
	constexpr int32 TitleFontSize = 16;
	constexpr int32 DescriptionFontSize = 13;
	constexpr int32 TierFontSize = 12;
	constexpr int32 TierLetterSpacing = 120;
	constexpr int32 MissingIconFontSize = 34;

	// One colour per tier so a kit reads at a glance; dark text on the badge.
	const FLinearColor TierIColor = FLinearColor(FColor::FromHex(TEXT("8FD46B")));
	const FLinearColor TierIIColor = FLinearColor(FColor::FromHex(TEXT("38C8FF")));
	const FLinearColor TierIIIColor = FLinearColor(FColor::FromHex(TEXT("E8B04A")));
	const FLinearColor TierIVColor = FLinearColor(FColor::FromHex(TEXT("E0614A")));
	const FLinearColor MissingTierColor = FLinearColor(FColor::FromHex(TEXT("6E6F6A")));
	const FSlateColor TierTextColor = FSlateColor(FColor::FromHex(TEXT("101418")));

	FLinearColor GetTierColor(const EGlobalAbilityTier Tier)
	{
		switch (Tier)
		{
		case EGlobalAbilityTier::TierI:
			return TierIColor;
		case EGlobalAbilityTier::TierII:
			return TierIIColor;
		case EGlobalAbilityTier::TierIII:
			return TierIIIColor;
		case EGlobalAbilityTier::TierIV:
			return TierIVColor;
		default:
			return MissingTierColor;
		}
	}
}

void UW_CommanderAbilityRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_CommanderAbilityRow::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UHorizontalBox* const RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	// Not HitTestInvisible: the tier badge must take the hover for its tooltip.
	RowBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = RowBox;
	BuildIconWell(RowBox);
	BuildTexts(RowBox);
}

void UW_CommanderAbilityRow::BuildIconWell(UHorizontalBox* RowBox)
{
	using namespace CommanderAbilityRowLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(IconWellSize);
	WellBox->SetHeightOverride(IconWellSize);
	UHorizontalBoxSlot* const WellSlot = RowBox->AddChildToHorizontalBox(WellBox);
	WellSlot->SetVerticalAlignment(VAlign_Top);

	M_IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_IconWell->SetPadding(IconWellPadding);
	M_IconWell->SetHorizontalAlignment(HAlign_Center);
	M_IconWell->SetVerticalAlignment(VAlign_Center);
	WellBox->SetContent(M_IconWell);
	UOverlay* const WellOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	M_IconWell->SetContent(WellOverlay);
	M_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	WellOverlay->AddChildToOverlay(M_Icon);
	M_MissingIconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_MissingIconText->SetText(NSLOCTEXT("CommanderSelection", "MissingIcon", "?"));
	UOverlaySlot* const MissingSlot = WellOverlay->AddChildToOverlay(M_MissingIconText);
	MissingSlot->SetHorizontalAlignment(HAlign_Center);
	MissingSlot->SetVerticalAlignment(VAlign_Center);
}

void UW_CommanderAbilityRow::BuildTexts(UHorizontalBox* RowBox)
{
	using namespace CommanderAbilityRowLayout;
	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* const ColumnSlot = RowBox->AddChildToHorizontalBox(TextColumn);
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnSlot->SetPadding(TextColumnPadding);

	UHorizontalBox* const TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TextColumn->AddChildToVerticalBox(TitleRow);
	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TitleText->SetAutoWrapText(true);
	UHorizontalBoxSlot* const TitleSlot = TitleRow->AddChildToHorizontalBox(M_TitleText);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* const BadgeSlot = TitleRow->AddChildToHorizontalBox(BuildTierBadge());
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(TierBadgeSlotPadding);

	M_DescriptionText = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass());
	M_DescriptionText->SetAutoWrapText(true);
	TextColumn->AddChildToVerticalBox(M_DescriptionText)->SetPadding(DescriptionPadding);
}

UWidget* UW_CommanderAbilityRow::BuildTierBadge()
{
	using namespace CommanderAbilityRowLayout;
	USizeBox* const BadgeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BadgeBox->SetMinDesiredWidth(TierBadgeMinWidth);
	BadgeBox->SetVisibility(ESlateVisibility::Visible);
	BadgeBox->SetToolTipText(NSLOCTEXT("CommanderSelection", "TierTooltip",
	                                   "Each operation sets the highest tier you may call in; abilities above it stay "
	                                   "locked until a later operation."));
	M_TierBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_TierBadge->SetPadding(TierBadgePadding);
	M_TierBadge->SetHorizontalAlignment(HAlign_Center);
	M_TierBadge->SetVerticalAlignment(VAlign_Center);
	BadgeBox->SetContent(M_TierBadge);
	M_TierText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TierBadge->SetContent(M_TierText);
	return BadgeBox;
}

void UW_CommanderAbilityRow::SetupRow(const FUnitVariantSelectionStyle& Style, UDataTable* RichTextStyle,
                                      const FCommanderSelectionAbilitySlot& AbilitySlot)
{
	using namespace CommanderAbilityRowLayout;
	if (not GetIsValidDescriptionText())
	{
		return;
	}
	M_IconWell->SetBrush(Style.UnitIconWell);
	ApplyIcon(AbilitySlot);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_MissingIconText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, MissingIconFontSize),
		Style.MutedTextColor);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_TitleText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TitleFontSize),
		AbilitySlot.bHasTemplate ? Style.TextColor : Style.MutedTextColor);
	M_TitleText->SetText(AbilitySlot.Title.ToUpper());
	ApplyTierBadge(Style, AbilitySlot);
	if (IsValid(RichTextStyle))
	{
		M_DescriptionText->SetTextStyleSet(RichTextStyle);
	}
	// Untagged text takes the screen's body font: the style set's monospace default made a kit overflow the panel.
	M_DescriptionText->SetDefaultFont(UnitVariantSelectionWidgets::MakeFont(Style.BodyFont, DescriptionFontSize));
	M_DescriptionText->SetDefaultColorAndOpacity(Style.DescriptionColor);
	M_DescriptionText->SetDefaultShadowOffset(UnitVariantSelectionWidgets::TextShadowOffset);
	M_DescriptionText->SetDefaultShadowColorAndOpacity(UnitVariantSelectionWidgets::TextShadowColor);
	M_DescriptionText->SetText(AbilitySlot.Description);
	M_DescriptionText->SetVisibility(AbilitySlot.Description.IsEmpty()
		                                 ? ESlateVisibility::Collapsed
		                                 : ESlateVisibility::HitTestInvisible);
}

void UW_CommanderAbilityRow::ApplyIcon(const FCommanderSelectionAbilitySlot& AbilitySlot)
{
	using namespace CommanderAbilityRowLayout;
	const bool bHasIcon = IsValid(AbilitySlot.Icon);
	if (bHasIcon)
	{
		// SetBrushFromTexture keeps the default 32 unit image size; the icons are drawn for the 64 unit well.
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(AbilitySlot.Icon);
		IconBrush.ImageSize = FVector2D(IconSize, IconSize);
		M_Icon->SetBrush(IconBrush);
	}
	// An empty brush would draw Slate's default white box.
	M_Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	M_MissingIconText->SetVisibility(bHasIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UW_CommanderAbilityRow::ApplyTierBadge(const FUnitVariantSelectionStyle& Style,
                                            const FCommanderSelectionAbilitySlot& AbilitySlot)
{
	using namespace CommanderAbilityRowLayout;
	const FText TierNumeral = AbilitySlot.bHasTemplate
		                          ? CommanderSelectionData::GetTierNumeral(AbilitySlot.Tier)
		                          : NSLOCTEXT("CommanderSelection", "TierPending", "?");
	M_TierText->SetText(FText::Format(NSLOCTEXT("CommanderSelection", "TierBadge", "TIER {0}"), TierNumeral));
	M_TierText->SetFont(UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TierFontSize, TierLetterSpacing));
	M_TierText->SetColorAndOpacity(TierTextColor);
	M_TierBadge->SetBrushColor(AbilitySlot.bHasTemplate ? GetTierColor(AbilitySlot.Tier) : MissingTierColor);
}

bool UW_CommanderAbilityRow::GetIsValidDescriptionText() const
{
	if (IsValid(M_DescriptionText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_DescriptionText",
	                                                             "GetIsValidDescriptionText", this);
	return false;
}
