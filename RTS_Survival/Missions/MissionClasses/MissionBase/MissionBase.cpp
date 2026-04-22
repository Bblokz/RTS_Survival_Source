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
#include "RTS_Survival/Missions/TriggerAreas/TriggerArea.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Missions/MissionWidgets/W_MissionTimer.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Player/PlayerAudioController/PlayerAudioController.h"
#include "RTS_Survival/Player/PortraitManager/PortraitManager.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
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
	RemoveAllTriggerAreas();
	MissionState.bIsMissionComplete = true;
	// Stop any tick behaviour.
	MissionState.bTickOnMission = false;
	Tracking_ClearState();
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
	Tracking_ClearState();
	DebugMission("Failed mission " + GetName());
	if (GetMissionManagerChecked())
	{
		M_MissionManager->OnAnyMissionFailed(this);
	}
	// Mark the widget as free to be able to be used by another mission.
	RemoveAllTriggerAreas();
	MarkWidgetAsFree();
	BP_OnMissionFailed();
}

void UMissionBase::StartNextMissionIgnoringTrigger()
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	if (not GetHasConfiguredNextMission())
	{
		return;
	}

	NextMission->bM_IgnoreTriggerOnMissionStart = true;
	GetMissionManagerChecked()->ActivateNewMission(NextMission);
}

void UMissionBase::TriggerMissionFromArray(const int32 TriggerableMissionIndex)
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	if (not GetHasValidTriggerableMissionIndex(TriggerableMissionIndex))
	{
		return;
	}

	UMissionBase* MissionToTrigger = TriggerableMissions[TriggerableMissionIndex];
	MissionToTrigger->bM_IgnoreTriggerOnMissionStart = true;
	GetMissionManagerChecked()->ActivateNewMission(MissionToTrigger);
}

void UMissionBase::OnEnemyUnitsDestroyedCallback(const int32 ID, const EEnemyUnitQueryType EnemyUnitQueryType)
{
	BP_OnCallBackEnemyActorsDestroyed(ID, EnemyUnitQueryType);
}

int32 UMissionBase::DoesPlayerHaveAnySquadsOfType(const TArray<ESquadSubtype>& SquadTypes) const
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("Mission failed checking squads by type because game unit manager is invalid.");
		return 0;
	}

	constexpr uint8 PlayerTeamId = 1;
	return GameUnitManager->GetPlayerSquadCountOfTypes(PlayerTeamId, SquadTypes);
}

int32 UMissionBase::DoesPlayerHaveAnyTankMastersOfType(const TArray<ETankSubtype>& TankTypes) const
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("Mission failed checking tanks by type because game unit manager is invalid.");
		return 0;
	}

	constexpr uint8 PlayerTeamId = 1;
	return GameUnitManager->GetPlayerTankCountOfTypes(PlayerTeamId, TankTypes);
}

int32 UMissionBase::DoesPlayerHaveAnyBuildingExpansionsOfType(
	const TArray<EBuildingExpansionType>& BuildingExpansionTypes) const
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("Mission failed checking building expansions by type because game unit manager is invalid.");
		return 0;
	}

	constexpr uint8 PlayerTeamId = 1;
	return GameUnitManager->GetPlayerBxpCountOfTypes(PlayerTeamId, BuildingExpansionTypes);
}

int32 UMissionBase::DoesPlayerHaveAnyAircraftMastersOfType(const TArray<EAircraftSubtype>& AircraftTypes) const
{
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		RTSFunctionLibrary::ReportError("Mission failed checking aircraft by type because game unit manager is invalid.");
		return 0;
	}

	constexpr uint8 PlayerTeamId = 1;
	return GameUnitManager->GetPlayerAircraftCountOfTypes(PlayerTeamId, AircraftTypes);
}

ATriggerArea* UMissionBase::CreateTriggerAreaSphere(const FVector& Location,
                                                     const FRotator& Rotation,
                                                     const FVector& Scale,
                                                     const ETriggerOverlapLogic TriggerOverlapLogic,
                                                     const float DelayBetweenCallbacks,
                                                     const int32 MaxCallbacks,
                                                     const int32 TriggerId)
{
	if (not GetIsValidMissionManager())
	{
		return nullptr;
	}

	return GetMissionManagerChecked()->CreateMissionTriggerAreaSphere(
		this,
		Location,
		Rotation,
		Scale,
		TriggerOverlapLogic,
		DelayBetweenCallbacks,
		MaxCallbacks,
		TriggerId
	);
}

ATriggerArea* UMissionBase::CreateTriggerAreaRectangle(const FVector& Location,
                                                        const FRotator& Rotation,
                                                        const FVector& Scale,
                                                        const ETriggerOverlapLogic TriggerOverlapLogic,
                                                        const float DelayBetweenCallbacks,
                                                        const int32 MaxCallbacks,
                                                        const int32 TriggerId)
{
	if (not GetIsValidMissionManager())
	{
		return nullptr;
	}

	return GetMissionManagerChecked()->CreateMissionTriggerAreaRectangle(
		this,
		Location,
		Rotation,
		Scale,
		TriggerOverlapLogic,
		DelayBetweenCallbacks,
		MaxCallbacks,
		TriggerId
	);
}

void UMissionBase::RemoveTriggerAreasById(const int32 TriggerId)
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	GetMissionManagerChecked()->RemoveMissionTriggerAreasById(this, TriggerId);
}

void UMissionBase::RemoveAllTriggerAreas()
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	GetMissionManagerChecked()->RemoveAllMissionTriggerAreasForMission(this);
}

void UMissionBase::OnTriggerAreaCallback(AActor* OverlappingActor, const int32 TriggerID, ATriggerArea* TriggerVolume)
{
	BP_OnTriggerAreaCallback(OverlappingActor, TriggerID, TriggerVolume);
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
	Tracking_ClearState();
	if (GetIsValidMissionManager())
	{
		CancelAllScheduledCallbacks();
		RemoveAllTriggerAreas();
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
	GetWorld()->GetTimerManager().ClearTimer(M_TrackingValidityTimerHandle);
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
		GetWorld()->GetTimerManager().ClearTimer(M_TrackingValidityTimerHandle);
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

bool UMissionBase::GetIsMissionComplete() const
{
	return MissionState.bIsMissionComplete;
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

ERTSFaction UMissionBase::GetPlayerFaction() const
{
	if(not GetIsValidMissionManager())
	{
		return ERTSFaction::GerStrikeDivision;
	}
	return M_MissionManager->GetPlayerFaction();
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

bool UMissionBase::CreateSingleRandomPatrolWithAttackMoveDifficultyConditional(
	ERTSGameDifficulty MinimalDifficulty,
	TArray<FVector> SpawnLocations,
	TArray<FTrainingOption> TrainingOptions,
	TArray<FVector> PatrolPoints,
	const int32 OverrideFirstPatrolPointIndex,
	const int32 AmountIterationsAtPatrolPoint,
	const float GuardTimePerPatrolPointIteration,
	const float GuardSphereRadius,
	const int32 MaxFormationWidth,
	float TimeTillPatrol,
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
	if (TrainingOptions.IsEmpty() || SpawnLocations.IsEmpty() || PatrolPoints.Num() < 2)
	{
		RTSFunctionLibrary::ReportError(
			"Mission attempted to create random patrol with attack move but one of the required arrays is invalid!"
			"\n Mission : " + GetName());
		return false;
	}

	EnemyController->CreateSingleRandomPatrolWithAttackMoveWave(
		SpawnLocations,
		TrainingOptions,
		PatrolPoints,
		OverrideFirstPatrolPointIndex,
		AmountIterationsAtPatrolPoint,
		GuardTimePerPatrolPointIteration,
		GuardSphereRadius,
		MaxFormationWidth,
		TimeTillPatrol,
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

void UMissionBase::CallbackOnEnemyUnitsDestroyed(
	const EEnemyUnitQueryType EnemyUnitQueryType,
	const int32 ID)
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	GetMissionManagerChecked()->CallBackOnMissionWhenEnemyUnitsDestroyed(
		EnemyUnitQueryType,
		this,
		ID);
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

	if (bCinematicStarted)
	{
		if (UPlayerPortraitManager* PlayerPortraitManager = FRTS_Statics::GetPlayerPortraitManager(this))
		{
			PlayerPortraitManager->StopCurrentPortraitPlayback();
		}
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

void UMissionBase::TrackingFunction(const EMissionTrackingType TrackingType,
                                    const TArray<AActor*>& ActorsToTrack,
                                    const float ValidityCheckInterval,
                                    const FString& Prefix,
                                    const FString& Partitive,
                                    const FString& Postfix,
                                    const ERTSRichText RichCountType)
{
	Tracking_ClearState();

	if (TrackingType == EMissionTrackingType::NoTracking)
	{
		return;
	}

	M_MissionTrackingRuntimeState.TrackingType = TrackingType;
	M_MissionTrackingRuntimeState.Prefix = Prefix;
	M_MissionTrackingRuntimeState.Partitive = Partitive;
	M_MissionTrackingRuntimeState.Postfix = Postfix;
	M_MissionTrackingRuntimeState.RichCountType = RichCountType;
	M_MissionTrackingRuntimeState.MaxCount = ActorsToTrack.Num();
	M_MissionTrackingRuntimeState.CurrentCount = 0;

	if (not Tracking_ConfigureActors(TrackingType, ActorsToTrack))
	{
		Tracking_ClearState();
		return;
	}

	Tracking_ApplyWidgetTitleUpdate();

	if (Tracking_GetHasReachedMaxCount())
	{
		Tracking_OnReachedMaxCount();
		return;
	}

	Tracking_StartBackupValidityTimer(ValidityCheckInterval);
}

void UMissionBase::Tracking_ClearState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TrackingValidityTimerHandle);
	}

	M_TrackedActors.Empty();
	M_TrackingHasCountedInvalid.Empty();
	M_MissionTrackingRuntimeState = FMissionTrackingRuntimeState();
}

bool UMissionBase::Tracking_ConfigureActors(const EMissionTrackingType TrackingType, const TArray<AActor*>& ActorsToTrack)
{
	M_TrackedActors.Reserve(ActorsToTrack.Num());
	M_TrackingHasCountedInvalid.SetNum(ActorsToTrack.Num());

	for (int32 TrackingIndex = 0; TrackingIndex < ActorsToTrack.Num(); ++TrackingIndex)
	{
		AActor* ActorToTrack = ActorsToTrack[TrackingIndex];
		M_TrackedActors.Add(ActorToTrack);
		M_TrackingHasCountedInvalid[TrackingIndex] = false;

		if (not IsValid(ActorToTrack))
		{
			Tracking_OnActorInvalidatedByIndex(TrackingIndex);
			continue;
		}

		switch (TrackingType)
		{
		case EMissionTrackingType::TrackDestructablesCollapse:
			Tracking_ConfigureDestructableActorAtIndex(ActorToTrack, TrackingIndex);
			break;
		case EMissionTrackingType::TrackOnActorsDestroyed:
			Tracking_ConfigureActorDestroyedAtIndex(ActorToTrack, TrackingIndex);
			break;
		default:
			break;
		}
	}

	return true;
}

void UMissionBase::Tracking_ConfigureDestructableActorAtIndex(AActor* ActorToTrack, const int32 TrackingIndex)
{
	ADestructableEnvActor* DestructableActor = Cast<ADestructableEnvActor>(ActorToTrack);
	if (not IsValid(DestructableActor))
	{
		Tracking_OnActorInvalidatedByIndex(TrackingIndex);
		return;
	}

	Tracking_RegisterDestructableCallbacks(DestructableActor, TrackingIndex);
}

void UMissionBase::Tracking_ConfigureActorDestroyedAtIndex(AActor* ActorToTrack, const int32 TrackingIndex)
{
	if (not IsValid(ActorToTrack))
	{
		Tracking_OnActorInvalidatedByIndex(TrackingIndex);
		return;
	}

	Tracking_RegisterActorDestroyedCallback(ActorToTrack);
}

void UMissionBase::Tracking_RegisterDestructableCallbacks(ADestructableEnvActor* DestructableActor, const int32 TrackingIndex)
{
	if (not IsValid(DestructableActor))
	{
		return;
	}

	const TWeakObjectPtr<UMissionBase> WeakThis(this);
	DestructableActor->OnDestructibleCollapse.AddLambda([WeakThis, TrackingIndex]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->Tracking_OnActorInvalidatedByIndex(TrackingIndex);
	});
}

void UMissionBase::Tracking_RegisterActorDestroyedCallback(AActor* ActorToTrack)
{
	if (not IsValid(ActorToTrack))
	{
		return;
	}

	ActorToTrack->OnDestroyed.AddUniqueDynamic(this, &UMissionBase::Tracking_OnTrackedActorDestroyed);
}

void UMissionBase::Tracking_OnTrackedActorDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == nullptr)
	{
		return;
	}

	for (int32 TrackingIndex = 0; TrackingIndex < M_TrackedActors.Num(); ++TrackingIndex)
	{
		if (M_TrackedActors[TrackingIndex].Get() != DestroyedActor)
		{
			continue;
		}

		Tracking_OnActorInvalidatedByIndex(TrackingIndex);
	}
}

void UMissionBase::Tracking_StartBackupValidityTimer(const float ValidityCheckInterval)
{
	if (ValidityCheckInterval <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	const TWeakObjectPtr<UMissionBase> WeakThis(this);
	FTimerDelegate ValidityCheckTimerDelegate;
	ValidityCheckTimerDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->Tracking_CheckForInvalidActorsByTimer();
	});

	World->GetTimerManager().SetTimer(
		M_TrackingValidityTimerHandle,
		ValidityCheckTimerDelegate,
		ValidityCheckInterval,
		true
	);
}

void UMissionBase::Tracking_OnActorInvalidatedByIndex(const int32 TrackingIndex)
{
	if (not M_TrackingHasCountedInvalid.IsValidIndex(TrackingIndex))
	{
		return;
	}

	if (M_TrackingHasCountedInvalid[TrackingIndex])
	{
		return;
	}

	M_TrackingHasCountedInvalid[TrackingIndex] = true;
	M_MissionTrackingRuntimeState.CurrentCount += 1;
	Tracking_ApplyWidgetTitleUpdate();

	if (not Tracking_GetHasReachedMaxCount())
	{
		return;
	}

	Tracking_OnReachedMaxCount();
}

void UMissionBase::Tracking_CheckForInvalidActorsByTimer()
{
	for (int32 TrackingIndex = 0; TrackingIndex < M_TrackedActors.Num(); ++TrackingIndex)
	{
		if (M_TrackingHasCountedInvalid.IsValidIndex(TrackingIndex) && M_TrackingHasCountedInvalid[TrackingIndex])
		{
			continue;
		}

		AActor* TrackedActor = nullptr;
		if (M_TrackedActors.IsValidIndex(TrackingIndex))
		{
			TrackedActor = M_TrackedActors[TrackingIndex].Get();
		}

		if (IsValid(TrackedActor))
		{
			continue;
		}

		Tracking_OnActorInvalidatedByIndex(TrackingIndex);
	}
}

void UMissionBase::Tracking_ApplyWidgetTitleUpdate()
{
	if (M_MissionTrackingRuntimeState.TrackingType == EMissionTrackingType::NoTracking)
	{
		return;
	}

	MissionWidgetState.MissionTitle = Tracking_BuildTitleText();

	if (not GetHasMissionWidget())
	{
		return;
	}

	M_MissionWidget->RefreshMissionWidget(MissionWidgetState);
}

FText UMissionBase::Tracking_BuildTitleText() const
{
	const FString CurrentCountAsText = FString::FromInt(M_MissionTrackingRuntimeState.CurrentCount);
	const FString MaxCountAsText = FString::FromInt(M_MissionTrackingRuntimeState.MaxCount);
	const FString CountText = CurrentCountAsText + M_MissionTrackingRuntimeState.Partitive + MaxCountAsText;
	const FString RichCountText = FRTSRichTextConverter::MakeRTSRich(CountText, M_MissionTrackingRuntimeState.RichCountType);

	FString FinalTitleText;
	if (not M_MissionTrackingRuntimeState.Prefix.IsEmpty())
	{
		FinalTitleText += M_MissionTrackingRuntimeState.Prefix + TEXT(" ");
	}

	FinalTitleText += RichCountText;

	if (not M_MissionTrackingRuntimeState.Postfix.IsEmpty())
	{
		FinalTitleText += TEXT(" ") + M_MissionTrackingRuntimeState.Postfix;
	}

	return FText::FromString(FinalTitleText);
}

bool UMissionBase::Tracking_GetHasReachedMaxCount() const
{
	return M_MissionTrackingRuntimeState.CurrentCount >= M_MissionTrackingRuntimeState.MaxCount;
}

void UMissionBase::Tracking_OnReachedMaxCount()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TrackingValidityTimerHandle);
	}

	if (MissionState.bIsMissionComplete)
	{
		return;
	}

	OnMissionComplete();
}

void UMissionBase::RegisterCallbackOnScavengableObjectScavenged(AScavengeableObject* ScavengableObject)
{
	if (not IsValid(ScavengableObject))
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto MissionCallBack = [WeakThis, ScavengableObject]()-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis.Get()->BP_OnCallBackScavengableObjectScavenged(ScavengableObject);
		}
	};
	ScavengableObject->OnScavenged.AddLambda(MissionCallBack);
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

void UMissionBase::RegisterCallbackOnBXPDies(ABuildingExpansion* BuildingExpansion)
{
	if(not EnsureBxpIsValid(BuildingExpansion))
	{
		return;
	}
	TWeakObjectPtr<UMissionBase> WeakThis(this);
	auto MissionCallBack = [WeakThis, BuildingExpansion]()-> void
	{
		if (WeakThis.IsValid())
		{
			WeakThis.Get()->BP_OnCallBackBXPDies(BuildingExpansion);
		}
	};
	BuildingExpansion->OnUnitDies.AddLambda(MissionCallBack);
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
	if (bM_IgnoreTriggerOnMissionStart)
	{
		bM_IgnoreTriggerOnMissionStart = false;
		DebugMission("Mission trigger was bypassed by explicit start request. + " + GetName());
	}
	else if (GetHasValidTrigger())
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

bool UMissionBase::GetHasConfiguredNextMission() const
{
	if (NextMission)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"Mission attempted to start NextMission ignoring trigger, but no NextMission is configured."
		"\n Mission : " + GetName());
	return false;
}

bool UMissionBase::GetHasValidTriggerableMissionIndex(const int32 TriggerableMissionIndex) const
{
	if (not TriggerableMissions.IsValidIndex(TriggerableMissionIndex))
	{
		RTSFunctionLibrary::ReportError(
			"Mission attempted to trigger a mission from TriggerableMissions with an invalid index."
			"\n Mission : " + GetName() +
			"\n Index : " + FString::FromInt(TriggerableMissionIndex) +
			"\n TriggerableMissions.Num() : " + FString::FromInt(TriggerableMissions.Num()));
		return false;
	}

	if (IsValid(TriggerableMissions[TriggerableMissionIndex]))
	{
		return true;
	}

	RTSFunctionLibrary::ReportError(
		"Mission attempted to trigger a mission from TriggerableMissions but the selected mission is null."
		"\n Mission : " + GetName() +
		"\n Index : " + FString::FromInt(TriggerableMissionIndex));
	return false;
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

int32 UMissionBase::GetNextAsyncSpawnId()
{
	const int32 NextSpawnId = M_NextAsyncSpawnId;
	++M_NextAsyncSpawnId;
	return NextSpawnId;
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

void UMissionBase::AsyncSpawnActorAtLocationWithQueue(
	const FTrainingOption& TrainingOption,
	const int32 ID,
	const FVector SpawnLocation,
	const FRotator Rotation,
	const TArray<TSubclassOf<class UBehaviour>>& BehavioursToApply,
	const TArray<FMissionSpawnCommandQueueOrder>& CommandQueue)
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	GetMissionManagerChecked()->SpawnActorAtLocationWithCommandQueue(
		TrainingOption,
		ID,
		SpawnLocation,
		Rotation,
		BehavioursToApply,
		CommandQueue,
		this);
}

void UMissionBase::SpawnPlayerCommandVehicle(const FVector SpawnLocation, const FRotator SpawnRotation)
{
	const FTrainingOption PlayerCommandVehicleTrainingOption = URTSBlueprintFunctionLibrary::GetGerPlayerCommandVehicle(
		this);
	AsyncSpawnActorAtLocation(
		PlayerCommandVehicleTrainingOption,
		GetNextAsyncSpawnId(),
		SpawnLocation,
		SpawnRotation);
}

void UMissionBase::SpawnPlayerLightMediumVehicle(const FVector SpawnLocation, const FRotator SpawnRotation)
{
	const FTrainingOption PlayerLightMediumVehicleTrainingOption = URTSBlueprintFunctionLibrary::
		GetGerPlayerLightMediumTank(this);
	AsyncSpawnActorAtLocation(
		PlayerLightMediumVehicleTrainingOption,
		GetNextAsyncSpawnId(),
		SpawnLocation,
		SpawnRotation);
}

void UMissionBase::SpawnPlayerMediumVehicle(const FVector SpawnLocation, const FRotator SpawnRotation)
{
	const FTrainingOption PlayerMediumVehicleTrainingOption = URTSBlueprintFunctionLibrary::GetGerPlayerMediumTank(this);
	AsyncSpawnActorAtLocation(
		PlayerMediumVehicleTrainingOption,
		GetNextAsyncSpawnId(),
		SpawnLocation,
		SpawnRotation);
}

void UMissionBase::SpawnPlayerPZIIIAAOrPZ38Rail(const FVector SpawnLocation, const FRotator SpawnRotation)
{
	
	const FTrainingOption PlayerMediumVehicleTrainingOption = URTSBlueprintFunctionLibrary::GetPlayerPanzerIIIAAOrRail38T(this);
	AsyncSpawnActorAtLocation(
		PlayerMediumVehicleTrainingOption,
		GetNextAsyncSpawnId(),
		SpawnLocation,
		SpawnRotation);
}

void UMissionBase::SpawnPlayerJaguarOrPzIVG(const FVector SpawnLocation, const FRotator SpawnRotation)
{
	const FTrainingOption PlayerMediumVehicleTrainingOption = URTSBlueprintFunctionLibrary::GetPlayerJaguarOrPanzerIVG(this);
	AsyncSpawnActorAtLocation(
		PlayerMediumVehicleTrainingOption,
		GetNextAsyncSpawnId(),
		SpawnLocation,
		SpawnRotation);
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

void UMissionBase::SpawnCargoSquadWithVehicle(
	const ETankSubtype TankSubtype,
	const ESquadSubtype SquadSubtype,
	const FVector SpawnLocation,
	const FRotator SpawnRotation,
	const FVector MoveLocationAfterEnter)
{
	AMissionManager* MissionManager = GetMissionManagerChecked();
	if (not IsValid(MissionManager))
	{
		return;
	}

	MissionManager->SpawnCargoSquadWithVehicle(
		TankSubtype,
		SquadSubtype,
		SpawnLocation,
		SpawnRotation,
		MoveLocationAfterEnter);
}

void UMissionBase::SpawnSeededChoiceGroups(const TArray<FSeededChoices>& SeededChoicesArray)
{
	if (not GetIsValidMissionManager())
	{
		return;
	}

	GetMissionManagerChecked()->SpawnSeededChoiceGroups(SeededChoicesArray, this);
}

bool UMissionBase::PlaceActorAtSeededLocation(
	AActor* ActorToPlace,
	const TArray<FVector>& CandidateLocations,
	const int32 GroupIndex) const
{
	if (not IsValid(ActorToPlace))
	{
		RTSFunctionLibrary::ReportError("PlaceActorAtSeededLocation failed because ActorToPlace was invalid.");
		return false;
	}

	if (CandidateLocations.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"PlaceActorAtSeededLocation failed because CandidateLocations was empty."
		);
		return false;
	}

	if (not GetIsValidMissionManager())
	{
		return false;
	}

	const int32 CampaignSeed = GetMissionManagerChecked()->GetGenerationSeed();
	const uint32 SeedHash = HashCombineFast(
		static_cast<uint32>(CampaignSeed),
		static_cast<uint32>(GroupIndex + 1)
	);
	const int32 SelectedLocationIndex = static_cast<int32>(
		SeedHash % static_cast<uint32>(CandidateLocations.Num())
	);
	if (not CandidateLocations.IsValidIndex(SelectedLocationIndex))
	{
		RTSFunctionLibrary::ReportError(
			"PlaceActorAtSeededLocation generated an invalid location index."
		);
		return false;
	}

	ActorToPlace->SetActorLocation(CandidateLocations[SelectedLocationIndex]);
	return true;
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

bool UMissionBase::RegisterCameraBoundary(const FCameraBoundaryRegistrationParams& BoundaryRegistrationParams)
{
	SetCameraControllerReference();
	if (not GetIsValidPlayerCameraController())
	{
		return false;
	}

	return M_PlayerCameraController->RegisterAdditionalCameraBoundary(BoundaryRegistrationParams);
}

bool UMissionBase::RemoveCameraBoundaryById(const FName BoundaryId)
{
	SetCameraControllerReference();
	if (not GetIsValidPlayerCameraController())
	{
		return false;
	}

	return M_PlayerCameraController->RemoveAdditionalCameraBoundaryById(BoundaryId);
}

void UMissionBase::RemoveAllCameraBoundaries()
{
	SetCameraControllerReference();
	if (not GetIsValidPlayerCameraController())
	{
		return;
	}

	M_PlayerCameraController->RemoveAllAdditionalCameraBounds();
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
		break;
	}
	return Option;
}

FTrainingOption UMissionBase::SelectLightTankOnDifficulty() const
{
	const FTrainingOption BackupOption = FTrainingOption(EAllUnitType::UNType_Tank,
	                                                     static_cast<uint8>(ETankSubtype::Tank_BT7));
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

FTrainingOption UMissionBase::SelectT34OnDifficulty() const
{
	return SelectTankOptionPerDifficultySeeded(
		{ETankSubtype::Tank_T34_76},
		{ETankSubtype::Tank_T34_76_L, ETankSubtype::Tank_T34E},
		{ETankSubtype::Tank_T34_85},
		{ETankSubtype::Tank_T34_85, ETankSubtype::Tank_T34_100}
	);
}

FTrainingOption UMissionBase::SelectHeavyOnDifficulty() const
{
	return SelectTankOptionPerDifficultySeeded(
		{ETankSubtype::Tank_KV_1, ETankSubtype::Tank_T28},
		{ETankSubtype::Tank_KV_1, ETankSubtype::Tank_KV_1E},
		{ETankSubtype::Tank_KV_1E, ETankSubtype::Tank_KV_1, ETankSubtype::Tank_IS_1},
		{ETankSubtype::Tank_KV_2, ETankSubtype::Tank_IS_2, ETankSubtype::Tank_KV_1_Arc}
	);
}

FTrainingOption UMissionBase::SelectTDOnDifficulty() const
{
	return SelectTankOptionPerDifficultySeeded(
		{ETankSubtype::Tank_SU_76},
		{ETankSubtype::Tank_SU_85, ETankSubtype::Tank_T34_100},
		{
			ETankSubtype::Tank_SU_85, ETankSubtype::Tank_SU_100,
			ETankSubtype::Tank_T34_100, ETankSubtype::Tank_T34_100, ETankSubtype::Tank_SU_100
		},
		{ETankSubtype::Tank_SU_100, ETankSubtype::Tank_T34_100, ETankSubtype::Tank_T34_100}
	);
}

FTrainingOption UMissionBase::SelectSuperHeavyOnDifficulty() const
{
	return SelectTankOptionPerDifficultySeeded(
		{ETankSubtype::Tank_IS_1, ETankSubtype::Tank_KV_1E},
		{ETankSubtype::Tank_KV_1_Arc, ETankSubtype::Tank_IS_2},
		{ETankSubtype::Tank_IS_3},
		{ETankSubtype::Tank_IS_3, ETankSubtype::Tank_KV_5}
	);
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

bool UMissionBase::EnsureBxpIsValid(const TObjectPtr<ABuildingExpansion>& BXP) const
{
	if (IsValid(BXP))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("For mission : " + GetName() +
		" Building expansion is not valid!");
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

FTrainingOption UMissionBase::SelectMixOptionPerDifficultySeeded(
	const FSeededDifficultyMixPool NewToRTSMix,
	const FSeededDifficultyMixPool NormalMix,
	const FSeededDifficultyMixPool HardMix,
	const FSeededDifficultyMixPool BrutalAndIronManMix) const
{
	const FTrainingOption FallbackOption = FTrainingOption(EAllUnitType::UNType_Squad,
    		                                                       static_cast<uint8>(ESquadSubtype::Squad_Rus_Mosin));
	if (not GetIsValidMissionManager())
	{
		return FallbackOption;
	}

	const FSeededDifficultyMixPool SelectedDifficultyMix = SelectDifficultyMixPool(
		NewToRTSMix,
		NormalMix,
		HardMix,
		BrutalAndIronManMix
	);
	return SelectSeededOptionFromMixPool(SelectedDifficultyMix, FallbackOption);
}

FSeededDifficultyMixPool UMissionBase::SelectDifficultyMixPool(
	const FSeededDifficultyMixPool& NewToRTSMix,
	const FSeededDifficultyMixPool& NormalMix,
	const FSeededDifficultyMixPool& HardMix,
	const FSeededDifficultyMixPool& BrutalAndIronManMix) const
{
	switch (GetGameDifficulty().DifficultyLevel)
	{
	case ERTSGameDifficulty::NewToRTS:
		return NewToRTSMix;
	case ERTSGameDifficulty::Normal:
		return NormalMix;
	case ERTSGameDifficulty::Hard:
		return HardMix;
	case ERTSGameDifficulty::Brutal:
		return BrutalAndIronManMix;
	case ERTSGameDifficulty::Ironman:
		return BrutalAndIronManMix;
	}
	return NewToRTSMix;
}

FTrainingOption UMissionBase::SelectSeededOptionFromMixPool(
	const FSeededDifficultyMixPool& DifficultyMixPool,
	const FTrainingOption& FallbackOption) const
{
	TArray<FTrainingOption> ValidOptions;
	ValidOptions.Reserve(8);

	const TArray<FTrainingOption> CandidateOptions = {
		DifficultyMixPool.Option01,
		DifficultyMixPool.Option02,
		DifficultyMixPool.Option03,
		DifficultyMixPool.Option04,
		DifficultyMixPool.Option05,
		DifficultyMixPool.Option06,
		DifficultyMixPool.Option07,
		DifficultyMixPool.Option08
	};
	for (const FTrainingOption& CandidateOption : CandidateOptions)
	{
		if (CandidateOption.IsNone())
		{
			continue;
		}
		ValidOptions.Add(CandidateOption);
	}

	if (ValidOptions.IsEmpty())
	{
		return FallbackOption;
	}

	const int32 Seed = GetMissionManagerChecked()->GetGenerationSeed();
	const int32 SelectedIndex = Seed % ValidOptions.Num();
	return ValidOptions[SelectedIndex];
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

bool UMissionBase::GetHasMissionWidget() const
{
	return M_MissionWidget.IsValid();
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
