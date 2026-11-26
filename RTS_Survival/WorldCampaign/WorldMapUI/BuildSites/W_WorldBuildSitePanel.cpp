// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/W_WorldBuildSitePanel.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireDirector.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeManager.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeRules.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/W_WorldBuildCard.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/WorldBuildSiteHelpers.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapTags/WorldMapScreenProjection.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

// Layout of the card row panel of UI sketch sheets 2 A and 7, in UI units at the 1920x1080 reference.
namespace WorldBuildSitePanelLayout
{
	// The bolted frame of sheet 14A is drawn around the plate's content, so the plate grows by it on every side.
	constexpr float FrameInset = 13.f;
	// The frame art keeps a soft shadow around its rim: the rim you see starts this far inside the plate's box.
	constexpr float FrameShadowMargin = 6.f;
	// The dark backing lies under the frame and ends below the middle of its rim, so no map shows through the plate
	// and no backing shows outside the frame.
	constexpr float BackingInset = 11.f;
	constexpr float BackingOpacity = 0.94f;

	constexpr float SidePadding = 22.f;
	constexpr float BottomPadding = 16.f;
	constexpr float TitleTop = 16.f;
	constexpr float TitleHeight = 22.f;
	constexpr float CardsTop = 46.f;
	constexpr float CardWidth = 154.f;
	constexpr float CardHeight = 196.f;
	constexpr float CardGap = 10.f;
	constexpr float StatusTop = 52.f;
	constexpr float StatusHeight = 48.f;
	constexpr float FooterGap = 12.f;
	constexpr float FooterTop = CardsTop + CardHeight + FooterGap;
	constexpr float BuildButtonWidth = 230.f;
	constexpr float BuildButtonHeight = 36.f;
	const FVector2D BuildButtonSize(BuildButtonWidth, BuildButtonHeight);
	constexpr float FailTextGapToButton = 12.f;
	// Room the refusal text keeps left of the Build button, however few cards are for sale.
	constexpr float FailTextMinWidth = 180.f;
	// Width of the text of a locked site; it wraps inside this.
	constexpr float LockedContentWidth = 400.f;
	// The plate is never narrower than its title. The title's width is read from its laid-out size; this stands in
	// until the title has been through a layout pass.
	constexpr float TitleFallbackWidth = 300.f;
	constexpr float TitleWidthSlack = 2.f;
	constexpr float TitleWidthChangeTolerance = 0.5f;

	// Each plate is only as high as what it shows: no footer row without a Build button.
	constexpr float PlateHeightForSale = FooterTop + BuildButtonHeight + BottomPadding;
	constexpr float PlateHeightBuilt = CardsTop + CardHeight + BottomPadding;
	constexpr float PlateHeightLocked = StatusTop + StatusHeight + BottomPadding;
	// A built battery: the fire mission button under the card, and under it one line saying why it is off.
	constexpr float FireCaptionGap = 6.f;
	constexpr float FireCaptionHeight = 16.f;
	constexpr float FireCaptionTop = FooterTop + BuildButtonHeight + FireCaptionGap;
	constexpr float PlateHeightBuiltWithFireMission = FireCaptionTop + FireCaptionHeight + BottomPadding;

	constexpr int32 TitleFontSize = 11;
	constexpr int32 TitleLetterSpacing = 150;
	constexpr int32 StatusFontSize = 11;
	constexpr int32 FailFontSize = 9;
	constexpr int32 ButtonFontSize = 10;
	constexpr int32 ButtonLetterSpacing = 100;

	// Space between the plate and the site, the stem's length when the plate sits right above it.
	constexpr float GapToSite = 46.f;
	constexpr float ScreenMargin = 12.f;
	// The stem ends on the rim of the site marker (its radius), so the two touch and the marker stays visible.
	constexpr float StemSiteClearance = 9.f;
	constexpr float StemWidth = 2.f;
	// The stem never leaves the plate's rounded corners.
	constexpr float StemCornerInset = 30.f;
	// The stem image hangs from the middle of its top edge and is turned around that point.
	const FVector2D StemPivot(0.5f, 0.f);

	constexpr int32 StemZOrder = 0;
	constexpr int32 BackingZOrder = 1;
	constexpr int32 PlateZOrder = 2;

	UCanvasPanelSlot* GetCanvasSlot(const UWidget* Widget)
	{
		return IsValid(Widget) ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr;
	}

	/** Moves and sizes a widget on its canvas; only touches the slot when something changed, so a tick is cheap. */
	void SetCanvasGeometryIfChanged(const UWidget* Widget, const FVector2D& Position, const FVector2D& Size)
	{
		UCanvasPanelSlot* const CanvasSlot = GetCanvasSlot(Widget);
		if (CanvasSlot == nullptr)
		{
			return;
		}

		if (not CanvasSlot->GetPosition().Equals(Position, KINDA_SMALL_NUMBER))
		{
			CanvasSlot->SetPosition(Position);
		}
		if (not CanvasSlot->GetSize().Equals(Size, KINDA_SMALL_NUMBER))
		{
			CanvasSlot->SetSize(Size);
		}
	}

	/**
	 * Width of the plate's content: what the cards, the title and the footer of this content need, and no more.
	 * @return Width between the plate's side paddings.
	 */
	float GetContentWidth(const EWorldBuildSitePanelContent Content, const float CardRowWidth, const float TitleWidth)
	{
		switch (Content)
		{
		case EWorldBuildSitePanelContent::Locked:
			return FMath::Max(TitleWidth, LockedContentWidth);
		case EWorldBuildSitePanelContent::ForSale:
			return FMath::Max3(TitleWidth, CardRowWidth, BuildButtonWidth + FailTextGapToButton + FailTextMinWidth);
		case EWorldBuildSitePanelContent::Built:
			return FMath::Max(TitleWidth, CardRowWidth);
		case EWorldBuildSitePanelContent::BuiltWithFireMission:
			return FMath::Max3(TitleWidth, CardRowWidth, BuildButtonWidth);
		}
		return FMath::Max(TitleWidth, CardRowWidth);
	}

	float GetPlateHeight(const EWorldBuildSitePanelContent Content)
	{
		switch (Content)
		{
		case EWorldBuildSitePanelContent::Locked:
			return PlateHeightLocked;
		case EWorldBuildSitePanelContent::ForSale:
			return PlateHeightForSale;
		case EWorldBuildSitePanelContent::Built:
			return PlateHeightBuilt;
		case EWorldBuildSitePanelContent::BuiltWithFireMission:
			return PlateHeightBuiltWithFireMission;
		}
		return PlateHeightForSale;
	}

	void SetShown(UWidget* Widget, const bool bShown, const ESlateVisibility ShownVisibility)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(bShown ? ShownVisibility : ESlateVisibility::Collapsed);
		}
	}
}

void UW_WorldBuildSitePanel::InitBuildSitePanel(AWorldPlayerController* WorldPlayerController,
                                                const FWorldCampaignUILook* Look)
{
	M_WorldPlayerController = WorldPlayerController;
	M_Look = Look;
	if (M_Look == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_WorldBuildSitePanel::InitBuildSitePanel was given no look."));
		return;
	}

	BuildWidgetTree();
	BindToCampaignManagers();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UW_WorldBuildSitePanel::OpenForBuildNode(AWorldBuildNode* BuildNode)
{
	if (not IsValid(BuildNode) || M_Look == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_WorldBuildSitePanel::OpenForBuildNode: no build node or no look."));
		return;
	}

	if (M_OpenBuildNode.Get() != BuildNode)
	{
		M_SelectedBuilding = EWorldCampaignBuilding::None;
	}
	M_OpenBuildNode = BuildNode;
	bM_IsOpen = true;
	bM_FollowsBuildNode = true;
	bM_HasAnchor = false;
	RefreshContent();
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlacePlate(GetCachedGeometry());
}

void UW_WorldBuildSitePanel::ClosePanel()
{
	const bool bWasOpen = bM_IsOpen;
	bM_IsOpen = false;
	M_OpenBuildNode.Reset();
	SetVisibility(ESlateVisibility::Collapsed);
	if (bWasOpen)
	{
		OnPanelClosed.Broadcast();
	}
}

AWorldBuildNode* UW_WorldBuildSitePanel::GetOpenBuildNode() const
{
	return M_OpenBuildNode.Get();
}

bool UW_WorldBuildSitePanel::GetIsCursorOverPlate() const
{
	return bM_IsOpen && IsValid(M_Plate) && M_Plate->IsHovered();
}

void UW_WorldBuildSitePanel::SetAnchorScreenPosition(const FVector2D& AnchorPosition)
{
	bM_FollowsBuildNode = false;
	bM_HasAnchor = true;
	M_AnchorPosition = AnchorPosition;
	if (bM_IsOpen)
	{
		PlacePlate(GetCachedGeometry());
	}
}

void UW_WorldBuildSitePanel::HandleCardClicked(const EWorldCampaignBuilding Building)
{
	if (M_Content != EWorldBuildSitePanelContent::ForSale)
	{
		return;
	}

	M_SelectedBuilding = Building;
	ApplySelection();
	if (IsValid(M_FailText))
	{
		M_FailText->SetText(FText::GetEmpty());
	}
}

void UW_WorldBuildSitePanel::SelectBuildingForCapture(const EWorldCampaignBuilding Building)
{
	HandleCardClicked(Building);
}

bool UW_WorldBuildSitePanel::BuildSelectedForCapture()
{
	return TryBuildSelected();
}

void UW_WorldBuildSitePanel::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (not bM_IsOpen)
	{
		return;
	}

	if (not M_OpenBuildNode.IsValid())
	{
		ClosePanel();
		return;
	}
	RefreshLayoutIfTitleWidthChanged();
	PlacePlate(MyGeometry);
}

void UW_WorldBuildSitePanel::NativeDestruct()
{
	UnbindFromCampaignManagers();
	Super::NativeDestruct();
}

void UW_WorldBuildSitePanel::BuildWidgetTree()
{
	using namespace WorldBuildSitePanelLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	M_RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = M_RootCanvas;

	M_Stem = DeckBuilderWidgets::MakeImage(*WidgetTree, FWorldCampaignUILook::MakeFlatBrush(WorldCampaignUIColors::Reach));
	DeckBuilderWidgets::PlaceOnCanvas(*M_RootCanvas, M_Stem, FVector2D::ZeroVector, FVector2D(StemWidth, GapToSite),
	                                  StemZOrder);
	M_Stem->SetRenderTransformPivot(StemPivot);
	M_Stem->SetVisibility(ESlateVisibility::Collapsed);

	// Under the frame, so the frame's rim and bolts are drawn over its edge.
	M_PlateBacking = DeckBuilderWidgets::MakeImage(*WidgetTree, FWorldCampaignUILook::MakeFlatBrush(
		                                               WorldCampaignUIColors::PlateFill.CopyWithNewOpacity(
			                                               BackingOpacity)));
	DeckBuilderWidgets::PlaceOnCanvas(*M_RootCanvas, M_PlateBacking, FVector2D::ZeroVector, FVector2D::ZeroVector,
	                                  BackingZOrder);

	BuildWidgetTree_Plate();
	BuildWidgetTree_Footer();
	SetContentLayout(EWorldBuildSitePanelContent::Locked, 0);
}

void UW_WorldBuildSitePanel::BuildWidgetTree_Plate()
{
	using namespace WorldBuildSitePanelLayout;
	const FDeckBuilderStyle& Style = M_Look->Deck.Style;
	M_Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_Plate->SetBrush(M_Look->BoltedPanel);
	M_Plate->SetPadding(FMargin(FrameInset));
	M_Plate->SetVisibility(ESlateVisibility::Visible);
	M_Plate->OnMouseButtonDownEvent.BindUFunction(
		this, GET_FUNCTION_NAME_CHECKED(UW_WorldBuildSitePanel, HandlePlateMouseDown));
	// Positions and widths are set by SetContentLayout for what the plate shows; only the heights are fixed here.
	DeckBuilderWidgets::PlaceOnCanvas(*M_RootCanvas, M_Plate, FVector2D::ZeroVector, FVector2D::ZeroVector, PlateZOrder);
	M_PlateCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_PlateCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	M_Plate->SetContent(M_PlateCanvas);

	M_TitleText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, TitleFontSize, TitleLetterSpacing), Style.TextColor);
	DeckBuilderWidgets::PlaceOnCanvas(*M_PlateCanvas, M_TitleText, FVector2D(SidePadding, TitleTop),
	                                  FVector2D(0.f, TitleHeight));

	M_CardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_CardRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_PlateCanvas, M_CardRow, FVector2D(SidePadding, CardsTop),
	                                  FVector2D(0.f, CardHeight));

	M_StatusText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, StatusFontSize),
		Style.MutedTextColor);
	M_StatusText->SetAutoWrapText(true);
	DeckBuilderWidgets::PlaceOnCanvas(*M_PlateCanvas, M_StatusText, FVector2D(SidePadding, StatusTop),
	                                  FVector2D(0.f, StatusHeight));
}

void UW_WorldBuildSitePanel::BuildWidgetTree_Footer()
{
	using namespace WorldBuildSitePanelLayout;
	const FDeckBuilderStyle& Style = M_Look->Deck.Style;
	M_FailText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.BodyFont, FailFontSize),
		FSlateColor(WorldCampaignUIColors::Bad));
	M_FailText->SetAutoWrapText(true);
	DeckBuilderWidgets::PlaceOnCanvas(*M_PlateCanvas, M_FailText, FVector2D(SidePadding, FooterTop),
	                                  FVector2D(0.f, BuildButtonHeight));

	M_BuildButtonBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, FText::GetEmpty(), M_Look->Deck.Button,
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, ButtonFontSize, ButtonLetterSpacing), Style.TextColor,
		BuildButtonSize, M_BuildButton, M_BuildLabel);
	M_BuildButton->OnClicked.AddDynamic(this, &UW_WorldBuildSitePanel::HandleBuildClicked);
	DeckBuilderWidgets::PlaceOnCanvas(*M_PlateCanvas, M_BuildButtonBox, FVector2D(SidePadding, FooterTop),
	                                  BuildButtonSize);

	// Same button as Build; only a built battery shows it.
	M_FireMissionButtonBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, NSLOCTEXT("WorldBuildSite", "FireMissionButton", "FIRE MISSION"), M_Look->Deck.Button,
		DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, ButtonFontSize, ButtonLetterSpacing), Style.TextColor,
		BuildButtonSize, M_FireMissionButton, M_FireMissionLabel);
	M_FireMissionButton->OnClicked.AddDynamic(this, &UW_WorldBuildSitePanel::HandleFireMissionClicked);
	DeckBuilderWidgets::PlaceOnCanvas(*M_PlateCanvas, M_FireMissionButtonBox, FVector2D(SidePadding, FooterTop),
	                                  BuildButtonSize);
}

void UW_WorldBuildSitePanel::BindToCampaignManagers()
{
	if (not GetIsValidWorldPlayerController())
	{
		return;
	}

	M_BuildNodeManager = M_WorldPlayerController->GetWorldBuildNodeManager();
	M_ProgressManager = M_WorldPlayerController->GetWorldCampaignProgressManager();
	M_BatteryFireDirector = M_WorldPlayerController->GetWorldBatteryFireDirector();
	if (GetIsValidBuildNodeManager())
	{
		M_BuildNodeManager->OnBuildNodeBuildingPlaced.AddUniqueDynamic(
			this, &UW_WorldBuildSitePanel::HandleBuildingPlaced);
		M_BuildNodeManager->OnBuildNodeUnlocked.AddUniqueDynamic(this, &UW_WorldBuildSitePanel::HandleBuildNodeUnlocked);
	}
	if (GetIsValidProgressManager())
	{
		M_ProgressManager->OnResourcesChanged.AddUniqueDynamic(this, &UW_WorldBuildSitePanel::HandleResourcesChanged);
	}
}

void UW_WorldBuildSitePanel::UnbindFromCampaignManagers()
{
	if (M_BuildNodeManager.IsValid())
	{
		M_BuildNodeManager->OnBuildNodeBuildingPlaced.RemoveDynamic(this, &UW_WorldBuildSitePanel::HandleBuildingPlaced);
		M_BuildNodeManager->OnBuildNodeUnlocked.RemoveDynamic(this, &UW_WorldBuildSitePanel::HandleBuildNodeUnlocked);
	}
	if (M_ProgressManager.IsValid())
	{
		M_ProgressManager->OnResourcesChanged.RemoveDynamic(this, &UW_WorldBuildSitePanel::HandleResourcesChanged);
	}
}

void UW_WorldBuildSitePanel::RefreshContent()
{
	const AWorldBuildNode* const BuildNode = M_OpenBuildNode.Get();
	if (not IsValid(BuildNode) || not IsValid(M_TitleText))
	{
		ClosePanel();
		return;
	}

	M_TitleText->SetText(WorldBuildSiteHelpers::GetSiteTitle(
		WorldBuildSiteHelpers::GetCountryDisplayName(BuildNode->GetCountryId()), GetUnlockRankOfNode(*BuildNode) + 1));
	// Lays the new title out now, so the plate is sized for it in this same call.
	M_TitleText->ForceLayoutPrepass();
	M_FailText->SetText(FText::GetEmpty());
	switch (BuildNode->GetNodeState())
	{
	case EWorldBuildNodeState::Locked:
		RefreshContent_Locked(*BuildNode);
		return;
	case EWorldBuildNodeState::Unlocked:
		RefreshContent_ForSale(*BuildNode);
		return;
	case EWorldBuildNodeState::Built:
		RefreshContent_Built(*BuildNode);
		return;
	}
}

void UW_WorldBuildSitePanel::RefreshContent_Locked(const AWorldBuildNode& BuildNode)
{
	float CurrentLiberation = 0.f;
	const AGeneratorWorldCampaign* const WorldGenerator = GetIsValidWorldPlayerController()
		                                                      ? M_WorldPlayerController->GetWorldGenerator()
		                                                      : nullptr;
	const UWorldCountryOccupationRegulator* const CountryRegulator =
		IsValid(WorldGenerator) ? WorldGenerator->FindComponentByClass<UWorldCountryOccupationRegulator>() : nullptr;
	if (IsValid(CountryRegulator))
	{
		CurrentLiberation = CountryRegulator->GetCountryLiberation(BuildNode.GetCountryId());
	}

	M_StatusText->SetText(WorldBuildSiteHelpers::GetLockedReasonText(
		GetUnlockRankOfNode(BuildNode), UWorldCampaignRulesSettings::GetNumbers().BuildNodeUnlockLiberation,
		CurrentLiberation));
	RebuildCards(TArray<EWorldCampaignBuilding>(), EWorldBuildCardMode::ForSale);
	SetContentLayout(EWorldBuildSitePanelContent::Locked, 0);
}

void UW_WorldBuildSitePanel::RefreshContent_ForSale(const AWorldBuildNode& BuildNode)
{
	const TArray<EWorldCampaignBuilding> BuildOptions = GetIsValidBuildNodeManager()
		                                                    ? M_BuildNodeManager->GetBuildOptionsForNode(
			                                                    BuildNode.GetBuildNodeId())
		                                                    : TArray<EWorldCampaignBuilding>();
	if (BuildOptions.Num() == 0)
	{
		// Only happens when no building has a price in the campaign rules.
		M_StatusText->SetText(NSLOCTEXT("WorldBuildSite", "NoBuildOptions", "No building can be ordered here."));
		RebuildCards(BuildOptions, EWorldBuildCardMode::ForSale);
		SetContentLayout(EWorldBuildSitePanelContent::Locked, 0);
		return;
	}

	RebuildCards(BuildOptions, EWorldBuildCardMode::ForSale);
	M_SelectedBuilding = ChooseDefaultSelection(BuildOptions);
	ApplySelection();
	RefreshAffordability();
	SetContentLayout(EWorldBuildSitePanelContent::ForSale, BuildOptions.Num());
}

void UW_WorldBuildSitePanel::RefreshContent_Built(const AWorldBuildNode& BuildNode)
{
	const TArray<EWorldCampaignBuilding> BuiltBuilding = {BuildNode.GetBuilding()};
	RebuildCards(BuiltBuilding, EWorldBuildCardMode::Built);
	// Quiet check: without a director the battery is shown like any other built site.
	const bool bHasFireMission = BuildNode.GetBuilding() == EWorldCampaignBuilding::Battery
		&& M_BatteryFireDirector.IsValid();
	if (not bHasFireMission)
	{
		SetContentLayout(EWorldBuildSitePanelContent::Built, BuiltBuilding.Num());
		return;
	}

	RefreshFireMissionButton(BuildNode);
	SetContentLayout(EWorldBuildSitePanelContent::BuiltWithFireMission, BuiltBuilding.Num());
}

void UW_WorldBuildSitePanel::RefreshFireMissionButton(const AWorldBuildNode& BuildNode)
{
	if (not M_BatteryFireDirector.IsValid() || not IsValid(M_FireMissionButton))
	{
		return;
	}

	const EWorldBatteryFireResult Readiness = M_BatteryFireDirector->GetBatteryReadiness(BuildNode.GetBuildNodeId());
	const FText RefusalText = WorldBatteryFireRules::GetFireResultText(Readiness);
	M_FireMissionButton->SetIsEnabled(Readiness == EWorldBatteryFireResult::Success);
	M_FireMissionButtonBox->SetToolTipText(RefusalText);
	M_FailText->SetText(RefusalText);
}

void UW_WorldBuildSitePanel::LayOutFireMissionFooter(const float ContentWidth)
{
	using namespace WorldBuildSitePanelLayout;
	SetCanvasGeometryIfChanged(M_FireMissionButtonBox,
	                           FVector2D(SidePadding + (ContentWidth - BuildButtonWidth) * 0.5f, FooterTop),
	                           BuildButtonSize);
	SetCanvasGeometryIfChanged(M_FailText, FVector2D(SidePadding, FireCaptionTop),
	                           FVector2D(ContentWidth, FireCaptionHeight));
}

void UW_WorldBuildSitePanel::RebuildCards(const TArray<EWorldCampaignBuilding>& Buildings,
                                          const EWorldBuildCardMode CardMode)
{
	using namespace WorldBuildSitePanelLayout;
	M_CardRow->ClearChildren();
	M_Cards.Reset();
	for (int32 CardIndex = 0; CardIndex < Buildings.Num(); ++CardIndex)
	{
		UW_WorldBuildCard* const Card = CreateWidget<UW_WorldBuildCard>(this, UW_WorldBuildCard::StaticClass());
		if (not IsValid(Card))
		{
			continue;
		}

		const FWorldMapResources Cost = GetIsValidBuildNodeManager()
			                                ? M_BuildNodeManager->GetBuildingCost(Buildings[CardIndex])
			                                : FWorldMapResources();
		Card->SetupBuildCard(this, *M_Look, Buildings[CardIndex], Cost, CardMode);
		const bool bIsLastCard = CardIndex == Buildings.Num() - 1;
		M_CardRow->AddChildToHorizontalBox(Card)->SetPadding(FMargin(0.f, 0.f, bIsLastCard ? 0.f : CardGap, 0.f));
		M_Cards.Add(Card);
	}
}

void UW_WorldBuildSitePanel::SetContentLayout(const EWorldBuildSitePanelContent Content, const int32 CardCount)
{
	using namespace WorldBuildSitePanelLayout;
	M_Content = Content;
	M_LaidOutCardCount = CardCount;
	M_LaidOutTitleWidth = GetTitleWidth();
	const float CardRowWidth = WorldBuildSiteHelpers::GetCardRowWidth(CardCount, CardWidth, CardGap);
	const float ContentWidth = GetContentWidth(Content, CardRowWidth, M_LaidOutTitleWidth);
	M_PlateSize = FVector2D(ContentWidth + SidePadding * 2.f, GetPlateHeight(Content)) + FVector2D(FrameInset * 2.f);
	if (UCanvasPanelSlot* const PlateSlot = GetCanvasSlot(M_Plate))
	{
		PlateSlot->SetSize(M_PlateSize);
	}
	LayOutPlateContent(ContentWidth, CardRowWidth);

	const bool bIsForSale = Content == EWorldBuildSitePanelContent::ForSale;
	const bool bHasFireMission = Content == EWorldBuildSitePanelContent::BuiltWithFireMission;
	SetShown(M_CardRow, Content != EWorldBuildSitePanelContent::Locked, ESlateVisibility::SelfHitTestInvisible);
	SetShown(M_StatusText, Content == EWorldBuildSitePanelContent::Locked, ESlateVisibility::HitTestInvisible);
	SetShown(M_FailText, bIsForSale || bHasFireMission, ESlateVisibility::HitTestInvisible);
	SetShown(M_BuildButtonBox, bIsForSale, ESlateVisibility::Visible);
	SetShown(M_FireMissionButtonBox, bHasFireMission, ESlateVisibility::Visible);
}

void UW_WorldBuildSitePanel::LayOutPlateContent(const float ContentWidth, const float CardRowWidth)
{
	using namespace WorldBuildSitePanelLayout;
	// A built site shows one card: title and card are centred on the small plate. Cards for sale start at the left
	// under the title.
	const bool bHasFireMission = M_Content == EWorldBuildSitePanelContent::BuiltWithFireMission;
	const bool bIsCentred = M_Content == EWorldBuildSitePanelContent::Built || bHasFireMission;
	if (IsValid(M_TitleText))
	{
		M_TitleText->SetJustification(bIsCentred ? ETextJustify::Center : ETextJustify::Left);
	}
	SetCanvasGeometryIfChanged(M_TitleText, FVector2D(SidePadding, TitleTop), FVector2D(ContentWidth, TitleHeight));

	const float CardRowLeft = SidePadding + (bIsCentred ? (ContentWidth - CardRowWidth) * 0.5f : 0.f);
	SetCanvasGeometryIfChanged(M_CardRow, FVector2D(CardRowLeft, CardsTop), FVector2D(CardRowWidth, CardHeight));
	SetCanvasGeometryIfChanged(M_StatusText, FVector2D(SidePadding, StatusTop),
	                           FVector2D(ContentWidth, StatusHeight));

	if (IsValid(M_FailText))
	{
		M_FailText->SetJustification(bHasFireMission ? ETextJustify::Center : ETextJustify::Left);
	}
	if (bHasFireMission)
	{
		LayOutFireMissionFooter(ContentWidth);
		return;
	}

	const float FailTextWidth = FMath::Max(0.f, ContentWidth - BuildButtonWidth - FailTextGapToButton);
	SetCanvasGeometryIfChanged(M_FailText, FVector2D(SidePadding, FooterTop),
	                           FVector2D(FailTextWidth, BuildButtonHeight));
	SetCanvasGeometryIfChanged(M_BuildButtonBox,
	                           FVector2D(SidePadding + ContentWidth - BuildButtonWidth, FooterTop), BuildButtonSize);
}

float UW_WorldBuildSitePanel::GetTitleWidth() const
{
	using namespace WorldBuildSitePanelLayout;
	// Zero until the title has been laid out once.
	const float LaidOutWidth = IsValid(M_TitleText) ? static_cast<float>(M_TitleText->GetDesiredSize().X) : 0.f;
	return LaidOutWidth > 0.f ? LaidOutWidth + TitleWidthSlack : TitleFallbackWidth;
}

void UW_WorldBuildSitePanel::RefreshLayoutIfTitleWidthChanged()
{
	if (FMath::IsNearlyEqual(GetTitleWidth(), M_LaidOutTitleWidth,
	                         WorldBuildSitePanelLayout::TitleWidthChangeTolerance))
	{
		return;
	}
	SetContentLayout(M_Content, M_LaidOutCardCount);
}

void UW_WorldBuildSitePanel::RefreshAffordability()
{
	if (not GetIsValidProgressManager() || not GetIsValidBuildNodeManager())
	{
		return;
	}

	for (UW_WorldBuildCard* const Card : M_Cards)
	{
		if (IsValid(Card))
		{
			Card->SetCanAfford(M_ProgressManager->GetCanAfford(M_BuildNodeManager->GetBuildingCost(Card->GetBuilding())));
		}
	}
}

void UW_WorldBuildSitePanel::ApplySelection()
{
	for (UW_WorldBuildCard* const Card : M_Cards)
	{
		if (IsValid(Card))
		{
			Card->SetIsSelected(Card->GetBuilding() == M_SelectedBuilding);
		}
	}

	if (IsValid(M_BuildLabel))
	{
		M_BuildLabel->SetText(FText::Format(
			NSLOCTEXT("WorldBuildSite", "BuildButton", "BUILD {0}"),
			WorldBuildSiteHelpers::GetBuildingDisplayName(M_SelectedBuilding).ToUpper()));
	}
}

EWorldCampaignBuilding UW_WorldBuildSitePanel::ChooseDefaultSelection(
	const TArray<EWorldCampaignBuilding>& Options) const
{
	if (Options.Contains(M_SelectedBuilding))
	{
		return M_SelectedBuilding;
	}

	// The first building the player can pay for, so Build works straight away when anything is affordable.
	const bool bCanCheckPrices = GetIsValidProgressManager() && GetIsValidBuildNodeManager();
	for (const EWorldCampaignBuilding Option : Options)
	{
		if (bCanCheckPrices && M_ProgressManager->GetCanAfford(M_BuildNodeManager->GetBuildingCost(Option)))
		{
			return Option;
		}
	}
	return Options.Num() > 0 ? Options[0] : EWorldCampaignBuilding::None;
}

int32 UW_WorldBuildSitePanel::GetUnlockRankOfNode(const AWorldBuildNode& BuildNode) const
{
	if (not GetIsValidBuildNodeManager())
	{
		return 0;
	}

	TArray<FWorldBuildNodeUnlockEntry> NodesOfCountry;
	const TArray<AWorldBuildNode*> AllBuildNodes = M_BuildNodeManager->GetBuildNodes();
	for (const AWorldBuildNode* const OtherNode : AllBuildNodes)
	{
		if (not IsValid(OtherNode) || OtherNode->GetCountryId() != BuildNode.GetCountryId())
		{
			continue;
		}

		FWorldBuildNodeUnlockEntry UnlockEntry;
		UnlockEntry.BuildNodeId = OtherNode->GetBuildNodeId();
		UnlockEntry.UnlockOrder = OtherNode->GetUnlockOrder();
		NodesOfCountry.Add(UnlockEntry);
	}

	const TArray<FName> NodesInUnlockOrder = WorldBuildNodeRules::GetNodesInUnlockOrder(NodesOfCountry);
	return FMath::Max(0, NodesInUnlockOrder.IndexOfByKey(BuildNode.GetBuildNodeId()));
}

void UW_WorldBuildSitePanel::PlacePlate(const FGeometry& MyGeometry)
{
	using namespace WorldBuildSitePanelLayout;
	const FVector2D ViewSize = MyGeometry.GetLocalSize();
	// Before the first layout the geometry is empty; the next tick places the plate.
	if (GetCanvasSlot(M_Plate) == nullptr || ViewSize.X <= 0.f || ViewSize.Y <= 0.f)
	{
		return;
	}

	const AWorldBuildNode* const BuildNode = M_OpenBuildNode.Get();
	if (bM_FollowsBuildNode && IsValid(BuildNode))
	{
		bM_HasAnchor = WorldMapScreenProjection::ProjectWorldToWidgetLocal(
			M_WorldPlayerController.Get(), this, MyGeometry, BuildNode->GetActorLocation(), M_AnchorPosition);
	}

	const FVector2D WantedTopLeft = bM_HasAnchor
		                                ? WorldBuildSiteHelpers::GetPanelTopLeft(
			                                M_AnchorPosition, M_PlateSize, ViewSize, GapToSite, ScreenMargin,
			                                WorldMapScreenProjection::CampaignTopBarHeight)
		                                : (ViewSize - M_PlateSize) * 0.5;
	// On whole units, so the plate's texts stay sharp while it follows the site.
	const FVector2D PlateTopLeft(FMath::RoundToDouble(WantedTopLeft.X), FMath::RoundToDouble(WantedTopLeft.Y));
	SetCanvasGeometryIfChanged(M_Plate, PlateTopLeft, M_PlateSize);
	const FVector2D BackingMargin(BackingInset);
	SetCanvasGeometryIfChanged(M_PlateBacking, PlateTopLeft + BackingMargin, M_PlateSize - BackingMargin * 2.);
	PlaceStem(PlateTopLeft);
}

void UW_WorldBuildSitePanel::PlaceStem(const FVector2D& PlateTopLeft)
{
	using namespace WorldBuildSitePanelLayout;
	UCanvasPanelSlot* const StemSlot = GetCanvasSlot(M_Stem);
	if (StemSlot == nullptr)
	{
		return;
	}

	// Hung from the rim you see, not from the plate's box, or the frame's shadow would leave a gap.
	const FVector2D FrameMargin(FrameShadowMargin);
	const FWorldBuildSiteStem Stem = bM_HasAnchor
		                                 ? WorldBuildSiteHelpers::GetStemPlacement(
			                                 M_AnchorPosition, PlateTopLeft + FrameMargin,
			                                 M_PlateSize - FrameMargin * 2., StemCornerInset, StemSiteClearance)
		                                 : FWorldBuildSiteStem();
	M_Stem->SetVisibility(Stem.bIsShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (not Stem.bIsShown)
	{
		return;
	}

	StemSlot->SetPosition(Stem.Start - FVector2D(StemWidth * 0.5f, 0.f));
	StemSlot->SetSize(FVector2D(StemWidth, Stem.Length));
	M_Stem->SetRenderTransformAngle(Stem.AngleDegrees);
}

bool UW_WorldBuildSitePanel::TryBuildSelected()
{
	const AWorldBuildNode* const BuildNode = M_OpenBuildNode.Get();
	if (not bM_IsOpen || not IsValid(BuildNode) || M_Content != EWorldBuildSitePanelContent::ForSale
		|| M_SelectedBuilding == EWorldCampaignBuilding::None || not GetIsValidBuildNodeManager())
	{
		return false;
	}

	FText FailReason;
	if (M_BuildNodeManager->TryBuildOnNode(BuildNode->GetBuildNodeId(), M_SelectedBuilding, FailReason))
	{
		ClosePanel();
		return true;
	}

	M_FailText->SetText(FailReason);
	return false;
}

void UW_WorldBuildSitePanel::HandleFireMissionClicked()
{
	AWorldBuildNode* const BuildNode = M_OpenBuildNode.Get();
	if (not bM_IsOpen || not IsValid(BuildNode) || not GetIsValidWorldPlayerController())
	{
		return;
	}

	// Picking closes this panel; a refusal keeps it open with the reason under the button.
	FText FailReason;
	if (not M_WorldPlayerController->BeginBatteryFireTargetPicking(BuildNode, FailReason) && IsValid(M_FailText))
	{
		M_FailText->SetText(FailReason);
	}
}

void UW_WorldBuildSitePanel::HandleBuildClicked()
{
	(void)TryBuildSelected();
}

void UW_WorldBuildSitePanel::HandleResourcesChanged(const FWorldMapResources& /*Resources*/)
{
	if (bM_IsOpen && M_Content == EWorldBuildSitePanelContent::ForSale)
	{
		RefreshAffordability();
	}
}

void UW_WorldBuildSitePanel::HandleBuildingPlaced(const FName BuildNodeId, const EWorldCampaignBuilding /*Building*/)
{
	const AWorldBuildNode* const BuildNode = M_OpenBuildNode.Get();
	if (bM_IsOpen && IsValid(BuildNode) && BuildNode->GetBuildNodeId() == BuildNodeId)
	{
		RefreshContent();
	}
}

void UW_WorldBuildSitePanel::HandleBuildNodeUnlocked(const FName BuildNodeId, const FName /*CountryId*/)
{
	const AWorldBuildNode* const BuildNode = M_OpenBuildNode.Get();
	if (bM_IsOpen && IsValid(BuildNode) && BuildNode->GetBuildNodeId() == BuildNodeId)
	{
		RefreshContent();
	}
}

FEventReply UW_WorldBuildSitePanel::HandlePlateMouseDown(FGeometry /*MyGeometry*/, const FPointerEvent& /*MouseEvent*/)
{
	return FEventReply(true);
}

bool UW_WorldBuildSitePanel::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_WorldPlayerController"), TEXT("UW_WorldBuildSitePanel::GetIsValidWorldPlayerController"), this);
	return false;
}

bool UW_WorldBuildSitePanel::GetIsValidBuildNodeManager() const
{
	if (M_BuildNodeManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_BuildNodeManager"), TEXT("UW_WorldBuildSitePanel::GetIsValidBuildNodeManager"), this);
	return false;
}

bool UW_WorldBuildSitePanel::GetIsValidProgressManager() const
{
	if (M_ProgressManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_ProgressManager"), TEXT("UW_WorldBuildSitePanel::GetIsValidProgressManager"), this);
	return false;
}
