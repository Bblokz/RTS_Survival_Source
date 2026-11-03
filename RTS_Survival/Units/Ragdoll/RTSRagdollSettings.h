// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTSRagdollSettings.generated.h"

class UMaterialInterface;
class USkeletalMesh;

/** One side's crew: a mesh per pool, as a swap would rebuild the bodies, and looks that are free to swap. */
USTRUCT()
struct FRTSCrewBodyType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Crew")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	// One is picked per body; leave empty to keep the mesh's own material.
	UPROPERTY(EditAnywhere, Category="Crew")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
};

/** How one kind of tank death throws its crew. */
USTRUCT()
struct FRTSCrewThrow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Crew", meta=(ClampMin="0"))
	int32 MinBodies = 1;

	UPROPERTY(EditAnywhere, Category="Crew", meta=(ClampMin="0"))
	int32 MaxBodies = 2;

	// Sideways speed range in cm/s.
	UPROPERTY(EditAnywhere, Category="Crew", meta=(ClampMin="0"))
	FVector2D OutwardSpeed = FVector2D(150.0, 300.0);

	// Upward speed range in cm/s.
	UPROPERTY(EditAnywhere, Category="Crew", meta=(ClampMin="0"))
	FVector2D UpwardSpeed = FVector2D(250.0, 400.0);

	// Tumble in degrees per second.
	UPROPERTY(EditAnywhere, Category="Crew", meta=(ClampMin="0"))
	float MaxSpinDegrees = 180.f;
};

/**
 * @brief Project-wide crew bodies thrown out of dying vehicles (Project Settings > Game > Ragdolls).
 * Read by URTSRagdollSubsystem; C++ defaults hold the Scavenger and Mosin crews, so no ini edit is needed.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Ragdolls"))
class RTS_SURVIVAL_API URTSRagdollSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSRagdollSettings();

	UPROPERTY(Config, EditAnywhere, Category="Crew")
	FRTSCrewBodyType GermanCrew;

	UPROPERTY(Config, EditAnywhere, Category="Crew")
	FRTSCrewBodyType SovietCrew;

	UPROPERTY(Config, EditAnywhere, Category="Throw")
	FRTSCrewThrow OrdinaryDeath;

	UPROPERTY(Config, EditAnywhere, Category="Throw")
	FRTSCrewThrow CookOffDeath;

	// Parked bodies per side. They never spawn in play: an empty pool just means fewer bodies.
	UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="0"))
	int32 PoolSizePerSide = 16;

	// Ragdolls simulating at once, soldiers included. On flat ground 48 cost about as much as 8, so raise this if
	// real maps agree.
	UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="0"))
	int32 MaxSimulatingRagdolls = 24;

	// From this many simulating ragdolls an explosion throws one body instead of its usual count.
	UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="0"))
	int32 PressureSimulatingRagdolls = 16;

	// Bodies nobody sees cost nothing: vehicles hidden by fog of war or off screen throw no crew.
	UPROPERTY(Config, EditAnywhere, Category="Budget")
	bool bSkipUnseenVehicles = true;

	UPROPERTY(Config, EditAnywhere, Category="Lifetime", meta=(ClampMin="0"))
	float MaxSimulationSeconds = 2.f;

	UPROPERTY(Config, EditAnywhere, Category="Lifetime", meta=(ClampMin="0"))
	float SinkSeconds = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category="Lifetime", meta=(ClampMin="0"))
	float SinkDepthCm = 40.f;
};
