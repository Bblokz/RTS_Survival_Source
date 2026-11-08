// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CommanderSelectionMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/FactionSystem/FactionSelection/FactionPlayerController.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_CommanderAbilityRow.h"
#include "W_CommanderSelectionEntry.h"

// 1920x1080 reference; the side columns use the variant screen's panel art at the sizes it was drawn for.
namespace CommanderSelectionLayout
{
	constexpr float ScreenMargin = 40.f;
	constexpr float EdgeShadeWidth = 640.f;
	constexpr float HeaderTop = 24.f;
	const FVector2D HeaderSize(640.f, 80.f);
	const FMargin HeaderPadding(40.f, 4.f, 40.f, 12.f);
	constexpr float PanelTop = 120.f;
	const FVector2D CommanderPanelSize(392.f, 848.f);
	const FVector2D KitPanelSize(480.f, 848.f);
	constexpr float PanelHeaderHeight = 52.f;
	const FMargin PanelTitlePadding(32.f, 0.f, 0.f, 0.f);
	const FMargin CommanderListPadding(20.f, 64.f, 20.f, 20.f);
	const FMargin AbilityColumnPadding(20.f, 68.f, 20.f, 20.f);
	// Twelve 60 unit entries with this spacing fill the 764 unit list (756 units) without a scroll bar.
	constexpr float EntrySpacing = 3.f;
	constexpr float AbilityRowSpacing = 10.f;
	// The centre column sits between the two side columns: (432 + 1400) / 2 is 44 left of the screen centre.
	constexpr float CentreColumnOffsetX = -44.f;
	const FVector2D PortraitSize(432.f, 540.f);
	constexpr float PortraitFramePadding = 4.f;
	constexpr float DescriptionTop = PanelTop + 540.f + 16.f;
	// Header, side panels, portrait and description hang from the vertical centre (at 1080 high exactly where their
	// top offsets put them), so a taller screen shares its extra height above and below them; the buttons keep to the
	// bottom corners.
	constexpr float ColumnAnchorY = 0.5f;
	constexpr float DescriptionWidth = 620.f;
	const FMargin DescriptionPlatePadding(28.f, 14.f, 28.f, 18.f);
	const FMargin CommanderDescriptionPadding(0.f, 6.f, 0.f, 0.f);
	const FVector2D BackButtonSize(240.f, 48.f);
	const FVector2D ConfirmButtonSize(280.f, 48.f);
	constexpr int32 TitleFontSize = 28;
	constexpr int32 TitleLetterSpacing = 150;
	constexpr int32 PanelTitleFontSize = 18;
	constexpr int32 PanelTitleLetterSpacing = 100;
	constexpr int32 CommanderNameFontSize = 26;
	constexpr int32 CommanderNameLetterSpacing = 120;
	constexpr int32 CommanderDescriptionFontSize = 16;
	constexpr int32 PortraitPendingFontSize = 18;
	const FLinearColor PortraitFrameColor(0.02f, 0.025f, 0.03f, 0.92f);
	const FLinearColor PortraitOutlineColor(0.25f, 0.27f, 0.27f, 1.f);
	constexpr float PortraitOutlineWidth = 2.f;
	const FLinearColor DescriptionPlateColor(0.f, 0.f, 0.f, 0.62f);

	/** Canvas offset from the centre line of a column widget whose top sits at TopAt1080 on a 1080 high screen. */
	float GetColumnOffsetY(const float TopAt1080)
	{
		return TopAt1080 - MenuDesignFrameLayout::DesignSize.Y * ColumnAnchorY;
	}
}

void UW_CommanderSelectionMenu::NativeOnInitialized()
{
	// Keyboard focus stays on the screen itself (its buttons never take it), for the arrow key navigation.
	SetIsFocusable(true);
	Super::NativeOnInitialized();
	BuildWidgetTree();
	ApplyStyle();
}

void UW_CommanderSelectionMenu::NativeDestruct()
{
	if (M_PortraitLoadHandle.IsValid())
	{
		M_PortraitLoadHandle->CancelHandle();
		M_PortraitLoadHandle.Reset();
	}
	Super::NativeDestruct();
}

void UW_CommanderSelectionMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	M_DesignFrame.UpdateFrame(MyGeometry);
}

void UW_CommanderSelectionMenu::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	UCanvasPanel* const ScreenCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = ScreenCanvas;
	UCanvasPanel* const RootCanvas = M_DesignFrame.BuildFrame(*WidgetTree, *ScreenCanvas);
	if (not IsValid(RootCanvas))
	{
		return;
	}
	BuildEdgeShades(RootCanvas);
	BuildHeader(RootCanvas);
	BuildCommanderPanel(RootCanvas);
	BuildKitPanel(RootCanvas);
	BuildPortrait(RootCanvas);
	BuildDescription(RootCanvas);
	BuildButtons(RootCanvas);
}

void UW_CommanderSelectionMenu::BuildEdgeShades(UCanvasPanel* RootCanvas)
{
	using namespace CommanderSelectionLayout;
	M_EdgeShadeLeft = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_EdgeShadeLeft->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* const LeftSlot = RootCanvas->AddChildToCanvas(M_EdgeShadeLeft);
	LeftSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
	LeftSlot->SetOffsets(FMargin(0.f, 0.f, EdgeShadeWidth, 0.f));

	M_EdgeShadeRight = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_EdgeShadeRight->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* const RightSlot = RootCanvas->AddChildToCanvas(M_EdgeShadeRight);
	RightSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
	RightSlot->SetOffsets(FMargin(-EdgeShadeWidth, 0.f, EdgeShadeWidth, 0.f));
}

void UW_CommanderSelectionMenu::BuildHeader(UCanvasPanel* RootCanvas)
{
	using namespace CommanderSelectionLayout;
	M_HeaderPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_HeaderPlate->SetPadding(HeaderPadding);
	M_HeaderPlate->SetHorizontalAlignment(HAlign_Center);
	M_HeaderPlate->SetVerticalAlignment(VAlign_Center);
	UCanvasPanelSlot* const HeaderSlot = RootCanvas->AddChildToCanvas(M_HeaderPlate);
	HeaderSlot->SetAnchors(FAnchors(0.5f, ColumnAnchorY));
	HeaderSlot->SetAlignment(FVector2D(0.5f, 0.f));
	HeaderSlot->SetPosition(FVector2D(0.f, GetColumnOffsetY(HeaderTop)));
	HeaderSlot->SetSize(HeaderSize);

	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TitleText->SetText(NSLOCTEXT("CommanderSelection", "Title", "COMMANDER"));
	M_HeaderPlate->SetContent(M_TitleText);
}

UOverlay* UW_CommanderSelectionMenu::BuildSidePanel(UCanvasPanel* RootCanvas, const FVector2D& PanelSize,
                                                    UCanvasPanelSlot*& OutPanelSlot,
                                                    TObjectPtr<UTextBlock>& OutTitleText,
                                                    TObjectPtr<UBorder>& OutPanelPlate)
{
	using namespace CommanderSelectionLayout;
	USizeBox* const PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(PanelSize.X);
	PanelBox->SetHeightOverride(PanelSize.Y);
	OutPanelSlot = RootCanvas->AddChildToCanvas(PanelBox);
	OutPanelSlot->SetAutoSize(true);

	OutPanelPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	OutPanelPlate->SetPadding(FMargin(0.f));
	PanelBox->SetContent(OutPanelPlate);
	UOverlay* const PanelOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	OutPanelPlate->SetContent(PanelOverlay);

	USizeBox* const HeaderStrip = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HeaderStrip->SetHeightOverride(PanelHeaderHeight);
	UOverlaySlot* const StripSlot = PanelOverlay->AddChildToOverlay(HeaderStrip);
	StripSlot->SetHorizontalAlignment(HAlign_Fill);
	StripSlot->SetVerticalAlignment(VAlign_Top);
	UHorizontalBox* const HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HeaderStrip->SetContent(HeaderRow);

	OutTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	UHorizontalBoxSlot* const TitleSlot = HeaderRow->AddChildToHorizontalBox(OutTitleText);
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleSlot->SetPadding(PanelTitlePadding);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	return PanelOverlay;
}

void UW_CommanderSelectionMenu::BuildCommanderPanel(UCanvasPanel* RootCanvas)
{
	using namespace CommanderSelectionLayout;
	UCanvasPanelSlot* PanelSlot = nullptr;
	UOverlay* const PanelOverlay = BuildSidePanel(RootCanvas, CommanderPanelSize, PanelSlot, M_CommanderPanelTitle,
	                                              M_CommanderPanel);
	PanelSlot->SetAnchors(FAnchors(0.f, ColumnAnchorY));
	PanelSlot->SetPosition(FVector2D(ScreenMargin, GetColumnOffsetY(PanelTop)));
	// The list's title sits centred in the header strip.
	if (UHorizontalBoxSlot* const TitleSlot = Cast<UHorizontalBoxSlot>(M_CommanderPanelTitle->Slot))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.f));
	}
	M_CommanderPanelTitle->SetText(NSLOCTEXT("CommanderSelection", "CommandersTitle", "COMMANDERS"));

	M_CommanderList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UOverlaySlot* const ListSlot = PanelOverlay->AddChildToOverlay(M_CommanderList);
	ListSlot->SetHorizontalAlignment(HAlign_Fill);
	ListSlot->SetVerticalAlignment(VAlign_Fill);
	ListSlot->SetPadding(CommanderListPadding);
}

void UW_CommanderSelectionMenu::BuildKitPanel(UCanvasPanel* RootCanvas)
{
	using namespace CommanderSelectionLayout;
	UCanvasPanelSlot* PanelSlot = nullptr;
	UOverlay* const PanelOverlay = BuildSidePanel(RootCanvas, KitPanelSize, PanelSlot, M_KitPanelTitle, M_KitPanel);
	PanelSlot->SetAnchors(FAnchors(1.f, ColumnAnchorY));
	PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
	PanelSlot->SetPosition(FVector2D(-ScreenMargin, GetColumnOffsetY(PanelTop)));
	M_KitPanelTitle->SetText(NSLOCTEXT("CommanderSelection", "KitTitle", "GLOBAL ABILITIES"));

	// Its scroll bar only shows once long descriptions need more than the panel's height.
	UScrollBox* const AbilityScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	UOverlaySlot* const ScrollSlot = PanelOverlay->AddChildToOverlay(AbilityScrollBox);
	ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
	ScrollSlot->SetVerticalAlignment(VAlign_Fill);
	ScrollSlot->SetPadding(AbilityColumnPadding);
	M_AbilityColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	AbilityScrollBox->AddChild(M_AbilityColumn);
}

void UW_CommanderSelectionMenu::BuildPortrait(UCanvasPanel* RootCanvas)
{
	using namespace CommanderSelectionLayout;
	USizeBox* const PortraitBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PortraitBox->SetWidthOverride(PortraitSize.X);
	PortraitBox->SetHeightOverride(PortraitSize.Y);
	UCanvasPanelSlot* const PortraitSlot = RootCanvas->AddChildToCanvas(PortraitBox);
	PortraitSlot->SetAnchors(FAnchors(0.5f, ColumnAnchorY));
	PortraitSlot->SetAlignment(FVector2D(0.5f, 0.f));
	PortraitSlot->SetPosition(FVector2D(CentreColumnOffsetX, GetColumnOffsetY(PanelTop)));
	PortraitSlot->SetAutoSize(true);

	M_PortraitFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_PortraitFrame->SetPadding(FMargin(PortraitFramePadding));
	M_PortraitFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
	PortraitBox->SetContent(M_PortraitFrame);
	UOverlay* const PortraitOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	M_PortraitFrame->SetContent(PortraitOverlay);
	M_PortraitImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_PortraitImage->SetVisibility(ESlateVisibility::Hidden);
	UOverlaySlot* const ImageSlot = PortraitOverlay->AddChildToOverlay(M_PortraitImage);
	ImageSlot->SetHorizontalAlignment(HAlign_Fill);
	ImageSlot->SetVerticalAlignment(VAlign_Fill);
	M_PortraitPendingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_PortraitPendingText->SetText(NSLOCTEXT("CommanderSelection", "PortraitPending", "PORTRAIT PENDING"));
	UOverlaySlot* const PendingSlot = PortraitOverlay->AddChildToOverlay(M_PortraitPendingText);
	PendingSlot->SetHorizontalAlignment(HAlign_Center);
	PendingSlot->SetVerticalAlignment(VAlign_Center);
}

void UW_CommanderSelectionMenu::BuildDescription(UCanvasPanel* RootCanvas)
{
	using namespace CommanderSelectionLayout;
	USizeBox* const DescriptionBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DescriptionBox->SetWidthOverride(DescriptionWidth);
	UCanvasPanelSlot* const DescriptionSlot = RootCanvas->AddChildToCanvas(DescriptionBox);
	DescriptionSlot->SetAnchors(FAnchors(0.5f, ColumnAnchorY));
	DescriptionSlot->SetAlignment(FVector2D(0.5f, 0.f));
	DescriptionSlot->SetPosition(FVector2D(CentreColumnOffsetX, GetColumnOffsetY(DescriptionTop)));
	DescriptionSlot->SetAutoSize(true);

	M_DescriptionPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_DescriptionPlate->SetPadding(DescriptionPlatePadding);
	M_DescriptionPlate->SetBrushColor(DescriptionPlateColor);
	M_DescriptionPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	DescriptionBox->SetContent(M_DescriptionPlate);
	UVerticalBox* const DescriptionColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_DescriptionPlate->SetContent(DescriptionColumn);
	M_CommanderNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_CommanderNameText->SetJustification(ETextJustify::Center);
	DescriptionColumn->AddChildToVerticalBox(M_CommanderNameText)->SetHorizontalAlignment(HAlign_Fill);
	M_CommanderDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_CommanderDescriptionText->SetAutoWrapText(true);
	M_CommanderDescriptionText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* const BodySlot = DescriptionColumn->AddChildToVerticalBox(M_CommanderDescriptionText);
	BodySlot->SetHorizontalAlignment(HAlign_Fill);
	BodySlot->SetPadding(CommanderDescriptionPadding);
}

void UW_CommanderSelectionMenu::BuildButtons(UCanvasPanel* RootCanvas)
{
	using namespace CommanderSelectionLayout;
	UWidget* const BackBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style, NSLOCTEXT("CommanderSelection", "Back", "BACK"), M_Style.BackButton, BackButtonSize,
		M_BackButton);
	M_BackButton->OnClicked.AddDynamic(this, &UW_CommanderSelectionMenu::OnBackClicked);
	UCanvasPanelSlot* const BackSlot = RootCanvas->AddChildToCanvas(BackBox);
	BackSlot->SetAnchors(FAnchors(0.f, 1.f));
	BackSlot->SetAlignment(FVector2D(0.f, 1.f));
	BackSlot->SetPosition(FVector2D(ScreenMargin, -ScreenMargin));
	BackSlot->SetAutoSize(true);

	UWidget* const ConfirmBox = UnitVariantSelectionWidgets::BuildTextButton(
		*WidgetTree, M_Style, NSLOCTEXT("CommanderSelection", "Confirm", "CONFIRM"), M_Style.ConfirmButton,
		ConfirmButtonSize, M_ConfirmButton);
	M_ConfirmButton->OnClicked.AddDynamic(this, &UW_CommanderSelectionMenu::OnConfirmClicked);
	UCanvasPanelSlot* const ConfirmSlot = RootCanvas->AddChildToCanvas(ConfirmBox);
	ConfirmSlot->SetAnchors(FAnchors(1.f, 1.f));
	ConfirmSlot->SetAlignment(FVector2D(1.f, 1.f));
	ConfirmSlot->SetPosition(FVector2D(-ScreenMargin, -ScreenMargin));
	ConfirmSlot->SetAutoSize(true);
}

void UW_CommanderSelectionMenu::ApplyStyle()
{
	using namespace CommanderSelectionLayout;
	using namespace UnitVariantSelectionWidgets;
	if (not IsValid(M_HeaderPlate) || not IsValid(M_CommanderPanel) || not IsValid(M_KitPanel)
		|| not GetIsValidPortraitImage())
	{
		return;
	}
	M_EdgeShadeLeft->SetBrush(M_Style.EdgeShade);
	FSlateBrush MirroredEdgeShade = M_Style.EdgeShade;
	MirroredEdgeShade.Mirroring = ESlateBrushMirrorType::Horizontal;
	M_EdgeShadeRight->SetBrush(MirroredEdgeShade);
	M_HeaderPlate->SetBrush(M_Style.HeaderPlate);
	M_CommanderPanel->SetBrush(M_Style.UnitListPanel);
	M_KitPanel->SetBrush(M_Style.VariantPanel);

	// A thin dark frame keeps the portrait's own background from bleeding into the hangar behind it.
	FSlateBrush FrameBrush;
	FrameBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	FrameBrush.TintColor = FSlateColor(PortraitFrameColor);
	FrameBrush.OutlineSettings = FSlateBrushOutlineSettings(FVector4(0.f, 0.f, 0.f, 0.f),
	                                                        FSlateColor(PortraitOutlineColor), PortraitOutlineWidth);
	M_PortraitFrame->SetBrush(FrameBrush);

	const FSlateFontInfo PanelTitleFont = MakeFont(M_Style.HeadingFont, PanelTitleFontSize, PanelTitleLetterSpacing);
	ApplyTextOnMetal(M_TitleText, MakeFont(M_Style.HeadingFont, TitleFontSize, TitleLetterSpacing), M_Style.TextColor);
	ApplyTextOnMetal(M_CommanderPanelTitle, PanelTitleFont, M_Style.TextColor);
	ApplyTextOnMetal(M_KitPanelTitle, PanelTitleFont, M_Style.TextColor);
	ApplyTextOnMetal(M_PortraitPendingText, MakeFont(M_Style.HeadingFont, PortraitPendingFontSize,
	                                                 PanelTitleLetterSpacing), M_Style.MutedTextColor);
	ApplyTextOnMetal(M_CommanderNameText, MakeFont(M_Style.HeadingFont, CommanderNameFontSize,
	                                               CommanderNameLetterSpacing), M_Style.SelectedTextColor);
	ApplyTextOnMetal(M_CommanderDescriptionText, MakeFont(M_Style.BodyFont, CommanderDescriptionFontSize),
	                 M_Style.DescriptionColor);
}

void UW_CommanderSelectionMenu::InitCommanderSelection(AFactionPlayerController* FactionPlayerController,
                                                       const ERTSCommander CurrentCommander)
{
	M_FactionPlayerController = FactionPlayerController;
	LoadAbilityData();
	M_Commanders = CommanderSelectionData::BuildCommanderEntries(M_AbilityDataAsset);
	if (M_Commanders.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"The commander selection found no commanders in UCommanderSettings (DefaultGame.ini)."
			"\nAt function: UW_CommanderSelectionMenu::InitCommanderSelection");
	}
	PopulateEntries();
	LoadPortraits();
	if (not SelectCommander(CurrentCommander))
	{
		SelectEntry(0);
	}
}

void UW_CommanderSelectionMenu::LoadAbilityData()
{
	const URTSGlobalAbilitySettings* const GlobalAbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	if (IsValid(GlobalAbilitySettings))
	{
		M_AbilityDataAsset = GlobalAbilitySettings->GlobalAbilityDataAsset.LoadSynchronous();
	}
	if (not IsValid(M_AbilityDataAsset))
	{
		RTSFunctionLibrary::ReportError(
			"The commander selection could not load the global ability data asset (GlobalAbilitySettings); every "
			"ability shows as a placeholder.\nAt function: UW_CommanderSelectionMenu::LoadAbilityData");
	}
	M_RichTextStyle = M_Style.AbilityRichTextStyle.LoadSynchronous();
}

void UW_CommanderSelectionMenu::PopulateEntries()
{
	using namespace CommanderSelectionLayout;
	if (not IsValid(M_CommanderList))
	{
		return;
	}
	M_CommanderList->ClearChildren();
	M_Entries.Reset();
	for (int32 CommanderIndex = 0; CommanderIndex < M_Commanders.Num(); ++CommanderIndex)
	{
		UW_CommanderSelectionEntry* const Entry = CreateWidget<UW_CommanderSelectionEntry>(
			this, UW_CommanderSelectionEntry::StaticClass());
		if (not IsValid(Entry))
		{
			continue;
		}
		Entry->SetupEntry(this, CommanderIndex, M_Style, M_Commanders[CommanderIndex]);
		if (UScrollBoxSlot* const EntrySlot = Cast<UScrollBoxSlot>(M_CommanderList->AddChild(Entry)))
		{
			EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, EntrySpacing));
		}
		M_Entries.Add(Entry);
	}
}

void UW_CommanderSelectionMenu::LoadPortraits()
{
	M_LoadedPortraits.Init(nullptr, M_Commanders.Num());
	TArray<FSoftObjectPath> PortraitPaths;
	for (const FCommanderSelectionEntryData& Commander : M_Commanders)
	{
		if (not Commander.Portrait.IsNull())
		{
			PortraitPaths.AddUnique(Commander.Portrait.ToSoftObjectPath());
		}
	}
	if (PortraitPaths.IsEmpty())
	{
		RefreshPortrait();
		return;
	}
	TWeakObjectPtr<UW_CommanderSelectionMenu> WeakThis(this);
	M_PortraitLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		PortraitPaths, FStreamableDelegate::CreateLambda([WeakThis]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->HandlePortraitsLoaded();
			}
		}));
}

void UW_CommanderSelectionMenu::HandlePortraitsLoaded()
{
	for (int32 CommanderIndex = 0; CommanderIndex < M_Commanders.Num(); ++CommanderIndex)
	{
		UTexture2D* const Portrait = M_Commanders[CommanderIndex].Portrait.Get();
		M_LoadedPortraits[CommanderIndex] = Portrait;
		if (M_Entries.IsValidIndex(CommanderIndex) && IsValid(M_Entries[CommanderIndex]))
		{
			M_Entries[CommanderIndex]->SetPortrait(Portrait);
		}
	}
	M_PortraitLoadHandle.Reset();
	RefreshPortrait();
}

void UW_CommanderSelectionMenu::HandleCommanderEntryClicked(const int32 EntryIndex)
{
	SelectEntry(EntryIndex);
}

bool UW_CommanderSelectionMenu::SelectCommander(const ERTSCommander Commander)
{
	const int32 EntryIndex = M_Commanders.IndexOfByPredicate([Commander](const FCommanderSelectionEntryData& Entry)
	{
		return Entry.Commander == Commander;
	});
	if (EntryIndex == INDEX_NONE)
	{
		return false;
	}
	SelectEntry(EntryIndex);
	return true;
}

ERTSCommander UW_CommanderSelectionMenu::GetSelectedCommander() const
{
	if (not M_Commanders.IsValidIndex(M_SelectedIndex))
	{
		return ERTSCommander::NoCommander;
	}
	return M_Commanders[M_SelectedIndex].Commander;
}

void UW_CommanderSelectionMenu::SelectEntry(const int32 EntryIndex)
{
	if (not M_Commanders.IsValidIndex(EntryIndex))
	{
		return;
	}
	M_SelectedIndex = EntryIndex;
	for (int32 Index = 0; Index < M_Entries.Num(); ++Index)
	{
		if (IsValid(M_Entries[Index]))
		{
			M_Entries[Index]->SetIsSelected(Index == EntryIndex);
		}
	}
	if (M_Entries.IsValidIndex(EntryIndex) && IsValid(M_CommanderList))
	{
		M_CommanderList->ScrollWidgetIntoView(M_Entries[EntryIndex], false);
	}
	const FCommanderSelectionEntryData& Commander = M_Commanders[EntryIndex];
	M_CommanderNameText->SetText(Commander.Title.ToUpper());
	M_CommanderDescriptionText->SetText(Commander.Description);
	RefreshPortrait();
	RebuildAbilityRows();
}

void UW_CommanderSelectionMenu::RefreshPortrait()
{
	if (not GetIsValidPortraitImage() || not M_LoadedPortraits.IsValidIndex(M_SelectedIndex))
	{
		return;
	}
	UTexture2D* const Portrait = M_LoadedPortraits[M_SelectedIndex];
	const bool bHasPortrait = IsValid(Portrait);
	if (bHasPortrait)
	{
		M_PortraitImage->SetBrushFromTexture(Portrait);
	}
	// An empty brush would draw Slate's default white box.
	M_PortraitImage->SetVisibility(bHasPortrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	// While the async load runs the frame stays empty instead of flashing the pending label.
	const bool bIsStillLoading = M_PortraitLoadHandle.IsValid() && M_PortraitLoadHandle->IsLoadingInProgress();
	M_PortraitPendingText->SetVisibility(bHasPortrait || bIsStillLoading
		                                     ? ESlateVisibility::Collapsed
		                                     : ESlateVisibility::HitTestInvisible);
}

void UW_CommanderSelectionMenu::RebuildAbilityRows()
{
	using namespace CommanderSelectionLayout;
	if (not GetIsValidAbilityColumn() || not M_Commanders.IsValidIndex(M_SelectedIndex))
	{
		return;
	}
	M_AbilityColumn->ClearChildren();
	const TArray<FCommanderSelectionAbilitySlot>& AbilitySlots = M_Commanders[M_SelectedIndex].AbilitySlots;
	for (int32 SlotIndex = 0; SlotIndex < AbilitySlots.Num(); ++SlotIndex)
	{
		UW_CommanderAbilityRow* const AbilityRow = CreateWidget<UW_CommanderAbilityRow>(
			this, UW_CommanderAbilityRow::StaticClass());
		if (not IsValid(AbilityRow))
		{
			continue;
		}
		AbilityRow->SetupRow(this, SlotIndex, M_Style, M_RichTextStyle, AbilitySlots[SlotIndex]);
		const float BottomSpacing = SlotIndex + 1 < AbilitySlots.Num() ? AbilityRowSpacing : 0.f;
		M_AbilityColumn->AddChildToVerticalBox(AbilityRow)->SetPadding(FMargin(0.f, 0.f, 0.f, BottomSpacing));
	}
}

UW_CommanderAbilityRow* UW_CommanderSelectionMenu::GetAbilityRow(const int32 SlotIndex) const
{
	if (not IsValid(M_AbilityColumn) || SlotIndex < 0 || SlotIndex >= M_AbilityColumn->GetChildrenCount())
	{
		return nullptr;
	}
	return Cast<UW_CommanderAbilityRow>(M_AbilityColumn->GetChildAt(SlotIndex));
}

void UW_CommanderSelectionMenu::HandleAbilityRowClicked(const int32 SlotIndex)
{
	(void)PreviewAbilitySlot(SlotIndex);
}

bool UW_CommanderSelectionMenu::PreviewAbilitySlot(const int32 SlotIndex)
{
	if (not M_Commanders.IsValidIndex(M_SelectedIndex))
	{
		return false;
	}
	const TArray<FCommanderSelectionAbilitySlot>& AbilitySlots = M_Commanders[M_SelectedIndex].AbilitySlots;
	if (not AbilitySlots.IsValidIndex(SlotIndex) || not AbilitySlots[SlotIndex].bHasTemplate
		|| not GetIsValidFactionPlayerController())
	{
		return false;
	}
	return M_FactionPlayerController->HandleAbilityPreviewRequested(AbilitySlots[SlotIndex]);
}

FReply UW_CommanderSelectionMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (HandleNavigationKey(InKeyEvent.GetKey()))
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

bool UW_CommanderSelectionMenu::HandleNavigationKey(const FKey& Key)
{
	const auto IsAnyOf = [&Key](const std::initializer_list<FKey> Keys)
	{
		for (const FKey& Candidate : Keys)
		{
			if (Candidate == Key)
			{
				return true;
			}
		}
		return false;
	};
	if (IsAnyOf({EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up}))
	{
		SelectAdjacentEntry(-1);
		return true;
	}
	if (IsAnyOf({EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down}))
	{
		SelectAdjacentEntry(1);
		return true;
	}
	if (IsAnyOf({EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_Special_Right}))
	{
		OnConfirmClicked();
		return true;
	}
	if (IsAnyOf({EKeys::Escape, EKeys::Gamepad_FaceButton_Right}))
	{
		OnBackClicked();
		return true;
	}
	return false;
}

void UW_CommanderSelectionMenu::SelectAdjacentEntry(const int32 Step)
{
	if (M_Commanders.IsEmpty())
	{
		return;
	}
	const int32 CurrentIndex = M_Commanders.IsValidIndex(M_SelectedIndex) ? M_SelectedIndex : 0;
	SelectEntry(FMath::Clamp(CurrentIndex + Step, 0, M_Commanders.Num() - 1));
}

void UW_CommanderSelectionMenu::ConfirmSelection()
{
	OnConfirmClicked();
}

void UW_CommanderSelectionMenu::OnConfirmClicked()
{
	const ERTSCommander SelectedCommander = GetSelectedCommander();
	if (SelectedCommander == ERTSCommander::NoCommander || not GetIsValidFactionPlayerController())
	{
		return;
	}
	M_FactionPlayerController->HandleCommanderSelectionConfirmed(SelectedCommander);
}

void UW_CommanderSelectionMenu::OnBackClicked()
{
	if (not GetIsValidFactionPlayerController())
	{
		return;
	}
	M_FactionPlayerController->HandleCommanderSelectionBack();
}

bool UW_CommanderSelectionMenu::GetIsValidFactionPlayerController() const
{
	if (M_FactionPlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_FactionPlayerController",
	                                                             "GetIsValidFactionPlayerController", this);
	return false;
}

bool UW_CommanderSelectionMenu::GetIsValidAbilityColumn() const
{
	if (IsValid(M_AbilityColumn))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_AbilityColumn", "GetIsValidAbilityColumn",
	                                                             this);
	return false;
}

bool UW_CommanderSelectionMenu::GetIsValidPortraitImage() const
{
	if (IsValid(M_PortraitImage))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_PortraitImage", "GetIsValidPortraitImage",
	                                                             this);
	return false;
}
