#pragma once

#include "CoreMinimal.h"

#include "EnemyAITechLevel.generated.h"

enum class EBuildingExpansionType : uint8;
enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;
struct FTrainingOption;

UENUM(BlueprintType)
enum EEnemyAITechLevel : uint8
{
	NotInitialized,
	// Has a barracks of some kind to train infantry.
	BasicInfantry,
	// Has a light tank factory or another type of tank factory to create tanks.
	LightTanks,
	// Has a medium tank factory.
	MediumTanks,
	// Has a tier2 building; unlocks variants of medium tanks as well as some more advanced infantry.
	Tier2,
	// Like the armory, unlocks more advanced types of infantry only.
	AdvancedInfantry,
	// Unlocks the most advanced vehicles and infantry.
	Tier3,
	// Super heavies and elite infantry.
	Experimental
};


// Holds the Tank and squad training options unlocked for a given tech level.
// Can be adjusted per map.
USTRUCT(BlueprintType)
struct FEnemyTrainingOptionsForTechLevel
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EEnemyAITechLevel TechLevel = EEnemyAITechLevel::NotInitialized;

	// What building expansions does the enemy AI need to have built to unlock this tech level?
	// If multiple are listed, then building either one of them will unlock the level.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TArray<EBuildingExpansionType> TypesUnlockingThisLevel;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ETankSubtype> TrainableTankSubtypes = {};
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<ESquadSubtype> TrainableSquadSubtypes = {};
	
};



// Holds the Tank and squad training options unlocked for a given tech level as well as the building types needed to unlock
// that tech level.
// Can be adjusted per map.
USTRUCT(BlueprintType)
struct FEnemyLevelTraining
{
	GENERATED_BODY()
	
	FEnemyLevelTraining();

	// With how many points does the AI start for training? Note that 10 = one light tank.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TrainingPointsStart = 10;
	// Scales how much the AI can train; 10 points is one light tank.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 TrainingPointsPerMinute = 3;

	TArray<EBuildingExpansionType> GetUniqueBuildingTypesForTechLevels() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyTrainingOptionsForTechLevel BasicInfantryOptions ;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyTrainingOptionsForTechLevel LightTankOptions;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyTrainingOptionsForTechLevel MediumTankOptions;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyTrainingOptionsForTechLevel Tier2Options;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyTrainingOptionsForTechLevel AdvancedInfantryOptions;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyTrainingOptionsForTechLevel Tier3Options;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnemyTrainingOptionsForTechLevel ExperimentalOptions;
	
};
