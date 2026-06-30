// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

class AActor;
class AAnchorPoint;
class AConnection;
class UWorld;
class UWorldCampaignSettings;

namespace WorldCampaignPruningHelper
{
	struct FWorldCampaignPruningContext
	{
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		AAnchorPoint* PlayerHQAnchor = nullptr;
		TSubclassOf<AAnchorPoint> AnchorClass;
		TSubclassOf<AConnection> ConnectionClass;
		const UWorldCampaignSettings* CampaignSettings = nullptr;
		TSet<FGuid> GameplayAnchorKeys;
		TArray<TObjectPtr<AAnchorPoint>>* CachedAnchors = nullptr;
		TArray<TObjectPtr<AConnection>>* CachedConnections = nullptr;
		TArray<TObjectPtr<AConnection>>* GeneratedConnections = nullptr;
	};

	struct FWorldCampaignPruningResult
	{
		bool bDidChange = false;
		int32 DestroyedEmptyAnchors = 0;
		int32 DestroyedConnections = 0;
		int32 SpawnedRepairAnchors = 0;
		int32 SpawnedConnections = 0;
	};

	/**
	 * @brief Rebuilds anchor neighbor lists after connection actors have been added or removed.
	 * @param CachedAnchors Authoritative anchors that should receive adjacency data.
	 * @param CachedConnections Authoritative connection actors used to derive adjacency.
	 */
	void RebuildAnchorConnectionReferences(
		const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors,
		const TArray<TObjectPtr<AConnection>>& CachedConnections);

	/**
	 * @brief Contracts unused empty anchors and repairs any gameplay islands disconnected from the player HQ.
	 * @param Context Mutable graph arrays and spawn settings owned by the world generator.
	 * @return Counts for destroyed/spawned graph actors and whether the graph changed.
	 */
	FWorldCampaignPruningResult PruneUnusedAnchorsAndRepairConnectivity(
		const FWorldCampaignPruningContext& Context);
}
