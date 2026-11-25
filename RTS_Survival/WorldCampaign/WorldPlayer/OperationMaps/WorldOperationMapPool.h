// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/SoftObjectPtr.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "WorldOperationMapPool.generated.h"

class UWorld;

/**
 * @brief Where enemy item and division operations take their level from; set on the world controller Blueprint
 * (FWorldOperationMapSettings). See Source/Docs/Campaign/EnemyItemMaps_Plan.md.
 */
UENUM(BlueprintType)
enum class EWorldEnemyOperationMapSource : uint8
{
	// The procedural maps: one shuffled list for items and divisions, the next map on every launch.
	ProceduralMaps,
	// Handmade maps: items and divisions each have their own shuffled list, and a target keeps its map on a retry.
	HandmadeMaps
};

/** How an attack on an enemy map item finds its level. */
enum class EWorldEnemyItemMapRoute : uint8
{
	// A fixed level set for this item type (EnemyHQ, EnemyWall).
	StoryMap,
	// The shuffled pool of the selected map source.
	MapPool,
	// A story item without its level while handmade maps are used: the attack is refused.
	Refuse
};

/**
 * @brief Saved state of one shuffled handmade map pool. The world controller picks through
 * WorldOperationMapPool::TryPickMapForTarget, so a new target takes the next map and a retried target gets the map
 * it had the first time.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldOperationMapPoolState
{
	GENERATED_BODY()

	// Index into the shuffled pool of the map the next new target gets.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	int32 NextMapIndex = 0;

	// The map each attacked target got (anchor key for items, division key for divisions). Kept as a path, not an
	// index, so adding maps to the pool during a campaign does not move a target to another map.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame)
	TMap<FGuid, FSoftObjectPath> MapByTargetKey;
};

/**
 * @brief Copy of every operation map cursor, taken before a launch picks its map so a launch that fails afterwards
 * can put the cursors back.
 */
struct FWorldOperationMapCursorSnapshot
{
	int32 ProceduralMapIndex = 0;
	FWorldOperationMapPoolState HandmadeEnemyItemMapPool;
	FWorldOperationMapPoolState HandmadeFieldMapPool;
};

namespace WorldOperationMapPool
{
	/**
	 * @brief Picks the level for an operation: the map the target had before when it is still in the pool,
	 * otherwise the next set map from the cursor, which then moves on and wraps.
	 * @param InOutPoolState Saved cursor and per-target memory; changed by the pick.
	 * @param TargetKey Anchor or division key; an invalid key picks from the cursor and remembers nothing.
	 * @param ShuffledMaps The pool after the campaign's shuffle; unset entries are skipped.
	 * @param OutMap The picked level.
	 * @return false when the pool holds no set map; the pool state is then unchanged.
	 */
	RTS_SURVIVAL_API bool TryPickMapForTarget(FWorldOperationMapPoolState& InOutPoolState,
	                                          const FGuid& TargetKey,
	                                          const TArray<TSoftObjectPtr<UWorld>>& ShuffledMaps,
	                                          TSoftObjectPtr<UWorld>& OutMap);

	/** @return Whether this enemy item is a story place with its own level rather than a pool item (owner pick D5). */
	RTS_SURVIVAL_API bool GetIsStoryEnemyItem(EMapEnemyItem ItemType);

	/**
	 * @brief Decides where an attack on an enemy map item goes: its own story level when one is set, never the
	 * handmade pool for EnemyHQ and EnemyWall, otherwise the pool (the procedural pool keeps its old behaviour).
	 * @param ItemType The attacked item.
	 * @param bHasStoryMap Whether a level is set for this item type in StoryEnemyItemMaps.
	 * @param MapSource The selected map source.
	 * @return The route.
	 */
	RTS_SURVIVAL_API EWorldEnemyItemMapRoute GetEnemyItemMapRoute(EMapEnemyItem ItemType, bool bHasStoryMap,
	                                                              EWorldEnemyOperationMapSource MapSource);
}
