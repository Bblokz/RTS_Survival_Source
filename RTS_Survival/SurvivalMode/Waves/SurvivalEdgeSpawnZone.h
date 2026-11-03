// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SurvivalEdgeSpawnZone.generated.h"

class UArrowComponent;
class UBoxComponent;
class UTextRenderComponent;

/**
 * @brief A stretch of the map edge where Soviet waves enter. Place several along the edges of a survival map:
 * the box's Y extent is the length along the edge, X the depth, and the actor's forward (arrow) points inward.
 * The wave director picks zones by weight, skips them before ActiveAfterMinute, and spawns units on navmesh
 * points spread along the box.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASurvivalEdgeSpawnZone : public AActor
{
	GENERATED_BODY()

public:
	ASurvivalEdgeSpawnZone();

	/** @return Normalised forward on the XY plane: the direction waves walk into the map. */
	FVector GetInwardDirection() const;

	/** @return Centre of the spawn box. */
	FVector GetSpawnCenter() const;

	float GetEdgeLength() const;
	float GetSelectionWeight() const { return SelectionWeight; }
	int32 GetMaxUnitsPerWave() const { return MaxUnitsPerWave; }

	/** @return Whether the designer left the zone usable at all (enabled with a positive weight). */
	bool GetIsEnabled() const { return bEnabled and SelectionWeight > 0.f; }

	/** @return Whether the zone may be used at this survival minute. */
	bool GetIsActiveAtMinute(const float SurvivalMinute) const;

	/**
	 * @brief Name for the HUD: DisplayName when set, else a compass direction ("North-East") seen from
	 * ReferenceLocation, with +X as north and +Y as east.
	 * @param ReferenceLocation Usually the player base the wave is aimed at.
	 * @return Name to show in "incoming from <edge>".
	 */
	FText GetEdgeDisplayName(const FVector& ReferenceLocation) const;

	/**
	 * @brief Spreads spawn points over a front along the edge (extra rows step inward) and projects them to the
	 * navmesh so wave units never spawn off-mesh at the map border.
	 * @param PointCount How many points are wanted.
	 * @param MinSpacing Distance between neighbouring points.
	 * @param OutPoints Receives up to PointCount points; repeats are offset copies when the zone is too small.
	 * @return False when not a single point could be projected (zone off the navmesh).
	 */
	bool GetSpawnPoints(const int32 PointCount, const float MinSpacing, TArray<FVector>& OutPoints) const;

	/**
	 * @brief A navigable point StagingDistance inward from the spawn centre; the first waypoint of every wave.
	 * @param StagingDistance How far inward from the spawn line.
	 * @param OutLocation The projected point (spawn centre projected when the inward point is off-mesh).
	 * @return False when neither point is on the navmesh.
	 */
	bool GetStagingLocation(const float StagingDistance, FVector& OutLocation) const;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Survival|Edge")
	TObjectPtr<UBoxComponent> SpawnArea = nullptr;

	// HUD name, e.g. "North" or "the river crossing"; empty uses the compass direction from the base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Edge")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Edge")
	bool bEnabled = true;

	// Relative chance of being picked compared with the other eligible zones.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Edge", meta = (ClampMin = "0.0"))
	float SelectionWeight = 1.f;

	// The zone is only used from this survival minute on (0 = from the first wave).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Edge", meta = (ClampMin = "0.0"))
	float ActiveAfterMinute = 0.f;

	// Units this zone may spawn per wave (narrow passes); 0 = unlimited. Surplus goes to the other picked edges.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Edge", meta = (ClampMin = "0"))
	int32 MaxUnitsPerWave = 0;

	// Vertical half extent of the navmesh projection; covers terrain height differences inside the box.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Survival|Edge", meta = (ClampMin = "100.0"))
	float NavProjectionHalfHeight = 5000.f;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> M_InwardArrow = nullptr;

	UPROPERTY()
	TObjectPtr<UTextRenderComponent> M_Label = nullptr;
#endif

	bool GetIsValidSpawnArea() const;
	bool TryProjectToNavigation(const FVector& Location, const float HorizontalExtent, FVector& OutLocation) const;

	/**
	 * @brief Projects one candidate; when it is off-mesh, retries a few steps further inward.
	 * @param Candidate Point on the spawn front.
	 * @param HorizontalExtent Horizontal projection extent.
	 * @param OutLocation Projected point.
	 * @return True when any of the attempts projected.
	 */
	bool TryProjectWithInwardRetries(const FVector& Candidate, const float HorizontalExtent,
	                                 FVector& OutLocation) const;

	void UpdateEditorLabel() const;
	static FText GetCompassName(const FVector& Direction);
};
