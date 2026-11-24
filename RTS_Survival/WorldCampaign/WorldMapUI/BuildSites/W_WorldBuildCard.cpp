// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/W_WorldBuildCard.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignRulesSettings.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildingSettings.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/W_WorldBuildSitePanel.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/BuildSites/WorldBuildSiteHelpers.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"

// Layout of one build card (.bcard of the sketches, 132 x 192 px there), in UI units at the 1920x1080 reference.
namespace WorldBuildCardLayout
{
	constexpr float CardWidth = 154.f;
	constexpr float CardHeight = 196.f;
	const FVector2D CardSize(CardWidth, CardHeight);
	// Texts, the divider and the footer keep this far from the card's left and right edges.
	constexpr float SidePadding = 12.f;
	constexpr float ContentWidth = CardWidth - SidePadding * 2.f;

	// The picture well: one square tile, centred, the same on every card.
	constexpr float WellEdge = 64.f;
	constexpr float WellTop = 14.f;
	const FVector2D WellPosition((CardWidth - WellEdge) * 0.5f, WellTop);
	const FVector2D WellSize(WellEdge, WellEdge);

	// Name and effect flow in one column, so a name that needs two lines pushes the effect down instead of
	// running over it.
	const FVector2D TextColumnPosition(SidePadding, 86.f);
	const FVector2D TextColumnSize(ContentWidth, 66.f);
	constexpr float EffectTopGap = 3.f;

	const FVector2D DividerPosition(SidePadding, 157.f);
	const FVector2D DividerSize(ContentWidth, 1.f);

	// The price and the built mark share this box and are centred in it, so every card's footer sits on one line
	// whether it shows one price, two prices or the built mark.
	const FVector2D FooterPosition(SidePadding, 163.f);
	const FVector2D FooterSize(ContentWidth, 22.f);
	const FVector2D PriceIconSize(18.f, 18.f);
	constexpr float PriceTextGap = 4.f;
	constexpr float PriceGroupGap = 10.f;

	constexpr int32 CodeFontSize = 22;
	constexpr int32 NameFontSize = 10;
	constexpr int32 NameLetterSpacing = 40;
	constexpr int32 EffectFontSize = 8;
	constexpr int32 PriceFontSize = 10;
	constexpr int32 BuiltFontSize = 9;
	constexpr int32 BuiltLetterSpacing = 250;

	constexpr float CornerRadius = 2.f;
	constexpr float OutlineWidth = 1.f;
	constexpr float SelectedOutlineWidth = 2.f;
	const FLinearColor CardFill(FColor::FromHex(TEXT("151B1E")));
	// The built mark is a lit plate in the state colour, not a text where the price was.
	constexpr float BuiltPlateFillOpacity = 0.16f;
	// The sketch's ".bcard.no" opacity for a building the player cannot pay for.
	constexpr float UnaffordableOpacity = 0.55f;

	constexpr int32 BackgroundZOrder = 0;
	constexpr int32 ContentZOrder = 1;

	UTextBlock* MakeCenteredText(UWidgetTree& Tree, const FSlateFontInfo& Font, const FSlateColor& Color)
	{
		UTextBlock* const TextBlock = DeckBuilderWidgets::MakeText(Tree, FText::GetEmpty(), Font, Color);
		TextBlock->SetJustification(ETextJustify::Center);
		return TextBlock;
	}

	/** A see-through border that centres its content both ways in the box it is placed in; never takes the mouse. */
	UBorder* MakeCenteringHolder(UWidgetTree& Tree)
	{
		UBorder* const Holder = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		Holder->SetBrush(FSlateColorBrush(FLinearColor::Transparent));
		Holder->SetPadding(FMargin(0.f));
		Holder->SetHorizontalAlignment(HAlign_Center);
		Holder->SetVerticalAlignment(VAlign_Center);
		Holder->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Holder;
	}
}

void UW_WorldBuildCard::SetupBuildCard(UW_WorldBuildSitePanel* Panel, const FWorldCampaignUILook& Look,
                                       const EWorldCampaignBuilding Building, const FWorldMapResources& Cost,
                                       const EWorldBuildCardMode Mode)
{
	M_Panel = Panel;
	M_Building = Building;
	M_Mode = Mode;
	BuildWidgetTree(Look);
	if (not IsValid(M_CodeText) || not IsValid(M_EffectText))
	{
		return;
	}

	const FText DisplayName = WorldBuildSiteHelpers::GetBuildingDisplayName(Building);
	M_CodeText->SetText(WorldBuildSiteHelpers::GetBuildingCode(Building, DisplayName));
	M_CodeText->SetColorAndOpacity(FSlateColor(WorldBuildSiteHelpers::GetBuildingCodeColor(Building)));
	M_NameText->SetText(DisplayName.ToUpper());

	const FWorldBuildingEffectDefinition* const Effect = UWorldBuildingSettings::FindBuildingEffect(Building);
	M_EffectText->SetText(Effect != nullptr
		                      ? WorldBuildSiteHelpers::GetBuildingEffectText(
			                      *Effect, UWorldCampaignRulesSettings::GetNumbers().SupplyDepotIncomePerTurn,
			                      WorldBuildSiteHelpers::GatherSupportRadii())
		                      : FText::GetEmpty());

	FillPriceRow(Cost);
	SetIsSelected(false);
	SetCanAfford(true);
}

void UW_WorldBuildCard::SetIsSelected(const bool bIsSelected)
{
	if (not IsValid(M_Background))
	{
		return;
	}

	// A built card cannot be selected; its outline shows the state instead.
	if (M_Mode == EWorldBuildCardMode::Built)
	{
		M_Background->SetBrush(M_BuiltBackgroundBrush);
		return;
	}
	M_Background->SetBrush(bIsSelected ? M_SelectedBackgroundBrush : M_NormalBackgroundBrush);
}

void UW_WorldBuildCard::SetCanAfford(const bool bCanAfford)
{
	SetRenderOpacity(bCanAfford || M_Mode == EWorldBuildCardMode::Built ? 1.f
		                 : WorldBuildCardLayout::UnaffordableOpacity);
}

FReply UW_WorldBuildCard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || M_Mode != EWorldBuildCardMode::ForSale
		|| not GetIsValidPanel())
	{
		// Handled anyway: a click on the card must never fall through to the map under the panel.
		return FReply::Handled();
	}

	M_Panel->HandleCardClicked(M_Building);
	return FReply::Handled();
}

void UW_WorldBuildCard::BuildWidgetTree(const FWorldCampaignUILook& Look)
{
	using namespace WorldBuildCardLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
	USizeBox* const RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RootBox->SetWidthOverride(CardSize.X);
	RootBox->SetHeightOverride(CardSize.Y);
	WidgetTree->RootWidget = RootBox;
	M_Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	RootBox->SetContent(M_Canvas);

	const FLinearColor AccentColor = WorldCampaignUIColors::Accent;
	M_NormalBackgroundBrush = FSlateRoundedBoxBrush(CardFill, CornerRadius, WorldCampaignUIColors::PlateBorder,
	                                                OutlineWidth);
	M_SelectedBackgroundBrush = FSlateRoundedBoxBrush(CardFill, CornerRadius, AccentColor, SelectedOutlineWidth);
	M_BuiltBackgroundBrush = FSlateRoundedBoxBrush(CardFill, CornerRadius, WorldCampaignUIColors::Good, OutlineWidth);
	// Visible, so the card body takes the click.
	M_Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_Background->SetBrush(M_NormalBackgroundBrush);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_Background, FVector2D::ZeroVector, CardSize, BackgroundZOrder);

	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, DeckBuilderWidgets::MakeImage(*WidgetTree, Look.Deck.IconWell),
	                                  WellPosition, WellSize, ContentZOrder);
	UBorder* const CodeHolder = MakeCenteringHolder(*WidgetTree);
	M_CodeText = MakeCenteredText(*WidgetTree, DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, CodeFontSize),
	                              FSlateColor(WorldCampaignUIColors::Cream));
	CodeHolder->SetContent(M_CodeText);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, CodeHolder, WellPosition, WellSize, ContentZOrder);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TextColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
	TextColumn->SetClipping(EWidgetClipping::ClipToBounds);
	M_NameText = MakeCenteredText(
		*WidgetTree, DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, NameFontSize, NameLetterSpacing),
		Look.Deck.Style.TextColor);
	M_NameText->SetAutoWrapText(true);
	TextColumn->AddChildToVerticalBox(M_NameText);
	M_EffectText = MakeCenteredText(*WidgetTree, DeckBuilderWidgets::MakeFont(Look.Deck.BodyFont, EffectFontSize),
	                                Look.Deck.Style.DescriptionColor);
	M_EffectText->SetAutoWrapText(true);
	TextColumn->AddChildToVerticalBox(M_EffectText)->SetPadding(FMargin(0.f, EffectTopGap, 0.f, 0.f));
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, TextColumn, TextColumnPosition, TextColumnSize, ContentZOrder);

	DeckBuilderWidgets::PlaceOnCanvas(
		*M_Canvas, DeckBuilderWidgets::MakeImage(*WidgetTree, FWorldCampaignUILook::MakeFlatBrush(
			                                         WorldCampaignUIColors::PlateBorder)),
		DividerPosition, DividerSize, ContentZOrder);

	BuildWidgetTree_PriceRow(Look);
	BuildWidgetTree_BuiltPlate(Look);
}

void UW_WorldBuildCard::BuildWidgetTree_PriceRow(const FWorldCampaignUILook& Look)
{
	using namespace WorldBuildCardLayout;
	const FSlateFontInfo PriceFont = DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, PriceFontSize);
	M_PriceRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	M_PriceRow->SetVisibility(ESlateVisibility::HitTestInvisible);

	USizeBox* const RadixiteIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RadixiteIconBox->SetWidthOverride(PriceIconSize.X);
	RadixiteIconBox->SetHeightOverride(PriceIconSize.Y);
	RadixiteIconBox->SetContent(DeckBuilderWidgets::MakeImage(
		*WidgetTree, DeckBuilderWidgets::MakePictureBrush(Look.RadixiteIcon, PriceIconSize)));
	M_PriceRow->AddChildToHorizontalBox(RadixiteIconBox)->SetVerticalAlignment(VAlign_Center);
	M_RadixiteText = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(), PriceFont, Look.Deck.Style.TextColor);
	UHorizontalBoxSlot* const RadixiteTextSlot = M_PriceRow->AddChildToHorizontalBox(M_RadixiteText);
	RadixiteTextSlot->SetVerticalAlignment(VAlign_Center);
	RadixiteTextSlot->SetPadding(FMargin(PriceTextGap, 0.f, 0.f, 0.f));

	USizeBox* const BlueprintIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BlueprintIconBox->SetWidthOverride(PriceIconSize.X);
	BlueprintIconBox->SetHeightOverride(PriceIconSize.Y);
	M_BlueprintIcon = DeckBuilderWidgets::MakeImage(
		*WidgetTree, DeckBuilderWidgets::MakePictureBrush(Look.BlueprintIcon, PriceIconSize));
	BlueprintIconBox->SetContent(M_BlueprintIcon);
	UHorizontalBoxSlot* const BlueprintIconSlot = M_PriceRow->AddChildToHorizontalBox(BlueprintIconBox);
	BlueprintIconSlot->SetVerticalAlignment(VAlign_Center);
	BlueprintIconSlot->SetPadding(FMargin(PriceGroupGap, 0.f, 0.f, 0.f));
	M_BlueprintText = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(), PriceFont,
	                                               Look.Deck.Style.TextColor);
	UHorizontalBoxSlot* const BlueprintTextSlot = M_PriceRow->AddChildToHorizontalBox(M_BlueprintText);
	BlueprintTextSlot->SetVerticalAlignment(VAlign_Center);
	BlueprintTextSlot->SetPadding(FMargin(PriceTextGap, 0.f, 0.f, 0.f));

	UBorder* const PriceHolder = MakeCenteringHolder(*WidgetTree);
	PriceHolder->SetContent(M_PriceRow);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, PriceHolder, FooterPosition, FooterSize, ContentZOrder);
}

void UW_WorldBuildCard::BuildWidgetTree_BuiltPlate(const FWorldCampaignUILook& Look)
{
	using namespace WorldBuildCardLayout;
	const FLinearColor StateColor = WorldCampaignUIColors::Good;
	M_BuiltPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_BuiltPlate->SetBrush(FSlateRoundedBoxBrush(StateColor.CopyWithNewOpacity(BuiltPlateFillOpacity), CornerRadius,
	                                             StateColor, OutlineWidth));
	M_BuiltPlate->SetPadding(FMargin(0.f));
	M_BuiltPlate->SetHorizontalAlignment(HAlign_Center);
	M_BuiltPlate->SetVerticalAlignment(VAlign_Center);
	M_BuiltPlate->SetVisibility(ESlateVisibility::Collapsed);
	M_BuiltPlate->SetContent(DeckBuilderWidgets::MakeText(
		*WidgetTree, NSLOCTEXT("WorldBuildSite", "BuiltMark", "BUILT"),
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, BuiltFontSize, BuiltLetterSpacing),
		FSlateColor(StateColor)));
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_BuiltPlate, FooterPosition, FooterSize, ContentZOrder);
}

void UW_WorldBuildCard::FillPriceRow(const FWorldMapResources& Cost)
{
	const bool bIsBuilt = M_Mode == EWorldBuildCardMode::Built;
	M_PriceRow->SetVisibility(bIsBuilt ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	M_BuiltPlate->SetVisibility(bIsBuilt ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (bIsBuilt)
	{
		return;
	}

	M_RadixiteText->SetText(FText::AsNumber(Cost.Radixite));
	const ESlateVisibility BlueprintVisibility = Cost.Blueprints > 0 ? ESlateVisibility::HitTestInvisible
		                                             : ESlateVisibility::Collapsed;
	// The icon's size box is what takes the room in the row, so collapse that rather than the image.
	if (UWidget* const BlueprintIconBox = M_BlueprintIcon->GetParent())
	{
		BlueprintIconBox->SetVisibility(BlueprintVisibility);
	}
	M_BlueprintText->SetVisibility(BlueprintVisibility);
	M_BlueprintText->SetText(FText::AsNumber(Cost.Blueprints));
}

bool UW_WorldBuildCard::GetIsValidPanel() const
{
	if (M_Panel.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_Panel"), TEXT("UW_WorldBuildCard::GetIsValidPanel"), this);
	return false;
}
