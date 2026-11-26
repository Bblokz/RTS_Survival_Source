// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/W_WorldMapTags.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionOwners.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintSettings.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldUnknownMissionMarkerComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/WorldBuildSiteHelpers.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/W_WorldMapArrowLayer.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/WorldMapScreenProjection.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/WorldMapTagsMath.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"
#include "RTS_Survival/WorldCampaign/WorldStatics/FRTS_WorldStatics.h"

// Sizes and colours of UI sketch sheet 6 B, in UI units at the 1920x1080 reference.
namespace WorldMapTagsLayout
{
	// Texts are refreshed and pieces added or dropped at this rate; positions follow the camera every frame.
	constexpr float DataRefreshIntervalSeconds = 0.5f;

	constexpr int32 CountryNameFontSize = 11;
	constexpr int32 CountryNameLetterSpacing = 300;
	// The liberation bar is a trough: a dark inset with a thin lighter rim, so an empty bar does not read as an
	// underline of the name. The fill grows inside the rim.
	const FVector2D LiberationBarSize(120.f, 7.f);
	constexpr float LiberationBarTopGap = 3.f;
	constexpr float LiberationRimWidth = 1.f;
	constexpr float LiberationCornerRadius = 1.f;
	const FLinearColor LiberationTroughColor(FColor::FromHex(TEXT("0B0E10F2")));
	const FLinearColor LiberationRimColor(FColor::FromHex(TEXT("A39E8CB3")));
	const FVector2D LiberationFillOrigin(LiberationRimWidth, LiberationRimWidth);
	const FVector2D LiberationFillRoom(LiberationBarSize.X - LiberationRimWidth * 2.f,
	                                   LiberationBarSize.Y - LiberationRimWidth * 2.f);
	constexpr int32 LiberationTroughZOrder = 0;
	constexpr int32 LiberationFillZOrder = 1;

	// Room kept between a country label and the player HQ, and between two labels.
	constexpr float CountryLabelGap = 6.f;
	// Half the room the player HQ model takes around its projected anchor at the map's default zoom; a label that
	// would lie on it is lifted above it.
	const FVector2D PlayerHQMarkerHalfSize(42.f, 36.f);
	// Height a tag plate is taken to have until it has been laid out once and reports its own.
	constexpr float PieceTagFallbackHeight = 34.f;

	// A mission's name sits on a small dark plate under its piece, so it reads on any country colour.
	constexpr int32 MissionNameFontSize = 9;
	constexpr int32 MissionNameLetterSpacing = 120;
	const FMargin MissionTagPadding(7.f, 2.f, 7.f, 3.f);
	// From the projected anchor of the piece to the middle of the plate.
	const FVector2D MissionTagOffsetFromPiece(0.f, 34.f);
	// Half the room a mission piece takes around its projected anchor at the map's default zoom.
	const FVector2D MissionPieceHalfSize(24.f, 24.f);
	// Lifting a label off one mission can put it on the next one; this many rounds settle a cluster of missions.
	constexpr int32 MissionClearancePasses = 3;

	constexpr int32 TagHeadlineFontSize = 9;
	constexpr int32 TagHeadlineLetterSpacing = 60;
	constexpr int32 TagTurnsFontSize = 7;
	const FMargin TagPadding(8.f, 4.f, 10.f, 5.f);
	constexpr float TagCornerRadius = 2.f;
	constexpr float TagOutlineWidth = 1.f;
	// The plate sits right of the piece, its middle a little above the piece's centre.
	const FVector2D TagOffsetFromPiece(22.f, -14.f);
	const FVector2D TagAlignment(0.f, 0.5f);
	const FLinearColor ConvoyAmber(FColor::FromHex(TEXT("FFB347")));

	// The dashed arrow: leaves the piece and stops short of the target so neither marker is covered.
	constexpr float ArrowDashLength = 8.f;
	constexpr float ArrowGapLength = 6.f;
	constexpr float ArrowStartClearance = 16.f;
	constexpr float ArrowEndClearance = 12.f;
	constexpr float ArrowMinLength = 24.f;
	constexpr float ArrowHeadLength = 9.f;
	constexpr float ArrowHeadHalfWidth = 5.f;
	constexpr int32 ArrowMaxDashCount = 400;
	// Arrows may run this far past the screen edge before they are cut.
	constexpr float ArrowClipMargin = 32.f;

	constexpr int32 ArrowLayerZOrder = 0;
	constexpr int32 CountryLabelZOrder = 1;
	constexpr int32 MissionTagZOrder = 2;
	constexpr int32 PieceTagZOrder = 3;

	FText GetDivisionTurnsText(const int32 TurnsAway)
	{
		if (TurnsAway == 1)
		{
			return NSLOCTEXT("WorldMapTags", "DivisionOneTurnAway", "1 turn away");
		}
		return FText::Format(NSLOCTEXT("WorldMapTags", "DivisionTurnsAway", "{0} turns away"),
		                     FText::AsNumber(TurnsAway));
	}

	FText GetConvoyTurnsText(const int32 TurnsAway)
	{
		if (TurnsAway == 1)
		{
			return NSLOCTEXT("WorldMapTags", "ConvoyNextTurn", "next turn");
		}
		return FText::Format(NSLOCTEXT("WorldMapTags", "ConvoyTurns", "{0} turns"), FText::AsNumber(TurnsAway));
	}

	void SetVisibleIfChanged(UWidget& Widget, const bool bVisible)
	{
		const ESlateVisibility WantedVisibility = bVisible ? ESlateVisibility::HitTestInvisible
			                                          : ESlateVisibility::Collapsed;
		if (Widget.GetVisibility() != WantedVisibility)
		{
			Widget.SetVisibility(WantedVisibility);
		}
	}

	/**
	 * Shows or hides a country label. Hidden, not collapsed: a hidden label is still laid out, so its size stays
	 * known for the placement pass of the next frame.
	 */
	void SetLabelShownIfChanged(UWidget& Label, const bool bShown)
	{
		const ESlateVisibility WantedVisibility = bShown ? ESlateVisibility::HitTestInvisible
			                                          : ESlateVisibility::Hidden;
		if (Label.GetVisibility() != WantedVisibility)
		{
			Label.SetVisibility(WantedVisibility);
		}
	}

	/** @return Whether a tag plate at this position lies wholly below the campaign top bar. */
	bool GetIsPiecePlateClearOfTopBar(const UWidget& Plate, const FVector2D& PlatePosition)
	{
		// The plate's slot is aligned on its left middle, so only its height matters for the bar.
		const FVector2D LaidOutSize = Plate.GetDesiredSize();
		const FVector2D PlateSize(LaidOutSize.X, LaidOutSize.Y > 0. ? LaidOutSize.Y : PieceTagFallbackHeight);
		return WorldMapTagsMath::GetIsTagClearOfTopBar(PlatePosition, PlateSize,
		                                               WorldMapScreenProjection::CampaignTopBarHeight);
	}
}

void UW_WorldMapTags::InitMapTags(AWorldPlayerController* WorldPlayerController, const FWorldCampaignUILook* Look)
{
	M_WorldPlayerController = WorldPlayerController;
	M_Look = Look;
	if (M_Look == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_WorldMapTags::InitMapTags was given no look."));
		return;
	}

	BuildWidgetTree();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	M_SecondsUntilDataRefresh = 0.f;
}

void UW_WorldMapTags::SetTagsVisible(const bool bVisible)
{
	bM_TagsVisible = bVisible;
	SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bVisible)
	{
		// Pieces may have moved while the page was open.
		M_SecondsUntilDataRefresh = 0.f;
	}
}

void UW_WorldMapTags::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (not bM_TagsVisible || M_Look == nullptr || not GetIsValidWorldPlayerController())
	{
		return;
	}

	M_SecondsUntilDataRefresh -= InDeltaTime;
	if (M_SecondsUntilDataRefresh <= 0.f)
	{
		M_SecondsUntilDataRefresh = WorldMapTagsLayout::DataRefreshIntervalSeconds;
		if (not bM_HasBuiltCountryLabels)
		{
			TryBuildCountryLabels();
		}
		RefreshPlayerHQLocation();
		RefreshMissionTags();
		RefreshPieceTags();
	}

	RepositionNameLabels(MyGeometry);
	RepositionPieceTags(MyGeometry);
	RepositionUnknownMissionTag(MyGeometry);
}

void UW_WorldMapTags::NativeDestruct()
{
	UnbindCountryRegulator();
	Super::NativeDestruct();
}

void UW_WorldMapTags::BuildWidgetTree()
{
	using namespace WorldMapTagsLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	M_RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_RootCanvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = M_RootCanvas;

	M_ArrowLayer = CreateWidget<UW_WorldMapArrowLayer>(this, UW_WorldMapArrowLayer::StaticClass());
	if (not IsValid(M_ArrowLayer))
	{
		return;
	}

	M_ArrowLayer->SetVisibility(ESlateVisibility::HitTestInvisible);
	// Painted by hand from data that changes every frame, so it must repaint even under global invalidation.
	M_ArrowLayer->ForceVolatile(true);
	UCanvasPanelSlot* const ArrowSlot = M_RootCanvas->AddChildToCanvas(M_ArrowLayer);
	ArrowSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	ArrowSlot->SetOffsets(FMargin(0.f));
	ArrowSlot->SetZOrder(ArrowLayerZOrder);
}

UWorldCountryOccupationRegulator* UW_WorldMapTags::GetOrBindCountryRegulator()
{
	if (M_CountryRegulator.IsValid())
	{
		return M_CountryRegulator.Get();
	}

	if (not GetIsValidWorldPlayerController())
	{
		return nullptr;
	}

	// Not an error while the map is still being set up: the generator is found during the controller's BeginPlay.
	const AGeneratorWorldCampaign* const WorldGenerator = M_WorldPlayerController->GetWorldGenerator();
	if (not IsValid(WorldGenerator))
	{
		return nullptr;
	}

	UWorldCountryOccupationRegulator* const CountryRegulator =
		WorldGenerator->FindComponentByClass<UWorldCountryOccupationRegulator>();
	if (not IsValid(CountryRegulator))
	{
		return nullptr;
	}

	M_CountryRegulator = CountryRegulator;
	CountryRegulator->OnCountryLiberationChanged.AddUniqueDynamic(
		this, &UW_WorldMapTags::HandleCountryLiberationChanged);
	return CountryRegulator;
}

void UW_WorldMapTags::UnbindCountryRegulator()
{
	if (not M_CountryRegulator.IsValid())
	{
		return;
	}

	M_CountryRegulator->OnCountryLiberationChanged.RemoveDynamic(
		this, &UW_WorldMapTags::HandleCountryLiberationChanged);
}

void UW_WorldMapTags::TryBuildCountryLabels()
{
	const UWorldCountryOccupationRegulator* const CountryRegulator = GetOrBindCountryRegulator();
	if (not IsValid(CountryRegulator))
	{
		return;
	}

	TMap<FName, TArray<FVector>> AnchorLocationsByCountry;
	GatherCountryAnchorLocations(*CountryRegulator, AnchorLocationsByCountry);
	if (AnchorLocationsByCountry.Num() == 0)
	{
		// Countries are mapped after generation or restore; try again on the next refresh.
		return;
	}

	const TArray<FName> CountryIds = CountryRegulator->GetCountryIds();
	for (const FName CountryId : CountryIds)
	{
		const TArray<FVector>* const AnchorLocations = AnchorLocationsByCountry.Find(CountryId);
		FVector LabelPoint;
		if (M_CountryLabels.Contains(CountryId) || CountryRegulator->GetTargetCount(CountryId) <= 0
			|| AnchorLocations == nullptr || not WorldMapTagsMath::TryGetCountryLabelPoint(*AnchorLocations, LabelPoint))
		{
			continue;
		}

		CreateCountryLabel(CountryId, LabelPoint, CountryRegulator->GetCountryLiberation(CountryId));
	}

	// No country counted a target yet means the tally has not run; keep trying.
	bM_HasBuiltCountryLabels = M_CountryLabels.Num() > 0;
}

void UW_WorldMapTags::GatherCountryAnchorLocations(const UWorldCountryOccupationRegulator& CountryRegulator,
                                                   TMap<FName, TArray<FVector>>& OutAnchorLocationsByCountry) const
{
	OutAnchorLocationsByCountry.Reset();
	if (not GetIsValidWorldPlayerController())
	{
		return;
	}

	const AGeneratorWorldCampaign* const WorldGenerator = M_WorldPlayerController->GetWorldGenerator();
	if (not IsValid(WorldGenerator))
	{
		return;
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : WorldGenerator->GetPlacementState().CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const FName CountryId = CountryRegulator.GetCountryIdForAnchorKey(AnchorPoint->GetAnchorKey());
		if (CountryId.IsNone())
		{
			continue;
		}
		OutAnchorLocationsByCountry.FindOrAdd(CountryId).Add(AnchorPoint->GetActorLocation());
	}
}

void UW_WorldMapTags::CreateCountryLabel(const FName CountryId, const FVector& LabelPoint, const float Liberation)
{
	using namespace WorldMapTagsLayout;
	if (not IsValid(M_RootCanvas))
	{
		return;
	}

	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->SetVisibility(ESlateVisibility::HitTestInvisible);
	UTextBlock* const NameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, WorldBuildSiteHelpers::GetCountryDisplayName(CountryId).ToUpper(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, CountryNameFontSize, CountryNameLetterSpacing),
		FSlateColor(WorldCampaignUIColors::Cream));
	Column->AddChildToVerticalBox(NameText)->SetHorizontalAlignment(HAlign_Center);

	USizeBox* const BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(LiberationBarSize.X);
	BarBox->SetHeightOverride(LiberationBarSize.Y);
	UCanvasPanel* const BarCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	BarBox->SetContent(BarCanvas);
	DeckBuilderWidgets::PlaceOnCanvas(
		*BarCanvas, DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateRoundedBoxBrush(
			                                          LiberationTroughColor, LiberationCornerRadius, LiberationRimColor,
			                                          LiberationRimWidth)),
		FVector2D::ZeroVector, LiberationBarSize, LiberationTroughZOrder);
	UCanvasPanelSlot* const FillSlot = DeckBuilderWidgets::PlaceOnCanvas(
		*BarCanvas, DeckBuilderWidgets::MakeImage(*WidgetTree, FWorldCampaignUILook::MakeFlatBrush(
			                                          WorldCampaignUIColors::Good)),
		LiberationFillOrigin, FVector2D(0.f, LiberationFillRoom.Y), LiberationFillZOrder);
	UVerticalBoxSlot* const BarSlot = Column->AddChildToVerticalBox(BarBox);
	BarSlot->SetHorizontalAlignment(HAlign_Center);
	BarSlot->SetPadding(FMargin(0.f, LiberationBarTopGap, 0.f, 0.f));

	UCanvasPanelSlot* const ColumnSlot = M_RootCanvas->AddChildToCanvas(Column);
	ColumnSlot->SetAnchors(FAnchors(0.f, 0.f));
	ColumnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ColumnSlot->SetAutoSize(true);
	ColumnSlot->SetPosition(FVector2D::ZeroVector);
	ColumnSlot->SetZOrder(CountryLabelZOrder);
	// Hidden until the next reposition has projected it, so it never flashes at the top-left corner.
	Column->SetVisibility(ESlateVisibility::Hidden);

	FWorldMapCountryLabel CountryLabel;
	CountryLabel.Root = Column;
	CountryLabel.FillSlot = FillSlot;
	CountryLabel.LabelPoint = LabelPoint;
	SetCountryLiberationFill(CountryLabel, Liberation);
	M_CountryLabels.Add(CountryId, CountryLabel);
	M_CountryLabelOrder.Add(CountryId);
}

void UW_WorldMapTags::SetCountryLiberationFill(const FWorldMapCountryLabel& CountryLabel, const float Liberation) const
{
	if (not IsValid(CountryLabel.FillSlot))
	{
		return;
	}

	const FVector2D& FillRoom = WorldMapTagsLayout::LiberationFillRoom;
	CountryLabel.FillSlot->SetSize(FVector2D(FillRoom.X * FMath::Clamp(Liberation, 0.f, 1.f), FillRoom.Y));
}

void UW_WorldMapTags::HandleCountryLiberationChanged(const FName CountryId, const float Liberation)
{
	const FWorldMapCountryLabel* const CountryLabel = M_CountryLabels.Find(CountryId);
	if (CountryLabel == nullptr)
	{
		return;
	}
	SetCountryLiberationFill(*CountryLabel, Liberation);
}

void UW_WorldMapTags::RefreshPieceTags()
{
	const TArray<FGuid> TaggedPieceKeys = GatherTaggedPieceKeys();

	// Collected first: removing while iterating the map would invalidate the iteration.
	TArray<FGuid> StalePieceKeys;
	for (const TPair<FGuid, FWorldMapPieceTag>& PieceTagByKey : M_PieceTags)
	{
		if (not TaggedPieceKeys.Contains(PieceTagByKey.Key) || not PieceTagByKey.Value.Piece.IsValid())
		{
			StalePieceKeys.Add(PieceTagByKey.Key);
		}
	}
	for (const FGuid& StalePieceKey : StalePieceKeys)
	{
		RemovePieceTag(StalePieceKey);
	}

	const UWorldDivisionManager* const DivisionManager = M_WorldPlayerController->GetWorldDivisionManager();
	for (const FGuid& PieceKey : TaggedPieceKeys)
	{
		if (M_PieceTags.Contains(PieceKey) || not IsValid(DivisionManager))
		{
			continue;
		}

		AWorldDivisionBase* const Piece = DivisionManager->FindDivisionByKey(PieceKey);
		if (IsValid(Piece))
		{
			CreatePieceTag(*Piece);
		}
	}

	for (TPair<FGuid, FWorldMapPieceTag>& PieceTagByKey : M_PieceTags)
	{
		RefreshPieceTagContent(PieceTagByKey.Value);
	}
}

TArray<FGuid> UW_WorldMapTags::GatherTaggedPieceKeys() const
{
	TArray<FGuid> TaggedPieceKeys;
	if (not GetIsValidWorldPlayerController())
	{
		return TaggedPieceKeys;
	}

	const UWorldDivisionManager* const DivisionManager = M_WorldPlayerController->GetWorldDivisionManager();
	if (not IsValid(DivisionManager))
	{
		return TaggedPieceKeys;
	}

	// The enemy turn remembers the target of every piece it sent, also once the piece stands still at it.
	const UWorldCampaignProgressManager* const ProgressManager =
		M_WorldPlayerController->GetWorldCampaignProgressManager();
	const TMap<FGuid, FGuid>* const TargetAnchorByPieceKey = IsValid(ProgressManager)
		                                                         ? &ProgressManager->GetProgressState().EnemyTurn.
		                                                         ReactionTargetAnchorByPieceKey
		                                                         : nullptr;
	for (const TObjectPtr<AWorldDivisionBase>& Piece : DivisionManager->GetWorldDivisions())
	{
		if (not IsValid(Piece) || Piece->GetOwningPlayer() != WorldDivisionOwners::EnemyOwner)
		{
			continue;
		}

		const bool bHasTarget = Piece->GetHasPendingMoveOrder()
			|| (TargetAnchorByPieceKey != nullptr && TargetAnchorByPieceKey->Contains(Piece->GetDivisionKey()));
		if (bHasTarget)
		{
			TaggedPieceKeys.Add(Piece->GetDivisionKey());
		}
	}
	return TaggedPieceKeys;
}

void UW_WorldMapTags::CreatePieceTag(AWorldDivisionBase& Piece)
{
	using namespace WorldMapTagsLayout;
	if (not IsValid(M_RootCanvas))
	{
		return;
	}

	FWorldMapPieceTag PieceTag;
	PieceTag.Piece = &Piece;
	PieceTag.Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	PieceTag.Plate->SetBrush(FSlateRoundedBoxBrush(WorldCampaignUIColors::PlateFill, TagCornerRadius,
	                                               WorldCampaignUIColors::PlateBorder, TagOutlineWidth));
	PieceTag.Plate->SetPadding(TagPadding);
	PieceTag.Plate->SetVisibility(ESlateVisibility::HitTestInvisible);

	UVerticalBox* const Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PieceTag.Plate->SetContent(Lines);
	PieceTag.HeadlineText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, TagHeadlineFontSize, TagHeadlineLetterSpacing),
		FSlateColor(WorldCampaignUIColors::Bad));
	Lines->AddChildToVerticalBox(PieceTag.HeadlineText);
	PieceTag.TurnsText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, TagTurnsFontSize),
		M_Look->Deck.Style.MutedTextColor);
	Lines->AddChildToVerticalBox(PieceTag.TurnsText);

	UCanvasPanelSlot* const PlateSlot = M_RootCanvas->AddChildToCanvas(PieceTag.Plate);
	PlateSlot->SetAnchors(FAnchors(0.f, 0.f));
	PlateSlot->SetAlignment(TagAlignment);
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetPosition(FVector2D::ZeroVector);
	PlateSlot->SetZOrder(PieceTagZOrder);
	// Collapsed until the next reposition has projected it, so it never flashes at the top-left corner.
	PieceTag.Plate->SetVisibility(ESlateVisibility::Collapsed);

	M_PieceTags.Add(Piece.GetDivisionKey(), PieceTag);
}

void UW_WorldMapTags::RemovePieceTag(const FGuid& PieceKey)
{
	FWorldMapPieceTag RemovedTag;
	if (not M_PieceTags.RemoveAndCopyValue(PieceKey, RemovedTag))
	{
		return;
	}

	if (IsValid(RemovedTag.Plate))
	{
		RemovedTag.Plate->RemoveFromParent();
	}
}

void UW_WorldMapTags::RefreshPieceTagContent(FWorldMapPieceTag& PieceTag) const
{
	using namespace WorldMapTagsLayout;
	const AWorldDivisionBase* const Piece = PieceTag.Piece.Get();
	if (not IsValid(Piece) || not IsValid(PieceTag.HeadlineText))
	{
		return;
	}

	if (Piece->GetIsSupplyConvoy())
	{
		PieceTag.HeadlineText->SetText(NSLOCTEXT("WorldMapTags", "Convoy", "CONVOY"));
		PieceTag.HeadlineText->SetColorAndOpacity(FSlateColor(ConvoyAmber));
	}
	else
	{
		PieceTag.HeadlineText->SetText(FText::Format(NSLOCTEXT("WorldMapTags", "DivisionStrength", "+{0} %"),
		                                             FText::AsNumber(Piece->GetCurrentStrengthPercentage())));
		PieceTag.HeadlineText->SetColorAndOpacity(FSlateColor(WorldCampaignUIColors::Bad));
	}

	PieceTag.bHasArrow = Piece->GetHasPendingMoveOrder();
	PieceTag.ArrowTarget = Piece->GetPendingTargetLocation();
	// The path index jumps ahead when a move starts, so the count is only read while the piece stands still.
	if (not Piece->GetIsMoving())
	{
		RefreshPieceTurnsText(PieceTag, *Piece);
	}
}

void UW_WorldMapTags::RefreshPieceTurnsText(FWorldMapPieceTag& PieceTag, const AWorldDivisionBase& Piece) const
{
	using namespace WorldMapTagsLayout;
	if (not IsValid(PieceTag.TurnsText))
	{
		return;
	}

	const float RemainingDistance = Piece.GetHasPendingMoveOrder()
		                                ? WorldMapTagsMath::GetRemainingPathLengthXY(
			                                Piece.GetActorLocation(), Piece.GetPendingPathPoints(),
			                                Piece.GetNextPathPointIndex())
		                                : 0.f;
	const int32 TurnsAway = WorldMapTagsMath::GetTurnsAway(RemainingDistance, Piece.GetDistanceTravelledPerTurn());
	if (TurnsAway <= 0)
	{
		PieceTag.TurnsText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	PieceTag.TurnsText->SetText(Piece.GetIsSupplyConvoy() ? GetConvoyTurnsText(TurnsAway)
		                            : GetDivisionTurnsText(TurnsAway));
	PieceTag.TurnsText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UW_WorldMapTags::RefreshPlayerHQLocation()
{
	M_PlayerHQLocation = FRTS_WorldStatics::GetPlayerHQWorldLocation(M_WorldPlayerController.Get());
	// The statics answer with the origin while the campaign state has no HQ yet.
	bM_HasPlayerHQLocation = not M_PlayerHQLocation.IsNearlyZero();
}

void UW_WorldMapTags::RefreshMissionTags()
{
	// Collected first: removing while iterating the map would invalidate the iteration.
	TArray<FGuid> StaleAnchorKeys;
	for (const TPair<FGuid, FWorldMapMissionTag>& MissionTagByKey : M_MissionTags)
	{
		if (not MissionTagByKey.Value.MissionObject.IsValid())
		{
			StaleAnchorKeys.Add(MissionTagByKey.Key);
		}
	}
	for (const FGuid& StaleAnchorKey : StaleAnchorKeys)
	{
		FWorldMapMissionTag RemovedTag;
		M_MissionTags.RemoveAndCopyValue(StaleAnchorKey, RemovedTag);
		M_MissionTagOrder.Remove(StaleAnchorKey);
		if (IsValid(RemovedTag.Plate))
		{
			RemovedTag.Plate->RemoveFromParent();
		}
	}

	// Not an error while the map is still being set up: the generator is found during the controller's BeginPlay.
	const AGeneratorWorldCampaign* const WorldGenerator = M_WorldPlayerController->GetWorldGenerator();
	if (not IsValid(WorldGenerator))
	{
		return;
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : WorldGenerator->GetPlacementState().CachedAnchors)
	{
		AWorldMissionObject* const MissionObject = IsValid(AnchorPoint)
			                                           ? Cast<AWorldMissionObject>(AnchorPoint->GetPromotedWorldObject())
			                                           : nullptr;
		if (not IsValid(MissionObject) || M_MissionTags.Contains(MissionObject->GetAnchorKey()))
		{
			continue;
		}

		const FText MissionName = MissionObject->GetMissionDisplayName();
		if (not MissionName.IsEmpty())
		{
			CreateMissionTag(*MissionObject, MissionName);
		}
	}
}

void UW_WorldMapTags::CreateMissionTag(AWorldMissionObject& MissionObject, const FText& MissionName)
{
	using namespace WorldMapTagsLayout;
	if (not IsValid(M_RootCanvas))
	{
		return;
	}

	FWorldMapMissionTag MissionTag;
	MissionTag.MissionObject = &MissionObject;
	MissionTag.Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	MissionTag.Plate->SetBrush(FSlateRoundedBoxBrush(WorldCampaignUIColors::PlateFill, TagCornerRadius,
	                                                 WorldCampaignUIColors::PlateBorder, TagOutlineWidth));
	MissionTag.Plate->SetPadding(MissionTagPadding);
	MissionTag.Plate->SetContent(DeckBuilderWidgets::MakeText(
		*WidgetTree, MissionName.ToUpper(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, MissionNameFontSize, MissionNameLetterSpacing),
		FSlateColor(WorldCampaignUIColors::Cream)));

	UCanvasPanelSlot* const PlateSlot = M_RootCanvas->AddChildToCanvas(MissionTag.Plate);
	PlateSlot->SetAnchors(FAnchors(0.f, 0.f));
	PlateSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetPosition(FVector2D::ZeroVector);
	PlateSlot->SetZOrder(MissionTagZOrder);
	// Hidden until the next reposition has projected it, so it never flashes at the top-left corner.
	MissionTag.Plate->SetVisibility(ESlateVisibility::Hidden);

	const FGuid AnchorKey = MissionObject.GetAnchorKey();
	M_MissionTags.Add(AnchorKey, MissionTag);
	M_MissionTagOrder.Add(AnchorKey);
}

bool UW_WorldMapTags::GetShowsMissionTag(const AWorldMissionObject& MissionObject) const
{
	// The fog of war decides what may be outlined; a name on a piece the player cannot see would give it away.
	return not MissionObject.GetIsClearedByPlayer() && not MissionObject.IsHidden() && MissionObject.GetCanBeOutlined();
}

void UW_WorldMapTags::RepositionNameLabels(const FGeometry& MyGeometry)
{
	// Same length and order: the placement of a name and the widget it belongs to. Missions first, so they win.
	TArray<FWorldMapTagPlacement> Placements;
	TArray<UWidget*> PlacedLabelRoots;
	AddMissionTagPlacements(MyGeometry, Placements, PlacedLabelRoots);
	AddCountryLabelPlacements(MyGeometry, Placements, PlacedLabelRoots);

	WorldMapTagsMath::ResolveTagOverlaps(Placements, WorldMapTagsLayout::CountryLabelGap,
	                                     WorldMapScreenProjection::CampaignTopBarHeight);
	for (int32 PlacementIndex = 0; PlacementIndex < Placements.Num(); ++PlacementIndex)
	{
		const FWorldMapTagPlacement& Placement = Placements[PlacementIndex];
		UWidget* const LabelRoot = PlacedLabelRoots[PlacementIndex];
		WorldMapTagsLayout::SetLabelShownIfChanged(*LabelRoot, Placement.bIsVisible);
		if (Placement.bIsVisible)
		{
			LabelRoot->SetRenderTranslation(Placement.Center);
		}
	}
}

void UW_WorldMapTags::AddMissionTagPlacements(const FGeometry& MyGeometry,
                                              TArray<FWorldMapTagPlacement>& OutPlacements,
                                              TArray<UWidget*>& OutLabelRoots)
{
	for (const FGuid& AnchorKey : M_MissionTagOrder)
	{
		const FWorldMapMissionTag* const MissionTag = M_MissionTags.Find(AnchorKey);
		if (MissionTag == nullptr || not IsValid(MissionTag->Plate))
		{
			continue;
		}

		const AWorldMissionObject* const MissionObject = MissionTag->MissionObject.Get();
		if (not IsValid(MissionObject) || not GetShowsMissionTag(*MissionObject))
		{
			WorldMapTagsLayout::SetLabelShownIfChanged(*MissionTag->Plate, false);
			continue;
		}

		FWorldMapTagPlacement Placement;
		if (TryGetLabelPlacement(MissionTag->Plate, MissionObject->GetActorLocation(),
		                         WorldMapTagsLayout::MissionTagOffsetFromPiece, MyGeometry, Placement))
		{
			OutPlacements.Add(Placement);
			OutLabelRoots.Add(MissionTag->Plate.Get());
		}
	}
}

void UW_WorldMapTags::AddCountryLabelPlacements(const FGeometry& MyGeometry,
                                                TArray<FWorldMapTagPlacement>& OutPlacements,
                                                TArray<UWidget*>& OutLabelRoots)
{
	using namespace WorldMapTagsLayout;
	FVector2D PlayerHQPosition;
	const bool bHasPlayerHQPosition = bM_HasPlayerHQLocation && WorldMapScreenProjection::ProjectWorldToWidgetLocal(
		M_WorldPlayerController.Get(), this, MyGeometry, M_PlayerHQLocation, PlayerHQPosition);

	// A copy: the country labels are appended to the same array the mission plates are in.
	const TArray<FWorldMapTagPlacement> MissionPlates = OutPlacements;
	for (const FName CountryId : M_CountryLabelOrder)
	{
		const FWorldMapCountryLabel* const CountryLabel = M_CountryLabels.Find(CountryId);
		FWorldMapTagPlacement Placement;
		if (CountryLabel == nullptr || not TryGetLabelPlacement(CountryLabel->Root, CountryLabel->LabelPoint,
		                                                        FVector2D::ZeroVector, MyGeometry, Placement))
		{
			continue;
		}

		if (bHasPlayerHQPosition)
		{
			Placement.Center = WorldMapTagsMath::GetTagCenterClearOfMarker(
				Placement.Center, Placement.Size, PlayerHQPosition, PlayerHQMarkerHalfSize, CountryLabelGap);
		}
		Placement.Center = GetLabelCenterClearOfMissions(Placement.Center, Placement.Size, MissionPlates);
		OutPlacements.Add(Placement);
		OutLabelRoots.Add(CountryLabel->Root.Get());
	}
}

FVector2D UW_WorldMapTags::GetLabelCenterClearOfMissions(const FVector2D& LabelCenter, const FVector2D& LabelSize,
                                                         const TConstArrayView<FWorldMapTagPlacement> MissionPlates) const
{
	using namespace WorldMapTagsLayout;
	FVector2D ClearCenter = LabelCenter;
	for (int32 PassIndex = 0; PassIndex < MissionClearancePasses; ++PassIndex)
	{
		const FVector2D CenterBeforePass = ClearCenter;
		for (const FWorldMapTagPlacement& MissionPlate : MissionPlates)
		{
			// One box around the piece and the plate under it: from the top of the piece to the bottom of the plate.
			const FVector2D PieceCenter = MissionPlate.Center - MissionTagOffsetFromPiece;
			const double BoxTop = PieceCenter.Y - MissionPieceHalfSize.Y;
			const double BoxBottom = MissionPlate.Center.Y + MissionPlate.Size.Y / 2.;
			const FVector2D BoxCenter(MissionPlate.Center.X, (BoxTop + BoxBottom) / 2.);
			const FVector2D BoxHalfSize(FMath::Max(MissionPlate.Size.X / 2., MissionPieceHalfSize.X),
			                            (BoxBottom - BoxTop) / 2.);
			ClearCenter = WorldMapTagsMath::GetTagCenterClearOfMarker(ClearCenter, LabelSize, BoxCenter, BoxHalfSize,
			                                                          CountryLabelGap);
		}
		if (ClearCenter.Equals(CenterBeforePass))
		{
			break;
		}
	}
	return ClearCenter;
}

bool UW_WorldMapTags::TryGetLabelPlacement(UWidget* const LabelRoot, const FVector& WorldPoint,
                                           const FVector2D& OffsetFromPoint, const FGeometry& MyGeometry,
                                           FWorldMapTagPlacement& OutPlacement)
{
	if (not IsValid(LabelRoot))
	{
		return false;
	}

	// Zero until the widget has been through its first layout pass; it stays hidden for that one frame.
	const FVector2D LabelSize = LabelRoot->GetDesiredSize();
	FVector2D LocalPosition;
	const bool bCanBePlaced = not LabelSize.IsNearlyZero() && WorldMapScreenProjection::ProjectWorldToWidgetLocal(
		M_WorldPlayerController.Get(), this, MyGeometry, WorldPoint, LocalPosition);
	if (not bCanBePlaced)
	{
		WorldMapTagsLayout::SetLabelShownIfChanged(*LabelRoot, false);
		return false;
	}

	// The widget's canvas slot is aligned on its middle, so this is the middle of the name.
	OutPlacement.Center = LocalPosition + OffsetFromPoint;
	OutPlacement.Size = LabelSize;
	OutPlacement.bIsVisible = true;
	return true;
}

void UW_WorldMapTags::RepositionPieceTags(const FGeometry& MyGeometry)
{
	const APlayerController* const PlayerController = M_WorldPlayerController.Get();
	TArray<FWorldMapArrowPaint> Arrows;
	for (const TPair<FGuid, FWorldMapPieceTag>& PieceTagByKey : M_PieceTags)
	{
		const FWorldMapPieceTag& PieceTag = PieceTagByKey.Value;
		const AWorldDivisionBase* const Piece = PieceTag.Piece.Get();
		if (not IsValid(PieceTag.Plate))
		{
			continue;
		}

		FVector2D PiecePosition = FVector2D::ZeroVector;
		const bool bIsProjected = IsValid(Piece) && WorldMapScreenProjection::ProjectWorldToWidgetLocal(
			PlayerController, this, MyGeometry, Piece->GetActorLocation(), PiecePosition);
		// A plate the top bar would cut in half is not drawn; its arrow still is.
		const FVector2D PlatePosition = PiecePosition + WorldMapTagsLayout::TagOffsetFromPiece;
		const bool bShowsPlate = bIsProjected
			&& WorldMapTagsLayout::GetIsPiecePlateClearOfTopBar(*PieceTag.Plate, PlatePosition);
		WorldMapTagsLayout::SetVisibleIfChanged(*PieceTag.Plate, bShowsPlate);
		if (not bIsProjected)
		{
			continue;
		}

		if (bShowsPlate)
		{
			PieceTag.Plate->SetRenderTranslation(PlatePosition);
		}
		if (PieceTag.bHasArrow)
		{
			AddPieceArrow(PieceTag, PiecePosition, MyGeometry, Arrows);
		}
	}

	if (GetIsValidArrowLayer())
	{
		M_ArrowLayer->SetArrows(MoveTemp(Arrows));
	}
}

void UW_WorldMapTags::RepositionUnknownMissionTag(const FGeometry& MyGeometry)
{
	const AWorldMissionObject* const HoveredMission = M_WorldPlayerController->GetHoveredUnknownMission();
	if (not IsValid(HoveredMission) || not HoveredMission->GetIsUnidentifiedUnderFog())
	{
		if (IsValid(M_UnknownMissionTag))
		{
			WorldMapTagsLayout::SetLabelShownIfChanged(*M_UnknownMissionTag, false);
		}
		return;
	}

	if (not IsValid(M_UnknownMissionTag))
	{
		CreateUnknownMissionTag();
	}

	// The plain marker lies above the fog, the piece seen through the fog stands on the map.
	const UWorldUnknownMissionMarkerComponent* const PlainMarker =
		HoveredMission->GetIsShownAsUnknownMarker() ? HoveredMission->GetUnknownMarker() : nullptr;
	const FVector TagWorldPoint = IsValid(PlainMarker) ? PlainMarker->GetComponentLocation()
		                              : HoveredMission->GetActorLocation();
	// Placed where a mission name would be; hidden for the one frame before its first layout.
	FWorldMapTagPlacement Placement;
	if (not TryGetLabelPlacement(M_UnknownMissionTag, TagWorldPoint,
	                             WorldMapTagsLayout::MissionTagOffsetFromPiece, MyGeometry, Placement))
	{
		return;
	}

	const bool bIsClearOfTopBar = WorldMapTagsMath::GetIsTagClearOfTopBar(
		Placement.Center, Placement.Size, WorldMapScreenProjection::CampaignTopBarHeight);
	WorldMapTagsLayout::SetLabelShownIfChanged(*M_UnknownMissionTag, bIsClearOfTopBar);
	if (bIsClearOfTopBar)
	{
		M_UnknownMissionTag->SetRenderTranslation(Placement.Center);
	}
}

void UW_WorldMapTags::CreateUnknownMissionTag()
{
	using namespace WorldMapTagsLayout;
	if (not IsValid(M_RootCanvas))
	{
		return;
	}

	M_UnknownMissionTag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_UnknownMissionTag->SetBrush(FSlateRoundedBoxBrush(WorldCampaignUIColors::PlateFill, TagCornerRadius,
	                                                    WorldCampaignUIColors::PlateBorder, TagOutlineWidth));
	M_UnknownMissionTag->SetPadding(MissionTagPadding);
	// Muted, not cream: the plate must not read as a mission name.
	M_UnknownMissionTag->SetContent(DeckBuilderWidgets::MakeText(
		*WidgetTree, UWorldFogHintSettings::Get()->UnknownMissionHoverText,
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, MissionNameFontSize, MissionNameLetterSpacing),
		M_Look->Deck.Style.MutedTextColor));

	UCanvasPanelSlot* const PlateSlot = M_RootCanvas->AddChildToCanvas(M_UnknownMissionTag);
	PlateSlot->SetAnchors(FAnchors(0.f, 0.f));
	PlateSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetPosition(FVector2D::ZeroVector);
	PlateSlot->SetZOrder(PieceTagZOrder);
	// Hidden until it has been laid out and projected once, so it never flashes at the top-left corner.
	M_UnknownMissionTag->SetVisibility(ESlateVisibility::Hidden);
}

void UW_WorldMapTags::AddPieceArrow(const FWorldMapPieceTag& PieceTag, const FVector2D& PiecePosition,
                                    const FGeometry& MyGeometry, TArray<FWorldMapArrowPaint>& OutArrows)
{
	using namespace WorldMapTagsLayout;
	FVector2D TargetPosition;
	if (not WorldMapScreenProjection::ProjectWorldToWidgetLocal(M_WorldPlayerController.Get(), this, MyGeometry,
	                                                            PieceTag.ArrowTarget, TargetPosition))
	{
		return;
	}

	const FVector2D Delta = TargetPosition - PiecePosition;
	const double FullLength = Delta.Size();
	if (FullLength < ArrowMinLength + ArrowStartClearance + ArrowEndClearance)
	{
		return;
	}

	const FVector2D Direction = Delta / FullLength;
	FVector2D ArrowStart = PiecePosition + Direction * ArrowStartClearance;
	const FVector2D UnclippedEnd = TargetPosition - Direction * ArrowEndClearance;
	FVector2D ArrowEnd = UnclippedEnd;
	const FVector2D ClipMargin(ArrowClipMargin, ArrowClipMargin);
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	if (not WorldMapTagsMath::ClipSegmentToRect(ArrowStart, ArrowEnd, -ClipMargin, LocalSize + ClipMargin))
	{
		return;
	}

	FWorldMapArrowPaint Arrow;
	Arrow.Color = WorldCampaignUIColors::Cream;
	WorldMapTagsMath::BuildDashSegments(ArrowStart, ArrowEnd, ArrowDashLength, ArrowGapLength, ArrowMaxDashCount,
	                                    Arrow.Dashes);
	Arrow.bHasHead = ArrowEnd.Equals(UnclippedEnd, KINDA_SMALL_NUMBER);
	if (Arrow.bHasHead)
	{
		const FVector2D Perpendicular(-Direction.Y, Direction.X);
		const FVector2D HeadBase = ArrowEnd - Direction * ArrowHeadLength;
		Arrow.HeadTip = ArrowEnd;
		Arrow.HeadLeft = HeadBase + Perpendicular * ArrowHeadHalfWidth;
		Arrow.HeadRight = HeadBase - Perpendicular * ArrowHeadHalfWidth;
	}
	OutArrows.Add(MoveTemp(Arrow));
}

bool UW_WorldMapTags::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_WorldPlayerController"), TEXT("UW_WorldMapTags::GetIsValidWorldPlayerController"), this);
	return false;
}

bool UW_WorldMapTags::GetIsValidArrowLayer() const
{
	if (IsValid(M_ArrowLayer))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_ArrowLayer"), TEXT("UW_WorldMapTags::GetIsValidArrowLayer"), this);
	return false;
}
