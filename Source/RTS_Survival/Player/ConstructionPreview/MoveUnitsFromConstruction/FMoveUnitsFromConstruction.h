// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FMoveUnitsFromConstruction.generated.h"

class ANomadicVehicle;
class ASquadController;
class URTSComponent;

/**
 * @brief Moves player units away from a construction preview if they overlap its bounds.
 * Why: avoid units being trapped in a freshly placed building while keeping code reusable + testable.
 */
USTRUCT()
struct FMoveUnitsFromConstruction
{
	GENERATED_BODY()

public:
	/**
	 * @brief Scan the preview actor bounds and order overlapping player-1 units to move away.
	 * @param WorldContextObject Any UObject with a world (for projection).
	 * @param PreviewActor The preview actor that contains the visual mesh.
	 * @param FinishedMoveRotation Units will adopt this final rotation upon arriving.
	 * @param NomadicToIgnore
	 */
	static void MoveOverlappingUnitsForPlayer1AwayFromConstruction(
		const UObject* WorldContextObject,
		const AActor* PreviewActor,
		const FRotator& FinishedMoveRotation, ANomadicVehicle* NomadicToIgnore);

private:
	/** @return World-space AABB of the preview. */
	static FBox GetWorldBoundsOfPreview(const AActor* PreviewActor);

	/** Gathers overlapping candidate actors using object channels that represent movable player units. */
	static void GatherOverlappingMoveUnitActors(
		const UObject* WorldContextObject,
		const AActor* PreviewActor,
		const FBox& BoundsWS,
		TArray<AActor*>& OutActors);

	/** @return true if actor has an RTSComponent with OwningPlayer == 1. */
	static bool IsOwnedByPlayer1MoveUnit(const AActor* Actor);

	/** Tries a few random targets and issues a move command (queue reset). */
	static void IssueMoveOrderIfPossible(
		AActor* Actor,
		const UObject* WorldContextObject,
		const FBox& BoundsWS,
		const FRotator& FinishedMoveRotation, TArray<ASquadController*>& SquadControllers);

	/** Random 2D direction on X/Y plane. */
	static FVector2D RandomUnitDir2D();

	/**
	 * @brief Picks a random point OUTSIDE the AABB at a radial distance of [Min, Max] from the box surface.
	 * @param BoundsWS World-space AABB.
	 * @param MinDistFromBounds Minimum outward distance from the box surface.
	 * @param MaxDistFromBounds Maximum outward distance from the box surface.
	 * @param OutPointXZ Point with the same Z as Bounds center.
	 */
	static void PickRandomOutsidePointOnPlane(
		const FBox& BoundsWS,
		float MinDistFromBounds,
		float MaxDistFromBounds,
		FVector& OutPointXZ);

	/** Projects location using nav/terrain helpers; returns false if projection failed. */
	static bool TryProject(
		const UObject* WorldContextObject,
		const FVector& Original,
		FVector& OutProjected);

	static ASquadController* AttemptGetSquadControllerFromUnit(AActor* Unit);
};
