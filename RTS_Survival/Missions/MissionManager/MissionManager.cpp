#include "MissionManager.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Missions/MissionWidgets/W_MissionTimer.h"
#include "RTS_Survival/Missions/MissionWidgets/MissionWidgetManager/W_MissionWidgetManager.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Sound/SoundCue.h"

AMissionManager::AMissionManager()
{
	// Set to true to tick missions.
	PrimaryActorTick.bCanEverTick = true;
}

void AMissionManager::OnAnyMissionCompleted(UMissionBase* CompletedMission, const bool bPlaySound)
{
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

void AMissionManager::ActivateNewMission(UMissionBase* NewMission)
{
	if (not EnsureMissionIsValid(NewMission))
	{
		return;
	}
	M_ActiveMissions.Add(NewMission);
	NewMission->LoadMission(this);
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

void AMissionManager::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitPlayerController();
	BeginPlay_InitMissionScheduler();
	BeginPlay_InitMissionWidgetManager();
	BeginPlay_InitGameDifficultyPickerWidget();

	// Start all configured missions.
	for (UMissionBase* Mission : Missions)
	{
		ActivateNewMission(Mission);
	}
	Missions.Empty();
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
	RemoveFinishedTowSpawnRequests();
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

void AMissionManager::BeginPlay_InitGameDifficultyPickerWidget()
{
	if (not bSetGameDifficultyWithWidget)
	{
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

void AMissionManager::TryCompleteTowedTeamWeaponSpawnRequest(const int32 RequestId)
{
	FMissionTowTeamWeaponSpawnState* TowSpawnState = FindTowedTeamWeaponSpawnState(RequestId);
	if (not EnsureValidTowedTeamWeaponSpawnState(TowSpawnState))
	{
		return;
	}
	TowSpawnState->TryFinishInstantTow();
}

void AMissionManager::RemoveFinishedTowSpawnRequests()
{
	M_TowedTeamWeaponSpawnStates.RemoveAll([](const FMissionTowTeamWeaponSpawnState& TowSpawnState)->bool
	{
		return TowSpawnState.GetIsFinished();
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

bool AMissionManager::EnsureValidTowedTeamWeaponSpawnState(FMissionTowTeamWeaponSpawnState* TowSpawnState) const
{
	if (TowSpawnState != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("Mission manager could not find requested tow spawn state.");
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
