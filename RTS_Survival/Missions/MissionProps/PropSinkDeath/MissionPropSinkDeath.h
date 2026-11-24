// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"

#include "MissionPropSinkDeath.generated.h"

class AActor;
class UMaterialInterface;
class UNiagaraSystem;

/** @brief One kind of ground decal left where a mission prop died; a material is picked at random per decal. */
USTRUCT(BlueprintType)
struct FMissionPropDeathDecal
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal", meta = (ClampMin = "1"))
	int32 Count = 1;

	// Random decal size (square, cm) in [X, Y].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal")
	FVector2D SizeRange = FVector2D(400.f, 500.f);

	// Each decal lands up to this far (cm, X and Y) from the death location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decal", meta = (ClampMin = "0"))
	float Scatter = 0.f;
};

/**
 * @brief Death of a mission prop: a smoke column at the spot, the actor sinking straight into the ground over a few
 * seconds and then destroyed, and permanent ground decals where it stood. While it sinks it counts as dead, takes no
 * damage and no longer blocks shells or weapon traces, so nothing ends the sink early.
 */
USTRUCT(BlueprintType)
struct FMissionPropSinkDeathSettings
{
	GENERATED_BODY()

	// Spawned once at the death location (not attached); it keeps rising while the actor sinks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	TSoftObjectPtr<UNiagaraSystem> SmokeSystem;

	// World offset of the smoke from the ground under the actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	FVector SmokeOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	FVector SmokeScale = FVector::OneVector;

	// One burst lasts ~5 s; more bursts keep the spot smoking through the whole sink.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "1", ClampMax = "8"))
	int32 SmokeBurstCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke", meta = (ClampMin = "0.1"))
	float SmokeBurstInterval = 3.f;

	// User parameters for the project's destruction smoke systems (size, lifetime, colour).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoke")
	FCollapseParameters SmokeParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sink", meta = (ClampMin = "0.1"))
	float SinkSeconds = 8.f;

	// How far the actor sinks; use at least its full height so nothing is left above ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sink", meta = (ClampMin = "0"))
	float SinkDepth = 500.f;

	// Random tilt (degrees) the actor leans over while it sinks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sink", meta = (ClampMin = "0"))
	float MaxTiltDegrees = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decals")
	TArray<FMissionPropDeathDecal> Decals;
};

/** @brief Runs an FMissionPropSinkDeathSettings death on any actor. */
class RTS_SURVIVAL_API FMissionPropSinkDeath
{
public:
	/**
	 * @brief Loads the smoke and decal assets now so the death does not hitch later.
	 * @param Settings The death to preload.
	 * @param OutLoadedAssets Keep these referenced (UPROPERTY) for as long as the death may still happen.
	 */
	static void PreloadAssets(const FMissionPropSinkDeathSettings& Settings, TArray<TObjectPtr<UObject>>& OutLoadedAssets);

	/**
	 * @brief Leaves the decals and smoke at the actor's ground point, then sinks the actor and destroys it.
	 * @param DyingActor Actor to sink; destroyed when the sink ends.
	 * @param Settings Smoke, sink and decals.
	 * @return False when the actor or its world is invalid.
	 */
	static bool Start(AActor* DyingActor, const FMissionPropSinkDeathSettings& Settings);

	/** @return Ground point under the actor (WorldStatic trace, ignores the actor); the actor location without ground. */
	static FVector GetGroundPointUnder(const AActor* Actor);

private:
	static void SpawnDecals(UWorld* World, const FVector& GroundPoint, const FMissionPropSinkDeathSettings& Settings);
	static void SpawnSmoke(const AActor* DyingActor, const FVector& GroundPoint,
	                       const FMissionPropSinkDeathSettings& Settings);
	/** Marks the actor dead, makes it immune to damage and lets shells and weapon traces pass through it. */
	static void StopDamageAndWeaponHits(AActor* DyingActor);
	static void StartSink(AActor* DyingActor, const FMissionPropSinkDeathSettings& Settings);
};
