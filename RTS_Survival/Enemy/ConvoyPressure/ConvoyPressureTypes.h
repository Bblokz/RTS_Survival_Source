// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyTypes.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawnerTypes.h"

#include "ConvoyPressureTypes.generated.h"

class AActor;
class AEnemyPlacementActor;
class ADormantDefenceSlot;

/**
 * An attack-move wave the director builds itself, so its path can end at targets that change hands during the
 * mission (stations the player captured). Only targets that are alive and hostile to the director's enemy player
 * become waypoints, nearest first from the spawn points.
 */
USTRUCT(BlueprintType)
struct FConvoyPressureTargetedAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeted Attack")
	bool bEnabled = false;

	// Actors the wave goes after; one that is dead or not hostile (not captured yet) is skipped when the wave starts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeted Attack",
		meta = (EditCondition = "bEnabled", EditConditionHides))
	TArray<TObjectPtr<AActor>> Targets;

	// On: only targets owned by a player hostile to the director's enemy player count (an RTS component's owner).
	// Off: every living target counts, for targets that do not change owner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeted Attack",
		meta = (EditCondition = "bEnabled", EditConditionHides))
	bool bOnlyHostileTargets = true;

	// Relative to the director; drag the diamonds. Units are spread over the points in turn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeted Attack",
		meta = (MakeEditWidget = "true", EditCondition = "bEnabled", EditConditionHides))
	TArray<FVector> SpawnPoints = {FVector::ZeroVector};

	// SpawnPointIndex of an entry indexes SpawnPoints above.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeted Attack",
		meta = (EditCondition = "bEnabled", EditConditionHides))
	TArray<FEnemyWaveSpawnerUnitEntry> Units;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeted Attack",
		meta = (EditCondition = "bEnabled", EditConditionHides))
	FEnemyWaveSpawnerAttackSettings AttackSettings;
};

/** What happens once when the alert reaches one tier. */
USTRUCT(BlueprintType)
struct FConvoyPressureTierResponse
{
	GENERATED_BODY()

	// Shown in logs only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tier")
	FName TierName = NAME_None;

	// Wave spawners, convoy routes, strongpoints, air drops and dormant defence slots; set them to not start on
	// BeginPlay. Each placement actor is started once by the director, whatever list it is in.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tier")
	TArray<TObjectPtr<AEnemyPlacementActor>> PlacementActorsToStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tier")
	FConvoyPressureTargetedAttack TargetedAttack;
};

/** What an escaped convoy of one cargo type strengthens; applied for every escaped convoy of that type. */
USTRUCT(BlueprintType)
struct FConvoyPressureCargoResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	EConvoyCargoType CargoType = EConvoyCargoType::Fuel;

	// Started in list order, PlacementActorsPerEscapedConvoy per escaped convoy; set them to not start on BeginPlay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	TArray<TObjectPtr<AEnemyPlacementActor>> PlacementActorsToStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo", meta = (ClampMin = "0"))
	int32 PlacementActorsPerEscapedConvoy = 0;

	// Switched on in list order, DormantDefencesPerEscapedTruck for every truck that escaped.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo")
	TArray<TObjectPtr<ADormantDefenceSlot>> DormantDefences;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo", meta = (ClampMin = "0"))
	int32 DormantDefencesPerEscapedTruck = 0;

	// Sent through OnEscapedAmmoSeconds (x escaped trucks); the mission Blueprint gives it to the artillery battery.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cargo", meta = (ClampMin = "0.0", Units = "s"))
	float AmmoSupplySecondsPerEscapedTruck = 0.f;
};
