#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "TrainMission.generated.h"

class ARoadSplineActor;
class ATrainUnit;

UENUM(BlueprintType)
enum class ETrainMissionTrainSource : uint8
{
	SpawnFromClass UMETA(DisplayName = "Spawn From Class"),
	ExistingTrain UMETA(DisplayName = "Existing Train"),
};

UENUM(BlueprintType)
enum class ETrainMissionCompletionCondition : uint8
{
	FirstTrainReached UMETA(DisplayName = "First Train Reached"),
	AllSurvivingTrainsReached UMETA(DisplayName = "All Surviving Trains Reached"),
};

UENUM(BlueprintType)
enum class ETrainMissionLogicEndReason : uint8
{
	CompletionConditionMet UMETA(DisplayName = "Completion Condition Met"),
	TrainDestroyed UMETA(DisplayName = "Train Destroyed"),
};

USTRUCT(BlueprintType)
struct FTrainMissionRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission|Route")
	TWeakObjectPtr<ARoadSplineActor> TrackSpline = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission|Route")
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission|Route")
	FVector EndLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FTrainMissionEntrySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission")
	FTrainMissionRoute Route;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission")
	ETrainMissionTrainSource TrainSource = ETrainMissionTrainSource::SpawnFromClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission",
		meta = (EditCondition = "TrainSource == ETrainMissionTrainSource::SpawnFromClass", EditConditionHides))
	TSubclassOf<ATrainUnit> TrainClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission",
		meta = (EditCondition = "TrainSource == ETrainMissionTrainSource::ExistingTrain", EditConditionHides))
	TWeakObjectPtr<ATrainUnit> ExistingTrain = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission|Departure",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TimeUntilDeparture = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission|Departure")
	bool bCallBeforeDepartureEvent = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Train Mission|Departure",
		meta = (EditCondition = "bCallBeforeDepartureEvent", EditConditionHides, ClampMin = "0.0", UIMin = "0.0"))
	float TimeBeforeDeparture = 0.0f;
};

enum class ETrainMissionEntryRuntimeStatus : uint8
{
	WaitingForBeforeDeparture,
	WaitingForDeparture,
	Moving,
	Reached,
	Destroyed,
	Skipped,
	Cancelled,
};

struct FTrainMissionEntryRuntimeState
{
	TWeakObjectPtr<ATrainUnit> M_TrainActor = nullptr;
	FTimerHandle M_DepartureTimerHandle;
	FDelegateHandle M_ReachedTargetDelegateHandle;
	FDelegateHandle M_UnitDiedDelegateHandle;
	FName M_MiniMapIconId = NAME_None;
	ETrainMissionEntryRuntimeStatus M_Status = ETrainMissionEntryRuntimeStatus::Skipped;
};

struct FTrainMissionLogicRuntimeState
{
	TArray<FTrainMissionEntryRuntimeState> M_TrainEntries;
	ETrainMissionCompletionCondition M_CompletionCondition = ETrainMissionCompletionCondition::FirstTrainReached;
	uint32 M_Generation = 0;
	bool bM_StopIfOneTrainDestroyed = false;
	bool bM_StopOtherTrainsIfOneSucceeded = false;
	bool bM_IsActive = false;
	bool bM_HasBroadcastLogicEnded = false;
};

/**
 * @brief Designers use this mission to schedule spline-driven trains and react to their outcomes in Blueprint.
 * Native logic owns departure timing, train callbacks, and minimap tracking without deciding mission victory.
 * @note SetupTrainLogic: call from BP_OnMissionStart to start the configured train sequence.
 */
UCLASS(Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UTrainMission : public UMissionBase
{
	GENERATED_BODY()

public:
	/**
	 * @brief Starts one isolated train sequence so Blueprint can decide how its result affects the mission.
	 * @param TrainEntries Zero-based train settings containing source, route, and departure timing.
	 * @param CompletionCondition Determines when BP_OnTrainLogicEnded reports a successful sequence.
	 * @param bStopIfOneTrainDestroyed Cancels and cleans all train logic after a moving train is destroyed.
	 * @param bStopOtherTrainsIfOneSucceeded Cancels only departures that have not started after any train arrives.
	 */
	UFUNCTION(BlueprintCallable, Category = "Train Mission")
	void SetupTrainLogic(
		const TArray<FTrainMissionEntrySettings>& TrainEntries,
		ETrainMissionCompletionCondition CompletionCondition,
		bool bStopIfOneTrainDestroyed,
		bool bStopOtherTrainsIfOneSucceeded);

	virtual void OnMissionComplete() override;
	virtual void OnMissionFailed() override;
	virtual void OnCleanUpMission() override;
	virtual void BeginDestroy() override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Train Mission")
	void BP_OnBeforeDeparture(int32 TrainNumber, ATrainUnit* TrainActor, float TimeUntilDeparture);

	UFUNCTION(BlueprintImplementableEvent, Category = "Train Mission")
	void BP_OnTrainReached(int32 TrainNumber, ATrainUnit* TrainActor, FVector EndLocation);

	UFUNCTION(BlueprintImplementableEvent, Category = "Train Mission")
	void BP_OnTrainDestroyed(int32 TrainNumber, ATrainUnit* TrainActor, FVector DestructionLocation);

	/**
	 * @brief Reports that the configured success condition was met or destruction explicitly stopped the sequence.
	 * @param EndReason Runtime reason the train logic ended.
	 * @param CompletionCondition Success rule selected when SetupTrainLogic was called.
	 * @param TrainNumber Zero-based entry that caused this result.
	 * @param TrainActor Train that reached its destination or was destroyed.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Train Mission")
	void BP_OnTrainLogicEnded(
		ETrainMissionLogicEndReason EndReason,
		ETrainMissionCompletionCondition CompletionCondition,
		int32 TrainNumber,
		ATrainUnit* TrainActor);

private:
	void SetupEntryDepartureTimer(int32 TrainNumber);
	bool GetIsEntrySourceConfigured(const FTrainMissionEntrySettings& EntrySettings) const;

	/** Schedules a float-precision callback while retaining its handle for mission cleanup. */
	void SetEntryTimer(int32 TrainNumber, float DelaySeconds, const FTimerDelegate& TimerDelegate);

	void HandleBeforeDepartureTimerElapsed(int32 TrainNumber, uint32 TrainLogicGeneration);
	void HandleDepartureTimerElapsed(int32 TrainNumber, uint32 TrainLogicGeneration);
	ATrainUnit* ResolveTrainForDeparture(const FTrainMissionEntrySettings& EntrySettings);
	ATrainUnit* SpawnTrainForEntry(const FTrainMissionEntrySettings& EntrySettings) const;
	bool StartTrainMovement(int32 TrainNumber, ATrainUnit* TrainActor, const FVector& EndLocation);
	void RegisterTrainCallbacks(int32 TrainNumber, ATrainUnit* TrainActor);
	void AddTrainMiniMapIcon(int32 TrainNumber, ATrainUnit* TrainActor);
	void HandleTrainReached(const FVector& ReachedLocation, int32 TrainNumber, uint32 TrainLogicGeneration);
	void HandleTrainUnitDied(int32 TrainNumber, uint32 TrainLogicGeneration);

	UFUNCTION()
	void HandleTrainActorDestroyed(AActor* DestroyedActor);

	void ProcessTrainDestroyed(int32 TrainNumber, ATrainUnit* TrainActor, uint32 TrainLogicGeneration);
	void CancelPendingTrainDepartures();
	bool GetHasMetCompletionCondition() const;
	void TryBroadcastSuccessfulTrainLogicEnd(int32 TrainNumber, ATrainUnit* TrainActor);
	bool GetIsCurrentTrainLogicCallback(int32 TrainNumber, uint32 TrainLogicGeneration) const;
	void MarkEntrySkipped(int32 TrainNumber);
	void CleanupEntryCallbacks(FTrainMissionEntryRuntimeState& RuntimeState);
	void RemoveEntryMiniMapIcon(FTrainMissionEntryRuntimeState& RuntimeState);
	void CleanupTrainLogic();

	UPROPERTY()
	TArray<FTrainMissionEntrySettings> M_TrainEntries;

	FTrainMissionLogicRuntimeState M_TrainLogicRuntimeState;
};
