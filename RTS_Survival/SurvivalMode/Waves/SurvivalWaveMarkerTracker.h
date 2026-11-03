// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "SurvivalWaveMarkerTracker.generated.h"

class AEnemyController;
struct FFormationData;

/** One launched wave group as the minimap shows it. */
USTRUCT()
struct FSurvivalWaveMarker
{
	GENERATED_BODY()

	// Base of the two minimap icon IDs (chevron and badge).
	FName IconId = NAME_None;

	// One wave from one edge; a large wave from one edge marches as several formations under one marker.
	FName GroupKey = NAME_None;

	// Last waypoints of the formations not found yet; a formation's last waypoint is exactly its group's point.
	TArray<FVector> PendingFinalWaypoints;

	FVector Location = FVector::ZeroVector;
	// Where the heading was last measured from; the heading follows the group's progress, not its jitter.
	FVector HeadingAnchor = FVector::ZeroVector;
	float HeadingYaw = 0.f;
	double LaunchTime = 0.0;

	// The group's units once its formation exists; the marker lives until all of them are gone.
	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> Units;

	bool bHasBoundUnits = false;
};

/**
 * @brief Shows every launched survival wave group on the minimap: a chevron along the group's heading plus an
 * upright badge, from the moment the wave is announced (at its spawn points, through unexplored fog) until its last
 * unit dies. Follows the centroid of the group's units, also after the formation reached the base.
 * @note Created and fed by USurvivalWaveDirectorComponent (InitTracker, OnGroupLaunched, StopTracking).
 */
UCLASS()
class RTS_SURVIVAL_API USurvivalWaveMarkerTracker : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * @param EnemyController Owner of the formations the waves march in.
	 * @param FormationBindTimeoutSeconds A marker whose formation never appears is removed after this.
	 */
	void InitTracker(AEnemyController* EnemyController, float FormationBindTimeoutSeconds);

	/**
	 * @brief Adds the marker of a formation that was just launched (its units appear a few seconds later), or
	 * joins it to the marker of the same wave and edge.
	 * @param GroupKey Same key for every formation of one wave from one edge.
	 * @param ExpectedFinalWaypoint Last waypoint of the formation, used to find its units.
	 * @param SpawnPoints Where the units will appear; a new marker starts at their centre.
	 * @param BaseLocation Where the group heads; the first heading points there.
	 */
	void OnGroupLaunched(const FName GroupKey, const FVector& ExpectedFinalWaypoint, const TArray<FVector>& SpawnPoints,
	                     const FVector& BaseLocation);

	/** @brief Removes every marker and stops updating (waves stopped, defeat, end of play). */
	void StopTracking();

	const TArray<FSurvivalWaveMarker>& GetMarkers() const { return M_Markers; }

#if RTS_WITH_MAP_TESTS
	/** @brief Test runs only: kills every unit bound to the marker with this icon ID. @return Units hit. */
	int32 DebugKillMarkerUnits(const FName IconId) const;
#endif

private:
	UPROPERTY()
	TWeakObjectPtr<AEnemyController> M_EnemyController;

	UPROPERTY()
	TArray<FSurvivalWaveMarker> M_Markers;

	float M_FormationBindTimeoutSeconds = 120.f;
	int32 M_NextMarkerIndex = 0;
	FTimerHandle M_UpdateTimerHandle;

	void EnsureUpdateTimer();
	void UpdateMarkers();
	void BindMarkersToFormations();
	void BindMarkerToFormations(FSurvivalWaveMarker& Marker, const TArray<FFormationData>& Formations) const;

	/** @return False when the marker has no unit left alive (or its formation never appeared) and must go. */
	bool UpdateMarker(FSurvivalWaveMarker& Marker, double Now) const;

	void AddMarkerIcons(const FSurvivalWaveMarker& Marker) const;
	void MoveMarkerIcons(const FSurvivalWaveMarker& Marker) const;
	void RemoveMarkerIcons(const FSurvivalWaveMarker& Marker) const;
	static FName GetBadgeIconId(const FSurvivalWaveMarker& Marker);
	/** @return The marker location pulled inside the minimap so an icon at the map edge is drawn whole. */
	FVector GetDrawLocation(const FVector& Location) const;

	bool GetIsValidEnemyController() const;
};
