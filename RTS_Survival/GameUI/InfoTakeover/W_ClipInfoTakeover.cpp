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
#include "Components/WidgetSwitcher.h"
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

FClipTakeoverClip UW_ClipInfoTakeover::GetPageClip(const int32 PageIndex) const
{
	FClipTakeoverClip Clip;
	Clip.Atlas = M_ClipAtlas;
	Clip.Columns = M_ClipColumns;
	Clip.Rows = M_ClipRows;
	Clip.FrameCount = M_ClipFrameCount;
	Clip.FramesPerSecond = M_ClipFramesPerSecond;
	Clip.EndHoldSeconds = M_ClipEndHoldSeconds;
	return Clip;
}

TArray<FClipTakeoverFact> UW_ClipInfoTakeover::MakePageFacts(const int32 PageIndex) const
{
	return PageIndex == 0 ? MakeFacts() : TArray<FClipTakeoverFact>();
}

FText UW_ClipInfoTakeover::GetPageClipCaption(const int32 PageIndex) const
{
	return PageIndex == 0 ? GetClipCaption() : FText::GetEmpty();
}

bool UW_ClipInfoTakeover::GetHasLoadedClip() const
{
	return M_ClipPages.IsValidIndex(M_ActiveClipPage) && IsValid(M_ClipPages[M_ActiveClipPage].LoadedAtlas);
}

void UW_ClipInfoTakeover::BuildClipBody()
{
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_ClipPages.Reset();
	M_ActiveClipPage = 0;
	M_ClipSeconds = 0.f;
	M_ShownClipFrame = INDEX_NONE;
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(ClipInfoTakeoverLayout::DescriptionPadding);
	}
	// Child order follows the page index; a one-page manual has a switcher with one child, which lays out the same.
	M_ClipPageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass());
	const int32 PageCount = FMath::Max(GetPageCount(), 1);
	M_ClipPages.SetNum(PageCount);
	for (int32 PageIndex = 0; PageIndex < PageCount; ++PageIndex)
	{
		M_ClipPageSwitcher->AddChild(BuildClipPage(PageIndex));
	}
	M_ClipPageSwitcher->SetActiveWidgetIndex(0);
	BodyColumn->AddChildToVerticalBox(M_ClipPageSwitcher);
	ShowClipFrame(0);
}

bool UW_ClipInfoTakeover::TryShowPage(const int32 PageIndex)
{
	if (PageIndex < 0 || PageIndex >= GetPageCount() || not M_ClipPages.IsValidIndex(PageIndex)
		|| not IsValid(M_ClipPageSwitcher))
	{
		return false;
	}
	M_ActiveClipPage = PageIndex;
	M_ClipPageSwitcher->SetActiveWidgetIndex(PageIndex);
	// The shown page's clip starts at its beginning, also when PREVIOUS returns to it.
	M_ClipSeconds = 0.f;
	M_ShownClipFrame = INDEX_NONE;
	ShowClipFrame(0);
	ApplyClipPageTexts(PageIndex);
	RestartTyping();
	return true;
}

UWidget* UW_ClipInfoTakeover::BuildClipPage(const int32 PageIndex)
{
	using namespace ClipInfoTakeoverLayout;
	UHorizontalBox* const CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UHorizontalBoxSlot* const ClipSlot = CardRow->AddChildToHorizontalBox(BuildClipCard(PageIndex));
	ClipSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	ClipSlot->SetPadding(FMargin(0.f, 0.f, CardGap, 0.f));
	UHorizontalBoxSlot* const FactsSlot = CardRow->AddChildToHorizontalBox(BuildFactsCard(PageIndex));
	FactsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return CardRow;
}

UWidget* UW_ClipInfoTakeover::BuildClipCard(const int32 PageIndex)
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

	FClipTakeoverPagePlayback& Playback = M_ClipPages[PageIndex];
	Playback.Clip = GetPageClip(PageIndex);
	UTexture2D* const LoadedAtlas = Playback.Clip.Atlas.LoadSynchronous();
	if (IsValid(LoadedAtlas))
	{
		Playback.LoadedAtlas = LoadedAtlas;
		Playback.Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Playback.Image->SetBrushFromTexture(LoadedAtlas, false);
		ClipBox->SetContent(MakePictureWithGlass(Playback.Image));
	}
	else
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("%s: clip atlas %s of page %d could not be loaded; the card shows an empty well."),
			*GetClass()->GetName(), *Playback.Clip.Atlas.ToString(), PageIndex));
	}

	UTextBlock* const CaptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CaptionText->SetText(GetPageClipCaption(PageIndex));
	CaptionText->SetWrapTextAt(CaptionWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		CaptionText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, BodyFontSize),
		M_Style.Skin.DescriptionColor);
	CardColumn->AddChildToVerticalBox(CaptionText)->SetPadding(CaptionPadding);
	return WrapInGlassPane(CardColumn);
}

UWidget* UW_ClipInfoTakeover::BuildFactsCard(const int32 PageIndex)
{
	UVerticalBox* const FactsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const TArray<FClipTakeoverFact> Facts = MakePageFacts(PageIndex);
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
	const FMargin& LeadPadding = bIsFirst ? FirstFactLabelPadding : FactLabelPadding;
	// A fact without a label starts at its title, which then takes the label's spacing from the fact above.
	const bool bHasLabel = not Fact.Label.IsEmpty();
	if (bHasLabel)
	{
		UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		LabelText->SetText(Fact.Label);
		UnitVariantSelectionWidgets::ApplyTextOnMetal(
			LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, FactLabelFontSize,
			                                                 FactLabelLetterSpacing), M_Style.Skin.LabelColor);
		FactsColumn.AddChildToVerticalBox(LabelText)->SetPadding(LeadPadding);
	}

	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(Fact.Title);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, FactTitleFontSize),
		M_Style.Skin.TextColor);
	const FMargin TitlePadding = bHasLabel
		                             ? FactTitlePadding
		                             : FMargin(0.f, LeadPadding.Top, 0.f, FactTitlePadding.Bottom);
	FactsColumn.AddChildToVerticalBox(TitleText)->SetPadding(TitlePadding);

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
	const FClipTakeoverPagePlayback* const Playback = GetActiveClipPlayback();
	if (Playback == nullptr)
	{
		return;
	}
	const FClipTakeoverClip& Clip = Playback->Clip;
	const float FrameSeconds = 1.f / Clip.FramesPerSecond;
	const float LoopSeconds = Clip.FrameCount * FrameSeconds + Clip.EndHoldSeconds;
	M_ClipSeconds = FMath::Fmod(M_ClipSeconds + RealDeltaSeconds, LoopSeconds);
	// Past the last frame the clip holds on it until the loop starts over.
	const int32 FrameIndex = FMath::Min(FMath::FloorToInt32(M_ClipSeconds / FrameSeconds), Clip.FrameCount - 1);
	if (FrameIndex != M_ShownClipFrame)
	{
		ShowClipFrame(FrameIndex);
	}
}

void UW_ClipInfoTakeover::ShowClipFrame(const int32 FrameIndex)
{
	using namespace ClipInfoTakeoverLayout;
	const FClipTakeoverPagePlayback* const Playback = GetActiveClipPlayback();
	if (Playback == nullptr)
	{
		return;
	}
	const FClipTakeoverClip& Clip = Playback->Clip;
	const int32 Column = FrameIndex % Clip.Columns;
	const int32 Row = FrameIndex / Clip.Columns;
	const float CellWidth = 1.f / Clip.Columns;
	const float CellHeight = 1.f / Clip.Rows;
	const float InsetU = CellEdgeInsetTexels / FMath::Max(1, Playback->LoadedAtlas->GetSizeX());
	const float InsetV = CellEdgeInsetTexels / FMath::Max(1, Playback->LoadedAtlas->GetSizeY());
	const FVector2f CellMin(Column * CellWidth + InsetU, Row * CellHeight + InsetV);
	const FVector2f CellMax((Column + 1) * CellWidth - InsetU, (Row + 1) * CellHeight - InsetV);
	FSlateBrush ClipBrush = Playback->Image->GetBrush();
	ClipBrush.SetUVRegion(FBox2f(CellMin, CellMax));
	Playback->Image->SetBrush(ClipBrush);
	M_ShownClipFrame = FrameIndex;
}

const FClipTakeoverPagePlayback* UW_ClipInfoTakeover::GetActiveClipPlayback() const
{
	if (not M_ClipPages.IsValidIndex(M_ActiveClipPage) || not GetIsClipSetUp(M_ClipPages[M_ActiveClipPage]))
	{
		return nullptr;
	}
	return &M_ClipPages[M_ActiveClipPage];
}

bool UW_ClipInfoTakeover::GetIsClipSetUp(const FClipTakeoverPagePlayback& Playback)
{
	const FClipTakeoverClip& Clip = Playback.Clip;
	return IsValid(Playback.Image) && IsValid(Playback.LoadedAtlas) && Clip.Columns > 0 && Clip.Rows > 0 &&
		Clip.FrameCount > 0 && Clip.FrameCount <= Clip.Columns * Clip.Rows && Clip.FramesPerSecond > 0.f;
}
