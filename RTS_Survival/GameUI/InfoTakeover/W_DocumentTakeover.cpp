// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DocumentTakeover.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"

namespace DocumentTakeoverLayout
{
	// 1080p reference units inside the base frame's 1240 wide panel.
	const FMargin DescriptionPadding(0.f, 0.f, 0.f, 18.f);
	// Keeps the sheet's bottom edge off the footer plate.
	const FMargin PaperSlotPadding(0.f, 0.f, 0.f, 16.f);
	constexpr float PaperWidth = 940.f;
	const FMargin PaperPadding(34.f, 24.f, 34.f, 22.f);
	// Aged paper and faded typewriter ink; plain text on paper, never the panel's bone-on-metal text.
	const FLinearColor PaperColor(0.74f, 0.7f, 0.6f, 1.f);
	const FLinearColor PaperEdgeColor(0.42f, 0.38f, 0.3f, 1.f);
	constexpr float PaperCornerRadius = 2.f;
	constexpr float PaperEdgeWidth = 1.f;
	const FLinearColor InkColor(0.1f, 0.09f, 0.08f, 1.f);
	const FLinearColor FadedInkColor(0.26f, 0.24f, 0.2f, 1.f);
	const FLinearColor RedInkColor(0.55f, 0.07f, 0.04f, 1.f);
	const FLinearColor MarkedBandColor(0.7f, 0.2f, 0.12f, 0.16f);
	constexpr int32 HeadingFontSize = 22;
	constexpr int32 HeadingLetterSpacing = 150;
	constexpr int32 SubheadingFontSize = 14;
	constexpr int32 LineFontSize = 16;
	constexpr int32 FooterFontSize = 13;
	constexpr float PaperTextWrapWidth = 860.f;
	const FMargin SubheadingPadding(0.f, 2.f, 0.f, 0.f);
	const FMargin RulePadding(0.f, 10.f, 0.f, 10.f);
	constexpr float RuleHeight = 2.f;
	const FMargin LinePadding(0.f, 3.f, 0.f, 3.f);
	const FMargin MarkedBandPadding(8.f, 3.f, 8.f, 3.f);
	const FMargin FooterPadding(0.f, 14.f, 0.f, 0.f);

	// The rubber stamp: red ink frame and letters, tilted, a little see-through like real ink.
	const FLinearColor StampColor(0.66f, 0.1f, 0.06f, 1.f);
	constexpr float StampCornerRadius = 6.f;
	constexpr float StampFrameWidth = 3.f;
	const FMargin StampInnerPadding(16.f, 6.f, 16.f, 6.f);
	const FMargin StampSlotPadding(0.f, 18.f, 44.f, 0.f);
	constexpr int32 StampFontSize = 30;
	constexpr int32 StampLetterSpacing = 220;
	constexpr float StampAngleDegrees = -9.f;
	constexpr float StampOpacity = 0.82f;
}

void UW_DocumentTakeover::SetupDocument(const FDocumentTakeoverContent& Content)
{
	if (Content.KindLabel.IsEmpty())
	{
		HideKindLabel();
	}
	else
	{
		SetKindLabel(Content.KindLabel);
	}
	SetTitleText(Content.Title);
	SetDescriptionText(Content.Description);
	BuildBody(Content);
}

void UW_DocumentTakeover::BuildBody(const FDocumentTakeoverContent& Content)
{
	using namespace DocumentTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	UVerticalBoxSlot* const PaperSlot = BodyColumn->AddChildToVerticalBox(BuildPaperSheet(Content));
	PaperSlot->SetHorizontalAlignment(HAlign_Center);
	PaperSlot->SetPadding(PaperSlotPadding);
}

UWidget* UW_DocumentTakeover::BuildPaperSheet(const FDocumentTakeoverContent& Content)
{
	using namespace DocumentTakeoverLayout;
	USizeBox* const PaperBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PaperBox->SetWidthOverride(PaperWidth);
	UOverlay* const PaperOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	PaperBox->SetContent(PaperOverlay);

	UBorder* const Paper = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Paper->SetBrush(FSlateRoundedBoxBrush(PaperColor, PaperCornerRadius, PaperEdgeColor, PaperEdgeWidth));
	Paper->SetPadding(PaperPadding);
	UOverlaySlot* const PaperSlot = PaperOverlay->AddChildToOverlay(Paper);
	PaperSlot->SetHorizontalAlignment(HAlign_Fill);
	PaperSlot->SetVerticalAlignment(VAlign_Fill);

	UVerticalBox* const PaperColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Paper->SetContent(PaperColumn);
	AddPaperHeader(*PaperColumn, Content);
	M_PaperLineCount = 0;
	for (const FDocumentTakeoverLine& Line : Content.PaperLines)
	{
		AddPaperLine(*PaperColumn, Line);
		++M_PaperLineCount;
	}
	AddPaperFooter(*PaperColumn, Content.PaperFooter);

	bM_HasStamp = not Content.StampText.IsEmpty();
	if (bM_HasStamp)
	{
		UOverlaySlot* const StampSlot = PaperOverlay->AddChildToOverlay(BuildStamp(Content.StampText));
		StampSlot->SetHorizontalAlignment(HAlign_Right);
		StampSlot->SetVerticalAlignment(VAlign_Top);
		StampSlot->SetPadding(StampSlotPadding);
	}
	return PaperBox;
}

void UW_DocumentTakeover::AddPaperHeader(UVerticalBox& PaperColumn, const FDocumentTakeoverContent& Content) const
{
	using namespace DocumentTakeoverLayout;
	UTextBlock* const HeadingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	HeadingText->SetText(Content.PaperHeading);
	HeadingText->SetFont(UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, HeadingFontSize,
	                                                           HeadingLetterSpacing));
	HeadingText->SetColorAndOpacity(FSlateColor(InkColor));
	PaperColumn.AddChildToVerticalBox(HeadingText);

	if (not Content.PaperSubheading.IsEmpty())
	{
		UTextBlock* const SubheadingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		SubheadingText->SetText(Content.PaperSubheading);
		SubheadingText->SetFont(UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, SubheadingFontSize));
		SubheadingText->SetColorAndOpacity(FSlateColor(FadedInkColor));
		SubheadingText->SetWrapTextAt(PaperTextWrapWidth);
		PaperColumn.AddChildToVerticalBox(SubheadingText)->SetPadding(SubheadingPadding);
	}

	UBorder* const Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Rule->SetBrush(FSlateColorBrush(FadedInkColor));
	USizeBox* const RuleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RuleBox->SetHeightOverride(RuleHeight);
	RuleBox->SetContent(Rule);
	PaperColumn.AddChildToVerticalBox(RuleBox)->SetPadding(RulePadding);
}

void UW_DocumentTakeover::AddPaperLine(UVerticalBox& PaperColumn, const FDocumentTakeoverLine& Line) const
{
	using namespace DocumentTakeoverLayout;
	UTextBlock* const LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LineText->SetText(Line.Text);
	LineText->SetFont(UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, LineFontSize));
	LineText->SetColorAndOpacity(FSlateColor(Line.bIsMarked ? RedInkColor : InkColor));
	LineText->SetWrapTextAt(PaperTextWrapWidth);
	if (not Line.bIsMarked)
	{
		PaperColumn.AddChildToVerticalBox(LineText)->SetPadding(LinePadding);
		return;
	}
	// A marked line sits on a faint red band, as if someone ran a red pencil over it.
	UBorder* const MarkedBand = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MarkedBand->SetBrush(FSlateColorBrush(MarkedBandColor));
	MarkedBand->SetPadding(MarkedBandPadding);
	MarkedBand->SetContent(LineText);
	PaperColumn.AddChildToVerticalBox(MarkedBand)->SetPadding(LinePadding);
}

void UW_DocumentTakeover::AddPaperFooter(UVerticalBox& PaperColumn, const FText& Footer) const
{
	using namespace DocumentTakeoverLayout;
	if (Footer.IsEmpty())
	{
		return;
	}
	UTextBlock* const FooterText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FooterText->SetText(Footer);
	FooterText->SetFont(UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, FooterFontSize));
	FooterText->SetColorAndOpacity(FSlateColor(FadedInkColor));
	FooterText->SetWrapTextAt(PaperTextWrapWidth);
	PaperColumn.AddChildToVerticalBox(FooterText)->SetPadding(FooterPadding);
}

UWidget* UW_DocumentTakeover::BuildStamp(const FText& StampText) const
{
	using namespace DocumentTakeoverLayout;
	UBorder* const StampFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	StampFrame->SetBrush(FSlateRoundedBoxBrush(FLinearColor::Transparent, StampCornerRadius, StampColor,
	                                           StampFrameWidth));
	StampFrame->SetPadding(StampInnerPadding);
	UTextBlock* const StampLetters = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	StampLetters->SetText(StampText);
	StampLetters->SetFont(UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, StampFontSize,
	                                                            StampLetterSpacing));
	StampLetters->SetColorAndOpacity(FSlateColor(StampColor));
	StampFrame->SetContent(StampLetters);
	StampFrame->SetRenderTransformAngle(StampAngleDegrees);
	StampFrame->SetRenderOpacity(StampOpacity);
	StampFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	return StampFrame;
}
