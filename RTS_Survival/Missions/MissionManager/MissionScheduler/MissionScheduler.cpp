#include "MissionScheduler.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

UMissionScheduler::UMissionScheduler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMissionScheduler::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"World",
			"UMissionScheduler::BeginPlay",
			this
		);
		return;
	}

	World->GetTimerManager().SetTimer(
		M_SchedulerTickHandle,
		this,
		&UMissionScheduler::TickScheduler,
		M_TickIntervalSeconds,
		true
	);
}

int32 UMissionScheduler::ScheduleCallback(
	const FMissionScheduledCallback& Callback,
	const int32 TotalCalls,
	const int32 IntervalSeconds,
	const int32 InitialDelaySeconds,
	UObject* CallbackOwner,
	const bool bFireBeforeFirstInterval,
	const bool bRepeatForever
)
{
	if (not bRepeatForever && TotalCalls <= 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback total calls must be greater than zero.");
		return INDEX_NONE;
	}

	if (IntervalSeconds <= 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback interval must be greater than zero.");
		return INDEX_NONE;
	}

	if (InitialDelaySeconds < 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback initial delay cannot be negative.");
		return INDEX_NONE;
	}

	if (not Callback.IsBound())
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback is not bound.");
		return INDEX_NONE;
	}

	if (not IsValid(CallbackOwner))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback owner is not valid.");
		return INDEX_NONE;
	}

	int32 RemainingExecutions = bRepeatForever ? 1 : TotalCalls;
	if (bFireBeforeFirstInterval)
	{
		Callback.Execute();
		if (not bRepeatForever)
		{
			RemainingExecutions--;
		}
	}

	if (not bRepeatForever && RemainingExecutions <= 0)
	{
		return INDEX_NONE;
	}

	FMissionScheduledTask NewTask;
	NewTask.M_TaskID = M_NextTaskID++;
	NewTask.M_RemainingExecutions = RemainingExecutions;
	NewTask.M_IntervalTicks = IntervalSeconds;
	NewTask.M_InitialDelayTicks = InitialDelaySeconds;
	NewTask.M_TicksUntilNextExecution = InitialDelaySeconds > 0 ? InitialDelaySeconds : IntervalSeconds;
	NewTask.bM_FireBeforeFirstInterval = bFireBeforeFirstInterval;
	NewTask.bM_RepeatForever = bRepeatForever;
	NewTask.M_Callback = Callback;
	NewTask.M_WeakOwner = CallbackOwner;
	M_Tasks.Add(NewTask.M_TaskID, NewTask);

	return NewTask.M_TaskID;
}

int32 UMissionScheduler::ScheduleSingleCallback(
	const FMissionScheduledCallback& Callback,
	const int32 DelaySeconds,
	UObject* CallbackOwner
)
{
	constexpr int32 SingleCallCount = 1;
	constexpr int32 DefaultIntervalSeconds = 1;
	constexpr bool bFireBeforeFirstInterval = false;
	constexpr bool bRepeatForever = false;

	return ScheduleCallback(
		Callback,
		SingleCallCount,
		DefaultIntervalSeconds,
		DelaySeconds,
		CallbackOwner,
		bFireBeforeFirstInterval,
		bRepeatForever
	);
}

void UMissionScheduler::CancelTask(const int32 TaskID)
{
	if (not M_Tasks.Contains(TaskID))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler tried to cancel unknown task id.");
		return;
	}

	M_Tasks.Remove(TaskID);
}

void UMissionScheduler::CancelAllTasksForObject(const UObject* CallbackOwner)
{
	if (not IsValid(CallbackOwner))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler tried to cancel tasks for invalid callback owner.");
		return;
	}

	TArray<int32> TaskIdsToRemove;
	for (const auto& EachTaskPair : M_Tasks)
	{
		const FMissionScheduledTask& Task = EachTaskPair.Value;
		if (Task.M_WeakOwner.Get() != CallbackOwner)
		{
			continue;
		}

		TaskIdsToRemove.Add(EachTaskPair.Key);
	}

	for (const int32 TaskId : TaskIdsToRemove)
	{
		M_Tasks.Remove(TaskId);
	}
}

void UMissionScheduler::PauseTask(const int32 TaskID)
{
	if (not M_Tasks.Contains(TaskID))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler tried to pause unknown task id.");
		return;
	}

	M_Tasks[TaskID].bM_IsPaused = true;
}

void UMissionScheduler::ResumeTask(const int32 TaskID)
{
	if (not M_Tasks.Contains(TaskID))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler tried to resume unknown task id.");
		return;
	}

	M_Tasks[TaskID].bM_IsPaused = false;
}

void UMissionScheduler::PauseAllTasksForObject(const UObject* CallbackOwner)
{
	if (not IsValid(CallbackOwner))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler tried to pause tasks for invalid callback owner.");
		return;
	}

	for (auto& EachTaskPair : M_Tasks)
	{
		FMissionScheduledTask& Task = EachTaskPair.Value;
		if (Task.M_WeakOwner.Get() != CallbackOwner)
		{
			continue;
		}

		Task.bM_IsPaused = true;
	}
}

void UMissionScheduler::ResumeAllTasksForObject(const UObject* CallbackOwner)
{
	if (not IsValid(CallbackOwner))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler tried to resume tasks for invalid callback owner.");
		return;
	}

	for (auto& EachTaskPair : M_Tasks)
	{
		FMissionScheduledTask& Task = EachTaskPair.Value;
		if (Task.M_WeakOwner.Get() != CallbackOwner)
		{
			continue;
		}

		Task.bM_IsPaused = false;
	}
}

void UMissionScheduler::PauseAllTasks()
{
	for (auto& EachTaskPair : M_Tasks)
	{
		EachTaskPair.Value.bM_IsPaused = true;
	}
}

void UMissionScheduler::ResumeAllTasks()
{
	for (auto& EachTaskPair : M_Tasks)
	{
		EachTaskPair.Value.bM_IsPaused = false;
	}
}

bool UMissionScheduler::GetIsTaskActive(const int32 TaskID) const
{
	return M_Tasks.Contains(TaskID);
}

int32 UMissionScheduler::GetTaskCountForObject(const UObject* CallbackOwner) const
{
	if (not IsValid(CallbackOwner))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler tried to query task count for invalid callback owner.");
		return 0;
	}

	int32 TaskCount = 0;
	for (const auto& EachTaskPair : M_Tasks)
	{
		const FMissionScheduledTask& Task = EachTaskPair.Value;
		if (Task.M_WeakOwner.Get() != CallbackOwner)
		{
			continue;
		}

		TaskCount++;
	}

	return TaskCount;
}

int32 UMissionScheduler::GetTotalScheduledTaskCount() const
{
	return M_Tasks.Num();
}

void UMissionScheduler::TickScheduler()
{
	TArray<int32> TaskIdsToRemove;

	for (auto& EachTaskPair : M_Tasks)
	{
		FMissionScheduledTask& Task = EachTaskPair.Value;
		TickScheduler_ProcessTask(Task, TaskIdsToRemove);
	}

	for (const int32 TaskId : TaskIdsToRemove)
	{
		M_Tasks.Remove(TaskId);
	}
}

void UMissionScheduler::TickScheduler_ProcessTask(FMissionScheduledTask& Task, TArray<int32>& TaskIdsToRemove)
{
	if (not GetIsValidTask(Task))
	{
		TaskIdsToRemove.Add(Task.M_TaskID);
		return;
	}

	if (Task.bM_IsPaused)
	{
		return;
	}

	Task.M_TicksUntilNextExecution--;
	if (Task.M_TicksUntilNextExecution > 0)
	{
		return;
	}

	Task.M_Callback.Execute();
	if (not Task.bM_RepeatForever)
	{
		Task.M_RemainingExecutions--;
	}
	Task.M_TicksUntilNextExecution = Task.M_IntervalTicks;

	if (Task.bM_RepeatForever || Task.M_RemainingExecutions > 0)
	{
		return;
	}

	TaskIdsToRemove.Add(Task.M_TaskID);
}

bool UMissionScheduler::GetIsValidTask(const FMissionScheduledTask& Task) const
{
	if (not Task.M_WeakOwner.IsValid())
	{
		return false;
	}

	if (not Task.M_Callback.IsBound())
	{
		return false;
	}

	return Task.bM_RepeatForever || Task.M_RemainingExecutions > 0;
}
