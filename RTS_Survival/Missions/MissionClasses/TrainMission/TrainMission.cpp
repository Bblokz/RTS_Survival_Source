#include "TrainMission.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Environment/Splines/RoadSplineActor.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Tanks/TrainUnit/TrainUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"

void UTrainMission::SetupTrainLogic(
	const TArray<FTrainMissionEntrySettings>& TrainEntries,
	const ETrainMissionCompletionCondition CompletionCondition,
	const bool bStopIfOneTrainDestroyed,
	const bool bStopOtherTrainsIfOneSucceeded)
{
	CleanupTrainLogic();
	if (TrainEntries.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Train mission cannot set up train logic without any entries."
			"\n See function: UTrainMission::SetupTrainLogic");
		return;
	}

	M_TrainEntries = TrainEntries;
	M_TrainLogicRuntimeState.M_CompletionCondition = CompletionCondition;
	M_TrainLogicRuntimeState.bM_StopIfOneTrainDestroyed = bStopIfOneTrainDestroyed;
	M_TrainLogicRuntimeState.bM_StopOtherTrainsIfOneSucceeded = bStopOtherTrainsIfOneSucceeded;
	M_TrainLogicRuntimeState.bM_IsActive = true;
	M_TrainLogicRuntimeState.M_TrainEntries.SetNum(M_TrainEntries.Num());

	for (int32 TrainNumber = 0; TrainNumber < M_TrainEntries.Num(); ++TrainNumber)
	{
		SetupEntryDepartureTimer(TrainNumber);
	}
}

void UTrainMission::OnMissionComplete()
{
	CleanupTrainLogic();
	Super::OnMissionComplete();
}

void UTrainMission::OnMissionFailed()
{
	CleanupTrainLogic();
	Super::OnMissionFailed();
}

void UTrainMission::OnCleanUpMission()
{
	CleanupTrainLogic();
	Super::OnCleanUpMission();
}

void UTrainMission::BeginDestroy()
{
	CleanupTrainLogic();
	Super::BeginDestroy();
}

void UTrainMission::SetupEntryDepartureTimer(const int32 TrainNumber)
{
	if (not M_TrainEntries.IsValidIndex(TrainNumber)
		|| not M_TrainLogicRuntimeState.M_TrainEntries.IsValidIndex(TrainNumber))
	{
		return;
	}

	const FTrainMissionEntrySettings& EntrySettings = M_TrainEntries[TrainNumber];
	if (not GetIsEntrySourceConfigured(EntrySettings))
	{
		MarkEntrySkipped(TrainNumber);
		return;
	}

	const float DepartureDelay = FMath::Max(0.0f, EntrySettings.TimeUntilDeparture);
	const uint32 TrainLogicGeneration = M_TrainLogicRuntimeState.M_Generation;
	if (EntrySettings.bCallBeforeDepartureEvent)
	{
		const float TimeBeforeDeparture = FMath::Clamp(
			EntrySettings.TimeBeforeDeparture,
			0.0f,
			DepartureDelay);
		const float BeforeDepartureDelay = DepartureDelay - TimeBeforeDeparture;
		M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber].M_Status =
			ETrainMissionEntryRuntimeStatus::WaitingForBeforeDeparture;
		SetEntryTimer(
			TrainNumber,
			BeforeDepartureDelay,
			FTimerDelegate::CreateUObject(
				this,
				&UTrainMission::HandleBeforeDepartureTimerElapsed,
				TrainNumber,
				TrainLogicGeneration));
		return;
	}

	M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber].M_Status =
		ETrainMissionEntryRuntimeStatus::WaitingForDeparture;
	SetEntryTimer(
		TrainNumber,
		DepartureDelay,
		FTimerDelegate::CreateUObject(
			this,
			&UTrainMission::HandleDepartureTimerElapsed,
			TrainNumber,
			TrainLogicGeneration));
}

bool UTrainMission::GetIsEntrySourceConfigured(const FTrainMissionEntrySettings& EntrySettings) const
{
	ARoadSplineActor* const TrackSpline = EntrySettings.Route.TrackSpline.Get();
	if (not IsValid(TrackSpline))
	{
		RTSFunctionLibrary::ReportError(
			"Train mission entry has no valid track spline."
			"\n See function: UTrainMission::GetIsEntrySourceConfigured");
		return false;
	}

	if (EntrySettings.TrainSource == ETrainMissionTrainSource::SpawnFromClass)
	{
		if (EntrySettings.TrainClass != nullptr)
		{
			return true;
		}

		RTSFunctionLibrary::ReportError(
			"Train mission entry is set to spawn from class, but TrainClass is null."
			"\n See function: UTrainMission::GetIsEntrySourceConfigured");
		return false;
	}

	return RTSFunctionLibrary::RTSIsValid(EntrySettings.ExistingTrain.Get());
}

void UTrainMission::SetEntryTimer(
	const int32 TrainNumber,
	const float DelaySeconds,
	const FTimerDelegate& TimerDelegate)
{
	if (not M_TrainLogicRuntimeState.M_TrainEntries.IsValidIndex(TrainNumber))
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(
			"Train mission could not schedule a departure callback because its world is invalid."
			"\n See function: UTrainMission::SetEntryTimer");
		return;
	}

	FTimerHandle& TimerHandle = M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber].M_DepartureTimerHandle;
	World->GetTimerManager().ClearTimer(TimerHandle);
	if (DelaySeconds <= 0.0f)
	{
		TimerHandle = World->GetTimerManager().SetTimerForNextTick(TimerDelegate);
		return;
	}

	World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, DelaySeconds, false);
}

void UTrainMission::HandleBeforeDepartureTimerElapsed(
	const int32 TrainNumber,
	const uint32 TrainLogicGeneration)
{
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	FTrainMissionEntryRuntimeState& RuntimeState = M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber];
	if (RuntimeState.M_Status != ETrainMissionEntryRuntimeStatus::WaitingForBeforeDeparture)
	{
		return;
	}
	RuntimeState.M_DepartureTimerHandle.Invalidate();

	const FTrainMissionEntrySettings EntrySettings = M_TrainEntries[TrainNumber];
	ATrainUnit* TrainActor = nullptr;
	if (EntrySettings.TrainSource == ETrainMissionTrainSource::ExistingTrain)
	{
		TrainActor = EntrySettings.ExistingTrain.Get();
		if (not RTSFunctionLibrary::RTSIsValid(TrainActor))
		{
			MarkEntrySkipped(TrainNumber);
			return;
		}
	}

	const float TimeUntilDeparture = FMath::Clamp(
		EntrySettings.TimeBeforeDeparture,
		0.0f,
		FMath::Max(0.0f, EntrySettings.TimeUntilDeparture));
	BP_OnBeforeDeparture(TrainNumber, TrainActor, TimeUntilDeparture);
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	if (EntrySettings.TrainSource == ETrainMissionTrainSource::ExistingTrain
		&& not RTSFunctionLibrary::RTSIsValid(TrainActor))
	{
		MarkEntrySkipped(TrainNumber);
		return;
	}

	RuntimeState.M_Status = ETrainMissionEntryRuntimeStatus::WaitingForDeparture;
	SetEntryTimer(
		TrainNumber,
		TimeUntilDeparture,
		FTimerDelegate::CreateUObject(
			this,
			&UTrainMission::HandleDepartureTimerElapsed,
			TrainNumber,
			TrainLogicGeneration));
}

void UTrainMission::HandleDepartureTimerElapsed(
	const int32 TrainNumber,
	const uint32 TrainLogicGeneration)
{
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	FTrainMissionEntryRuntimeState& RuntimeState = M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber];
	if (RuntimeState.M_Status != ETrainMissionEntryRuntimeStatus::WaitingForDeparture)
	{
		return;
	}
	RuntimeState.M_DepartureTimerHandle.Invalidate();

	const FTrainMissionEntrySettings EntrySettings = M_TrainEntries[TrainNumber];
	ATrainUnit* const TrainActor = ResolveTrainForDeparture(EntrySettings);
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	if (not RTSFunctionLibrary::RTSIsValid(TrainActor))
	{
		MarkEntrySkipped(TrainNumber);
		return;
	}

	RuntimeState.M_TrainActor = TrainActor;
	RuntimeState.M_Status = ETrainMissionEntryRuntimeStatus::Moving;
	RegisterTrainCallbacks(TrainNumber, TrainActor);
	AddTrainMiniMapIcon(TrainNumber, TrainActor);
	if (StartTrainMovement(TrainNumber, TrainActor, EntrySettings.Route.EndLocation))
	{
		return;
	}

	if (GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration)
		&& RuntimeState.M_Status == ETrainMissionEntryRuntimeStatus::Moving)
	{
		CleanupEntryCallbacks(RuntimeState);
		RemoveEntryMiniMapIcon(RuntimeState);
		MarkEntrySkipped(TrainNumber);
	}
}

ATrainUnit* UTrainMission::ResolveTrainForDeparture(const FTrainMissionEntrySettings& EntrySettings)
{
	ARoadSplineActor* const TrackSpline = EntrySettings.Route.TrackSpline.Get();
	if (not IsValid(TrackSpline))
	{
		return nullptr;
	}

	if (EntrySettings.TrainSource == ETrainMissionTrainSource::SpawnFromClass)
	{
		return SpawnTrainForEntry(EntrySettings);
	}

	ATrainUnit* const ExistingTrain = EntrySettings.ExistingTrain.Get();
	if (not RTSFunctionLibrary::RTSIsValid(ExistingTrain))
	{
		return nullptr;
	}

	return ExistingTrain->SetAssignedRoadSpline(TrackSpline) ? ExistingTrain : nullptr;
}

ATrainUnit* UTrainMission::SpawnTrainForEntry(const FTrainMissionEntrySettings& EntrySettings) const
{
	UWorld* const World = GetWorld();
	ARoadSplineActor* const TrackSpline = EntrySettings.Route.TrackSpline.Get();
	if (not IsValid(World) || not IsValid(TrackSpline) || EntrySettings.TrainClass == nullptr)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, EntrySettings.Route.StartLocation);
	ATrainUnit* const SpawnedTrain = World->SpawnActorDeferred<ATrainUnit>(
		EntrySettings.TrainClass,
		SpawnTransform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(SpawnedTrain))
	{
		RTSFunctionLibrary::ReportError(
			"Train mission failed to spawn its configured TrainClass."
			"\n See function: UTrainMission::SpawnTrainForEntry");
		return nullptr;
	}

	if (not SpawnedTrain->SetAssignedRoadSpline(TrackSpline))
	{
		SpawnedTrain->Destroy();
		return nullptr;
	}

	UGameplayStatics::FinishSpawningActor(SpawnedTrain, SpawnTransform);
	return SpawnedTrain;
}

bool UTrainMission::StartTrainMovement(
	const int32 TrainNumber,
	ATrainUnit* TrainActor,
	const FVector& EndLocation)
{
	const ECommandQueueError MoveError = TrainActor->MoveToLocation(
		EndLocation,
		true,
		FRotator::ZeroRotator);
	if (MoveError == ECommandQueueError::NoError)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"Train mission failed to order train " + FString::FromInt(TrainNumber)
		+ " to its end location. Command error: " + UEnum::GetValueAsString(MoveError)
		+ "\n See function: UTrainMission::StartTrainMovement");
	return false;
}

void UTrainMission::RegisterTrainCallbacks(const int32 TrainNumber, ATrainUnit* TrainActor)
{
	if (not IsValid(TrainActor) || not M_TrainLogicRuntimeState.M_TrainEntries.IsValidIndex(TrainNumber))
	{
		return;
	}

	FTrainMissionEntryRuntimeState& RuntimeState = M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber];
	const uint32 TrainLogicGeneration = M_TrainLogicRuntimeState.M_Generation;
	RuntimeState.M_ReachedTargetDelegateHandle = TrainActor->OnReachedSplineTarget.AddUObject(
		this,
		&UTrainMission::HandleTrainReached,
		TrainNumber,
		TrainLogicGeneration);
	RuntimeState.M_UnitDiedDelegateHandle = TrainActor->OnUnitDies.AddUObject(
		this,
		&UTrainMission::HandleTrainUnitDied,
		TrainNumber,
		TrainLogicGeneration);
	TrainActor->OnDestroyed.AddUniqueDynamic(this, &UTrainMission::HandleTrainActorDestroyed);
}

void UTrainMission::AddTrainMiniMapIcon(const int32 TrainNumber, ATrainUnit* TrainActor)
{
	if (not IsValid(TrainActor) || not M_TrainLogicRuntimeState.M_TrainEntries.IsValidIndex(TrainNumber))
	{
		return;
	}

	const FName RequestedIconId(*FString::Printf(
		TEXT("TrainMission_%u_%d_%u"),
		GetUniqueID(),
		TrainNumber,
		M_TrainLogicRuntimeState.M_Generation));
	FTrainMissionEntryRuntimeState& RuntimeState = M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber];
	RuntimeState.M_MiniMapIconId = AddCustomMiniMapIconAttachedToActor(
		RequestedIconId,
		EMinimapIconType::Train,
		TrainActor->GetActorLocation(),
		TrainActor,
		FRotator::ZeroRotator,
		false,
		FMinimapIconTextPayload());
}

void UTrainMission::HandleTrainReached(
	const FVector& ReachedLocation,
	const int32 TrainNumber,
	const uint32 TrainLogicGeneration)
{
	(void)ReachedLocation;
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	FTrainMissionEntryRuntimeState& RuntimeState = M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber];
	if (RuntimeState.M_Status != ETrainMissionEntryRuntimeStatus::Moving)
	{
		return;
	}

	ATrainUnit* const TrainActor = RuntimeState.M_TrainActor.Get();
	const FVector EndLocation = M_TrainEntries[TrainNumber].Route.EndLocation;
	RuntimeState.M_Status = ETrainMissionEntryRuntimeStatus::Reached;
	CleanupEntryCallbacks(RuntimeState);
	if (M_TrainLogicRuntimeState.bM_StopOtherTrainsIfOneSucceeded)
	{
		CancelPendingTrainDepartures();
	}

	BP_OnTrainReached(TrainNumber, TrainActor, EndLocation);
	if (GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		TryBroadcastSuccessfulTrainLogicEnd(TrainNumber, TrainActor);
	}
}

void UTrainMission::HandleTrainUnitDied(
	const int32 TrainNumber,
	const uint32 TrainLogicGeneration)
{
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	ProcessTrainDestroyed(
		TrainNumber,
		M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber].M_TrainActor.Get(),
		TrainLogicGeneration);
}

void UTrainMission::HandleTrainActorDestroyed(AActor* DestroyedActor)
{
	ATrainUnit* const DestroyedTrain = Cast<ATrainUnit>(DestroyedActor);
	if (DestroyedTrain == nullptr || not M_TrainLogicRuntimeState.bM_IsActive)
	{
		return;
	}

	for (int32 TrainNumber = 0; TrainNumber < M_TrainLogicRuntimeState.M_TrainEntries.Num(); ++TrainNumber)
	{
		const FTrainMissionEntryRuntimeState& RuntimeState =
			M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber];
		if (RuntimeState.M_Status != ETrainMissionEntryRuntimeStatus::Moving
			|| RuntimeState.M_TrainActor.Get() != DestroyedTrain)
		{
			continue;
		}

		ProcessTrainDestroyed(
			TrainNumber,
			DestroyedTrain,
			M_TrainLogicRuntimeState.M_Generation);
		return;
	}
}

void UTrainMission::ProcessTrainDestroyed(
	const int32 TrainNumber,
	ATrainUnit* TrainActor,
	const uint32 TrainLogicGeneration)
{
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	FTrainMissionEntryRuntimeState& RuntimeState = M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber];
	if (RuntimeState.M_Status != ETrainMissionEntryRuntimeStatus::Moving)
	{
		return;
	}

	const FVector DestructionLocation = TrainActor != nullptr
		? TrainActor->GetActorLocation()
		: FVector::ZeroVector;
	RuntimeState.M_Status = ETrainMissionEntryRuntimeStatus::Destroyed;
	CleanupEntryCallbacks(RuntimeState);
	RemoveEntryMiniMapIcon(RuntimeState);
	BP_OnTrainDestroyed(TrainNumber, TrainActor, DestructionLocation);
	if (not GetIsCurrentTrainLogicCallback(TrainNumber, TrainLogicGeneration))
	{
		return;
	}

	if (not M_TrainLogicRuntimeState.bM_StopIfOneTrainDestroyed)
	{
		TryBroadcastSuccessfulTrainLogicEnd(TrainNumber, TrainActor);
		return;
	}

	const ETrainMissionCompletionCondition CompletionCondition =
		M_TrainLogicRuntimeState.M_CompletionCondition;
	const bool bShouldBroadcastLogicEnded = not M_TrainLogicRuntimeState.bM_HasBroadcastLogicEnded;
	CleanupTrainLogic();
	if (bShouldBroadcastLogicEnded)
	{
		BP_OnTrainLogicEnded(
			ETrainMissionLogicEndReason::TrainDestroyed,
			CompletionCondition,
			TrainNumber,
			TrainActor);
	}
}

void UTrainMission::CancelPendingTrainDepartures()
{
	UWorld* const World = GetWorld();
	for (FTrainMissionEntryRuntimeState& RuntimeState : M_TrainLogicRuntimeState.M_TrainEntries)
	{
		const bool bIsWaiting =
			RuntimeState.M_Status == ETrainMissionEntryRuntimeStatus::WaitingForBeforeDeparture
			|| RuntimeState.M_Status == ETrainMissionEntryRuntimeStatus::WaitingForDeparture;
		if (not bIsWaiting)
		{
			continue;
		}

		if (IsValid(World))
		{
			World->GetTimerManager().ClearTimer(RuntimeState.M_DepartureTimerHandle);
		}
		RuntimeState.M_Status = ETrainMissionEntryRuntimeStatus::Cancelled;
	}
}

bool UTrainMission::GetHasMetCompletionCondition() const
{
	bool bAnyTrainReached = false;
	for (const FTrainMissionEntryRuntimeState& RuntimeState : M_TrainLogicRuntimeState.M_TrainEntries)
	{
		if (RuntimeState.M_Status == ETrainMissionEntryRuntimeStatus::Reached)
		{
			bAnyTrainReached = true;
			continue;
		}

		const bool bIsUnresolved =
			RuntimeState.M_Status == ETrainMissionEntryRuntimeStatus::WaitingForBeforeDeparture
			|| RuntimeState.M_Status == ETrainMissionEntryRuntimeStatus::WaitingForDeparture
			|| RuntimeState.M_Status == ETrainMissionEntryRuntimeStatus::Moving;
		if (bIsUnresolved
			&& M_TrainLogicRuntimeState.M_CompletionCondition
			== ETrainMissionCompletionCondition::AllSurvivingTrainsReached)
		{
			return false;
		}
	}

	return bAnyTrainReached;
}

void UTrainMission::TryBroadcastSuccessfulTrainLogicEnd(const int32 TrainNumber, ATrainUnit* TrainActor)
{
	if (M_TrainLogicRuntimeState.bM_HasBroadcastLogicEnded || not GetHasMetCompletionCondition())
	{
		return;
	}

	M_TrainLogicRuntimeState.bM_HasBroadcastLogicEnded = true;
	BP_OnTrainLogicEnded(
		ETrainMissionLogicEndReason::CompletionConditionMet,
		M_TrainLogicRuntimeState.M_CompletionCondition,
		TrainNumber,
		TrainActor);
}

bool UTrainMission::GetIsCurrentTrainLogicCallback(
	const int32 TrainNumber,
	const uint32 TrainLogicGeneration) const
{
	return M_TrainLogicRuntimeState.bM_IsActive
		&& M_TrainLogicRuntimeState.M_Generation == TrainLogicGeneration
		&& M_TrainEntries.IsValidIndex(TrainNumber)
		&& M_TrainLogicRuntimeState.M_TrainEntries.IsValidIndex(TrainNumber);
}

void UTrainMission::MarkEntrySkipped(const int32 TrainNumber)
{
	if (not M_TrainLogicRuntimeState.M_TrainEntries.IsValidIndex(TrainNumber))
	{
		return;
	}

	M_TrainLogicRuntimeState.M_TrainEntries[TrainNumber].M_Status =
		ETrainMissionEntryRuntimeStatus::Skipped;
	TryBroadcastSuccessfulTrainLogicEnd(TrainNumber, nullptr);
}

void UTrainMission::CleanupEntryCallbacks(FTrainMissionEntryRuntimeState& RuntimeState)
{
	ATrainUnit* const TrainActor = RuntimeState.M_TrainActor.Get();
	if (IsValid(TrainActor))
	{
		if (RuntimeState.M_ReachedTargetDelegateHandle.IsValid())
		{
			TrainActor->OnReachedSplineTarget.Remove(RuntimeState.M_ReachedTargetDelegateHandle);
		}
		if (RuntimeState.M_UnitDiedDelegateHandle.IsValid())
		{
			TrainActor->OnUnitDies.Remove(RuntimeState.M_UnitDiedDelegateHandle);
		}
		TrainActor->OnDestroyed.RemoveDynamic(this, &UTrainMission::HandleTrainActorDestroyed);
	}

	RuntimeState.M_ReachedTargetDelegateHandle.Reset();
	RuntimeState.M_UnitDiedDelegateHandle.Reset();
}

void UTrainMission::RemoveEntryMiniMapIcon(FTrainMissionEntryRuntimeState& RuntimeState)
{
	if (RuntimeState.M_MiniMapIconId.IsNone())
	{
		return;
	}

	RemoveCustomMiniMapIcon(RuntimeState.M_MiniMapIconId);
	RuntimeState.M_MiniMapIconId = NAME_None;
}

void UTrainMission::CleanupTrainLogic()
{
	UWorld* const World = GetWorld();
	for (FTrainMissionEntryRuntimeState& RuntimeState : M_TrainLogicRuntimeState.M_TrainEntries)
	{
		if (IsValid(World))
		{
			World->GetTimerManager().ClearTimer(RuntimeState.M_DepartureTimerHandle);
		}
		CleanupEntryCallbacks(RuntimeState);
		RemoveEntryMiniMapIcon(RuntimeState);
	}

	M_TrainEntries.Empty();
	M_TrainLogicRuntimeState.M_TrainEntries.Empty();
	M_TrainLogicRuntimeState.bM_IsActive = false;
	M_TrainLogicRuntimeState.bM_HasBroadcastLogicEnded = false;
	++M_TrainLogicRuntimeState.M_Generation;
}
