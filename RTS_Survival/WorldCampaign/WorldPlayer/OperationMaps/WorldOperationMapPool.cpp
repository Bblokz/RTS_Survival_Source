// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldOperationMapPool.h"

#include "Engine/World.h"

namespace WorldOperationMapPool
{
	namespace
	{
		bool GetIsPathInPool(const FSoftObjectPath& MapPath, const TArray<TSoftObjectPtr<UWorld>>& ShuffledMaps)
		{
			for (const TSoftObjectPtr<UWorld>& PoolMap : ShuffledMaps)
			{
				if (not PoolMap.IsNull() && PoolMap.ToSoftObjectPath() == MapPath)
				{
					return true;
				}
			}

			return false;
		}

		bool TryGetRememberedMap(FWorldOperationMapPoolState& InOutPoolState,
		                         const FGuid& TargetKey,
		                         const TArray<TSoftObjectPtr<UWorld>>& ShuffledMaps,
		                         TSoftObjectPtr<UWorld>& OutMap)
		{
			if (not TargetKey.IsValid())
			{
				return false;
			}

			const FSoftObjectPath* RememberedMapPath = InOutPoolState.MapByTargetKey.Find(TargetKey);
			if (RememberedMapPath == nullptr)
			{
				return false;
			}

			if (GetIsPathInPool(*RememberedMapPath, ShuffledMaps))
			{
				OutMap = TSoftObjectPtr<UWorld>(*RememberedMapPath);
				return true;
			}

			// The map was taken out of the pool; the target gets a new one.
			InOutPoolState.MapByTargetKey.Remove(TargetKey);
			return false;
		}

		int32 FindNextSetMapIndex(const int32 StartIndex, const TArray<TSoftObjectPtr<UWorld>>& ShuffledMaps)
		{
			const int32 MapCount = ShuffledMaps.Num();
			for (int32 StepsFromStart = 0; StepsFromStart < MapCount; ++StepsFromStart)
			{
				const int32 CandidateIndex = (StartIndex + StepsFromStart) % MapCount;
				if (not ShuffledMaps[CandidateIndex].IsNull())
				{
					return CandidateIndex;
				}
			}

			return INDEX_NONE;
		}
	}

	bool TryPickMapForTarget(FWorldOperationMapPoolState& InOutPoolState,
	                         const FGuid& TargetKey,
	                         const TArray<TSoftObjectPtr<UWorld>>& ShuffledMaps,
	                         TSoftObjectPtr<UWorld>& OutMap)
	{
		OutMap.Reset();
		if (ShuffledMaps.Num() == 0)
		{
			return false;
		}

		if (TryGetRememberedMap(InOutPoolState, TargetKey, ShuffledMaps, OutMap))
		{
			return true;
		}

		const int32 CursorIndex = ShuffledMaps.IsValidIndex(InOutPoolState.NextMapIndex)
			                          ? InOutPoolState.NextMapIndex
			                          : 0;
		const int32 PickedMapIndex = FindNextSetMapIndex(CursorIndex, ShuffledMaps);
		if (PickedMapIndex == INDEX_NONE)
		{
			return false;
		}

		OutMap = ShuffledMaps[PickedMapIndex];
		InOutPoolState.NextMapIndex = (PickedMapIndex + 1) % ShuffledMaps.Num();
		if (TargetKey.IsValid())
		{
			InOutPoolState.MapByTargetKey.Add(TargetKey, OutMap.ToSoftObjectPath());
		}

		return true;
	}

	bool GetIsStoryEnemyItem(const EMapEnemyItem ItemType)
	{
		return ItemType == EMapEnemyItem::EnemyHQ || ItemType == EMapEnemyItem::EnemyWall;
	}

	EWorldEnemyItemMapRoute GetEnemyItemMapRoute(const EMapEnemyItem ItemType,
	                                             const bool bHasStoryMap,
	                                             const EWorldEnemyOperationMapSource MapSource)
	{
		if (bHasStoryMap)
		{
			return EWorldEnemyItemMapRoute::StoryMap;
		}

		const bool bUsesHandmadeMaps = MapSource == EWorldEnemyOperationMapSource::HandmadeMaps;
		if (bUsesHandmadeMaps && GetIsStoryEnemyItem(ItemType))
		{
			return EWorldEnemyItemMapRoute::Refuse;
		}

		return EWorldEnemyItemMapRoute::MapPool;
	}
}
