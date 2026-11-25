// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RTS_Survival/Enemy/EnemyAIBehaviour/EnemyAIBehaviour.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"

#include "BerlinReichstagTrainingData.generated.h"

/** What the Reichstag base does on one difficulty: what it trains, how fast, the cap, and where the units go. */
USTRUCT(BlueprintType)
struct FBerlinReichstagTrainingLevel
{
	GENERATED_BODY()

	// Units, training points per minute, start points and the cap of living base-trained units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
	FEnemyMissionTrainingOverride Training;

	// Trained units that leave together as one patrol round the Reichstag (BerlinReichstagPatrolRoute actors).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (ClampMin = "1"))
	int32 PatrolGroupSize = 2;

	// Most Reichstag patrols out at once; the rest of the trained units guard the base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (ClampMin = "0"))
	int32 MaxReichstagPatrols = 1;

	// After the alarm: trained units that leave together to attack the player's base.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (ClampMin = "1"))
	int32 AttackGroupSize = 3;

	// After the alarm: share of the new groups sent at the player's base (the rest keep the patrols full; once those
	// are full every group attacks). 0.5 = every second group.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackShareAfterAlarm = 0.5f;
};

/**
 * @brief The Reichstag Director of Shield base's training in the Berlin mission, one asset for all difficulties
 * (BerlinMissionScripting.md section 10). Mission_Berlin_Arrival hands it to UBerlinReichstagTrainingSubsystem.
 */
UCLASS(BlueprintType)
class RTS_SURVIVAL_API UBerlinReichstagTrainingData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UBerlinReichstagTrainingData();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", DisplayName = "New To RTS")
	FBerlinReichstagTrainingLevel NewToRTS;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FBerlinReichstagTrainingLevel Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	FBerlinReichstagTrainingLevel Hard;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty", DisplayName = "Brutal (+ Ironman)")
	FBerlinReichstagTrainingLevel Brutal;

	// How a Reichstag patrol guards each point of its route and helps its units in combat.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routing")
	FRandomPatrolWithAttackMoveSettings PatrolSettings;

	// How an attack group on the player's base helps its units in combat on the way.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routing")
	FAttackMoveWaveSettings AttackSettings;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Routing", meta = (ClampMin = "1"))
	int32 MaxFormationWidth = 2;

	/** @return The level of a difficulty; Ironman uses Brutal. */
	UFUNCTION(BlueprintPure, Category = "Berlin|Reichstag")
	FBerlinReichstagTrainingLevel GetLevelForDifficulty(const ERTSGameDifficulty Difficulty) const;
};
