// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/CardSystem/DeckBuilder/DeckBuilderStyle.h"
#include "Styling/SlateBrush.h"
#include "WorldCampaignUILook.generated.h"

class UTexture2D;

/**
 * @brief The colours of the world map campaign screens, picked with the owner from the UI sketches
 * (Saved/WorldMapUIDesigns, sheets 7, 13 and 14A: steel and cream, no light-blue accent). One place, so the bar, the build cards, the tech wheel and the
 * commander panel cannot drift apart.
 */
namespace WorldCampaignUIColors
{
	// Bought, owned, the main line colour of the tech wheel.
	const FLinearColor Cream = FLinearColor(FColor::FromHex(TEXT("E6DFC8")));
	// Lit lamps, the selected rim and highlighted text. The campaign screens do not use the deck builder's cyan.
	const FLinearColor Accent = Cream;
	// Can be bought or used next: the dashed outline.
	const FLinearColor Reach = FLinearColor(FColor::FromHex(TEXT("F4F1E6")));
	// Not reachable yet.
	const FLinearColor Locked = FLinearColor(FColor::FromHex(TEXT("3A4246")));
	const FLinearColor Good = FLinearColor(FColor::FromHex(TEXT("B9C79A")));
	const FLinearColor Bad = FLinearColor(FColor::FromHex(TEXT("FF6A4D")));
	// The enemy superweapon bar: frosted white glass on a dark track.
	const FLinearColor Freeze = FLinearColor(FColor::FromHex(TEXT("DCE3E6")));
	const FLinearColor FreezeDark = FLinearColor(FColor::FromHex(TEXT("1A1D1F")));
	// Flat plates drawn without art: bar background, tag plates on the map.
	const FLinearColor PlateFill = FLinearColor(FColor::FromHex(TEXT("0F1416")));
	const FLinearColor PlateBorder = FLinearColor(FColor::FromHex(TEXT("3A4448")));
	// Build site markers on the map.
	const FLinearColor SiteLocked = FLinearColor(FColor::FromHex(TEXT("3C4A50")));
	const FLinearColor SiteOpen = FLinearColor(FColor::FromHex(TEXT("FFCF3A")));
	const FLinearColor SiteBuilt = FLinearColor(FColor::FromHex(TEXT("B9C79A")));
	// Tech wheel branch colours, taken from the hexagon frames of the technology icons.
	const FLinearColor BranchInfantry = FLinearColor(FColor::FromHex(TEXT("37E07A")));
	const FLinearColor BranchLightArmour = FLinearColor(FColor::FromHex(TEXT("3AA0FF")));
	const FLinearColor BranchHeavyArmour = FLinearColor(FColor::FromHex(TEXT("FF4A4A")));
	const FLinearColor BranchSupport = FLinearColor(FColor::FromHex(TEXT("A970FF")));
	const FLinearColor BranchEconomy = FLinearColor(FColor::FromHex(TEXT("FFE14A")));
}

/**
 * @brief Loaded art shared by the world map campaign widgets: the gunmetal panels, buttons and fonts of the deck
 * builder plus the two resource icons. The campaign HUD builds one and hands a pointer to every child widget; it is a
 * UPROPERTY there, so the loaded textures stay alive.
 */
USTRUCT()
struct RTS_SURVIVAL_API FWorldCampaignUILook
{
	GENERATED_BODY()

	// Panels (Deck.Panel), buttons (Deck.Button, Deck.Tab), rows, fonts (Deck.HeadingFont, Deck.BodyFont) and the
	// text colours in Deck.Style.
	UPROPERTY()
	FDeckBuilderLook Deck;

	// The radixite icon of the in-mission resource bar, at 24 units.
	UPROPERTY()
	FSlateBrush RadixiteIcon;

	// The one blueprint currency of the world map, drawn with the construction blueprint icon.
	UPROPERTY()
	FSlateBrush BlueprintIcon;

	// Frontstahl skin parts of sheet 14A: the small bolted cell behind each value of the top strip.
	UPROPERTY()
	FSlateBrush MetalCell;

	// The scratched gunmetal panel with a bolt in each corner, for plates that float over the map (half-size copy
	// of the skin's panel, so its corners draw at 37 units).
	UPROPERTY()
	FSlateBrush BoltedPanel;

	// The gunmetal ring around a round glass button, 104 units across.
	UPROPERTY()
	FSlateBrush RoundBezel;

	// A grey-scale glass dome, 74 units across; a button tints it per state.
	UPROPERTY()
	FSlateBrush GlassDome;

	/**
	 * @brief Loads the art and builds the brushes.
	 * @param OutMissingTextures Receives the path of every texture that did not load; those parts draw as flat fills.
	 */
	void Build(TArray<FString>& OutMissingTextures);

	/**
	 * @brief A picture brush for any texture at an exact drawn size; a missing texture gives a flat dark plate.
	 * @param Texture Texture to draw, may be null.
	 * @param DrawSize Size in UI units at the 1920x1080 reference.
	 * @return Brush to put on a UImage.
	 */
	static FSlateBrush MakeIconBrush(UTexture2D* Texture, const FVector2D& DrawSize);

	/**
	 * @brief A nine-slice brush; Slate draws its corners at texture size, so the slice is given in texels.
	 * @param Texture Texture to draw, may be null (then a flat dark plate).
	 * @param SliceTexels Corner sizes in texels: left, top, right, bottom.
	 * @return Brush to put on a UBorder or UImage of any size.
	 */
	static FSlateBrush MakeBoxBrush(UTexture2D* Texture, const FMargin& SliceTexels);

	/** A flat one-colour brush for plates, bars and fills that have no art. */
	static FSlateBrush MakeFlatBrush(const FLinearColor& Color);
};
