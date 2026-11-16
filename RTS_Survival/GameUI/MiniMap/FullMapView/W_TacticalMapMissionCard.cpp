// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "W_TacticalMapMissionCard.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/RichTextBlock.h"
#include "Components/RichTextBlockDecorator.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/DataTable.h"
#include "Styling/CoreStyle.h"
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace TacticalMapMissionCardLayout
{
	// MI_FactionButton_Glass draws its rim about 13 units in from the sides and 10 from the top and bottom; the texts
	// keep a clear gap inside that rim (see UW_CommanderAbilityRow).
	const FMargin GlassRimInset(13.f, 10.f, 13.f, 10.f);
	constexpr float GapInsideGlassRim = 6.f;
	const FMargin ContentPadding = GlassRimInset + FMargin(GapInsideGlassRim);
	constexpr float AccentBarWidth = 3.f;
	const FMargin TextColumnPadding(10.f, 0.f, 0.f, 0.f);
	const FMargin DescriptionPadding(0.f, 4.f, 0.f, 0.f);
	const FLinearColor AccentColor(0.0f, 0.9f, 1.0f, 1.0f);
	constexpr float AccentIdleOpacity = 0.4f;

	// The tactical map's panel fonts (UW_FullMapView); tags come from the game's shared rich text table.
	const TCHAR* const RichTextStylePath =
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/WeaponUI/AmmoButtonWidgets/DT_RichText.DT_RichText");
	constexpr int32 TitleFontSize = 14;
	constexpr int32 DescriptionFontSize = 12;
	constexpr int32 TextOutlineSize = 1;
	const FLinearColor TitleColor(0.86f, 0.93f, 0.96f, 1.0f);
	const FLinearColor DescriptionColor(0.68f, 0.75f, 0.79f, 1.0f);
}

namespace TacticalMapMissionCardGlass
{
	const TCHAR* const GlassMaterialPath =
		TEXT("/Game/RTS_Survival/FactionSelection/UI/Materials/MI_FactionButton_Glass.MI_FactionButton_Glass");

	// Scalars on M_UI_GlassPane (ui_glass_panes.py) beyond the hover ones FGlassHoverAnimator writes.
	const FName OccupiedParameter = TEXT("Occupied");
	const FName ScratchSeedParameter = TEXT("ScratchSeed");
	const FName StreakOffsetParameter = TEXT("StreakOffset");
	// The faction buttons' selection parameters: rim, reflection streak and edge tint rise with the hover in the
	// same cool tint as the commander ability rows.
	const FName RimParameter = TEXT("RimIntensity");
	const FName StreakParameter = TEXT("StreakIntensity");
	const FName TintParameter = TEXT("EdgeTint");
	constexpr float IdleRimIntensity = 0.26f;
	constexpr float HoverRimIntensity = 0.95f;
	constexpr float IdleStreakIntensity = 0.045f;
	constexpr float HoverStreakIntensity = 0.12f;
	const FLinearColor IdleEdgeTint(0.80f, 0.90f, 1.0f, 0.06f);
	const FLinearColor HoverEdgeTint(0.35f, 0.85f, 1.0f, 0.34f);
	constexpr float MinParameterChange = 0.002f;

	// Every card is scratched differently, within a range that still reads as one set.
	constexpr float ScratchSeedBase = 23.f;
	constexpr float ScratchSeedStep = 6.7f;
	constexpr float StreakOffsetCentre = 0.22f;
	constexpr float StreakOffsetStep = 0.035f;
}

void UW_TacticalMapMissionCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_TacticalMapMissionCard::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	// Visible (hit-testable) so the card itself reports the hover; everything inside ignores the mouse.
	UOverlay* const CardOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MissionCard"));
	CardOverlay->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = CardOverlay;
	BuildGlassPane(*CardOverlay);
	BuildContent(*CardOverlay);
}

void UW_TacticalMapMissionCard::BuildGlassPane(UOverlay& CardOverlay)
{
	using namespace TacticalMapMissionCardGlass;
	// Same name as the HUD's glass panes, so -UISnapshotHideGlass hides these too.
	M_GlassPane = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("M_GlassPane"));
	M_GlassPane->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const GlassSlot = CardOverlay.AddChildToOverlay(M_GlassPane);
	GlassSlot->SetHorizontalAlignment(HAlign_Fill);
	GlassSlot->SetVerticalAlignment(VAlign_Fill);

	UMaterialInterface* const GlassMaterial = LoadObject<UMaterialInterface>(nullptr, GlassMaterialPath);
	if (not IsValid(GlassMaterial))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("The tactical map mission card could not load its glass material %s."
				"\nAt function: UW_TacticalMapMissionCard::BuildGlassPane"), GlassMaterialPath));
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
	M_GlassAnimator.ApplyTo(*M_GlassMaterial);
}

void UW_TacticalMapMissionCard::BuildContent(UOverlay& CardOverlay)
{
	using namespace TacticalMapMissionCardLayout;
	UHorizontalBox* const ContentRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
	                                                                              TEXT("MissionCardContent"));
	ContentRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const ContentSlot = CardOverlay.AddChildToOverlay(ContentRow);
	ContentSlot->SetHorizontalAlignment(HAlign_Fill);
	ContentSlot->SetVerticalAlignment(VAlign_Fill);
	ContentSlot->SetPadding(ContentPadding);

	USizeBox* const AccentBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	AccentBox->SetWidthOverride(AccentBarWidth);
	UHorizontalBoxSlot* const AccentSlot = ContentRow->AddChildToHorizontalBox(AccentBox);
	AccentSlot->SetVerticalAlignment(VAlign_Fill);
	M_AccentBar = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MissionAccentBar"));
	M_AccentBar->SetColorAndOpacity(AccentColor);
	M_AccentBar->SetRenderOpacity(AccentIdleOpacity);
	AccentBox->SetContent(M_AccentBar);

	M_TextColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MissionTextColumn"));
	UHorizontalBoxSlot* const ColumnSlot = ContentRow->AddChildToHorizontalBox(M_TextColumn);
	ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ColumnSlot->SetPadding(TextColumnPadding);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
}

void UW_TacticalMapMissionCard::SetupMissionCard(const int32 CardIndex)
{
	using namespace TacticalMapMissionCardGlass;
	if (not IsValid(M_GlassMaterial))
	{
		return;
	}
	M_GlassMaterial->SetScalarParameterValue(ScratchSeedParameter, ScratchSeedBase + CardIndex * ScratchSeedStep);
	const float StreakOffset = StreakOffsetCentre + ((CardIndex % 3) - 1) * StreakOffsetStep;
	M_GlassMaterial->SetScalarParameterValue(StreakOffsetParameter, StreakOffset);
}

void UW_TacticalMapMissionCard::ShowMission(const UW_Mission& MissionWidget)
{
	const URichTextBlock* const SourceTitle = MissionWidget.GetRichTitleBlock();
	const URichTextBlock* const SourceDescription = MissionWidget.GetRichDescriptionBlock();
	if (not IsValid(SourceTitle) || not IsValid(SourceDescription))
	{
		RTSFunctionLibrary::ReportError("The tactical map mission card got a mission widget without text blocks: "
			+ MissionWidget.GetName() + "\nAt function: UW_TacticalMapMissionCard::ShowMission");
		return;
	}
	if (not IsValid(M_TitleText))
	{
		M_TitleText = CreateText(*SourceTitle, true);
	}
	if (not IsValid(M_DescriptionText))
	{
		M_DescriptionText = CreateText(*SourceDescription, false);
	}
	const FMissionWidgetState& WidgetState = MissionWidget.GetShownWidgetState();
	SetTextIfChanged(M_TitleText, M_ShownTitle, WidgetState.MissionTitle);
	SetTextIfChanged(M_DescriptionText, M_ShownDescription, WidgetState.MissionDescription);
	if (IsValid(M_DescriptionText))
	{
		M_DescriptionText->SetVisibility(WidgetState.MissionDescription.IsEmpty()
			                                 ? ESlateVisibility::Collapsed
			                                 : ESlateVisibility::HitTestInvisible);
	}
}

URichTextBlock* UW_TacticalMapMissionCard::CreateText(const URichTextBlock& Source, const bool bIsTitle)
{
	using namespace TacticalMapMissionCardLayout;
	if (not IsValid(M_TextColumn))
	{
		return nullptr;
	}
	URichTextBlock* const TextBlock = WidgetTree->ConstructWidget<URichTextBlock>(
		URichTextBlock::StaticClass(), bIsTitle ? TEXT("MissionTitle") : TEXT("MissionDescription"));
	UDataTable* const RichTextStyle = LoadObject<UDataTable>(nullptr, RichTextStylePath);
	if (IsValid(RichTextStyle))
	{
		TextBlock->SetTextStyleSet(RichTextStyle);
	}
	else
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("The tactical map mission card could not load its rich text style %s."
				"\nAt function: UW_TacticalMapMissionCard::CreateText"), RichTextStylePath));
	}
	CopyDecorators(Source, *TextBlock);
	// Untagged text takes the map panel's font, not the style set's default.
	TextBlock->SetDefaultFont(FCoreStyle::GetDefaultFontStyle(bIsTitle ? TEXT("Bold") : TEXT("Regular"),
	                                                          bIsTitle ? TitleFontSize : DescriptionFontSize,
	                                                          FFontOutlineSettings(TextOutlineSize)));
	TextBlock->SetDefaultColorAndOpacity(bIsTitle ? TitleColor : DescriptionColor);
	TextBlock->SetAutoWrapText(true);
	TextBlock->SetJustification(ETextJustify::Left);
	TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBoxSlot* const TextSlot = M_TextColumn->AddChildToVerticalBox(TextBlock);
	TextSlot->SetHorizontalAlignment(HAlign_Fill);
	if (not bIsTitle)
	{
		TextSlot->SetPadding(DescriptionPadding);
	}
	return TextBlock;
}

void UW_TacticalMapMissionCard::CopyDecorators(const URichTextBlock& Source, URichTextBlock& Target)
{
	// Inline images and similar decorators the mission texts may use; the list has no public getter.
	const FArrayProperty* const DecoratorsProperty = FindFProperty<FArrayProperty>(
		URichTextBlock::StaticClass(), TEXT("DecoratorClasses"));
	if (DecoratorsProperty == nullptr)
	{
		return;
	}
	const TArray<TSubclassOf<URichTextBlockDecorator>>* const SourceDecorators =
		DecoratorsProperty->ContainerPtrToValuePtr<TArray<TSubclassOf<URichTextBlockDecorator>>>(&Source);
	if (SourceDecorators != nullptr && SourceDecorators->Num() > 0)
	{
		Target.SetDecorators(*SourceDecorators);
	}
}

void UW_TacticalMapMissionCard::SetTextIfChanged(URichTextBlock* TextBlock, FText& InOutShownText,
                                                 const FText& NewText) const
{
	if (not IsValid(TextBlock) || InOutShownText.IdenticalTo(NewText) || InOutShownText.EqualTo(NewText))
	{
		return;
	}
	InOutShownText = NewText;
	TextBlock->SetText(NewText);
}

void UW_TacticalMapMissionCard::SetForcedHover(const bool bForceHover, const float FrozenSweepProgress)
{
	bM_IsForcedHover = bForceHover;
	M_GlassAnimator.FrozenSweepProgress = FrozenSweepProgress;
}

void UW_TacticalMapMissionCard::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickGlass(InDeltaTime);
}

void UW_TacticalMapMissionCard::TickGlass(const float DeltaSeconds)
{
	using namespace TacticalMapMissionCardGlass;
	const bool bIsHovered = bM_IsForcedHover || IsHovered();
	if (not M_GlassAnimator.Tick(DeltaSeconds, bIsHovered, false, M_GlassTimings))
	{
		return;
	}
	const float Hover = M_GlassAnimator.GetHover();
	if (IsValid(M_GlassMaterial))
	{
		M_GlassAnimator.ApplyTo(*M_GlassMaterial);
		if (not FMath::IsNearlyEqual(Hover, M_SentRimHover, MinParameterChange))
		{
			M_GlassMaterial->SetScalarParameterValue(RimParameter,
			                                         FMath::Lerp(IdleRimIntensity, HoverRimIntensity, Hover));
			M_GlassMaterial->SetScalarParameterValue(StreakParameter,
			                                         FMath::Lerp(IdleStreakIntensity, HoverStreakIntensity, Hover));
			M_GlassMaterial->SetVectorParameterValue(TintParameter, FMath::Lerp(IdleEdgeTint, HoverEdgeTint, Hover));
			M_SentRimHover = Hover;
		}
	}
	if (IsValid(M_AccentBar))
	{
		M_AccentBar->SetRenderOpacity(FMath::Lerp(TacticalMapMissionCardLayout::AccentIdleOpacity, 1.f, Hover));
	}
}
