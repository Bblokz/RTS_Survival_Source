// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"

#include "TrainerFieldDefenses.generated.h"

class ABuildingExpansion;
class AStaticPreviewMesh;

/**
 * @brief Where a field defense of a trainer's queue is in its life; only planned ones count towards the
 * limit of field defenses the player may place ahead.
 */
UENUM()
enum class EFieldDefenseConstructionState : uint8
{
	// Placed by the player; waits until its queue item becomes the active item.
	Planned,
	// Its queue item is active and the expansion class is loading.
	LoadingExpansion,
	// The expansion stands at the planned spot and follows the progress of its queue item.
	Constructing
};

/**
 * @brief One field defense the player placed from a trainer's training menu: the spot, the static preview that
 * marks it while the item waits in the queue and, once its turn came, the expansion being built there.
 */
USTRUCT()
struct FFieldDefensePlacement
{
	GENERATED_BODY()

	UPROPERTY()
	EBuildingExpansionType BxpType = EBuildingExpansionType::BXT_Invalid;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	// Marks and blocks the planned spot until the expansion replaces it.
	UPROPERTY()
	TWeakObjectPtr<AStaticPreviewMesh> StaticPreview;

	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> Expansion;

	UPROPERTY()
	EFieldDefenseConstructionState State = EFieldDefenseConstructionState::Planned;
};

/**
 * @brief The field defenses a trainer has planned or is building; the trainer component keeps one so that the
 * previews and unfinished constructions live and die with the trainer's owner.
 * Queue items refer to a placement by the id returned from AddPlacement.
 */
USTRUCT()
struct FTrainerFieldDefenses
{
	GENERATED_BODY()

	/** @return The id under which the placement is stored. */
	int32 AddPlacement(const FFieldDefensePlacement& Placement);

	FFieldDefensePlacement* FindPlacement(const int32 FieldDefenseId);
	const FFieldDefensePlacement* FindPlacement(const int32 FieldDefenseId) const;

	/** @return How many placements still wait for their construction to start. */
	int32 GetPlannedCount() const;

	/** @return The ids of placements whose expansion was destroyed or died while it was being built. */
	TArray<int32> GetIdsWithLostConstruction() const;

	/** @brief Destroys the preview and, if it is not finished, the expansion of this placement and forgets it. */
	void DestroyPlacement(const int32 FieldDefenseId);

	/** @brief Forgets a placement whose expansion is finished; destroys the preview if one is left. */
	void ReleaseFinishedPlacement(const int32 FieldDefenseId);

	/** @brief Destroys every preview and every expansion still under construction. */
	void DestroyAllPlacements();

private:
	UPROPERTY()
	TMap<int32, FFieldDefensePlacement> M_Placements;

	UPROPERTY()
	int32 M_NextFieldDefenseId = 1;

	static void DestroyStaticPreview(const FFieldDefensePlacement& Placement);
	static void DestroyUnfinishedExpansion(const FFieldDefensePlacement& Placement);
};
