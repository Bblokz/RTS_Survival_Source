#include "MissionManager.h"

#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionManager/MissionTriggerVolumesManager/MissionTriggerVolumesManager.h"
#include "RTS_Survival/Missions/TriggerAreas/TriggerArea.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Missions/MissionWidgets/W_MissionTimer.h"
#include "RTS_Survival/Missions/MissionWidgets/MissionWidgetManager/W_MissionWidgetManager.h"
#include "RTS_Survival/Missions/Defeat/W_Defeat.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"

void FMissionEnemyUnitDestroyedCallbackState::Init(
	UMissionBase* Mission,
	const EEnemyUnitQueryType EnemyUnitQueryType,
	const int32 CallbackID,
	const TArray<AActor*>& TrackedActors)
{
	M_Mission = Mission;
	M_EnemyUnitQueryType = EnemyUnitQueryType;
	M_CallbackID = CallbackID;
	M_AllTrackedActors.Empty();
	M_RemainingTrackedActors.Empty();
	bM_HasNotifiedMission = false;

	for (AActor* TrackedActor : TrackedActors)
	{
		if (not RTSFunctionLibrary::RTSIsValid(TrackedActor))
		{
			continue;
		}

		M_AllTrackedActors.Add(TrackedActor);
		M_RemainingTrackedActors.Add(TrackedActor);
	}
}

bool FMissionEnemyUnitDestroyedCallbackState::GetIsTrackingActor(const AActor* Actor) const
{
	if (not IsValid(Actor))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& TrackedActor : M_RemainingTrackedActors)
	{
		if (TrackedActor.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}

void FMissionEnemyUnitDestroyedCallbackState::OnTrackedActorDestroyed(AActor* DestroyedActor)
{
	if (not IsValid(DestroyedActor))
	{
		return;
	}

	M_RemainingTrackedActors.Remove(DestroyedActor);
}

bool FMissionEnemyUnitDestroyedCallbackState::TryNotifyMission()
{
	if (M_RemainingTrackedActors.Num() > 0 || bM_HasNotifiedMission)
	{
		return false;
	}

	if (not M_Mission.IsValid())
	{
		bM_HasNotifiedMission = true;
		return true;
	}

	M_Mission->OnEnemyUnitsDestroyedCallback(M_CallbackID, M_EnemyUnitQueryType);
	bM_HasNotifiedMission = true;
	return true;
}

bool FMissionEnemyUnitDestroyedCallbackState::GetShouldCleanup() const
{
	if (bM_HasNotifiedMission)
	{
		return true;
	}

	if (not M_Mission.IsValid())
	{
		return true;
	}

	return M_RemainingTrackedActors.IsEmpty();
}

TArray<AActor*> FMissionEnemyUnitDestroyedCallbackState::GetTrackedActorsForCleanup() const
{
	TArray<AActor*> TrackedActors;
	for (const TWeakObjectPtr<AActor>& TrackedActor : M_AllTrackedActors)
	{
		AActor* TrackedActorRaw = TrackedActor.Get();
		if (not IsValid(TrackedActorRaw))
		{
			continue;
		}

		TrackedActors.Add(TrackedActorRaw);
	}

	return TrackedActors;
}

AMissionManager::AMissionManager()
{
	// Set to true to tick missions.
	PrimaryActorTick.bCanEverTick = true;
	M_MissionTriggerVolumesManager = CreateDefaultSubobject<UMissionTriggerVolumesManager>(TEXT("MissionTriggerVolumesManager"));
}

void AMissionManager::OnAnyMissionCompleted(UMissionBase* CompletedMission, const bool bPlaySound)
{
	if (not EnsureMissionIsValid(CompletedMission))
	{
		return;
	}

	M_CompletedMissionClasses.Add(CompletedMission->GetClass());
	RemoveAllMissionTriggerAreasForMission(CompletedMission);
	RemoveActiveMission(CompletedMission);
	if (bPlaySound)
	{
		PlayMissionSound(EMissionSoundType::MissionCompleted);
	}
}

void AMissionManager::OnAnyMissionStarted(UMissionBase* LoadedMission)
{
	PlayMissionSound(EMissionSoundType::MissionLoaded);
}

void AMissionManager::OnAnyMissionFailed(UMissionBase* FailedMission)
{
	RemoveAllMissionTriggerAreasForMission(FailedMission);
	RemoveActiveMission(FailedMission);
	PlayMissionSound(EMissionSoundType::MissionFailed);
}

void AMissionManager::SetMissionDifficulty(const int32 NewDifficultyPercentage, const ERTSGameDifficulty GameDifficulty)
{
	M_GameDifficulty.DifficultyLevel = GameDifficulty;
	M_GameDifficulty.DifficultyPercentage = NewDifficultyPercentage;
	M_GameDifficulty.bIsInitialized = true;
}

FRTSGameDifficulty AMissionManager::GetCurrentGameDifficulty() const
{
	if (not M_GameDifficulty.bIsInitialized)
	{
		RTSFunctionLibrary::ReportError("Game difficulty requested but not initialized yet in mission manager.");
		return FRTSGameDifficulty();
	}
	return M_GameDifficulty;
}

UW_Mission* AMissionManager::OnMissionRequestSetupWidget(UMissionBase* RequestingMission)
{
	if (not EnsureMissionIsValid(RequestingMission) || not EnsureMissionWidgetIsValid())
	{
		return nullptr;
	}
	if (not M_ActiveMissions.Contains(RequestingMission))
	{
		RTSFunctionLibrary::ReportError("A mission requested for a widget to be setup for it but this mission is not"
			"registered as an active mission on the manager."
			"\n Mission : " + RequestingMission->GetName() +
			"\n See function : OnMissionRequestSetupWidget");
		return nullptr;
	}
	UW_Mission* MissionWidget = M_MissionWidgetManager->GetFreeMissionWidget();
	if (MissionWidget)
	{
		return MissionWidget;
	}
	RTSFunctionLibrary::ReportError("A mission requested a widget but could not get a free one"
		"\n Mission : " + RequestingMission->GetName());
	return nullptr;
}

UW_MissionTimer* AMissionManager::CreateAndInitMissionTimerWidget(TSubclassOf<UW_MissionTimer> MissionTimerWidgetClass,
                                                                  const FText& TimerText,
                                                                  float TimerInSeconds,
                                                                  const FMissionTimerLifetimeSettings& LifetimeSettings)
{
	if (not EnsureValidPlayerController())
	{
		return nullptr;
	}

	if (not MissionTimerWidgetClass)
	{
		RTSFunctionLibrary::ReportError("Mission timer widget class is not set on mission manager.");
		return nullptr;
	}

	UW_MissionTimer* MissionTimerWidget = CreateWidget<UW_MissionTimer>(M_PlayerController.Get(), MissionTimerWidgetClass);
	if (not IsValid(MissionTimerWidget))
	{
		RTSFunctionLibrary::ReportError("Mission manager failed to create mission timer widget.");
		return nullptr;
	}

	constexpr int32 MissionTimerWidgetZOrder = 11;
	MissionTimerWidget->AddToViewport(MissionTimerWidgetZOrder);
	MissionTimerWidget->InitMissionTimer(TimerText, TimerInSeconds, LifetimeSettings);
	return MissionTimerWidget;
}

void AMissionManager::PlaySound2DForMission(USoundBase* SoundToPlay) const
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}
	UGameplayStatics::PlaySound2D(World, SoundToPlay, 1, 1, 0);
}

int32 AMissionManager::ScheduleMissionCallback(
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
	if (not GetIsValidMissionScheduler())
	{
		return INDEX_NONE;
	}

	return M_MissionScheduler->ScheduleCallback(
		Callback,
		TotalCalls,
		IntervalSeconds,
		InitialDelaySeconds,
		CallbackOwner,
		RequiredActors,
		bFireBeforeFirstInterval,
		bRepeatForever
	);
}

int32 AMissionManager::ScheduleSingleMissionCallback(
	const FMissionScheduledCallback& Callback,
	const int32 DelaySeconds,
	UObject* CallbackOwner,
	const TArray<AActor*>& RequiredActors
)
{
	if (not GetIsValidMissionScheduler())
	{
		return INDEX_NONE;
	}

	return M_MissionScheduler->ScheduleSingleCallback(Callback, DelaySeconds, CallbackOwner, RequiredActors);
}

void AMissionManager::CancelMissionCallback(const int32 TaskID)
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->CancelTask(TaskID);
}

void AMissionManager::CancelAllCallbacksForObject(const UObject* CallbackOwner)
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->CancelAllTasksForObject(CallbackOwner);
}

void AMissionManager::PauseMissionCallback(const int32 TaskID)
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->PauseTask(TaskID);
}

void AMissionManager::ResumeMissionCallback(const int32 TaskID)
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->ResumeTask(TaskID);
}

void AMissionManager::PauseAllCallbacksForObject(const UObject* CallbackOwner)
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->PauseAllTasksForObject(CallbackOwner);
}

void AMissionManager::ResumeAllCallbacksForObject(const UObject* CallbackOwner)
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->ResumeAllTasksForObject(CallbackOwner);
}

void AMissionManager::PauseAllCallbacks()
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->PauseAllTasks();
}

void AMissionManager::ResumeAllCallbacks()
{
	if (not GetIsValidMissionScheduler())
	{
		return;
	}

	M_MissionScheduler->ResumeAllTasks();
}

bool AMissionManager::GetIsMissionCallbackActive(const int32 TaskID) const
{
	if (not GetIsValidMissionScheduler())
	{
		return false;
	}

	return M_MissionScheduler->GetIsTaskActive(TaskID);
}

int32 AMissionManager::GetCallbackCountForObject(const UObject* CallbackOwner) const
{
	if (not GetIsValidMissionScheduler())
	{
		return 0;
	}

	return M_MissionScheduler->GetTaskCountForObject(CallbackOwner);
}

int32 AMissionManager::GetTotalScheduledCallbackCount() const
{
	if (not GetIsValidMissionScheduler())
	{
		return 0;
	}

	return M_MissionScheduler->GetTotalScheduledTaskCount();
}

ATriggerArea* AMissionManager::CreateMissionTriggerAreaSphere(UMissionBase* Mission,
                                                              const FVector& Location,
                                                              const FRotator& Rotation,
                                                              const FVector& Scale,
                                                              const ETriggerOverlapLogic TriggerOverlapLogic,
                                                              const float DelayBetweenCallbacks,
                                                              const int32 MaxCallbacks,
                                                              const int32 TriggerId)
{
	if (not GetIsValidMissionTriggerVolumesManager())
	{
		return nullptr;
	}

	return M_MissionTriggerVolumesManager->CreateTriggerAreaForMission(
		Mission,
		EMissionTriggerAreaShape::Sphere,
		Location,
		Rotation,
		Scale,
		TriggerOverlapLogic,
		DelayBetweenCallbacks,
		MaxCallbacks,
		TriggerId
	);
}

ATriggerArea* AMissionManager::CreateMissionTriggerAreaRectangle(UMissionBase* Mission,
                                                                 const FVector& Location,
                                                                 const FRotator& Rotation,
                                                                 const FVector& Scale,
                                                                 const ETriggerOverlapLogic TriggerOverlapLogic,
                                                                 const float DelayBetweenCallbacks,
                                                                 const int32 MaxCallbacks,
                                                                 const int32 TriggerId)
{
	if (not GetIsValidMissionTriggerVolumesManager())
	{
		return nullptr;
	}

	return M_MissionTriggerVolumesManager->CreateTriggerAreaForMission(
		Mission,
		EMissionTriggerAreaShape::Rectangle,
		Location,
		Rotation,
		Scale,
		TriggerOverlapLogic,
		DelayBetweenCallbacks,
		MaxCallbacks,
		TriggerId
	);
}

void AMissionManager::RemoveMissionTriggerAreasById(UMissionBase* Mission, const int32 TriggerId)
{
	if (not GetIsValidMissionTriggerVolumesManager())
	{
		return;
	}

	M_MissionTriggerVolumesManager->RemoveTriggerAreasForMissionById(Mission, TriggerId);
}

void AMissionManager::RemoveAllMissionTriggerAreasForMission(UMissionBase* Mission)
{
	if (not GetIsValidMissionTriggerVolumesManager())
	{
		return;
	}

	M_MissionTriggerVolumesManager->RemoveAllTriggerAreasForMission(Mission);
}

void AMissionManager::CallBackOnMissionWhenEnemyUnitsDestroyed(
	const EEnemyUnitQueryType EnemyUnitQueryType,
	TWeakObjectPtr<UMissionBase> Mission,
	const int32 CallbackID)
{
	if (not Mission.IsValid())
	{
		RTSFunctionLibrary::ReportError(
			"Mission manager received enemy units destroyed callback request with invalid mission.");
		return;
	}

	if (EnemyUnitQueryType == EEnemyUnitQueryType::None)
	{
		RTSFunctionLibrary::ReportError(
			"Mission manager received enemy units destroyed callback request with query type None.");
		return;
	}

	const TArray<AActor*> EnemyActorsToTrack = GetEnemyActorsForQueryType(EnemyUnitQueryType);
	if (EnemyActorsToTrack.IsEmpty())
	{
		Mission->OnEnemyUnitsDestroyedCallback(CallbackID, EnemyUnitQueryType);
		return;
	}

	FMissionEnemyUnitDestroyedCallbackState NewCallbackState;
	NewCallbackState.Init(Mission.Get(), EnemyUnitQueryType, CallbackID, EnemyActorsToTrack);
	const TArray<AActor*> TrackedActors = NewCallbackState.GetTrackedActorsForCleanup();
	if (TrackedActors.IsEmpty())
	{
		Mission->OnEnemyUnitsDestroyedCallback(CallbackID, EnemyUnitQueryType);
		return;
	}

	for (AActor* TrackedActor : TrackedActors)
	{
		RegisterTrackedEnemyActor(TrackedActor);
	}

	M_EnemyUnitDestroyedCallbacks.Add(NewCallbackState);
}

void AMissionManager::ActivateNewMission(UMissionBase* NewMission)
{
	if (not EnsureMissionIsValid(NewMission))
	{
		return;
	}
	M_ActiveMissions.Add(NewMission);
	NewMission->LoadMission(this);
}

bool AMissionManager::GetHasCompletedMissionClassExact(TSubclassOf<UMissionBase> MissionClass) const
{
	if (not MissionClass)
	{
		return false;
	}

	return M_CompletedMissionClasses.Contains(MissionClass);
}

void AMissionManager::SpawnTowedTeamWeapon(const ETankSubtype TankSubtype, const ESquadSubtype SquadSubtype,
                                           const FVector& TankSpawnLocation)
{
	ARTSAsyncSpawner* RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not EnsureValidTowAsyncSpawner(RTSAsyncSpawner))
	{
		return;
	}

	FMissionTowTeamWeaponSpawnState NewTowSpawnState;
	const int32 RequestId = M_NextTowedTeamWeaponSpawnRequestId++;
	NewTowSpawnState.Init(RequestId, this, TankSubtype, SquadSubtype, TankSpawnLocation);
	M_TowedTeamWeaponSpawnStates.Add(NewTowSpawnState);

	FMissionTowTeamWeaponSpawnState* TowSpawnState = FindTowedTeamWeaponSpawnState(RequestId);
	if (not EnsureValidTowedTeamWeaponSpawnState(TowSpawnState))
	{
		return;
	}

	if (not TowSpawnState->StartAsyncSpawn(RTSAsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Mission manager failed to start SpawnTowedTeamWeapon async requests.");
		return;
	}
}

void AMissionManager::SpawnCargoSquadWithVehicle(
	const ETankSubtype TankSubtype,
	const ESquadSubtype SquadSubtype,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	const FVector& MoveLocationAfterEnter)
{
	ARTSAsyncSpawner* RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not EnsureValidTowAsyncSpawner(RTSAsyncSpawner))
	{
		return;
	}

	FMissionCargoSquadWithVehicleSpawnState NewCargoVehicleSpawnState;
	const int32 RequestId = M_NextCargoVehicleSpawnRequestId++;
	NewCargoVehicleSpawnState.Init(
		RequestId,
		this,
		TankSubtype,
		SquadSubtype,
		SpawnLocation,
		SpawnRotation,
		MoveLocationAfterEnter);
	M_CargoVehicleSpawnStates.Add(NewCargoVehicleSpawnState);

	FMissionCargoSquadWithVehicleSpawnState* CargoVehicleSpawnState = FindCargoVehicleSpawnState(RequestId);
	if (not EnsureValidCargoVehicleSpawnState(CargoVehicleSpawnState))
	{
		return;
	}

	if (not CargoVehicleSpawnState->StartAsyncSpawn(RTSAsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Mission manager failed to start SpawnCargoSquadWithVehicle async requests.");
	}
}

void AMissionManager::SpawnActorAtLocationWithCommandQueue(
	const FTrainingOption& TrainingOption,
	const int32 SpawnId,
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	const TArray<TSubclassOf<class UBehaviour>>& BehavioursToApply,
	const TArray<FMissionSpawnCommandQueueOrder>& CommandQueue,
	UMissionBase* MissionOwner)
{
	ARTSAsyncSpawner* RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not EnsureValidSpawnCommandQueueAsyncSpawner(RTSAsyncSpawner))
	{
		return;
	}

	FMissionSpawnCommandQueueState NewSpawnCommandQueueState;
	const int32 RequestId = M_NextSpawnCommandQueueRequestId++;
	NewSpawnCommandQueueState.Init(
		RequestId,
		this,
		MissionOwner,
		TrainingOption,
		SpawnId,
		SpawnLocation,
		SpawnRotation,
		BehavioursToApply,
		CommandQueue);
	M_SpawnCommandQueueStates.Add(NewSpawnCommandQueueState);

	FMissionSpawnCommandQueueState* SpawnCommandQueueState = FindSpawnCommandQueueState(RequestId);
	if (not EnsureValidSpawnCommandQueueState(SpawnCommandQueueState))
	{
		return;
	}

	if (not SpawnCommandQueueState->StartAsyncSpawn(RTSAsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Mission manager failed to start SpawnActorAtLocationWithCommandQueue request.");
		return;
	}
}

void AMissionManager::HandleSpawnTowedTeamWeaponTankSpawned(const int32 RequestId, AActor* SpawnedTankActor)
{
	FMissionTowTeamWeaponSpawnState* TowSpawnState = FindTowedTeamWeaponSpawnState(RequestId);
	if (not EnsureValidTowedTeamWeaponSpawnState(TowSpawnState))
	{
		return;
	}

	if (not TowSpawnState->HandleTankSpawnedActor(SpawnedTankActor))
	{
		return;
	}

	TryCompleteTowedTeamWeaponSpawnRequest(RequestId);
}

void AMissionManager::HandleSpawnTowedTeamWeaponSquadSpawned(const int32 RequestId, AActor* SpawnedSquadActor)
{
	FMissionTowTeamWeaponSpawnState* TowSpawnState = FindTowedTeamWeaponSpawnState(RequestId);
	if (not EnsureValidTowedTeamWeaponSpawnState(TowSpawnState))
	{
		return;
	}

	if (not TowSpawnState->HandleSquadSpawnedActor(SpawnedSquadActor))
	{
		return;
	}

	TryCompleteTowedTeamWeaponSpawnRequest(RequestId);
}

void AMissionManager::HandleSpawnTowedTeamWeaponSquadReady(const int32 RequestId)
{
	FMissionTowTeamWeaponSpawnState* TowSpawnState = FindTowedTeamWeaponSpawnState(RequestId);
	if (not EnsureValidTowedTeamWeaponSpawnState(TowSpawnState))
	{
		return;
	}

	if (not TowSpawnState->HandleSquadDataReady())
	{
		return;
	}

	TryCompleteTowedTeamWeaponSpawnRequest(RequestId);
}

void AMissionManager::HandleSpawnCargoVehicleTankSpawned(const int32 RequestId, AActor* SpawnedTankActor)
{
	FMissionCargoSquadWithVehicleSpawnState* CargoVehicleSpawnState = FindCargoVehicleSpawnState(RequestId);
	if (not EnsureValidCargoVehicleSpawnState(CargoVehicleSpawnState))
	{
		return;
	}

	if (not CargoVehicleSpawnState->HandleTankSpawnedActor(SpawnedTankActor))
	{
		return;
	}

	TryProgressCargoVehicleSpawnRequest(RequestId);
}

void AMissionManager::HandleSpawnCargoVehicleSquadSpawned(const int32 RequestId, AActor* SpawnedSquadActor)
{
	FMissionCargoSquadWithVehicleSpawnState* CargoVehicleSpawnState = FindCargoVehicleSpawnState(RequestId);
	if (not EnsureValidCargoVehicleSpawnState(CargoVehicleSpawnState))
	{
		return;
	}

	if (not CargoVehicleSpawnState->HandleSquadSpawnedActor(SpawnedSquadActor))
	{
		return;
	}

	TryProgressCargoVehicleSpawnRequest(RequestId);
}

void AMissionManager::HandleSpawnCargoVehicleSquadReady(const int32 RequestId)
{
	FMissionCargoSquadWithVehicleSpawnState* CargoVehicleSpawnState = FindCargoVehicleSpawnState(RequestId);
	if (not EnsureValidCargoVehicleSpawnState(CargoVehicleSpawnState))
	{
		return;
	}

	if (not CargoVehicleSpawnState->HandleSquadDataReady())
	{
		return;
	}

	TryProgressCargoVehicleSpawnRequest(RequestId);
}

void AMissionManager::HandleSpawnActorWithCommandQueueSpawned(const int32 RequestId, AActor* SpawnedActor)
{
	FMissionSpawnCommandQueueState* SpawnCommandQueueState = FindSpawnCommandQueueState(RequestId);
	if (not EnsureValidSpawnCommandQueueState(SpawnCommandQueueState))
	{
		return;
	}

	if (not SpawnCommandQueueState->HandleSpawnedActor(SpawnedActor))
	{
		return;
	}

	TryTickSpawnCommandQueueRequest(RequestId);
}

void AMissionManager::BeginPlay()
{
	Super::BeginPlay();
	M_CompletedMissionClasses.Empty();
	BeginPlay_InitPlayerController();
	BeginPlay_InitMissionScheduler();
	BeginPlay_InitMissionTriggerVolumesManager();
	BeginPlay_InitMissionWidgetManager();
	BeginPlay_InitGameDifficultyAndSettings();

	// Start all configured missions.
	for (UMissionBase* Mission : Missions)
	{
		ActivateNewMission(Mission);
	}
	Missions.Empty();
}

void AMissionManager::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if(PlayerFactionBackupData.bSetFactionManually)
	{
		URTSGameInstance* GameInstance =  FRTS_Statics::GetRTSGameInstance(this);
		if(not GameInstance)
		{
			return;
		}
		GameInstance->SetPlayerFaction(PlayerFactionBackupData.PlayerFaction);
	}
}

void AMissionManager::InitMissionSounds(const FMissionSoundSettings MissionSettings)
{
	M_MissionSoundSettings = MissionSettings;
}

void AMissionManager::SetMissionManagerWidgetClass(TSubclassOf<UW_MissionWidgetManager> WidgetClass)
{
	M_WidgetManagerClass = WidgetClass;
}

void AMissionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<TWeakObjectPtr<AActor>, int32>& TrackedActorPair : M_TrackedEnemyActorRefCounts)
	{
		AActor* TrackedActor = TrackedActorPair.Key.Get();
		if (not IsValid(TrackedActor))
		{
			continue;
		}

		TrackedActor->OnDestroyed.RemoveDynamic(this, &AMissionManager::OnTrackedEnemyActorDestroyed);
	}

	M_TrackedEnemyActorRefCounts.Empty();
	M_EnemyUnitDestroyedCallbacks.Empty();
	M_CompletedMissionClasses.Empty();

	for (UMissionBase* Mission : M_ActiveMissions)
	{
		if (not EnsureMissionIsValid(Mission))
		{
			continue;
		}

		Mission->OnCleanUpMission();
	}

	M_ActiveMissions.Empty();
	Super::EndPlay(EndPlayReason);
}

void AMissionManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (int32 MissionIndex = M_ActiveMissions.Num() - 1; MissionIndex >= 0; --MissionIndex)
	{
		UMissionBase* Mission = M_ActiveMissions[MissionIndex];
		if (not EnsureMissionIsValid(Mission))
		{
			continue;
		}
		if (Mission->GetCanCheckTrigger())
		{
			Mission->MissionTrigger->TickTrigger();
		}
		if (Mission->GetCanTickMission())
		{
			Mission->TickMission(DeltaSeconds);
		}
	}
	TickTowSpawnRequests();
	TickCargoVehicleSpawnRequests();
	TickSpawnCommandQueueRequests();
	RemoveFinishedTowSpawnRequests();
	RemoveFinishedCargoVehicleSpawnRequests();
	RemoveFinishedSpawnCommandQueueRequests();
	RemoveCompletedEnemyUnitDestroyedCallbacks();
}

void AMissionManager::InitMissionManagerWidget()
{
	// Create widget class and add to viewport.
	M_MissionWidgetManager = CreateWidget<UW_MissionWidgetManager>(
		M_PlayerController.Get(), M_WidgetManagerClass);
	if (not IsValid(M_MissionWidgetManager))
	{
		OnCouldNotInitWidgetManager();
		return;
	}
	M_MissionWidgetManager->AddToViewport(10);
	ProvideMissionWidgetToMainGameUI();
}

void AMissionManager::OnCouldNotInitWidgetManager() const
{
	RTSFunctionLibrary::ReportError("At InitMissionManagerWidget the mission widget manager could not be initialized."
		"\n Reported by mission manager."
		"\n See AMissionManager::InitMissionManagerWidget");
}

void AMissionManager::BeginPlay_InitPlayerController()
{
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(this);
	if (not PlayerController)
	{
		RTSFunctionLibrary::ReportError("Could not get player controller in mission manager.");
		return;
	}
	M_PlayerController = PlayerController;
}

void AMissionManager::BeginPlay_InitMissionWidgetManager()
{
	if (not EnsureValidPlayerController())
	{
		return;
	}
	InitMissionManagerWidget();
}

void AMissionManager::BeginPlay_InitGameDifficultyAndSettings()
{
	SetCampaignGenerationSettingsWithGameInstance();
	if (not bSetGameDifficultyWithWidget)
	{
		// The Game instance will determine the difficulty set.
		SetGameDifficultyWithGameInstance();
		return;
	}
	
	if (not EnsureValidPlayerController())
	{
		return;
	}

	if (not M_GameDifficultyPickerWidgetClass)
	{
		RTSFunctionLibrary::ReportError(
			"Game difficulty widget class is not set on the mission manager.");
		return;
	}

	const int32 DifficultyWidgetZOrder = 100;
	UW_GameDifficultyPicker* DifficultyPickerWidget = CreateWidget<UW_GameDifficultyPicker>(
		M_PlayerController.Get(), M_GameDifficultyPickerWidgetClass);
	if (not IsValid(DifficultyPickerWidget))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to create game difficulty picker widget on mission manager.");
		return;
	}

	DifficultyPickerWidget->AddToViewport(DifficultyWidgetZOrder);
}

void AMissionManager::BeginPlay_InitMissionScheduler()
{
	M_MissionScheduler = NewObject<UMissionScheduler>(this);
	if (not IsValid(M_MissionScheduler))
	{
		RTSFunctionLibrary::ReportError("Failed to create MissionScheduler");
		return;
	}

	M_MissionScheduler->RegisterComponent();
}

void AMissionManager::BeginPlay_InitMissionTriggerVolumesManager()
{
	GetIsValidMissionTriggerVolumesManager();
}

bool AMissionManager::EnsureValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Player controller is not valid in mission manager.");
	return false;
}

bool AMissionManager::GetIsValidMissionScheduler() const
{
	if (IsValid(M_MissionScheduler))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_MissionScheduler",
		"GetIsValidMissionScheduler",
		this
	);
	return false;
}

bool AMissionManager::GetIsValidMissionTriggerVolumesManager() const
{
	if (IsValid(M_MissionTriggerVolumesManager))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_MissionTriggerVolumesManager",
		"GetIsValidMissionTriggerVolumesManager",
		this
	);
	return false;
}

void AMissionManager::SetCampaignGenerationSettingsWithGameInstance()
{
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not GameInstance)
	{
		RTSFunctionLibrary::ReportError("Could not get game instance in mission manager.");
		return;
	}
	M_CampaignGenerationSettings = GameInstance->GetCampaignGenerationSettings();
	if(not M_CampaignGenerationSettings.bAreSettingsLoaded)
	{
		RTSFunctionLibrary::ReportError("Game instance has no valid Campaign generattion settings loaded while"
								  "the mission manager requested it!"
		  "\n see AMissionManager::SetCampaignGenerationSettingsWithGameInstance");
	}
}

void AMissionManager::SetGameDifficultyWithGameInstance()
{
	const URTSGameInstance* GameInstance = FRTS_Statics::GetRTSGameInstance(this);
	if (not GameInstance)
	{
		RTSFunctionLibrary::ReportError("Could not get game instance in mission manager.");
		return;
	}
	const FRTSGameDifficulty GameDifficulty = GameInstance->GetSelectedGameDifficulty();
	if (not GameDifficulty.bIsInitialized)
	{
		RTSFunctionLibrary::ReportError("Game instance has no valid game difficulty loaded while"
								  "the mission manager requested it!"
		  "\n see AMissionManager::SetGameDifficultyWithGameInstance");
	}
	M_GameDifficulty = GameDifficulty;
}

void AMissionManager::RemoveActiveMission(UMissionBase* Mission)
{
	if (not EnsureMissionIsValid(Mission))
	{
		return;
	}
	if (not M_ActiveMissions.Contains(Mission))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to remove active mission that is not registered as active on the manager"
			"\n Mission : " + Mission->GetName());
	}
	M_ActiveMissions.Remove(Mission);
}

bool AMissionManager::EnsureMissionIsValid(UMissionBase* Mission)
{
	if (not IsValid(Mission))
	{
		RTSFunctionLibrary::ReportError("A mission is not valid for the mission manager.");
		return false;
	}
	return true;
}

bool AMissionManager::EnsureMissionWidgetIsValid() const
{
	if (not IsValid(M_MissionWidgetManager))
	{
		RTSFunctionLibrary::ReportError("Mission widget manager is not valid in mission manager.");
		return false;
	}
	return true;
}

bool AMissionManager::EnsureMissionIsNotAlreadyActivated(UMissionBase* Mission)
{
	if (not EnsureMissionIsValid(Mission))
	{
		return false;
	}
	if (M_ActiveMissions.Contains(Mission))
	{
		RTSFunctionLibrary::ReportError("Mission already activated.");
		return false;
	}
	return true;
}

void AMissionManager::PlayMissionSound(const EMissionSoundType SoundType)
{
	switch (SoundType)
	{
	case EMissionSoundType::None:
		return;
	case EMissionSoundType::MissionLoaded:
		PlayMissionLoaded();
		break;
	case EMissionSoundType::MissionCompleted:
		PlayMissionCompleted();
		break;
	case EMissionSoundType::MissionFailed:
		PlayMissionFailed();
		break;
	}
}

void AMissionManager::PlayMissionLoaded()
{
	USoundCue* MissionLoadedSound = M_MissionSoundSettings.MissionLoadedSound;
	if (not IsValid(MissionLoadedSound))
	{
		RTSFunctionLibrary::ReportError("NO mission loaded sound set on mission manager");
		return;
	}
	UGameplayStatics::PlaySound2D(this, MissionLoadedSound, 1, 1, 0,
	                              M_MissionSoundSettings.MissionSoundConcurrency);
}

void AMissionManager::PlayMissionCompleted()
{
	USoundCue* MissionCompletedSound = M_MissionSoundSettings.MissionCompleteSound;
	if (not IsValid(MissionCompletedSound))
	{
		RTSFunctionLibrary::ReportError("NO mission completed sound set on mission manager");
		return;
	}
	UGameplayStatics::PlaySound2D(this, MissionCompletedSound, 1, 1, 0,
	                              M_MissionSoundSettings.MissionSoundConcurrency);
}

void AMissionManager::PlayMissionFailed()
{
	USoundCue* MissionFailedSound = M_MissionSoundSettings.MissionFailedSound;
	if (not IsValid(MissionFailedSound))
	{
		RTSFunctionLibrary::ReportError("NO mission failed sound set on mission manager");
		return;
	}
	UGameplayStatics::PlaySound2D(this, MissionFailedSound, 1, 1, 0,
	                              M_MissionSoundSettings.MissionSoundConcurrency);
}

void AMissionManager::ProvideMissionWidgetToMainGameUI() const
{
	UMainGameUI* MainGameUI = FRTS_Statics::GetMainGameUI(this);
	if (not MainGameUI)
	{
		return;
	}
	MainGameUI->SetMissionManagerWidget(M_MissionWidgetManager);
}

FMissionTowTeamWeaponSpawnState* AMissionManager::FindTowedTeamWeaponSpawnState(const int32 RequestId)
{
	for (FMissionTowTeamWeaponSpawnState& TowSpawnState : M_TowedTeamWeaponSpawnStates)
	{
		if (TowSpawnState.GetBelongsToRequest(RequestId))
		{
			return &TowSpawnState;
		}
	}
	return nullptr;
}

FMissionCargoSquadWithVehicleSpawnState* AMissionManager::FindCargoVehicleSpawnState(const int32 RequestId)
{
	for (FMissionCargoSquadWithVehicleSpawnState& CargoVehicleSpawnState : M_CargoVehicleSpawnStates)
	{
		if (CargoVehicleSpawnState.GetBelongsToRequest(RequestId))
		{
			return &CargoVehicleSpawnState;
		}
	}

	return nullptr;
}

FMissionSpawnCommandQueueState* AMissionManager::FindSpawnCommandQueueState(const int32 RequestId)
{
	for (FMissionSpawnCommandQueueState& SpawnCommandQueueState : M_SpawnCommandQueueStates)
	{
		if (SpawnCommandQueueState.GetBelongsToRequest(RequestId))
		{
			return &SpawnCommandQueueState;
		}
	}
	return nullptr;
}

void AMissionManager::TryCompleteTowedTeamWeaponSpawnRequest(const int32 RequestId)
{
	FMissionTowTeamWeaponSpawnState* TowSpawnState = FindTowedTeamWeaponSpawnState(RequestId);
	if (not EnsureValidTowedTeamWeaponSpawnState(TowSpawnState))
	{
		return;
	}

	TowSpawnState->TryFinishInstantTow();
}

void AMissionManager::TryProgressCargoVehicleSpawnRequest(const int32 RequestId)
{
	FMissionCargoSquadWithVehicleSpawnState* CargoVehicleSpawnState = FindCargoVehicleSpawnState(RequestId);
	if (not EnsureValidCargoVehicleSpawnState(CargoVehicleSpawnState))
	{
		return;
	}

	CargoVehicleSpawnState->TryProgressState();
}

void AMissionManager::TryTickSpawnCommandQueueRequest(const int32 RequestId)
{
	FMissionSpawnCommandQueueState* SpawnCommandQueueState = FindSpawnCommandQueueState(RequestId);
	if (not EnsureValidSpawnCommandQueueState(SpawnCommandQueueState))
	{
		return;
	}
	SpawnCommandQueueState->TickExecution();
}

void AMissionManager::RemoveFinishedTowSpawnRequests()
{
	M_TowedTeamWeaponSpawnStates.RemoveAll([](const FMissionTowTeamWeaponSpawnState& TowSpawnState)->bool
	{
		return TowSpawnState.GetIsFinished();
	});
}

void AMissionManager::RemoveFinishedCargoVehicleSpawnRequests()
{
	M_CargoVehicleSpawnStates.RemoveAll([](const FMissionCargoSquadWithVehicleSpawnState& CargoVehicleSpawnState)->bool
	{
		return CargoVehicleSpawnState.GetIsFinished();
	});
}

void AMissionManager::RemoveFinishedSpawnCommandQueueRequests()
{
	M_SpawnCommandQueueStates.RemoveAll([](const FMissionSpawnCommandQueueState& SpawnCommandQueueState)->bool
	{
		return SpawnCommandQueueState.GetIsFinished();
	});
}

void AMissionManager::TickTowSpawnRequests()
{
	for (FMissionTowTeamWeaponSpawnState& TowSpawnState : M_TowedTeamWeaponSpawnStates)
	{
		if (TowSpawnState.GetIsFinished())
		{
			continue;
		}

		TowSpawnState.TryFinishInstantTow();
	}
}

void AMissionManager::TickCargoVehicleSpawnRequests()
{
	for (FMissionCargoSquadWithVehicleSpawnState& CargoVehicleSpawnState : M_CargoVehicleSpawnStates)
	{
		if (CargoVehicleSpawnState.GetIsFinished())
		{
			continue;
		}

		CargoVehicleSpawnState.TryProgressState();
	}
}

void AMissionManager::TickSpawnCommandQueueRequests()
{
	for (FMissionSpawnCommandQueueState& SpawnCommandQueueState : M_SpawnCommandQueueStates)
	{
		if (SpawnCommandQueueState.GetIsFinished())
		{
			continue;
		}
		SpawnCommandQueueState.TickExecution();
	}
}

bool AMissionManager::EnsureValidTowedTeamWeaponSpawnState(FMissionTowTeamWeaponSpawnState* TowSpawnState) const
{
	if (TowSpawnState != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission manager could not find requested tow spawn state.");
	return false;
}

bool AMissionManager::EnsureValidCargoVehicleSpawnState(
	FMissionCargoSquadWithVehicleSpawnState* CargoVehicleSpawnState) const
{
	if (CargoVehicleSpawnState != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission manager could not find requested cargo vehicle spawn state.");
	return false;
}

bool AMissionManager::EnsureValidSpawnCommandQueueState(FMissionSpawnCommandQueueState* SpawnCommandQueueState) const
{
	if (SpawnCommandQueueState != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission manager could not find requested spawn command queue state.");
	return false;
}

bool AMissionManager::EnsureValidTowAsyncSpawner(ARTSAsyncSpawner* RTSAsyncSpawner) const
{
	if (IsValid(RTSAsyncSpawner))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission manager failed SpawnTowedTeamWeapon because async spawner is invalid.");
	return false;
}

bool AMissionManager::EnsureValidSpawnCommandQueueAsyncSpawner(ARTSAsyncSpawner* RTSAsyncSpawner) const
{
	if (IsValid(RTSAsyncSpawner))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"Mission manager failed SpawnActorAtLocationWithCommandQueue because async spawner is invalid.");
	return false;
}

bool AMissionManager::EnsureEnemyControllerIsValid(AEnemyController* EnemyController) const
{
	if (IsValid(EnemyController))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission manager could not fetch a valid enemy controller.");
	return false;
}

TArray<AActor*> AMissionManager::GetEnemyActorsForQueryType(const EEnemyUnitQueryType EnemyUnitQueryType) const
{
	TArray<AActor*> EnemyActors;
	AEnemyController* EnemyController = FRTS_Statics::GetEnemyController(this);
	if (not EnsureEnemyControllerIsValid(EnemyController))
	{
		return EnemyActors;
	}

	if (EnemyUnitQueryType == EEnemyUnitQueryType::TrackTanks)
	{
		return EnemyController->GetAllTankActorsInFormations();
	}

	if (EnemyUnitQueryType == EEnemyUnitQueryType::TrackSquads)
	{
		return EnemyController->GetAllSquadControllerActorsInFormations();
	}

	if (EnemyUnitQueryType == EEnemyUnitQueryType::TrackTanksAndSquads)
	{
		EnemyActors = EnemyController->GetAllTankActorsInFormations();
		EnemyActors.Append(EnemyController->GetAllSquadControllerActorsInFormations());
		return EnemyActors;
	}

	return EnemyActors;
}

void AMissionManager::RegisterTrackedEnemyActor(AActor* EnemyActor)
{
	if (not RTSFunctionLibrary::RTSIsValid(EnemyActor))
	{
		return;
	}

	TWeakObjectPtr<AActor> WeakEnemyActor = EnemyActor;
	const int32 ExistingRefCount = M_TrackedEnemyActorRefCounts.FindRef(WeakEnemyActor);
	if (ExistingRefCount > 0)
	{
		M_TrackedEnemyActorRefCounts.Add(WeakEnemyActor, ExistingRefCount + 1);
		return;
	}

	EnemyActor->OnDestroyed.AddDynamic(this, &AMissionManager::OnTrackedEnemyActorDestroyed);
	M_TrackedEnemyActorRefCounts.Add(WeakEnemyActor, 1);
}

void AMissionManager::UnregisterTrackedEnemyActor(AActor* EnemyActor)
{
	if (EnemyActor == nullptr)
	{
		return;
	}

	TWeakObjectPtr<AActor> WeakEnemyActor = EnemyActor;
	const int32 ExistingRefCount = M_TrackedEnemyActorRefCounts.FindRef(WeakEnemyActor);
	if (ExistingRefCount <= 1)
	{
		M_TrackedEnemyActorRefCounts.Remove(WeakEnemyActor);
		if (IsValid(EnemyActor))
		{
			EnemyActor->OnDestroyed.RemoveDynamic(this, &AMissionManager::OnTrackedEnemyActorDestroyed);
		}
		return;
	}

	M_TrackedEnemyActorRefCounts.Add(WeakEnemyActor, ExistingRefCount - 1);
}

void AMissionManager::RemoveCompletedEnemyUnitDestroyedCallbacks()
{
	for (int32 CallbackIndex = M_EnemyUnitDestroyedCallbacks.Num() - 1; CallbackIndex >= 0; --CallbackIndex)
	{
		FMissionEnemyUnitDestroyedCallbackState& CallbackState = M_EnemyUnitDestroyedCallbacks[CallbackIndex];
		CallbackState.TryNotifyMission();
		if (not CallbackState.GetShouldCleanup())
		{
			continue;
		}

		for (AActor* TrackedActor : CallbackState.GetTrackedActorsForCleanup())
		{
			UnregisterTrackedEnemyActor(TrackedActor);
		}

		M_EnemyUnitDestroyedCallbacks.RemoveAtSwap(CallbackIndex);
	}
}

void AMissionManager::OnTrackedEnemyActorDestroyed(AActor* DestroyedActor)
{
	for (FMissionEnemyUnitDestroyedCallbackState& CallbackState : M_EnemyUnitDestroyedCallbacks)
	{
		if (not CallbackState.GetIsTrackingActor(DestroyedActor))
		{
			continue;
		}

		CallbackState.OnTrackedActorDestroyed(DestroyedActor);
	}

	UnregisterTrackedEnemyActor(DestroyedActor);
	RemoveCompletedEnemyUnitDestroyedCallbacks();
}

FTrainingOption AMissionManager::SelectSeededTankOption(const TArray<ETankSubtype>& TankOptions) const
{
	FTrainingOption Option = FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(ETankSubtype::Tank_T26));
    // Ensure the array is not empty to avoid division by zero
    if (TankOptions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TankOptions array is empty. Returning default FTrainingOptions."));
    	return Option;
    }

    // Use the GenerationSeed to determine the index
    int32 Index = M_CampaignGenerationSettings.GenerationSeed % TankOptions.Num();

    // Return the selected option
	Option.SubtypeValue = static_cast<uint8>(TankOptions[Index]);
	return Option;
}

FTrainingOption AMissionManager::SelectSeededSquadOption(const TArray<ESquadSubtype>& SquadOptions) const
{
	
	FTrainingOption Option = FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(ESquadSubtype::Squad_Rus_HazmatEngineers));
    // Ensure the array is not empty to avoid division by zero
    if (SquadOptions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SquadOptions array is empty. Returning default FTrainingOptions."));
    	return Option;
    }

    // Use the GenerationSeed to determine the index
    int32 Index = M_CampaignGenerationSettings.GenerationSeed % SquadOptions.Num();

    // Return the selected option
	Option.SubtypeValue = static_cast<uint8>(SquadOptions[Index]);
	return Option;
}

void AMissionManager::SpawnSeededChoiceGroups(const TArray<FSeededChoices>& SeededChoicesArray, UObject* WorldContextObject)
{
	if (SeededChoicesArray.IsEmpty())
	{
		return;
	}

	for (int32 GroupIndex = 0; GroupIndex < SeededChoicesArray.Num(); ++GroupIndex)
	{
		const FSeededChoices& SeededChoicesGroup = SeededChoicesArray[GroupIndex];
		if (SeededChoicesGroup.Choices.IsEmpty())
		{
			continue;
		}

		const int32 SelectedChoiceIndex = GetSeededChoiceIndex(SeededChoicesGroup.Choices, GroupIndex);
		if (not SeededChoicesGroup.Choices.IsValidIndex(SelectedChoiceIndex))
		{
			continue;
		}

		const FSeededSpawnChoice& SelectedChoice = SeededChoicesGroup.Choices[SelectedChoiceIndex];
		if (not GetIsSeededChoiceConfigured(SelectedChoice))
		{
			RTSFunctionLibrary::ReportError(
				"Mission manager selected a seeded choice with empty TrainingOptionSpawns and SoftActorSpawns."
			);
			continue;
		}

		SpawnSeededChoice(SelectedChoice, WorldContextObject);
	}
}

void AMissionManager::SpawnSeededChoice(const FSeededSpawnChoice& SeededChoice, UObject* WorldContextObject)
{
	SpawnSeededChoiceTrainingOptions(SeededChoice.TrainingOptionSpawns, WorldContextObject);
	SpawnSeededChoiceSoftActors(SeededChoice.SoftActorSpawns, WorldContextObject);
}

void AMissionManager::SpawnSeededChoiceTrainingOptions(
	const TArray<FSeededSpawnTrainingOptionEntry>& TrainingOptionSpawns,
	UObject* WorldContextObject)
{
	if (TrainingOptionSpawns.IsEmpty())
	{
		return;
	}

	ARTSAsyncSpawner* RTSAsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not EnsureValidTowAsyncSpawner(RTSAsyncSpawner))
	{
		return;
	}

	UObject* SpawnOwner = IsValid(WorldContextObject) ? WorldContextObject : this;
	for (const FSeededSpawnTrainingOptionEntry& TrainingOptionSpawn : TrainingOptionSpawns)
	{
		if (TrainingOptionSpawn.TrainingOption.IsNone())
		{
			continue;
		}

		constexpr int32 SpawnRequestID = INDEX_NONE;
		const TFunction<void(const FTrainingOption&, AActor*, const int32)> EmptyCallback;
		RTSAsyncSpawner->AsyncSpawnOptionAtLocation(
			TrainingOptionSpawn.TrainingOption,
			TrainingOptionSpawn.SpawnLocation,
			SpawnOwner,
			SpawnRequestID,
			EmptyCallback
		);
	}
}

void AMissionManager::SpawnSeededChoiceSoftActors(
	const TArray<FSeededSpawnSoftActorEntry>& SoftActorSpawns,
	UObject* WorldContextObject)
{
	if (SoftActorSpawns.IsEmpty())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	UObject* SpawnOwnerObject = IsValid(WorldContextObject) ? WorldContextObject : this;
	AActor* SpawnOwnerActor = Cast<AActor>(SpawnOwnerObject);
	if (not IsValid(SpawnOwnerActor))
	{
		SpawnOwnerActor = this;
	}

	for (const FSeededSpawnSoftActorEntry& SoftActorSpawn : SoftActorSpawns)
	{
		if (SoftActorSpawn.ActorClass.IsNull())
		{
			continue;
		}

		const TSoftClassPtr<AActor> ActorClassToLoad = SoftActorSpawn.ActorClass;
		const FVector SpawnLocation = SoftActorSpawn.SpawnLocation;
		const TWeakObjectPtr<AMissionManager> WeakMissionManager = this;
		const TWeakObjectPtr<AActor> WeakSpawnOwner = SpawnOwnerActor;
		TSharedPtr<FStreamableHandle> LoadingHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			ActorClassToLoad.ToSoftObjectPath(),
			FStreamableDelegate::CreateLambda([WeakMissionManager, WeakSpawnOwner, ActorClassToLoad, SpawnLocation]()
			{
				if (not WeakMissionManager.IsValid())
				{
					return;
				}

				UWorld* MissionWorld = WeakMissionManager->GetWorld();
				if (not MissionWorld)
				{
					return;
				}

				UClass* LoadedClass = ActorClassToLoad.Get();
				if (not IsValid(LoadedClass))
				{
					return;
				}

				FActorSpawnParameters SpawnParameters;
				SpawnParameters.Owner = WeakSpawnOwner.IsValid() ? WeakSpawnOwner.Get() : WeakMissionManager.Get();
				SpawnParameters.SpawnCollisionHandlingOverride =
					ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				MissionWorld->SpawnActor<AActor>(
					LoadedClass,
					SpawnLocation,
					FRotator::ZeroRotator,
					SpawnParameters
				);
			})
		);

		if (LoadingHandle.IsValid())
		{
			M_SeededSpawnAssetLoadHandles.Add(LoadingHandle);
		}
	}
}

int32 AMissionManager::GetSeededChoiceIndex(const TArray<FSeededSpawnChoice>& Choices, const int32 GroupIndex) const
{
	if (Choices.IsEmpty())
	{
		return INDEX_NONE;
	}

	const int32 CampaignSeed = GetGenerationSeed();
	const uint32 SeedHash = HashCombineFast(
		static_cast<uint32>(CampaignSeed),
		static_cast<uint32>(GroupIndex + 1)
	);
	return static_cast<int32>(SeedHash % static_cast<uint32>(Choices.Num()));
}

bool AMissionManager::GetIsSeededChoiceConfigured(const FSeededSpawnChoice& SeededChoice) const
{
	return SeededChoice.TrainingOptionSpawns.Num() > 0 || SeededChoice.SoftActorSpawns.Num() > 0;
}


void AMissionManager::TriggerDefeat(const ERTSDefeatType DefeatType)
{
	if (DefeatType == ERTSDefeatType::Uninitialized)
	{
		RTSFunctionLibrary::ReportError("Mission manager received TriggerDefeat with Uninitialized defeat type.");
		return;
	}

	if (DefeatType == ERTSDefeatType::LostHQ)
	{
		TriggerDefeat_LostHQ();
		return;
	}

	TriggerDefeat_LockPauseAndShowWidget();
}

void AMissionManager::TriggerDefeat_LostHQ()
{
	if (not EnsureValidPlayerController())
	{
		return;
	}

	const float AnnouncerDelaySeconds = FMath::Max(
		0.0f,
		M_PlayerController->PlayAnnouncerVoiceLine(EAnnouncerVoiceLineType::LostHQ, true, true)
	);
	if (AnnouncerDelaySeconds <= KINDA_SMALL_NUMBER)
	{
		TriggerDefeat_LockPauseAndShowWidget();
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("Mission manager failed TriggerDefeat_LostHQ because world is invalid.");
		return;
	}

	const TWeakObjectPtr<AMissionManager> WeakMissionManager = this;
	FTimerDelegate DefeatTimerDelegate;
	DefeatTimerDelegate.BindLambda([WeakMissionManager]()
	{
		if (not WeakMissionManager.IsValid())
		{
			return;
		}

		WeakMissionManager->TriggerDefeat_LockPauseAndShowWidget();
	});

	FTimerHandle DefeatDelayTimerHandle;
	World->GetTimerManager().SetTimer(
		DefeatDelayTimerHandle,
		DefeatTimerDelegate,
		AnnouncerDelaySeconds,
		false
	);
}

void AMissionManager::TriggerDefeat_LockPauseAndShowWidget()
{
	if (not EnsureValidPlayerController())
	{
		return;
	}

	M_PlayerController->PauseAndLockGame(true);
	TriggerDefeat_ShowDefeatWidget();
}

void AMissionManager::TriggerDefeat_ShowDefeatWidget()
{
	if (not EnsureValidPlayerController())
	{
		return;
	}

	if (not M_DefeatWidget)
	{
		if (not GetIsValidDefeatWidgetClass())
		{
			return;
		}

		UW_Defeat* CreatedDefeatWidget = CreateWidget<UW_Defeat>(M_PlayerController.Get(), M_DefeatWidgetClass);
		if (not IsValid(CreatedDefeatWidget))
		{
			RTSFunctionLibrary::ReportError("Mission manager failed to create defeat widget from M_DefeatWidgetClass.");
			return;
		}

		M_DefeatWidget = CreatedDefeatWidget;
	}

	if (M_DefeatWidget->IsInViewport())
	{
		return;
	}

	constexpr int32 DefeatWidgetZOrder = 300;
	M_DefeatWidget->AddToViewport(DefeatWidgetZOrder);
}

bool AMissionManager::GetIsValidDefeatWidgetClass() const
{
	if (M_DefeatWidgetClass)
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_DefeatWidgetClass",
		"GetIsValidDefeatWidgetClass",
		this
	);
	return false;
}

int32 AMissionManager::GetGenerationSeed() const
{
	return M_CampaignGenerationSettings.GenerationSeed;
}
