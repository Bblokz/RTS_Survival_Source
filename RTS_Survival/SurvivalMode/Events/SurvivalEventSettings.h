// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"

#include "SurvivalEventSettings.generated.h"

class UConvoyDefinition;
class USoundBase;

RTS_SURVIVAL_API DECLARE_LOG_CATEGORY_EXTERN(LogSurvivalEvents, Log, All);

/** One convoy the scheduler may roll. */
USTRUCT(BlueprintType)
struct FSurvivalConvoyEventEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TSoftObjectPtr<UConvoyDefinition> Definition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0"))
	float Weight = 1.f;
};

/** From this survived round on, convoys come with this guard tier (index into the definition's tiers). */
USTRUCT(BlueprintType)
struct FSurvivalConvoyTierStep
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "1"))
	int32 FromRound = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0"))
	int32 GuardTier = 0;
};

/** A circle the convoy route keeps out of (the oasis). */
USTRUCT(BlueprintType)
struct FSurvivalEventExclusionZone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	FVector2D Centre = FVector2D::ZeroVector;

	// Already includes the margin.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0"))
	float Radius = 0.f;
};

/** Where convoy routes may run relative to the player and the Soviet bases. */
USTRUCT(BlueprintType)
struct FSurvivalConvoyRouteSettings
{
	GENERATED_BODY()

	// Closest approach of the route to the player's deploy centre must lie in this band.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float MinClosestApproach = 15000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float MaxClosestApproach = 26000.f;

	// The planner aims the route's middle at this distance from the base.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float TargetClosestApproach = 19000.f;

	// Unpicked starts carry Soviet bases: the route keeps this far from their deploy centres.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float SovietBaseClearance = 16000.f;

	// Entry and exit lie at least this far from the base (spawned out of sight, leaves out of sight).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float MinEndpointDistanceFromBase = 30000.f;

	// No player unit may stand this close to the entry when the convoy spawns.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float MinSpawnDistanceFromPlayerUnits = 9000.f;

	// Edge zones the last wave used are avoided by this much.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float ActiveSpawnZoneClearance = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float MinRouteLength = 50000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float MaxRouteLength = 150000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TArray<FSurvivalEventExclusionZone> ExclusionZones;

	// Distance between the route points handed to the convoy.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	float RouteResampleSpacing = 600.f;

	// Candidates checked against the real navmesh per plan (each is a few chained path queries).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "1"))
	int32 MaxNavCandidates = 6;
};

/**
 * @brief Tuning of the survival random events (first type: escorted supply convoys): when they roll, how strong
 * the guard is, what the cargo is worth, where the route runs and how long the intermission waits.
 * Loaded by USurvivalEventSubsystem from DA_SurvivalEventSettings; the C++ defaults are the shipped values.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API USurvivalEventSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USurvivalEventSettings();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TArray<FSurvivalConvoyEventEntry> Convoys;

	// Chance that a survived round (from FirstAllowedRound on, outside the cooldown) gets an event.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0", ClampMax = "1"))
	float EventChance = 0.45f;

	// Survived rounds before the first event may roll (round 1 is the first wave).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "1"))
	int32 FirstAllowedRound = 2;

	// Rounds without an event after one fired.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0"))
	int32 CooldownRounds = 1;

	// After this many rounds without an event (once allowed) the next one is forced.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "1"))
	int32 ForceAfterQuietRounds = 4;

	// The first 180 s grace may host an event too.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	bool bAllowInFirstGrace = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TArray<FSurvivalConvoyTierStep> GuardTierByRound;

	// Hard and above step one tier up from this round earlier than the table says.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TMap<ERTSGameDifficulty, int32> TierBonusByDifficulty;

	// Cargo multiplier per difficulty (harder games pay more for the same guard).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TMap<ERTSGameDifficulty, float> CargoMultiplierByDifficulty;

	// The convoy spawns in the clearing phase once this few round enemies are left (design: spawn out of sight
	// before the intermission, announce when it starts).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0"))
	int32 ClearingSpawnEnemiesThreshold = 4;

	// While a convoy is live the intermission is extended by up to this (shown on the HUD countdown), ended early
	// when the convoy escapes, its trucks are destroyed and their cargo is taken or gone.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "0"))
	float MaxIntermissionExtensionSeconds = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	FSurvivalConvoyRouteSettings Route;

	// Banner seconds.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events", meta = (ClampMin = "1"))
	float AnnouncementSeconds = 9.f;

	// Played once with the announcement; none plays nothing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Events")
	TSoftObjectPtr<USoundBase> AnnouncementSound;

	int32 GetGuardTierForRound(int32 Round, ERTSGameDifficulty Difficulty) const;
	float GetCargoMultiplier(ERTSGameDifficulty Difficulty) const;
};
