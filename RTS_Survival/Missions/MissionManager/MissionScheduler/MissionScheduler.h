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
	bool bM_FireBeforeFirstInterval = true;

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
	 * @param CallbackOwner UObject whose lifetime controls task validity.
	 * @param bFireBeforeFirstInterval If true, one call is executed immediately on schedule.
	 * @return Created task id, or INDEX_NONE when validation fails or all calls were consumed instantly.
	 */
	int32 ScheduleCallback(
		const FMissionScheduledCallback& Callback,
		const int32 TotalCalls,
		const int32 IntervalSeconds,
		UObject* CallbackOwner,
		const bool bFireBeforeFirstInterval = true
	);

	void CancelTask(const int32 TaskID);
	void CancelAllTasksForObject(const UObject* CallbackOwner);

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
