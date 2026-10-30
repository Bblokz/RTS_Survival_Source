// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

#include "SurvivalUiStyle.generated.h"

/**
 * @brief The Frontstahl look of every survival mission widget (army selection, start location picker, HUD). One
 * asset (DA_SurvivalUiStyle) holds it so the three widgets never drift apart; the brushes are the same slice
 * instances the unit variant menu and the faction unit browser use (see Docs/20_MissionUI.md, "Style asset").
 * Sizes, paddings and font sizes live in SurvivalUiLayout.h because the art is drawn for them.
 */
USTRUCT(BlueprintType)
struct FSurvivalUiStyle
{
	GENERATED_BODY()

	// ---------------------------------------------------------------- Panels
	// MI9_Custom_VariantHeader_560x80_u_n: the title plate of the army selection screen.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush HeaderPlate;

	// MI9_Custom_VariantPanel_480x848_u_n: behind the roster list.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush RosterPanel;

	// MI9_Custom_ThinPanel_1078x679_u_n: behind the army slots, supply bar and buttons.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush ArmyPanel;

	// MI9_Custom_DialogPanel_578x242_u_n: the start location picker.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush DialogPanel;

	// MI9_Custom_ResourceCounter_510x130_u_n: the HUD timer plate.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush HudPlate;

	// MI9_Theme01_T_Panel_Tip03_511x150_f_n: the incoming-wave banner.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush BannerPlate;

	// MI9_Theme01_T_Panel06_Dark_750x450_f_n: the defeat result panel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush ResultPanel;

	// A black, half transparent brush that dims the world behind full-screen panels.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	FSlateBrush ScreenShade;

	// ---------------------------------------------------------------- Tabs
	// MI9_Custom_FactionTab_*_104x32: the roster category tabs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs")
	FButtonStyle Tab;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs")
	FButtonStyle TabSelected;

	// ---------------------------------------------------------------- Roster entry
	// MI9_Custom_VariantUnitEntry_n/h_352x80: normal and hovered row of a roster entry (rows are drag sources, not
	// buttons, so only Normal and Hovered are used).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roster Entry")
	FButtonStyle RosterEntry;

	// MI9_Custom_VariantIconWell_n_72x72_u_n
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Roster Entry")
	FSlateBrush RosterIconWell;

	// ---------------------------------------------------------------- Army slot
	// MI9_Custom_FactionUnitTile_*_120x164: Normal = empty slot, Hovered = drag hover, Pressed unused.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army Slot")
	FButtonStyle SlotEmpty;

	// MI9_Custom_FactionUnitTile_s_*_120x164: a slot that holds a unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army Slot")
	FButtonStyle SlotFilled;

	// Slot 0 with the HQ; usually the selected tile pressed brush.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army Slot")
	FSlateBrush SlotLocked;

	// MI9_Custom_VariantIconWell_n_104x104_u_n
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army Slot")
	FSlateBrush SlotIconWell;

	// MI_FactionUnitTileWear: battle wear over each filled slot; every slot draws a dynamic copy with its own Seed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army Slot")
	FSlateBrush SlotWear;

	// MI9_Custom_VariantSelectedMarker_28x28_u_n: behind the supply number in a slot corner.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Army Slot")
	FSlateBrush Badge;

	// ---------------------------------------------------------------- Bars and buttons
	// MI9_Theme02_T_LinearProgress03_Bg_987x42_u_n / MI9_Theme02_T_LinearProgress03_987x42_u_n
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bars")
	FProgressBarStyle SupplyBar;

	// MI9_Custom_VariantConfirm_*_280x48
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons")
	FButtonStyle ConfirmButton;

	// MI9_Custom_VariantBack_*_240x48
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons")
	FButtonStyle BackButton;

	// MI9_Theme01_T_Btn_Theme01Next_*_62x69; the previous button draws it mirrored.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons")
	FButtonStyle ArrowButton;

	// Vertical scroll bar of the roster and slot lists (MI9_Custom_SettingsScrollThumbV / TrackV).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buttons")
	FScrollBarStyle ScrollBar;

	// ---------------------------------------------------------------- Text
	// Titles, names, labels and buttons; the code sets size and letter spacing per element.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo HeadingFont;

	// Counts, hints and the feedback line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo BodyFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor TextColor = FSlateColor(FColor::FromHex(TEXT("ECE8DC")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor MutedTextColor = FSlateColor(FColor::FromHex(TEXT("9FA09A")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor TabTextColor = FSlateColor(FColor::FromHex(TEXT("C9C7BD")));

	// Variant badges, supply numbers and the "new record" label.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor AccentColor = FSlateColor(FColor::FromHex(TEXT("3FF0FF")));

	// Refused drops, a full supply bar, defeat titles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor WarningColor = FSlateColor(FColor::FromHex(TEXT("E07A5F")));

	// A drop the slot would accept.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor AcceptColor = FSlateColor(FColor::FromHex(TEXT("8FD46B")));
};

/**
 * @brief Holds the survival UI look once; every survival widget Blueprint points its StyleAsset at the same asset.
 * Hard references on purpose: the asset keeps every brush texture/material it names loaded while a widget shows it.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalUiStyleAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Style")
	FSurvivalUiStyle Style;
};
