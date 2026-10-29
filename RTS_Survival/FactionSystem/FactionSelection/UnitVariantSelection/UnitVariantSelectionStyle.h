// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

#include "UnitVariantSelectionStyle.generated.h"

class UDataTable;
class UTexture2D;

/**
 * @brief The look of the unit variant selection screen, filled in the class defaults of its Blueprint child with the
 * Frontstahl Custom_Variant* slice instances (UISkins/_generator/variant_selection_spec.md). The menu hands it to its
 * unit entries and variant cards; sizes, paddings and font sizes live in code because the art is drawn for them.
 */
USTRUCT(BlueprintType)
struct FUnitVariantSelectionStyle
{
	GENERATED_BODY()

	// T_VariantEdgeShade: dims the screen behind each side column; the right one is mirrored in code.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush EdgeShade;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush HeaderPlate;

	// MI9_Custom_VariantPanel_392x848_u_n
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush UnitListPanel;

	// MI9_Custom_VariantPanel_480x848_u_n
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush VariantPanel;

	// Normal, hovered and pressed of an entry that is not selected.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit Entry")
	FButtonStyle UnitEntry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit Entry")
	FButtonStyle UnitEntrySelected;

	// The 72 unit well behind the entry icon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit Entry")
	FSlateBrush UnitIconWell;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Unit Entry")
	FSlateBrush UnitIconWellSelected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant Card")
	FButtonStyle Card;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant Card")
	FButtonStyle CardSelected;

	// The 136 unit well behind the card icon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant Card")
	FSlateBrush CardIconWell;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant Card")
	FSlateBrush CardIconWellSelected;

	// The EQUIPPED badge on the selected card.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Variant Card")
	FSlateBrush SelectedMarker;

	// Buttons_DataTableb: the title, description and icon the action bar shows for each ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSoftObjectPtr<UDataTable> AbilityCardTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb.Buttons_DataTableb")));

	// The <Style> tags of the ability descriptions, the set the action bar's description widget uses.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
	TSoftObjectPtr<UDataTable> AbilityRichTextStyle = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/WeaponUI/AmmoButtonWidgets/DT_RichText.DT_RichText")));

	// WeaponUI_Data: the icon brush ("Slate") the weapon bar shows per weapon, rows named after EWeaponName.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	TSoftObjectPtr<UDataTable> WeaponCardTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/WeaponUI/WeaponUI_Data.WeaponUI_Data")));

	// White pictograms in front of the 1-10 pip bars of each weapon; tinted by the screen.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	TSoftObjectPtr<UTexture2D> DamageStatIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/VariantSelection/T_VariantStat_Damage.T_VariantStat_Damage")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	TSoftObjectPtr<UTexture2D> PenetrationStatIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/VariantSelection/T_VariantStat_Penetration.T_VariantStat_Penetration")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	TSoftObjectPtr<UTexture2D> MagazineStatIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/VariantSelection/T_VariantStat_Magazine.T_VariantStat_Magazine")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapons")
	TSoftObjectPtr<UTexture2D> ReloadStatIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/VariantSelection/T_VariantStat_Reload.T_VariantStat_Reload")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons")
	FButtonStyle ConfirmButton;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons")
	FButtonStyle BackButton;

	// Titles, names, labels and buttons; the code sets size and letter spacing per element.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo HeadingFont;

	// Counts, descriptions and the footnote.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo BodyFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor TextColor = FSlateColor(FColor::FromHex(TEXT("ECE8DC")));

	// Name of the selected entry and card.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor SelectedTextColor = FSlateColor(FColor::FromHex(TEXT("FFFFFF")));

	// Counts and the footnote.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor MutedTextColor = FSlateColor(FColor::FromHex(TEXT("9FA09A")));

	// The count of the selected entry.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor SelectedMutedTextColor = FSlateColor(FColor::FromHex(TEXT("9EDBFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor DescriptionColor = FSlateColor(FColor::FromHex(TEXT("C9C7BD")));

	// "BASE" / "VARIANT A" labels on the cards.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor LabelColor = FSlateColor(FColor::FromHex(TEXT("38C8FF")));

	// The REMOVED tag of an ability a variant lost against Base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor RemovedColor = FSlateColor(FColor::FromHex(TEXT("E07A5F")));

	// A stat that is better than Base's.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor BetterColor = FSlateColor(FColor::FromHex(TEXT("8FD46B")));

	// The faction name in the header and the EQUIPPED label.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor AccentColor = FSlateColor(FColor::FromHex(TEXT("3FF0FF")));
};
