#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/TrainingFocusTypes/EnemyAITrainingFocus.h"

#include "EnemyTrainingPressureTypes.generated.h"

USTRUCT(BlueprintType)
struct FEnemyStrategicTrainingPressureContribution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus FocusPressure = EAITrainingFocus::NoFocus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty SpecialtyPressure = EAITrainingFocusSpecialty::NoTrainingPressure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
	float PressureAmount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSpreadFocusPressureAcrossAllBuckets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceDebugName = TEXT("Unknown Training Pressure Source");
};

USTRUCT(BlueprintType)
struct FEnemyStrategicTrainingSelection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus Focus = EAITrainingFocus::NoFocus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty Specialty = EAITrainingFocusSpecialty::NoTrainingPressure;
};

USTRUCT(BlueprintType)
struct FEnemyStrategicTrainingPressureDebugEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus FocusPressure = EAITrainingFocus::NoFocus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty SpecialtyPressure = EAITrainingFocusSpecialty::NoTrainingPressure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PressureAmount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSpreadFocusPressureAcrossAllBuckets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceDebugName = TEXT("Unknown Training Pressure Source");
};
