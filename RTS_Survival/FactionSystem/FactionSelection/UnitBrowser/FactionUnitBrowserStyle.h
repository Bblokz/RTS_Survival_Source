// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"

#include "FactionUnitBrowserStyle.generated.h"

/**
 * @brief The look of the faction menu's unit browser, filled in the class defaults of its Blueprint child with the
 * Frontstahl Custom_FactionTab / Custom_FactionUnitTile / Custom_Variant slice instances
 * (UISkins/_generator/faction_unit_browser_spec.md). Sizes, paddings and font sizes live in code: the art is drawn for them.
 */
USTRUCT(BlueprintType)
struct FFactionUnitBrowserStyle
{
	GENERATED_BODY()

	// MI9_Custom_VariantPanel_1312x254_u_n; its header strip holds the title, tabs and count.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panel")
	FSlateBrush Panel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs")
	FButtonStyle Tab;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tabs")
	FButtonStyle TabSelected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	FButtonStyle Tile;

	// The unit shown in the hangar.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	FButtonStyle TileSelected;

	// The 104 unit well behind the tile icon.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	FSlateBrush IconWell;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	FSlateBrush IconWellSelected;

	// MI_FactionUnitTileWear: battle wear over each tile. Every tile draws a dynamic copy with its own Seed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tiles")
	FSlateBrush TileWear;

	// MI_FactionUnitGlass_1284x192: smoked, reflecting, lightly damaged glass behind the tiles.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Row")
	FSlateBrush TileRowGlass;

	// Edge shadows that show when more tiles are off to that side.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Row")
	FScrollBoxStyle TileRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tile Row")
	FScrollBarStyle TileRowScrollBar;

	// Title, tabs and unit names; the code sets size and letter spacing per element.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo HeadingFont;

	// The title and the unit names.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor TextColor = FSlateColor(FColor::FromHex(TEXT("ECE8DC")));

	// The selected tab and the name of the selected unit.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor SelectedTextColor = FSlateColor(FColor::FromHex(TEXT("FFFFFF")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor TabTextColor = FSlateColor(FColor::FromHex(TEXT("C9C7BD")));
};

/** One filter tab of the unit browser and the units it shows; the menu builds these from its faction setting. */
USTRUCT()
struct FFactionUnitBrowserGroup
{
	GENERATED_BODY()

	UPROPERTY()
	FText Label;

	UPROPERTY()
	TArray<FTrainingOption> Units;
};
