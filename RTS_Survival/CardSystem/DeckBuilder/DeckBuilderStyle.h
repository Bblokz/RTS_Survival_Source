// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "DeckBuilderTypes.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateTypes.h"

#include "DeckBuilderStyle.generated.h"

class UDataTable;
class UTexture2D;

/**
 * @brief The art of the deck builder as soft references with defaults, so the menu works without a Blueprint child.
 * A Blueprint child of UW_DeckBuilderMenu may override any of it in its class defaults. Sizes and paddings live in
 * code because the art is drawn for them.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FDeckBuilderStyle
{
	GENERATED_BODY()

	FDeckBuilderStyle();

	// DT_RTSCards: type, rarity, texts, picture, unit and technology per card.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TSoftObjectPtr<UDataTable> CardTable;

	// T_Panel01_Light of the Frontstahl card set: the plate behind each of the four columns.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panels")
	TSoftObjectPtr<UTexture2D> PanelTexture;

	// T_Panel02_* of the Frontstahl card set, indexed by ERTSCardRarity (Common first).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cards")
	TArray<TSoftObjectPtr<UTexture2D>> CardFrames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cards")
	TSoftObjectPtr<UTexture2D> VacantCardFrame;

	// The narrow variant card with its painted spine, indexed by ERTSCardRarity.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cards")
	TArray<TSoftObjectPtr<UTexture2D>> SpineFrames;

	// Building pictures of the rail and the cards, indexed by EDeckSection.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sections")
	TArray<TSoftObjectPtr<UTexture2D>> SectionIcons;

	// Half-size copies of the hangar variant screen's Frontstahl slice textures (T_Deck_Part_*), drawn as box
	// brushes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> VariantRow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> VariantRowHovered;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> VariantRowSelected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> IconWell;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> SelectedMarker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> RailEntry;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> RailEntryHovered;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> RailEntrySelected;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> Button;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> ButtonHovered;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> ButtonPressed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> Tab;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hangar Parts")
	TSoftObjectPtr<UTexture2D> TabSelected;

	// Titles, names, labels and buttons; left empty the engine's bold face is used.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo HeadingFont;

	// Descriptions, counts and hints; left empty the engine's regular face is used.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateFontInfo BodyFont;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor TextColor = FSlateColor(FColor::FromHex(TEXT("ECE8DC")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor MutedTextColor = FSlateColor(FColor::FromHex(TEXT("9FA09A")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor DescriptionColor = FSlateColor(FColor::FromHex(TEXT("C9C7BD")));

	// Small caps labels ("VARIANT", "FITS").
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor LabelColor = FSlateColor(FColor::FromHex(TEXT("38C8FF")));

	// Selected items, variant names under a unit and the drop highlight.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor AccentColor = FSlateColor(FColor::FromHex(TEXT("3FF0FF")));

	// Rarity names and a section whose slots are all filled.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor GoodColor = FSlateColor(FColor::FromHex(TEXT("8FE3C8")));

	// Spine lettering on the four light rarity paints and on the two dark ones (Legendary, Mythic).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor SpineInkOnLightPaint = FSlateColor(FColor::FromHex(TEXT("040506")));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Text")
	FSlateColor SpineInkOnDarkPaint = FSlateColor(FColor::FromHex(TEXT("F6F1E4")));
};

/**
 * @brief FDeckBuilderStyle with its textures loaded and turned into brushes; the menu builds one and hands it to
 * its cards, rows and rail entries. A UPROPERTY on the menu, so the loaded textures stay alive with it.
 */
USTRUCT()
struct RTS_SURVIVAL_API FDeckBuilderLook
{
	GENERATED_BODY()

	UPROPERTY()
	FSlateBrush Panel;

	UPROPERTY()
	TArray<FSlateBrush> CardFrames;

	UPROPERTY()
	FSlateBrush VacantCardFrame;

	UPROPERTY()
	TArray<FSlateBrush> SpineFrames;

	UPROPERTY()
	TArray<FSlateBrush> SectionIcons;

	UPROPERTY()
	FSlateBrush IconWell;

	UPROPERTY()
	FSlateBrush SelectedMarker;

	UPROPERTY()
	FButtonStyle VariantRow;

	UPROPERTY()
	FButtonStyle VariantRowSelected;

	UPROPERTY()
	FButtonStyle RailEntry;

	UPROPERTY()
	FButtonStyle RailEntrySelected;

	UPROPERTY()
	FButtonStyle Button;

	UPROPERTY()
	FButtonStyle Tab;

	UPROPERTY()
	FButtonStyle TabSelected;

	UPROPERTY()
	FSlateFontInfo HeadingFont;

	UPROPERTY()
	FSlateFontInfo BodyFont;

	UPROPERTY()
	FDeckBuilderStyle Style;

	const FSlateBrush& GetCardFrame(const ERTSCardRarity Rarity) const;
	const FSlateBrush& GetSpineFrame(const ERTSCardRarity Rarity) const;
	const FSlateBrush& GetSectionIcon(const EDeckSection Section) const;

	/** @return True for the two rarities whose paint is dark (Legendary, Mythic). */
	static bool GetIsDarkPaint(const ERTSCardRarity Rarity);

	/**
	 * @brief Loads every texture of the style and builds the brushes.
	 * @param InStyle The style of the menu.
	 * @param OutMissingTextures Receives the path of every texture that did not load; those parts draw as flat fills.
	 */
	void BuildFromStyle(const FDeckBuilderStyle& InStyle, TArray<FString>& OutMissingTextures);
};
