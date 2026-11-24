// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapUI/CommanderPanel/W_WorldCommanderPanel.h"

#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderWidgetHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/CommanderSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CommanderMapKit/WorldCommanderMapKit.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowParticipant.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/CampaignHUD/WorldCampaignUILook.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

// Layout of sketch 4A / sheet 7 (bottom-left), in UI units at the 1920x1080 reference. The panel is a fixed box the
// HUD anchors to the bottom-left corner of the screen; y positions are measured from the top of that box.
namespace WorldCommanderPanelLayout
{
	// Wide enough for the pick prompt with its Cancel button.
	const FVector2D PanelSize(560.f, 260.f);

	const FVector2D PortraitPosition(24.f, 116.f);
	const FVector2D PortraitSize(120.f, 120.f);
	constexpr float PortraitRingWidth = 3.f;

	// The name plate, the passive tag and the hover plates keep their text this far from their left and right rim.
	constexpr float PlateSidePadding = 12.f;

	const FVector2D NamePlatePosition(14.f, 82.f);
	constexpr float NamePlateMinWidth = 170.f;
	constexpr float NamePlateHeight = 26.f;
	const FMargin NamePlateTextPadding(PlateSidePadding, 0.f, PlateSidePadding, 0.f);

	// Far enough right that the bezel only laps over the ring of the portrait, not over the face.
	const FVector2D AbilityButtonPosition(152.f, 164.f);
	const FVector2D AbilityButtonSize(74.f, 74.f);
	constexpr float AbilityRingWidth = 3.f;
	// The soft glow behind a lit button reaches this far past its ring.
	constexpr float AbilityHaloSpread = 9.f;
	// Glass button of sheet 14A: a gunmetal bezel around a glass dome that is tinted per state.
	constexpr float AbilityBezelSpread = 15.f;
	// The plates beside and above the ability button stay this far clear of its bezel.
	constexpr float BezelToPlateGap = 6.f;
	const float AbilityBezelRight = AbilityButtonPosition.X + AbilityButtonSize.X + AbilityBezelSpread;
	const float AbilityBezelTop = AbilityButtonPosition.Y - AbilityBezelSpread;
	const FVector2D GlyphRingSize(30.f, 30.f);
	constexpr float GlyphRingWidth = 3.f;
	const FVector2D GlyphDotSize(10.f, 10.f);

	const FVector2D PassivePlatePosition(AbilityBezelRight + BezelToPlateGap, 186.f);
	constexpr float PassivePlateMinWidth = 150.f;
	constexpr float PassivePlateHeight = 44.f;
	const FMargin PassiveLabelPadding(PlateSidePadding, 6.f, PlateSidePadding, 0.f);
	const FMargin PassiveNamePadding(PlateSidePadding, 1.f, PlateSidePadding, 0.f);

	// Hover plates and the pick prompt grow upward from their bottom-left corner.
	// Their left edge follows the button, so they stay right of the name plate.
	constexpr float PlatesAboveButtonInset = 64.f;
	const float PlatesAboveButtonLeft = AbilityButtonPosition.X + PlatesAboveButtonInset;
	const FVector2D AbilityHoverPlateBottomLeft(PlatesAboveButtonLeft, AbilityBezelTop - BezelToPlateGap);
	constexpr float AbilityHoverPlateWidth = 250.f;
	const FVector2D PassiveHoverPlateBottomLeft(PassivePlatePosition.X, PassivePlatePosition.Y - BezelToPlateGap);
	constexpr float PassiveHoverPlateWidth = 250.f;
	const FVector2D PickPromptBottomLeft(PlatesAboveButtonLeft, AbilityBezelTop - BezelToPlateGap);
	constexpr float PickPromptWidth = 340.f;
	const FMargin HoverPlatePadding(PlateSidePadding, 8.f, PlateSidePadding, 9.f);
	const FVector2D CancelButtonSize(92.f, 28.f);
	constexpr float PlateLineGap = 4.f;
	constexpr float PlateCornerRadius = 2.f;
	constexpr float PlateBorderWidth = 1.f;

	constexpr int32 NameFontSize = 11;
	constexpr int32 NameLetterSpacing = 100;
	constexpr int32 LabelFontSize = 9;
	constexpr int32 LabelLetterSpacing = 200;
	constexpr int32 PassiveNameFontSize = 12;
	constexpr int32 CooldownFontSize = 24;
	constexpr int32 PlateTitleFontSize = 14;
	constexpr int32 PlateTitleLetterSpacing = 150;
	constexpr int32 PlateBodyFontSize = 11;
	constexpr int32 PlateStateFontSize = 11;
	constexpr int32 PromptFontSize = 12;
	constexpr int32 PromptLetterSpacing = 50;
	constexpr int32 CancelFontSize = 11;
	constexpr int32 CancelLetterSpacing = 100;

	constexpr int32 ZOrderPortrait = 1;
	constexpr int32 ZOrderHalo = 2;
	constexpr int32 ZOrderControls = 3;
	constexpr int32 ZOrderPlates = 10;

	// A failed use keeps its reason on the plate this long.
	constexpr float FailReasonSeconds = 2.5f;
}

// Colours of the sketch that are not in WorldCampaignUIColors; only the commander panel draws them.
namespace WorldCommanderPanelColors
{
	const FLinearColor PortraitRing = FLinearColor(FColor::FromHex(TEXT("9A9EA0")));
	const FLinearColor PortraitEmpty = FLinearColor(FColor::FromHex(TEXT("2B3236")));
	const FLinearColor LitRing = FLinearColor(FColor::FromHex(TEXT("E6DFC8")));
	const FLinearColor LitFill = FLinearColor(FColor::FromHex(TEXT("E6DFC8")));
	const FLinearColor LitFillHovered = FLinearColor(FColor::FromHex(TEXT("FFFBEA")));
	const FLinearColor LitFillPressed = FLinearColor(FColor::FromHex(TEXT("A9A698")));
	const FLinearColor LitGlyph = FLinearColor(FColor::FromHex(TEXT("1A1B1A")));
	const FLinearColor Halo = FLinearColor(FColor::FromHex(TEXT("E6DFC8"))).CopyWithNewOpacity(0.22f);
	const FLinearColor CooldownRing = FLinearColor(FColor::FromHex(TEXT("6B7377")));
	const FLinearColor CooldownFill = FLinearColor(FColor::FromHex(TEXT("6B7377")));
	const FLinearColor CooldownFillHovered = FLinearColor(FColor::FromHex(TEXT("7C858A")));
	const FLinearColor CooldownNumber = FLinearColor(FColor::FromHex(TEXT("C9C7BD")));
	// "READY IN N TURNS" on the hover plate.
	const FLinearColor CooldownText = FLinearColor(FColor::FromHex(TEXT("FFB347")));
	const FLinearColor UnavailableFill = FLinearColor(FColor::FromHex(TEXT("3A4044")));
	const FLinearColor PlateFill = WorldCampaignUIColors::PlateFill.CopyWithNewOpacity(0.94f);
	// The one thin steel rim of the name plate, the passive tag and both hover plates.
	const FLinearColor PlateRim = FLinearColor(FColor::FromHex(TEXT("6B7377")));
}

namespace WorldCommanderPanelPrivate
{
	/** A circle (half-height rounding) with an inner ring; Fill may be transparent to draw only the ring. */
	FSlateBrush MakeDiscBrush(const FLinearColor& Fill, const FLinearColor& Ring, const float RingWidth,
	                          const FVector2D& Size)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Fill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(FSlateColor(Ring), RingWidth);
		Brush.ImageSize = Size;
		return Brush;
	}

	FSlateBrush MakePlateBrush(const FLinearColor& BorderColor)
	{
		using namespace WorldCommanderPanelLayout;
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(WorldCommanderPanelColors::PlateFill);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(PlateCornerRadius, FSlateColor(BorderColor),
		                                                   PlateBorderWidth);
		return Brush;
	}

	/** The top square of a portrait (head and shoulders), whatever the texture's aspect. */
	FBox2d MakeSquareTopCrop(const UTexture2D& Texture)
	{
		const double Width = Texture.GetSizeX();
		const double Height = Texture.GetSizeY();
		if (Width <= 0.0 || Height <= 0.0)
		{
			return FBox2d(FVector2d(0.0, 0.0), FVector2d(1.0, 1.0));
		}
		if (Height >= Width)
		{
			return FBox2d(FVector2d(0.0, 0.0), FVector2d(1.0, Width / Height));
		}
		const double CropWidth = Height / Width;
		const double CropStart = (1.0 - CropWidth) * 0.5;
		return FBox2d(FVector2d(CropStart, 0.0), FVector2d(CropStart + CropWidth, 1.0));
	}

	/**
	 * Round portrait without a mask asset: a rounded-box brush clips the texture to a circle (Slate's rounded box
	 * shader samples the brush texture) and draws the ring as its outline.
	 */
	FSlateBrush MakePortraitBrush(UTexture2D* Portrait)
	{
		using namespace WorldCommanderPanelLayout;
		FSlateBrush Brush = MakeDiscBrush(WorldCommanderPanelColors::PortraitEmpty, WorldCommanderPanelColors::PortraitRing,
		                                  PortraitRingWidth, PortraitSize);
		if (not IsValid(Portrait))
		{
			return Brush;
		}
		Brush.SetResourceObject(Portrait);
		Brush.TintColor = FSlateColor(FLinearColor::White);
		Brush.SetUVRegion(MakeSquareTopCrop(*Portrait));
		return Brush;
	}

	struct FAbilityButtonColors
	{
		FLinearColor Fill;
		FLinearColor FillHovered;
		FLinearColor FillPressed;
		FLinearColor Ring;
		FLinearColor Glyph;
	};

	FAbilityButtonColors GetAbilityButtonColors(const EWorldCommanderAbilityButtonState State)
	{
		using namespace WorldCommanderPanelColors;
		switch (State)
		{
		case EWorldCommanderAbilityButtonState::Ready:
			return {LitFill, LitFillHovered, LitFillPressed, LitRing, LitGlyph};
		case EWorldCommanderAbilityButtonState::PickingTarget:
			return {LitFillPressed, LitFill, LitFillPressed, WorldCampaignUIColors::Cream, WorldCampaignUIColors::Cream};
		case EWorldCommanderAbilityButtonState::CoolingDown:
			return {CooldownFill, CooldownFillHovered, CooldownFill, CooldownRing, CooldownNumber};
		case EWorldCommanderAbilityButtonState::Unavailable:
			break;
		}
		return {UnavailableFill, UnavailableFill, UnavailableFill, WorldCampaignUIColors::Locked,
		        WorldCampaignUIColors::Locked};
	}

	FSlateBrush MakeTintedDome(const FSlateBrush& GlassDome, const FLinearColor& Tint)
	{
		FSlateBrush Brush = GlassDome;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	FButtonStyle MakeAbilityButtonStyle(const FAbilityButtonColors& Colors, const FSlateBrush& GlassDome)
	{
		FButtonStyle Style;
		const FSlateBrush Normal = MakeTintedDome(GlassDome, Colors.Fill);
		Style.SetNormal(Normal);
		Style.SetHovered(MakeTintedDome(GlassDome, Colors.FillHovered));
		Style.SetPressed(MakeTintedDome(GlassDome, Colors.FillPressed));
		Style.SetDisabled(Normal);
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	FButtonStyle MakePassivePlateStyle()
	{
		FButtonStyle Style;
		Style.SetNormal(MakePlateBrush(WorldCommanderPanelColors::PlateRim));
		Style.SetHovered(MakePlateBrush(WorldCampaignUIColors::Good));
		Style.SetPressed(MakePlateBrush(WorldCampaignUIColors::Good));
		Style.SetDisabled(MakePlateBrush(WorldCommanderPanelColors::PlateRim));
		Style.SetNormalPadding(FMargin(0.f));
		Style.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	void SetTextOrCollapse(UTextBlock& TextBlock, const FText& Text)
	{
		TextBlock.SetText(Text);
		TextBlock.SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	void PlaceInCenter(UOverlay& Overlay, UWidget* Widget)
	{
		UOverlaySlot* const CenterSlot = Overlay.AddChildToOverlay(Widget);
		CenterSlot->SetHorizontalAlignment(HAlign_Center);
		CenterSlot->SetVerticalAlignment(VAlign_Center);
	}
}

// ------------------------------------------------------------------ Public

void UW_WorldCommanderPanel::InitCommanderPanel(AWorldPlayerController* Controller, const FWorldCampaignUILook* Look)
{
	if (Look == nullptr)
	{
		RTSFunctionLibrary::ReportError(TEXT("UW_WorldCommanderPanel::InitCommanderPanel: no campaign UI look given; "
			"the commander panel is not built."));
		return;
	}
	M_Controller = Controller;
	if (GetIsValidController())
	{
		M_CommanderMapKit = Controller->GetWorldCommanderMapKit();
	}
	BuildWidgetTree(*Look);
	if (GetIsValidCommanderMapKit())
	{
		M_CommanderMapKit->OnMapAbilityUsed.AddUniqueDynamic(this, &UW_WorldCommanderPanel::OnMapAbilityUsed);
		M_CommanderMapKit->OnMapAbilityCooldownChanged.AddUniqueDynamic(
			this, &UW_WorldCommanderPanel::OnMapAbilityCooldownChanged);
	}
	RefreshFromCampaign();
}

void UW_WorldCommanderPanel::RefreshFromCampaign()
{
	if (not bM_IsTreeBuilt || not GetIsValidCommanderMapKit())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const ERTSCommander Commander = M_CommanderMapKit->GetCommander();
	if (Commander == ERTSCommander::NoCommander)
	{
		CancelTargetPicking();
		M_ShownCommander = Commander;
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (Commander != M_ShownCommander)
	{
		M_ShownCommander = Commander;
		RefreshPortraitAndName();
	}
	bM_HasKit = M_CommanderMapKit->GetCommanderKit(M_Kit);
	if (not bM_HasKit)
	{
		M_Kit = FWorldCommanderMapKitDefinition();
	}
	// A used ability, a lost kit or a new cooldown ends a pick that was still open.
	if (bM_IsPickingTarget && GetAbilityButtonState() != EWorldCommanderAbilityButtonState::PickingTarget)
	{
		CancelTargetPicking();
	}
	RefreshPassive();
	RefreshAbilityButton();
	RefreshPlates();
}

void UW_WorldCommanderPanel::CancelTargetPicking()
{
	if (not bM_IsPickingTarget)
	{
		return;
	}
	bM_IsPickingTarget = false;
	M_PickingTargetKind = EWorldCommanderAbilityTargetKind::None;
	if (bM_IsTreeBuilt)
	{
		WorldCommanderPanelPrivate::SetTextOrCollapse(*M_PickFailText, FText::GetEmpty());
	}
	RefreshAbilityButton();
	RefreshPlates();
}

bool UW_WorldCommanderPanel::TryUseAbilityOnPickedActor(AActor* ClickedActor, const FVector& ClickedWorldLocation,
                                                        FText& OutFailReason)
{
	if (not bM_IsPickingTarget)
	{
		OutFailReason = FText::GetEmpty();
		return false;
	}
	const FWorldCommanderPickCandidate Candidate = MakePickCandidate(ClickedActor, ClickedWorldLocation);
	FWorldCommanderMapAbilityTarget Target;
	if (not WorldCommanderPanelRules::BuildTargetFromPick(M_PickingTargetKind, Candidate, Target, OutFailReason))
	{
		ShowFailReason(OutFailReason);
		return false;
	}
	if (not UseAbilityOnTarget(Target, OutFailReason))
	{
		return false;
	}
	CancelTargetPicking();
	return true;
}

void UW_WorldCommanderPanel::SetIsInputBlocked(const bool bIsBlocked)
{
	bM_IsInputBlocked = bIsBlocked;
	if (bIsBlocked)
	{
		CancelTargetPicking();
	}
}

void UW_WorldCommanderPanel::ShowAbilityTooltipForCapture(const bool bShow)
{
	M_PlateState.bIsAbilityPlatePinned = bShow;
	RefreshPlates();
}

bool UW_WorldCommanderPanel::UseAbilityForCapture(const FWorldCommanderMapAbilityTarget& Target)
{
	FText FailReason;
	if (not UseAbilityOnTarget(Target, FailReason))
	{
		return false;
	}
	CancelTargetPicking();
	return true;
}

void UW_WorldCommanderPanel::NativeDestruct()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_HideFailReasonTimer);
	}
	// The kit delegates stay bound: NativeDestruct also runs when the HUD is only taken off the screen, and a dynamic
	// delegate holds the panel weakly, so a destroyed panel is skipped.
	Super::NativeDestruct();
}

FReply UW_WorldCommanderPanel::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		CancelTargetPicking();
	}
	return FReply::Handled();
}

// ------------------------------------------------------------------ Build

void UW_WorldCommanderPanel::NativeOnInitialized()
{
	using namespace WorldCommanderPanelLayout;
	Super::NativeOnInitialized();
	// The root exists before the widget is first added anywhere, so the panel shows its content whether the HUD
	// calls InitCommanderPanel before or after adding it; Init only fills the canvas.
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	// Only the buttons and the pick prompt take the mouse; every other part lets clicks through to the map.
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	USizeBox* const PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(PanelSize.X);
	PanelBox->SetHeightOverride(PanelSize.Y);
	PanelBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = PanelBox;
	M_Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	M_Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PanelBox->SetContent(M_Canvas);
}

void UW_WorldCommanderPanel::BuildWidgetTree(const FWorldCampaignUILook& Look)
{
	if (bM_IsTreeBuilt || not GetIsValidCanvas())
	{
		return;
	}
	BuildPortrait();
	BuildNamePlate(Look);
	BuildAbilityButton(Look);
	BuildPassivePlate(Look);
	BuildAbilityHoverPlate(Look);
	BuildPassiveHoverPlate(Look);
	BuildPickPrompt(Look);
	bM_IsTreeBuilt = true;
}

void UW_WorldCommanderPanel::BuildPortrait()
{
	using namespace WorldCommanderPanelLayout;
	M_PortraitImage = DeckBuilderWidgets::MakeImage(*WidgetTree, WorldCommanderPanelPrivate::MakePortraitBrush(nullptr));
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_PortraitImage, PortraitPosition, PortraitSize, ZOrderPortrait);
}

void UW_WorldCommanderPanel::BuildNamePlate(const FWorldCampaignUILook& Look)
{
	using namespace WorldCommanderPanelLayout;
	USizeBox* const PlateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PlateBox->SetMinDesiredWidth(NamePlateMinWidth);
	PlateBox->SetHeightOverride(NamePlateHeight);
	PlateBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UBorder* const Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Plate->SetBrush(WorldCommanderPanelPrivate::MakePlateBrush(WorldCommanderPanelColors::PlateRim));
	Plate->SetPadding(NamePlateTextPadding);
	Plate->SetHorizontalAlignment(HAlign_Center);
	Plate->SetVerticalAlignment(VAlign_Center);
	PlateBox->SetContent(Plate);
	M_NameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, NameFontSize, NameLetterSpacing), Look.Deck.Style.TextColor);
	Plate->SetContent(M_NameText);
	UCanvasPanelSlot* const PlateSlot = DeckBuilderWidgets::PlaceOnCanvas(
		*M_Canvas, PlateBox, NamePlatePosition, FVector2D(NamePlateMinWidth, NamePlateHeight), ZOrderControls);
	PlateSlot->SetAutoSize(true);
}

void UW_WorldCommanderPanel::BuildAbilityButton(const FWorldCampaignUILook& Look)
{
	using namespace WorldCommanderPanelLayout;
	using namespace WorldCommanderPanelPrivate;
	const FVector2D HaloSize = AbilityButtonSize + FVector2D(AbilityHaloSpread * 2.f, AbilityHaloSpread * 2.f);
	M_AbilityHalo = DeckBuilderWidgets::MakeImage(
		*WidgetTree, MakeDiscBrush(WorldCommanderPanelColors::Halo, FLinearColor::Transparent, 0.f, HaloSize));
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_AbilityHalo,
	                                  AbilityButtonPosition - FVector2D(AbilityHaloSpread, AbilityHaloSpread), HaloSize,
	                                  ZOrderHalo);

	const FVector2D BezelSize = AbilityButtonSize + FVector2D(AbilityBezelSpread * 2.f, AbilityBezelSpread * 2.f);
	UImage* const Bezel = DeckBuilderWidgets::MakeImage(*WidgetTree, Look.RoundBezel);
	Bezel->SetVisibility(ESlateVisibility::HitTestInvisible);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, Bezel,
	                                  AbilityButtonPosition - FVector2D(AbilityBezelSpread, AbilityBezelSpread), BezelSize,
	                                  ZOrderHalo);

	M_AbilityButton = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(
		UUnitVariantSelectionButton::StaticClass());
	M_GlassDomeBrush = Look.GlassDome;
	M_AbilityButton->SetStyle(MakeAbilityButtonStyle(
		GetAbilityButtonColors(EWorldCommanderAbilityButtonState::Unavailable), M_GlassDomeBrush));
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_AbilityButton->SetContent(BuildAbilityGlyph(Look))))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	M_AbilityButton->OnClicked.AddUniqueDynamic(this, &UW_WorldCommanderPanel::OnAbilityButtonClicked);
	M_AbilityButton->OnHovered.AddUniqueDynamic(this, &UW_WorldCommanderPanel::OnAbilityButtonHovered);
	M_AbilityButton->OnUnhovered.AddUniqueDynamic(this, &UW_WorldCommanderPanel::OnAbilityButtonUnhovered);
	DeckBuilderWidgets::PlaceOnCanvas(*M_Canvas, M_AbilityButton, AbilityButtonPosition, AbilityButtonSize,
	                                  ZOrderControls);
}

UWidget* UW_WorldCommanderPanel::BuildAbilityGlyph(const FWorldCampaignUILook& Look)
{
	using namespace WorldCommanderPanelLayout;
	using namespace WorldCommanderPanelPrivate;
	// The sketch's target glyph, drawn as a ring and a dot because no map ability icon exists.
	UOverlay* const GlyphOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	GlyphOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	M_AbilityGlyphRing = DeckBuilderWidgets::MakeImage(
		*WidgetTree, MakeDiscBrush(FLinearColor::Transparent, WorldCampaignUIColors::Locked, GlyphRingWidth,
		                           GlyphRingSize));
	PlaceInCenter(*GlyphOverlay, M_AbilityGlyphRing);
	M_AbilityGlyphDot = DeckBuilderWidgets::MakeImage(
		*WidgetTree, MakeDiscBrush(WorldCampaignUIColors::Locked, FLinearColor::Transparent, 0.f, GlyphDotSize));
	PlaceInCenter(*GlyphOverlay, M_AbilityGlyphDot);
	M_AbilityCooldownText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, CooldownFontSize),
		FSlateColor(WorldCommanderPanelColors::CooldownNumber));
	M_AbilityCooldownText->SetVisibility(ESlateVisibility::Collapsed);
	PlaceInCenter(*GlyphOverlay, M_AbilityCooldownText);
	return GlyphOverlay;
}

void UW_WorldCommanderPanel::BuildPassivePlate(const FWorldCampaignUILook& Look)
{
	using namespace WorldCommanderPanelLayout;
	M_PassivePlateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	M_PassivePlateBox->SetMinDesiredWidth(PassivePlateMinWidth);
	M_PassivePlateBox->SetHeightOverride(PassivePlateHeight);
	M_PassivePlateBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	M_PassiveButton = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(
		UUnitVariantSelectionButton::StaticClass());
	M_PassiveButton->SetStyle(WorldCommanderPanelPrivate::MakePassivePlateStyle());
	M_PassiveButton->OnHovered.AddUniqueDynamic(this, &UW_WorldCommanderPanel::OnPassiveHovered);
	M_PassiveButton->OnUnhovered.AddUniqueDynamic(this, &UW_WorldCommanderPanel::OnPassiveUnhovered);
	M_PassivePlateBox->SetContent(M_PassiveButton);

	UVerticalBox* const Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Lines->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_PassiveButton->SetContent(Lines)))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UTextBlock* const Label = DeckBuilderWidgets::MakeText(
		*WidgetTree, NSLOCTEXT("WorldCommanderPanel", "PassiveLabel", "PASSIVE"),
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, LabelFontSize, LabelLetterSpacing),
		Look.Deck.Style.MutedTextColor);
	Lines->AddChildToVerticalBox(Label)->SetPadding(PassiveLabelPadding);
	M_PassiveNameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.BodyFont, PassiveNameFontSize),
		FSlateColor(WorldCampaignUIColors::Good));
	Lines->AddChildToVerticalBox(M_PassiveNameText)->SetPadding(PassiveNamePadding);

	UCanvasPanelSlot* const PlateSlot = DeckBuilderWidgets::PlaceOnCanvas(
		*M_Canvas, M_PassivePlateBox, PassivePlatePosition, FVector2D(PassivePlateMinWidth, PassivePlateHeight),
		ZOrderControls);
	PlateSlot->SetAutoSize(true);
}

UBorder* UW_WorldCommanderPanel::MakeHoverPlate(const FLinearColor& BorderColor, const float PlateWidth,
                                               const FVector2D& BottomLeft)
{
	using namespace WorldCommanderPanelLayout;
	UBorder* const Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Plate->SetBrush(WorldCommanderPanelPrivate::MakePlateBrush(BorderColor));
	Plate->SetPadding(HoverPlatePadding);
	Plate->SetVisibility(ESlateVisibility::Collapsed);
	USizeBox* const PlateBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PlateBox->SetWidthOverride(PlateWidth);
	PlateBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	PlateBox->SetContent(Plate);
	// Placed by its bottom-left corner; the plate grows upward with its text.
	UCanvasPanelSlot* const PlateSlot = M_Canvas->AddChildToCanvas(PlateBox);
	PlateSlot->SetAnchors(FAnchors(0.f, 0.f));
	PlateSlot->SetAlignment(FVector2D(0.f, 1.f));
	PlateSlot->SetPosition(BottomLeft);
	PlateSlot->SetAutoSize(true);
	PlateSlot->SetZOrder(ZOrderPlates);
	return Plate;
}

void UW_WorldCommanderPanel::BuildAbilityHoverPlate(const FWorldCampaignUILook& Look)
{
	using namespace WorldCommanderPanelLayout;
	M_AbilityHoverPlate = MakeHoverPlate(WorldCommanderPanelColors::PlateRim, AbilityHoverPlateWidth,
	                                     AbilityHoverPlateBottomLeft);
	const float WrapWidth = AbilityHoverPlateWidth - HoverPlatePadding.Left - HoverPlatePadding.Right;
	UVerticalBox* const Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_AbilityHoverPlate->SetContent(Lines);
	M_AbilityHoverNameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, PlateTitleFontSize, PlateTitleLetterSpacing),
		FSlateColor(WorldCommanderPanelColors::LitRing));
	Lines->AddChildToVerticalBox(M_AbilityHoverNameText);
	M_AbilityHoverDescriptionText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.BodyFont, PlateBodyFontSize),
		Look.Deck.Style.DescriptionColor);
	M_AbilityHoverDescriptionText->SetWrapTextAt(WrapWidth);
	Lines->AddChildToVerticalBox(M_AbilityHoverDescriptionText)->SetPadding(FMargin(0.f, PlateLineGap, 0.f, 0.f));
	M_AbilityHoverStateText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, PlateStateFontSize),
		FSlateColor(WorldCommanderPanelColors::LitRing));
	Lines->AddChildToVerticalBox(M_AbilityHoverStateText)->SetPadding(FMargin(0.f, PlateLineGap, 0.f, 0.f));
	M_AbilityHoverFailText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.BodyFont, PlateBodyFontSize),
		FSlateColor(WorldCampaignUIColors::Bad));
	M_AbilityHoverFailText->SetWrapTextAt(WrapWidth);
	M_AbilityHoverFailText->SetVisibility(ESlateVisibility::Collapsed);
	Lines->AddChildToVerticalBox(M_AbilityHoverFailText)->SetPadding(FMargin(0.f, PlateLineGap, 0.f, 0.f));
}

void UW_WorldCommanderPanel::BuildPassiveHoverPlate(const FWorldCampaignUILook& Look)
{
	using namespace WorldCommanderPanelLayout;
	M_PassiveHoverPlate = MakeHoverPlate(WorldCommanderPanelColors::PlateRim, PassiveHoverPlateWidth,
	                                     PassiveHoverPlateBottomLeft);
	UVerticalBox* const Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_PassiveHoverPlate->SetContent(Lines);
	M_PassiveHoverNameText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, PlateTitleFontSize, PlateTitleLetterSpacing),
		FSlateColor(WorldCampaignUIColors::Good));
	Lines->AddChildToVerticalBox(M_PassiveHoverNameText);
	M_PassiveHoverDescriptionText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.BodyFont, PlateBodyFontSize),
		Look.Deck.Style.DescriptionColor);
	M_PassiveHoverDescriptionText->SetWrapTextAt(
		PassiveHoverPlateWidth - HoverPlatePadding.Left - HoverPlatePadding.Right);
	Lines->AddChildToVerticalBox(M_PassiveHoverDescriptionText)->SetPadding(FMargin(0.f, PlateLineGap, 0.f, 0.f));
}

void UW_WorldCommanderPanel::BuildPickPrompt(const FWorldCampaignUILook& Look)
{
	using namespace WorldCommanderPanelLayout;
	M_PickPromptPlate = MakeHoverPlate(WorldCommanderPanelColors::LitRing, PickPromptWidth, PickPromptBottomLeft);
	UVerticalBox* const Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	M_PickPromptPlate->SetContent(Lines);
	UHorizontalBox* const PromptRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Lines->AddChildToVerticalBox(PromptRow);
	M_PickPromptText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(),
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, PromptFontSize, PromptLetterSpacing),
		FSlateColor(WorldCommanderPanelColors::LitRing));
	UHorizontalBoxSlot* const PromptSlot = PromptRow->AddChildToHorizontalBox(M_PickPromptText);
	PromptSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	PromptSlot->SetVerticalAlignment(VAlign_Center);
	UWidget* const CancelBox = DeckBuilderWidgets::BuildTextButton(
		*WidgetTree, NSLOCTEXT("WorldCommanderPanel", "CancelPicking", "CANCEL"), Look.Deck.Button,
		DeckBuilderWidgets::MakeFont(Look.Deck.HeadingFont, CancelFontSize, CancelLetterSpacing),
		Look.Deck.Style.TextColor, CancelButtonSize, M_CancelPickingButton, M_CancelPickingLabel);
	M_CancelPickingButton->OnClicked.AddUniqueDynamic(this, &UW_WorldCommanderPanel::OnCancelPickingClicked);
	PromptRow->AddChildToHorizontalBox(CancelBox)->SetVerticalAlignment(VAlign_Center);
	M_PickFailText = DeckBuilderWidgets::MakeText(
		*WidgetTree, FText::GetEmpty(), DeckBuilderWidgets::MakeFont(Look.Deck.BodyFont, PlateBodyFontSize),
		FSlateColor(WorldCampaignUIColors::Bad));
	M_PickFailText->SetWrapTextAt(PickPromptWidth - HoverPlatePadding.Left - HoverPlatePadding.Right);
	M_PickFailText->SetVisibility(ESlateVisibility::Collapsed);
	Lines->AddChildToVerticalBox(M_PickFailText)->SetPadding(FMargin(0.f, PlateLineGap, 0.f, 0.f));
}

// ------------------------------------------------------------------ Refresh

void UW_WorldCommanderPanel::RefreshPortraitAndName()
{
	FText CommanderName = UEnum::GetDisplayValueAsText(M_ShownCommander);
	UTexture2D* Portrait = nullptr;
	const UCommanderSettings* const CommanderSettings = UCommanderSettings::Get();
	const FRTSCommanderSettings* const Settings = IsValid(CommanderSettings)
		                                              ? CommanderSettings->CommanderSettingsByType.Find(M_ShownCommander)
		                                              : nullptr;
	if (Settings != nullptr)
	{
		CommanderName = Settings->Title.IsEmpty() ? CommanderName : Settings->Title;
		Portrait = Settings->Portrait.LoadSynchronous();
	}
	M_PortraitTexture = Portrait;
	M_PortraitImage->SetBrush(WorldCommanderPanelPrivate::MakePortraitBrush(Portrait));
	M_NameText->SetText(CommanderName.ToUpper());
}

void UW_WorldCommanderPanel::RefreshPassive()
{
	const bool bHasPassive = bM_HasKit && not M_Kit.PassiveDisplayName.IsEmpty();
	M_PassivePlateBox->SetVisibility(bHasPassive ? ESlateVisibility::SelfHitTestInvisible
		                                 : ESlateVisibility::Collapsed);
	M_PassiveNameText->SetText(M_Kit.PassiveDisplayName);
	M_PassiveHoverNameText->SetText(M_Kit.PassiveDisplayName.ToUpper());
	M_PassiveHoverDescriptionText->SetText(M_Kit.PassiveDescription);
	if (not bHasPassive)
	{
		M_PlateState.bIsPassiveHovered = false;
	}
}

void UW_WorldCommanderPanel::RefreshAbilityButton()
{
	using namespace WorldCommanderPanelLayout;
	using namespace WorldCommanderPanelPrivate;
	if (not bM_IsTreeBuilt)
	{
		return;
	}
	const EWorldCommanderAbilityButtonState State = GetAbilityButtonState();
	const FAbilityButtonColors Colors = GetAbilityButtonColors(State);
	M_AbilityButton->SetStyle(MakeAbilityButtonStyle(Colors, M_GlassDomeBrush));
	M_AbilityGlyphRing->SetBrush(MakeDiscBrush(FLinearColor::Transparent, Colors.Glyph, GlyphRingWidth, GlyphRingSize));
	M_AbilityGlyphDot->SetBrush(MakeDiscBrush(Colors.Glyph, FLinearColor::Transparent, 0.f, GlyphDotSize));

	const bool bIsCoolingDown = State == EWorldCommanderAbilityButtonState::CoolingDown;
	const ESlateVisibility GlyphVisibility = bIsCoolingDown
		                                         ? ESlateVisibility::Collapsed
		                                         : ESlateVisibility::HitTestInvisible;
	M_AbilityGlyphRing->SetVisibility(GlyphVisibility);
	M_AbilityGlyphDot->SetVisibility(GlyphVisibility);
	SetTextOrCollapse(*M_AbilityCooldownText,
	                  WorldCommanderPanelRules::GetAbilityButtonLabel(State, GetCooldownTurnsRemaining()));

	const bool bIsLit = State == EWorldCommanderAbilityButtonState::Ready
		|| State == EWorldCommanderAbilityButtonState::PickingTarget;
	M_AbilityHalo->SetVisibility(bIsLit ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UW_WorldCommanderPanel::RefreshPlates()
{
	if (not bM_IsTreeBuilt)
	{
		return;
	}
	const bool bWantsAbilityPlate = M_PlateState.bIsAbilityHovered || M_PlateState.bIsAbilityPlatePinned
		|| M_PlateState.bIsShowingFailReason;
	const bool bShowsAbilityPlate = bWantsAbilityPlate && GetHasAbility() && not bM_IsPickingTarget;
	if (bShowsAbilityPlate)
	{
		RefreshAbilityHoverPlateTexts();
	}
	M_AbilityHoverPlate->SetVisibility(bShowsAbilityPlate ? ESlateVisibility::HitTestInvisible
		                                   : ESlateVisibility::Collapsed);

	const bool bShowsPassivePlate = M_PlateState.bIsPassiveHovered && not bShowsAbilityPlate
		&& not M_Kit.PassiveDescription.IsEmpty();
	M_PassiveHoverPlate->SetVisibility(bShowsPassivePlate ? ESlateVisibility::HitTestInvisible
		                                   : ESlateVisibility::Collapsed);

	M_PickPromptText->SetText(WorldCommanderPanelRules::GetTargetPromptText(M_PickingTargetKind));
	// Visible: a click on the prompt must not fall through to the map as a pick.
	M_PickPromptPlate->SetVisibility(bM_IsPickingTarget ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UW_WorldCommanderPanel::RefreshAbilityHoverPlateTexts()
{
	const EWorldCommanderAbilityButtonState State = GetAbilityButtonState();
	M_AbilityHoverNameText->SetText(M_Kit.AbilityDisplayName.ToUpper());
	M_AbilityHoverDescriptionText->SetText(M_Kit.AbilityDescription);
	M_AbilityHoverStateText->SetText(WorldCommanderPanelRules::GetAbilityStateLine(
		State, GetCooldownTurnsRemaining(), M_Kit.Ability.CooldownTurns));
	const bool bIsCoolingDown = State == EWorldCommanderAbilityButtonState::CoolingDown;
	M_AbilityHoverStateText->SetColorAndOpacity(FSlateColor(
		bIsCoolingDown ? WorldCommanderPanelColors::CooldownText : WorldCommanderPanelColors::LitRing));
	if (not M_PlateState.bIsShowingFailReason)
	{
		WorldCommanderPanelPrivate::SetTextOrCollapse(*M_AbilityHoverFailText, FText::GetEmpty());
	}
}

EWorldCommanderAbilityButtonState UW_WorldCommanderPanel::GetAbilityButtonState() const
{
	return WorldCommanderPanelRules::GetAbilityButtonState(GetHasAbility(), GetCooldownTurnsRemaining(),
	                                                       bM_IsPickingTarget);
}

bool UW_WorldCommanderPanel::GetHasAbility() const
{
	return bM_HasKit && M_Kit.Ability.Type != EWorldCommanderMapAbility::None;
}

int32 UW_WorldCommanderPanel::GetCooldownTurnsRemaining() const
{
	if (not GetIsValidCommanderMapKit())
	{
		return 0;
	}
	return M_CommanderMapKit->GetMapAbilityCooldownTurnsRemaining();
}

// ------------------------------------------------------------------ Use

void UW_WorldCommanderPanel::BeginTargetPicking(const EWorldCommanderAbilityTargetKind Kind)
{
	bM_IsPickingTarget = true;
	M_PickingTargetKind = Kind;
	M_PlateState.bIsShowingFailReason = false;
	WorldCommanderPanelPrivate::SetTextOrCollapse(*M_PickFailText, FText::GetEmpty());
	RefreshAbilityButton();
	RefreshPlates();
}

bool UW_WorldCommanderPanel::UseAbilityOnTarget(const FWorldCommanderMapAbilityTarget& Target, FText& OutFailReason)
{
	if (not GetIsValidCommanderMapKit())
	{
		return false;
	}
	if (M_CommanderMapKit->TryUseMapAbility(Target, OutFailReason))
	{
		HideFailReason();
		RefreshFromCampaign();
		return true;
	}
	ShowFailReason(OutFailReason);
	return false;
}

FWorldCommanderPickCandidate UW_WorldCommanderPanel::MakePickCandidate(AActor* ClickedActor,
                                                                       const FVector& ClickedWorldLocation) const
{
	FWorldCommanderPickCandidate Candidate;
	if (not IsValid(ClickedActor))
	{
		return Candidate;
	}
	Candidate.Thing = EWorldCommanderPickedThing::Ground;
	Candidate.WorldLocation = ClickedWorldLocation;
	Candidate.bHasWorldLocation = true;
	// Same rule as a normal click of the controller: what the fog hides cannot be picked, only the place under it.
	const IWorldFowParticipant* const FowParticipant = Cast<IWorldFowParticipant>(ClickedActor);
	if (FowParticipant != nullptr && not FowParticipant->GetCanPrimaryClickInteract())
	{
		return Candidate;
	}
	if (const AWorldMapObject* const MapObject = Cast<AWorldMapObject>(ClickedActor))
	{
		const bool bIsOperationTarget = MapObject->IsA(AWorldMissionObject::StaticClass())
			|| MapObject->IsA(AWorldEnemyObject::StaticClass());
		const bool bIsOpenTarget = bIsOperationTarget && not MapObject->GetIsClearedByPlayer();
		Candidate.Thing = bIsOpenTarget
			                  ? EWorldCommanderPickedThing::OperationTarget
			                  : EWorldCommanderPickedThing::OtherMapObject;
		Candidate.AnchorKey = MapObject->GetAnchorKey();
		return Candidate;
	}
	if (const AWorldDivisionBase* const Division = Cast<AWorldDivisionBase>(ClickedActor))
	{
		Candidate.Thing = EWorldCommanderPickedThing::Division;
		Candidate.DivisionKey = Division->GetDivisionKey();
		return Candidate;
	}
	if (const AWorldBuildNode* const BuildNode = Cast<AWorldBuildNode>(ClickedActor))
	{
		Candidate.Thing = EWorldCommanderPickedThing::BuildNode;
		Candidate.BuildNodeId = BuildNode->GetBuildNodeId();
	}
	return Candidate;
}

void UW_WorldCommanderPanel::ShowFailReason(const FText& FailReason)
{
	if (FailReason.IsEmpty() || not bM_IsTreeBuilt)
	{
		return;
	}
	if (bM_IsPickingTarget)
	{
		WorldCommanderPanelPrivate::SetTextOrCollapse(*M_PickFailText, FailReason);
	}
	else
	{
		M_PlateState.bIsShowingFailReason = true;
		WorldCommanderPanelPrivate::SetTextOrCollapse(*M_AbilityHoverFailText, FailReason);
	}
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().SetTimer(M_HideFailReasonTimer, this, &UW_WorldCommanderPanel::HideFailReason,
		                                  WorldCommanderPanelLayout::FailReasonSeconds, false);
	}
	RefreshPlates();
}

void UW_WorldCommanderPanel::HideFailReason()
{
	if (const UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_HideFailReasonTimer);
	}
	M_PlateState.bIsShowingFailReason = false;
	if (bM_IsTreeBuilt)
	{
		WorldCommanderPanelPrivate::SetTextOrCollapse(*M_PickFailText, FText::GetEmpty());
	}
	RefreshPlates();
}

void UW_WorldCommanderPanel::OnAbilityButtonClicked()
{
	if (bM_IsInputBlocked)
	{
		return;
	}
	if (bM_IsPickingTarget)
	{
		CancelTargetPicking();
		return;
	}
	if (GetAbilityButtonState() != EWorldCommanderAbilityButtonState::Ready || not GetIsValidCommanderMapKit())
	{
		return;
	}
	const EWorldCommanderAbilityTargetKind TargetKind = M_CommanderMapKit->GetMapAbilityTargetKind();
	if (TargetKind != EWorldCommanderAbilityTargetKind::None)
	{
		BeginTargetPicking(TargetKind);
		return;
	}
	FText FailReason;
	UseAbilityOnTarget(FWorldCommanderMapAbilityTarget(), FailReason);
}

void UW_WorldCommanderPanel::OnAbilityButtonHovered()
{
	M_PlateState.bIsAbilityHovered = true;
	RefreshPlates();
}

void UW_WorldCommanderPanel::OnAbilityButtonUnhovered()
{
	M_PlateState.bIsAbilityHovered = false;
	RefreshPlates();
}

void UW_WorldCommanderPanel::OnPassiveHovered()
{
	M_PlateState.bIsPassiveHovered = true;
	RefreshPlates();
}

void UW_WorldCommanderPanel::OnPassiveUnhovered()
{
	M_PlateState.bIsPassiveHovered = false;
	RefreshPlates();
}

void UW_WorldCommanderPanel::OnCancelPickingClicked()
{
	CancelTargetPicking();
}

void UW_WorldCommanderPanel::OnMapAbilityUsed(EWorldCommanderMapAbility Ability,
                                              const FWorldCommanderMapAbilityTarget& Target)
{
	RefreshFromCampaign();
}

void UW_WorldCommanderPanel::OnMapAbilityCooldownChanged(int32 TurnsRemaining)
{
	RefreshFromCampaign();
}

bool UW_WorldCommanderPanel::GetIsValidController() const
{
	if (M_Controller.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Controller", "GetIsValidController", this);
	return false;
}

bool UW_WorldCommanderPanel::GetIsValidCanvas() const
{
	if (IsValid(M_Canvas))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_Canvas", "GetIsValidCanvas", this);
	return false;
}

bool UW_WorldCommanderPanel::GetIsValidCommanderMapKit() const
{
	if (M_CommanderMapKit.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_CommanderMapKit",
	                                                             "GetIsValidCommanderMapKit", this);
	return false;
}
