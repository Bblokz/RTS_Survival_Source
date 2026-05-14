#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/TrainingFocusTypes/EnemyAITrainingFocus.h"

#include "EnemyTrainingPressureTypes.generated.h"

/**
 * @brief Carries one reason for future training demand from strategic AI into the bucket state.
 */
USTRUCT(BlueprintType)
struct FEnemyStrategicTrainingPressureContribution
{
	GENERATED_BODY()

	// Chosen when the strategic reason needs a broad unit family instead of one concrete trainable option.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus FocusPressure = EAITrainingFocus::NoFocus;

	// Chosen when the strategic reason needs a role bias; NoTrainingPressure intentionally leaves specialty buckets unchanged.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty SpecialtyPressure = EAITrainingFocusSpecialty::NoTrainingPressure;

	// Larger values represent stronger strategic debt, so missing multi-unit requirements can outweigh generic action desire.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0"))
	float PressureAmount = 0.f;

	// Used for "any idle unit" requirements because they should not pretend one specific focus is the correct answer.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSpreadFocusPressureAcrossAllBuckets = false;

	// Stored for designers to understand why a bucket changed when reading debug output.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceDebugName = TEXT("Unknown Training Pressure Source");
};

/**
 * @brief Returned when bucket pressure is spent before later tech-aware unit selection chooses exact units.
 */
USTRUCT(BlueprintType)
struct FEnemyStrategicTrainingSelection
{
	GENERATED_BODY()

	// Kept separate from specialty so future unit selection can first narrow the trainable family.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus Focus = EAITrainingFocus::NoFocus;

	// Kept separate from focus so counters like AntiTank can bias units across several tech tiers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty Specialty = EAITrainingFocusSpecialty::NoTrainingPressure;
};

/**
 * @brief Mirrors recent pressure inputs so tuning sessions can explain why a bucket became dominant.
 */
USTRUCT(BlueprintType)
struct FEnemyStrategicTrainingPressureDebugEntry
{
	GENERATED_BODY()

	// Shows which focus bucket was affected, or NoFocus when the pressure was intentionally spread.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus FocusPressure = EAITrainingFocus::NoFocus;

	// Shows whether pressure was role-specific or intentionally left as no specialty pressure.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty SpecialtyPressure = EAITrainingFocusSpecialty::NoTrainingPressure;

	// Captures the applied value so designers can compare raw bucket causes against weighted selection output.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float PressureAmount = 0.f;

	// Explains why focus pressure may appear in every bucket from one requirement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSpreadFocusPressureAcrossAllBuckets = false;

	// Records the action or requirement that emitted the pressure, making debug strings actionable.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString SourceDebugName = TEXT("Unknown Training Pressure Source");
};
