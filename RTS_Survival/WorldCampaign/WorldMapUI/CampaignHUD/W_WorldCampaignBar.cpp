// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/W_WorldCampaignBar.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRules.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

// Layout of the top strip of UI sketch sheet 7, in UI units at the 1920x1080 reference.
namespace WorldCampaignBarLayout
{
	constexpr float StripHeight = 54.f;
	constexpr float EdgePadding = 20.f;
	// The same gap between every two framed cells.
	constexpr float CellGap = 10.f;
	// Every cell gives its value this height and centres it, so texts, icons, pips and the freeze track share one
	// centre line and all cells are equally tall.
	constexpr float CellValueRowHeight = 24.f;
	constexpr float CellLabelToValueGap = 2.f;
	constexpr float CellIconToValueGap = 6.f;
	constexpr float FreezeBarWidth = 210.f;
	constexpr float FreezeBarHeight = 12.f;
	// The empty track: a light rim around a dark trough, so it reads at 0 %.
	constexpr float FreezeTrackRimWidth = 1.f;
	constexpr float FreezeTrackInnerWidth = FreezeBarWidth - FreezeTrackRimWidth * 2.f;
	constexpr float FreezeTrackInnerHeight = FreezeBarHeight - FreezeTrackRimWidth * 2.f;
	constexpr float FreezeTickWidth = 2.f;
	constexpr int32 FreezeZOrderRim = 0;
	constexpr int32 FreezeZOrderTrough = 1;
	constexpr int32 FreezeZOrderFill = 2;
	constexpr int32 FreezeZOrderTicks = 3;
	constexpr float TierPipSize = 9.f;
	constexpr float TierPipGap = 6.f;
	constexpr int32 TierPipCount = 4;
	constexpr int32 LabelFontSize = 8;
	constexpr int32 LabelLetterSpacing = 200;
	constexpr int32 ValueFontSize = 15;
	constexpr int32 SecondaryFontSize = 10;
	constexpr int32 ButtonFontSize = 10;
	constexpr float RefreshIntervalSeconds = 0.25f;
	const FVector2D PageButtonSize(118.f, 34.f);
	// The shoulder board sits in the value row of the RANK cell, so it is as tall as that row.
	const FVector2D RankPlateSize(22.f, CellValueRowHeight);
	const FVector2D RankBarSize(14.f, 3.f);
	constexpr float RankBarGap = 4.f;
	constexpr float RankPlateToTextGap = 8.f;
	const FLinearColor RankPlateColor(FColor::FromHex(TEXT("2A3033")));
	const FLinearColor RankBarColor(FColor::FromHex(TEXT("D8C27A")));
	const FMargin CellFramePadding(12.f, 4.f, 14.f, 5.f);
	const FLinearColor TierPipOffColor(FColor::FromHex(TEXT("23282A")));
	const FLinearColor FreezeTrackRimColor(FColor::FromHex(TEXT("6B7377")));
	const FLinearColor FreezeTroughColor(FColor::FromHex(TEXT("07090A")));
	// Mid grey: readable on the dark trough and on the pale fill once it passes a tick.
	const FLinearColor FreezeTickColor(FColor::FromHex(TEXT("8E989C")));
}

void UW_WorldCampaignBar::InitCampaignBar(AWorldPlayerController* WorldPlayerController,
                                          const FWorldCampaignUILook* Look)
{
	M_WorldPlayerController = WorldPlayerController;
	M_Look = Look;
	if (M_Look == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_WorldCampaignBar::InitCampaignBar was given no look."));
		return;
	}

	BuildWidgetTree();
	RefreshFromCampaign();
}

void UW_WorldCampaignBar::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	M_SecondsUntilRefresh -= InDeltaTime;
	if (M_SecondsUntilRefresh > 0.f)
	{
		return;
	}

	M_SecondsUntilRefresh = WorldCampaignBarLayout::RefreshIntervalSeconds;
	RefreshFromCampaign();
}

void UW_WorldCampaignBar::BuildWidgetTree()
{
	using namespace WorldCampaignBarLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	UCanvasPanel* const Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	UBorder* const Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Strip->SetBrush(FWorldCampaignUILook::MakeFlatBrush(WorldCampaignUIColors::PlateFill));
	Strip->SetPadding(FMargin(EdgePadding, 0.f, EdgePadding * 0.5f, 0.f));
	UCanvasPanelSlot* const StripSlot = Root->AddChildToCanvas(Strip);
	StripSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
	StripSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, StripHeight));

	UImage* const BottomEdge = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	BottomEdge->SetBrush(FWorldCampaignUILook::MakeFlatBrush(WorldCampaignUIColors::PlateBorder));
	BottomEdge->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* const EdgeSlot = Root->AddChildToCanvas(BottomEdge);
	EdgeSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
	EdgeSlot->SetOffsets(FMargin(0.f, StripHeight, 0.f, 1.f));

	UOverlay* const Groups = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Strip->SetContent(Groups);

	UHorizontalBox* const LeftRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UOverlaySlot* const LeftSlot = Groups->AddChildToOverlay(LeftRow);
	LeftSlot->SetHorizontalAlignment(HAlign_Left);
	LeftSlot->SetVerticalAlignment(VAlign_Center);
	BuildWidgetTree_LeftGroup(*LeftRow);
	BuildWidgetTree_FreezeBar(*LeftRow);

	UHorizontalBox* const RightRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UOverlaySlot* const RightSlot = Groups->AddChildToOverlay(RightRow);
	RightSlot->SetHorizontalAlignment(HAlign_Right);
	RightSlot->SetVerticalAlignment(VAlign_Center);
	BuildWidgetTree_RightGroup(*RightRow);
}

void UW_WorldCampaignBar::BuildWidgetTree_LeftGroup(UHorizontalBox& Row)
{
	using namespace WorldCampaignBarLayout;
	AddCell(Row, NSLOCTEXT("WorldCampaignBar", "Turn", "TURN"), M_TurnText);
	AddGap(Row, CellGap);
	AddCell(Row, NSLOCTEXT("WorldCampaignBar", "Rank", "RANK"), M_RankText, nullptr, nullptr, BuildRankInsignia());
	AddGap(Row, CellGap);
	BuildWidgetTree_TierCell(Row);
	AddGap(Row, CellGap);
	AddCell(Row, NSLOCTEXT("WorldCampaignBar", "NextTier", "NEXT TIER"), M_NextTierText);
	AddGap(Row, CellGap);
}

UWidget* UW_WorldCampaignBar::BuildRankInsignia()
{
	using namespace WorldCampaignBarLayout;
	// A small shoulder board: two bars on a dark plate.
	USizeBox* const RankBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RankBox->SetWidthOverride(RankPlateSize.X);
	RankBox->SetHeightOverride(RankPlateSize.Y);
	RankBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanel* const RankCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	RankBox->SetContent(RankCanvas);
	DeckBuilderWidgets::PlaceOnCanvas(*RankCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(RankPlateColor)),
	                                  FVector2D::ZeroVector, RankPlateSize);
	const float RankBarLeft = (RankPlateSize.X - RankBarSize.X) * 0.5f;
	const float UpperBarTop = (RankPlateSize.Y - RankBarSize.Y * 2.f - RankBarGap) * 0.5f;
	const float LowerBarTop = UpperBarTop + RankBarSize.Y + RankBarGap;
	DeckBuilderWidgets::PlaceOnCanvas(*RankCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(RankBarColor)),
	                                  FVector2D(RankBarLeft, UpperBarTop), RankBarSize);
	DeckBuilderWidgets::PlaceOnCanvas(*RankCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(RankBarColor)),
	                                  FVector2D(RankBarLeft, LowerBarTop), RankBarSize);
	return RankBox;
}

void UW_WorldCampaignBar::BuildWidgetTree_TierCell(UHorizontalBox& Row)
{
	using namespace WorldCampaignBarLayout;
	// Ability tier: four pips and the tier number.
	UHorizontalBox* const PipRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 PipIndex = 0; PipIndex < TierPipCount; PipIndex++)
	{
		UImage* const Pip = DeckBuilderWidgets::MakeImage(
			*WidgetTree, FWorldCampaignUILook::MakeFlatBrush(FLinearColor::White));
		Pip->SetRenderTransformAngle(45.f);
		USizeBox* const PipBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PipBox->SetWidthOverride(TierPipSize);
		PipBox->SetHeightOverride(TierPipSize);
		PipBox->SetContent(Pip);
		UHorizontalBoxSlot* const PipSlot = PipRow->AddChildToHorizontalBox(PipBox);
		PipSlot->SetVerticalAlignment(VAlign_Center);
		PipSlot->SetPadding(FMargin(2.f, 0.f, TierPipGap, 0.f));
		M_TierPips.Add(Pip);
	}
	M_TierText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, SecondaryFontSize),
		FSlateColor(WorldCampaignUIColors::Accent));
	UHorizontalBoxSlot* const TierTextSlot = PipRow->AddChildToHorizontalBox(M_TierText);
	TierTextSlot->SetVerticalAlignment(VAlign_Center);
	TierTextSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
	AddFramedCell(Row, MakeCellLabel(NSLOCTEXT("WorldCampaignBar", "Tier", "ABILITY TIER"),
	                                 M_Look->Deck.Style.MutedTextColor), PipRow);
}

void UW_WorldCampaignBar::BuildWidgetTree_FreezeBar(UHorizontalBox& Row)
{
	using namespace WorldCampaignBarLayout;
	M_FreezeLabel = MakeCellLabel(FText::GetEmpty(), FSlateColor(WorldCampaignUIColors::Freeze));

	USizeBox* const BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(FreezeBarWidth);
	BarBox->SetHeightOverride(FreezeBarHeight);
	BarBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_FreezeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	BarBox->SetContent(M_FreezeCanvas);
	// The empty track: a light 1 px rim with a dark trough set into it.
	const FVector2D TrackInnerPosition(FreezeTrackRimWidth, FreezeTrackRimWidth);
	DeckBuilderWidgets::PlaceOnCanvas(*M_FreezeCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(FreezeTrackRimColor)),
	                                  FVector2D::ZeroVector, FVector2D(FreezeBarWidth, FreezeBarHeight),
	                                  FreezeZOrderRim);
	DeckBuilderWidgets::PlaceOnCanvas(*M_FreezeCanvas, DeckBuilderWidgets::MakeImage(
		                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(FreezeTroughColor)),
	                                  TrackInnerPosition, FVector2D(FreezeTrackInnerWidth, FreezeTrackInnerHeight),
	                                  FreezeZOrderTrough);
	M_FreezeFill = DeckBuilderWidgets::MakeImage(
		*WidgetTree, FWorldCampaignUILook::MakeFlatBrush(WorldCampaignUIColors::Freeze));
	DeckBuilderWidgets::PlaceOnCanvas(*M_FreezeCanvas, M_FreezeFill, TrackInnerPosition,
	                                  FVector2D(0.f, FreezeTrackInnerHeight), FreezeZOrderFill);
	BuildWidgetTree_FreezeTicks();
	AddFramedCell(Row, M_FreezeLabel, BarBox);
}

void UW_WorldCampaignBar::BuildWidgetTree_FreezeTicks()
{
	using namespace WorldCampaignBarLayout;
	// One tick where each stage of the freeze ends.
	const FWorldCampaignRuleNumbers& Numbers = UWorldCampaignRulesSettings::GetNumbers();
	const int32 FinalStage = FMath::Max(1, Numbers.FreezeFinalStage);
	int32 TotalPoints = 0;
	for (int32 StageIndex = 0; StageIndex < FinalStage; StageIndex++)
	{
		TotalPoints += WorldCampaignRules::GetFreezePointsNeededForStage(Numbers, StageIndex);
	}
	int32 PointsSoFar = 0;
	for (int32 StageIndex = 0; StageIndex < FinalStage - 1 && TotalPoints > 0; StageIndex++)
	{
		PointsSoFar += WorldCampaignRules::GetFreezePointsNeededForStage(Numbers, StageIndex);
		const float StageEndFraction = static_cast<float>(PointsSoFar) / static_cast<float>(TotalPoints);
		const float TickLeft = FreezeTrackRimWidth + FreezeTrackInnerWidth * StageEndFraction - FreezeTickWidth * 0.5f;
		DeckBuilderWidgets::PlaceOnCanvas(*M_FreezeCanvas, DeckBuilderWidgets::MakeImage(
			                                  *WidgetTree, FWorldCampaignUILook::MakeFlatBrush(FreezeTickColor)),
		                                  FVector2D(TickLeft, 0.f), FVector2D(FreezeTickWidth, FreezeBarHeight),
		                                  FreezeZOrderTicks);
	}
}

void UW_WorldCampaignBar::BuildWidgetTree_RightGroup(UHorizontalBox& Row)
{
	using namespace WorldCampaignBarLayout;
	AddCell(Row, NSLOCTEXT("WorldCampaignBar", "Radixite", "RADIXITE"), M_RadixiteText, &M_Look->RadixiteIcon,
	        &M_RadixiteIncomeText);
	AddGap(Row, CellGap);
	AddCell(Row, NSLOCTEXT("WorldCampaignBar", "Blueprints", "BLUEPRINTS"), M_BlueprintText, &M_Look->BlueprintIcon,
	        &M_BlueprintIncomeText);
	AddGap(Row, CellGap);
	AddPageButton(Row, NSLOCTEXT("WorldCampaignBar", "Army", "ARMY"), M_ArmyButton);
	AddPageButton(Row, NSLOCTEXT("WorldCampaignBar", "Tech", "TECHNOLOGY"), M_TechButton);
	AddPageButton(Row, NSLOCTEXT("WorldCampaignBar", "Archive", "ARCHIVE"), M_ArchiveButton);
	if (IsValid(M_ArmyButton))
	{
		M_ArmyButton->OnClicked.AddDynamic(this, &UW_WorldCampaignBar::HandleArmyClicked);
	}
	if (IsValid(M_TechButton))
	{
		M_TechButton->OnClicked.AddDynamic(this, &UW_WorldCampaignBar::HandleTechClicked);
	}
	if (IsValid(M_ArchiveButton))
	{
		M_ArchiveButton->OnClicked.AddDynamic(this, &UW_WorldCampaignBar::HandleArchiveClicked);
	}
}

void UW_WorldCampaignBar::AddCell(UHorizontalBox& Row, const FText& Label, TObjectPtr<UTextBlock>& OutValue,
                                  const FSlateBrush* IconBrush, TObjectPtr<UTextBlock>* OutSecondary,
                                  UWidget* LeadingWidget)
{
	using namespace WorldCampaignBarLayout;
	const FDeckBuilderStyle& Style = M_Look->Deck.Style;
	UHorizontalBox* const ValueRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (IsValid(LeadingWidget))
	{
		UHorizontalBoxSlot* const LeadingSlot = ValueRow->AddChildToHorizontalBox(LeadingWidget);
		LeadingSlot->SetVerticalAlignment(VAlign_Center);
		LeadingSlot->SetPadding(FMargin(0.f, 0.f, RankPlateToTextGap, 0.f));
	}
	if (IconBrush != nullptr)
	{
		UHorizontalBoxSlot* const IconSlot = ValueRow->AddChildToHorizontalBox(
			DeckBuilderWidgets::MakeImage(*WidgetTree, *IconBrush));
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, CellIconToValueGap, 0.f));
	}
	OutValue = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(),
	                                        DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, ValueFontSize, 40),
	                                        Style.TextColor);
	UHorizontalBoxSlot* const ValueSlot = ValueRow->AddChildToHorizontalBox(OutValue);
	ValueSlot->SetVerticalAlignment(VAlign_Center);
	if (OutSecondary != nullptr)
	{
		*OutSecondary = DeckBuilderWidgets::MakeText(
			*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, SecondaryFontSize),
			Style.GoodColor);
		UHorizontalBoxSlot* const SecondarySlot = ValueRow->AddChildToHorizontalBox(*OutSecondary);
		SecondarySlot->SetVerticalAlignment(VAlign_Bottom);
		SecondarySlot->SetPadding(FMargin(7.f, 0.f, 0.f, 2.f));
	}

	AddFramedCell(Row, MakeCellLabel(Label, Style.MutedTextColor), ValueRow);
}

UTextBlock* UW_WorldCampaignBar::MakeCellLabel(const FText& Label, const FSlateColor& Color)
{
	using namespace WorldCampaignBarLayout;
	return DeckBuilderWidgets::MakeText(
		*WidgetTree, Label, DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, LabelFontSize, LabelLetterSpacing),
		Color);
}

void UW_WorldCampaignBar::AddFramedCell(UHorizontalBox& Row, UTextBlock* Label, UWidget* ValueContent)
{
	using namespace WorldCampaignBarLayout;
	UVerticalBox* const Cell = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Cell->AddChildToVerticalBox(Label);

	// The same value height in every cell keeps the cells equally tall and their values on one centre line.
	USizeBox* const ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ValueBox->SetHeightOverride(CellValueRowHeight);
	if (USizeBoxSlot* const ValueContentSlot = Cast<USizeBoxSlot>(ValueBox->SetContent(ValueContent)))
	{
		ValueContentSlot->SetHorizontalAlignment(HAlign_Left);
		ValueContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	UVerticalBoxSlot* const ValueBoxSlot = Cell->AddChildToVerticalBox(ValueBox);
	ValueBoxSlot->SetPadding(FMargin(0.f, CellLabelToValueGap, 0.f, 0.f));

	// Each value sits in its own bolted metal cell (sheet 14A).
	UBorder* const CellFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	CellFrame->SetBrush(M_Look->MetalCell);
	CellFrame->SetPadding(CellFramePadding);
	CellFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	CellFrame->SetContent(Cell);
	UHorizontalBoxSlot* const CellSlot = Row.AddChildToHorizontalBox(CellFrame);
	CellSlot->SetVerticalAlignment(VAlign_Center);
}

void UW_WorldCampaignBar::AddGap(UHorizontalBox& Row, const float Width) const
{
	USpacer* const Gap = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Gap->SetSize(FVector2D(Width, 1.f));
	Row.AddChildToHorizontalBox(Gap);
}

void UW_WorldCampaignBar::AddPageButton(UHorizontalBox& Row, const FText& Label, TObjectPtr<UButton>& OutButton)
{
	using namespace WorldCampaignBarLayout;
	TObjectPtr<UTextBlock> ButtonLabel;
	UWidget* const ButtonBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, Label, M_Look->Deck.Tab, DeckBuilderWidgets::MakeFont(M_Look->Deck.HeadingFont, ButtonFontSize, 80),
		M_Look->Deck.Style.TextColor, PageButtonSize, OutButton, ButtonLabel);
	UHorizontalBoxSlot* const ButtonSlot = Row.AddChildToHorizontalBox(ButtonBox);
	ButtonSlot->SetVerticalAlignment(VAlign_Center);
	ButtonSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
}

void UW_WorldCampaignBar::RefreshFromCampaign()
{
	if (not IsValid(M_TurnText) || not GetIsValidWorldPlayerController())
	{
		return;
	}

	const UWorldStateAndSaveManager* SaveManager = M_WorldPlayerController->GetWorldStateAndSaveManager();
	if (IsValid(SaveManager))
	{
		M_TurnText->SetText(FText::FromString(FString::Printf(TEXT("%02d"), SaveManager->GetCurrentTurn())));
	}

	RefreshRankAndTier();
	RefreshFreeze();
	RefreshResources();
}

void UW_WorldCampaignBar::RefreshRankAndTier()
{
	const UWorldCampaignProgressManager* ProgressManager = M_WorldPlayerController->GetWorldCampaignProgressManager();
	if (not IsValid(ProgressManager))
	{
		return;
	}

	const UEnum* RankEnum = StaticEnum<ERTSPlayerRank>();
	if (RankEnum != nullptr)
	{
		M_RankText->SetText(RankEnum->GetDisplayNameTextByValue(static_cast<int64>(ProgressManager->GetPlayerRank()))
		                            .ToUpper());
	}

	const int32 TierIndex = static_cast<int32>(ProgressManager->GetGlobalAbilityTierCap());
	for (int32 PipIndex = 0; PipIndex < M_TierPips.Num(); PipIndex++)
	{
		const bool bIsLit = PipIndex <= TierIndex;
		M_TierPips[PipIndex]->SetColorAndOpacity(bIsLit
			                                         ? WorldCampaignUIColors::Accent
			                                         : WorldCampaignBarLayout::TierPipOffColor);
	}
	static const TCHAR* const TierNumerals[] = {TEXT("I"), TEXT("II"), TEXT("III"), TEXT("IV")};
	M_TierText->SetText(FText::FromString(TierNumerals[FMath::Clamp(TierIndex, 0, 3)]));

	const FWorldCampaignRuleNumbers& Numbers = UWorldCampaignRulesSettings::GetNumbers();
	const int32 MissionsCompleted = ProgressManager->GetMissionsCompleted();
	int32 MissionsToNextTier = INDEX_NONE;
	for (const int32 MissionsNeeded : Numbers.MissionsNeededPerTierAboveOne)
	{
		if (MissionsNeeded > MissionsCompleted)
		{
			MissionsToNextTier = MissionsNeeded - MissionsCompleted;
			break;
		}
	}

	const bool bIsHighestTier = MissionsToNextTier == INDEX_NONE || TierIndex >= 3;
	M_NextTierText->SetText(bIsHighestTier
		                        ? NSLOCTEXT("WorldCampaignBar", "HighestTier", "REACHED")
		                        : FText::Format(NSLOCTEXT("WorldCampaignBar", "MissionsToTier",
		                                                  "{0} {0}|plural(one=mission,other=missions)"),
		                                        MissionsToNextTier));
}

void UW_WorldCampaignBar::RefreshFreeze()
{
	using namespace WorldCampaignBarLayout;
	const UWorldCampaignProgressManager* ProgressManager = M_WorldPlayerController->GetWorldCampaignProgressManager();
	if (not IsValid(ProgressManager) || not IsValid(M_FreezeFill))
	{
		return;
	}

	const FWorldCampaignRuleNumbers& Numbers = UWorldCampaignRulesSettings::GetNumbers();
	const FWorldEnemyTurnState& EnemyTurnState = ProgressManager->GetProgressState().EnemyTurn;
	const int32 FinalStage = FMath::Max(1, Numbers.FreezeFinalStage);
	int32 TotalPoints = 0;
	int32 PaidPoints = 0;
	for (int32 StageIndex = 0; StageIndex < FinalStage; StageIndex++)
	{
		const int32 StagePoints = WorldCampaignRules::GetFreezePointsNeededForStage(Numbers, StageIndex);
		TotalPoints += StagePoints;
		if (StageIndex < EnemyTurnState.FreezeStage)
		{
			PaidPoints += StagePoints;
		}
	}
	PaidPoints += EnemyTurnState.FreezeStage < FinalStage ? EnemyTurnState.FreezeProgressPoints : 0;
	const float Fraction = TotalPoints > 0
		                       ? FMath::Clamp(static_cast<float>(PaidPoints) / static_cast<float>(TotalPoints), 0.f, 1.f)
		                       : 0.f;

	M_FreezeLabel->SetText(FText::Format(NSLOCTEXT("WorldCampaignBar", "Freeze", "THE FREEZE   {0} %"),
	                                     FMath::RoundToInt(Fraction * 100.f)));
	if (UCanvasPanelSlot* const FillSlot = Cast<UCanvasPanelSlot>(M_FreezeFill->Slot))
	{
		FillSlot->SetSize(FVector2D(FreezeTrackInnerWidth * Fraction, FreezeTrackInnerHeight));
	}
}

void UW_WorldCampaignBar::RefreshResources()
{
	const UWorldCampaignProgressManager* ProgressManager = M_WorldPlayerController->GetWorldCampaignProgressManager();
	if (not IsValid(ProgressManager))
	{
		return;
	}

	const FWorldMapResources Resources = ProgressManager->GetResources();
	const FWorldCampaignRuleNumbers& Numbers = UWorldCampaignRulesSettings::GetNumbers();
	const int32 DepotCount = ProgressManager->GetBuildingCount(EWorldCampaignBuilding::SupplyDepot);
	M_RadixiteText->SetText(FText::AsNumber(Resources.Radixite));
	M_BlueprintText->SetText(FText::AsNumber(Resources.Blueprints));

	const int32 RadixiteIncome = DepotCount * Numbers.SupplyDepotIncomePerTurn.Radixite;
	const int32 BlueprintIncome = DepotCount * Numbers.SupplyDepotIncomePerTurn.Blueprints;
	M_RadixiteIncomeText->SetText(RadixiteIncome > 0
		                              ? FText::FromString(FString::Printf(TEXT("+%d"), RadixiteIncome))
		                              : FText::GetEmpty());
	M_BlueprintIncomeText->SetText(BlueprintIncome > 0
		                               ? FText::FromString(FString::Printf(TEXT("+%d"), BlueprintIncome))
		                               : FText::GetEmpty());
}

void UW_WorldCampaignBar::HandleArmyClicked()
{
	M_OnNavigateRequested.Broadcast(EWorldUIFocusState::CardMenu);
}

void UW_WorldCampaignBar::HandleTechClicked()
{
	M_OnNavigateRequested.Broadcast(EWorldUIFocusState::TechTree);
}

void UW_WorldCampaignBar::HandleArchiveClicked()
{
	M_OnNavigateRequested.Broadcast(EWorldUIFocusState::Archive);
}

bool UW_WorldCampaignBar::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_WorldPlayerController"), TEXT("GetIsValidWorldPlayerController"), this);
	return false;
}
