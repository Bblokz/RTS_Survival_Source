// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TrainerFieldDefenses.h"

#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Player/ConstructionPreview/StaticMeshPreview/StaticPreviewMesh.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

int32 FTrainerFieldDefenses::AddPlacement(const FFieldDefensePlacement& Placement)
{
	const int32 FieldDefenseId = M_NextFieldDefenseId++;
	M_Placements.Add(FieldDefenseId, Placement);
	return FieldDefenseId;
}

FFieldDefensePlacement* FTrainerFieldDefenses::FindPlacement(const int32 FieldDefenseId)
{
	return M_Placements.Find(FieldDefenseId);
}

const FFieldDefensePlacement* FTrainerFieldDefenses::FindPlacement(const int32 FieldDefenseId) const
{
	return M_Placements.Find(FieldDefenseId);
}

int32 FTrainerFieldDefenses::GetPlannedCount() const
{
	int32 PlannedCount = 0;
	for (const TPair<int32, FFieldDefensePlacement>& PlacementPair : M_Placements)
	{
		if (PlacementPair.Value.State == EFieldDefenseConstructionState::Planned)
		{
			++PlannedCount;
		}
	}
	return PlannedCount;
}

TArray<int32> FTrainerFieldDefenses::GetIdsWithLostConstruction() const
{
	TArray<int32> LostIds;
	for (const TPair<int32, FFieldDefensePlacement>& PlacementPair : M_Placements)
	{
		const FFieldDefensePlacement& Placement = PlacementPair.Value;
		if (Placement.State != EFieldDefenseConstructionState::Constructing)
		{
			continue;
		}
		if (not RTSFunctionLibrary::RTSIsValid(Placement.Expansion.Get()))
		{
			LostIds.Add(PlacementPair.Key);
		}
	}
	return LostIds;
}

void FTrainerFieldDefenses::DestroyPlacement(const int32 FieldDefenseId)
{
	FFieldDefensePlacement RemovedPlacement;
	// Remove first: destroying actors may run callbacks that look at the placements again.
	if (not M_Placements.RemoveAndCopyValue(FieldDefenseId, RemovedPlacement))
	{
		return;
	}
	DestroyStaticPreview(RemovedPlacement);
	DestroyUnfinishedExpansion(RemovedPlacement);
}

void FTrainerFieldDefenses::ReleaseFinishedPlacement(const int32 FieldDefenseId)
{
	FFieldDefensePlacement RemovedPlacement;
	if (not M_Placements.RemoveAndCopyValue(FieldDefenseId, RemovedPlacement))
	{
		return;
	}
	DestroyStaticPreview(RemovedPlacement);
}

void FTrainerFieldDefenses::DestroyAllPlacements()
{
	TMap<int32, FFieldDefensePlacement> RemovedPlacements;
	Swap(RemovedPlacements, M_Placements);
	for (const TPair<int32, FFieldDefensePlacement>& PlacementPair : RemovedPlacements)
	{
		DestroyStaticPreview(PlacementPair.Value);
		DestroyUnfinishedExpansion(PlacementPair.Value);
	}
}

void FTrainerFieldDefenses::DestroyStaticPreview(const FFieldDefensePlacement& Placement)
{
	if (not Placement.StaticPreview.IsValid())
	{
		return;
	}
	Placement.StaticPreview->Destroy();
}

void FTrainerFieldDefenses::DestroyUnfinishedExpansion(const FFieldDefensePlacement& Placement)
{
	ABuildingExpansion* Expansion = Placement.Expansion.Get();
	if (not IsValid(Expansion))
	{
		return;
	}
	if (Expansion->GetBuildingExpansionStatus() == EBuildingExpansionStatus::BXS_Built)
	{
		// A finished field defense belongs to the player and no longer to the trainer's queue.
		return;
	}
	Expansion->Destroy();
}
