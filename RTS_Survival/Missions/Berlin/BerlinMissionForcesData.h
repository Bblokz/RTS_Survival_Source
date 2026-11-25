// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawnerTypes.h"
#include "RTS_Survival/Enemy/Fallback/EnemyFallbackSubsystem.h"
#include "RTS_Survival/Enemy/Strongpoint/EnemyStrongpoint.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Missions/PlayerAirDrop/PlayerAirDropSubsystem.h"

#include "BerlinMissionForcesData.generated.h"

/** The Forge survey station part of the Berlin forces (Source/Docs/Campaign/BerlinForgeStation.md, sections 4 and 7). */
USTRUCT(BlueprintType)
struct FBerlinForgeStationForces
{
	GENERATED_BODY()

	// Units per named post of the station strongpoint (Arch, RimCrewA, Console, RigFoot, RimCrewB, CrewHut,
	// CrewEscort); posts left out stay unmanned on this difficulty.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	TArray<FEnemyStrongpointPostUnits> Garrison;

	// HQ flies these in when the station is destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Forge Station")
	FPlayerAirDropPayload Reinforcements;
};

/** The early patrols near the player start and when beaten Soviets run (BerlinMissionScripting.md section 8). */
USTRUCT(BlueprintType)
struct FBerlinPatrolForces
{
	GENERATED_BODY()

	// The units of ONE patrol: every placed BerlinPatrol_* wave spawner spawns this list.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrols")
	TArray<FEnemyWaveSpawnerUnitEntry> PatrolUnits;

	// When a beaten squad (patrol or garrison) gives up and runs to the Reichstag forecourt, where the alarm goes off.
	// A patrol's infantry also runs once its last vehicle is destroyed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrols")
	FEnemyFallbackSettings Fallback;
};

/**
 * @brief The tunable enemy side of the Berlin mission, one asset per difficulty (BerlinMissionFlow.md 4.5). Missions
 * pick the asset of the running difficulty with LoadForDifficulty; it holds the Forge survey station, the early
 * patrols and the fleeing thresholds (later the Reichstag factory).
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UBerlinMissionForcesData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station")
	FBerlinForgeStationForces ForgeStation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrols")
	FBerlinPatrolForces Patrols;

	/**
	 * @brief Loads the forces asset of a difficulty, so every Berlin stage picks the same asset the same way.
	 * @param ForcesPerDifficulty One asset per difficulty; Ironman falls back to Brutal, a missing one to Normal.
	 * @param Difficulty The running game difficulty.
	 * @return The loaded asset, or null when not even Normal is set.
	 */
	UFUNCTION(BlueprintCallable, Category = "Berlin|Forces")
	static UBerlinMissionForcesData* LoadForDifficulty(
		const TMap<ERTSGameDifficulty, TSoftObjectPtr<UBerlinMissionForcesData>>& ForcesPerDifficulty,
		const ERTSGameDifficulty Difficulty);

	/** @return How many units one patrol spawns (the wave supply a patrol needs). */
	UFUNCTION(BlueprintPure, Category = "Berlin|Forces")
	int32 GetPatrolUnitCount() const;
};
