// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/W_WorldTechWheel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignTech/WorldCampaignTechManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/TechWheelLabelPlacement.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/TechWheelNodeButton.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/TechWheel/W_TechWheelPaintLayer.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

#define LOCTEXT_NAMESPACE "WorldTechWheel"

// Layout of UI sketch sheet 13 (Saved/WorldMapUIDesigns/build_ui_sheets6.py), in UI units at the 1920x1080
// reference. The sketch's mock area is 1872x1100, so the ring radii are scaled to 95 % to fit under the strip.
namespace TechWheelLayout
{
	const FVector2D ScreenSize(1920.f, 1080.f);
	const FVector2D WheelCentre(664.f, 572.f);
	// Ring 0 is the HQ; sketch rings 96, 184, 276, 366 and 430 at 95 %.
	constexpr float RingRadii[] = {0.f, 91.f, 175.f, 262.f, 348.f, 409.f};
	constexpr float TechnologyIconSize = 86.f;
	constexpr float RimIconSize = 104.f;
	// The wedges and their bright rim arc end this far outside the outer ring.
	constexpr float WedgeOuterMargin = RimIconSize * 0.62f;
	constexpr int32 ArcSegmentsPerSector = 24;
	constexpr float StartHexagonRadius = RimIconSize * 0.42f;
	constexpr float PipHalfDiagonal = 7.f;
	const FVector2D PipButtonSize(26.f, 26.f);
	// Outline radii as fractions of the icon size (sheet 13: branch 0.57, key 0.66, reach and selection 0.75).
	constexpr float BranchOutlineScale = 0.57f;
	constexpr float KeyOutlineScale = 0.66f;
	constexpr float ReachOutlineScale = 0.75f;
	constexpr float ChordTargetOutlineScale = 0.64f;
	constexpr float LockedOutlineOpacity = 0.45f;
	constexpr float WedgeOpacity = 0.10f;
	constexpr float RimArcOpacity = 0.85f;
	constexpr float BoughtPathThickness = 4.f;
	constexpr float OtherPathThickness = 2.f;
	constexpr float PathDashLength = 6.f;
	constexpr float OutlineThickness = 3.f;
	constexpr float ReachOutlineThickness = 2.5f;
	constexpr float ReachOutlineDashLength = 7.f;
	constexpr float ChordThickness = 3.f;
	constexpr float ChordDashLength = 4.f;
	constexpr float RimArcThickness = 4.f;
	constexpr float PipOutlineThickness = 2.f;
	// Key badge: a round plate at the icon's upper right with the needed technology's icon and a small marker.
	constexpr float KeyBadgeOffsetXScale = 0.36f;
	constexpr float KeyBadgeOffsetYScale = -0.40f;
	constexpr float KeyBadgeSize = 34.f;
	constexpr float KeyBadgeIconSize = 26.f;
	constexpr float KeyBadgeOutlineWidth = 2.f;
	constexpr float KeyMarkerSize = 16.f;
	constexpr float KeyMarkerOffset = 14.f;
	constexpr float KeyMarkThickness = 2.f;
	// Branch names sit outside the rim, further out at the sides (sketch: outer + 26 + 62 * |cos|).
	constexpr float BranchLabelGap = 26.f;
	constexpr float BranchLabelSideExtra = 62.f;
	constexpr float BranchLabelVerticalSquash = 0.985f;
	const FVector2D BranchLabelSize(220.f, 22.f);
	// Technology names sit on a small dark plate, so they stay readable on the path lines; where each one goes
	// is chosen by TechWheelLabelPlacement.
	const FMargin NodeLabelPlatePadding(5.f, 2.f, 5.f, 2.f);
	constexpr float NodeLabelPlateOpacity = 0.8f;
	constexpr float NodeLabelPlateCornerRadius = 3.f;
	// Slate draws a font of N points N * 96 / 72 units high; letter spacing is in thousandths of that.
	constexpr float FontPointsToUnits = 96.f / 72.f;
	constexpr float LetterSpacingUnitsPerEm = 1000.f;
	// Only used when the font cannot be measured: average glyph width and line height per font unit.
	constexpr float FallbackGlyphWidthScale = 0.55f;
	constexpr float FallbackLineHeightScale = 1.2f;

	constexpr float StripHeight = 56.f;
	const FVector2D TitlePosition(24.f, 13.f);
	const FVector2D CountPosition(272.f, 21.f);
	const FVector2D BackButtonPosition(1736.f, 10.f);
	const FVector2D BackButtonSize(166.f, 36.f);
	// The two resource cells end this far left of the back button, in the bolted cells of the map bar
	// (W_WorldCampaignBar: CellFramePadding and CellGap).
	const FMargin CounterCellPadding(12.f, 4.f, 14.f, 5.f);
	constexpr float CounterCellGap = 10.f;

	const FVector2D DetailPanelPosition(1330.f, 70.f);
	const FVector2D DetailPanelSize(570.f, 1000.f);
	const FMargin DetailPanelPadding(24.f, 20.f, 24.f, 18.f);
	constexpr float DetailIconWellHeight = 230.f;
	const FVector2D DetailIconSize(200.f, 200.f);
	const FVector2D PriceIconSize(30.f, 30.f);
	const FVector2D ResearchButtonSize(522.f, 48.f);

	constexpr int32 TitleFontSize = 17;
	constexpr int32 TitleLetterSpacing = 500;
	constexpr int32 CountFontSize = 10;
	constexpr int32 CountLetterSpacing = 60;
	constexpr int32 SmallLabelFontSize = 8;
	constexpr int32 SmallLabelLetterSpacing = 200;
	constexpr int32 CounterValueFontSize = 15;
	constexpr int32 ButtonFontSize = 10;
	constexpr int32 ButtonLetterSpacing = 100;
	constexpr int32 BranchLabelFontSize = 11;
	constexpr int32 BranchLabelLetterSpacing = 270;
	constexpr int32 NodeLabelFontSize = 8;
	constexpr int32 NodeLabelLetterSpacing = 40;
	constexpr int32 StartLabelFontSize = 16;
	constexpr int32 StartLabelLetterSpacing = 120;
	constexpr int32 DetailKindFontSize = 9;
	constexpr int32 DetailNameFontSize = 21;
	constexpr int32 DetailNameLetterSpacing = 200;
	constexpr int32 DetailBodyFontSize = 11;
	constexpr int32 DetailValueFontSize = 11;
	constexpr int32 DetailPriceFontSize = 20;

	const FLinearColor ScreenColor(FColor::FromHex(TEXT("14181A")));
	const FLinearColor DarkNodeFill(FColor::FromHex(TEXT("15191B")));
	const FLinearColor IconWellColor(FColor::FromHex(TEXT("07090A")));
	const FLinearColor BadgeFill(FColor::FromHex(TEXT("0B0D0E")));
	const FLinearColor NodeTextColor(FColor::FromHex(TEXT("F2EDE0")));
	const FLinearColor LockedTextColor(FColor::FromHex(TEXT("8D9498")));
	const FLinearColor SelectionColor = FLinearColor::White;
	// Icon tints per state; a UImage cannot desaturate, so locked icons are darkened instead.
	const FLinearColor BoughtIconTint = FLinearColor::White;
	const FLinearColor ReachableIconTint(0.92f, 0.92f, 0.92f, 1.f);
	const FLinearColor LockedIconTint(0.17f, 0.18f, 0.19f, 1.f);
	const FLinearColor KeyIconMissingTint(0.75f, 0.75f, 0.75f, 1.f);

	// Canvas z orders: under paint, nodes, branch and HQ labels, key badge parts, over paint, technology names,
	// panels. The technology names are above the badges and their marks, so nothing of the wheel covers a name.
	constexpr int32 ZUnderPaint = 0;
	constexpr int32 ZNodes = 1;
	constexpr int32 ZLabels = 2;
	constexpr int32 ZBadgePlate = 3;
	constexpr int32 ZBadgeIcon = 4;
	constexpr int32 ZBadgeMarker = 5;
	constexpr int32 ZOverPaint = 6;
	constexpr int32 ZNodeLabels = 7;
	constexpr int32 ZPanels = 8;
}

namespace TechWheelDrawing
{
	constexpr int32 HexagonCorners = 6;
	constexpr float HexagonCornerStepDegrees = 60.f;

	FVector2f ToPoint(const FVector2D& Position)
	{
		return FVector2f(static_cast<float>(Position.X), static_cast<float>(Position.Y));
	}

	FVector2D GetPointOnCircle(const FVector2D& Centre, const float Radius, const float AngleDegrees)
	{
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		return Centre + FVector2D(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians)) * Radius;
	}

	/** Corners of a hexagon with a corner pointing right, like the technology badges. */
	TArray<FVector2f> MakeHexagon(const FVector2D& Centre, const float Radius, const bool bIsClosed)
	{
		TArray<FVector2f> Points;
		for (int32 CornerIndex = 0; CornerIndex < HexagonCorners; ++CornerIndex)
		{
			Points.Add(ToPoint(GetPointOnCircle(Centre, Radius, HexagonCornerStepDegrees * CornerIndex)));
		}
		if (bIsClosed)
		{
			// A copy first: adding a reference into the same array is not allowed.
			const FVector2f FirstPoint = Points[0];
			Points.Add(FirstPoint);
		}
		return Points;
	}

	TArray<FVector2f> MakeDiamond(const FVector2D& Centre, const float HalfDiagonal, const bool bIsClosed)
	{
		TArray<FVector2f> Points = {
			ToPoint(Centre + FVector2D(0.f, -HalfDiagonal)), ToPoint(Centre + FVector2D(HalfDiagonal, 0.f)),
			ToPoint(Centre + FVector2D(0.f, HalfDiagonal)), ToPoint(Centre + FVector2D(-HalfDiagonal, 0.f))
		};
		if (bIsClosed)
		{
			// A copy first: adding a reference into the same array is not allowed.
			const FVector2f FirstPoint = Points[0];
			Points.Add(FirstPoint);
		}
		return Points;
	}

	TArray<FVector2f> MakeArc(const FVector2D& Centre, const float Radius, const float StartDegrees,
	                          const float EndDegrees, const int32 Segments)
	{
		TArray<FVector2f> Points;
		for (int32 SegmentIndex = 0; SegmentIndex <= Segments; ++SegmentIndex)
		{
			const float Alpha = static_cast<float>(SegmentIndex) / static_cast<float>(Segments);
			Points.Add(ToPoint(GetPointOnCircle(Centre, Radius, FMath::Lerp(StartDegrees, EndDegrees, Alpha))));
		}
		return Points;
	}

	FLinearColor WithOpacity(const FLinearColor& Color, const float Opacity)
	{
		FLinearColor Result = Color;
		Result.A *= Opacity;
		return Result;
	}

	FLinearColor GetBranchColor(const EWorldCampaignTechBranch Branch)
	{
		switch (Branch)
		{
		case EWorldCampaignTechBranch::Infantry:
			return WorldCampaignUIColors::BranchInfantry;
		case EWorldCampaignTechBranch::LightArmour:
			return WorldCampaignUIColors::BranchLightArmour;
		case EWorldCampaignTechBranch::HeavyArmour:
			return WorldCampaignUIColors::BranchHeavyArmour;
		case EWorldCampaignTechBranch::Air:
			return WorldCampaignUIColors::BranchSupport;
		case EWorldCampaignTechBranch::Economy:
			return WorldCampaignUIColors::BranchEconomy;
		case EWorldCampaignTechBranch::None:
			break;
		}
		return WorldCampaignUIColors::Cream;
	}

	float GetNodeIconSize(const FWorldTechWheelNode& Node)
	{
		return Node.Kind == EWorldTechWheelNodeKind::Rim
			       ? TechWheelLayout::RimIconSize
			       : TechWheelLayout::TechnologyIconSize;
	}

	FVector2D GetKeyBadgeCentre(const FVector2D& NodePosition, const float IconSize)
	{
		using namespace TechWheelLayout;
		return NodePosition + FVector2D(IconSize * KeyBadgeOffsetXScale, IconSize * KeyBadgeOffsetYScale);
	}

	/** A filled circle with a coloured rim, drawn by Slate's rounded box. */
	FSlateBrush MakeCircleBrush(const FLinearColor& Fill, const FLinearColor& Outline, const float OutlineWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FSlateColor(Outline), OutlineWidth);
		return Brush;
	}

	/** The dark see-through plate behind a technology name. */
	FSlateBrush MakeLabelPlateBrush()
	{
		using namespace TechWheelLayout;
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(WithOpacity(IconWellColor, NodeLabelPlateOpacity));
		Brush.OutlineSettings = FSlateBrushOutlineSettings(NodeLabelPlateCornerRadius,
		                                                   FSlateColor(FLinearColor::Transparent), 0.f);
		return Brush;
	}

	/**
	 * @brief Size a text will take, to plan where its label fits before the widget exists.
	 * @param Text Text of the label.
	 * @param Font Font of the label, at its size and letter spacing.
	 * @return Measured size, or an estimate from the glyph count when Slate cannot measure.
	 */
	FVector2D GetLabelTextSize(const FText& Text, const FSlateFontInfo& Font)
	{
		using namespace TechWheelLayout;
		const float FontUnits = static_cast<float>(Font.Size) * FontPointsToUnits;
		const int32 GlyphCount = Text.ToString().Len();
		// Added to the measured width too, in case the measure leaves the letter spacing out: a plan that is a few
		// units too wide only costs free space, one that is too narrow lets labels touch.
		const float SpacingWidth = GlyphCount * FontUnits * static_cast<float>(Font.LetterSpacing) /
			LetterSpacingUnitsPerEm;
		const FVector2D EstimatedSize(GlyphCount * FontUnits * FallbackGlyphWidthScale + SpacingWidth,
		                              FontUnits * FallbackLineHeightScale);
		if (not FSlateApplication::IsInitialized())
		{
			return EstimatedSize;
		}
		const FSlateRenderer* const SlateRenderer = FSlateApplication::Get().GetRenderer();
		if (SlateRenderer == nullptr)
		{
			return EstimatedSize;
		}
		const FVector2D MeasuredSize = SlateRenderer->GetFontMeasureService()->Measure(Text, Font);
		return FVector2D(MeasuredSize.X + SpacingWidth, MeasuredSize.Y);
	}

	FTechWheelLabelObstacle MakeSquareLabelObstacle(const FVector2D& Centre, const float HalfSize)
	{
		FTechWheelLabelObstacle Obstacle;
		Obstacle.Rect = FTechWheelLabelRect::MakeCentred(Centre, FVector2D(HalfSize, HalfSize));
		return Obstacle;
	}

	FText GetKindText(const EWorldTechWheelNodeKind Kind)
	{
		switch (Kind)
		{
		case EWorldTechWheelNodeKind::Start:
			return LOCTEXT("KindStart", "HEADQUARTERS");
		case EWorldTechWheelNodeKind::Minor:
			return LOCTEXT("KindMinor", "PATH NODE");
		case EWorldTechWheelNodeKind::Technology:
			return LOCTEXT("KindTechnology", "TECHNOLOGY");
		case EWorldTechWheelNodeKind::Rim:
			return LOCTEXT("KindRim", "RIM UPGRADE");
		}
		return FText::GetEmpty();
	}
}

void UW_WorldTechWheel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

FReply UW_WorldTechWheel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

void UW_WorldTechWheel::InitTechWheel(AWorldPlayerController* WorldPlayerController,
                                      const FWorldCampaignUILook* Look)
{
	M_WorldPlayerController = WorldPlayerController;
	M_Look = Look;
	if (M_Look == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_WorldTechWheel::InitTechWheel was given no look."));
		return;
	}

	BuildWidgetTree();
	BuildScreen();
	if (M_SelectedNodeId.IsNone())
	{
		SelectNode(GetWheel().GetStartNodeId());
	}
	RefreshFromCampaign();
}

void UW_WorldTechWheel::BuildWidgetTree()
{
	using namespace TechWheelLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;
	// Visible, so it takes the mouse over the whole screen and no click falls through to the map.
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrush(FWorldCampaignUILook::MakeFlatBrush(ScreenColor));
	UOverlaySlot* const BackdropSlot = Root->AddChildToOverlay(Backdrop);
	BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
	BackdropSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* const ScreenBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ScreenBox->SetWidthOverride(ScreenSize.X);
	ScreenBox->SetHeightOverride(ScreenSize.Y);
	UOverlaySlot* const ScreenSlot = Root->AddChildToOverlay(ScreenBox);
	ScreenSlot->SetHorizontalAlignment(HAlign_Center);
	ScreenSlot->SetVerticalAlignment(VAlign_Center);
	M_ScreenCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	ScreenBox->SetContent(M_ScreenCanvas);
}

void UW_WorldTechWheel::BuildScreen()
{
	using namespace TechWheelLayout;
	if (bM_IsScreenBuilt || not GetIsValidScreenCanvas())
	{
		return;
	}
	bM_IsScreenBuilt = true;
	LoadTechnologyIcons();

	// The two paint layers cover the whole screen, so their local units are the screen canvas units.
	M_UnderPaintLayer = CreateWidget<UW_TechWheelPaintLayer>(this, UW_TechWheelPaintLayer::StaticClass());
	M_OverPaintLayer = CreateWidget<UW_TechWheelPaintLayer>(this, UW_TechWheelPaintLayer::StaticClass());
	if (GetIsValidUnderPaintLayer())
	{
		DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, M_UnderPaintLayer, FVector2D::ZeroVector, ScreenSize,
		                                  ZUnderPaint);
	}
	if (GetIsValidOverPaintLayer())
	{
		DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, M_OverPaintLayer, FVector2D::ZeroVector, ScreenSize,
		                                  ZOverPaint);
	}

	BuildScreen_TopStrip();
	BuildScreen_BranchLabels();
	BuildScreen_Nodes();
	BuildScreen_DetailPanel();
}

void UW_WorldTechWheel::BuildScreen_TopStrip()
{
	using namespace TechWheelLayout;
	const FDeckBuilderStyle& Style = M_Look->Deck.Style;
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(
			                                  WorldCampaignUIColors::PlateFill)),
	                                  FVector2D::ZeroVector, FVector2D(ScreenSize.X, StripHeight), ZPanels);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(
			                                  WorldCampaignUIColors::PlateBorder)),
	                                  FVector2D(0.f, StripHeight), FVector2D(ScreenSize.X, 1.f), ZPanels);

	UTextBlock* const Title = DeckBuilderWidgets::MakeText(
		*WidgetTree, LOCTEXT("Title", "TECHNOLOGY"),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, TitleFontSize, TitleLetterSpacing),
		FSlateColor(WorldCampaignUIColors::Cream));
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, Title, TitlePosition, FVector2D::ZeroVector, ZPanels)->
		SetAutoSize(true);
	M_CountText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, CountFontSize, CountLetterSpacing), Style.MutedTextColor);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, M_CountText, CountPosition, FVector2D::ZeroVector, ZPanels)->
		SetAutoSize(true);

	BuildScreen_TopStrip_Counters();

	TObjectPtr<UTextBlock> BackLabel;
	UWidget* const BackBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, LOCTEXT("BackToMap", "BACK TO MAP"), M_Look->Deck.Button,
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, ButtonFontSize, ButtonLetterSpacing), Style.TextColor,
		BackButtonSize, M_BackButton, BackLabel);
	M_BackButton->OnClicked.AddDynamic(this, &UW_WorldTechWheel::HandleBackClicked);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, BackBox, BackButtonPosition, BackButtonSize, ZPanels);
}

void UW_WorldTechWheel::BuildScreen_TopStrip_Counters()
{
	using namespace TechWheelLayout;
	const FDeckBuilderStyle& Style = M_Look->Deck.Style;
	struct FCounterCell
	{
		FText Label;
		const FSlateBrush* Icon;
		TObjectPtr<UTextBlock>* OutValue;
	};
	const FCounterCell Cells[] = {
		{LOCTEXT("Radixite", "RADIXITE"), &M_Look->RadixiteIcon, &M_RadixiteText},
		{LOCTEXT("Blueprints", "BLUEPRINTS"), &M_Look->BlueprintIcon, &M_BlueprintText}
	};
	UHorizontalBox* const CounterRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (const FCounterCell& Cell : Cells)
	{
		UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Column->AddChildToVerticalBox(DeckBuilderWidgets::MakeText(
			*WidgetTree, Cell.Label,
			DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, SmallLabelFontSize, SmallLabelLetterSpacing),
			Style.MutedTextColor));
		UHorizontalBox* const ValueRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UHorizontalBoxSlot* const IconSlot = ValueRow->AddChildToHorizontalBox(
			DeckBuilderWidgets::MakeImage(*WidgetTree, *Cell.Icon));
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		*Cell.OutValue = DeckBuilderWidgets::MakeText(
			*WidgetTree, FText::GetEmpty(),
			DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, CounterValueFontSize), Style.TextColor);
		ValueRow->AddChildToHorizontalBox(*Cell.OutValue)->SetVerticalAlignment(VAlign_Center);
		Column->AddChildToVerticalBox(ValueRow)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		// The same bolted metal cell as the values of the map bar (sheet 14A).
		UBorder* const CellFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		CellFrame->SetBrush(M_Look->MetalCell);
		CellFrame->SetPadding(CounterCellPadding);
		CellFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
		CellFrame->SetContent(Column);
		UHorizontalBoxSlot* const CellSlot = CounterRow->AddChildToHorizontalBox(CellFrame);
		CellSlot->SetVerticalAlignment(VAlign_Center);
		CellSlot->SetPadding(FMargin(0.f, 0.f, CounterCellGap, 0.f));
	}
	// The row grows to the left from the back button and is centred on the strip, whatever the values' widths.
	UCanvasPanelSlot* const RowSlot = DeckBuilderWidgets::PlaceOnCanvas(
		*M_ScreenCanvas, CounterRow, FVector2D(BackButtonPosition.X, StripHeight * 0.5f), FVector2D::ZeroVector,
		ZPanels);
	RowSlot->SetAutoSize(true);
	RowSlot->SetAlignment(FVector2D(1.f, 0.5f));
}

void UW_WorldTechWheel::BuildScreen_BranchLabels()
{
	using namespace TechWheelLayout;
	const float OuterRadius = RingRadii[UE_ARRAY_COUNT(RingRadii) - 1] + WedgeOuterMargin;
	for (const EWorldCampaignTechBranch Branch : WorldCampaignTechWheel::GetSectorBranches())
	{
		float CentreDegrees = 0.f;
		if (not WorldCampaignTechWheel::GetSectorCentreDegrees(Branch, CentreDegrees))
		{
			continue;
		}
		const float AngleRadians = FMath::DegreesToRadians(CentreDegrees);
		const float Distance = OuterRadius + BranchLabelGap + BranchLabelSideExtra * FMath::Abs(FMath::Cos(AngleRadians));
		const FVector2D LabelCentre = WheelCentre + FVector2D(Distance * FMath::Cos(AngleRadians),
		                                                      Distance * FMath::Sin(AngleRadians) *
		                                                      BranchLabelVerticalSquash);
		UTextBlock* const Label = DeckBuilderWidgets::MakeText(
			*WidgetTree, WorldCampaignTechWheel::GetBranchDisplayName(Branch),
			DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, BranchLabelFontSize, BranchLabelLetterSpacing),
			FSlateColor(TechWheelDrawing::GetBranchColor(Branch)));
		Label->SetJustification(ETextJustify::Center);
		DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, Label, LabelCentre - BranchLabelSize * 0.5f,
		                                  BranchLabelSize, ZLabels);
	}
}

void UW_WorldTechWheel::BuildScreen_Nodes()
{
	M_NodeWidgets.Reset();
	for (const FWorldTechWheelNode& Node : GetWheel().Nodes)
	{
		BuildScreen_Node(Node);
	}
	BuildScreen_NodeLabels();
}

void UW_WorldTechWheel::BuildScreen_NodeLabels()
{
	using namespace TechWheelLayout;
	const FSlateFontInfo LabelFont = DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, NodeLabelFontSize,
	                                                              NodeLabelLetterSpacing);
	const FVector2D PlatePaddingSize(NodeLabelPlatePadding.Left + NodeLabelPlatePadding.Right,
	                                 NodeLabelPlatePadding.Top + NodeLabelPlatePadding.Bottom);
	TArray<FName> LabelledNodeIds;
	TArray<FText> LabelTexts;
	TArray<FTechWheelLabelRequest> Requests;
	TArray<FTechWheelLabelObstacle> Obstacles;
	for (const FWorldTechWheelNode& Node : GetWheel().Nodes)
	{
		// A node's own outline is free for its own name: the sides are laid out clear of it.
		const int32 OwnLabelIndex = Node.GetIsTechnologyNode() ? Requests.Num() : INDEX_NONE;
		AddLabelObstaclesOfNode(Node, OwnLabelIndex, Obstacles);
		if (not Node.GetIsTechnologyNode())
		{
			continue;
		}
		const FText LabelText = GetNodeDisplayName(Node);
		FTechWheelLabelRequest& Request = Requests.AddDefaulted_GetRef();
		Request.NodeCentre = GetNodePosition(Node);
		Request.OutlineRadius = TechWheelDrawing::GetNodeIconSize(Node) * ReachOutlineScale;
		Request.LabelSize = TechWheelDrawing::GetLabelTextSize(LabelText, LabelFont) + PlatePaddingSize;
		LabelledNodeIds.Add(Node.NodeId);
		LabelTexts.Add(LabelText);
	}

	FTechWheelLabelRing Ring;
	Ring.Centre = WheelCentre;
	Ring.Radius = RingRadii[UE_ARRAY_COUNT(RingRadii) - 1] + WedgeOuterMargin;
	TArray<FTechWheelLabelPlacementResult> Placements;
	TechWheelLabelPlacement::PlaceLabels(Requests, Obstacles, Ring, Placements);
	for (int32 LabelIndex = 0; LabelIndex < Placements.Num(); ++LabelIndex)
	{
		BuildScreen_NodeLabel(LabelledNodeIds[LabelIndex], LabelTexts[LabelIndex], LabelFont,
		                      Placements[LabelIndex].Rect.GetCentre());
	}
}

void UW_WorldTechWheel::BuildScreen_NodeLabel(const FName NodeId, const FText& LabelText,
                                              const FSlateFontInfo& LabelFont, const FVector2D& LabelCentre)
{
	using namespace TechWheelLayout;
	FTechWheelNodeWidgets* const NodeWidgets = M_NodeWidgets.Find(NodeId);
	if (NodeWidgets == nullptr)
	{
		return;
	}
	NodeWidgets->Label = DeckBuilderWidgets::MakeText(*WidgetTree, LabelText, LabelFont, FSlateColor(NodeTextColor));
	NodeWidgets->Label->SetJustification(ETextJustify::Center);
	// Hit test invisible with its text: the plate lies over the node buttons and must not take their clicks.
	UBorder* const Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Plate->SetBrush(TechWheelDrawing::MakeLabelPlateBrush());
	Plate->SetPadding(NodeLabelPlatePadding);
	Plate->SetVisibility(ESlateVisibility::HitTestInvisible);
	Plate->SetContent(NodeWidgets->Label);
	// Sized by its text and hung on its centre, so a measuring error cannot push the name off its place.
	UCanvasPanelSlot* const PlateSlot = DeckBuilderWidgets::PlaceOnCanvas(
		*M_ScreenCanvas, Plate, LabelCentre, FVector2D::ZeroVector, ZNodeLabels);
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetAlignment(FVector2D(0.5f, 0.5f));
}

void UW_WorldTechWheel::AddLabelObstaclesOfNode(const FWorldTechWheelNode& Node, const int32 OwnLabelIndex,
                                                TArray<FTechWheelLabelObstacle>& OutObstacles) const
{
	using namespace TechWheelLayout;
	const FVector2D Position = GetNodePosition(Node);
	if (Node.Kind == EWorldTechWheelNodeKind::Start)
	{
		TechWheelLabelPlacement::AppendHexagonObstacles(Position, StartHexagonRadius, OutObstacles);
		return;
	}
	if (not Node.GetIsTechnologyNode())
	{
		OutObstacles.Add(TechWheelDrawing::MakeSquareLabelObstacle(Position, PipHalfDiagonal));
		return;
	}
	const float IconSize = TechWheelDrawing::GetNodeIconSize(Node);
	TechWheelLabelPlacement::AppendHexagonObstacles(Position, IconSize * ReachOutlineScale, OutObstacles,
	                                                OwnLabelIndex);
	if (Node.RequiredTechnology == ETechnology::Tech_NONE)
	{
		return;
	}
	const FVector2D BadgeCentre = TechWheelDrawing::GetKeyBadgeCentre(Position, IconSize);
	OutObstacles.Add(TechWheelDrawing::MakeSquareLabelObstacle(BadgeCentre, KeyBadgeSize * 0.5f));
	OutObstacles.Add(TechWheelDrawing::MakeSquareLabelObstacle(
		BadgeCentre + FVector2D(KeyMarkerOffset, KeyMarkerOffset), KeyMarkerSize * 0.5f));
}

void UW_WorldTechWheel::BuildScreen_Node(const FWorldTechWheelNode& Node)
{
	using namespace TechWheelLayout;
	const FVector2D Position = GetNodePosition(Node);
	FTechWheelNodeWidgets NodeWidgets;
	NodeWidgets.Button = WidgetTree->ConstructWidget<UTechWheelNodeButton>(UTechWheelNodeButton::StaticClass());
	NodeWidgets.Button->InitNodeButton(Node.NodeId);
	NodeWidgets.Button->OnNodeClicked.AddUObject(this, &UW_WorldTechWheel::HandleNodeClicked);

	if (not Node.GetIsTechnologyNode())
	{
		const FVector2D ButtonSize = Node.Kind == EWorldTechWheelNodeKind::Start
			                             ? FVector2D(StartHexagonRadius * 2.f, StartHexagonRadius * 2.f)
			                             : PipButtonSize;
		DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, NodeWidgets.Button, Position - ButtonSize * 0.5f,
		                                  ButtonSize, ZNodes);
		if (Node.Kind == EWorldTechWheelNodeKind::Start)
		{
			BuildScreen_StartLabel(Node);
		}
		M_NodeWidgets.Add(Node.NodeId, NodeWidgets);
		return;
	}

	const float IconSize = TechWheelDrawing::GetNodeIconSize(Node);
	const FVector2D IconDrawSize(IconSize, IconSize);
	NodeWidgets.Icon = DeckBuilderWidgets::MakeImage(*WidgetTree, MakeTechnologyBrush(Node.Technology, IconDrawSize));
	// The button is exactly icon-sized; the default slot padding would shrink the icon.
	if (UButtonSlot* const IconSlot = Cast<UButtonSlot>(NodeWidgets.Button->SetContent(NodeWidgets.Icon)))
	{
		IconSlot->SetPadding(FMargin(0.f));
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
	}
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, NodeWidgets.Button, Position - IconDrawSize * 0.5f,
	                                  IconDrawSize, ZNodes);

	// The name is added by BuildScreen_NodeLabels once every node is known.
	if (Node.RequiredTechnology != ETechnology::Tech_NONE)
	{
		BuildScreen_KeyBadge(Node, NodeWidgets);
	}
	M_NodeWidgets.Add(Node.NodeId, NodeWidgets);
}

void UW_WorldTechWheel::BuildScreen_StartLabel(const FWorldTechWheelNode& Node)
{
	using namespace TechWheelLayout;
	UTextBlock* const StartLabel = DeckBuilderWidgets::MakeText(
		*WidgetTree, LOCTEXT("StartLabel", "HQ"),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, StartLabelFontSize, StartLabelLetterSpacing),
		FSlateColor(WorldCampaignUIColors::Cream));
	StartLabel->SetJustification(ETextJustify::Center);
	// Sized by its text and hung on its centre: a fixed box left the text a few units above the hexagon's middle.
	UCanvasPanelSlot* const LabelSlot = DeckBuilderWidgets::PlaceOnCanvas(
		*M_ScreenCanvas, StartLabel, GetNodePosition(Node), FVector2D::ZeroVector, ZLabels);
	LabelSlot->SetAutoSize(true);
	LabelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
}

void UW_WorldTechWheel::BuildScreen_KeyBadge(const FWorldTechWheelNode& Node, FTechWheelNodeWidgets& NodeWidgets)
{
	using namespace TechWheelLayout;
	const FWorldTechWheelNode* const KeyNode = GetWheel().FindNodeByTechnology(Node.RequiredTechnology);
	const FLinearColor KeyColor = TechWheelDrawing::GetBranchColor(
		KeyNode != nullptr ? KeyNode->Branch : EWorldCampaignTechBranch::None);
	const FVector2D BadgeCentre = TechWheelDrawing::GetKeyBadgeCentre(GetNodePosition(Node),
	                                                                  TechWheelDrawing::GetNodeIconSize(Node));

	const FVector2D PlateSize(KeyBadgeSize, KeyBadgeSize);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, TechWheelDrawing::MakeCircleBrush(
			                                  BadgeFill, KeyColor, KeyBadgeOutlineWidth)),
	                                  BadgeCentre - PlateSize * 0.5f, PlateSize, ZBadgePlate);

	const FVector2D KeyIconSize(KeyBadgeIconSize, KeyBadgeIconSize);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, MakeTechnologyBrush(Node.RequiredTechnology, KeyIconSize)),
	                                  BadgeCentre - KeyIconSize * 0.5f, KeyIconSize, ZBadgeIcon);

	// White fill, tinted green or red on refresh; the tick or "!" is painted over it by the over layer.
	const FVector2D MarkerSize(KeyMarkerSize, KeyMarkerSize);
	NodeWidgets.KeyMarker = DeckBuilderWidgets::MakeImage(
		*WidgetTree, TechWheelDrawing::MakeCircleBrush(FLinearColor::White, FLinearColor::Transparent, 0.f));
	const FVector2D MarkerCentre = BadgeCentre + FVector2D(KeyMarkerOffset, KeyMarkerOffset);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, NodeWidgets.KeyMarker, MarkerCentre - MarkerSize * 0.5f,
	                                  MarkerSize, ZBadgeMarker);
}

void UW_WorldTechWheel::BuildScreen_DetailPanel()
{
	using namespace TechWheelLayout;
	const FDeckBuilderStyle& Style = M_Look->Deck.Style;
	UBorder* const Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrush(M_Look->Deck.Panel);
	Panel->SetPadding(DetailPanelPadding);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, Panel, DetailPanelPosition, DetailPanelSize, ZPanels);
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	M_Detail.KindText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, DetailKindFontSize, SmallLabelLetterSpacing),
		Style.MutedTextColor);
	Column->AddChildToVerticalBox(M_Detail.KindText);
	M_Detail.NameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, DetailNameFontSize, DetailNameLetterSpacing),
		FSlateColor(NodeTextColor));
	Column->AddChildToVerticalBox(M_Detail.NameText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	UBorder* const IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	IconWell->SetBrush(FWorldCampaignUILook::MakeFlatBrush(IconWellColor));
	IconWell->SetHorizontalAlignment(HAlign_Center);
	IconWell->SetVerticalAlignment(VAlign_Center);
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetHeightOverride(DetailIconWellHeight);
	WellBox->SetContent(IconWell);
	M_Detail.Icon = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	IconWell->SetContent(M_Detail.Icon);
	Column->AddChildToVerticalBox(WellBox)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));

	M_Detail.DescriptionText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, DetailBodyFontSize),
		Style.DescriptionColor);
	M_Detail.DescriptionText->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(M_Detail.DescriptionText)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));

	BuildScreen_DetailPanel_Rows(*Column);
	BuildScreen_DetailPanel_Price(*Column);
	BuildScreen_DetailPanel_Button(*Column);
}

UTextBlock* UW_WorldTechWheel::AddDetailLabel(UVerticalBox& Column, const FText& Label, const float TopPadding)
{
	using namespace TechWheelLayout;
	UTextBlock* const LabelText = DeckBuilderWidgets::MakeText(
		*WidgetTree, Label,
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, SmallLabelFontSize, SmallLabelLetterSpacing),
		M_Look->Deck.Style.MutedTextColor);
	Column.AddChildToVerticalBox(LabelText)->SetPadding(FMargin(0.f, TopPadding, 0.f, 4.f));
	return LabelText;
}

void UW_WorldTechWheel::BuildScreen_DetailPanel_Rows(UVerticalBox& Column)
{
	using namespace TechWheelLayout;
	const FSlateFontInfo ValueFont = DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, DetailValueFontSize);
	constexpr float RowTopPadding = 18.f;

	M_Detail.ReachedGroup = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddDetailLabel(*M_Detail.ReachedGroup, LOCTEXT("ReachedThrough", "REACHED THROUGH"), RowTopPadding);
	M_Detail.ReachedText = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(), ValueFont,
	                                                    FSlateColor(WorldCampaignUIColors::Reach));
	M_Detail.ReachedGroup->AddChildToVerticalBox(M_Detail.ReachedText);
	Column.AddChildToVerticalBox(M_Detail.ReachedGroup);

	M_Detail.KeyGroup = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddDetailLabel(*M_Detail.KeyGroup, LOCTEXT("AlsoNeeds", "ALSO NEEDS"), RowTopPadding);
	UHorizontalBox* const KeyRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_Detail.KeyNameText = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(), ValueFont,
	                                                    FSlateColor(WorldCampaignUIColors::Cream));
	KeyRow->AddChildToHorizontalBox(M_Detail.KeyNameText)->SetVerticalAlignment(VAlign_Bottom);
	M_Detail.KeyStatusText = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(), ValueFont,
	                                                      FSlateColor(WorldCampaignUIColors::Bad));
	UHorizontalBoxSlot* const StatusSlot = KeyRow->AddChildToHorizontalBox(M_Detail.KeyStatusText);
	StatusSlot->SetVerticalAlignment(VAlign_Bottom);
	StatusSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	M_Detail.KeyBranchText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, SmallLabelFontSize, SmallLabelLetterSpacing),
		M_Look->Deck.Style.MutedTextColor);
	UHorizontalBoxSlot* const BranchSlot = KeyRow->AddChildToHorizontalBox(M_Detail.KeyBranchText);
	BranchSlot->SetVerticalAlignment(VAlign_Bottom);
	BranchSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 1.f));
	M_Detail.KeyGroup->AddChildToVerticalBox(KeyRow);
	Column.AddChildToVerticalBox(M_Detail.KeyGroup);
}

void UW_WorldTechWheel::BuildScreen_DetailPanel_Price(UVerticalBox& Column)
{
	using namespace TechWheelLayout;
	constexpr float PriceTopPadding = 22.f;
	M_Detail.PriceGroup = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AddDetailLabel(*M_Detail.PriceGroup, LOCTEXT("Price", "PRICE"), PriceTopPadding);
	UHorizontalBox* const PriceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(PriceIconSize.X);
	IconBox->SetHeightOverride(PriceIconSize.Y);
	M_Detail.PriceIcon = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	IconBox->SetContent(M_Detail.PriceIcon);
	PriceRow->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
	M_Detail.PriceText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, DetailPriceFontSize),
		FSlateColor(NodeTextColor));
	UHorizontalBoxSlot* const PriceSlot = PriceRow->AddChildToHorizontalBox(M_Detail.PriceText);
	PriceSlot->SetVerticalAlignment(VAlign_Center);
	PriceSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
	M_Detail.HoldText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, DetailBodyFontSize - 1),
		M_Look->Deck.Style.MutedTextColor);
	UHorizontalBoxSlot* const HoldSlot = PriceRow->AddChildToHorizontalBox(M_Detail.HoldText);
	HoldSlot->SetVerticalAlignment(VAlign_Center);
	HoldSlot->SetPadding(FMargin(18.f, 4.f, 0.f, 0.f));
	M_Detail.PriceGroup->AddChildToVerticalBox(PriceRow);
	Column.AddChildToVerticalBox(M_Detail.PriceGroup);
}

void UW_WorldTechWheel::BuildScreen_DetailPanel_Button(UVerticalBox& Column)
{
	using namespace TechWheelLayout;
	// Pushes the reason and the button to the bottom of the panel.
	USpacer* const Filler = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Column.AddChildToVerticalBox(Filler)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	M_Detail.ReasonText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, DetailBodyFontSize),
		FSlateColor(WorldCampaignUIColors::Bad));
	M_Detail.ReasonText->SetJustification(ETextJustify::Center);
	M_Detail.ReasonText->SetAutoWrapText(true);
	Column.AddChildToVerticalBox(M_Detail.ReasonText)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	UWidget* const ButtonBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, LOCTEXT("Research", "RESEARCH"), M_Look->Deck.Button,
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, ButtonFontSize, ButtonLetterSpacing),
		M_Look->Deck.Style.TextColor, ResearchButtonSize, M_Detail.ResearchButton, M_Detail.ResearchLabel);
	M_Detail.ResearchButton->OnClicked.AddDynamic(this, &UW_WorldTechWheel::HandleResearchClicked);
	Column.AddChildToVerticalBox(ButtonBox)->SetHorizontalAlignment(HAlign_Center);
}

void UW_WorldTechWheel::LoadTechnologyIcons()
{
	TArray<FString> MissingIcons;
	const FWorldTechWheelPresentationTable& Presentation = UWorldCampaignTechSettings::GetWheelPresentation();
	for (const FWorldTechWheelNode& Node : GetWheel().Nodes)
	{
		if (not Node.GetIsTechnologyNode() || M_LoadedIcons.Contains(Node.Technology))
		{
			continue;
		}
		const FWorldTechWheelTechPresentation* const TechPresentation = Presentation.FindPresentation(Node.Technology);
		UTexture2D* const Icon = TechPresentation != nullptr ? TechPresentation->Icon.LoadSynchronous() : nullptr;
		if (not IsValid(Icon))
		{
			MissingIcons.Add(Global_GetTechAsString(Node.Technology));
			continue;
		}
		M_LoadedIcons.Add(Node.Technology, Icon);
	}
	if (MissingIcons.Num() > 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UW_WorldTechWheel: %d technology icons did not load and draw as flat plates: %s. Check "
				"WheelPresentation in the World Campaign Tech Tree project settings."),
			MissingIcons.Num(), *FString::Join(MissingIcons, TEXT(", "))));
	}
}

FSlateBrush UW_WorldTechWheel::MakeTechnologyBrush(const ETechnology Technology, const FVector2D& DrawSize) const
{
	const TObjectPtr<UTexture2D>* const Icon = M_LoadedIcons.Find(Technology);
	return FWorldCampaignUILook::MakeIconBrush(Icon != nullptr ? Icon->Get() : nullptr, DrawSize);
}

FText UW_WorldTechWheel::GetNodeDisplayName(const FWorldTechWheelNode& Node) const
{
	switch (Node.Kind)
	{
	case EWorldTechWheelNodeKind::Start:
		return LOCTEXT("StartName", "HQ");
	case EWorldTechWheelNodeKind::Minor:
		return LOCTEXT("MinorName", "Path node");
	case EWorldTechWheelNodeKind::Technology:
	case EWorldTechWheelNodeKind::Rim:
		return WorldCampaignTechWheel::GetTechnologyDisplayName(Node.Technology);
	}
	return FText::GetEmpty();
}

const FWorldTechWheel& UW_WorldTechWheel::GetWheel() const
{
	return UWorldCampaignTechSettings::GetWheel();
}

FVector2D UW_WorldTechWheel::GetNodePosition(const FWorldTechWheelNode& Node) const
{
	using namespace TechWheelLayout;
	const int32 RingIndex = FMath::Clamp(Node.Ring, 0, static_cast<int32>(UE_ARRAY_COUNT(RingRadii)) - 1);
	return TechWheelDrawing::GetPointOnCircle(WheelCentre, RingRadii[RingIndex], Node.AngleDegrees);
}

bool UW_WorldTechWheel::ReadProgressSnapshot(FTechWheelProgressSnapshot& OutSnapshot) const
{
	OutSnapshot = FTechWheelProgressSnapshot();
	if (not GetIsValidWorldPlayerController())
	{
		return false;
	}
	const UWorldCampaignProgressManager* const ProgressManager =
		M_WorldPlayerController->GetWorldCampaignProgressManager();
	if (not IsValid(ProgressManager))
	{
		return false;
	}
	const FWorldCampaignProgressState& ProgressState = ProgressManager->GetProgressState();
	OutSnapshot.BoughtNodeIds = ProgressState.BoughtTechNodeIds;
	OutSnapshot.ResearchedTechnologies = ProgressState.ResearchedTechnologies;
	OutSnapshot.Resources = ProgressState.Resources;
	return true;
}

void UW_WorldTechWheel::RefreshFromCampaign()
{
	if (not bM_IsScreenBuilt || M_Look == nullptr)
	{
		return;
	}
	FTechWheelProgressSnapshot Snapshot;
	ReadProgressSnapshot(Snapshot);
	RefreshStrip(Snapshot);
	RefreshNodes(Snapshot);
	RefreshDetailPanel(Snapshot);
	RefreshPaintLayers(Snapshot);
}

void UW_WorldTechWheel::RefreshStrip(const FTechWheelProgressSnapshot& Snapshot)
{
	const FWorldTechWheel& Wheel = GetWheel();
	int32 BoughtTechnologies = 0;
	int32 BoughtNodes = 0;
	for (const FWorldTechWheelNode& Node : Wheel.Nodes)
	{
		if (not WorldCampaignTechWheel::GetIsNodeBought(Wheel, Node.NodeId, Snapshot.BoughtNodeIds))
		{
			continue;
		}
		BoughtNodes++;
		BoughtTechnologies += Node.GetIsTechnologyNode() ? 1 : 0;
	}
	M_CountText->SetText(FText::Format(
		LOCTEXT("Count", "{0} of {1} technologies   \u00B7   {2} of {3} nodes"), BoughtTechnologies,
		Wheel.GetTechnologyNodeCount(), BoughtNodes, Wheel.Nodes.Num()));
	M_RadixiteText->SetText(FText::AsNumber(Snapshot.Resources.Radixite));
	M_BlueprintText->SetText(FText::AsNumber(Snapshot.Resources.Blueprints));
}

void UW_WorldTechWheel::RefreshNodes(const FTechWheelProgressSnapshot& Snapshot)
{
	using namespace TechWheelLayout;
	const FWorldTechWheel& Wheel = GetWheel();
	for (const FWorldTechWheelNode& Node : Wheel.Nodes)
	{
		const FTechWheelNodeWidgets* const NodeWidgets = M_NodeWidgets.Find(Node.NodeId);
		if (NodeWidgets == nullptr)
		{
			continue;
		}
		const EWorldTechWheelNodeState State = WorldCampaignTechWheel::GetNodeState(
			Wheel, Node.NodeId, Snapshot.BoughtNodeIds);
		if (IsValid(NodeWidgets->Icon))
		{
			NodeWidgets->Icon->SetColorAndOpacity(State == EWorldTechWheelNodeState::Bought
				                                      ? BoughtIconTint
				                                      : State == EWorldTechWheelNodeState::Reachable
				                                      ? ReachableIconTint
				                                      : LockedIconTint);
		}
		if (IsValid(NodeWidgets->Label))
		{
			NodeWidgets->Label->SetColorAndOpacity(FSlateColor(
				State == EWorldTechWheelNodeState::Locked ? LockedTextColor : NodeTextColor));
		}
		if (IsValid(NodeWidgets->KeyMarker))
		{
			const bool bIsKeyResearched = Snapshot.ResearchedTechnologies.Contains(Node.RequiredTechnology);
			NodeWidgets->KeyMarker->SetColorAndOpacity(bIsKeyResearched
				                                           ? WorldCampaignUIColors::Good
				                                           : WorldCampaignUIColors::Bad);
		}
	}
}

void UW_WorldTechWheel::RefreshDetailPanel(const FTechWheelProgressSnapshot& Snapshot)
{
	const FWorldTechWheelNode* const Node = GetWheel().FindNode(M_SelectedNodeId);
	if (Node == nullptr)
	{
		return;
	}
	RefreshDetailPanel_Header(*Node);
	RefreshDetailPanel_Reached(*Node, Snapshot);
	RefreshDetailPanel_Key(*Node, Snapshot);
	RefreshDetailPanel_Price(*Node, Snapshot);
	RefreshDetailPanel_Button(*Node, Snapshot);
}

void UW_WorldTechWheel::RefreshDetailPanel_Header(const FWorldTechWheelNode& Node)
{
	using namespace TechWheelLayout;
	const FText KindText = TechWheelDrawing::GetKindText(Node.Kind);
	M_Detail.KindText->SetText(Node.Kind == EWorldTechWheelNodeKind::Start
		                           ? KindText
		                           : FText::Format(LOCTEXT("KindLine", "{0}   \u00B7   {1}"),
		                                           WorldCampaignTechWheel::GetBranchDisplayName(Node.Branch),
		                                           KindText));
	M_Detail.NameText->SetText(GetNodeDisplayName(Node).ToUpper());

	if (Node.GetIsTechnologyNode())
	{
		M_Detail.Icon->SetBrush(MakeTechnologyBrush(Node.Technology, DetailIconSize));
		M_Detail.Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		M_Detail.Icon->SetVisibility(ESlateVisibility::Hidden);
	}

	const FWorldTechWheelTechPresentation* const Presentation = Node.GetIsTechnologyNode()
		                                                            ? UWorldCampaignTechSettings::GetWheelPresentation().
		                                                            FindPresentation(Node.Technology)
		                                                            : nullptr;
	M_Detail.DescriptionText->SetText(Presentation != nullptr ? Presentation->Description : FText::GetEmpty());
}

void UW_WorldTechWheel::RefreshDetailPanel_Reached(const FWorldTechWheelNode& Node,
                                                   const FTechWheelProgressSnapshot& Snapshot)
{
	const FWorldTechWheel& Wheel = GetWheel();
	const FName NeighbourId = WorldCampaignTechWheel::GetBoughtNeighbour(Wheel, Node.NodeId, Snapshot.BoughtNodeIds);
	const FWorldTechWheelNode* const Neighbour = Wheel.FindNode(NeighbourId);
	// Nothing reaches the node yet: the red line above the button already says so, the block would repeat it.
	if (Node.Kind == EWorldTechWheelNodeKind::Start || Neighbour == nullptr)
	{
		M_Detail.ReachedGroup->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_Detail.ReachedGroup->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	M_Detail.ReachedText->SetText(GetNodeDisplayName(*Neighbour));
}

void UW_WorldTechWheel::RefreshDetailPanel_Key(const FWorldTechWheelNode& Node,
                                               const FTechWheelProgressSnapshot& Snapshot)
{
	if (Node.RequiredTechnology == ETechnology::Tech_NONE)
	{
		M_Detail.KeyGroup->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_Detail.KeyGroup->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const FWorldTechWheelNode* const KeyNode = GetWheel().FindNodeByTechnology(Node.RequiredTechnology);
	const EWorldCampaignTechBranch KeyBranch = KeyNode != nullptr ? KeyNode->Branch : EWorldCampaignTechBranch::None;
	const bool bIsKeyResearched = Snapshot.ResearchedTechnologies.Contains(Node.RequiredTechnology);
	M_Detail.KeyNameText->SetText(WorldCampaignTechWheel::GetTechnologyDisplayName(Node.RequiredTechnology));
	M_Detail.KeyNameText->SetColorAndOpacity(FSlateColor(TechWheelDrawing::GetBranchColor(KeyBranch)));
	M_Detail.KeyStatusText->SetText(bIsKeyResearched
		                                ? LOCTEXT("KeyResearched", "researched")
		                                : LOCTEXT("KeyNotResearched", "not researched"));
	M_Detail.KeyStatusText->SetColorAndOpacity(FSlateColor(bIsKeyResearched
		                                                       ? WorldCampaignUIColors::Good
		                                                       : WorldCampaignUIColors::Bad));
	M_Detail.KeyBranchText->SetText(WorldCampaignTechWheel::GetBranchDisplayName(KeyBranch));
}

void UW_WorldTechWheel::RefreshDetailPanel_Price(const FWorldTechWheelNode& Node,
                                                 const FTechWheelProgressSnapshot& Snapshot)
{
	using namespace TechWheelLayout;
	const FWorldMapResources& Cost = Node.Cost;
	if (Node.Kind == EWorldTechWheelNodeKind::Start || (Cost.Blueprints <= 0 && Cost.Radixite <= 0))
	{
		M_Detail.PriceGroup->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_Detail.PriceGroup->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const bool bCostsBlueprints = Cost.Blueprints > 0;
	const FSlateBrush& IconSource = bCostsBlueprints ? M_Look->BlueprintIcon : M_Look->RadixiteIcon;
	M_Detail.PriceIcon->SetBrush(DeckBuilderWidgets::MakePictureBrush(IconSource, PriceIconSize));
	const int32 Price = bCostsBlueprints ? Cost.Blueprints : Cost.Radixite;
	const int32 Held = bCostsBlueprints ? Snapshot.Resources.Blueprints : Snapshot.Resources.Radixite;
	// A node that costs both shows the radixite after the blueprints.
	M_Detail.PriceText->SetText(bCostsBlueprints && Cost.Radixite > 0
		                            ? FText::Format(LOCTEXT("PriceBoth", "{0}  +  {1} radixite"), Price,
		                                            Cost.Radixite)
		                            : FText::AsNumber(Price));
	M_Detail.HoldText->SetText(FText::Format(LOCTEXT("YouHold", "you hold {0}"), Held));
}

void UW_WorldTechWheel::RefreshDetailPanel_Button(const FWorldTechWheelNode& Node,
                                                  const FTechWheelProgressSnapshot& Snapshot)
{
	const FWorldTechWheel& Wheel = GetWheel();
	const EWorldTechWheelBuyBlock Block = WorldCampaignTechWheel::GetNodeBuyBlock(
		Wheel, Node.NodeId, Snapshot.BoughtNodeIds, Snapshot.ResearchedTechnologies, Snapshot.Resources);
	const bool bIsBought = Block == EWorldTechWheelBuyBlock::AlreadyBought;
	const bool bCanBuy = Block == EWorldTechWheelBuyBlock::None;
	M_Detail.ResearchButton->SetIsEnabled(bCanBuy);
	M_Detail.ResearchLabel->SetText(bIsBought ? LOCTEXT("Researched", "RESEARCHED") : LOCTEXT("Research", "RESEARCH"));
	M_Detail.ResearchLabel->SetColorAndOpacity(bCanBuy
		                                           ? M_Look->Deck.Style.TextColor
		                                           : M_Look->Deck.Style.MutedTextColor);
	// An owned node needs no reason; the button already says it.
	M_Detail.ReasonText->SetText(bCanBuy || bIsBought
		                             ? FText::GetEmpty()
		                             : WorldCampaignTechWheel::GetNodeBuyBlockText(Block, &Node));
}

void UW_WorldTechWheel::RefreshPaintLayers(const FTechWheelProgressSnapshot& Snapshot)
{
	if (GetIsValidUnderPaintLayer())
	{
		FTechWheelPaintList UnderList;
		AddPaint_Sectors(UnderList.Back);
		AddPaint_Paths(UnderList.Back, Snapshot);
		AddPaint_NodeOutlines(UnderList.Back, Snapshot);
		AddPaint_SelectionAndChord(UnderList.Back);
		AddPaint_StartAndPips(UnderList.Front, Snapshot);
		M_UnderPaintLayer->SetPaintList(MoveTemp(UnderList));
	}
	if (GetIsValidOverPaintLayer())
	{
		FTechWheelPaintList OverList;
		AddPaint_KeyMarks(OverList.Front, Snapshot);
		M_OverPaintLayer->SetPaintList(MoveTemp(OverList));
	}
}

void UW_WorldTechWheel::AddPaint_Sectors(FTechWheelPaintPass& Pass) const
{
	using namespace TechWheelLayout;
	const float OuterRadius = RingRadii[UE_ARRAY_COUNT(RingRadii) - 1] + WedgeOuterMargin;
	const float HalfWidth = WorldCampaignTechWheel::GetSectorHalfWidthDegrees();
	for (const EWorldCampaignTechBranch Branch : WorldCampaignTechWheel::GetSectorBranches())
	{
		float CentreDegrees = 0.f;
		if (not WorldCampaignTechWheel::GetSectorCentreDegrees(Branch, CentreDegrees))
		{
			continue;
		}
		const FLinearColor BranchColor = TechWheelDrawing::GetBranchColor(Branch);
		TArray<FVector2f> Arc = TechWheelDrawing::MakeArc(WheelCentre, OuterRadius, CentreDegrees - HalfWidth,
		                                                  CentreDegrees + HalfWidth, ArcSegmentsPerSector);
		TArray<FVector2f> Wedge = Arc;
		Wedge.Insert(TechWheelDrawing::ToPoint(WheelCentre), 0);
		Pass.AddConvexFill(Wedge, TechWheelDrawing::WithOpacity(BranchColor, WedgeOpacity));
		Pass.AddStroke(MoveTemp(Arc), TechWheelDrawing::WithOpacity(BranchColor, RimArcOpacity), RimArcThickness);
	}
}

void UW_WorldTechWheel::AddPaint_Paths(FTechWheelPaintPass& Pass, const FTechWheelProgressSnapshot& Snapshot) const
{
	const FWorldTechWheel& Wheel = GetWheel();
	for (int32 NodeIndex = 0; NodeIndex < Wheel.Nodes.Num(); ++NodeIndex)
	{
		AddPaint_PathsOfNode(Pass, NodeIndex, Snapshot);
	}
}

void UW_WorldTechWheel::AddPaint_PathsOfNode(FTechWheelPaintPass& Pass, const int32 NodeIndex,
                                             const FTechWheelProgressSnapshot& Snapshot) const
{
	using namespace TechWheelLayout;
	const FWorldTechWheel& Wheel = GetWheel();
	const FWorldTechWheelNode& Node = Wheel.Nodes[NodeIndex];
	const bool bIsNodeBought = WorldCampaignTechWheel::GetIsNodeBought(Wheel, Node.NodeId, Snapshot.BoughtNodeIds);
	for (const FName NeighbourId : Node.Neighbours)
	{
		const int32 NeighbourIndex = Wheel.Nodes.IndexOfByPredicate([NeighbourId](const FWorldTechWheelNode& Other)
		{
			return Other.NodeId == NeighbourId;
		});
		// Each link is listed on both nodes; draw it once, from the node that comes first.
		if (NeighbourIndex == INDEX_NONE || NeighbourIndex < NodeIndex)
		{
			continue;
		}
		const bool bIsNeighbourBought = WorldCampaignTechWheel::GetIsNodeBought(Wheel, NeighbourId,
		                                                                       Snapshot.BoughtNodeIds);
		TArray<FVector2f> Line = {
			TechWheelDrawing::ToPoint(GetNodePosition(Node)),
			TechWheelDrawing::ToPoint(GetNodePosition(Wheel.Nodes[NeighbourIndex]))
		};
		if (bIsNodeBought && bIsNeighbourBought)
		{
			Pass.AddStroke(MoveTemp(Line), WorldCampaignUIColors::Cream, BoughtPathThickness);
			continue;
		}
		if (bIsNodeBought != bIsNeighbourBought)
		{
			Pass.AddStroke(MoveTemp(Line), WorldCampaignUIColors::Reach, OtherPathThickness, PathDashLength);
			continue;
		}
		Pass.AddStroke(MoveTemp(Line), WorldCampaignUIColors::Locked, OtherPathThickness);
	}
}

void UW_WorldTechWheel::AddPaint_NodeOutlines(FTechWheelPaintPass& Pass,
                                              const FTechWheelProgressSnapshot& Snapshot) const
{
	const FWorldTechWheel& Wheel = GetWheel();
	for (const FWorldTechWheelNode& Node : Wheel.Nodes)
	{
		if (not Node.GetIsTechnologyNode())
		{
			continue;
		}
		AddPaint_TechnologyOutlines(Pass, Node,
		                            WorldCampaignTechWheel::GetNodeState(Wheel, Node.NodeId, Snapshot.BoughtNodeIds));
	}
}

void UW_WorldTechWheel::AddPaint_TechnologyOutlines(FTechWheelPaintPass& Pass, const FWorldTechWheelNode& Node,
                                                    const EWorldTechWheelNodeState State) const
{
	using namespace TechWheelLayout;
	const FVector2D Position = GetNodePosition(Node);
	const float IconSize = TechWheelDrawing::GetNodeIconSize(Node);
	const float BranchOpacity = State == EWorldTechWheelNodeState::Locked ? LockedOutlineOpacity : 1.f;
	Pass.AddStroke(TechWheelDrawing::MakeHexagon(Position, IconSize * BranchOutlineScale, true),
	               TechWheelDrawing::WithOpacity(TechWheelDrawing::GetBranchColor(Node.Branch), BranchOpacity),
	               OutlineThickness);

	const FWorldTechWheelNode* const KeyNode = GetWheel().FindNodeByTechnology(Node.RequiredTechnology);
	if (KeyNode != nullptr)
	{
		Pass.AddStroke(TechWheelDrawing::MakeHexagon(Position, IconSize * KeyOutlineScale, true),
		               TechWheelDrawing::GetBranchColor(KeyNode->Branch), OutlineThickness);
	}
	if (State == EWorldTechWheelNodeState::Reachable)
	{
		Pass.AddStroke(TechWheelDrawing::MakeHexagon(Position, IconSize * ReachOutlineScale, true),
		               WorldCampaignUIColors::Reach, ReachOutlineThickness, ReachOutlineDashLength);
	}
}

void UW_WorldTechWheel::AddPaint_SelectionAndChord(FTechWheelPaintPass& Pass) const
{
	using namespace TechWheelLayout;
	const FWorldTechWheelNode* const Node = GetWheel().FindNode(M_SelectedNodeId);
	if (Node == nullptr)
	{
		return;
	}
	const FVector2D Position = GetNodePosition(*Node);
	if (Node->GetIsTechnologyNode())
	{
		Pass.AddStroke(TechWheelDrawing::MakeHexagon(
			               Position, TechWheelDrawing::GetNodeIconSize(*Node) * ReachOutlineScale, true),
		               SelectionColor, OutlineThickness);
	}
	else if (Node->Kind == EWorldTechWheelNodeKind::Start)
	{
		Pass.AddStroke(TechWheelDrawing::MakeHexagon(Position, StartHexagonRadius + PipHalfDiagonal, true),
		               SelectionColor, OutlineThickness);
	}
	else
	{
		Pass.AddStroke(TechWheelDrawing::MakeDiamond(Position, PipHalfDiagonal * 2.f, true), SelectionColor,
		               PipOutlineThickness);
	}

	const FWorldTechWheelNode* const KeyNode = GetWheel().FindNodeByTechnology(Node->RequiredTechnology);
	if (KeyNode == nullptr)
	{
		return;
	}
	const FLinearColor KeyColor = TechWheelDrawing::GetBranchColor(KeyNode->Branch);
	const FVector2D KeyPosition = GetNodePosition(*KeyNode);
	Pass.AddStroke({TechWheelDrawing::ToPoint(Position), TechWheelDrawing::ToPoint(KeyPosition)}, KeyColor,
	               ChordThickness, ChordDashLength);
	Pass.AddStroke(TechWheelDrawing::MakeHexagon(KeyPosition, TechnologyIconSize * ChordTargetOutlineScale, true),
	               KeyColor, ChordThickness);
}

void UW_WorldTechWheel::AddPaint_StartAndPips(FTechWheelPaintPass& Pass,
                                              const FTechWheelProgressSnapshot& Snapshot) const
{
	using namespace TechWheelLayout;
	const FWorldTechWheel& Wheel = GetWheel();
	for (const FWorldTechWheelNode& Node : Wheel.Nodes)
	{
		if (Node.GetIsTechnologyNode())
		{
			continue;
		}
		const FVector2D Position = GetNodePosition(Node);
		if (Node.Kind == EWorldTechWheelNodeKind::Start)
		{
			Pass.AddConvexFill(TechWheelDrawing::MakeHexagon(Position, StartHexagonRadius, false), DarkNodeFill);
			Pass.AddStroke(TechWheelDrawing::MakeHexagon(Position, StartHexagonRadius, true),
			               WorldCampaignUIColors::Cream, OutlineThickness);
			continue;
		}
		const EWorldTechWheelNodeState State = WorldCampaignTechWheel::GetNodeState(
			Wheel, Node.NodeId, Snapshot.BoughtNodeIds);
		const bool bIsBought = State == EWorldTechWheelNodeState::Bought;
		const FLinearColor StrokeColor = bIsBought
			                                 ? WorldCampaignUIColors::Cream
			                                 : State == EWorldTechWheelNodeState::Reachable
			                                 ? WorldCampaignUIColors::Reach
			                                 : WorldCampaignUIColors::Locked;
		Pass.AddConvexFill(TechWheelDrawing::MakeDiamond(Position, PipHalfDiagonal, false),
		                   bIsBought ? WorldCampaignUIColors::Cream : DarkNodeFill);
		Pass.AddStroke(TechWheelDrawing::MakeDiamond(Position, PipHalfDiagonal, true), StrokeColor,
		               PipOutlineThickness);
	}
}

void UW_WorldTechWheel::AddPaint_KeyMarks(FTechWheelPaintPass& Pass, const FTechWheelProgressSnapshot& Snapshot) const
{
	using namespace TechWheelLayout;
	for (const FWorldTechWheelNode& Node : GetWheel().Nodes)
	{
		if (not Node.GetIsTechnologyNode() || Node.RequiredTechnology == ETechnology::Tech_NONE)
		{
			continue;
		}
		const FVector2D BadgeCentre = TechWheelDrawing::GetKeyBadgeCentre(GetNodePosition(Node),
		                                                                  TechWheelDrawing::GetNodeIconSize(Node));
		const FVector2D Mark = BadgeCentre + FVector2D(KeyMarkerOffset, KeyMarkerOffset);
		if (Snapshot.ResearchedTechnologies.Contains(Node.RequiredTechnology))
		{
			// A tick.
			Pass.AddStroke({
				               TechWheelDrawing::ToPoint(Mark + FVector2D(-4.f, 0.f)),
				               TechWheelDrawing::ToPoint(Mark + FVector2D(-1.f, 3.f)),
				               TechWheelDrawing::ToPoint(Mark + FVector2D(4.f, -3.f))
			               }, BadgeFill, KeyMarkThickness);
			continue;
		}
		// An exclamation mark: a bar and a dot.
		Pass.AddStroke({
			               TechWheelDrawing::ToPoint(Mark + FVector2D(0.f, -5.f)),
			               TechWheelDrawing::ToPoint(Mark + FVector2D(0.f, 1.5f))
		               }, BadgeFill, KeyMarkThickness);
		Pass.AddStroke({
			               TechWheelDrawing::ToPoint(Mark + FVector2D(0.f, 3.f)),
			               TechWheelDrawing::ToPoint(Mark + FVector2D(0.f, 5.f))
		               }, BadgeFill, KeyMarkThickness);
	}
}

void UW_WorldTechWheel::SelectNode(const FName NodeId)
{
	if (GetWheel().FindNode(NodeId) == nullptr)
	{
		return;
	}
	M_SelectedNodeId = NodeId;
}

void UW_WorldTechWheel::HandleNodeClicked(const FName NodeId)
{
	SelectNode(NodeId);
	RefreshFromCampaign();
}

bool UW_WorldTechWheel::TryBuySelectedNode(FText& OutFailReason)
{
	UWorldCampaignTechManager* const TechManager = GetTechManager();
	if (not IsValid(TechManager))
	{
		OutFailReason = LOCTEXT("NoTechManager", "No campaign is running.");
		return false;
	}
	return TechManager->TryBuyWheelNode(M_SelectedNodeId, OutFailReason);
}

void UW_WorldTechWheel::HandleResearchClicked()
{
	FText FailReason;
	const bool bWasBought = TryBuySelectedNode(FailReason);
	RefreshFromCampaign();
	if (not bWasBought && IsValid(M_Detail.ReasonText))
	{
		M_Detail.ReasonText->SetText(FailReason);
	}
}

void UW_WorldTechWheel::HandleBackClicked()
{
	M_OnCloseRequested.Broadcast();
}

void UW_WorldTechWheel::SelectNodeForCapture(const FName NodeId)
{
	HandleNodeClicked(NodeId);
}

bool UW_WorldTechWheel::BuyNodeForCapture(const FName NodeId)
{
	SelectNode(NodeId);
	if (M_SelectedNodeId != NodeId)
	{
		return false;
	}
	FText FailReason;
	const bool bWasBought = TryBuySelectedNode(FailReason);
	RefreshFromCampaign();
	return bWasBought;
}

UWorldCampaignTechManager* UW_WorldTechWheel::GetTechManager() const
{
	if (not GetIsValidWorldPlayerController())
	{
		return nullptr;
	}
	return M_WorldPlayerController->GetWorldCampaignTechManager();
}

bool UW_WorldTechWheel::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_WorldPlayerController"), TEXT("GetIsValidWorldPlayerController"), this);
	return false;
}

bool UW_WorldTechWheel::GetIsValidUnderPaintLayer() const
{
	if (IsValid(M_UnderPaintLayer))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_UnderPaintLayer"), TEXT("GetIsValidUnderPaintLayer"), this);
	return false;
}

bool UW_WorldTechWheel::GetIsValidOverPaintLayer() const
{
	if (IsValid(M_OverPaintLayer))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_OverPaintLayer"), TEXT("GetIsValidOverPaintLayer"), this);
	return false;
}

bool UW_WorldTechWheel::GetIsValidScreenCanvas() const
{
	if (IsValid(M_ScreenCanvas))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_ScreenCanvas"), TEXT("GetIsValidScreenCanvas"), this);
	return false;
}

#undef LOCTEXT_NAMESPACE
