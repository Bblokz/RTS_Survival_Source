#include "MissionBase.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Missions/MissionManager/MissionManager.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Missions/MissionWidgets/W_MissionTimer.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Player/PlayerAudioController/PlayerAudioController.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

UMissionBase::UMissionBase()
{
	MissionState.bIsMissionComplete = false;
}

void UMissionBase::LoadMission(AMissionManager* MissionManager)
{
	M_MissionManager = MissionManager;
	MissionState.bIsMissionComplete = false;
	DebugMission("Loading mission " + GetName());
	LoadMissionGetRTSAsyncSpawner();
	if (LoadToStartDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(M_LoadToStartTimerHandle, this, &UMissionBase::OnMissionStart,
		                                       LoadToStartDelay, false);
	}
	else
	{
		OnMissionStart();
	}
}

bool UMissionBase::GetCanCheckTrigger() const
{
	return not MissionState.bIsTriggered && GetHasValidTrigger();
}

void UMissionBase::OnMissionComplete()
{
	if (MissionState.bIsMissionComplete)
	{
		RTSFunctionLibrary::ReportError("Mission already completed: " + GetName());
		return;
	}
	// Mark the widget as free to be able to be used by another mission.
	MarkWidgetAsFree();
	MissionState.bIsMissionComplete = true;
	// Stop any tick behaviour.
	MissionState.bTickOnMission = false;
	DebugMission("Completed mission " + GetName());
	if (GetMissionManagerChecked())
	{
		const bool bPlaySound = not bIsTextOnlyMission;
		M_MissionManager->OnAnyMissionCompleted(this, bPlaySound);
	}
	if (NextMission)
	{
		// Load the new mission that comes after this one is completed.
		M_MissionManager->ActivateNewMission(NextMission);
	}
	BP_OnMissionComplete();
}

void UMissionBase::OnMissionFailed()
{
	MissionState.bIsMissionComplete = true;
	DebugMission("Failed mission " + GetName());
	if (GetMissionManagerChecked())
	{
		M_MissionManager->OnAnyMissionFailed(this);
	}
	// Mark the widget as free to be able to be used by another mission.
	MarkWidgetAsFree();
	BP_OnMissionFailed();
}

void UMissionBase::TickMission(float DeltaTime)
{
	// Default does nothing.
	// Derived missions can implement per-frame updates if necessary.
}

UW_MissionTimer* UMissionBase::SpawnMissionTimerWidget(TSubclassOf<UW_MissionTimer> MissionTimerWidgetClass,
                                                       const FText& TimerText,
                                                       float TimerInSeconds,
                                                       const FMissionTimerLifetimeSettings& LifetimeSettings)
{
	AMissionManager* MissionManager = GetMissionManagerChecked();
	if (not MissionManager)
	{
		return nullptr;
	}

	return MissionManager->CreateAndInitMissionTimerWidget(
		MissionTimerWidgetClass,
		TimerText,
		TimerInSeconds,
		LifetimeSettings);
}

int32 UMissionBase::ScheduleRepeatedCallback(
	const FMissionScheduledCallback& Callback,
	const int32 TotalCalls,
	const int32 IntervalSeconds,
	const int32 InitialDelaySeconds,
	const bool bFireBeforeFirstInterval,
	const bool bRepeatForever
)
{
	if (not GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportError("Mission tried to schedule callback while mission manager is invalid.");
		return INDEX_NONE;
	}

	const TArray<AActor*> RequiredActors = {};
	const int32 TaskID = GetMissionManagerChecked()->ScheduleMissionCallback(
		Callback,
		TotalCalls,
		IntervalSeconds,
		InitialDelaySeconds,
		this,
		RequiredActors,
		bFireBeforeFirstInterval,
		bRepeatForever
	);

	RegisterScheduledTaskID(TaskID);
	return TaskID;
}

int32 UMissionBase::ScheduleRepeatedCallbackWithRequirement(const FMissionScheduledCallback& Callback,
                                                            const int32 TotalCalls, const int32 IntervalSeconds,
                                                            const int32 InitialDelaySeconds,
                                                            const TArray<AActor*>& RequiredActors,
                                                            const bool bFireBeforeFirstInterval,
                                                            const bool bRepeatForever)
{
	if (not GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportError("Mission tried to schedule callback while mission manager is invalid.");
		return INDEX_NONE;
	}

	const int32 TaskID = GetMissionManagerChecked()->ScheduleMissionCallback(
		Callback,
		TotalCalls,
		IntervalSeconds,
		InitialDelaySeconds,
		this,
		RequiredActors,
		bFireBeforeFirstInterval,
		bRepeatForever
	);

	RegisterScheduledTaskID(TaskID);
	return TaskID;
}

int32 UMissionBase::ScheduleSingleCallback(
	const FMissionScheduledCallback& Callback,
	const int32 DelaySeconds
)
{
	if (not GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportError("Mission tried to schedule single callback while mission manager is invalid.");
		return INDEX_NONE;
	}
	const TArray<AActor*> RequiredActors = {};

	const int32 TaskID = GetMissionManagerChecked()->ScheduleSingleMissionCallback(
		Callback,
		DelaySeconds,
		this,
		RequiredActors
	);
	RegisterScheduledTaskID(TaskID);
	return TaskID;
}

int32 UMissionBase::ScheduleSingleCallbackWithRequirement(const FMissionScheduledCallback& Callback,
                                                          const int32 DelaySeconds,
                                                          const TArray<AActor*>& RequiredActors)
{
	if (not GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportError("Mission tried to schedule single callback while mission manager is invalid.");
		return INDEX_NONE;
	}

	const int32 TaskID = GetMissionManagerChecked()->ScheduleSingleMissionCallback(
		Callback,
		DelaySeconds,
		this,
		RequiredActors
	);
	RegisterScheduledTaskID(TaskID);
	return TaskID;
}

void UMissionBase::CancelScheduledCallback(const int32 TaskID)
{
	if (not GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportError("Mission tried to cancel callback while mission manager is invalid.");
		return;
	}

	PruneInactiveScheduledTaskIDs();
	if (not GetMissionManagerChecked()->GetIsMissionCallbackActive(TaskID))
	{
		RemoveTrackedTaskID(TaskID);
		return;
	}

	GetMissionManagerChecked()->CancelMissionCallback(TaskID);
	RemoveTrackedTaskID(TaskID);
}

void UMissionBase::CancelAllScheduledCallbacks()
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	PruneInactiveScheduledTaskIDs();
	GetMissionManagerChecked()->CancelAllCallbacksForObject(this);
	M_ScheduledTaskIds.Empty();
}

bool UMissionBase::GetIsScheduledTaskActive(const int32 TaskID) const
{
	if (not GetIsValidMissionManager())
	{
		return false;
	}

	return GetMissionManagerChecked()->GetIsMissionCallbackActive(TaskID);
}

void UMissionBase::OnCleanUpMission()
{
	if (GetIsValidMissionManager())
	{
		CancelAllScheduledCallbacks();
	}
	else
	{
		M_ScheduledTaskIds.Empty();
	}

	if (not GetWorld())
	{
		return;
	}
	MarkWidgetAsFree();
	GetWorld()->GetTimerManager().ClearTimer(M_LoadToStartTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(M_TextOnlyDurationHandle);
	CleanUp_TimerHandles(GetWorld());
	BP_OnMissionCleanUp();
}

void UMissionBase::BeginDestroy()
{
	UObject::BeginDestroy();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(M_TextOnlyDurationHandle);
		GetWorld()->GetTimerManager().ClearTimer(M_LoadToStartTimerHandle);
	}
}

AMissionManager* UMissionBase::GetMissionManagerChecked() const
{
	if (not M_MissionManager.IsValid())
	{
		RTSFunctionLibrary::ReportError("Mission manager not valid for mission: " + GetName());
		return nullptr;
	}
	return M_MissionManager.Get();
}

TArray<FTrainingOption> UMissionBase::GetTrainingOptionsDifficultyAdjusted(TArray<FTrainingOption> EasyOptions,
                                                                           TArray<FTrainingOption> NormalOptions,
                                                                           TArray<FTrainingOption> HardOptions,
                                                                           TArray<FTrainingOption> BrutalOptions,
                                                                           TArray<FTrainingOption> IronmanOptions) const
{
	if (not GetGameDifficulty().bIsInitialized)
	{
		RTSFunctionLibrary::ReportError(
			"Mission attempted to get training options but game difficulty is not initialized!"
			"\n Mission : " + GetName());
	}
	if (GetIsGameDifficultyIronMan())
	{
		return IronmanOptions;
	}
	if (GetIsGameDifficultyBrutalOrHigher())
	{
		return BrutalOptions;
	}
	if (GetIsGameDifficultyHardOrHigher())
	{
		return HardOptions;
	}
	if (GetIsGameDifficultyNormalOrHigher())
	{
		return NormalOptions;
	}
	return EasyOptions;
}

bool UMissionBase::CreateSingleAttackMoveDifficultyConditional(ERTSGameDifficulty MinimalDifficulty,
                                                               TArray<FVector> SpawnLocations,
                                                               TArray<FTrainingOption> TrainingOptions,
                                                               TArray<FVector> WayPoints,
                                                               FRotator FinalRotation,
                                                               const int32 MaxFormationWidth, float TimeTillWave,
                                                               const float FormationOffsetMultiplier,
                                                               const float HelpOffsetRadiusMltMax,
                                                               const float HelpOffsetRadiusMltMin,
                                                               const float MaxAttackTimeBeforeAdvancingToNextWayPoint,
                                                               const int32 MaxTriesFindNavPointForHelpOffset,
                                                               const float ProjectionScale)
{
	AEnemyController* EnemyController = GetEnemyControllerChecked();
	if (not EnemyController)
	{
		return false;
	}
	if (not GetIsDifficultyAtLeast(MinimalDifficulty))
	{
		return false;
	}
	if (TrainingOptions.IsEmpty() || SpawnLocations.IsEmpty() || WayPoints.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"Mission attempted to create single attack move wave but one of the required arrays is empty!"
			"\n Mission : " + GetName());
		return false;
	}

	TArray<FAttackWaveElement> WaveElements;
	for (const FVector& EachSpawnLocation : SpawnLocations)
	{
		FAttackWaveElement NewWaveElement;
		NewWaveElement.SpawnLocation = EachSpawnLocation;
		NewWaveElement.UnitOptions = TrainingOptions;
		WaveElements.Add(NewWaveElement);
	}
	EnemyController->CreateSingleAttackMoveWave(
		EEnemyWaveType::Wave_NoOwningBuilding,
		WaveElements,
		WayPoints,
		FinalRotation,
		MaxFormationWidth,
		TimeTillWave,
		nullptr,
		FormationOffsetMultiplier,
		HelpOffsetRadiusMltMax,
		HelpOffsetRadiusMltMin,
		MaxAttackTimeBeforeAdvancingToNextWayPoint,
		MaxTriesFindNavPointForHelpOffset,
		ProjectionScale);
	return true;
}


FRTSGameDifficulty UMissionBase::GetGameDifficulty() const
{
	if (not GetIsValidMissionManager())
	{
		return FRTSGameDifficulty();
	}
	return M_MissionManager->GetCurrentGameDifficulty();
}

int32 UMissionBase::GetGameDifficultyPercentage() const
{
	if (not GetIsValidMissionManager())
	{
		return 100;
	}
	return M_MissionManager->GetCurrentGameDifficulty().DifficultyPercentage;
}

int32 UMissionBase::GetGameDifficultyPercentageDividedBy(const int32 Divisor) const
{
	if (not GetIsValidMissionManager())
	{
		return FMath::Max(1, 100 / Divisor);
	}
	return FMath::Max(1, M_MissionManager->GetCurrentGameDifficulty().DifficultyPercentage / Divisor);
}

bool UMissionBase::GetIsGameDifficultyNormalOrHigher() const
{
	return GetGameDifficulty().IsNormalOrHigher();
}

bool UMissionBase::GetIsGameDifficultyHardOrHigher() const
{
	return GetGameDifficulty().IsHardOrHigher();
}

bool UMissionBase::GetIsGameDifficultyBrutalOrHigher() const
{
	return GetGameDifficulty().IsBrutalOrHigher();
}

bool UMissionBase::GetIsGameDifficultyIronMan() const
{
	return GetGameDifficulty().IsIronman();
}

bool UMissionBase::GetIsDifficultyAtLeast(const ERTSGameDifficulty DifficultyLevel) const
{
	if (not GetGameDifficulty().bIsInitialized)
	{
		RTSFunctionLibrary::ReportError("Mission attempted to get game difficulty but it is not initialized!"
			"\n Mission : " + GetName());
		return false;
	}
	return GetGameDifficulty().DifficultyLevel >= DifficultyLevel;
}


AEnemyController* UMissionBase::GetEnemyControllerChecked() const
{
	if (AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(M_MissionManager.Get()))
	{
		return EnemyController;
	}
	RTSFunctionLibrary::ReportError("Mission attempted to get enemy controller but it is not valid!"
		"\n Mission : " + GetName());
	return nullptr;
}

UW_Mission* UMissionBase::GetMissionWidgetChecked() const
{
	if (not GetIsValidMissionWidget())
	{
		RTSFunctionLibrary::ReportError("Blueprint mission instance attempted to get mission widget but is invalid!"
		);
		return nullptr;
	}
	return M_MissionWidget.Get();
}

void UMissionBase::PlaySound2D(USoundBase* SoundToPlay) const
{
	if (not GetIsValidMissionManager())
	{
		return;
	}
	M_MissionManager->PlaySound2DForMission(SoundToPlay);
}

void UMissionBase::PlayPortrait(const ERTSPortraitTypes PortraitType, USoundBase* VoiceLine) const
{
	const auto PlayerPortraitManager = FRTS_Statics::GetPlayerPortraitManager(this);
	if (not PlayerPortraitManager)
	{
		RTSFunctionLibrary::ReportError("Mission attempted to play portrait but player portrait manager is not valid!"
			"\n Mission : " + GetName());
		return;
	}
	PlayerPortraitManager->PlayPortrait(PortraitType, VoiceLine);
}

void UMissionBase::PlayPlayerAnnouncerLine(const EAnnouncerVoiceLineType AnnouncerLineType) const
{
	if (not GetIsValidMissionManager())
	{
		return;
	}
	const auto PlayerAudioController = FRTS_Statics::GetPlayerAudioController(M_MissionManager.Get());
	if (not PlayerAudioController)
	{
		return;
	}
	PlayerAudioController->PlayAnnouncerVoiceLine(AnnouncerLineType, true, true);
}

bool UMissionBase::OnCinematicTakeOverFromMission(const bool bCinematicStarted) const
{
	if (not GetIsValidMissionManager())
	{
		RTSFunctionLibrary::ReportError("Mission cinematic take over failed!");
		return false;
	}
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(M_MissionManager.Get());
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError("Mission cinematic take over failed! Player controller is not valid!");
		return false;
	}
	return PlayerController->OnCinematicTakeOver(bCinematicStarted);
}

void UMissionBase::AddArchiveItem(const ERTSArchiveItem ItemType, const FTrainingOption OptionalUnit,
                                  const int32 SortingPriority, const float NotificationTime) const
{
	if (not GetIsValidMissionManager())
	{
		return;
	}
	UMainGameUI* MainGameUI = FRTS_Statics::GetMainGameUI(M_MissionManager.Get());
	if (not MainGameUI)
	{
		RTSFunctionLibrary::ReportError("Mission failed to add acrhive item as main menu reference is null!");
		return;
	}
	MainGameUI->AddNewArchiveItem(ItemType, OptionalUnit, SortingPriority, NotificationTime);
}

AActor* UMissionBase::FindActorInRange(
	const FVector Location,
	const float Range,
	const TSubclassOf<AActor> ActorClass,
	const bool bFindClosest, const bool bFindPlayerUnits)
{
	// 1) Validate pointers
	if (!GetIsValidMissionManager())
	{
		return nullptr;
	}
	UWorld* World = M_MissionManager->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 2) Build a sphere
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Range);

	// 3) Set up query params: ignore the MissionManager’s owner, no complex collision
	FCollisionQueryParams QueryParams(TEXT("FindActorInRange"), false);
	if (const AActor* Owner = M_MissionManager->GetOwner())
	{
		QueryParams.AddIgnoredActor(Owner);
	}
	QueryParams.bTraceComplex = false;
	const ECollisionChannel TraceChannel = bFindPlayerUnits ? COLLISION_OBJ_PLAYER : COLLISION_OBJ_ENEMY;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(TraceChannel);

	// 5) Perform the overlap
	TArray<FOverlapResult> Hits;
	bool bGotHits = World->OverlapMultiByObjectType(
		Hits,
		Location,
		FQuat::Identity,
		ObjectParams,
		Sphere,
		QueryParams
	);

	if (!bGotHits || Hits.Num() == 0)
	{
		return nullptr;
	}

	AActor* BestActor = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	for (const FOverlapResult& R : Hits)
	{
		AActor* HitActor = R.GetActor();
		if (!HitActor)
		{
			continue;
		}

		if (ActorClass && !HitActor->IsA(ActorClass))
		{
			continue;
		}

		if (bFindClosest)
		{
			float DistSq = FVector::DistSquared(Location, HitActor->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestActor = HitActor;
			}
		}
		else
		{
			// early exit on first valid hit
			return HitActor;
		}
	}

	return BestActor;
}

void UMissionBase::RegisterCallbackOnDestructibleCollapse(ADestructableEnvActor* ActorToTrackWhenCollapse)
{
	if (not IsValid(ActorToTrackWhenCollapse))
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto MissionCallBack = [WeakThis, ActorToTrackWhenCollapse]()-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis.Get()->BP_OnCallBackDestructibleCollapse(ActorToTrackWhenCollapse);
		}
	};
	ActorToTrackWhenCollapse->OnDestructibleCollapse.AddLambda(MissionCallBack);
}


void UMissionBase::RegisterCallbackOnNomadicVehicle(ANomadicVehicle* NomadicVehicle,
                                                    const bool bCallBackOnBuildingConversion)
{
	if (not EnsureNomadicIsValid(NomadicVehicle))
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto MissionCallBack = [WeakThis, NomadicVehicle, bCallBackOnBuildingConversion]()-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis.Get()->BP_OnCallBackNomadicVehicle(NomadicVehicle, bCallBackOnBuildingConversion);
		}
	};
	if (bCallBackOnBuildingConversion)
	{
		NomadicVehicle->OnNomadConvertToBuilding.AddLambda(MissionCallBack);
	}
	else
	{
		NomadicVehicle->OnNomadConvertToVehicle.AddLambda(MissionCallBack);
	}
}

void UMissionBase::RegisterCallbackOnTankDies(ATankMaster* Tank)
{
	if (not EnsureTankIsValid(Tank))
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto MissionCallBack = [WeakThis, Tank]()-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis.Get()->BP_OnCallBackTankDies(Tank);
		}
	};
	Tank->OnUnitDies.AddLambda(MissionCallBack);
}

void UMissionBase::DiginFortifyVehicleOverTime(ATankMaster* Tank, const float TimeTillStartDigIn, const int32 ID)
{
	if (not EnsureTankIsValid(Tank) || TimeTillStartDigIn <= 0.0f)
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto MissionCallBack = [WeakThis, Tank, ID]()-> void
	{
		if (not WeakThis.IsValid() || not IsValid(Tank))
		{
			return;
		}
		Tank->DigIn(true);
		WeakThis.Get()->BP_OnCallBackTankDigIn(Tank, ID);
	};
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(TimerHandle, MissionCallBack, TimeTillStartDigIn, false);
}

void UMissionBase::RegisterCallBackOnPickUpWeapon(ASquadController* SquadController)
{
	if (not EnsureSquadIsValid(SquadController))
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto MissionCallBack = [WeakThis, SquadController]()-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis.Get()->BP_OnCallBackSquadPickupWeapon(SquadController);
		}
	};
	SquadController->OnWeaponPickup.AddLambda(MissionCallBack);
}

void UMissionBase::DestroyActorsInRange(
	float Range,
	const TArray<TSubclassOf<AActor>>& ActorClasses,
	const FVector& Location
)
{
	UWorld* World = GetWorld();
	if (!World || ActorClasses.Num() == 0)
		return;

	const float RangeSq = Range * Range;
	for (const TSubclassOf<AActor>& Class : ActorClasses)
	{
		if (!*Class)
			continue;

		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(World, Class, FoundActors);

		for (AActor* Actor : FoundActors)
		{
			if (!Actor)
				continue;

			if ((Actor->GetActorLocation() - Location).SizeSquared() > RangeSq)
				continue;

			Actor->Destroy();
		}
	}
}


void UMissionBase::OnMissionStart()
{
	MissionState.bIsMissionStarted = true;
	DebugMission("Starting mission " + GetName());
	if (GetHasValidTrigger())
	{
		DebugMission("Mission has a valid trigger. + " + GetName());
		MissionTrigger->InitTrigger(this);
		// Set to false now as we wait for the trigger to be evaluated.
		MissionState.bIsTriggered = false;
		// Wait till the trigger activates the mission UI.
		return;
	}
	DebugMission("Mission has no trigger. + " + GetName());
	CompleteMissionStartSetUIAndNotifyMgr();
	SetCameraControllerReference();
	if (bIsTextOnlyMission)
	{
		TextOnlyMission_SetAutoCompleteTimer();
	}
}

void UMissionBase::DebugMission(const FString& Message) const
{
	if constexpr (DeveloperSettings::Debugging::GMissions_CompileDebugSymbols)
	{
		RTSFunctionLibrary::PrintString(Message, FColor::Blue);
	}
}

void UMissionBase::CleanUp_TimerHandles(const UWorld* World)
{
	if (not World)
	{
		return;
	}
	for (auto EachTimer : M_DelaySpawnTimerHandles)
	{
		World->GetTimerManager().ClearTimer(EachTimer);
	}
}

bool UMissionBase::GetHasValidTrigger() const
{
	return IsValid(MissionTrigger) && MissionTrigger->GetTriggerType() != EMissionTriggerType::None;
}

bool UMissionBase::GetIsValidMissionManager() const
{
	if (M_MissionManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Mission manager not valid for mission: " + GetName());
	return false;
}

void UMissionBase::RegisterScheduledTaskID(const int32 TaskID)
{
	if (TaskID == INDEX_NONE)
	{
		return;
	}

	M_ScheduledTaskIds.Add(TaskID);
}

void UMissionBase::RemoveTrackedTaskID(const int32 TaskID)
{
	M_ScheduledTaskIds.Remove(TaskID);
}

void UMissionBase::PruneInactiveScheduledTaskIDs()
{
	if (not GetIsValidMissionManager())
	{
		M_ScheduledTaskIds.Empty();
		return;
	}

	const AMissionManager* MissionManager = GetMissionManagerChecked();
	if (not MissionManager)
	{
		M_ScheduledTaskIds.Empty();
		return;
	}

	for (int32 TaskIndex = M_ScheduledTaskIds.Num() - 1; TaskIndex >= 0; --TaskIndex)
	{
		const int32 TaskID = M_ScheduledTaskIds[TaskIndex];
		if (MissionManager->GetIsMissionCallbackActive(TaskID))
		{
			continue;
		}

		M_ScheduledTaskIds.RemoveAtSwap(TaskIndex);
	}
}

void UMissionBase::AsyncSpawnActor(
	const FTrainingOption& TrainingOption, const int32 ID, AActor* SpawnPointActor)
{
	if (not EnsureValidRTSSpawner() || not EnsureValidSpawnPoint(SpawnPointActor, TrainingOption))
	{
		return;
	}
	M_MapIdToRotation.Add(ID, SpawnPointActor->GetActorRotation());
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	M_RTSAsyncSpawner->AsyncSpawnOptionAtLocation(TrainingOption,
	                                              SpawnPointActor->GetActorLocation() + FVector(0, 0, 50), this, ID,
	                                              [WeakThis](const FTrainingOption& Option, AActor* SpawnedActor,
	                                                         const int32 ID)-> void
	                                              {
		                                              if (not WeakThis.IsValid())
		                                              {
			                                              return;
		                                              }
		                                              WeakThis->OnAsyncSpawnComplete(Option, SpawnedActor, ID);
	                                              });
}


void UMissionBase::AsyncSpawnActorAtLocation(const FTrainingOption& TrainingOption, const int32 ID,
                                             const FVector SpawnLocation, const FRotator Rotation)
{
	if (not EnsureValidRTSSpawner())
	{
		return;
	}
	M_MapIdToRotation.Add(ID, Rotation);
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	M_RTSAsyncSpawner->AsyncSpawnOptionAtLocation(TrainingOption, SpawnLocation, this, ID,
	                                              [WeakThis](const FTrainingOption& Option, AActor* SpawnedActor,
	                                                         const int32 ID)-> void
	                                              {
		                                              if (not WeakThis.IsValid())
		                                              {
			                                              return;
		                                              }
		                                              WeakThis->OnAsyncSpawnComplete(Option, SpawnedActor, ID);
	                                              });
}


void UMissionBase::AsyncSpawnActorAtLocationWithDelay(const FTrainingOption& TrainingOption, const int32 ID,
                                                      const FVector SpawnLocation, const FRotator Rotation,
                                                      const float Delay)
{
	if (not GetIsValidMissionManager())
	{
		return;
	}
	const UWorld* World = M_MissionManager->GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("Mission attempted async spawn with delay but world is not valid!"
			"\n Mission : " + GetName() +
			"\n Attempted Spawn option: " + FRTS_Statics::Global_GetTrainingOptionString(TrainingOption));
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto DelayedSpawn = [WeakThis, TrainingOption, ID, SpawnLocation, Rotation]()-> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->AsyncSpawnActorAtLocation(
			TrainingOption,
			ID,
			SpawnLocation,
			Rotation);
	};
	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(TimerHandle, DelayedSpawn, Delay, false);
	M_DelaySpawnTimerHandles.Add(TimerHandle);
}

void UMissionBase::SpawnTowedTeamWeapon(const ETankSubtype TankSubtype, const ESquadSubtype SquadSubtype,
                                        const FVector SpawnLocation)
{
	AMissionManager* MissionManager = GetMissionManagerChecked();
	if (not IsValid(MissionManager))
	{
		return;
	}

	MissionManager->SpawnTowedTeamWeapon(TankSubtype, SquadSubtype, SpawnLocation);
}

void UMissionBase::MoveCamera(const FMovePlayerCamera CameraMove)
{
	SetCameraControllerReference();
	if (not GetIsValidPlayerCameraController())
	{
		return;
	}
	M_PlayerCameraController->MoveCameraOverTime(CameraMove);
}

FTrainingOption UMissionBase::SelectSquadOnDifficultyAt() const
{
	FTrainingOption Option = FTrainingOption(EAllUnitType::UNType_Squad,
	                                         static_cast<uint8>(ESquadSubtype::Squad_Rus_Okhotnik));
	if (not GetIsValidMissionManager())
	{
		return Option;
	}
	switch (GetGameDifficulty().DifficultyLevel)
	{
	case ERTSGameDifficulty::NewToRTS:
		break;
	case ERTSGameDifficulty::Normal:
		break;
	case ERTSGameDifficulty::Hard:
		Option.SubtypeValue = static_cast<uint8>(ESquadSubtype::Squad_Rus_LargePTRS);
		break;
	case ERTSGameDifficulty::Brutal:
		Option.SubtypeValue = static_cast<uint8>(ESquadSubtype::Squad_Rus_RedHamerPTRS);
		break;
	case ERTSGameDifficulty::Ironman:
		Option.SubtypeValue = static_cast<uint8>(ESquadSubtype::Squad_Rus_RedHamerPTRS);
		break;
	}
	return Option;
}

FTrainingOption UMissionBase::SelectSquadOnDifficultyAntiInfantry() const
{
	FTrainingOption Option = FTrainingOption(EAllUnitType::UNType_Squad,
	                                         static_cast<uint8>(ESquadSubtype::Squad_Rus_HazmatEngineers));
	if (not GetIsValidMissionManager())
	{
		return Option;
	}
	switch (GetGameDifficulty().DifficultyLevel)
	{
	case ERTSGameDifficulty::NewToRTS:
		break;
	case ERTSGameDifficulty::Normal:
		break;
	case ERTSGameDifficulty::Hard:
		Option.SubtypeValue = static_cast<uint8>(ESquadSubtype::Squad_Rus_Mosin);
		break;
	case ERTSGameDifficulty::Brutal:
		Option.SubtypeValue = static_cast<uint8>(ESquadSubtype::Squad_Rus_RedHammer);
		break;
	case ERTSGameDifficulty::Ironman:
		Option.SubtypeValue = static_cast<uint8>(ESquadSubtype::Squad_Rus_RedHammer);
		break
	}
	return Option;
}

FTrainingOption UMissionBase::SelectLightTankOnDifficulty() const
{
	const FTrainingOption BackupOption = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_BT7));
	if (not GetIsValidMissionManager())
	{
		return BackupOption;
	}
	switch (GetGameDifficulty().DifficultyLevel)
	{
	case ERTSGameDifficulty::NewToRTS:
		return M_MissionManager->SelectSeededTankOption({
			ETankSubtype::Tank_Ba12, ETankSubtype::Tank_BT7, ETankSubtype::Tank_BT7_4
		});
	case ERTSGameDifficulty::Normal:
		return M_MissionManager->SelectSeededTankOption({
			ETankSubtype::Tank_Ba14, ETankSubtype::Tank_T26, ETankSubtype::Tank_BT7_4
		});
	case ERTSGameDifficulty::Hard:
		return M_MissionManager->SelectSeededTankOption({
			ETankSubtype::Tank_Ba14, ETankSubtype::Tank_T70, ETankSubtype::Tank_T70
		});
	// Falls through (brutal and ironman always share)
	case ERTSGameDifficulty::Brutal:
	case ERTSGameDifficulty::Ironman:
		return URTSBlueprintFunctionLibrary::GetHeavyTank_T28();
	}
	return BackupOption;
}

void UMissionBase::SetCameraControllerReference()
{
	if (not GetIsValidMissionManager())
	{
		return;
	}
	M_PlayerCameraController = FRTS_Statics::GetPlayerCameraController(GetMissionManagerChecked());
}

bool UMissionBase::GetIsValidPlayerCameraController() const
{
	if (not M_PlayerCameraController.IsValid())
	{
		RTSFunctionLibrary::ReportError("Invalid Player Camera Controller for mission " + GetName());
		return false;
	}
	return true;
}

bool UMissionBase::EnsureNomadicIsValid(const TObjectPtr<ANomadicVehicle>& NomadicVehicle) const
{
	if (IsValid(NomadicVehicle))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("For mission : " + GetName() +
		" Nomadic vehicle is not valid!");
	return false;
}

bool UMissionBase::EnsureTankIsValid(const TObjectPtr<ATankMaster>& Tank) const
{
	if (IsValid(Tank))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("For mission : " + GetName() +
		" Tank is not valid!");
	return false;
}

bool UMissionBase::EnsureSquadIsValid(const TObjectPtr<ASquadController>& SquadController) const
{
	if (IsValid(SquadController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("For mission : " + GetName() +
		" Squad controller is not valid!");
	return false;
}

void UMissionBase::TextOnlyMission_SetAutoCompleteTimer()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	if (TextOnlyMissionDuration <= 0)
	{
		// Purely information missions may want to finish instantly.
		OnMissionComplete();
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto AutoComplete = [WeakThis]()-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnMissionComplete();
		}
	};
	World->GetTimerManager().SetTimer(M_TextOnlyDurationHandle, AutoComplete, TextOnlyMissionDuration,
	                                  false);
}

FTrainingOption UMissionBase::SelectTankOptionPerDifficultySeeded(TArray<ETankSubtype> NewToRTSTypes,
                                                                  TArray<ETankSubtype> NormalDiffTypes,
                                                                  TArray<ETankSubtype> HardDiffTypes, TArray<
	                                                                  ETankSubtype> BrutalAndIronManTypes) const
{
	FTrainingOption Option = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_T26));
	if (not GetIsValidMissionManager())
	{
		return Option;
	}
	TArray<ETankSubtype> SelectedArray = NewToRTSTypes;
	switch (GetGameDifficulty().DifficultyLevel)
	{
	case ERTSGameDifficulty::NewToRTS:
		break;
	case ERTSGameDifficulty::Normal:
		SelectedArray = NormalDiffTypes;
		break;
	case ERTSGameDifficulty::Hard:
		SelectedArray = HardDiffTypes;
		break;
	case ERTSGameDifficulty::Brutal:
		SelectedArray = BrutalAndIronManTypes;
		break;
	case ERTSGameDifficulty::Ironman:
		SelectedArray = BrutalAndIronManTypes;
		break;
	}

	if (SelectedArray.Num() == 1)
	{
		Option = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(SelectedArray[0]));
		return Option;
	}
	Option = M_MissionManager->SelectSeededTankOption(SelectedArray);
	return Option;
}

FTrainingOption UMissionBase::SelectSquadOptionPerDifficultySeeded(TArray<ESquadSubtype> NewToRTSTypes,
                                                                   TArray<ESquadSubtype> NormalDiffTypes,
                                                                   TArray<ESquadSubtype> HardDiffTypes,
                                                                   TArray<ESquadSubtype> BrutalAndIronManTypes) const
{
	FTrainingOption Option = FTrainingOption(EAllUnitType::UNType_Squad,
	                                         static_cast<uint8>(ESquadSubtype::Squad_Rus_Mosin));
	if (not GetIsValidMissionManager())
	{
		return Option;
	}
	TArray<ESquadSubtype> SelectedArray = NewToRTSTypes;
	switch (GetGameDifficulty().DifficultyLevel)
	{
	case ERTSGameDifficulty::NewToRTS:
		break;
	case ERTSGameDifficulty::Normal:
		SelectedArray = NormalDiffTypes;
		break;
	case ERTSGameDifficulty::Hard:
		SelectedArray = HardDiffTypes;
		break;
	case ERTSGameDifficulty::Brutal:
		SelectedArray = BrutalAndIronManTypes;
		break;
	case ERTSGameDifficulty::Ironman:
		SelectedArray = BrutalAndIronManTypes;
		break;
	}
	if (SelectedArray.Num() == 1)
	{
		Option = FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SelectedArray[0]));
		return Option;
	}
	Option = M_MissionManager->SelectSeededSquadOption(SelectedArray);
	return Option;
}


bool UMissionBase::GetIsValidMissionWidget() const
{
	if (M_MissionWidget.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Mission widget not valid for mission: " + GetName());
	return false;
}

void UMissionBase::MarkWidgetAsFree() const
{
	if (not GetIsValidMissionWidget())
	{
		return;
	}
	M_MissionWidget->MarkWidgetAsFree();
}

void UMissionBase::CompleteMissionStartSetUIAndNotifyMgr()
{
	if (GetMissionManagerChecked())
	{
		M_MissionManager->OnAnyMissionStarted(this);
		M_MissionWidget = M_MissionManager->OnMissionRequestSetupWidget(this);
	}
	if (M_MissionWidget.IsValid())
	{
		M_MissionWidget->InitMissionWidget(MissionWidgetState, this,
		                                   bIsTextOnlyMission, bStartAsCollapsedWidget);
	}
	BP_OnMissionStart();
}

void UMissionBase::OnTriggerActivated()
{
	if (MissionState.bIsTriggered)
	{
		RTSFunctionLibrary::ReportError("Undefined state: trigger attempted to trigger mission start but the mission "
			"was already triggered!"
			"\n Mission : " + GetName());
		return;
	}
	// Makes sure that the mission is not triggered twice.
	MissionState.bIsTriggered = true;
	// If the mission had set loading time before starting then we wait for that to complete before updating the UI.
	if (LoadToStartDelay > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(M_LoadToStartTimerHandle, this,
		                                       &UMissionBase::CompleteMissionStartSetUIAndNotifyMgr,
		                                       LoadToStartDelay, false);
	}
	else
	{
		CompleteMissionStartSetUIAndNotifyMgr();
	}
	if (bIsTextOnlyMission)
	{
		TextOnlyMission_SetAutoCompleteTimer();
	}
}

void UMissionBase::LoadMissionGetRTSAsyncSpawner()
{
	if (not GetIsValidMissionManager())
	{
		return;
	}
	M_RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(M_MissionManager.Get());
}

bool UMissionBase::EnsureValidRTSSpawner()
{
	if (M_RTSAsyncSpawner.IsValid())
	{
		return true;
	}
	if (not GetIsValidMissionManager())
	{
		return false;
	}
	M_RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(M_MissionManager.Get());
	return M_RTSAsyncSpawner.IsValid();
}

void UMissionBase::OnAsyncSpawnComplete(const FTrainingOption& TrainingOption, AActor* SpawnedActor, const int32 ID)
{
	if (not EnsureSpawnedActorIsValid(SpawnedActor))
	{
		BP_OnAsyncSpawnComplete(TrainingOption, SpawnedActor, ID);
		return;
	}
	if (M_MapIdToRotation.Contains(ID))
	{
		SpawnedActor->SetActorRotation(M_MapIdToRotation[ID], ETeleportType::ResetPhysics);
		M_MapIdToRotation.Remove(ID);
	}
	BP_OnAsyncSpawnComplete(TrainingOption, SpawnedActor, ID);
}

bool UMissionBase::EnsureSpawnedActorIsValid(const AActor* SpawnedActor) const
{
	if (not IsValid(SpawnedActor))
	{
		RTSFunctionLibrary::ReportError("Mission failed to async load and spawn actor!, "
			"\n Mission : " + GetName());
		return false;
	}
	return true;
}

bool UMissionBase::EnsureValidSpawnPoint(AActor* SpawnPointActor, const FTrainingOption& TrainingOption)
{
	if (not IsValid(SpawnPointActor))
	{
		const FString MainEnumTypeName = UEnum::GetValueAsString(TrainingOption.UnitType);
		const FString SubEnumTypeName = FString::FromInt(TrainingOption.SubtypeValue);
		RTSFunctionLibrary::ReportError("Invalid spawn point provided, cannot spawn training option"
			"\n Main type: " + MainEnumTypeName +
			"\n Sub type: " + SubEnumTypeName);
		return false;
	}
	return true;
}
