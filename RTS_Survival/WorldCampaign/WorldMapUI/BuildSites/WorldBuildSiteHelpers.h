// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"

struct FWorldBuildingEffectDefinition;

/** How a support building's reach compares with the other support buildings, for its one-line effect text. */
enum class EWorldBuildSiteRangeWord : uint8
{
	// Only one support radius exists, or this one is neither the largest nor the smallest.
	None,
	Long,
	Short
};

/** The pointer line between the build site panel and its site: a thin bar hung from Start and turned around it. */
struct RTS_SURVIVAL_API FWorldBuildSiteStem
{
	bool bIsShown = false;

	// Where the line leaves the panel's edge.
	FVector2D Start = FVector2D::ZeroVector;

	float Length = 0.f;

	// Clockwise turn on screen, in degrees, of a line that hangs straight down from Start.
	float AngleDegrees = 0.f;
};

/**
 * @brief Texts, unlock thresholds and placement of the build site panel as functions of plain data, so they are unit
 * tested and every number on a card comes from the campaign rules and building settings.
 */
namespace WorldBuildSiteHelpers
{
	/**
	 * @brief The short code drawn in a card's picture well (WP, SD, AF, BT); unknown buildings get the initials of
	 * their display name.
	 * @param Building Building type.
	 * @param DisplayName Name used for a building without a fixed code.
	 * @return One to two capital letters.
	 */
	RTS_SURVIVAL_API FText GetBuildingCode(EWorldCampaignBuilding Building, const FText& DisplayName);

	/** @return Colour of the code in the picture well; cream for a building without its own colour. */
	RTS_SURVIVAL_API FLinearColor GetBuildingCodeColor(EWorldCampaignBuilding Building);

	/** @return The settings' display name, or the enum's name when the building has no settings entry. */
	RTS_SURVIVAL_API FText GetBuildingDisplayName(EWorldCampaignBuilding Building);

	/**
	 * @brief Long or short reach, judged against every support radius that exists.
	 * @param Radius This building's support radius.
	 * @param AllSupportRadii Support radius of every building type that lowers enemy strength, this one included.
	 * @return None when there is nothing to compare with.
	 */
	RTS_SURVIVAL_API EWorldBuildSiteRangeWord GetSupportRangeWord(float Radius, const TArray<float>& AllSupportRadii);

	/**
	 * @brief The card's one line of effect, written from what the building really does.
	 * @param Effect Effects of the building.
	 * @param DepotIncome What one paying building delivers each turn (campaign rules).
	 * @param AllSupportRadii Support radius of every strength-lowering building type, for "long" or "short range".
	 * @return The effect line; the building's description when it has no known effect.
	 */
	RTS_SURVIVAL_API FText GetBuildingEffectText(const FWorldBuildingEffectDefinition& Effect,
	                                             const FWorldMapResources& DepotIncome,
	                                             const TArray<float>& AllSupportRadii);

	/** @return Support radius of every building type in the building settings that lowers enemy strength. */
	RTS_SURVIVAL_API TArray<float> GatherSupportRadii();

	/**
	 * @brief Liberation a country needs before the node at this place of its unlock order opens; mirrors
	 * WorldCampaignRules::GetUnlockedBuildNodeCount.
	 * @param UnlockRank Place of the node in WorldBuildNodeRules::GetNodesInUnlockOrder, 0 first.
	 * @param UnlockLiberationThresholds BuildNodeUnlockLiberation of the campaign rules.
	 * @return 0 for the first node (it opens with the first cleared target), else 0..1.
	 */
	RTS_SURVIVAL_API float GetUnlockLiberation(int32 UnlockRank, const TArray<float>& UnlockLiberationThresholds);

	/**
	 * @brief Why a locked site is still locked, with how far the country is now.
	 * @param UnlockRank Place of the node in its country's unlock order, 0 first.
	 * @param UnlockLiberationThresholds BuildNodeUnlockLiberation of the campaign rules.
	 * @param CurrentLiberation The country's liberation now, 0..1.
	 * @return E.g. "Opens at 50 % liberated, now 20 %" (with a middle dot).
	 */
	RTS_SURVIVAL_API FText GetLockedReasonText(int32 UnlockRank, const TArray<float>& UnlockLiberationThresholds,
	                                           float CurrentLiberation);

	/** @return The country's DisplayName from UWorldCampaignMapContentSettings::Countries, else its id. */
	RTS_SURVIVAL_API FText GetCountryDisplayName(FName CountryId);

	/** @return "BUILD SITE, GERMANY 02" (with a middle dot). */
	RTS_SURVIVAL_API FText GetSiteTitle(const FText& CountryName, int32 SiteNumber);

	/**
	 * @brief Width of a row of cards, so the panel is only as wide as the cards it shows.
	 * @param CardCount Cards in the row.
	 * @param CardWidth Width of one card.
	 * @param CardGap Space between two cards.
	 * @return 0 for an empty row.
	 */
	RTS_SURVIVAL_API float GetCardRowWidth(int32 CardCount, float CardWidth, float CardGap);

	/**
	 * @brief Puts the panel centred above the site, below it when there is no room above, and always on screen and
	 * below the campaign top bar.
	 * @param Anchor The site in the panel's local units.
	 * @param PanelSize Size of the panel.
	 * @param ViewSize Size of the area the panel must stay inside.
	 * @param GapToAnchor Space between the panel and the site.
	 * @param ScreenMargin Space kept free at the screen edges and under the top bar.
	 * @param TopBarHeight Height of the bar at the top of the view that the panel must not go under.
	 * @return Top-left corner of the panel.
	 */
	RTS_SURVIVAL_API FVector2D GetPanelTopLeft(const FVector2D& Anchor, const FVector2D& PanelSize,
	                                           const FVector2D& ViewSize, float GapToAnchor, float ScreenMargin,
	                                           float TopBarHeight);

	/**
	 * @brief The pointer line from the panel's edge to its site. It leans when the panel was pushed sideways by the
	 * screen edge, so its end always touches the site marker.
	 * @param Anchor The site in the panel's local units.
	 * @param PanelTopLeft Top-left corner of the panel's visible frame.
	 * @param PanelSize Size of the panel's visible frame.
	 * @param CornerInset The line leaves the panel at least this far from its left and right corners.
	 * @param AnchorClearance The line stops this far from the site: the radius of the site marker.
	 * @return The line; not shown when the site lies beside or under the panel or closer than the clearance.
	 */
	RTS_SURVIVAL_API FWorldBuildSiteStem GetStemPlacement(const FVector2D& Anchor, const FVector2D& PanelTopLeft,
	                                                      const FVector2D& PanelSize, float CornerInset,
	                                                      float AnchorClearance);
}
