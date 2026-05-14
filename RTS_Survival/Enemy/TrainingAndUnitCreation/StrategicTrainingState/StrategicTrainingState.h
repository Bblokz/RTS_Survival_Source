#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyAITechLevel/EnemyAITechLevel.h"
#include "RTS_Survival/Enemy/TrainingAndUnitCreation/StrategicTrainingState/EnemyTrainingPressureTypes.h"

#include "StrategicTrainingState.generated.h"


enum EEnemyAITechLevel : uint8;

/**
 * @brief Stores strategic demand until future training logic spends it on available unit options.
 */
USTRUCT(BlueprintType)
struct FEnemyStrategicTrainingBuckets
{
	GENERATED_BODY()

	/**
	 * @brief Adds one strategic reason without forcing the AI to immediately train that unit family.
	 * @param PressureContribution Reason emitted by an action or missing-unit requirement.
	 */
	void AddPressureContribution(const FEnemyStrategicTrainingPressureContribution& PressureContribution);

	/**
	 * @brief Spends bucket debt before exact unit choice so later training code can stay tech-focused.
	 * @return The broad pressure result that should guide future unit-option selection.
	 */
	FEnemyStrategicTrainingSelection PickAndSpendTrainingSelection();

	FString GetDebugString() const;

	// Stores long-lived demand by broad unit family so later tech checks can choose exact trainable units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EAITrainingFocus, float> FocusBuckets;

	// Stores role demand separately because the same focus can satisfy different tactical reasons.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EAITrainingFocusSpecialty, float> SpecialtyBuckets;

	// Remembers the last selected focus so debugging and consecutive-pick penalties explain visible AI behavior.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus LastPickedFocus = EAITrainingFocus::NoFocus;

	// Remembers the last selected specialty so anti-repeat logic can be tuned independently from focus selection.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty LastPickedSpecialty = EAITrainingFocusSpecialty::NoTrainingPressure;

	// Tracks uninterrupted focus repeats, making it obvious when one bucket is overpowering all alternatives.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 ConsecutiveFocusPickCount = 0;

	// Tracks uninterrupted specialty repeats, helping diagnose role spam separately from unit-family spam.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 ConsecutiveSpecialtyPickCount = 0;

	// Keeps the last few focus picks so selection can avoid short alternating loops, not only exact repeats.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EAITrainingFocus> RecentPickedFocusHistory;

	// Keeps the last few specialty picks so counters like AntiTank cannot dominate every other training decision.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EAITrainingFocusSpecialty> RecentPickedSpecialtyHistory;

	// Stores recent pressure sources because designers need to know which strategic data pushed a bucket upward.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FEnemyStrategicTrainingPressureDebugEntry> RecentPressureDebugEntries;

private:
	void AddFocusPressure(const FEnemyStrategicTrainingPressureContribution& PressureContribution);
	void AddSpecialtyPressure(const FEnemyStrategicTrainingPressureContribution& PressureContribution);
	void AddRecentPressureDebugEntry(const FEnemyStrategicTrainingPressureContribution& PressureContribution);
	EAITrainingFocus PickFocusBucket() const;
	EAITrainingFocusSpecialty PickSpecialtyBucket() const;
	void SpendPickedFocus(const EAITrainingFocus PickedFocus);
	void SpendPickedSpecialty(const EAITrainingFocusSpecialty PickedSpecialty);
	float GetFocusBucketWeight(const EAITrainingFocus Focus, const float Pressure) const;
	float GetSpecialtyBucketWeight(const EAITrainingFocusSpecialty Specialty, const float Pressure) const;
	int32 GetRecentFocusPickCount(const EAITrainingFocus Focus) const;
	int32 GetRecentSpecialtyPickCount(const EAITrainingFocusSpecialty Specialty) const;
	void AddRecentPickedFocus(const EAITrainingFocus PickedFocus);
	void AddRecentPickedSpecialty(const EAITrainingFocusSpecialty PickedSpecialty);
};

/**
 * @brief Owned by strategic AI to keep tech, points, and pressure state together between think steps.
 */
USTRUCT()
struct FEnemyStrategicTrainingState
{
	GENERATED_BODY()

	/**
	 * @brief Collects pressure over time so training reacts to strategic debt rather than one action tick.
	 * @param PressureContribution Demand emitted by a strategic action or requirement.
	 */
	void AddTrainingPressureContribution(const FEnemyStrategicTrainingPressureContribution& PressureContribution);

	/**
	 * @brief Exposes bucket spending without coupling this state to concrete trainable unit data.
	 * @return Focus/specialty pair for future tech-aware training selection.
	 */
	FEnemyStrategicTrainingSelection PickAndSpendTrainingSelection();

	// The start value is set by the Enemy controller when propagating the settings to the strategic ai component.
	// see void AEnemyController::BeginPlay_MoveAISettingsToStrategicAIBlackboard() const
	// see void UEnemyStrategicAIComponent::GetTrainingPoints_ThinkStep()
	UPROPERTY()
	int32 TrainingPoints = 0;

	// Contains the result of TrainingRequirements_ThinkStep which checks if the correct building expansion is built
	// to unlock each tech level. 
	UPROPERTY()
	TMap<EEnemyAITechLevel, bool> TechLevelUnlockedMap = {};

	// Contains the units available per tech level.
	UPROPERTY()
	FEnemyLevelTraining EnemyLevelTraining = {};

	// Keeps all pressure state in one place so future exact-unit selection stays manageable.
	UPROPERTY()
	FEnemyStrategicTrainingBuckets TrainingBuckets = {};
	
};
