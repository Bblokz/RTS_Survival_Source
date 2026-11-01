// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"

struct FSurvivalConvoyRouteSettings;
class UWorld;

/** One edge stretch the convoy may enter or leave by. */
struct FSurvivalConvoyEndpoint
{
	FString Name;
	FVector Location = FVector::ZeroVector;
};

/** Everything the planner needs; filled from the survival director (or a test). */
struct FSurvivalConvoyRouteRequest
{
	FVector BaseCentre = FVector::ZeroVector;
	TArray<FVector> SovietBaseCentres;
	TArray<FSurvivalConvoyEndpoint> Endpoints;
	// Spawn points of the waves of this round; endpoints near them are not used.
	TArray<FVector> ActiveSpawnLocations;
	TArray<FVector> PlayerUnitLocations;
	const FSurvivalConvoyRouteSettings* Settings = nullptr;
	int32 Seed = 0;
	// Longest route that still fits the decision window at the convoy pace (0 = no limit beyond the settings).
	float LengthBudget = 0.f;
	// Test override: only this entry / exit pair (names), empty = any.
	FString ForcedEntry;
	FString ForcedExit;
};

/** Measures of a route against the request's rules. */
struct FSurvivalConvoyRouteMetrics
{
	float Length = 0.f;
	float ClosestApproach = 0.f;
	float MinSovietBaseDistance = 0.f;
	float MinExclusionDistance = 0.f;
	float MinActiveSpawnDistance = 0.f;
	FString Violation;

	bool GetIsValid() const { return Violation.IsEmpty(); }
};

struct FSurvivalConvoyRoutePlan
{
	bool bIsValid = false;
	FConvoyRoute Route;
	FString EntryName;
	FString ExitName;
	FSurvivalConvoyRouteMetrics Metrics;
	int32 CandidatesConsidered = 0;
	int32 NavCandidatesTried = 0;
	FString Notes;
};

/**
 * @brief Picks a convoy route from one edge stretch to another that passes within reach of the player's base but
 * never through it, a Soviet base, the oasis or this round's spawn zones; checked on the real vehicle navmesh.
 */
class RTS_SURVIVAL_API FSurvivalConvoyRoutePlanner
{
public:
	/**
	 * @brief Builds candidates (entry, via point near the base, exit), scores them on straight lines, then builds
	 * the best few on the navmesh until one passes every rule.
	 * @param World The match world.
	 * @param Request Base, bases, endpoints and rules.
	 * @return The plan; bIsValid false with notes when no candidate passed.
	 */
	static FSurvivalConvoyRoutePlan Plan(UWorld* World, const FSurvivalConvoyRouteRequest& Request);

	/** @return The route's measures; Violation names the first broken rule. */
	static FSurvivalConvoyRouteMetrics Evaluate(const TArray<FVector>& Points, const FSurvivalConvoyRouteRequest& Request);

private:
	struct FCandidate
	{
		int32 EntryIndex = INDEX_NONE;
		int32 ExitIndex = INDEX_NONE;
		TArray<FVector> ControlPoints;
		float Score = 0.f;
	};

	static void BuildCandidates(const FSurvivalConvoyRouteRequest& Request, FRandomStream& Random,
	                            TArray<FCandidate>& OutCandidates);
	static bool GetIsEndpointUsable(const FSurvivalConvoyRouteRequest& Request, const FSurvivalConvoyEndpoint& Endpoint,
	                                bool bIsEntry);
	static TArray<FVector> Densify(const TArray<FVector>& ControlPoints, float Spacing);
};
