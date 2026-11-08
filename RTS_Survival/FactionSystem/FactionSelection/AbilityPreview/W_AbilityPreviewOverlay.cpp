// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_AbilityPreviewOverlay.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"

// 1920x1080 reference: a slim plate along the top and the BACK button at the bottom keep the desert in view.
namespace AbilityPreviewOverlayLayout
{
	constexpr float PlateTop = 24.f;
	constexpr float PlateWidth = 1040.f;
	const FMargin PlatePadding(20.f, 14.f, 24.f, 16.f);
	constexpr float IconWellSize = 88.f;
	constexpr float IconSize = 80.f;
	const FMargin IconWellPadding(4.f);
	const FMargin TextColumnPadding(18.f, 0.f, 0.f, 0.f);
	const FMargin TierBadgePadding(10.f, 3.f, 10.f, 4.f);
	const FMargin TierBadgeSlotPadding(12.f, 0.f, 0.f, 0.f);
	const FMargin DescriptionPadding(0.f, 6.f, 0.f, 0.f);
	const FMargin CountdownPadding(0.f, 8.f, 0.f, 0.f);
	constexpr float BottomMargin = 40.f;
	const FVector2D BackButtonSize(240.f, 48.f);
	constexpr int32 HeaderFontSize = 12;
	constexpr int32 HeaderLetterSpacing = 300;
	constexpr int32 TitleFontSize = 26;
	constexpr int32 TitleLetterSpacing = 120;
	constexpr int32 TierFontSize = 14;
	constexpr int32 TierLetterSpacing = 120;
	constexpr int32 DescriptionFontSize = 16;
	constexpr int32 CountdownFontSize = 13;
	const FLinearColor PlateColor(0.015f, 0.018f, 0.022f, 0.78f);
	const FLinearColor PlateOutlineColor(0.25f, 0.27f, 0.27f, 0.9f);
	constexpr float PlateOutlineWidth = 2.f;
	constexpr float PlateCornerRadius = 6.f;
}

void UW_AbilityPreviewOverlay::NativeOnInitialized()
{
	// Keyboard focus stays on the overlay, for Escape / B.
	SetIsFocusable(true);
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_AbilityPreviewOverlay::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UCanvasPanel* const ScreenCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = ScreenCanvas;
	M_LayoutCanvas = M_DesignFrame.BuildFrame(*WidgetTree, *ScreenCanvas);
	if (not IsValid(M_LayoutCanvas))
	{
		return;
	}
	BuildAbilityPlate();
}

void UW_AbilityPreviewOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	M_DesignFrame.UpdateFrame(MyGeometry);
}

void UW_AbilityPreviewOverlay::BuildAbilityPlate()
{
	using namespace AbilityPreviewOverlayLayout;
	USizeBox* const PlateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PlateBox->SetWidthOverride(PlateWidth);
	UCanvasPanelSlot* const PlateSlot = M_LayoutCanvas->AddChildToCanvas(PlateBox);
	PlateSlot->SetAnchors(FAnchors(0.5f, 0.f));
	PlateSlot->SetAlignment(FVector2D(0.5f, 0.f));
	PlateSlot->SetPosition(FVector2D(0.f, PlateTop));
	PlateSlot->SetAutoSize(true);

	M_AbilityPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_AbilityPlate->SetPadding(PlatePadding);
	M_AbilityPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	FSlateBrush PlateBrush;
	PlateBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	PlateBrush.TintColor = FSlateColor(PlateColor);
	PlateBrush.OutlineSettings = FSlateBrushOutlineSettings(
		FVector4(PlateCornerRadius, PlateCornerRadius, PlateCornerRadius, PlateCornerRadius),
		FSlateColor(PlateOutlineColor), PlateOutlineWidth);
	M_AbilityPlate->SetBrush(PlateBrush);
	PlateBox->SetContent(M_AbilityPlate);

	UHorizontalBox* const PlateRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_AbilityPlate->SetContent(PlateRow);

	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(IconWellSize);
	WellBox->SetHeightOverride(IconWellSize);
	UHorizontalBoxSlot* const WellSlot = PlateRow->AddChildToHorizontalBox(WellBox);
	WellSlot->SetVerticalAlignment(VAlign_Top);
	M_IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_IconWell->SetPadding(IconWellPadding);
	M_IconWell->SetHorizontalAlignment(HAlign_Center);
	M_IconWell->SetVerticalAlignment(VAlign_Center);
	WellBox->SetContent(M_IconWell);
	M_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_Icon->SetVisibility(ESlateVisibility::Hidden);
	M_IconWell->SetContent(M_Icon);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* const ColumnSlot = PlateRow->AddChildToHorizontalBox(TextColumn);
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnSlot->SetPadding(TextColumnPadding);

	M_HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_HeaderText->SetText(NSLOCTEXT("AbilityPreview", "Header", "ABILITY PREVIEW"));
	TextColumn->AddChildToVerticalBox(M_HeaderText);

	UHorizontalBox* const TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TextColumn->AddChildToVerticalBox(TitleRow);
	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TitleText->SetAutoWrapText(true);
	UHorizontalBoxSlot* const TitleSlot = TitleRow->AddChildToHorizontalBox(M_TitleText);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	M_TierBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_TierBadge->SetPadding(TierBadgePadding);
	M_TierBadge->SetHorizontalAlignment(HAlign_Center);
	M_TierBadge->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* const BadgeSlot = TitleRow->AddChildToHorizontalBox(M_TierBadge);
	BadgeSlot->SetVerticalAlignment(VAlign_Center);
	BadgeSlot->SetPadding(TierBadgeSlotPadding);
	M_TierText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TierBadge->SetContent(M_TierText);

	M_DescriptionText = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass());
	M_DescriptionText->SetAutoWrapText(true);
	TextColumn->AddChildToVerticalBox(M_DescriptionText)->SetPadding(DescriptionPadding);

	M_CountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_CountdownText->SetVisibility(ESlateVisibility::Collapsed);
	TextColumn->AddChildToVerticalBox(M_CountdownText)->SetPadding(CountdownPadding);
}

void UW_AbilityPreviewOverlay::BuildBackButton(const FUnitVariantSelectionStyle& Style)
{
	using namespace AbilityPreviewOverlayLayout;
	if (IsValid(M_BackButton) || not IsValid(M_LayoutCanvas))
	{
		return;
	}
	UWidget* const BackBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, Style, NSLOCTEXT("AbilityPreview", "Back", "BACK"), Style.BackButton, BackButtonSize,
		M_BackButton);
	M_BackButton->OnClicked.AddDynamic(this, &UW_AbilityPreviewOverlay::OnBackClicked);
	UCanvasPanelSlot* const BackSlot = M_LayoutCanvas->AddChildToCanvas(BackBox);
	BackSlot->SetAnchors(FAnchors(0.5f, 1.f));
	BackSlot->SetAlignment(FVector2D(0.5f, 1.f));
	BackSlot->SetPosition(FVector2D(0.f, -BottomMargin));
	BackSlot->SetAutoSize(true);
}

void UW_AbilityPreviewOverlay::SetupOverlay(const FUnitVariantSelectionStyle& Style, UDataTable* RichTextStyle,
                                            const FCommanderSelectionAbilitySlot& AbilitySlot)
{
	using namespace AbilityPreviewOverlayLayout;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_DescriptionText))
	{
		return;
	}
	BuildBackButton(Style);
	M_IconWell->SetBrush(Style.UnitIconWell);
	if (IsValid(AbilitySlot.Icon))
	{
		// SetBrushFromTexture keeps the default 32 unit image size.
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(AbilitySlot.Icon);
		IconBrush.ImageSize = FVector2D(IconSize, IconSize);
		M_Icon->SetBrush(IconBrush);
		M_Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	ApplyTextOnMetal(M_HeaderText, MakeFont(Style.HeadingFont, HeaderFontSize, HeaderLetterSpacing),
	                 Style.AccentColor);
	ApplyTextOnMetal(M_TitleText, MakeFont(Style.HeadingFont, TitleFontSize, TitleLetterSpacing),
	                 Style.SelectedTextColor);
	M_TitleText->SetText(AbilitySlot.Title.ToUpper());
	M_TierText->SetText(FText::Format(NSLOCTEXT("AbilityPreview", "TierBadge", "TIER {0}"),
	                                  CommanderSelectionData::GetTierNumeral(AbilitySlot.Tier)));
	M_TierText->SetFont(MakeFont(Style.HeadingFont, TierFontSize, TierLetterSpacing));
	M_TierText->SetColorAndOpacity(CommanderSelectionData::GetTierBadgeTextColor());
	M_TierBadge->SetBrushColor(CommanderSelectionData::GetTierBadgeColor(AbilitySlot.Tier, AbilitySlot.bHasTemplate));
	if (IsValid(RichTextStyle))
	{
		M_DescriptionText->SetTextStyleSet(RichTextStyle);
	}
	M_DescriptionText->SetDefaultFont(MakeFont(Style.BodyFont, DescriptionFontSize));
	M_DescriptionText->SetDefaultColorAndOpacity(Style.DescriptionColor);
	M_DescriptionText->SetDefaultShadowOffset(TextShadowOffset);
	M_DescriptionText->SetDefaultShadowColorAndOpacity(TextShadowColor);
	M_DescriptionText->SetText(AbilitySlot.Description);
	ApplyTextOnMetal(M_CountdownText, MakeFont(Style.BodyFont, CountdownFontSize), Style.MutedTextColor);
}

void UW_AbilityPreviewOverlay::ShowAutoReturnCountdown(const float SecondsLeft)
{
	if (not IsValid(M_CountdownText))
	{
		return;
	}
	const int32 WholeSeconds = FMath::Max(0, FMath::CeilToInt(SecondsLeft));
	M_CountdownText->SetText(FText::Format(
		NSLOCTEXT("AbilityPreview", "Countdown", "Back to the hangar in {0} s"), FText::AsNumber(WholeSeconds)));
	M_CountdownText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

FReply UW_AbilityPreviewOverlay::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right)
	{
		RequestBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UW_AbilityPreviewOverlay::RequestBack()
{
	OnBackRequested.ExecuteIfBound();
}

void UW_AbilityPreviewOverlay::OnBackClicked()
{
	RequestBack();
}
