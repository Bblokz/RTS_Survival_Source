// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_ClipInfoTakeover.h"

#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ClipInfoTakeoverLayout
{
	// Sizes are for the 1080p reference height and match the spaced armour takeover's cards, so the field manuals
	// read alike.
	constexpr float CardGap = 20.f;
	const FMargin GlassPanePadding(14.f, 14.f, 14.f, 18.f);
	// Each clip frame is 480 x 270; shown a third larger, which keeps the low-res look without blurring it to mush.
	const FVector2D ClipSize(640.f, 360.f);
	const FMargin ClipWellPadding(6.f);
	const FMargin DescriptionPadding(0.f, 0.f, 0.f, 22.f);
	const FMargin CaptionPadding(0.f, 12.f, 0.f, 0.f);
	const FMargin FirstFactLabelPadding(0.f, 0.f, 0.f, 2.f);
	const FMargin FactLabelPadding(0.f, 18.f, 0.f, 2.f);
	const FMargin FactTitlePadding(0.f, 0.f, 0.f, 4.f);
	constexpr float CaptionWrapWidth = 640.f;
	constexpr float FactTextWrapWidth = 440.f;
	constexpr int32 FactLabelFontSize = 12;
	constexpr int32 FactLabelLetterSpacing = 100;
	constexpr int32 FactTitleFontSize = 19;
	constexpr int32 BodyFontSize = 15;
	// Half a texel kept off every cell edge, so bilinear filtering never pulls in the neighbouring frame.
	constexpr float CellEdgeInsetTexels = 0.5f;
}

UW_ClipInfoTakeover::UW_ClipInfoTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UW_ClipInfoTakeover::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Slate's delta is real time, so the clip plays while the takeover holds the game paused.
	AdvanceClip(InDeltaTime);
}

TArray<FClipTakeoverFact> UW_ClipInfoTakeover::MakeFacts() const
{
	return {};
}

FText UW_ClipInfoTakeover::GetClipCaption() const
{
	return FText::GetEmpty();
}

void UW_ClipInfoTakeover::BuildClipBody()
{
	using namespace ClipInfoTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_LoadedClipAtlas = nullptr;
	M_ClipImage = nullptr;
	M_ClipSeconds = 0.f;
	M_ShownClipFrame = INDEX_NONE;
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	UHorizontalBox* const CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BodyColumn->AddChildToVerticalBox(CardRow);
	UHorizontalBoxSlot* const ClipSlot = CardRow->AddChildToHorizontalBox(BuildClipCard());
	ClipSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	ClipSlot->SetPadding(FMargin(0.f, 0.f, CardGap, 0.f));
	UHorizontalBoxSlot* const FactsSlot = CardRow->AddChildToHorizontalBox(BuildFactsCard());
	FactsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
}

UWidget* UW_ClipInfoTakeover::BuildClipCard()
{
	using namespace ClipInfoTakeoverLayout;
	UVerticalBox* const CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UBorder* const ClipWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ClipWell->SetBrush(M_Style.Skin.CardIconWell);
	ClipWell->SetPadding(ClipWellPadding);
	USizeBox* const ClipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ClipBox->SetWidthOverride(ClipSize.X);
	ClipBox->SetHeightOverride(ClipSize.Y);
	ClipWell->SetContent(ClipBox);
	CardColumn->AddChildToVerticalBox(ClipWell)->SetHorizontalAlignment(HAlign_Center);

	UTexture2D* const LoadedAtlas = M_ClipAtlas.LoadSynchronous();
	if (IsValid(LoadedAtlas))
	{
		M_LoadedClipAtlas = LoadedAtlas;
		M_ClipImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		M_ClipImage->SetBrushFromTexture(LoadedAtlas, false);
		ShowClipFrame(0);
		ClipBox->SetContent(MakePictureWithGlass(M_ClipImage));
	}
	else
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("%s: clip atlas %s could not be loaded; the card shows an empty well."), *GetClass()->GetName(),
			*M_ClipAtlas.ToString()));
	}

	UTextBlock* const CaptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CaptionText->SetText(GetClipCaption());
	CaptionText->SetWrapTextAt(CaptionWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		CaptionText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, BodyFontSize),
		M_Style.Skin.DescriptionColor);
	CardColumn->AddChildToVerticalBox(CaptionText)->SetPadding(CaptionPadding);
	return WrapInGlassPane(CardColumn);
}

UWidget* UW_ClipInfoTakeover::BuildFactsCard()
{
	UVerticalBox* const FactsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const TArray<FClipTakeoverFact> Facts = MakeFacts();
	for (int32 FactIndex = 0; FactIndex < Facts.Num(); ++FactIndex)
	{
		AddFactText(*FactsColumn, Facts[FactIndex], FactIndex == 0);
	}
	return WrapInGlassPane(FactsColumn);
}

UWidget* UW_ClipInfoTakeover::WrapInGlassPane(UWidget* Content)
{
	// The vehicle takeover's facts pane: blurred glass over the plate.
	UBackgroundBlur* const GlassBlur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass());
	GlassBlur->SetBlurStrength(M_Style.Skin.VariantSectionBlurStrength);
	GlassBlur->SetLowQualityFallbackBrush(FSlateNoResource());
	GlassBlur->SetPadding(FMargin(0.f));
	GlassBlur->SetHorizontalAlignment(HAlign_Fill);
	GlassBlur->SetVerticalAlignment(VAlign_Fill);
	UBorder* const GlassPane = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	GlassPane->SetBrush(M_Style.Skin.VariantSectionGlass);
	GlassPane->SetPadding(ClipInfoTakeoverLayout::GlassPanePadding);
	GlassPane->SetContent(Content);
	RegisterGlassPane(*GlassPane);
	GlassBlur->SetContent(GlassPane);
	return GlassBlur;
}

void UW_ClipInfoTakeover::AddFactText(UVerticalBox& FactsColumn, const FClipTakeoverFact& Fact,
                                      const bool bIsFirst) const
{
	using namespace ClipInfoTakeoverLayout;
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Fact.Label);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, FactLabelFontSize,
		                                                 FactLabelLetterSpacing), M_Style.Skin.LabelColor);
	FactsColumn.AddChildToVerticalBox(LabelText)->SetPadding(bIsFirst ? FirstFactLabelPadding : FactLabelPadding);

	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(Fact.Title);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, FactTitleFontSize),
		M_Style.Skin.TextColor);
	FactsColumn.AddChildToVerticalBox(TitleText)->SetPadding(FactTitlePadding);

	UTextBlock* const BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BodyText->SetText(Fact.Body);
	BodyText->SetWrapTextAt(FactTextWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		BodyText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, BodyFontSize),
		M_Style.Skin.DescriptionColor);
	FactsColumn.AddChildToVerticalBox(BodyText);
}

void UW_ClipInfoTakeover::AdvanceClip(const float RealDeltaSeconds)
{
	if (not GetIsClipSetUp())
	{
		return;
	}
	const float FrameSeconds = 1.f / M_ClipFramesPerSecond;
	const float LoopSeconds = M_ClipFrameCount * FrameSeconds + M_ClipEndHoldSeconds;
	M_ClipSeconds = FMath::Fmod(M_ClipSeconds + RealDeltaSeconds, LoopSeconds);
	// Past the last frame the clip holds on it until the loop starts over.
	const int32 FrameIndex = FMath::Min(FMath::FloorToInt32(M_ClipSeconds / FrameSeconds), M_ClipFrameCount - 1);
	if (FrameIndex != M_ShownClipFrame)
	{
		ShowClipFrame(FrameIndex);
	}
}

void UW_ClipInfoTakeover::ShowClipFrame(const int32 FrameIndex)
{
	using namespace ClipInfoTakeoverLayout;
	if (not GetIsClipSetUp())
	{
		return;
	}
	const int32 Column = FrameIndex % M_ClipColumns;
	const int32 Row = FrameIndex / M_ClipColumns;
	const float CellWidth = 1.f / M_ClipColumns;
	const float CellHeight = 1.f / M_ClipRows;
	const float InsetU = CellEdgeInsetTexels / FMath::Max(1, M_LoadedClipAtlas->GetSizeX());
	const float InsetV = CellEdgeInsetTexels / FMath::Max(1, M_LoadedClipAtlas->GetSizeY());
	const FVector2f CellMin(Column * CellWidth + InsetU, Row * CellHeight + InsetV);
	const FVector2f CellMax((Column + 1) * CellWidth - InsetU, (Row + 1) * CellHeight - InsetV);
	FSlateBrush ClipBrush = M_ClipImage->GetBrush();
	ClipBrush.SetUVRegion(FBox2f(CellMin, CellMax));
	M_ClipImage->SetBrush(ClipBrush);
	M_ShownClipFrame = FrameIndex;
}

bool UW_ClipInfoTakeover::GetIsClipSetUp() const
{
	return IsValid(M_ClipImage) && IsValid(M_LoadedClipAtlas) && M_ClipColumns > 0 && M_ClipRows > 0 &&
		M_ClipFrameCount > 0 && M_ClipFrameCount <= M_ClipColumns * M_ClipRows && M_ClipFramesPerSecond > 0.f;
}
