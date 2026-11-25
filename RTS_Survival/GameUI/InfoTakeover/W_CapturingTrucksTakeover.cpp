// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CapturingTrucksTakeover.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "PauseSafeProtoRichTextBlock.h"
#include "RTS_Survival/Enemy/Convoy/SurrenderingCargoTruck/W_SurrenderingTruckCountdown.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace CapturingTrucksTakeoverLayout
{
	// Card sizes match the spaced armour and clip field manuals (1080p reference height) so they read alike.
	constexpr float CardGap = 20.f;
	const FMargin GlassPanePadding(14.f, 14.f, 14.f, 18.f);
	const FVector2D DiagramSize(640.f, 360.f);
	const FMargin DiagramWellPadding(6.f);
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

	// The diagram, in diagram units (DiagramSize): a top-down road with a stopped truck facing right.
	const FLinearColor GroundColor(0.045f, 0.047f, 0.038f, 1.f);
	const FLinearColor RoadColor(0.105f, 0.098f, 0.08f, 1.f);
	constexpr float RoadTop = 150.f;
	constexpr float RoadHeight = 80.f;
	const FVector2D TruckCentre(420.f, 190.f);
	const FVector2D TruckBedPosition(352.f, 170.f);
	const FVector2D TruckBedSize(92.f, 40.f);
	const FVector2D TruckCabPosition(446.f, 174.f);
	const FVector2D TruckCabSize(30.f, 32.f);
	const FLinearColor TruckBedColor(0.23f, 0.25f, 0.16f, 1.f);
	const FLinearColor TruckCabColor(0.18f, 0.2f, 0.12f, 1.f);
	const FLinearColor TruckOutlineColor(0.02f, 0.02f, 0.015f, 1.f);
	constexpr float TruckCornerRadius = 4.f;
	constexpr float TruckOutlineWidth = 1.5f;

	// 20 m (CaptorSearchRadius 2000 cm), not to scale with the truck.
	constexpr float RingRadius = 120.f;
	const FLinearColor RingFillColor(0.78f, 0.23f, 0.17f, 0.07f);
	const FLinearColor RingOutlineColor(0.76f, 0.29f, 0.23f, 0.9f);
	constexpr float RingOutlineWidth = 2.f;
	const FVector2D RingLabelPosition(506.f, 88.f);
	constexpr int32 DiagramLabelFontSize = 14;

	// The truck's health bar with the 60 % mark (MinHealthFractionToSurrender).
	const FVector2D HealthBarPosition(372.f, 242.f);
	const FVector2D HealthBarSize(96.f, 8.f);
	constexpr float ShownHealthFraction = 0.85f;
	constexpr float SurrenderHealthFraction = 0.6f;
	const FLinearColor HealthBarBackColor(0.02f, 0.02f, 0.02f, 0.85f);
	const FLinearColor HealthBarFillColor(0.82f, 0.8f, 0.72f, 1.f);
	const FVector2D HealthMarkSize(2.f, 16.f);
	const FLinearColor HealthMarkColor(0.86f, 0.25f, 0.18f, 1.f);
	const FVector2D HealthMarkLabelOffset(-14.f, 14.f);
	constexpr int32 HealthMarkFontSize = 12;

	// The squad walks from left of the ring to inside it while the plate counts down, then the truck surrenders.
	constexpr float SquadIconSize = 56.f;
	const FVector2D SquadStartCentre(60.f, 190.f);
	constexpr float SquadEndCentreX = 316.f;
	constexpr float SquadWalkSeconds = 7.f;
	constexpr float SurrenderedHoldSeconds = 2.5f;
	constexpr float CountdownSeconds = 12.f;
	// The plate's bottom centre sits just over the truck.
	const FVector2D PlateAnchor(420.f, 162.f);
	const FVector2D PlateAlignment(0.5f, 1.f);
	const FLinearColor SurrenderedPlateColor(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor SurrenderedTextColor(0.72f, 0.9f, 0.62f, 1.f);
	constexpr float SurrenderedPlatePadding = 6.f;
	constexpr int32 SurrenderedFontSize = 18;
	constexpr int32 SurrenderedLetterSpacing = 80;

	const TCHAR* const SquadIconPath = TEXT(
		"/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/Squads/T_Ger_Squad_VultureHazmats."
		"T_Ger_Squad_VultureHazmats");
}

UW_CapturingTrucksTakeover::UW_CapturingTrucksTakeover(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	M_SquadIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(CapturingTrucksTakeoverLayout::SquadIconPath));
}

FText UW_CapturingTrucksTakeover::GetCapturingTrucksTitle()
{
	return NSLOCTEXT("CapturingTrucksTakeover", "Title", "Capturing Trucks");
}

FText UW_CapturingTrucksTakeover::GetCapturingTrucksSummary()
{
	return NSLOCTEXT("CapturingTrucksTakeover", "ArchiveSummary",
	                 "Kill the last escort of a convoy and its trucks stop. Each crew starts a 12 second countdown to "
	                 "blow up its truck.\n\n"
	                 "Capture: get an infantry squad within 20 m of the truck before the countdown ends and it "
	                 "surrenders. Vehicles cannot take a truck, and a truck under 60 % health only scuttles.\n\n"
	                 "A captured truck drives itself to the mine adit and pays 2.5 times its cargo when it arrives.\n\n"
	                 "Destroy: a destroyed or scuttled truck drops a cargo cache that your squads can scavenge. Faster "
	                 "and safer, but a smaller share.");
}

void UW_CapturingTrucksTakeover::SetupCapturingTrucks()
{
	SetKindLabel(NSLOCTEXT("CapturingTrucksTakeover", "KindLabel", "CONVOYS  -  CAPTURE OR DESTROY"));
	SetTitleText(GetCapturingTrucksTitle());
	SetDescriptionText(NSLOCTEXT("CapturingTrucksTakeover", "Description",
	                             "Kill a convoy's last escort and its trucks stop. Capture a truck for its whole cargo, "
	                             "or destroy it for a quick share."));
	BuildBody();
}

bool UW_CapturingTrucksTakeover::GetHasDiagram() const
{
	return IsValid(M_DiagramParts.CountdownPlate) && IsValid(M_DiagramParts.SquadIcon)
		&& IsValid(M_DiagramParts.SurrenderedTag);
}

void UW_CapturingTrucksTakeover::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// Slate's delta is real time, so the diagram plays while the takeover holds the game paused.
	AdvanceDiagram(InDeltaTime);
}

TArray<FClipTakeoverFact> UW_CapturingTrucksTakeover::MakeFacts() const
{
	// Mirrors the ASurrenderingCargoTruck defaults (SurrenderingCargoTruck.md, 2026-11-23): CountdownSeconds 12,
	// Captors SquadsOnly, CaptorSearchRadius 2000 cm, MinHealthFractionToSurrender 0.6, CapturedCargoMultiplier 2.5.
	// Change the text here when those defaults change.
	TArray<FClipTakeoverFact> Facts;
	FClipTakeoverFact& CountdownFact = Facts.AddDefaulted_GetRef();
	CountdownFact.Label = NSLOCTEXT("CapturingTrucksTakeover", "CountdownLabel", "COUNTDOWN");
	CountdownFact.Title = NSLOCTEXT("CapturingTrucksTakeover", "CountdownTitle", "12 seconds to scuttle");
	CountdownFact.Body = NSLOCTEXT("CapturingTrucksTakeover", "CountdownBody",
	                               "When the last escort dies, every truck stops and its crew starts a 12 second "
	                               "countdown to blow it up.");

	FClipTakeoverFact& CaptureFact = Facts.AddDefaulted_GetRef();
	CaptureFact.Label = NSLOCTEXT("CapturingTrucksTakeover", "CaptureLabel", "CAPTURE");
	CaptureFact.Title = NSLOCTEXT("CapturingTrucksTakeover", "CaptureTitle", "A squad within 20 m");
	CaptureFact.Body = NSLOCTEXT("CapturingTrucksTakeover", "CaptureBody",
	                             "Get an infantry squad within 20 m before the countdown ends and the truck "
	                             "surrenders. Vehicles cannot take it, and a truck under 60 % health only scuttles.");

	FClipTakeoverFact& ChoiceFact = Facts.AddDefaulted_GetRef();
	ChoiceFact.Label = NSLOCTEXT("CapturingTrucksTakeover", "ChoiceLabel", "CAPTURE OR DESTROY");
	ChoiceFact.Title = NSLOCTEXT("CapturingTrucksTakeover", "ChoiceTitle", "Full cargo or a quick share");
	ChoiceFact.Body = NSLOCTEXT("CapturingTrucksTakeover", "ChoiceBody",
	                            "A captured truck drives itself to the mine adit and pays 2.5 times its cargo there. "
	                            "A destroyed truck drops a cargo cache for your squads to scavenge.");
	return Facts;
}

void UW_CapturingTrucksTakeover::BuildBody()
{
	using namespace CapturingTrucksTakeoverLayout;
	UVerticalBox* const BodyColumn = GetBodyColumn();
	if (not IsValid(BodyColumn) || not IsValid(WidgetTree))
	{
		return;
	}
	BodyColumn->ClearChildren();
	M_DiagramParts = FCapturingTrucksDiagramParts();
	M_LoadedSquadIcon = nullptr;
	if (UPauseSafeProtoRichTextBlock* const DescriptionBlock = GetDescriptionBlock())
	{
		BodyColumn->AddChildToVerticalBox(DescriptionBlock)->SetPadding(DescriptionPadding);
	}
	UHorizontalBox* const CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	BodyColumn->AddChildToVerticalBox(CardRow);
	UHorizontalBoxSlot* const DiagramSlot = CardRow->AddChildToHorizontalBox(BuildDiagramCard());
	DiagramSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	DiagramSlot->SetPadding(FMargin(0.f, 0.f, CardGap, 0.f));
	UHorizontalBoxSlot* const FactsSlot = CardRow->AddChildToHorizontalBox(BuildFactsCard());
	FactsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	AdvanceDiagram(0.f);
}

UWidget* UW_CapturingTrucksTakeover::BuildDiagramCard()
{
	using namespace CapturingTrucksTakeoverLayout;
	UVerticalBox* const CardColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UBorder* const DiagramWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DiagramWell->SetBrush(M_Style.Skin.CardIconWell);
	DiagramWell->SetPadding(DiagramWellPadding);
	USizeBox* const DiagramBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DiagramBox->SetWidthOverride(DiagramSize.X);
	DiagramBox->SetHeightOverride(DiagramSize.Y);
	DiagramBox->SetClipping(EWidgetClipping::ClipToBounds);
	DiagramBox->SetContent(MakePictureWithGlass(BuildDiagram()));
	DiagramWell->SetContent(DiagramBox);
	CardColumn->AddChildToVerticalBox(DiagramWell)->SetHorizontalAlignment(HAlign_Center);

	UTextBlock* const CaptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CaptionText->SetText(NSLOCTEXT("CapturingTrucksTakeover", "DiagramCaption",
	                               "A squad reaches the stopped truck before the countdown ends: the truck surrenders "
	                               "and drives itself to the mine adit."));
	CaptionText->SetWrapTextAt(CaptionWrapWidth);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		CaptionText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.BodyFont, BodyFontSize),
		M_Style.Skin.DescriptionColor);
	CardColumn->AddChildToVerticalBox(CaptionText)->SetPadding(CaptionPadding);
	return WrapInGlassPane(CardColumn);
}

UWidget* UW_CapturingTrucksTakeover::BuildDiagram()
{
	using namespace CapturingTrucksTakeoverLayout;
	UOverlay* const DiagramOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UBorder* const Ground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Ground->SetBrush(FSlateColorBrush(GroundColor));
	UOverlaySlot* const GroundSlot = DiagramOverlay->AddChildToOverlay(Ground);
	GroundSlot->SetHorizontalAlignment(HAlign_Fill);
	GroundSlot->SetVerticalAlignment(VAlign_Fill);

	UCanvasPanel* const Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	UOverlaySlot* const CanvasSlot = DiagramOverlay->AddChildToOverlay(Canvas);
	CanvasSlot->SetHorizontalAlignment(HAlign_Fill);
	CanvasSlot->SetVerticalAlignment(VAlign_Fill);
	// Draw order: road, ring, truck, health bar, squad, then the plates on top.
	AddRoadAndTruck(*Canvas);
	AddCaptureRing(*Canvas);
	AddHealthBar(*Canvas);
	AddSquadIcon(*Canvas);
	AddCountdownPlate(*Canvas);
	AddSurrenderedTag(*Canvas);
	return DiagramOverlay;
}

void UW_CapturingTrucksTakeover::AddRoadAndTruck(UCanvasPanel& Canvas) const
{
	using namespace CapturingTrucksTakeoverLayout;
	AddBox(Canvas, FVector2D(0.f, RoadTop), FVector2D(DiagramSize.X, RoadHeight), FSlateColorBrush(RoadColor));
	AddBox(Canvas, TruckBedPosition, TruckBedSize,
	       FSlateRoundedBoxBrush(TruckBedColor, TruckCornerRadius, TruckOutlineColor, TruckOutlineWidth));
	AddBox(Canvas, TruckCabPosition, TruckCabSize,
	       FSlateRoundedBoxBrush(TruckCabColor, TruckCornerRadius, TruckOutlineColor, TruckOutlineWidth));
}

void UW_CapturingTrucksTakeover::AddCaptureRing(UCanvasPanel& Canvas) const
{
	using namespace CapturingTrucksTakeoverLayout;
	const FVector2D RingSize(RingRadius * 2.f, RingRadius * 2.f);
	AddBox(Canvas, TruckCentre - RingSize * 0.5f, RingSize,
	       FSlateRoundedBoxBrush(RingFillColor, RingRadius, RingOutlineColor, RingOutlineWidth));
	AddDiagramText(Canvas, NSLOCTEXT("CapturingTrucksTakeover", "RingLabel", "20 m"), RingLabelPosition,
	               DiagramLabelFontSize, RingOutlineColor);
}

void UW_CapturingTrucksTakeover::AddHealthBar(UCanvasPanel& Canvas) const
{
	using namespace CapturingTrucksTakeoverLayout;
	AddBox(Canvas, HealthBarPosition, HealthBarSize, FSlateColorBrush(HealthBarBackColor));
	AddBox(Canvas, HealthBarPosition, FVector2D(HealthBarSize.X * ShownHealthFraction, HealthBarSize.Y),
	       FSlateColorBrush(HealthBarFillColor));
	const FVector2D MarkPosition(HealthBarPosition.X + HealthBarSize.X * SurrenderHealthFraction
	                             - HealthMarkSize.X * 0.5f,
	                             HealthBarPosition.Y + (HealthBarSize.Y - HealthMarkSize.Y) * 0.5f);
	AddBox(Canvas, MarkPosition, HealthMarkSize, FSlateColorBrush(HealthMarkColor));
	AddDiagramText(Canvas, NSLOCTEXT("CapturingTrucksTakeover", "HealthMarkLabel", "60 %"),
	               MarkPosition + HealthMarkLabelOffset, HealthMarkFontSize, HealthMarkColor);
}

void UW_CapturingTrucksTakeover::AddSquadIcon(UCanvasPanel& Canvas)
{
	using namespace CapturingTrucksTakeoverLayout;
	UImage* const SquadImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	UTexture2D* const LoadedIcon = M_SquadIcon.LoadSynchronous();
	if (IsValid(LoadedIcon))
	{
		M_LoadedSquadIcon = LoadedIcon;
		SquadImage->SetBrushFromTexture(LoadedIcon, false);
	}
	else
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Capturing trucks takeover: squad icon %s could not be loaded; the diagram shows a blank square."),
			*M_SquadIcon.ToString()));
	}
	UCanvasPanelSlot* const IconSlot = Canvas.AddChildToCanvas(SquadImage);
	IconSlot->SetPosition(SquadStartCentre - FVector2D(SquadIconSize * 0.5f));
	IconSlot->SetSize(FVector2D(SquadIconSize));
	M_DiagramParts.SquadIcon = SquadImage;
}

void UW_CapturingTrucksTakeover::AddCountdownPlate(UCanvasPanel& Canvas)
{
	using namespace CapturingTrucksTakeoverLayout;
	// The class the trucks show over their heads in the world; its native layout needs no asset.
	UW_SurrenderingTruckCountdown* const CountdownPlate = WidgetTree->ConstructWidget<UW_SurrenderingTruckCountdown>(
		UW_SurrenderingTruckCountdown::StaticClass());
	if (not IsValid(CountdownPlate))
	{
		RTSFunctionLibrary::ReportError(TEXT("Capturing trucks takeover: the scuttle countdown plate could not be "
			"created; the diagram shows no countdown."));
		return;
	}
	UCanvasPanelSlot* const PlateSlot = Canvas.AddChildToCanvas(CountdownPlate);
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetAlignment(PlateAlignment);
	PlateSlot->SetPosition(PlateAnchor);
	M_DiagramParts.CountdownPlate = CountdownPlate;
}

void UW_CapturingTrucksTakeover::AddSurrenderedTag(UCanvasPanel& Canvas)
{
	using namespace CapturingTrucksTakeoverLayout;
	UBorder* const TagPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	TagPlate->SetBrush(FSlateColorBrush(SurrenderedPlateColor));
	TagPlate->SetPadding(FMargin(SurrenderedPlatePadding));
	UTextBlock* const TagText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	TagText->SetText(NSLOCTEXT("CapturingTrucksTakeover", "SurrenderedTag", "SURRENDERED"));
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		TagText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, SurrenderedFontSize,
		                                               SurrenderedLetterSpacing), FSlateColor(SurrenderedTextColor));
	TagPlate->SetContent(TagText);
	TagPlate->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* const TagSlot = Canvas.AddChildToCanvas(TagPlate);
	TagSlot->SetAutoSize(true);
	TagSlot->SetAlignment(PlateAlignment);
	TagSlot->SetPosition(PlateAnchor);
	M_DiagramParts.SurrenderedTag = TagPlate;
}

UWidget* UW_CapturingTrucksTakeover::AddBox(UCanvasPanel& Canvas, const FVector2D& Position, const FVector2D& Size,
                                            const FSlateBrush& Brush) const
{
	UBorder* const Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Box->SetBrush(Brush);
	UCanvasPanelSlot* const BoxSlot = Canvas.AddChildToCanvas(Box);
	BoxSlot->SetPosition(Position);
	BoxSlot->SetSize(Size);
	return Box;
}

void UW_CapturingTrucksTakeover::AddDiagramText(UCanvasPanel& Canvas, const FText& Text, const FVector2D& Position,
                                                const int32 FontSize, const FLinearColor& Color) const
{
	UTextBlock* const LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetText(Text);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		LabelText, UnitVariantSelectionWidgets::MakeFont(M_Style.Skin.HeadingFont, FontSize), FSlateColor(Color));
	UCanvasPanelSlot* const LabelSlot = Canvas.AddChildToCanvas(LabelText);
	LabelSlot->SetAutoSize(true);
	LabelSlot->SetPosition(Position);
}

UWidget* UW_CapturingTrucksTakeover::BuildFactsCard()
{
	UVerticalBox* const FactsColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	const TArray<FClipTakeoverFact> Facts = MakeFacts();
	for (int32 FactIndex = 0; FactIndex < Facts.Num(); ++FactIndex)
	{
		AddFactText(*FactsColumn, Facts[FactIndex], FactIndex == 0);
	}
	return WrapInGlassPane(FactsColumn);
}

UWidget* UW_CapturingTrucksTakeover::WrapInGlassPane(UWidget* Content)
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
	GlassPane->SetPadding(CapturingTrucksTakeoverLayout::GlassPanePadding);
	GlassPane->SetContent(Content);
	RegisterGlassPane(*GlassPane);
	GlassBlur->SetContent(GlassPane);
	return GlassBlur;
}

void UW_CapturingTrucksTakeover::AddFactText(UVerticalBox& FactsColumn, const FClipTakeoverFact& Fact,
                                             const bool bIsFirst) const
{
	using namespace CapturingTrucksTakeoverLayout;
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

void UW_CapturingTrucksTakeover::AdvanceDiagram(const float RealDeltaSeconds)
{
	using namespace CapturingTrucksTakeoverLayout;
	if (not GetHasDiagram())
	{
		return;
	}
	const float LoopLengthSeconds = SquadWalkSeconds + SurrenderedHoldSeconds;
	M_DiagramParts.LoopSeconds = FMath::Fmod(M_DiagramParts.LoopSeconds + RealDeltaSeconds, LoopLengthSeconds);
	const float WalkAlpha = FMath::Clamp(M_DiagramParts.LoopSeconds / SquadWalkSeconds, 0.f, 1.f);
	const float SquadOffsetX = (SquadEndCentreX - SquadStartCentre.X) * WalkAlpha;
	M_DiagramParts.SquadIcon->SetRenderTranslation(FVector2D(SquadOffsetX, 0.f));

	const bool bHasSurrendered = M_DiagramParts.LoopSeconds >= SquadWalkSeconds;
	M_DiagramParts.CountdownPlate->SetVisibility(bHasSurrendered
		                                             ? ESlateVisibility::Collapsed
		                                             : ESlateVisibility::HitTestInvisible);
	M_DiagramParts.SurrenderedTag->SetVisibility(bHasSurrendered
		                                             ? ESlateVisibility::HitTestInvisible
		                                             : ESlateVisibility::Collapsed);
	if (bHasSurrendered)
	{
		M_DiagramParts.ShownCountdownSeconds = INDEX_NONE;
		return;
	}
	const float SecondsLeft = CountdownSeconds - M_DiagramParts.LoopSeconds;
	M_DiagramParts.CountdownPlate->SetCountdown(SecondsLeft, SecondsLeft / CountdownSeconds,
	                                            NSLOCTEXT("CapturingTrucksTakeover", "CountdownFormat",
	                                                      "SCUTTLING {0}"));
	M_DiagramParts.ShownCountdownSeconds = FMath::Max(0, FMath::CeilToInt(SecondsLeft));
}
