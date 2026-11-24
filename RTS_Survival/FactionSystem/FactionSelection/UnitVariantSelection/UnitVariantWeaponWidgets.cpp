// Copyright (C) Bas Blokzijl - All rights reserved.

#include "UnitVariantWeaponWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Slate/SlateBrushAsset.h"
#include "UnitVariantAbilityCards.h"
#include "UnitVariantProbes.h"
#include "UnitVariantSelectionStyle.h"
#include "UnitVariantSelectionWidgetHelpers.h"
#include "UnitVariantStatComparison.h"

namespace UnitVariantWeaponWidgetsLayout
{
	const TCHAR* const WeaponIconColumn = TEXT("Slate");
	constexpr int32 CalibreFallbackFontSize = 14;
	// Weapon stat bars: a pictogram, then one pip per level.
	constexpr float StatIconSize = 16.f;
	const FMargin StatIconPadding(0.f, 0.f, 6.f, 0.f);
	const FVector2D StatPipSize(9.f, 10.f);
	constexpr float StatPipGap = 2.f;
	constexpr float StatBarSpacing = 3.f;
	const FMargin StatBarsPadding(0.f, 2.f, 0.f, 0.f);
	const FLinearColor EmptyPipColor(1.f, 1.f, 1.f, 0.12f);
}

UnitVariantWeaponWidgets::FStatIcons UnitVariantWeaponWidgets::LoadStatIcons(const FUnitVariantSelectionStyle& Style)
{
	FStatIcons StatIcons;
	StatIcons.Damage = Style.DamageStatIcon.LoadSynchronous();
	StatIcons.Penetration = Style.PenetrationStatIcon.LoadSynchronous();
	StatIcons.Reload = Style.ReloadStatIcon.LoadSynchronous();
	StatIcons.Magazine = Style.MagazineStatIcon.LoadSynchronous();
	return StatIcons;
}

UWidget* UnitVariantWeaponWidgets::BuildWeaponIcon(UWidgetTree& Tree, const FUnitVariantSelectionStyle& Style,
                                                   const UDataTable* WeaponCardTable,
                                                   const FUnitVariantWeaponStats& Weapon)
{
	using namespace UnitVariantWeaponWidgetsLayout;
	if (USlateBrushAsset* const IconBrush = Cast<USlateBrushAsset>(UnitVariantAbilityCards::FindRowObject(
		WeaponCardTable, Weapon.WeaponRowName, WeaponIconColumn)))
	{
		// Weapon icons are not all square; the scale box keeps their aspect inside the well.
		UScaleBox* const IconScale = Tree.ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		IconScale->SetStretch(EStretch::ScaleToFit);
		UImage* const IconImage = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromAsset(IconBrush);
		IconScale->SetContent(IconImage);
		return IconScale;
	}
	// Not every weapon has a WeaponUI_Data row (the weapon bar shows no icon for those either): the calibre
	// keeps the well from looking broken until the art exists.
	UTextBlock* const CalibreText = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	CalibreText->SetText(FText::Format(NSLOCTEXT("UnitVariantSelection", "CalibreFallback", "{0}\nmm"),
	                                   FText::AsNumber(FMath::RoundToInt(Weapon.CalibreMm))));
	CalibreText->SetJustification(ETextJustify::Center);
	UnitVariantSelectionWidgets::ApplyTextOnMetal(
		CalibreText, UnitVariantSelectionWidgets::MakeFont(Style.HeadingFont, CalibreFallbackFontSize),
		Style.MutedTextColor);
	UScaleBox* const CalibreScale = Tree.ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	CalibreScale->SetStretch(EStretch::ScaleToFit);
	CalibreScale->SetStretchDirection(EStretchDirection::DownOnly);
	CalibreScale->SetContent(CalibreText);
	return CalibreScale;
}

UWidget* UnitVariantWeaponWidgets::BuildStatBar(UWidgetTree& Tree, const FUnitVariantSelectionStyle& Style,
                                                UTexture2D* StatIcon, const int32 Level,
                                                const EUnitVariantStatTrend Trend)
{
	using namespace UnitVariantWeaponWidgetsLayout;
	UHorizontalBox* const Bar = Tree.ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* const IconBox = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
	IconBox->SetWidthOverride(StatIconSize);
	IconBox->SetHeightOverride(StatIconSize);
	if (IsValid(StatIcon))
	{
		UImage* const IconImage = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		IconImage->SetBrushFromTexture(StatIcon);
		IconImage->SetColorAndOpacity(Style.DescriptionColor.GetSpecifiedColor());
		IconBox->SetContent(IconImage);
	}
	UHorizontalBoxSlot* const IconSlot = Bar->AddChildToHorizontalBox(IconBox);
	IconSlot->SetVerticalAlignment(VAlign_Center);
	IconSlot->SetPadding(StatIconPadding);
	const FSlateColor& FilledPipStyleColor = Trend == EUnitVariantStatTrend::Better
		                                         ? Style.BetterColor
		                                         : Trend == EUnitVariantStatTrend::Worse
		                                         ? Style.RemovedColor
		                                         : Style.AccentColor;
	const FLinearColor FilledPipColor = FilledPipStyleColor.GetSpecifiedColor();
	for (int32 PipIndex = 0; PipIndex < UnitVariantStatComparison::MaxStatLevel; ++PipIndex)
	{
		USizeBox* const PipBox = Tree.ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PipBox->SetWidthOverride(StatPipSize.X);
		PipBox->SetHeightOverride(StatPipSize.Y);
		// The default brush is a plain white quad; the tint makes it a filled or an empty pip.
		UImage* const Pip = Tree.ConstructWidget<UImage>(UImage::StaticClass());
		Pip->SetColorAndOpacity(PipIndex < Level ? FilledPipColor : EmptyPipColor);
		PipBox->SetContent(Pip);
		UHorizontalBoxSlot* const PipSlot = Bar->AddChildToHorizontalBox(PipBox);
		PipSlot->SetVerticalAlignment(VAlign_Center);
		PipSlot->SetPadding(FMargin(0.f, 0.f, StatPipGap, 0.f));
	}
	return Bar;
}

UWidget* UnitVariantWeaponWidgets::BuildStatBars(UWidgetTree& Tree, const FUnitVariantSelectionStyle& Style,
                                                 const FStatIcons& StatIcons, const FUnitVariantWeaponStats& Weapon,
                                                 const FUnitVariantWeaponStats* BaseWeapon)
{
	using namespace UnitVariantWeaponWidgetsLayout;
	// Pips turn green or red where the weapon beats or trails the Base weapon it replaces.
	const auto TrendAgainstBase = [BaseWeapon](const float Value, const float BaseValue, const bool bHigherIsBetter)
	{
		return BaseWeapon != nullptr
			       ? UnitVariantStatComparison::GetTrend(Value, BaseValue, bHigherIsBetter)
			       : EUnitVariantStatTrend::Same;
	};
	const float NoBaseValue = 0.f;
	UVerticalBox* const StatBars = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	StatBars->AddChildToVerticalBox(BuildStatBar(
		Tree, Style, StatIcons.Damage, UnitVariantStatComparison::GetDamageLevel(Weapon.Damage),
		TrendAgainstBase(Weapon.Damage, BaseWeapon != nullptr ? BaseWeapon->Damage : NoBaseValue, true)))->SetPadding(
		StatBarsPadding);
	StatBars->AddChildToVerticalBox(BuildStatBar(
		Tree, Style, StatIcons.Penetration, UnitVariantStatComparison::GetPenetrationLevel(Weapon.ArmorPen),
		TrendAgainstBase(Weapon.ArmorPen, BaseWeapon != nullptr ? BaseWeapon->ArmorPen : NoBaseValue, true)))->
		SetPadding(FMargin(0.f, StatBarSpacing, 0.f, 0.f));
	StatBars->AddChildToVerticalBox(BuildStatBar(
		Tree, Style, StatIcons.Reload, UnitVariantStatComparison::GetReloadLevel(Weapon.ReloadSeconds),
		TrendAgainstBase(Weapon.ReloadSeconds, BaseWeapon != nullptr ? BaseWeapon->ReloadSeconds : NoBaseValue,
		                 false)))->SetPadding(FMargin(0.f, StatBarSpacing, 0.f, 0.f));
	StatBars->AddChildToVerticalBox(BuildStatBar(
		Tree, Style, StatIcons.Magazine, UnitVariantStatComparison::GetMagazineLevel(Weapon.MagCapacity),
		TrendAgainstBase(Weapon.MagCapacity, BaseWeapon != nullptr ? BaseWeapon->MagCapacity : NoBaseValue, true)))->
		SetPadding(FMargin(0.f, StatBarSpacing, 0.f, 0.f));
	return StatBars;
}

FText UnitVariantWeaponWidgets::GetWeaponTitle(const FUnitVariantWeaponStats& Weapon)
{
	return Weapon.Count > 1
		       ? FText::Format(NSLOCTEXT("UnitVariantSelection", "WeaponCount", "{0}× {1}"),
		                       FText::AsNumber(Weapon.Count), Weapon.DisplayName)
		       : Weapon.DisplayName;
}
