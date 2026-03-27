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
	const TArray<AActor*>& RequiredActors,
	const bool bFireBeforeFirstInterval,
	const bool bRepeatForever
)
{
	if (not GetIsValidScheduleInput(
		Callback,
		TotalCalls,
		IntervalSeconds,
		InitialDelaySeconds,
		CallbackOwner,
		bRepeatForever))
	{
		return INDEX_NONE;
	}

	if (not GetAreRequiredActorsInputValid(RequiredActors))
	{
		return INDEX_NONE;
	}

	const TArray<TWeakObjectPtr<AActor>> RequiredActorWeakReferences = BuildRequiredActorWeakReferences(RequiredActors);

	int32 RemainingExecutions = bRepeatForever ? 1 : TotalCalls;
	if (bFireBeforeFirstInterval)
	{
		FMissionScheduledTask ImmediateExecutionTask;
		ImmediateExecutionTask.bM_RepeatForever = bRepeatForever;
		ImmediateExecutionTask.M_Callback = Callback;
		ImmediateExecutionTask.M_WeakOwner = CallbackOwner;
		ImmediateExecutionTask.M_RequiredActors = RequiredActorWeakReferences;

		if (not GetIsValidTask(ImmediateExecutionTask))
		{
			return INDEX_NONE;
		}

		if (not GetAreRequiredActorsValid(ImmediateExecutionTask))
		{
			return INDEX_NONE;
		}

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

	const int32 TicksUntilNextExecution = InitialDelaySeconds > 0 ? InitialDelaySeconds : IntervalSeconds;

	FMissionScheduledTask NewTask;
	NewTask.M_TaskID = M_NextTaskID++;
	NewTask.M_RemainingExecutions = RemainingExecutions;
	NewTask.M_IntervalTicks = IntervalSeconds;
	NewTask.M_TicksUntilNextExecution = TicksUntilNextExecution;
	NewTask.bM_RepeatForever = bRepeatForever;
	NewTask.M_Callback = Callback;
	NewTask.M_WeakOwner = CallbackOwner;
	NewTask.M_RequiredActors = RequiredActorWeakReferences;
	M_Tasks.Add(NewTask.M_TaskID, NewTask);

	return NewTask.M_TaskID;
}

int32 UMissionScheduler::ScheduleSingleCallback(
	const FMissionScheduledCallback& Callback,
	const int32 DelaySeconds,
	UObject* CallbackOwner,
	const TArray<AActor*>& RequiredActors
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
		RequiredActors,
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

	if (bM_IsTickingTasks)
	{
		TickScheduler_MarkTaskForRemoval(TaskID);
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

	for (const int32 TaskID : TaskIdsToRemove)
	{
		if (bM_IsTickingTasks)
		{
			TickScheduler_MarkTaskForRemoval(TaskID);
			continue;
		}

		M_Tasks.Remove(TaskID);
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
	return M_Tasks.Contains(TaskID) && not M_PendingTaskRemovals.Contains(TaskID);
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
		if (M_PendingTaskRemovals.Contains(EachTaskPair.Key))
		{
			continue;
		}

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
	return M_Tasks.Num() - M_PendingTaskRemovals.Num();
}

bool UMissionScheduler::GetIsValidScheduleInput(
	const FMissionScheduledCallback& Callback,
	const int32 TotalCalls,
	const int32 IntervalSeconds,
	const int32 InitialDelaySeconds,
	const UObject* CallbackOwner,
	const bool bRepeatForever
) const
{
	if (not bRepeatForever && TotalCalls <= 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback total calls must be greater than zero.");
		return false;
	}

	if (IntervalSeconds <= 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback interval must be greater than zero.");
		return false;
	}

	if (InitialDelaySeconds < 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback initial delay cannot be negative.");
		return false;
	}

	if (not Callback.IsBound())
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback is not bound.");
		return false;
	}

	if (not IsValid(CallbackOwner))
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback owner is not valid.");
		return false;
	}

	return true;
}

bool UMissionScheduler::GetAreRequiredActorsInputValid(const TArray<AActor*>& RequiredActors) const
{
	for (const AActor* RequiredActor : RequiredActors)
	{
		if (IsValid(RequiredActor))
		{
			continue;
		}

		RTSFunctionLibrary::ReportError("Mission scheduler received an invalid required actor while scheduling callback.");
		return false;
	}

	return true;
}

TArray<TWeakObjectPtr<AActor>> UMissionScheduler::BuildRequiredActorWeakReferences(
	const TArray<AActor*>& RequiredActors) const
{
	TArray<TWeakObjectPtr<AActor>> RequiredActorWeakReferences;
	RequiredActorWeakReferences.Reserve(RequiredActors.Num());

	for (AActor* RequiredActor : RequiredActors)
	{
		RequiredActorWeakReferences.Add(RequiredActor);
	}

	return RequiredActorWeakReferences;
}

void UMissionScheduler::TickScheduler()
{
	TArray<int32> TaskIDs;
	M_Tasks.GetKeys(TaskIDs);

	bM_IsTickingTasks = true;
	for (const int32 TaskID : TaskIDs)
	{
		TickScheduler_ProcessTaskByID(TaskID);
	}
	bM_IsTickingTasks = false;

	TickScheduler_FlushPendingTaskRemovals();
}

void UMissionScheduler::TickScheduler_ProcessTaskByID(const int32 TaskID)
{
	if (M_PendingTaskRemovals.Contains(TaskID))
	{
		return;
	}

	FMissionScheduledTask* Task = M_Tasks.Find(TaskID);
	if (not Task)
	{
		return;
	}

	if (not GetIsValidTask(*Task))
	{
		TickScheduler_MarkTaskForRemoval(TaskID);
		return;
	}

	if (Task->bM_IsPaused)
	{
		return;
	}

	Task->M_TicksUntilNextExecution--;
	if (Task->M_TicksUntilNextExecution > 0)
	{
		return;
	}

	TickScheduler_ExecuteTask(*Task);
}

void UMissionScheduler::TickScheduler_ExecuteTask(FMissionScheduledTask& Task)
{
	if (not GetAreRequiredActorsValid(Task))
	{
		TickScheduler_MarkTaskForRemoval(Task.M_TaskID);
		return;
	}

	Task.M_Callback.Execute();

	if (M_PendingTaskRemovals.Contains(Task.M_TaskID))
	{
		return;
	}

	if (not Task.bM_RepeatForever)
	{
		Task.M_RemainingExecutions--;
	}

	Task.M_TicksUntilNextExecution = Task.M_IntervalTicks;
	if (Task.bM_RepeatForever || Task.M_RemainingExecutions > 0)
	{
		return;
	}

	TickScheduler_MarkTaskForRemoval(Task.M_TaskID);
}

void UMissionScheduler::TickScheduler_MarkTaskForRemoval(const int32 TaskID)
{
	M_PendingTaskRemovals.Add(TaskID);
}

void UMissionScheduler::TickScheduler_FlushPendingTaskRemovals()
{
	for (const int32 TaskID : M_PendingTaskRemovals)
	{
		M_Tasks.Remove(TaskID);
	}

	M_PendingTaskRemovals.Empty();
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

bool UMissionScheduler::GetAreRequiredActorsValid(const FMissionScheduledTask& Task) const
{
	for (const TWeakObjectPtr<AActor>& RequiredActor : Task.M_RequiredActors)
	{
		if (RequiredActor.IsValid())
		{
			continue;
		}

		return false;
	}

	return true;
}
