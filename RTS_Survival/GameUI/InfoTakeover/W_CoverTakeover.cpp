// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CoverTakeover.h"

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

namespace CoverTakeoverLayout
{
	// Sizes are for the 1080p reference height, inside the base frame's 1240 wide panel; they match the camera
	// controls cards so both field manuals read alike.
	constexpr float CardGap = 20.f;
	const FMargin GlassPanePadding(14.f, 14.f, 14.f, 18.f);
	const FVector2D ScreenshotSize(330.f, 186.f);
	const FMargin ScreenshotWellPadding(6.f);
	const FMargin DescriptionPadding(0.f, 0.f, 0.f, 22.f);
	const FMargin CardLabelPadding(0.f, 16.f, 0.f, 2.f);
	const FMargin CardTitlePadding(0.f, 0.f, 0.f, 6.f);
	constexpr float CardTextWrapWidth = 336.f;
	constexpr int32 CardLabelFontSize = 12;
	constexpr int32 CardLabelLetterSpacing = 100;
	constexpr int32 CardTitleFontSize = 19;
	constexpr int32 CardBodyFontSize = 15;

	// The colours of the preview's stance materials: MI_StrongCover, MI_WeakCover and MI_NoCover.
	const FLinearColor StrongCoverColor(0.f, 1.f, 0.f);
	const FLinearColor WeakCoverColor(0.54f, 0.547f, 0.f);
	const FLinearColor NoCoverColor(1.f, 0.f, 0.f);

	const TCHAR* const ScreenshotFolder = TEXT("/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/Cover/");

	TSoftObjectPtr<UTexture2D> MakeScreenshotPath(const TCHAR* const AssetName)
	{
		return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
			FString::Printf(TEXT("%s%s.%s"), ScreenshotFolder, AssetName, AssetName)));
	}
}

UW_CoverTakeover::UW_CoverTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	using namespace CoverTakeoverLayout;
	M_StrongCoverScreenshot = MakeScreenshotPath(TEXT("T_CoverPreview_Green"));
	M_WeakCoverScreenshot = MakeScreenshotPath(TEXT("T_CoverPreview_Yellow"));
	M_NoCoverScreenshot = MakeScreenshotPath(TEXT("T_CoverPreview_Red"));
}

FText UW_CoverTakeover::GetCoverTitle()
{
	return NSLOCTEXT("CoverTakeover", "Title", "Cover");
}

FText UW_CoverTakeover::GetCoverSummary()
{
	return NSLOCTEXT("CoverTakeover", "ArchiveSummary",
	                 "With only infantry selected, the move preview shows where every soldier will stand, coloured by "
	                 "the cover he gets there. Right-click to send them.\n\n"
	                 "Green: strong cover. Lying flat behind something low, or in a trench.\n\n"
	                 "Yellow: weak cover. Crouching behind something waist high, or standing behind a wall and "
	                 "leaning out to fire.\n\n"
	                 "Red: no cover. Standing in the open.\n\n"
	                 "Cover only stops fire from the side it faces.");
}

void UW_CoverTakeover::SetupCover()
{
	HideKindLabel();
	SetTitleText(GetCoverTitle());
	SetDescriptionText(NSLOCTEXT("CoverTakeover", "Description",
	                             "With only infantry selected, the move preview shows where every soldier will "
	                             "stand, coloured by the cover he gets there. Right-click to send them."));
	BuildBody();
}

TArray<FCoverTakeoverCard> UW_CoverTakeover::MakeCards() const
{
	// What the colours mean comes from FSquadMovePreviewStances::GetStanceForPosition and the stance meshes'
	// materials: prone and trench cover are green, crouch and standing cover yellow, no cover red.
	using namespace CoverTakeoverLayout;
	TArray<FCoverTakeoverCard> Cards;
	FCoverTakeoverCard& StrongCard = Cards.AddDefaulted_GetRef();
	StrongCard.ColourLabel = NSLOCTEXT("CoverTakeover", "GreenLabel", "GREEN");
	StrongCard.ColourLabelColor = StrongCoverColor;
	StrongCard.Title = NSLOCTEXT("CoverTakeover", "GreenTitle", "Strong cover");
	StrongCard.Body = NSLOCTEXT("CoverTakeover", "GreenBody",
	                            "Lying flat behind something low, or in a trench.");
	StrongCard.Screenshot = M_StrongCoverScreenshot;

	FCoverTakeoverCard& WeakCard = Cards.AddDefaulted_GetRef();
	WeakCard.ColourLabel = NSLOCTEXT("CoverTakeover", "YellowLabel", "YELLOW");
	WeakCard.ColourLabelColor = WeakCoverColor;
	WeakCard.Title = NSLOCTEXT("CoverTakeover", "YellowTitle", "Weak cover");
	WeakCard.Body = NSLOCTEXT("CoverTakeover", "YellowBody",
	                          "Crouching behind something waist high, or standing behind a wall and leaning out to "
	                          "fire.");
	WeakCard.Screenshot = M_WeakCoverScreenshot;

	FCoverTakeoverCard& NoCoverCard = Cards.AddDefaulted_GetRef();
	NoCoverCard.ColourLabel = NSLOCTEXT("CoverTakeover", "RedLabel", "RED");
	NoCoverCard.ColourLabelColor = NoCoverColor;
	NoCoverCard.Title = NSLOCTEXT("CoverTakeover", "RedTitle", "No cover");
	NoCoverCard.Body = NSLOCTEXT("CoverTakeover", "RedBody",
	                             "Standing in the open. Cover only stops fire from the side it faces.");
	NoCoverCard.Screenshot = M_NoCoverScreenshot;
	return Cards;
}

void UW_CoverTakeover::BuildBody()
{
	using namespace CoverTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_Screenshots.Reset();
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	UHorizontalBox* const CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BodyColumn->AddChildToVerticalBox(CardRow);
	const TArray<FCoverTakeoverCard> Cards = MakeCards();
	for (int32 CardIndex = 0; CardIndex < Cards.Num(); ++CardIndex)
	{
		UHorizontalBoxSlot* const CardSlot = CardRow->AddChildToHorizontalBox(BuildCard(Cards[CardIndex]));
		CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		const bool bIsLastCard = CardIndex == Cards.Num() - 1;
		CardSlot->SetPadding(FMargin(0.f, 0.f, bIsLastCard ? 0.f : CardGap, 0.f));
	}
}

UWidget* UW_CoverTakeover::BuildCard(const FCoverTakeoverCard& Card)
{
	using namespace CoverTakeoverLayout;
	UVerticalBox* const CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	CardColumn->AddChildToVerticalBox(BuildScreenshotWell(Card.Screenshot))->SetHorizontalAlignment(HAlign_Center);
	AddCardText(*CardColumn, Card);

	// The vehicle takeover's facts pane: blurred glass over the plate.
	UBackgroundBlur* const GlassBlur = WidgetTree->ConstructWidget<UBackgroundBlur>(UBackgroundBlur::StaticClass());
	GlassBlur->SetBlurStrength(M_Style.Skin.VariantSectionBlurStrength);
	GlassBlur->SetLowQualityFallbackBrush(FSlateNoResource());
	GlassBlur->SetPadding(FMargin(0.f));
	GlassBlur->SetHorizontalAlignment(HAlign_Fill);
	GlassBlur->SetVerticalAlignment(VAlign_Fill);
	UBorder* const GlassPane = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	GlassPane->SetBrush(M_Style.Skin.VariantSectionGlass);
	GlassPane->SetPadding(GlassPanePadding);
	GlassPane->SetContent(CardColumn);
	GlassBlur->SetContent(GlassPane);
	return GlassBlur;
}

UWidget* UW_CoverTakeover::BuildScreenshotWell(const TSoftObjectPtr<UTexture2D>& Screenshot)
{
	using namespace CoverTakeoverLayout;
	UBorder* const ScreenshotWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	ScreenshotWell->SetBrush(M_Style.Skin.CardIconWell);
	ScreenshotWell->SetPadding(ScreenshotWellPadding);
	USizeBox* const ScreenshotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ScreenshotBox->SetWidthOverride(ScreenshotSize.X);
	ScreenshotBox->SetHeightOverride(ScreenshotSize.Y);
	ScreenshotWell->SetContent(ScreenshotBox);
	UTexture2D* const LoadedScreenshot = Screenshot.LoadSynchronous();
	if (not IsValid(LoadedScreenshot))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Cover takeover: screenshot %s could not be loaded; the card shows an empty well."),
			*Screenshot.ToString()));
		return ScreenshotWell;
	}
	M_Screenshots.Add(LoadedScreenshot);
	UImage* const ScreenshotImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	ScreenshotImage->SetBrushFromTexture(LoadedScreenshot, false);
	ScreenshotBox->SetContent(ScreenshotImage);
	return ScreenshotWell;
}

void UW_CoverTakeover::AddCardText(UVerticalBox& CardColumn, const FCoverTakeoverCard& Card) const
{
	using namespace CoverTakeoverLayout;
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Card.ColourLabel);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, CardLabelFontSize,
		                                                 CardLabelLetterSpacing), FSlateColor(Card.ColourLabelColor));
	CardColumn.AddChildToVerticalBox(LabelText)->SetPadding(CardLabelPadding);

	UTextBlock* const TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TitleText->SetText(Card.Title);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TitleText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, CardTitleFontSize),
		M_Style.Skin.TextColor);
	CardColumn.AddChildToVerticalBox(TitleText)->SetPadding(CardTitlePadding);

	UTextBlock* const BodyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	BodyText->SetText(Card.Body);
	BodyText->SetWrapTextAt(CardTextWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		BodyText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, CardBodyFontSize),
		M_Style.Skin.DescriptionColor);
	CardColumn.AddChildToVerticalBox(BodyText);
}
