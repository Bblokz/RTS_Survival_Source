// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Player/Abilities.h"

#include "UnitVariantAbilityCards.generated.h"

class UDataTable;
class UTexture2D;
struct FUnitAbilityEntry;

/** One special ability of a variant as the action bar shows it: the card from Buttons_DataTableb. */
USTRUCT()
struct FUnitVariantAbilityCard
{
	GENERATED_BODY()

	// Without rich text markup, for the heading font of the variant screen.
	UPROPERTY()
	FText Title;

	// Rich text with the action bar's <Style> tags, for a RichTextBlock on DT_RichText.
	UPROPERTY()
	FText Description;

	// Held by Buttons_DataTableb, which the menu keeps loaded.
	UPROPERTY()
	TObjectPtr<UTexture2D> Icon = nullptr;

	// Which ability the card is, so two variants' lists can be compared.
	UPROPERTY()
	EAbilityID AbilityId = EAbilityID::IdNoAbility;

	UPROPERTY()
	int32 CustomType = 0;

	bool GetIsSameAbility(const FUnitVariantAbilityCard& Other) const
	{
		return AbilityId == Other.AbilityId && CustomType == Other.CustomType;
	}
};

/** The ability cards of one variant; a struct so a TMap UPROPERTY can hold the list. */
USTRUCT()
struct FUnitVariantAbilityCardList
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FUnitVariantAbilityCard> Cards;
};

/**
 * Reads which special abilities a unit class has without spawning it: the ability components on its Blueprint add
 * them at BeginPlay, so their templates say what the unit will get. Movement, attack and the other abilities every
 * unit shares come from unit data, not from these components, so they are left out.
 */
namespace UnitVariantAbilityCards
{
	/**
	 * @param UnitClass A loaded unit class (tank, squad controller or building expansion).
	 * @return One entry per distinct ability id and custom type, in Blueprint component order.
	 */
	TArray<FUnitAbilityEntry> CollectComponentAbilities(const UClass* UnitClass);

	/**
	 * @brief Looks the ability up the way BP_W_ActionUI_Item does: custom type 0 uses the row's own card, any other
	 * type its CustomTypeMapping entry and the row's card when the mapping has none.
	 * @param ButtonsTable Buttons_DataTableb (row struct Struct_Buttons, rows named after EAbilityID).
	 * @param Ability The entry the unit gets.
	 * @param OutCard Receives the title, description and icon.
	 * @return False when the table has no row for the ability.
	 */
	bool FindAbilityCard(const UDataTable* ButtonsTable, const FUnitAbilityEntry& Ability,
	                     FUnitVariantAbilityCard& OutCard);

	/**
	 * @brief Reads an object column of a row whose struct is user defined (column names carry a GUID suffix).
	 * @param Table The data table.
	 * @param RowName The row.
	 * @param AuthoredColumnName The column name as shown in the editor ("Slate").
	 * @return Null when the row or column is missing or empty.
	 */
	UObject* FindRowObject(const UDataTable* Table, const FName RowName, const TCHAR* AuthoredColumnName);

	/** @return The cards of every component ability of UnitClass that has a row in ButtonsTable. */
	FUnitVariantAbilityCardList MakeAbilityCards(const UClass* UnitClass, const UDataTable* ButtonsTable);
}
