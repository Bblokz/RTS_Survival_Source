// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_CommanderAbilityRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionButton.h"
#include "RTS_Survival/FactionSystem/FactionSelection/UnitVariantSelection/UnitVariantSelectionWidgetHelpers.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_CommanderSelectionMenu.h"

// Sized for the 440 unit wide body of the 480 wide kit panel (1920x1080 reference).
namespace CommanderAbilityRowLayout
{
	constexpr float IconWellSize = 72.f;
	constexpr float IconSize = 64.f;
	const FMargin IconWellPadding(4.f);
	// MI_FactionButton_Glass draws its rim about 13 units in from the sides and 10 from the top and bottom; the icon,
	// badge and text keep a clear gap inside that rim (measured on 1920x1080 captures).
	const FMargin GlassRimInset(13.f, 10.f, 13.f, 10.f);
	constexpr float GapInsideGlassRim = 10.f;
	const FMargin RowContentPadding = GlassRimInset + FMargin(GapInsideGlassRim);
	const FMargin TextColumnPadding(14.f, 2.f, 0.f, 0.f);
	const FMargin DescriptionPadding(0.f, 4.f, 0.f, 0.f);
	const FMargin TierBadgePadding(8.f, 2.f, 8.f, 3.f);
	const FMargin TierBadgeSlotPadding(8.f, 0.f, 0.f, 0.f);
	const FMargin PreviewHintPadding(0.f, 3.f, 0.f, 0.f);
	constexpr float TierBadgeMinWidth = 74.f;
	constexpr int32 TitleFontSize = 16;
	constexpr int32 DescriptionFontSize = 13;
	constexpr int32 TierFontSize = 12;
	constexpr int32 TierLetterSpacing = 120;
	constexpr int32 MissingIconFontSize = 34;
	constexpr int32 PreviewHintFontSize = 10;
	constexpr int32 PreviewHintLetterSpacing = 200;
	constexpr float PreviewHintIdleOpacity = 0.35f;
}

// The wide glass of the faction buttons (Docs/UI/FactionSelectionGlass.md) on the shared M_UI_GlassPane shader.
namespace CommanderAbilityRowGlass
{
	const TCHAR* const GlassMaterialPath =
		TEXT("/Game/RTS_Survival/FactionSelection/UI/Materials/MI_FactionButton_Glass.MI_FactionButton_Glass");

	// Scalars on M_UI_GlassPane (ui_glass_panes.py), as UW_ControlGroupImage drives them.
	const FName HoverParameter = TEXT("Hover");
	const FName HoverSweepParameter = TEXT("HoverSweep");
	const FName PressedParameter = TEXT("Pressed");
	const FName OccupiedParameter = TEXT("Occupied");
	const FName ScratchSeedParameter = TEXT("ScratchSeed");
	const FName StreakOffsetParameter = TEXT("StreakOffset");
	// The faction buttons' selection parameters (UW_FactionSelectionMenu::UpdateFactionButtonSelection): the row's
	// rim, reflection streak and edge tint rise with the hover, in a cool tint where a selected faction is warm.
	const FName RimParameter = TEXT("RimIntensity");
	const FName StreakParameter = TEXT("StreakIntensity");
	const FName TintParameter = TEXT("EdgeTint");
	constexpr float IdleRimIntensity = 0.26f;
	constexpr float HoverRimIntensity = 0.95f;
	constexpr float IdleStreakIntensity = 0.045f;
	constexpr float HoverStreakIntensity = 0.12f;
	const FLinearColor IdleEdgeTint(0.80f, 0.90f, 1.0f, 0.06f);
	const FLinearColor HoverEdgeTint(0.35f, 0.85f, 1.0f, 0.34f);

	constexpr float HoverInSeconds = 0.12f;
	constexpr float HoverOutSeconds = 0.22f;
	constexpr float SweepSeconds = 0.32f;
	constexpr float PressSeconds = 0.05f;
	constexpr float MinParameterChange = 0.002f;
	// A row without a template hovers weaker: it cannot be previewed.
	constexpr float NoPreviewHoverStrength = 0.35f;

	// Every row is scratched differently, within a range that still reads as one set.
	constexpr float ScratchSeedBase = 11.f;
	constexpr float ScratchSeedStep = 6.7f;
	constexpr float StreakOffsetCentre = 0.22f;
	constexpr float StreakOffsetStep = 0.035f;
}

void UW_CommanderAbilityRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_CommanderAbilityRow::BuildWidgetTree()
{
	using namespace CommanderAbilityRowLayout;
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	// Never takes keyboard focus, so the menu keeps the arrow keys for the commander list.
	M_RowButton = WidgetTree->ConstructWidget<UUnitVariantSelectionButton>(UUnitVariantSelectionButton::StaticClass());
	FButtonStyle GlassOnlyStyle;
	FSlateBrush NoBrush;
	NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	GlassOnlyStyle.SetNormal(NoBrush);
	GlassOnlyStyle.SetHovered(NoBrush);
	GlassOnlyStyle.SetPressed(NoBrush);
	GlassOnlyStyle.SetDisabled(NoBrush);
	GlassOnlyStyle.SetNormalPadding(FMargin(0.f));
	GlassOnlyStyle.SetPressedPadding(FMargin(0.f));
	M_RowButton->SetStyle(GlassOnlyStyle);
	M_RowButton->OnClicked.AddDynamic(this, &UW_CommanderAbilityRow::OnRowClicked);
	WidgetTree->RootWidget = M_RowButton;

	UOverlay* const RowOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	if (UButtonSlot* const ContentSlot = Cast<UButtonSlot>(M_RowButton->SetContent(RowOverlay)))
	{
		ContentSlot->SetPadding(FMargin(0.f));
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
	}
	BuildGlassPane(RowOverlay);

	UHorizontalBox* const RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	// Not HitTestInvisible: the tier badge must take the hover for its tooltip.
	RowBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UOverlaySlot* const RowSlot = RowOverlay->AddChildToOverlay(RowBox);
	RowSlot->SetHorizontalAlignment(HAlign_Fill);
	RowSlot->SetVerticalAlignment(VAlign_Fill);
	RowSlot->SetPadding(RowContentPadding);
	BuildIconWell(RowBox);
	BuildTexts(RowBox);
}

void UW_CommanderAbilityRow::BuildGlassPane(UOverlay* RowOverlay)
{
	using namespace CommanderAbilityRowGlass;
	M_GlassPane = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	M_GlassPane->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const GlassSlot = RowOverlay->AddChildToOverlay(M_GlassPane);
	GlassSlot->SetHorizontalAlignment(HAlign_Fill);
	GlassSlot->SetVerticalAlignment(VAlign_Fill);

	UMaterialInterface* const GlassMaterial = LoadObject<UMaterialInterface>(nullptr, GlassMaterialPath);
	if (not IsValid(GlassMaterial))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("The commander ability row could not load its glass material %s."
				"\nAt function: UW_CommanderAbilityRow::BuildGlassPane"), GlassMaterialPath));
		M_GlassPane->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	M_GlassMaterial = UMaterialInstanceDynamic::Create(GlassMaterial, this);
	FSlateBrush GlassBrush;
	GlassBrush.SetResourceObject(M_GlassMaterial);
	GlassBrush.DrawAs = ESlateBrushDrawType::Image;
	M_GlassPane->SetBrush(GlassBrush);
	M_GlassMaterial->SetScalarParameterValue(OccupiedParameter, 0.f);
	M_GlassMaterial->SetScalarParameterValue(RimParameter, IdleRimIntensity);
	M_GlassMaterial->SetScalarParameterValue(StreakParameter, IdleStreakIntensity);
	M_GlassMaterial->SetVectorParameterValue(TintParameter, IdleEdgeTint);
}

void UW_CommanderAbilityRow::BuildIconWell(UHorizontalBox* RowBox)
{
	using namespace CommanderAbilityRowLayout;
	USizeBox* const WellBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WellBox->SetWidthOverride(IconWellSize);
	WellBox->SetHeightOverride(IconWellSize);
	WellBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UHorizontalBoxSlot* const WellSlot = RowBox->AddChildToHorizontalBox(WellBox);
	WellSlot->SetVerticalAlignment(VAlign_Top);

	M_IconWell = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_IconWell->SetPadding(IconWellPadding);
	M_IconWell->SetHorizontalAlignment(HAlign_Center);
	M_IconWell->SetVerticalAlignment(VAlign_Center);
	WellBox->SetContent(M_IconWell);
	UOverlay* const WellOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	M_IconWell->SetContent(WellOverlay);
	M_Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	WellOverlay->AddChildToOverlay(M_Icon);
	M_MissingIconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_MissingIconText->SetText(NSLOCTEXT("CommanderSelection", "MissingIcon", "?"));
	UOverlaySlot* const MissingSlot = WellOverlay->AddChildToOverlay(M_MissingIconText);
	MissingSlot->SetHorizontalAlignment(HAlign_Center);
	MissingSlot->SetVerticalAlignment(VAlign_Center);
}

void UW_CommanderAbilityRow::BuildTexts(UHorizontalBox* RowBox)
{
	using namespace CommanderAbilityRowLayout;
	UVerticalBox* const TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	TextColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UHorizontalBoxSlot* const ColumnSlot = RowBox->AddChildToHorizontalBox(TextColumn);
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnSlot->SetPadding(TextColumnPadding);

	UHorizontalBox* const TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	TitleRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	TextColumn->AddChildToVerticalBox(TitleRow);
	M_TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TitleText->SetAutoWrapText(true);
	M_TitleText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UHorizontalBoxSlot* const TitleSlot = TitleRow->AddChildToHorizontalBox(M_TitleText);
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBox* const BadgeColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BadgeColumn->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UHorizontalBoxSlot* const BadgeSlot = TitleRow->AddChildToHorizontalBox(BadgeColumn);
	BadgeSlot->SetVerticalAlignment(VAlign_Top);
	BadgeSlot->SetPadding(TierBadgeSlotPadding);
	BadgeColumn->AddChildToVerticalBox(BuildTierBadge());
	M_PreviewHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_PreviewHintText->SetText(NSLOCTEXT("CommanderSelection", "PreviewHint", "PREVIEW"));
	M_PreviewHintText->SetJustification(ETextJustify::Center);
	M_PreviewHintText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBoxSlot* const HintSlot = BadgeColumn->AddChildToVerticalBox(M_PreviewHintText);
	HintSlot->SetHorizontalAlignment(HAlign_Fill);
	HintSlot->SetPadding(PreviewHintPadding);

	M_DescriptionText = WidgetTree->ConstructWidget<URichTextBlock>(URichTextBlock::StaticClass());
	M_DescriptionText->SetAutoWrapText(true);
	TextColumn->AddChildToVerticalBox(M_DescriptionText)->SetPadding(DescriptionPadding);
}

UWidget* UW_CommanderAbilityRow::BuildTierBadge()
{
	using namespace CommanderAbilityRowLayout;
	USizeBox* const BadgeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BadgeBox->SetMinDesiredWidth(TierBadgeMinWidth);
	BadgeBox->SetVisibility(ESlateVisibility::Visible);
	BadgeBox->SetToolTipText(NSLOCTEXT("CommanderSelection", "TierTooltip",
	                                   "Each operation sets the highest tier you may call in; abilities above it stay "
	                                   "locked until a later operation."));
	M_TierBadge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	M_TierBadge->SetPadding(TierBadgePadding);
	M_TierBadge->SetHorizontalAlignment(HAlign_Center);
	M_TierBadge->SetVerticalAlignment(VAlign_Center);
	BadgeBox->SetContent(M_TierBadge);
	M_TierText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_TierBadge->SetContent(M_TierText);
	return BadgeBox;
}

void UW_CommanderAbilityRow::SetupRow(UW_CommanderSelectionMenu* Menu, const int32 SlotIndex,
                                      const FUnitVariantSelectionStyle& Style, UDataTable* RichTextStyle,
                                      const FCommanderSelectionAbilitySlot& AbilitySlot)
{
	using namespace CommanderAbilityRowLayout;
	M_Menu = Menu;
	M_SlotIndex = SlotIndex;
	bM_CanPreview = AbilitySlot.bHasTemplate;
	if (not GetIsValidDescriptionText())
	{
		return;
	}
	M_IconWell->SetBrush(Style.UnitIconWell);
	ApplyIcon(AbilitySlot);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_MissingIconText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, MissingIconFontSize),
		Style.MutedTextColor);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_TitleText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TitleFontSize),
		AbilitySlot.bHasTemplate ? Style.TextColor : Style.MutedTextColor);
	M_TitleText->SetText(AbilitySlot.Title.ToUpper());
	ApplyTierBadge(Style, AbilitySlot);
	if (IsValid(RichTextStyle))
	{
		M_DescriptionText->SetTextStyleSet(RichTextStyle);
	}
	// Untagged text takes the screen's body font: the style set's monospace default made a kit overflow the panel.
	M_DescriptionText->SetDefaultFont(UnitVariantSelectionWidgets::MakeFont(Style.BodyFont, DescriptionFontSize));
	M_DescriptionText->SetDefaultColorAndOpacity(Style.DescriptionColor);
	M_DescriptionText->SetDefaultShadowOffset(UnitVariantSelectionWidgets::TextShadowOffset);
	M_DescriptionText->SetDefaultShadowColorAndOpacity(UnitVariantSelectionWidgets::TextShadowColor);
	M_DescriptionText->SetText(AbilitySlot.Description);
	M_DescriptionText->SetVisibility(AbilitySlot.Description.IsEmpty()
		                                 ? ESlateVisibility::Collapsed
		                                 : ESlateVisibility::HitTestInvisible);

	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		M_PreviewHintText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, PreviewHintFontSize,
		                                                         PreviewHintLetterSpacing), Style.AccentColor);
	M_PreviewHintText->SetVisibility(bM_CanPreview ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	M_PreviewHintText->SetRenderOpacity(PreviewHintIdleOpacity);
	// Rows without a preview explain why the click does nothing; previewable rows say it with the lit PREVIEW label,
	// a tooltip would cover the description the player is reading.
	if (not bM_CanPreview)
	{
		M_RowButton->SetToolTipText(NSLOCTEXT("CommanderSelection", "NoPreviewTooltip",
		                                      "This ability has no preview yet."));
	}
	ApplyGlassWear(SlotIndex);
}

void UW_CommanderAbilityRow::ApplyIcon(const FCommanderSelectionAbilitySlot& AbilitySlot)
{
	using namespace CommanderAbilityRowLayout;
	const bool bHasIcon = IsValid(AbilitySlot.Icon);
	if (bHasIcon)
	{
		// SetBrushFromTexture keeps the default 32 unit image size; the icons are drawn for the 64 unit well.
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(AbilitySlot.Icon);
		IconBrush.ImageSize = FVector2D(IconSize, IconSize);
		M_Icon->SetBrush(IconBrush);
	}
	// An empty brush would draw Slate's default white box.
	M_Icon->SetVisibility(bHasIcon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	M_MissingIconText->SetVisibility(bHasIcon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UW_CommanderAbilityRow::ApplyTierBadge(const FUnitVariantSelectionStyle& Style,
                                            const FCommanderSelectionAbilitySlot& AbilitySlot)
{
	using namespace CommanderAbilityRowLayout;
	const FText TierNumeral = AbilitySlot.bHasTemplate
		                          ? CommanderSelectionData::GetTierNumeral(AbilitySlot.Tier)
		                          : NSLOCTEXT("CommanderSelection", "TierPending", "?");
	M_TierText->SetText(FText::Format(NSLOCTEXT("CommanderSelection", "TierBadge", "TIER {0}"), TierNumeral));
	M_TierText->SetFont(UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, TierFontSize, TierLetterSpacing));
	M_TierText->SetColorAndOpacity(CommanderSelectionData::GetTierBadgeTextColor());
	M_TierBadge->SetBrushColor(CommanderSelectionData::GetTierBadgeColor(AbilitySlot.Tier, AbilitySlot.bHasTemplate));
}

void UW_CommanderAbilityRow::ApplyGlassWear(const int32 SlotIndex)
{
	using namespace CommanderAbilityRowGlass;
	if (not IsValid(M_GlassMaterial))
	{
		return;
	}
	M_GlassMaterial->SetScalarParameterValue(ScratchSeedParameter, ScratchSeedBase + SlotIndex * ScratchSeedStep);
	const float StreakOffset = StreakOffsetCentre + ((SlotIndex % 3) - 1) * StreakOffsetStep;
	M_GlassMaterial->SetScalarParameterValue(StreakOffsetParameter, StreakOffset);
}

UMaterialInterface* UW_CommanderAbilityRow::GetGlassSourceMaterial() const
{
	return IsValid(M_GlassMaterial) ? M_GlassMaterial->Parent.Get() : nullptr;
}

void UW_CommanderAbilityRow::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickGlass(InDeltaTime);
}

void UW_CommanderAbilityRow::TickGlass(const float DeltaSeconds)
{
	using namespace CommanderAbilityRowGlass;
	if (not IsValid(M_GlassMaterial) || not IsValid(M_RowButton))
	{
		return;
	}
	const bool bIsHovered = M_RowButton->IsHovered();
	if (bIsHovered && not bM_WasHovered)
	{
		// One light band across the pane per mouse enter.
		M_SweepProgress = 0.f;
	}
	bM_WasHovered = bIsHovered;
	const float HoverSeconds = bIsHovered ? HoverInSeconds : HoverOutSeconds;
	M_HoverRamp = FMath::FInterpConstantTo(M_HoverRamp, bIsHovered ? 1.f : 0.f, DeltaSeconds, 1.f / HoverSeconds);
	M_SweepProgress = FMath::Min(1.f, M_SweepProgress + DeltaSeconds / SweepSeconds);
	const bool bIsPressed = M_RowButton->IsPressed();
	M_PressRamp = FMath::FInterpConstantTo(M_PressRamp, bIsPressed ? 1.f : 0.f, DeltaSeconds, 1.f / PressSeconds);

	const float HoverStrength = bM_CanPreview ? 1.f : NoPreviewHoverStrength;
	const float Hover = FMath::SmoothStep(0.f, 1.f, M_HoverRamp) * HoverStrength;
	// Ease out: the band enters quickly and settles.
	const float Sweep = 1.f - FMath::Square(1.f - M_SweepProgress);
	if (not FMath::IsNearlyEqual(Hover, M_SentHover, MinParameterChange))
	{
		M_GlassMaterial->SetScalarParameterValue(HoverParameter, Hover);
		M_GlassMaterial->SetScalarParameterValue(RimParameter, FMath::Lerp(IdleRimIntensity, HoverRimIntensity, Hover));
		M_GlassMaterial->SetScalarParameterValue(StreakParameter,
		                                         FMath::Lerp(IdleStreakIntensity, HoverStreakIntensity, Hover));
		M_GlassMaterial->SetVectorParameterValue(TintParameter, FMath::Lerp(IdleEdgeTint, HoverEdgeTint, Hover));
		M_SentHover = Hover;
		if (IsValid(M_PreviewHintText))
		{
			M_PreviewHintText->SetRenderOpacity(FMath::Lerp(CommanderAbilityRowLayout::PreviewHintIdleOpacity, 1.f,
			                                                Hover));
		}
	}
	if (not FMath::IsNearlyEqual(Sweep, M_SentSweep, MinParameterChange))
	{
		M_GlassMaterial->SetScalarParameterValue(HoverSweepParameter, Sweep);
		M_SentSweep = Sweep;
	}
	if (not FMath::IsNearlyEqual(M_PressRamp, M_SentPressed, MinParameterChange))
	{
		M_GlassMaterial->SetScalarParameterValue(PressedParameter, M_PressRamp);
		M_SentPressed = M_PressRamp;
	}
}

void UW_CommanderAbilityRow::OnRowClicked()
{
	if (not bM_CanPreview || not M_Menu.IsValid())
	{
		return;
	}
	M_Menu->HandleAbilityRowClicked(M_SlotIndex);
}

bool UW_CommanderAbilityRow::GetIsValidDescriptionText() const
{
	if (IsValid(M_DescriptionText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_DescriptionText",
	                                                             "GetIsValidDescriptionText", this);
	return false;
}
