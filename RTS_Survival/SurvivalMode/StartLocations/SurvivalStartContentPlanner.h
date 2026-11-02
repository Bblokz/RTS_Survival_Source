// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalDefensePointFinder.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartContentSettings.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"

class ANavigationData;

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
	// Close nodes by the drop-off (requested and planned) and the special's pieces.
	int32 RequestedCloseRadixite = 0;
	int32 RequestedCloseMetal = 0;
	int32 PlannedCloseRadixite = 0;
	int32 PlannedCloseMetal = 0;
	int32 RequestedSpecialPieces = 0;
	int32 PlannedSpecialPieces = 0;

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
	// This start's fixed special (hangar, wrecks, extra radixite or metal by the drop-off).
	ESurvivalStartSpecial Special = ESurvivalStartSpecial::None;
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

	TWeakObjectPtr<UWorld> M_World;
	// Navigation data used for the reachability test (heavy tank agent) and the deploy centre projected onto it;
	// null when the world has no navigation data yet (then only the ground tests apply).
	TWeakObjectPtr<const ANavigationData> M_ReachNavData;
	FVector M_ReachDeploy = FVector::ZeroVector;

	// Wrecks that may land below the bench (more than WreckOffBenchDrop under the deploy centre); set by PlanWrecks.
	int32 M_MaxWrecksOffBench = 0;
	int32 M_WrecksOffBench = 0;

	// Extra metal nodes already planned in the scrap yard; PlanCloseResources plans only the rest.
	int32 M_YardMetalNodes = 0;

	void PlanPits();
	void ReserveProtectedAreas();
	// Round 10: the special and the close nodes go first, inside the build area but outside the landing ring.
	void PlanSpecial();
	bool TryPlanHangar(const FSurvivalStartSpecialSettings& Special);
	void PlanSpecialWrecks(const FSurvivalStartSpecialSettings& Special);
	// Round 11: the metal special's extra nodes in one scrap yard; nodes that do not fit fall back to PlanCloseResources.
	void TryPlanScrapYard(const FSurvivalStartSpecialSettings& Special);
	int32 PlanYardNodes(const FSurvivalStartSpecialSettings& Special, const FVector2D& YardCenter);
	void PlanYardDressing(const FSurvivalStartSpecialSettings& Special, const FVector2D& YardCenter, const FVector2D& Outward);
	void PlanCloseResources();
	bool TryPlanCloseNode(const TArray<FSurvivalWeightedActorClass>& Actors, ESurvivalStartContentCategory Category,
	                      float PreferredAngle);
	void PlanGroundDetail();
	/** @return True when Position keeps out of the landing ring of the HQ and the drop pods. */
	bool GetIsOutsideLandingRing(const FVector2D& Position, float Radius) const;
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

	/**
	 * @return True when one field actor found room inside the field and was planned.
	 * @param CentreFootprint Footprint of the field's centre actor (0 without one): entries that cannot fit beside it
	 * are skipped, so a large centre actor does not silently starve the field.
	 */
	bool TryPlaceFieldActor(
		const FSurvivalResourceFieldSettings& FieldSettings,
		const ESurvivalStartContentCategory Category,
		const FVector2D& FieldCenter,
		const float CentreFootprint,
		FSurvivalOccupancy& FieldOccupancy);

	/** @brief Finds the heavy-tank navigation data and projects the deploy centre onto it (once per plan). */
	void InitReachability();

	/** @return True when Location lies on the navmesh with a full path from the deploy centre (true without navmesh). */
	bool GetIsReachable(const FVector& Location) const;

	/**
	 * @return True when the ground round a field actor or wreck (its footprint plus an access margin) keeps the slope
	 * limit and the spot is reachable, so no resource can end up on a ledge, a cliff edge or off the navmesh.
	 */
	bool GetIsActorSpotValid(const FVector2D& Position, const FSurvivalGroundHit& Ground, const float FootprintRadius,
	                         const float MaxSlopeDegrees) const;

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
