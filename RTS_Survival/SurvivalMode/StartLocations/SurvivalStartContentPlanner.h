// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalDefensePointFinder.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartContentSettings.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"

class AActor;
class UStaticMesh;
class UWorld;

/** @brief One actor the plan wants spawned. */
struct FSurvivalPlannedActor
{
	TSoftClassPtr<AActor> ActorClass;
	FTransform Transform = FTransform::Identity;
	ESurvivalStartContentCategory Category = ESurvivalStartContentCategory::Crater;
	float FootprintRadius = 0.f;
};

/** @brief One foliage instance the plan wants added (instanced, no collision). */
struct FSurvivalPlannedMeshInstance
{
	TSoftObjectPtr<UStaticMesh> Mesh;
	FTransform Transform = FTransform::Identity;
};

/** @brief A resource field disc; used by the debug preview and to keep wrecks/craters out of fields. */
struct FSurvivalPlannedField
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.f;
	ESurvivalStartContentCategory Category = ESurvivalStartContentCategory::Radixite;
};

/**
 * @brief Everything one start location will spawn; built synchronously (traces only), spawned after the soft
 * classes finished loading. Also drawn by the editor preview.
 */
struct FSurvivalStartContentPlan
{
	TArray<FSurvivalPlannedActor> Actors;
	TArray<FSurvivalPlannedMeshInstance> MeshInstances;
	TArray<FSurvivalPlannedField> Fields;

	// 0 keeps the mesh defaults.
	float FoliageCullDistance = 0.f;

	int32 RequestedRadixiteFields = 0;
	int32 RequestedMetalFields = 0;
	int32 RequestedWrecks = 0;
	int32 RequestedPits = 0;

	int32 GetPlannedFieldCount(const ESurvivalStartContentCategory Category) const;
	int32 GetPlannedActorCount(const ESurvivalStartContentCategory Category) const;

	/** @brief Unique soft paths of every class and mesh in the plan, for one async load request. */
	void CollectSoftObjectPaths(TArray<FSoftObjectPath>& OutPaths) const;

	void Reset();
};

/** @brief The world facts a plan is built from; filled by ASurvivalStartLocation. */
struct FSurvivalStartContentPlanInput
{
	FVector DeployCenter = FVector::ZeroVector;
	float BuildAreaRadius = 6000.f;
	// Ordered by priority: designer points, hill tops, build-edge fallback.
	TArray<FSurvivalDefensePoint> DefensePoints;
	// Build areas of the other start locations (Soviet bases) grown by OtherStartLocationClearance.
	TArray<FSurvivalReservedCircle> OtherStartAreas;
	ERTSGameDifficulty Difficulty = ERTSGameDifficulty::Normal;
	int32 Seed = 0;
};

/**
 * @brief Plans the player's start content in a fixed order so the important things get the best spots:
 * pits on defense points → protected areas (build area, hill tops, other starts) → radixite/metal fields
 * interleaved around the base → wrecks further out → craters → foliage.
 * @note Stack-only helper: create, call BuildPlan once, discard. Landscape-only ground tests.
 */
class FSurvivalStartContentPlanner
{
public:
	FSurvivalStartContentPlanner(
		UWorld& World,
		const USurvivalStartContentSettings& Settings,
		const FSurvivalStartContentPlanInput& Input);

	/** @return The finished plan; fewer items than requested when the terrain had no valid room. */
	FSurvivalStartContentPlan BuildPlan();

private:
	const USurvivalStartContentSettings& M_Settings;
	const FSurvivalStartContentPlanInput& M_Input;
	FSurvivalGroundProjector M_Projector;
	FSurvivalOccupancy M_Occupancy;
	FRandomStream M_Random;
	FSurvivalStartContentDifficultyScale M_Scale;
	FSurvivalStartContentPlan M_Plan;

	void PlanPits();
	void ReserveProtectedAreas();
	void PlanResourceFields();
	void PlanWrecks();
	void PlanWreckGroup(const FSurvivalWreckGroup& Group);
	void PlanCraters();
	void PlanFoliage();

	/**
	 * @brief Searches around the preferred angle (and further out after each sweep) for a free, buildable disc.
	 * @param FieldSettings Radius, distances and slope of this resource type.
	 * @param Category Radixite or Metal.
	 * @param PreferredAngle Radians; the slot this field was assigned so both types spread around the base.
	 * @param SlotAngle Radians between two field slots; limits the search step.
	 * @return True when a field was planned and populated.
	 */
	bool TryPlanField(
		const FSurvivalResourceFieldSettings& FieldSettings,
		const ESurvivalStartContentCategory Category,
		const float PreferredAngle,
		const float SlotAngle);

	bool GetIsFieldGroundValid(
		const FVector2D& Center,
		const FSurvivalResourceFieldSettings& FieldSettings,
		FSurvivalGroundHit& OutCenterHit) const;

	void PopulateField(
		const FSurvivalResourceFieldSettings& FieldSettings,
		const ESurvivalStartContentCategory Category,
		const FSurvivalGroundHit& CenterHit);

	/** @return True when one field actor found room inside the field and was planned. */
	bool TryPlaceFieldActor(
		const FSurvivalResourceFieldSettings& FieldSettings,
		const ESurvivalStartContentCategory Category,
		const FVector2D& FieldCenter,
		FSurvivalOccupancy& FieldOccupancy);

	/** @return True when Position projects onto the landscape with a slope of at most MaxSlopeDegrees. */
	bool TryFindGround(const FVector2D& Position, const float MaxSlopeDegrees, FSurvivalGroundHit& OutHit) const;

	void AddPlannedActor(
		const FSurvivalWeightedActorClass& Entry,
		const FSurvivalGroundHit& Ground,
		const float YawDegrees,
		const ESurvivalStartContentCategory Category);

	/** @return Base * Multiplier rounded; 0 when Base is 0, else at least MinWhenBasePositive. */
	static int32 ScaleCount(const int32 Base, const float Multiplier, const int32 MinWhenBasePositive);

	float GetRandomYaw();
	FVector2D GetDeployCenter2D() const { return FVector2D(M_Input.DeployCenter); }
};
