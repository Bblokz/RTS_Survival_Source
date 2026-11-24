// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UDataTable;
class UTexture2D;
class UWidget;
class UWidgetTree;
struct FUnitVariantSelectionStyle;
struct FUnitVariantWeaponStats;
enum class EUnitVariantStatTrend : uint8;

/**
 * The weapon pictograms of the unit variant screen (the weapon bar icon and the four 1-10 stat bars), shared with
 * every screen that lists a unit's weapons the same way (the vehicle info takeover), so they never drift apart.
 */
namespace UnitVariantWeaponWidgets
{
	// The white pictograms in front of the stat bars, loaded once by the screen that draws them.
	struct FStatIcons
	{
		UTexture2D* Damage = nullptr;
		UTexture2D* Penetration = nullptr;
		UTexture2D* Reload = nullptr;
		UTexture2D* Magazine = nullptr;
	};

	/** @return The style's four stat pictograms, loaded; the caller keeps them in UPROPERTYs. */
	FStatIcons LoadStatIcons(const FUnitVariantSelectionStyle& Style);

	/**
	 * @brief The weapon bar's icon for the weapon, or its calibre when WeaponUI_Data has no row for it, so the well
	 * never looks broken.
	 * @param Tree Widget tree of the widget that owns the icon.
	 * @param Style Fonts and colours of the calibre fallback.
	 * @param WeaponCardTable WeaponUI_Data.
	 * @param Weapon The weapon to show.
	 * @return The scale box to put in the icon well.
	 */
	UWidget* BuildWeaponIcon(UWidgetTree& Tree, const FUnitVariantSelectionStyle& Style,
	                         const UDataTable* WeaponCardTable, const FUnitVariantWeaponStats& Weapon);

	/**
	 * @brief A weapon stat as a pictogram and UnitVariantStatComparison::MaxStatLevel pips, Level of them filled.
	 * @param Tree Widget tree of the widget that owns the bar.
	 * @param Style Colours of the pictogram and the filled pips.
	 * @param StatIcon The stat's pictogram; the bar is drawn without it when it is missing.
	 * @param Level 0-10 filled pips.
	 * @param Trend Green or red filled pips against a compared weapon; Same keeps the accent colour.
	 * @return The bar.
	 */
	UWidget* BuildStatBar(UWidgetTree& Tree, const FUnitVariantSelectionStyle& Style, UTexture2D* StatIcon,
	                      const int32 Level, const EUnitVariantStatTrend Trend);

	/**
	 * @brief Damage, penetration, reload and magazine bars stacked, each coloured against BaseWeapon when given.
	 * @param Tree Widget tree of the widget that owns the bars.
	 * @param Style Colours of the bars.
	 * @param StatIcons The four pictograms.
	 * @param Weapon The weapon whose stats are drawn.
	 * @param BaseWeapon The weapon it is compared with; null keeps every pip neutral.
	 * @return A vertical box with the four bars.
	 */
	UWidget* BuildStatBars(UWidgetTree& Tree, const FUnitVariantSelectionStyle& Style, const FStatIcons& StatIcons,
	                       const FUnitVariantWeaponStats& Weapon, const FUnitVariantWeaponStats* BaseWeapon);

	/** @return "2× MG" for paired weapons, else the weapon's display name. */
	FText GetWeaponTitle(const FUnitVariantWeaponStats& Weapon);
}
