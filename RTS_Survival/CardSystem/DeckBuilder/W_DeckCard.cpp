// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DeckCard.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_DeckBuilderMenu.h"
#include "W_DeckVariantCard.h"

// Layout of the unit card (240 x 315 units), the anatomy the Blueprint card always had.
namespace DeckCardLayout
{
	// The part of the frame texture that is card; the rest is its soft shadow.
	const FVector2D BodyPosition(11.f, 6.f);
	const FVector2D BodySize(215.f, 303.f);
	const FVector2D PicturePosition(24.f, 27.f);
	// Technology cards have no building picture, so their picture sits in the middle.
	const FVector2D TechnologyPicturePosition(86.f, 27.f);
	const FVector2D PictureSize(68.f, 68.f);
	const FVector2D BuildingWellPosition(142.f, 35.f);
	const FVector2D BuildingWellSize(52.f, 52.f);
	// The building portrait fills the well up to its rim.
	const FVector2D BuildingIconPosition(145.f, 38.f);
	const FVector2D BuildingIconSize(46.f, 46.f);
	const FVector2D TypeBarPosition(17.f, 104.f);
	const FVector2D TypeBarSize(206.f, 19.f);
	const FVector2D NamePosition(12.f, 128.f);
	const FVector2D NameSize(216.f, 18.f);
	const FVector2D DescriptionPosition(23.f, 152.f);
	const FVector2D DescriptionSize(196.f, 128.f);
	const FVector2D RarityPosition(12.f, 284.f);
	const FVector2D RaritySize(216.f, 16.f);
	const FVector2D EmptySlotTextPosition(12.f, 142.f);
	const FVector2D EmptySlotTextSize(216.f, 22.f);
	// A clipped variant card sits behind the unit card so that only its painted spine shows.
	const FVector2D ClippedVariantPosition(140.f, 24.f);
	// Each further owned variant shows this much metal edge behind the spine.
	constexpr float ClippedVariantEdge = 6.f;
	const FVector2D VariantLanePosition(229.f, 30.f);
	const FVector2D VariantLaneSize(28.f, 256.f);
	constexpr int32 TypeFontSize = 10;
	constexpr int32 NameFontSize = 9;
	constexpr int32 DescriptionFontSize = 8;
	constexpr int32 RarityFontSize = 8;
	constexpr int32 EmptySlotFontSize = 10;
	// The lane's lettering runs down the lane like a spine's, so its box is laid out flat and turned.
	const FVector2D VariantLaneTextSize(212.f, 18.f);
	const FVector2D VariantLaneTextPosition(137.f, 149.f);
	constexpr float VariantLaneTextAngle = 90.f;
	const FVector2D VariantLaneTextPivot(0.5f, 0.5f);
	constexpr int32 VariantLaneFontSize = 9;
	constexpr int32 VariantLaneLetterSpacing = 220;
	constexpr float VariantLaneCornerRadius = 3.f;
	constexpr float VariantLaneOutlineWidth = 1.f;
	const FLinearColor TypeBarColor(0.02f, 0.022f, 0.024f, 0.9f);
	const FLinearColor VariantLaneColor(0.02f, 0.09f, 0.1f, 0.55f);
	const FLinearColor VariantLaneOutlineColor(0.25f, 0.94f, 1.f, 0.55f);
	const FLinearColor DropHighlightColor(0.25f, 0.94f, 1.f, 0.22f);
	constexpr float DragVisualOpacity = 0.85f;
	constexpr int32 SpineZOrder = 0;
	constexpr int32 HitAreaZOrder = 1;
	constexpr int32 ArtZOrder = 2;
	constexpr int32 ContentZOrder = 3;
	constexpr int32 HighlightZOrder = 4;

	FText GetRarityDisplayName(const ERTSCardRarity Rarity)
	{
		switch (Rarity)
		{
		case ERTSCardRarity::Rarity_Common:
			return NSLOCTEXT("DeckBuilder", "RarityCommon", "Common");
		case ERTSCardRarity::Rarity_UnCommon:
			return NSLOCTEXT("DeckBuilder", "RarityUncommon", "Uncommon");
		case ERTSCardRarity::Rarity_Rare:
			return NSLOCTEXT("DeckBuilder", "RarityRare", "Rare");
		case ERTSCardRarity::Rarity_VeryRare:
			return NSLOCTEXT("DeckBuilder", "RarityVeryRare", "Very Rare");
		case ERTSCardRarity::Rarity_Legendary:
			return NSLOCTEXT("DeckBuilder", "RarityLegendary", "Legendary");
		case ERTSCardRarity::Rarity_Mythic:
			return NSLOCTEXT("DeckBuilder", "RarityMythic", "Mythic");
		}
		return FText::GetEmpty();
	}
}

void UW_DeckCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_DeckCard::BuildWidgetTree()
{
	using namespace DeckCardLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	M_RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	M_RootBox->SetWidthOverride(DeckBuilderWidgets::CardSize.X);
	M_RootBox->SetHeightOverride(DeckBuilderWidgets::CardSize.Y);
	WidgetTree->RootWidget = M_RootBox;
	M_Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_RootBox->SetContent(M_Canvas);

	M_SpineCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	UCanvasPanelSlot* const SpineSlot = M_Canvas->AddChildToCanvas(M_SpineCanvas);
	SpineSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	SpineSlot->SetOffsets(FMargin(0.f));
	SpineSlot->SetZOrder(SpineZOrder);

	// Invisible but hit-testable: the card body takes the hover, the click, the drag and the drop, while the
	// spine of a clipped variant card stays reachable to the right of it.
	UBorder* const HitArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	HitArea->SetBrush(FSlateColorBrush(FLinearColor::Transparent));
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, HitArea, BodyPosition, BodySize, HitAreaZOrder);

	BuildWidgetTree_Art();
	BuildWidgetTree_Texts();

	M_DropHighlight = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_DropHighlight->SetBrush(FSlateColorBrush(DropHighlightColor));
	M_DropHighlight->SetVisibility(ESlateVisibility::Collapsed);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_DropHighlight, BodyPosition, BodySize, HighlightZOrder);
}

void UW_DeckCard::BuildWidgetTree_Art()
{
	using namespace DeckCardLayout;
	M_Frame = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_Frame, FVector2D::ZeroVector, DeckBuilderWidgets::CardSize,
	                                  ArtZOrder);
	M_Picture = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_Picture, PicturePosition, PictureSize, ContentZOrder);
	M_BuildingWell = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_BuildingWell, BuildingWellPosition, BuildingWellSize,
	                                  ContentZOrder);
	M_BuildingIcon = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_BuildingIcon, BuildingIconPosition, BuildingIconSize,
	                                  ContentZOrder);

	M_VariantLane = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	// An empty socket: dark inside with a thin accent outline, where a variant card's spine would show.
	M_VariantLane->SetBrush(FSlateRoundedBoxBrush(VariantLaneColor, VariantLaneCornerRadius, VariantLaneOutlineColor,
	                                              VariantLaneOutlineWidth));
	M_VariantLane->SetPadding(FMargin(0.f));
	M_VariantLane->SetVisibility(ESlateVisibility::Collapsed);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_VariantLane, VariantLanePosition, VariantLaneSize, ContentZOrder);
	M_VariantLaneText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_VariantLaneText->SetJustification(ETextJustify::Center);
	M_VariantLaneText->SetRenderTransformPivot(VariantLaneTextPivot);
	M_VariantLaneText->SetRenderTransformAngle(VariantLaneTextAngle);
	M_VariantLaneText->SetVisibility(ESlateVisibility::Collapsed);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_VariantLaneText, VariantLaneTextPosition, VariantLaneTextSize,
	                                  ContentZOrder);
}

void UW_DeckCard::BuildWidgetTree_Texts()
{
	using namespace DeckCardLayout;
	M_TypeBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_TypeBar->SetBrush(FSlateColorBrush(TypeBarColor));
	M_TypeBar->SetPadding(FMargin(0.f));
	M_TypeBar->SetHorizontalAlignment(HAlign_Center);
	M_TypeBar->SetVerticalAlignment(VAlign_Center);
	M_TypeBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_TypeBar, TypeBarPosition, TypeBarSize, ContentZOrder);
	M_TypeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TypeBar->SetContent(M_TypeText);

	M_NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_NameText->SetJustification(ETextJustify::Center);
	M_NameText->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_NameText, NamePosition, NameSize, ContentZOrder);

	M_DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_DescriptionText->SetAutoWrapText(true);
	M_DescriptionText->SetClipping(EWidgetClipping::ClipToBounds);
	M_DescriptionText->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_DescriptionText, DescriptionPosition, DescriptionSize,
	                                  ContentZOrder);

	M_RarityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_RarityText->SetJustification(ETextJustify::Center);
	M_RarityText->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_RarityText, RarityPosition, RaritySize, ContentZOrder);

	M_EmptySlotText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_EmptySlotText->SetText(NSLOCTEXT("DeckBuilder", "EmptySlot", "Empty Slot"));
	M_EmptySlotText->SetJustification(ETextJustify::Center);
	M_EmptySlotText->SetVisibility(ESlateVisibility::Collapsed);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_EmptySlotText, EmptySlotTextPosition, EmptySlotTextSize,
	                                  ContentZOrder);
}

void UW_DeckCard::SetupCard(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look,
                            const FDeckCardDefinition& Definition, const FDeckCardVariantsView& Variants,
                            const EDeckCardPlace Place, const EDeckSection Section, const int32 SlotIndex)
{
	M_Menu = Menu;
	M_Definition = Definition;
	M_Variants = Variants;
	M_Place = Place;
	M_Section = Section;
	M_SlotIndex = SlotIndex;
	bM_IsEmptySlot = false;
	if (not IsValid(M_Frame) || not IsValid(M_DescriptionText))
	{
		return;
	}
	const bool bIsTechnology = Definition.CardType == ECardType::Technology;
	ApplyTextStyle(Look);
	SetupCard_Art(Look, bIsTechnology);
	SetupCard_Texts(Look, bIsTechnology);
	SetupCard_Variants(Look, Variants);
	if (Place == EDeckCardPlace::Preview)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UW_DeckCard::SetupCard_Art(const FDeckBuilderLook& Look, const bool bIsTechnology)
{
	using namespace DeckCardLayout;
	M_Frame->SetBrush(Look.GetCardFrame(M_Definition.Rarity));
	M_Picture->SetBrush(DeckBuilderWidgets::MakePictureBrush(M_Definition.ImageBrush, PictureSize));
	M_Picture->SetVisibility(M_Definition.ImageBrush.GetResourceObject() ? ESlateVisibility::HitTestInvisible
		                         : ESlateVisibility::Hidden);
	if (UCanvasPanelSlot* const PictureSlot = Cast<UCanvasPanelSlot>(M_Picture->Slot))
	{
		PictureSlot->SetPosition(bIsTechnology ? TechnologyPicturePosition : PicturePosition);
	}
	const ESlateVisibility BuildingVisibility = bIsTechnology ? ESlateVisibility::Hidden
		                                            : ESlateVisibility::HitTestInvisible;
	M_BuildingWell->SetBrush(Look.IconWell);
	M_BuildingWell->SetVisibility(BuildingVisibility);
	EDeckSection CardSection = EDeckSection::Technologies;
	DeckBuilder::GetSectionOfCardType(M_Definition.CardType, CardSection);
	M_BuildingIcon->SetBrush(DeckBuilderWidgets::MakePictureBrush(Look.GetSectionIcon(CardSection), BuildingIconSize));
	M_BuildingIcon->SetVisibility(BuildingVisibility);
}

void UW_DeckCard::SetupCard_Texts(const FDeckBuilderLook& Look, const bool bIsTechnology)
{
	EDeckSection CardSection = EDeckSection::Technologies;
	DeckBuilder::GetSectionOfCardType(M_Definition.CardType, CardSection);
	const FText TypeText = bIsTechnology
		                       ? NSLOCTEXT("DeckBuilder", "CardTypeTechnology", "Technology")
		                       : FText::Format(NSLOCTEXT("DeckBuilder", "CardTypeUnit", "{0} Unit"),
		                                       DeckBuilder::GetSectionDisplayName(CardSection));
	M_TypeText->SetText(TypeText);
	M_NameText->SetText(UUnitVariantPresentationLibrary::StripRichTextMarkup(M_Definition.Title));
	M_DescriptionText->SetText(UUnitVariantPresentationLibrary::StripRichTextMarkup(M_Definition.Description));
	M_RarityText->SetText(DeckCardLayout::GetRarityDisplayName(M_Definition.Rarity));
	M_EmptySlotText->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_DeckCard::SetupCard_Variants(const FDeckBuilderLook& Look, const FDeckCardVariantsView& Variants)
{
	using namespace DeckCardLayout;
	M_SpineCanvas->ClearChildren();
	M_ClippedVariantCards.Reset();
	const int32 ClippedCount = Variants.ClippedVariants.Num();
	// Back to front, so the attached variant (index 0) is drawn last and its spine is the one that shows.
	for (int32 VariantIndex = ClippedCount - 1; VariantIndex >= 0; --VariantIndex)
	{
		UW_DeckVariantCard* const VariantCard = CreateWidget<UW_DeckVariantCard>(this, UW_DeckVariantCard::StaticClass());
		if (not IsValid(VariantCard))
		{
			continue;
		}
		const EDeckVariantCardPlace VariantPlace = M_Place == EDeckCardPlace::Preview
			                                           ? EDeckVariantCardPlace::Preview
			                                           : EDeckVariantCardPlace::Clipped;
		VariantCard->SetupVariantCard(M_Menu.Get(), Look, Variants.ClippedVariants[VariantIndex], VariantPlace);
		const FVector2D Position = ClippedVariantPosition + FVector2D(ClippedVariantEdge * VariantIndex, 0.f);
		DeckBuilderWidgets::PlaceOnCanvas(*M_SpineCanvas, VariantCard, Position, DeckBuilderWidgets::VariantCardSize);
		M_ClippedVariantCards.Add(VariantCard);
	}
	const bool bShowsLane = ClippedCount == 0 && Variants.AvailableVariantCount > 0;
	const ESlateVisibility LaneVisibility = bShowsLane ? ESlateVisibility::HitTestInvisible
		                                        : ESlateVisibility::Collapsed;
	M_VariantLane->SetVisibility(LaneVisibility);
	M_VariantLaneText->SetVisibility(LaneVisibility);
	M_VariantLaneText->SetText(FText::Format(NSLOCTEXT("DeckBuilder", "VariantLane", "+ {0} AVAILABLE"),
	                                         FText::AsNumber(Variants.AvailableVariantCount)));
	// A slot is always as wide as a card with a spine, so the grid stays even; elsewhere the card is only as wide
	// as what it shows.
	float CardWidth = DeckBuilderWidgets::CardSize.X;
	if (M_Place == EDeckCardPlace::Slot || ClippedCount > 0 || bShowsLane)
	{
		CardWidth = DeckBuilderWidgets::CardWidthWithSpine + ClippedVariantEdge * FMath::Max(0, ClippedCount - 1);
	}
	M_RootBox->SetWidthOverride(CardWidth);
}

void UW_DeckCard::SetupEmptySlot(UW_DeckBuilderMenu* Menu, const FDeckBuilderLook& Look, const EDeckSection Section,
                                 const int32 SlotIndex)
{
	M_Menu = Menu;
	M_Definition = FDeckCardDefinition();
	M_Variants = FDeckCardVariantsView();
	M_Place = EDeckCardPlace::Slot;
	M_Section = Section;
	M_SlotIndex = SlotIndex;
	bM_IsEmptySlot = true;
	if (not IsValid(M_Frame) || not IsValid(M_EmptySlotText))
	{
		return;
	}
	ApplyTextStyle(Look);
	M_Frame->SetBrush(Look.VacantCardFrame);
	for (UWidget* const HiddenPart : TArray<UWidget*>{M_Picture, M_BuildingWell, M_BuildingIcon, M_TypeBar, M_NameText,
	                                                  M_DescriptionText, M_RarityText, M_VariantLane,
	                                                  M_VariantLaneText})
	{
		HiddenPart->SetVisibility(ESlateVisibility::Collapsed);
	}
	M_EmptySlotText->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_SpineCanvas->ClearChildren();
	M_ClippedVariantCards.Reset();
	M_RootBox->SetWidthOverride(DeckBuilderWidgets::CardWidthWithSpine);
}

void UW_DeckCard::ApplyTextStyle(const FDeckBuilderLook& Look)
{
	using namespace DeckCardLayout;
	const FDeckBuilderStyle& Style = Look.Style;
	const auto StyleText = [](UTextBlock* TextBlock, const FSlateFontInfo& Font, const FSlateColor& Color)
	{
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(Color);
		TextBlock->SetShadowOffset(DeckBuilderWidgets::TextShadowOffset);
		TextBlock->SetShadowColorAndOpacity(DeckBuilderWidgets::TextShadowColor);
	};
	StyleText(M_TypeText, DeckBuilderWidgets::MakeFont(Look.BodyFont, TypeFontSize), Style.TextColor);
	StyleText(M_NameText, DeckBuilderWidgets::MakeFont(Look.BodyFont, NameFontSize), Style.TextColor);
	StyleText(M_DescriptionText, DeckBuilderWidgets::MakeFont(Look.BodyFont, DescriptionFontSize),
	          Style.DescriptionColor);
	StyleText(M_RarityText, DeckBuilderWidgets::MakeFont(Look.BodyFont, RarityFontSize), Style.GoodColor);
	StyleText(M_EmptySlotText, DeckBuilderWidgets::MakeFont(Look.BodyFont, EmptySlotFontSize), Style.MutedTextColor);
	StyleText(M_VariantLaneText,
	          DeckBuilderWidgets::MakeFont(Look.HeadingFont, VariantLaneFontSize, VariantLaneLetterSpacing),
	          Style.AccentColor);
}

void UW_DeckCard::SetDropHighlight(const bool bIsHighlighted)
{
	if (not IsValid(M_DropHighlight))
	{
		return;
	}
	M_DropHighlight->SetVisibility(bIsHighlighted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UW_DeckCard::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (M_Place == EDeckCardPlace::Preview || bM_IsEmptySlot || not GetIsValidMenu())
	{
		return;
	}
	M_Menu->HandleCardHovered(M_Definition.Card);
}

FReply UW_DeckCard::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (M_Place == EDeckCardPlace::Preview || bM_IsEmptySlot || not GetIsValidMenu())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		M_Menu->HandleCardRightClicked(*this);
		return FReply::Handled();
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UW_DeckCard::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                       UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (bM_IsEmptySlot || not GetIsValidMenu())
	{
		return;
	}
	UDeckCardDragOperation* const DragOperation = NewObject<UDeckCardDragOperation>(this);
	DragOperation->bIsVariant = false;
	DragOperation->Card = M_Definition.Card;
	DragOperation->bIsFromSlot = M_Place == EDeckCardPlace::Slot;
	DragOperation->Section = M_Section;
	DragOperation->SlotIndex = M_SlotIndex;
	UW_DeckCard* const DragVisual = CreateWidget<UW_DeckCard>(this, GetClass());
	if (IsValid(DragVisual))
	{
		DragVisual->SetupCard(M_Menu.Get(), M_Menu->GetLook(), M_Definition, FDeckCardVariantsView(),
		                      EDeckCardPlace::Preview, M_Section, INDEX_NONE);
		DragVisual->SetRenderOpacity(DeckCardLayout::DragVisualOpacity);
		DragOperation->DefaultDragVisual = DragVisual;
	}
	DragOperation->Pivot = EDragPivot::MouseDown;
	OutOperation = DragOperation;
}

void UW_DeckCard::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                    UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	const UDeckCardDragOperation* const DragOperation = Cast<UDeckCardDragOperation>(InOperation);
	if (not DragOperation || not M_Menu.IsValid())
	{
		return;
	}
	SetDropHighlight(M_Menu->GetIsDropAllowed(*DragOperation, *this));
}

void UW_DeckCard::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	SetDropHighlight(false);
}

bool UW_DeckCard::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                               UDragDropOperation* InOperation)
{
	SetDropHighlight(false);
	const UDeckCardDragOperation* const DragOperation = Cast<UDeckCardDragOperation>(InOperation);
	if (not DragOperation || M_Place == EDeckCardPlace::Preview || not GetIsValidMenu())
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	// Handled even when the menu refuses the drop, so a refused card is not read as "dropped on the collection".
	M_Menu->HandleDropOnCard(*DragOperation, *this);
	return true;
}

bool UW_DeckCard::GetIsValidMenu() const
{
	if (M_Menu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_Menu"), TEXT("GetIsValidMenu"), this);
	return false;
}
