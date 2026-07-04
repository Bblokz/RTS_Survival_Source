// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"

namespace
{
	bool GetDoesContainInvalidAnchorKey(const TArray<FWorldCampaignAnchorSaveData>& Anchors)
	{
		for (const FWorldCampaignAnchorSaveData& AnchorSaveData : Anchors)
		{
			if (not AnchorSaveData.AnchorKey.IsValid())
			{
				return true;
			}
		}

		return false;
	}

	bool GetDoesContainDuplicateAnchorKey(const TArray<FWorldCampaignAnchorSaveData>& Anchors)
	{
		TSet<FGuid> AnchorKeys;
		AnchorKeys.Reserve(Anchors.Num());
		for (const FWorldCampaignAnchorSaveData& AnchorSaveData : Anchors)
		{
			const int32 AnchorKeyCountBeforeAdd = AnchorKeys.Num();
			AnchorKeys.Add(AnchorSaveData.AnchorKey);
			if (AnchorKeys.Num() == AnchorKeyCountBeforeAdd)
			{
				return true;
			}
		}

		return false;
	}

	bool GetDoesContainDuplicateGuid(const TArray<FGuid>& AnchorKeys)
	{
		TSet<FGuid> UniqueAnchorKeys;
		UniqueAnchorKeys.Reserve(AnchorKeys.Num());
		for (const FGuid& AnchorKey : AnchorKeys)
		{
			const int32 AnchorKeyCountBeforeAdd = UniqueAnchorKeys.Num();
			UniqueAnchorKeys.Add(AnchorKey);
			if (UniqueAnchorKeys.Num() == AnchorKeyCountBeforeAdd)
			{
				return true;
			}
		}

		return false;
	}

	bool GetDoesReferenceMissingAnchor(const TArray<FGuid>& AnchorKeys, const TSet<FGuid>& ValidAnchorKeys)
	{
		for (const FGuid& AnchorKey : AnchorKeys)
		{
			if (not ValidAnchorKeys.Contains(AnchorKey))
			{
				return true;
			}
		}

		return false;
	}

	TSet<FGuid> BuildValidAnchorKeys(const TArray<FWorldCampaignAnchorSaveData>& Anchors)
	{
		TSet<FGuid> AnchorKeys;
		AnchorKeys.Reserve(Anchors.Num());
		for (const FWorldCampaignAnchorSaveData& AnchorSaveData : Anchors)
		{
			AnchorKeys.Add(AnchorSaveData.AnchorKey);
		}

		return AnchorKeys;
	}

	bool GetDoesContainInvalidDivisionData(const TArray<FWorldDivisionSaveData>& WorldDivisions)
	{
		TSet<FGuid> DivisionKeys;
		DivisionKeys.Reserve(WorldDivisions.Num());

		for (const FWorldDivisionSaveData& DivisionSaveData : WorldDivisions)
		{
			if (not DivisionSaveData.DivisionKey.IsValid()
				|| DivisionSaveData.DivisionType == EWorldFieldDivisions::None)
			{
				return true;
			}

			const int32 DivisionKeyCountBeforeAdd = DivisionKeys.Num();
			DivisionKeys.Add(DivisionSaveData.DivisionKey);
			if (DivisionKeys.Num() == DivisionKeyCountBeforeAdd)
			{
				return true;
			}

			if (not DivisionSaveData.bHasPendingMoveOrder)
			{
				continue;
			}

			if (DivisionSaveData.PathPoints.Num() == 0
				|| not DivisionSaveData.PathPoints.IsValidIndex(DivisionSaveData.CurrentPathPointIndex))
			{
				return true;
			}
		}

		return false;
	}
}

bool FWorldCampaignState::GetIsValidForRestore() const
{
	if (Anchors.Num() == 0)
	{
		return false;
	}

	if (GetDoesContainInvalidAnchorKey(Anchors) || GetDoesContainDuplicateAnchorKey(Anchors))
	{
		return false;
	}

	const TSet<FGuid> ValidAnchorKeys = BuildValidAnchorKeys(Anchors);
	if (PlayerHQAnchorKey.IsValid() && not ValidAnchorKeys.Contains(PlayerHQAnchorKey))
	{
		return false;
	}

	if (EnemyHQAnchorKey.IsValid() && not ValidAnchorKeys.Contains(EnemyHQAnchorKey))
	{
		return false;
	}

	for (const FWorldCampaignConnectionSaveData& Connection : Connections)
	{
		if (Connection.ConnectedAnchorKeys.Num() < 2)
		{
			return false;
		}

		if (GetDoesContainDuplicateGuid(Connection.ConnectedAnchorKeys)
			|| GetDoesReferenceMissingAnchor(Connection.ConnectedAnchorKeys, ValidAnchorKeys))
		{
			return false;
		}
	}

	for (const FWorldCampaignMapItemSaveData& MapItem : MapItems)
	{
		if (not ValidAnchorKeys.Contains(MapItem.AnchorKey))
		{
			return false;
		}
	}

	if (GetDoesContainInvalidDivisionData(WorldDivisions))
	{
		return false;
	}

	return true;
}
