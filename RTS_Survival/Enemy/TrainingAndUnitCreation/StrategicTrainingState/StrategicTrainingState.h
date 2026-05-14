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

	void AddPressureContribution(const FEnemyStrategicTrainingPressureContribution& PressureContribution);
	FEnemyStrategicTrainingSelection PickAndSpendTrainingSelection();
	FString GetDebugString() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EAITrainingFocus, float> FocusBuckets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EAITrainingFocusSpecialty, float> SpecialtyBuckets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocus LastPickedFocus = EAITrainingFocus::NoFocus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EAITrainingFocusSpecialty LastPickedSpecialty = EAITrainingFocusSpecialty::NoTrainingPressure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 ConsecutiveFocusPickCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	int32 ConsecutiveSpecialtyPickCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EAITrainingFocus> RecentPickedFocusHistory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<EAITrainingFocusSpecialty> RecentPickedSpecialtyHistory;

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

USTRUCT()
struct FEnemyStrategicTrainingState
{
	GENERATED_BODY()

	void AddTrainingPressureContribution(const FEnemyStrategicTrainingPressureContribution& PressureContribution);
	FEnemyStrategicTrainingSelection PickAndSpendTrainingSelection();

	// The start value is set by the Enemy controller when propagating the settings to the strategic ai component.
	UPROPERTY()
	int32 TrainingPoints = 0;
	
	UPROPERTY()
	TMap<EEnemyAITechLevel, bool> TechLevelUnlockedMap = {};
	
	UPROPERTY()
	FEnemyLevelTraining EnemyLevelTraining = {};

	UPROPERTY()
	FEnemyStrategicTrainingBuckets TrainingBuckets = {};
	
};
