// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_DeckBuilderMenu.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "DeckBuilderWidgetHelpers.h"
#include "DeckCardCatalog.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/CardSystem/PlayerProfile/PlayerProfileLoader/FRTS_Profile.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Units/Variants/UnitVariantPresentationLibrary.h"
#include "RTS_Survival/Units/Variants/UnitVariantSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/W_WorldMenu.h"
#include "Slate/SlateBrushAsset.h"
#include "W_DeckCard.h"
#include "W_DeckSectionEntry.h"
#include "W_DeckVariantCard.h"
#include "W_DeckVariantRow.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSDeckBuilder, Log, All);

// Screen layout of the approved "O2 deck builder" sketch (1920x1080 reference, whole units).
namespace DeckBuilderMenuLayout
{
	const FVector2D ScreenSize(1920.f, 1080.f);
	const FVector2D RailPosition(8.f, 8.f);
	const FVector2D RailSize(262.f, 1064.f);
	const FMargin RailPadding(18.f, 18.f, 18.f, 18.f);
	const FVector2D SectionPanelPosition(276.f, 8.f);
	const FVector2D SectionPanelSize(1072.f, 708.f);
	// Leaves exactly four slots of 262 units across.
	const FMargin SectionPanelPadding(12.f, 14.f, 12.f, 12.f);
	const FVector2D DockPosition(276.f, 722.f);
	const FVector2D DockSize(1072.f, 350.f);
	const FMargin DockPadding(18.f, 16.f, 18.f, 12.f);
	const FVector2D CollectionPosition(1354.f, 8.f);
	const FVector2D CollectionSize(558.f, 1064.f);
	const FMargin CollectionPadding(16.f, 14.f, 16.f, 14.f);

	constexpr int32 GridColumns = 4;
	const FVector2D SlotCellSize(262.f, 315.f);
	// Collection cards are drawn smaller so three unit cards fit across the column.
	constexpr float CollectionCardScale = 0.68f;
	const FVector2D CollectionCardSpacing(8.f, 8.f);
	const FVector2D DockRowSpacing(8.f, 8.f);
	const FVector2D DockPreviewSize(262.f, 315.f);
	const FVector2D SectionIconSize(36.f, 36.f);
	const FVector2D SaveButtonSize(226.f, 48.f);
	const FVector2D FitsTabSize(230.f, 32.f);
	const FVector2D EverythingTabSize(170.f, 32.f);
	const FVector2D SortButtonSize(150.f, 32.f);
	const FVector2D VariantPictureSize(64.f, 64.f);
	constexpr float RailEntrySpacing = 6.f;

	constexpr int32 RailTitleFontSize = 13;
	constexpr int32 RailTitleLetterSpacing = 150;
	constexpr int32 ButtonFontSize = 12;
	constexpr int32 ButtonLetterSpacing = 100;
	constexpr int32 SectionNameFontSize = 16;
	constexpr int32 SectionNameLetterSpacing = 120;
	constexpr int32 SectionSubtitleFontSize = 10;
	constexpr int32 SectionCountFontSize = 10;
	constexpr int32 UnavailableFontSize = 12;
	constexpr int32 DockTitleFontSize = 15;
	constexpr int32 DockTitleLetterSpacing = 100;
	constexpr int32 DockSubtitleFontSize = 10;
	constexpr int32 DockDescriptionFontSize = 10;
	constexpr int32 GroupHeaderFontSize = 9;
	constexpr int32 GroupHeaderLetterSpacing = 150;
	constexpr int32 TabFontSize = 10;
	constexpr int32 HintFontSize = 10;

	// Dims the campaign map behind the menu; opaque enough that the map does not compete with the cards.
	const FLinearColor BackdropColor(0.03f, 0.035f, 0.04f, 0.86f);
}

void UW_DeckBuilderMenu::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	TArray<FString> MissingTextures;
	M_Look.BuildFromStyle(M_Style, MissingTextures);
	if (MissingTextures.Num() > 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UW_DeckBuilderMenu: %d textures of the deck builder did not load and draw as flat fills. Run "
				"Source/Tools/DeckBuilder/import_deck_builder_art.py if the deck art is missing. First: %s"),
			MissingTextures.Num(), *MissingTextures[0]));
		for (const FString& MissingTexture : MissingTextures)
		{
			UE_LOG(LogRTSDeckBuilder, Warning, TEXT("DECKBUILDER missing texture %s"), *MissingTexture);
		}
	}
	BuildWidgetTree();
}

FReply UW_DeckBuilderMenu::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

bool UW_DeckBuilderMenu::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
                                      UDragDropOperation* InOperation)
{
	const UDeckCardDragOperation* const DragOperation = Cast<UDeckCardDragOperation>(InOperation);
	if (not DragOperation || not DragOperation->bIsFromSlot)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}
	const bool bChanged = DragOperation->bIsVariant
		                      ? M_State.DetachVariant(DragOperation->Variant.Unit)
		                      : M_State.ReturnCardToCollection(DragOperation->Section, DragOperation->SlotIndex);
	if (bChanged)
	{
		RefreshAll();
	}
	return true;
}

void UW_DeckBuilderMenu::BuildWidgetTree()
{
	using namespace DeckBuilderMenuLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UOverlay* const Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	WidgetTree->RootWidget = Root;
	// Visible, so it takes the mouse over the whole screen and no click falls through to the map.
	UBorder* const Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Backdrop->SetBrush(FSlateColorBrush(BackdropColor));
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

	BuildRail();
	BuildSectionPanel();
	BuildDock();
	BuildCollection();
}

UBorder* UW_DeckBuilderMenu::BuildPanel(const FVector2D& Position, const FVector2D& Size, const FMargin& PanelPadding)
{
	UBorder* const Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Panel->SetBrush(M_Look.Panel);
	Panel->SetPadding(PanelPadding);
	DeckBuilderWidgets::PlaceOnCanvas(*M_ScreenCanvas, Panel, Position, Size);
	return Panel;
}

void UW_DeckBuilderMenu::BuildRail()
{
	using namespace DeckBuilderMenuLayout;
	const FDeckBuilderStyle& Style = M_Look.Style;
	UBorder* const Panel = BuildPanel(RailPosition, RailSize, RailPadding);
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);
	UTextBlock* const Title = DeckBuilderWidgets::MakeText(
		*WidgetTree, NSLOCTEXT("DeckBuilder", "RailTitle", "YOUR ARMY"),
		DeckBuilderWidgets::MakeFont(M_Look.HeadingFont, RailTitleFontSize, RailTitleLetterSpacing), Style.TextColor);
	Column->AddChildToVerticalBox(Title)->SetPadding(FMargin(4.f, 2.f, 0.f, 12.f));

	M_SectionEntries.Reset();
	for (const EDeckSection Section : DeckBuilder::GetAllSections())
	{
		UW_DeckSectionEntry* const Entry = CreateWidget<UW_DeckSectionEntry>(this, UW_DeckSectionEntry::StaticClass());
		if (not IsValid(Entry))
		{
			continue;
		}
		Entry->SetupEntry(this, M_Look, Section);
		Column->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0.f, 0.f, 0.f, RailEntrySpacing));
		M_SectionEntries.Add(Entry);
	}

	TObjectPtr<UTextBlock> SaveLabel;
	UWidget* const SaveBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, NSLOCTEXT("DeckBuilder", "SaveAndGoBack", "SAVE & GO BACK"), M_Look.Button,
		DeckBuilderWidgets::MakeFont(M_Look.HeadingFont, ButtonFontSize, ButtonLetterSpacing), Style.TextColor,
		SaveButtonSize, M_SaveButton, SaveLabel);
	M_SaveButton->OnClicked.AddDynamic(this, &UW_DeckBuilderMenu::OnSaveClicked);
	Column->AddChildToVerticalBox(SaveBox)->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
}

void UW_DeckBuilderMenu::BuildSectionPanel()
{
	using namespace DeckBuilderMenuLayout;
	const FDeckBuilderStyle& Style = M_Look.Style;
	UBorder* const Panel = BuildPanel(SectionPanelPosition, SectionPanelSize, SectionPanelPadding);
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);

	UHorizontalBox* const Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Header)->SetPadding(FMargin(8.f, 0.f, 10.f, 8.f));
	USizeBox* const IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(SectionIconSize.X);
	IconBox->SetHeightOverride(SectionIconSize.Y);
	M_SectionIcon = DeckBuilderWidgets::MakeImage(*WidgetTree, FSlateBrush());
	IconBox->SetContent(M_SectionIcon);
	Header->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);

	M_SectionNameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look.HeadingFont, SectionNameFontSize, SectionNameLetterSpacing), Style.TextColor);
	UHorizontalBoxSlot* const NameSlot = Header->AddChildToHorizontalBox(M_SectionNameText);
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	M_SectionSubtitleText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look.BodyFont, SectionSubtitleFontSize),
		Style.MutedTextColor);
	UHorizontalBoxSlot* const SubtitleSlot = Header->AddChildToHorizontalBox(M_SectionSubtitleText);
	SubtitleSlot->SetVerticalAlignment(VAlign_Center);
	SubtitleSlot->SetPadding(FMargin(12.f, 4.f, 0.f, 0.f));
	USpacer* const HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
	Header->AddChildToHorizontalBox(HeaderSpacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	M_SectionCountText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look.BodyFont, SectionCountFontSize),
		Style.GoodColor);
	Header->AddChildToHorizontalBox(M_SectionCountText)->SetVerticalAlignment(VAlign_Center);

	UOverlay* const Body = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Column->AddChildToVerticalBox(Body)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	// A section with more than two rows of slots scrolls down; nothing scrolls sideways.
	UScrollBox* const GridScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UOverlaySlot* const GridSlot = Body->AddChildToOverlay(GridScroll);
	GridSlot->SetHorizontalAlignment(HAlign_Fill);
	GridSlot->SetVerticalAlignment(VAlign_Fill);
	M_SlotGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	// Left-aligned: a section with fewer slots than columns keeps the same spacing as a full row.
	if (UScrollBoxSlot* const GridScrollSlot = Cast<UScrollBoxSlot>(GridScroll->AddChild(M_SlotGrid)))
	{
		GridScrollSlot->SetHorizontalAlignment(HAlign_Left);
	}

	M_SectionUnavailableText = DeckBuilderWidgets::MakeText(
		*WidgetTree, NSLOCTEXT("DeckBuilder", "SectionUnavailable", "This section is not part of your army yet."),
		DeckBuilderWidgets::MakeFont(M_Look.BodyFont, UnavailableFontSize), Style.MutedTextColor);
	UOverlaySlot* const UnavailableSlot = Body->AddChildToOverlay(M_SectionUnavailableText);
	UnavailableSlot->SetHorizontalAlignment(HAlign_Center);
	UnavailableSlot->SetVerticalAlignment(VAlign_Center);
	M_SectionUnavailableText->SetVisibility(ESlateVisibility::Collapsed);
}

void UW_DeckBuilderMenu::BuildDock()
{
	using namespace DeckBuilderMenuLayout;
	const FDeckBuilderStyle& Style = M_Look.Style;
	UBorder* const Panel = BuildPanel(DockPosition, DockSize, DockPadding);
	UHorizontalBox* const Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Panel->SetContent(Row);
	// Fixed size, so the texts do not jump when the dock changes from a unit card to a narrow variant card.
	USizeBox* const PreviewBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PreviewBox->SetWidthOverride(DockPreviewSize.X);
	PreviewBox->SetHeightOverride(DockPreviewSize.Y);
	Row->AddChildToHorizontalBox(PreviewBox)->SetVerticalAlignment(VAlign_Top);
	M_DockPreviewHost = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DeckBuilderWidgets::SetTopLeftContent(*PreviewBox, M_DockPreviewHost);

	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBoxSlot* const TextSlot = Row->AddChildToHorizontalBox(TextColumn);
	TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TextSlot->SetPadding(FMargin(10.f, 4.f, 0.f, 0.f));
	M_DockTitleText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look.HeadingFont, DockTitleFontSize, DockTitleLetterSpacing), Style.TextColor);
	TextColumn->AddChildToVerticalBox(M_DockTitleText);
	M_DockSubtitleText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look.BodyFont, DockSubtitleFontSize),
		Style.AccentColor);
	TextColumn->AddChildToVerticalBox(M_DockSubtitleText)->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
	M_DockDescriptionText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look.BodyFont, DockDescriptionFontSize),
		Style.DescriptionColor);
	M_DockDescriptionText->SetAutoWrapText(true);
	TextColumn->AddChildToVerticalBox(M_DockDescriptionText)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	M_DockVariantsLabel = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(M_Look.HeadingFont, GroupHeaderFontSize, GroupHeaderLetterSpacing),
		Style.MutedTextColor);
	TextColumn->AddChildToVerticalBox(M_DockVariantsLabel)->SetPadding(FMargin(0.f, 14.f, 0.f, 8.f));
	M_DockVariantRows = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	M_DockVariantRows->SetInnerSlotPadding(DockRowSpacing);
	TextColumn->AddChildToVerticalBox(M_DockVariantRows);
}

void UW_DeckBuilderMenu::BuildCollection()
{
	using namespace DeckBuilderMenuLayout;
	const FDeckBuilderStyle& Style = M_Look.Style;
	UBorder* const Panel = BuildPanel(CollectionPosition, CollectionSize, CollectionPadding);
	UVerticalBox* const Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Column);
	const FSlateFontInfo TabFont = DeckBuilderWidgets::MakeFont(M_Look.HeadingFont, TabFontSize);

	UHorizontalBox* const Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Column->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	UWidget* const FitsTab = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, FText::GetEmpty(), M_Look.TabSelected, TabFont, Style.TextColor, FitsTabSize, M_FitsTabButton,
		M_FitsTabLabel);
	M_FitsTabButton->OnClicked.AddDynamic(this, &UW_DeckBuilderMenu::OnFitsTabClicked);
	Tabs->AddChildToHorizontalBox(FitsTab)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	UWidget* const EverythingTab = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, FText::GetEmpty(), M_Look.Tab, TabFont, Style.TextColor, EverythingTabSize,
		M_EverythingTabButton, M_EverythingTabLabel);
	M_EverythingTabButton->OnClicked.AddDynamic(this, &UW_DeckBuilderMenu::OnEverythingTabClicked);
	Tabs->AddChildToHorizontalBox(EverythingTab);

	UScrollBox* const Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Column->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* const Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Body);
	const FSlateFontInfo GroupFont = DeckBuilderWidgets::MakeFont(M_Look.HeadingFont, GroupHeaderFontSize,
	                                                              GroupHeaderLetterSpacing);
	M_CollectionCardsHeader = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(), GroupFont, Style.TextColor);
	Body->AddChildToVerticalBox(M_CollectionCardsHeader)->SetPadding(FMargin(2.f, 6.f, 0.f, 8.f));
	M_CollectionCards = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	M_CollectionCards->SetInnerSlotPadding(CollectionCardSpacing);
	Body->AddChildToVerticalBox(M_CollectionCards);
	M_CollectionVariantsHeader = DeckBuilderWidgets::MakeText(*WidgetTree, FText::GetEmpty(), GroupFont,
	                                                         Style.TextColor);
	Body->AddChildToVerticalBox(M_CollectionVariantsHeader)->SetPadding(FMargin(2.f, 14.f, 0.f, 8.f));
	M_CollectionVariants = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass());
	M_CollectionVariants->SetInnerSlotPadding(CollectionCardSpacing);
	Body->AddChildToVerticalBox(M_CollectionVariants);

	UHorizontalBox* const SortRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Body->AddChildToVerticalBox(SortRow)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	TObjectPtr<UTextBlock> SortByRarityLabel;
	UWidget* const SortByRarity = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, NSLOCTEXT("DeckBuilder", "SortByRarity", "Sort By Rarity"), M_Look.Tab, TabFont,
		Style.TextColor, SortButtonSize, M_SortByRarityButton, SortByRarityLabel);
	M_SortByRarityButton->OnClicked.AddDynamic(this, &UW_DeckBuilderMenu::OnSortByRarityClicked);
	SortRow->AddChildToHorizontalBox(SortByRarity)->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	TObjectPtr<UTextBlock> SortByTypeLabel;
	UWidget* const SortByType = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, NSLOCTEXT("DeckBuilder", "SortByType", "Sort By Type"), M_Look.Tab, TabFont, Style.TextColor,
		SortButtonSize, M_SortByTypeButton, SortByTypeLabel);
	M_SortByTypeButton->OnClicked.AddDynamic(this, &UW_DeckBuilderMenu::OnSortByTypeClicked);
	SortRow->AddChildToHorizontalBox(SortByType);

	M_CollectionHintText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(M_Look.BodyFont, HintFontSize),
		Style.MutedTextColor);
	M_CollectionHintText->SetAutoWrapText(true);
	Body->AddChildToVerticalBox(M_CollectionHintText)->SetPadding(FMargin(4.f, 18.f, 4.f, 0.f));
}

void UW_DeckBuilderMenu::InitDeckBuilder(UW_WorldMenu* WorldMenu)
{
	M_WorldMenu = WorldMenu;
	(void)GetIsValidWorldMenu();
}

void UW_DeckBuilderMenu::LoadCatalogIfNeeded()
{
	if (IsValid(M_Catalog))
	{
		return;
	}
	M_Catalog = NewObject<UDeckCardCatalog>(this);
	const int32 CardCount = M_Catalog->LoadFromTable(M_Style.CardTable.LoadSynchronous());
	M_VariantDataAsset = UUnitVariantSettings::Get()->LoadVariantDataAsset();
	UE_LOG(LogRTSDeckBuilder, Display, TEXT("DECKBUILDER catalog loaded: %d cards, variant asset %s"), CardCount,
	       IsValid(M_VariantDataAsset) ? *M_VariantDataAsset->GetName() : TEXT("none"));
}

bool UW_DeckBuilderMenu::GetOwnsEveryVariant() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return bM_OwnEveryVariantUntilRewardsExist;
#endif
}

TArray<FUnitVariantSelection> UW_DeckBuilderMenu::GatherOwnedVariants(const FPlayerCardSaveData& CardData) const
{
	TArray<FUnitVariantSelection> OwnedVariants = CardData.OwnedUnitVariants;
	if (not GetOwnsEveryVariant() || not IsValid(M_Catalog) || not IsValid(M_VariantDataAsset))
	{
		return OwnedVariants;
	}
	const EUnitVariant VariantChoices[] = {EUnitVariant::VariantA, EUnitVariant::VariantB, EUnitVariant::VariantC};
	for (const TPair<ERTSCard, FDeckCardDefinition>& Entry : M_Catalog->GetDefinitions())
	{
		// Only units that have a card can carry a variant card.
		if (M_Catalog->FindDefinitionOfUnit(Entry.Value.VariantUnit) != &Entry.Value)
		{
			continue;
		}
		for (const EUnitVariant Variant : VariantChoices)
		{
			if (not M_VariantDataAsset->GetIsVariantImplemented(Entry.Value.VariantUnit, Variant))
			{
				continue;
			}
			FUnitVariantSelection Owned;
			Owned.Unit = Entry.Value.VariantUnit;
			Owned.Variant = Variant;
			OwnedVariants.Add(Owned);
		}
	}
	return OwnedVariants;
}

void UW_DeckBuilderMenu::SetupFromCardData(const FPlayerCardSaveData& CardData)
{
	LoadCatalogIfNeeded();
	if (not GetIsValidCatalog())
	{
		return;
	}
	M_State.Init(CardData, M_Catalog->GetDefinitions(), GatherOwnedVariants(CardData));
	if (not M_State.GetSection(M_SelectedSection).bIsAvailable)
	{
		for (const EDeckSection Section : DeckBuilder::GetAllSections())
		{
			if (Section != EDeckSection::Technologies && M_State.GetSection(Section).bIsAvailable)
			{
				M_SelectedSection = Section;
				break;
			}
		}
	}
	M_DockSubject = FDeckDockSubject();
	RefreshAll();
	UE_LOG(LogRTSDeckBuilder, Display, TEXT("DECKBUILDER opened: collection %d cards, %d loose variants, section %s"),
	       M_State.GetCollection().Num(), M_State.GetLooseVariants().Num(),
	       *DeckBuilder::GetSectionDisplayName(M_SelectedSection).ToString());
}

void UW_DeckBuilderMenu::HandleSectionClicked(const EDeckSection Section)
{
	M_SelectedSection = Section;
	RefreshAll();
}

void UW_DeckBuilderMenu::HandleCardHovered(const ERTSCard Card)
{
	if (M_DockSubject.Kind == FDeckDockSubject::EKind::Card && M_DockSubject.Card == Card)
	{
		return;
	}
	M_DockSubject.Kind = FDeckDockSubject::EKind::Card;
	M_DockSubject.Card = Card;
	RefreshDock();
}

void UW_DeckBuilderMenu::HandleVariantHovered(const FUnitVariantSelection& Variant)
{
	const bool bIsShown = M_DockSubject.Kind == FDeckDockSubject::EKind::Variant
		&& DeckBuilder::GetIsSameVariant(M_DockSubject.Variant, Variant);
	if (bIsShown)
	{
		return;
	}
	M_DockSubject.Kind = FDeckDockSubject::EKind::Variant;
	M_DockSubject.Variant = Variant;
	RefreshDock();
}

void UW_DeckBuilderMenu::HandleCardRightClicked(const UW_DeckCard& Card)
{
	if (Card.GetPlace() == EDeckCardPlace::Slot)
	{
		if (M_State.ReturnCardToCollection(Card.GetSection(), Card.GetSlotIndex()))
		{
			RefreshAll();
		}
		return;
	}
	if (not M_State.SlotCard(Card.GetCard(), Card.GetSection()))
	{
		return;
	}
	// The card went to its own section, which may not be the one on screen.
	M_SelectedSection = Card.GetSection();
	RefreshAll();
}

void UW_DeckBuilderMenu::HandleVariantRightClicked(const FUnitVariantSelection& Variant, const bool bIsClipped)
{
	const bool bChanged = bIsClipped ? M_State.DetachVariant(Variant.Unit) : M_State.AttachVariant(Variant);
	if (bChanged)
	{
		RefreshAll();
	}
}

bool UW_DeckBuilderMenu::GetIsDropAllowed(const UDeckCardDragOperation& Operation, const UW_DeckCard& Target) const
{
	if (Operation.bIsVariant)
	{
		return not Target.GetIsEmptySlot() && not Operation.Variant.Unit.IsNone()
			&& Target.GetDefinition().VariantUnit == Operation.Variant.Unit;
	}
	if (Operation.bIsFromSlot)
	{
		return Target.GetPlace() == EDeckCardPlace::Collection;
	}
	const FDeckCardDefinition* Definition = M_State.FindDefinition(Operation.Card);
	const FDeckSectionState& TargetSection = M_State.GetSection(Target.GetSection());
	return Target.GetPlace() == EDeckCardPlace::Slot && Definition && TargetSection.bIsAvailable
		&& Definition->CardType == DeckBuilder::GetCardTypeOfSection(Target.GetSection());
}

bool UW_DeckBuilderMenu::HandleDropOnCard(const UDeckCardDragOperation& Operation, const UW_DeckCard& Target)
{
	if (not GetIsDropAllowed(Operation, Target))
	{
		return false;
	}
	bool bChanged = false;
	if (Operation.bIsVariant)
	{
		bChanged = M_State.AttachVariant(Operation.Variant);
	}
	else if (Operation.bIsFromSlot)
	{
		bChanged = M_State.ReturnCardToCollection(Operation.Section, Operation.SlotIndex);
	}
	else
	{
		bChanged = M_State.SlotCard(Operation.Card, Target.GetSection(), Target.GetSlotIndex());
	}
	if (bChanged)
	{
		RefreshAll();
	}
	return bChanged;
}

void UW_DeckBuilderMenu::HandleDockVariantClicked(const FUnitVariantSelection& Variant)
{
	const bool bChanged = Variant.Variant == EUnitVariant::Base
		                      ? M_State.DetachVariant(Variant.Unit)
		                      : M_State.AttachVariant(Variant);
	if (bChanged)
	{
		RefreshAll();
	}
}

void UW_DeckBuilderMenu::RefreshAll()
{
	RefreshRail();
	RefreshSectionPanel();
	RefreshCollection();
	RefreshDock();
}

void UW_DeckBuilderMenu::RefreshRail()
{
	for (UW_DeckSectionEntry* const Entry : M_SectionEntries)
	{
		if (not IsValid(Entry))
		{
			continue;
		}
		const FDeckSectionState& Section = M_State.GetSection(Entry->GetSection());
		Entry->RefreshEntry(M_Look, Section.bIsAvailable, Section.Cards.Num(), Section.SlotCount,
		                    Entry->GetSection() == M_SelectedSection);
	}
}

void UW_DeckBuilderMenu::RefreshSectionPanel()
{
	if (not IsValid(M_SlotGrid) || not IsValid(M_SectionCountText))
	{
		return;
	}
	const FDeckSectionState& Section = M_State.GetSection(M_SelectedSection);
	M_SectionIcon->SetBrush(DeckBuilderWidgets::MakePictureBrush(M_Look.GetSectionIcon(M_SelectedSection),
	                                                            DeckBuilderMenuLayout::SectionIconSize));
	M_SectionNameText->SetText(DeckBuilder::GetSectionDisplayName(M_SelectedSection).ToUpper());
	M_SectionSubtitleText->SetText(DeckBuilder::GetSectionSubtitle(M_SelectedSection));
	const bool bIsFull = Section.Cards.Num() >= Section.SlotCount;
	M_SectionCountText->SetColorAndOpacity(bIsFull ? M_Look.Style.GoodColor : M_Look.Style.OpenSlotsColor);
	FText CountText = FText::Format(NSLOCTEXT("DeckBuilder", "SectionCount", "{0} / {1} slots"),
	                                FText::AsNumber(Section.Cards.Num()), FText::AsNumber(Section.SlotCount));
	if (M_SelectedSection != EDeckSection::Technologies)
	{
		CountText = FText::Format(NSLOCTEXT("DeckBuilder", "SectionCountVariants", "{0}, {1} {1}|plural(one=variant,other=variants) attached"),
		                          CountText, M_State.GetAttachedVariantCount(M_SelectedSection));
	}
	M_SectionCountText->SetText(Section.bIsAvailable ? CountText : FText::GetEmpty());
	M_SectionUnavailableText->SetVisibility(Section.bIsAvailable ? ESlateVisibility::Collapsed
		                                        : ESlateVisibility::HitTestInvisible);
	RefreshSectionPanel_Grid(Section);
}

void UW_DeckBuilderMenu::RefreshSectionPanel_Grid(const FDeckSectionState& Section)
{
	using namespace DeckBuilderMenuLayout;
	M_SlotGrid->ClearChildren();
	if (not Section.bIsAvailable)
	{
		return;
	}
	for (int32 SlotIndex = 0; SlotIndex < Section.SlotCount; ++SlotIndex)
	{
		UW_DeckCard* const CardWidget = CreateWidget<UW_DeckCard>(this, UW_DeckCard::StaticClass());
		if (not IsValid(CardWidget))
		{
			continue;
		}
		const FDeckCardDefinition* Definition = Section.Cards.IsValidIndex(SlotIndex)
			                                        ? M_State.FindDefinition(Section.Cards[SlotIndex])
			                                        : nullptr;
		if (Definition)
		{
			CardWidget->SetupCard(this, M_Look, *Definition, MakeCardVariantsView(*Definition), EDeckCardPlace::Slot,
			                      Section.Section, SlotIndex);
		}
		else
		{
			CardWidget->SetupEmptySlot(this, M_Look, Section.Section, SlotIndex);
		}
		USizeBox* const Cell = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Cell->SetWidthOverride(SlotCellSize.X);
		Cell->SetHeightOverride(SlotCellSize.Y);
		DeckBuilderWidgets::SetTopLeftContent(*Cell, CardWidget);
		M_SlotGrid->AddChildToUniformGrid(Cell, SlotIndex / GridColumns, SlotIndex % GridColumns);
	}
}

void UW_DeckBuilderMenu::SortCollectionCards(TArray<ERTSCard>& InOutCards) const
{
	if (M_CollectionSort == EDeckCollectionSort::AsOwned)
	{
		return;
	}
	const bool bByRarity = M_CollectionSort == EDeckCollectionSort::ByRarity;
	InOutCards.StableSort([this, bByRarity](const ERTSCard Left, const ERTSCard Right)
	{
		const FDeckCardDefinition* LeftDefinition = M_State.FindDefinition(Left);
		const FDeckCardDefinition* RightDefinition = M_State.FindDefinition(Right);
		if (not LeftDefinition || not RightDefinition)
		{
			return false;
		}
		// Rarest first, or in section order (the card types are declared in that order).
		return bByRarity
			       ? static_cast<uint8>(LeftDefinition->Rarity) > static_cast<uint8>(RightDefinition->Rarity)
			       : static_cast<uint8>(LeftDefinition->CardType) < static_cast<uint8>(RightDefinition->CardType);
	});
}

void UW_DeckBuilderMenu::RefreshCollection()
{
	if (not IsValid(M_CollectionCards) || not IsValid(M_CollectionVariants) || not IsValid(M_FitsTabLabel))
	{
		return;
	}
	const FText SectionName = DeckBuilder::GetSectionDisplayName(M_SelectedSection);
	const TArray<ERTSCard> FittingCards = M_State.GetCollectionForSection(M_SelectedSection);
	const TArray<FUnitVariantSelection> FittingVariants = M_State.GetLooseVariantsForSection(M_SelectedSection);
	const TArray<FUnitVariantSelection> AllLooseVariants = M_State.GetLooseVariants();
	const int32 FittingCount = FittingCards.Num() + FittingVariants.Num();
	const int32 EverythingCount = M_State.GetCollection().Num() + AllLooseVariants.Num();
	M_FitsTabLabel->SetText(FText::Format(NSLOCTEXT("DeckBuilder", "FitsTab", "Fits the {0}   {1}"), SectionName,
	                                      FText::AsNumber(FittingCount)));
	M_EverythingTabLabel->SetText(FText::Format(NSLOCTEXT("DeckBuilder", "EverythingTab", "Everything   {0}"),
	                                            FText::AsNumber(EverythingCount)));
	M_FitsTabButton->SetStyle(bM_ShowsWholeCollection ? M_Look.Tab : M_Look.TabSelected);
	M_EverythingTabButton->SetStyle(bM_ShowsWholeCollection ? M_Look.TabSelected : M_Look.Tab);

	TArray<ERTSCard> ShownCards = bM_ShowsWholeCollection ? M_State.GetCollection() : FittingCards;
	SortCollectionCards(ShownCards);
	const TArray<FUnitVariantSelection>& ShownVariants = bM_ShowsWholeCollection ? AllLooseVariants : FittingVariants;
	const bool bIsTechnologies = M_SelectedSection == EDeckSection::Technologies;
	const FText CardsHeader = bM_ShowsWholeCollection
		                          ? NSLOCTEXT("DeckBuilder", "AllCardsHeader", "UNIT AND TECHNOLOGY CARDS   {0} not slotted")
		                          : bIsTechnologies
		                          ? NSLOCTEXT("DeckBuilder", "TechCardsHeader", "TECHNOLOGIES   {0} not slotted")
		                          : NSLOCTEXT("DeckBuilder", "UnitCardsHeader", "UNITS THE {1} CAN TRAIN   {0} not slotted");
	M_CollectionCardsHeader->SetText(FText::Format(CardsHeader, FText::AsNumber(ShownCards.Num()),
	                                               SectionName.ToUpper()));
	const FText VariantsHeader = bM_ShowsWholeCollection
		                             ? NSLOCTEXT("DeckBuilder", "AllVariantsHeader", "VARIANTS   {0} not attached")
		                             : NSLOCTEXT("DeckBuilder", "VariantsHeader",
		                                         "VARIANTS FOR {1} UNITS   {0} not attached");
	M_CollectionVariantsHeader->SetText(FText::Format(VariantsHeader, FText::AsNumber(ShownVariants.Num()),
	                                                  SectionName.ToUpper()));
	// Technologies carry no variants, so that group is only shown where it can hold something.
	const bool bShowsVariantGroup = bM_ShowsWholeCollection || not bIsTechnologies;
	const ESlateVisibility VariantGroupVisibility = bShowsVariantGroup ? ESlateVisibility::SelfHitTestInvisible
		                                                : ESlateVisibility::Collapsed;
	M_CollectionVariantsHeader->SetVisibility(bShowsVariantGroup ? ESlateVisibility::HitTestInvisible
		                                          : ESlateVisibility::Collapsed);
	M_CollectionVariants->SetVisibility(VariantGroupVisibility);
	const int32 HiddenCount = EverythingCount - FittingCount;
	M_CollectionHintText->SetText(bM_ShowsWholeCollection
		                              ? NSLOCTEXT("DeckBuilder", "HintEverything",
		                                          "Drag a unit card onto a slot of its building, or a variant card onto its unit. "
		                                          "Right-click does the same in one go.")
		                              : FText::Format(NSLOCTEXT("DeckBuilder", "HintFits",
		                                                        "Drag a unit card onto a slot, or a variant card onto its unit. "
		                                                        "Right-click does the same in one go.\n\n{0} more cards "
		                                                        "belong to other sections and are hidden by this filter."),
		                                              FText::AsNumber(HiddenCount)));
	RefreshCollection_Cards(ShownCards);
	RefreshCollection_Variants(ShownVariants);
}

void UW_DeckBuilderMenu::RefreshCollection_Cards(const TArray<ERTSCard>& Cards)
{
	M_CollectionCards->ClearChildren();
	for (const ERTSCard Card : Cards)
	{
		const FDeckCardDefinition* Definition = M_State.FindDefinition(Card);
		UW_DeckCard* const CardWidget = CreateWidget<UW_DeckCard>(this, UW_DeckCard::StaticClass());
		if (not Definition || not IsValid(CardWidget))
		{
			continue;
		}
		EDeckSection CardSection = EDeckSection::Technologies;
		DeckBuilder::GetSectionOfCardType(Definition->CardType, CardSection);
		CardWidget->SetupCard(this, M_Look, *Definition, MakeCardVariantsView(*Definition), EDeckCardPlace::Collection,
		                      CardSection, INDEX_NONE);
		UScaleBox* const ScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		ScaleBox->SetStretch(EStretch::UserSpecified);
		ScaleBox->SetUserSpecifiedScale(DeckBuilderMenuLayout::CollectionCardScale);
		ScaleBox->SetContent(CardWidget);
		M_CollectionCards->AddChildToWrapBox(ScaleBox);
	}
}

void UW_DeckBuilderMenu::RefreshCollection_Variants(const TArray<FUnitVariantSelection>& Variants)
{
	M_CollectionVariants->ClearChildren();
	for (const FUnitVariantSelection& Variant : Variants)
	{
		UW_DeckVariantCard* const VariantCard = CreateWidget<UW_DeckVariantCard>(this, UW_DeckVariantCard::StaticClass());
		if (not IsValid(VariantCard))
		{
			continue;
		}
		VariantCard->SetupVariantCard(this, M_Look, MakeVariantView(Variant), EDeckVariantCardPlace::Collection);
		UScaleBox* const ScaleBox = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		ScaleBox->SetStretch(EStretch::UserSpecified);
		ScaleBox->SetUserSpecifiedScale(DeckBuilderMenuLayout::CollectionCardScale);
		ScaleBox->SetContent(VariantCard);
		M_CollectionVariants->AddChildToWrapBox(ScaleBox);
	}
}

void UW_DeckBuilderMenu::SetDockTexts(const FText& Title, const FText& Subtitle, const FText& Description)
{
	M_DockTitleText->SetText(Title);
	M_DockSubtitleText->SetText(Subtitle);
	M_DockDescriptionText->SetText(Description);
}

void UW_DeckBuilderMenu::RefreshDock()
{
	if (not IsValid(M_DockPreviewHost) || not IsValid(M_DockVariantRows) || not IsValid(M_DockTitleText))
	{
		return;
	}
	M_DockPreviewHost->ClearChildren();
	M_DockVariantRows->ClearChildren();
	M_DockVariantsLabel->SetText(FText::GetEmpty());
	const FDeckCardDefinition* Definition = M_DockSubject.Kind == FDeckDockSubject::EKind::Card
		                                        ? M_State.FindDefinition(M_DockSubject.Card)
		                                        : nullptr;
	if (Definition)
	{
		RefreshDock_Card(*Definition);
		return;
	}
	if (M_DockSubject.Kind == FDeckDockSubject::EKind::Variant)
	{
		RefreshDock_Variant(M_DockSubject.Variant);
		return;
	}
	SetDockTexts(NSLOCTEXT("DeckBuilder", "DockEmptyTitle", "POINT AT A CARD"), FText::GetEmpty(),
	             NSLOCTEXT("DeckBuilder", "DockEmptyText",
	                       "Its full text shows here, and for a unit the variants you own for it."));
}

void UW_DeckBuilderMenu::RefreshDock_Card(const FDeckCardDefinition& Definition)
{
	UW_DeckCard* const Preview = CreateWidget<UW_DeckCard>(this, UW_DeckCard::StaticClass());
	EDeckSection CardSection = EDeckSection::Technologies;
	DeckBuilder::GetSectionOfCardType(Definition.CardType, CardSection);
	if (IsValid(Preview))
	{
		Preview->SetupCard(this, M_Look, Definition, MakeCardVariantsView(Definition), EDeckCardPlace::Preview,
		                   CardSection, INDEX_NONE);
		DeckBuilderWidgets::SetTopLeftContent(*M_DockPreviewHost, Preview);
	}
	const bool bIsTechnology = Definition.CardType == ECardType::Technology;
	const FText Subtitle = bIsTechnology
		                       ? NSLOCTEXT("DeckBuilder", "DockTechnology", "Technology: starts the mission researched")
		                       : FText::Format(NSLOCTEXT("DeckBuilder", "DockUnit", "Trained at the {0}"),
		                                       DeckBuilder::GetSectionDisplayName(CardSection));
	SetDockTexts(UUnitVariantPresentationLibrary::StripRichTextMarkup(Definition.Title).ToUpper(), Subtitle,
	             UUnitVariantPresentationLibrary::StripRichTextMarkup(Definition.Description));
	if (not bIsTechnology)
	{
		RefreshDock_VariantRows(Definition.VariantUnit);
	}
}

void UW_DeckBuilderMenu::RefreshDock_Variant(const FUnitVariantSelection& Variant)
{
	const FDeckVariantCardView View = MakeVariantView(Variant);
	UW_DeckVariantCard* const Preview = CreateWidget<UW_DeckVariantCard>(this, UW_DeckVariantCard::StaticClass());
	if (IsValid(Preview))
	{
		Preview->SetupVariantCard(this, M_Look, View, EDeckVariantCardPlace::Preview);
		DeckBuilderWidgets::SetTopLeftContent(*M_DockPreviewHost, Preview);
	}
	SetDockTexts(View.Name.ToUpper(),
	             FText::Format(NSLOCTEXT("DeckBuilder", "DockVariantFits", "Variant: fits {0}"), View.UnitName),
	             View.Description);
	RefreshDock_VariantRows(Variant.Unit);
}

void UW_DeckBuilderMenu::RefreshDock_VariantRows(const FTrainingOption& Unit)
{
	const TArray<EUnitVariant> OwnedVariants = M_State.GetOwnedVariants(Unit);
	if (OwnedVariants.Num() == 0)
	{
		return;
	}
	M_DockVariantsLabel->SetText(FText::Format(NSLOCTEXT("DeckBuilder", "DockVariantsOwned",
	                                                     "{0} {0}|plural(one=VARIANT,other=VARIANTS) OWNED: CLICK ONE TO EQUIP IT"),
	                                           OwnedVariants.Num()));
	const EUnitVariant Attached = M_State.GetAttachedVariant(Unit);
	TArray<FDeckVariantCardView> Rows;
	Rows.Add(MakeBaseView(Unit));
	for (const EUnitVariant Variant : OwnedVariants)
	{
		FUnitVariantSelection Selection;
		Selection.Unit = Unit;
		Selection.Variant = Variant;
		Rows.Add(MakeVariantView(Selection));
	}
	for (const FDeckVariantCardView& RowView : Rows)
	{
		UW_DeckVariantRow* const Row = CreateWidget<UW_DeckVariantRow>(this, UW_DeckVariantRow::StaticClass());
		if (not IsValid(Row))
		{
			continue;
		}
		Row->SetupRow(this, M_Look, RowView, RowView.Variant.Variant == Attached);
		M_DockVariantRows->AddChildToWrapBox(Row);
	}
}

FSlateBrush UW_DeckBuilderMenu::LoadVariantPicture(const FUnitVariantSelection& Variant)
{
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = DeckBuilderMenuLayout::VariantPictureSize;
	const FUnitVariantDefinition* Definition = IsValid(M_VariantDataAsset)
		                                           ? M_VariantDataAsset->FindVariantDefinition(Variant.Unit, Variant.Variant)
		                                           : nullptr;
	if (not Definition)
	{
		return Brush;
	}
	if (not Definition->Icon.IsNull())
	{
		const FString PictureKey = Definition->Icon.ToString();
		TObjectPtr<UTexture2D>& LoadedPicture = M_LoadedVariantPictures.FindOrAdd(PictureKey);
		if (not IsValid(LoadedPicture))
		{
			LoadedPicture = Definition->Icon.LoadSynchronous();
		}
		if (IsValid(LoadedPicture))
		{
			Brush.SetResourceObject(LoadedPicture);
			return Brush;
		}
	}
	// No selection picture: the picture the training menu shows for the variant, when it has its own.
	if (Definition->bOverrideTrainingImageBrush && IsValid(Definition->TrainingImageBrush))
	{
		return Definition->TrainingImageBrush->Brush;
	}
	return Brush;
}

FDeckVariantCardView UW_DeckBuilderMenu::MakeVariantView(const FUnitVariantSelection& Variant)
{
	// The letter the hangar screen shows: VariantA is 'A'.
	const int32 VariantLetterIndex = static_cast<int32>(Variant.Variant) - static_cast<int32>(EUnitVariant::VariantA);
	const FString VariantLetter = FString::Chr(static_cast<TCHAR>(TEXT('A') + FMath::Max(0, VariantLetterIndex)));
	// Variants have no rarity of their own yet; the letter picks the spine paint so a unit's variants differ.
	const ERTSCardRarity RarityByLetter[] = {
		ERTSCardRarity::Rarity_UnCommon, ERTSCardRarity::Rarity_Rare, ERTSCardRarity::Rarity_VeryRare
	};
	FDeckVariantCardView View;
	View.Variant = Variant;
	View.Label = FText::Format(NSLOCTEXT("DeckBuilder", "VariantLabel", "VARIANT {0}"),
	                           FText::FromString(VariantLetter));
	View.Rarity = RarityByLetter[FMath::Clamp(VariantLetterIndex, 0, static_cast<int32>(UE_ARRAY_COUNT(RarityByLetter)) - 1)];
	View.Name = View.Label;
	if (IsValid(M_VariantDataAsset))
	{
		if (const FUnitVariantDefinition* Definition = M_VariantDataAsset->FindVariantDefinition(Variant.Unit, Variant.Variant))
		{
			View.Name = Definition->DisplayName.IsEmpty() ? View.Label : Definition->DisplayName;
			View.Description = Definition->Description;
		}
	}
	View.Picture = LoadVariantPicture(Variant);
	View.UnitName = UUnitVariantPresentationLibrary::GetUnitDisplayNameFallback(Variant.Unit);
	const FDeckCardDefinition* UnitCard = IsValid(M_Catalog) ? M_Catalog->FindDefinitionOfUnit(Variant.Unit) : nullptr;
	if (UnitCard)
	{
		View.UnitName = UUnitVariantPresentationLibrary::StripRichTextMarkup(UnitCard->Title);
		View.UnitPicture = UnitCard->ImageBrush;
	}
	if (not View.Picture.GetResourceObject())
	{
		View.Picture = View.UnitPicture;
	}
	return View;
}

FDeckVariantCardView UW_DeckBuilderMenu::MakeBaseView(const FTrainingOption& Unit)
{
	FUnitVariantSelection BaseSelection;
	BaseSelection.Unit = Unit;
	BaseSelection.Variant = EUnitVariant::Base;
	FDeckVariantCardView View = MakeVariantView(BaseSelection);
	View.Label = NSLOCTEXT("DeckBuilder", "BaseLabel", "BASE");
	View.Name = View.UnitName;
	if (View.Description.IsEmpty())
	{
		View.Description = NSLOCTEXT("DeckBuilder", "BaseDescription", "The standard configuration.");
	}
	return View;
}

FDeckCardVariantsView UW_DeckBuilderMenu::MakeCardVariantsView(const FDeckCardDefinition& Definition)
{
	FDeckCardVariantsView VariantsView;
	if (Definition.CardType == ECardType::Technology || Definition.VariantUnit.IsNone())
	{
		return VariantsView;
	}
	const TArray<EUnitVariant> OwnedVariants = M_State.GetOwnedVariants(Definition.VariantUnit);
	const EUnitVariant Attached = M_State.GetAttachedVariant(Definition.VariantUnit);
	if (Attached == EUnitVariant::Base)
	{
		VariantsView.AvailableVariantCount = OwnedVariants.Num();
		return VariantsView;
	}
	FUnitVariantSelection Selection;
	Selection.Unit = Definition.VariantUnit;
	Selection.Variant = Attached;
	VariantsView.ClippedVariants.Add(MakeVariantView(Selection));
	for (const EUnitVariant Variant : OwnedVariants)
	{
		if (Variant == Attached)
		{
			continue;
		}
		Selection.Variant = Variant;
		VariantsView.ClippedVariants.Add(MakeVariantView(Selection));
	}
	return VariantsView;
}

FPlayerCardSaveData UW_DeckBuilderMenu::BuildCardDataToSave() const
{
	FPlayerCardSaveData Unsaved = M_State.BuildSaveData();
	// The same normaliser the old card menu saved through, so both menus leave the same shape behind.
	FPlayerCardSaveData CardData = FRTS_Profile::BuildPlayerCardSaveData(
		MoveTemp(Unsaved.PlayerAvailableCards), MoveTemp(Unsaved.SelectedCards), Unsaved.MaxCardsInUnits,
		Unsaved.MaxCardsInTechResources, MoveTemp(Unsaved.NomadicBuildingLayouts), MoveTemp(Unsaved.CardsInUnits),
		MoveTemp(Unsaved.CardsInTechResources), MoveTemp(Unsaved.UnitVariantSelections));
	CardData.OwnedUnitVariants = MoveTemp(Unsaved.OwnedUnitVariants);
	return CardData;
}

void UW_DeckBuilderMenu::OnSaveClicked()
{
	if (not GetIsValidWorldMenu())
	{
		return;
	}
	M_WorldMenu->HandleDeckBuilderSaved(BuildCardDataToSave());
}

void UW_DeckBuilderMenu::OnFitsTabClicked()
{
	bM_ShowsWholeCollection = false;
	RefreshCollection();
}

void UW_DeckBuilderMenu::OnEverythingTabClicked()
{
	bM_ShowsWholeCollection = true;
	RefreshCollection();
}

void UW_DeckBuilderMenu::OnSortByRarityClicked()
{
	M_CollectionSort = M_CollectionSort == EDeckCollectionSort::ByRarity ? EDeckCollectionSort::AsOwned
		                   : EDeckCollectionSort::ByRarity;
	RefreshCollection();
}

void UW_DeckBuilderMenu::OnSortByTypeClicked()
{
	M_CollectionSort = M_CollectionSort == EDeckCollectionSort::ByType ? EDeckCollectionSort::AsOwned
		                   : EDeckCollectionSort::ByType;
	RefreshCollection();
}

namespace DeckBuilderMenuDebug
{
	/** The card widgets under a panel, in the order they were added to it. */
	TArray<UW_DeckCard*> FindCardWidgets(UWidget* Panel)
	{
		TArray<UWidget*> Widgets;
		UWidgetTree::GetChildWidgets(Panel, Widgets);
		TArray<UW_DeckCard*> CardWidgets;
		for (UWidget* const Widget : Widgets)
		{
			if (UW_DeckCard* const CardWidget = Cast<UW_DeckCard>(Widget))
			{
				CardWidgets.Add(CardWidget);
			}
		}
		return CardWidgets;
	}
}

bool UW_DeckBuilderMenu::Debug_ReturnFirstSlottedCard()
{
	for (const UW_DeckCard* const CardWidget : DeckBuilderMenuDebug::FindCardWidgets(M_SlotGrid))
	{
		if (CardWidget->GetIsEmptySlot())
		{
			continue;
		}
		const int32 CollectionCountBefore = M_State.GetCollection().Num();
		// What a right-click on the slotted card calls.
		HandleCardRightClicked(*CardWidget);
		return M_State.GetCollection().Num() > CollectionCountBefore;
	}
	return false;
}

bool UW_DeckBuilderMenu::Debug_SlotFirstFittingCard()
{
	const TArray<UW_DeckCard*> CollectionWidgets = DeckBuilderMenuDebug::FindCardWidgets(M_CollectionCards);
	const UW_DeckCard* EmptySlot = nullptr;
	for (const UW_DeckCard* const SlotWidget : DeckBuilderMenuDebug::FindCardWidgets(M_SlotGrid))
	{
		if (SlotWidget->GetIsEmptySlot())
		{
			EmptySlot = SlotWidget;
			break;
		}
	}
	if (CollectionWidgets.Num() == 0 || not EmptySlot)
	{
		return false;
	}
	// The drag a collection card starts, dropped on the first empty slot.
	UDeckCardDragOperation* const Drag = NewObject<UDeckCardDragOperation>(this);
	Drag->bIsVariant = false;
	Drag->Card = CollectionWidgets[0]->GetCard();
	Drag->bIsFromSlot = false;
	Drag->Section = CollectionWidgets[0]->GetSection();
	Drag->SlotIndex = CollectionWidgets[0]->GetSlotIndex();
	const ERTSCard Card = Drag->Card;
	if (not HandleDropOnCard(*Drag, *EmptySlot))
	{
		return false;
	}
	HandleCardHovered(Card);
	return true;
}

bool UW_DeckBuilderMenu::Debug_AttachFirstLooseVariant()
{
	const TArray<FUnitVariantSelection> LooseVariants = M_State.GetLooseVariantsForSection(M_SelectedSection);
	if (LooseVariants.Num() == 0)
	{
		return false;
	}
	// The drag a loose variant card starts, dropped on the slotted card of its unit.
	UDeckCardDragOperation* const Drag = NewObject<UDeckCardDragOperation>(this);
	Drag->bIsVariant = true;
	Drag->Variant = LooseVariants[0];
	for (const UW_DeckCard* const SlotWidget : DeckBuilderMenuDebug::FindCardWidgets(M_SlotGrid))
	{
		if (not GetIsDropAllowed(*Drag, *SlotWidget))
		{
			continue;
		}
		const ERTSCard UnitCard = SlotWidget->GetCard();
		if (not HandleDropOnCard(*Drag, *SlotWidget))
		{
			return false;
		}
		HandleCardHovered(UnitCard);
		return true;
	}
	return false;
}

void UW_DeckBuilderMenu::Debug_ShowWholeCollection(const bool bShowsWholeCollection)
{
	bM_ShowsWholeCollection = bShowsWholeCollection;
	RefreshCollection();
}

FString UW_DeckBuilderMenu::Debug_DescribeState() const
{
	FString Description = FString::Printf(TEXT("selected=%s collection=%d loose_variants=%d"),
	                                      *DeckBuilder::GetSectionDisplayName(M_SelectedSection).ToString(),
	                                      M_State.GetCollection().Num(), M_State.GetLooseVariants().Num());
	for (const EDeckSection Section : DeckBuilder::GetAllSections())
	{
		const FDeckSectionState& SectionState = M_State.GetSection(Section);
		Description += FString::Printf(TEXT(" | %s %d/%d%s attached=%d"),
		                               *DeckBuilder::GetSectionDisplayName(Section).ToString(),
		                               SectionState.Cards.Num(), SectionState.SlotCount,
		                               SectionState.bIsAvailable ? TEXT("") : TEXT(" (unavailable)"),
		                               M_State.GetAttachedVariantCount(Section));
	}
	return Description;
}

bool UW_DeckBuilderMenu::GetIsValidWorldMenu() const
{
	if (M_WorldMenu.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_WorldMenu"), TEXT("GetIsValidWorldMenu"),
	                                                            this);
	return false;
}

bool UW_DeckBuilderMenu::GetIsValidCatalog() const
{
	if (IsValid(M_Catalog))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_Catalog"), TEXT("GetIsValidCatalog"),
	                                                            this);
	return false;
}

bool UW_DeckBuilderMenu::GetIsValidVariantDataAsset() const
{
	if (IsValid(M_VariantDataAsset))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_VariantDataAsset"),
	                                                            TEXT("GetIsValidVariantDataAsset"), this);
	return false;
}
