#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MissionScheduler.generated.h"

DECLARE_DYNAMIC_DELEGATE(FMissionScheduledCallback);

USTRUCT()
struct FMissionScheduledTask
{
	GENERATED_BODY()

	int32 M_TaskID = INDEX_NONE;

	int32 M_RemainingExecutions = 0;

	int32 M_IntervalTicks = 1;
	int32 M_TicksUntilNextExecution = 1;
	int32 M_InitialDelayTicks = 0;
	bool bM_FireBeforeFirstInterval = true;
	bool bM_IsPaused = false;
	bool bM_RepeatForever = false;

	FMissionScheduledCallback M_Callback;

	UPROPERTY()
	TWeakObjectPtr<UObject> M_WeakOwner;
};

/**
 * @brief Centralizes mission callbacks into one deterministic fixed-rate loop.
 * Mission code should schedule through AMissionManager so callback ownership and cleanup stay consistent.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UMissionScheduler : public UActorComponent
{
	GENERATED_BODY()

public:
	UMissionScheduler();

	/**
	 * @brief Queues a repeated callback so all mission callbacks share one tick loop.
	 * @param Callback Delegate to execute every interval.
	 * @param TotalCalls Number of executions before automatic removal.
	 * @param IntervalSeconds Fixed interval in scheduler ticks between calls.
	 * @param InitialDelaySeconds Initial delay in scheduler ticks before first non-immediate execution.
	 * @param CallbackOwner UObject whose lifetime controls task validity.
	 * @param bFireBeforeFirstInterval If true, one call is executed immediately on schedule.
	 * @param bRepeatForever If true, ignores TotalCalls and keeps executing until canceled.
	 * @return Created task id, or INDEX_NONE when validation fails or all calls were consumed instantly.
	 */
	int32 ScheduleCallback(
		const FMissionScheduledCallback& Callback,
		const int32 TotalCalls,
		const int32 IntervalSeconds,
		const int32 InitialDelaySeconds,
		UObject* CallbackOwner,
		const bool bFireBeforeFirstInterval = true,
		const bool bRepeatForever = false
	);
	int32 ScheduleSingleCallback(
		const FMissionScheduledCallback& Callback,
		const int32 DelaySeconds,
		UObject* CallbackOwner
	);

	void CancelTask(const int32 TaskID);
	void CancelAllTasksForObject(const UObject* CallbackOwner);
	void PauseTask(const int32 TaskID);
	void ResumeTask(const int32 TaskID);
	void PauseAllTasksForObject(const UObject* CallbackOwner);
	void ResumeAllTasksForObject(const UObject* CallbackOwner);
	void PauseAllTasks();
	void ResumeAllTasks();

	bool GetIsTaskActive(const int32 TaskID) const;
	int32 GetTaskCountForObject(const UObject* CallbackOwner) const;
	int32 GetTotalScheduledTaskCount() const;

protected:
	virtual void BeginPlay() override;

private:
	void TickScheduler();
	void TickScheduler_ProcessTask(FMissionScheduledTask& Task, TArray<int32>& TaskIdsToRemove);
	bool GetIsValidTask(const FMissionScheduledTask& Task) const;

private:
	// Task map keyed by generated id so callbacks can be canceled explicitly.
	UPROPERTY()
	TMap<int32, FMissionScheduledTask> M_Tasks;

	int32 M_NextTaskID = 1;

	FTimerHandle M_SchedulerTickHandle;

	static constexpr int32 M_TickIntervalSeconds = 1;
};
