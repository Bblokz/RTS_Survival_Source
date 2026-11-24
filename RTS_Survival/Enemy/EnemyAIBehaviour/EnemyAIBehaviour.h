#pragma once

#include "CoreMinimal.h"


#include "EnemyAIBehaviour.generated.h"

enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;

UENUM(Blueprintable)
enum class EEnemyStrategicAIBehaviour :uint8
{
	NotInitialized,
	PreferDefensive,
	PreferAggressive,
};

UENUM(BlueprintType)
enum class EEnemyStrategicAIThinkingStartCondition : uint8
{
	StartImmediatelyAtGameStart,
	StartAfterSpecifiedSeconds,
	StartAfterMissionTrigger,
};


USTRUCT(Blueprintable)
struct FEnemyAIMissionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategic AI|Startup")
	EEnemyStrategicAIThinkingStartCondition StrategicAIThinkingStartCondition =
		EEnemyStrategicAIThinkingStartCondition::StartImmediatelyAtGameStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategic AI|Startup",
		meta=(EditCondition="StrategicAIThinkingStartCondition == EEnemyStrategicAIThinkingStartCondition::StartAfterSpecifiedSeconds",
			EditConditionHides, ClampMin="0.0", UIMin="0.0"))
	float StrategicAIThinkingStartDelaySeconds = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EEnemyStrategicAIBehaviour StrategicAIBehaviour = EEnemyStrategicAIBehaviour::NotInitialized;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FVector> PointsReactDefensivelyTo;

	// This is used to evaluate whether the AI can use its decision tree to manually control units that are 'idle' on the blackboard
	// which makes them eligible for strategic AI commands that take direct control of units.
	// This can be turned on and off during runtime using the EnemyAIController @see AEnemyController::SetAllowDirectControlStochasticDecisionTree
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowDirectControlStochasticDecisionTree = false;

	// What is the minimal amount of units needed in the blackboard to start a strategic action?
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MinPickedUnitsForAction = 2;
	
	// Maximally how many units can be randomly picked from the blackboard to participate in a strategic action?
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxPickedUnitsForAction = 8;

	// Minimum amount of units the strategic AI tries to include in one training batch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	int32 MinUnitsTrainedPerBatch = 1;

	// Maximum amount of units the strategic AI tries to include in one training batch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
	int32 MaxUnitsTrainedPerBatch = 3;


	// Whether the AI can train units based on the buildings available to it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowUnitTraining = true;

	// Both lists empty: the AI trains anything its buildings unlock. Otherwise it only trains the squads and tanks
	// listed here (still only those its buildings unlock), so an early mission can keep the AI to Mosin squads at its
	// barracks. A list left empty while the other is filled means none of that kind.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategic AI|Training", meta=(EditCondition="bAllowUnitTraining"))
	TArray<ESquadSubtype> RestrictTrainingToSquads;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategic AI|Training", meta=(EditCondition="bAllowUnitTraining"))
	TArray<ETankSubtype> RestrictTrainingToTanks;

	bool GetHasTrainingRestriction() const
	{
		return not RestrictTrainingToSquads.IsEmpty() || not RestrictTrainingToTanks.IsEmpty();
	}

	// 0 = no cap. Otherwise training (and training point income) pauses while this many units trained at the AI's
	// buildings are alive. Units already queued still finish, so the count can overshoot by one batch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategic AI|Training",
		meta=(EditCondition="bAllowUnitTraining", ClampMin="0"))
	int32 MaxAliveBuildingTrainedUnits = 0;

	// Off: the AI builds nothing, which includes the trenches it digs around its bases.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAllowBuilding = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bAllowBuilding"))
	TArray<FVector> StartBuildLocations;

	// How many free T-34 Breaker harvesters each Soviet refinery keeps (trains and replaces); hand-placed harvesters
	// near a refinery count towards it. See Docs/EnemyAI/EnemyHarvestEconomy.md (H5).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strategic AI|Harvesting", meta=(ClampMin="0"))
	int32 MaxHarvestersPerRefinery = 2;
};

/**
 * What a mission sets at runtime with AEnemyController::ApplyMissionTraining (for example per difficulty from a data
 * asset): which units, how fast, and the cap. The listed units become trainable at any standing barracks (squads) or
 * vehicle building (tanks), whatever the map's tech levels say.
 */
USTRUCT(BlueprintType)
struct FEnemyMissionTrainingOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
	bool bAllowUnitTraining = true;

	// The only squads the AI trains; empty while Tanks is filled means no squads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
	TArray<ESquadSubtype> Squads;

	// The only tanks the AI trains; empty while Squads is filled means no tanks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
	TArray<ETankSubtype> Tanks;

	// 10 points = one BT-7; Mosin squad 3, PTRS squad 4, T-34/76 20, T-34/85 28.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training", meta = (ClampMin = "0.0"))
	float TrainingPointsPerMinute = 4.f;

	// The AI's points are set to this when the override is applied.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training", meta = (ClampMin = "0"))
	int32 TrainingPointsStart = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training", meta = (ClampMin = "1"))
	int32 MinUnitsTrainedPerBatch = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training", meta = (ClampMin = "1"))
	int32 MaxUnitsTrainedPerBatch = 1;

	// 0 = no cap; see FEnemyAIMissionSettings::MaxAliveBuildingTrainedUnits.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training", meta = (ClampMin = "0"))
	int32 MaxAliveBuildingTrainedUnits = 0;
};
