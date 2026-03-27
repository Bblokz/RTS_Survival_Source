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
	UObject* CallbackOwner,
	const bool bFireBeforeFirstInterval
)
{
	if (TotalCalls <= 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback total calls must be greater than zero.");
		return INDEX_NONE;
	}

	if (IntervalSeconds <= 0)
	{
		RTSFunctionLibrary::ReportError("Mission scheduler callback interval must be greater than zero.");
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

	int32 RemainingExecutions = TotalCalls;
	if (bFireBeforeFirstInterval)
	{
		Callback.Execute();
		RemainingExecutions--;
	}

	if (RemainingExecutions <= 0)
	{
		return INDEX_NONE;
	}

	FMissionScheduledTask NewTask;
	NewTask.M_TaskID = M_NextTaskID++;
	NewTask.M_RemainingExecutions = RemainingExecutions;
	NewTask.M_IntervalTicks = IntervalSeconds;
	NewTask.M_TicksUntilNextExecution = IntervalSeconds;
	NewTask.bM_FireBeforeFirstInterval = bFireBeforeFirstInterval;
	NewTask.M_Callback = Callback;
	NewTask.M_WeakOwner = CallbackOwner;
	M_Tasks.Add(NewTask.M_TaskID, NewTask);

	return NewTask.M_TaskID;
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

	Task.M_TicksUntilNextExecution--;
	if (Task.M_TicksUntilNextExecution > 0)
	{
		return;
	}

	Task.M_Callback.Execute();
	Task.M_RemainingExecutions--;
	Task.M_TicksUntilNextExecution = Task.M_IntervalTicks;

	if (Task.M_RemainingExecutions > 0)
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

	return Task.M_RemainingExecutions > 0;
}
