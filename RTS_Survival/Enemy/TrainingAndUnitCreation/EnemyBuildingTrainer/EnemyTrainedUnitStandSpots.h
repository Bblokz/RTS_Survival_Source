// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

class ANavigationData;

/** A strip straight out of a building exit that stays free, so the units still to come out are not blocked. */
struct FEnemyStandSpotKeepClearLane
{
	FVector Start = FVector::ZeroVector;
	// Flat and normalised.
	FVector Forward = FVector::ForwardVector;
	float Length = 0.f;
	float HalfWidth = 0.f;
};

/** Ground a unit of the faction stands on, or a spot one was sent to. */
struct FEnemyStandSpotOccupied
{
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
};

/** A stand spot a unit was sent to: taken while the unit walks there or stands on it. */
struct FEnemyStandSpotReservation
{
	TWeakObjectPtr<AActor> Unit;
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
	float ReservedAtSeconds = 0.f;
};

/** A building-trained unit that waits at its stand spot before it goes on to a base defense position. */
struct FEnemyStandSpotWait
{
	TWeakObjectPtr<AActor> Unit;
	float ReleaseAtSeconds = 0.f;
};

struct FEnemyStandSpotQuery
{
	FVector ExitLocation = FVector::ZeroVector;
	// Flat and normalised; points out of the building.
	FVector ExitForward = FVector::ForwardVector;
	// The unit faces away from this point once it stands.
	FVector BuildingCenter = FVector::ZeroVector;
	float UnitRadius = 300.f;
	// The navmesh the unit paths on; null uses the default (character) navmesh.
	TWeakObjectPtr<const ANavigationData> NavData;
	TArray<FEnemyStandSpotKeepClearLane> KeepClearLanes;
	TArray<FEnemyStandSpotOccupied> Occupied;
	FString UnitNameForLog;
};

struct FEnemyStandSpot
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	int32 CandidatesTried = 0;
	// False when no spot had a straight navmesh line from the exit and a reachable-looking one was taken instead.
	bool bHasStraightNavLine = true;
};

namespace EnemyTrainedUnitStandSpots
{
	/**
	 * @brief Finds where a unit a building just trained can wait without blocking its own side: its whole footprint
	 * on the navmesh, close to the exit, out of every keep-clear lane and clear of the faction's units and of the spots
	 * other units were sent to.
	 * Rings around the exit are tried nearest first, the area in front of the exit before the building's sides. A
	 * spot with a straight navmesh line from the exit wins; without any, the first spot that passes everything else.
	 * @return false when nothing within the search radius passes; the caller leaves the unit where it is.
	 */
	bool FindStandSpot(UWorld* World, const FEnemyStandSpotQuery& Query, FEnemyStandSpot& OutSpot);

	/** Ground radius of a unit: a squad from its member count, anything else from its root's local bounds. */
	float GetUnitFootprintRadius(const AActor* Unit);

	/** The navmesh a pawn (a tank) paths on for its agent size; null for anything else (a squad's controller). */
	const ANavigationData* GetNavDataForUnit(UWorld* World, const AActor* Unit);

	/** Ground radius the search gives a single soldier of the faction. */
	float GetSoldierFootprintRadius();
}
