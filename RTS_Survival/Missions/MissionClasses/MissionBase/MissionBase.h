#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Missions/MissionManager/EnemyUnitQueryType.h"
#include "RTS_Survival/Missions/MissionManager/MissionScheduler/MissionScheduler.h"
#include "RTS_Survival/Missions/MissionWidgets/MissionWidgetState/MissionWidgetState.h"
#include "RTS_Survival/Player/Camera/CameraController/PlayerCameraController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "UObject/NoExportTypes.h"
#include "Engine/OverlapResult.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "MissionBase.generated.h"

class ADestructableEnvActor;
enum class ERTSPortraitTypes : uint8;
class AEnemyController;
class ASquadController;
class ATankMaster;
class ANomadicVehicle;
class UPlayerCameraController;
struct FMovePlayerCamera;
struct FTrainingOption;
class ARTSAsyncSpawner;
class UMissionTrigger;
class UW_Mission;
class UW_MissionTimer;
struct FMissionWidgetState;
struct FMissionTimerLifetimeSettings;
class AMissionManager;


USTRUCT(Blueprintable)
struct FMissionState
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsMissionComplete = false;

	// Set to true after finished loading.
	// After being set this allows for the mission tick to be called.
	UPROPERTY()
	bool bIsMissionStarted = false;

	// Whether this mission is triggered. If set to false we call the trigger tick to check the condition.
	// This is only set to false if the mission has a valid trigger to check.
	UPROPERTY()
	bool bIsTriggered = true;

	UPROPERTY(BlueprintReadWrite)
	bool bTickOnMission = false;
};

USTRUCT(BlueprintType)
struct FSeededDifficultyMixPool
{
	GENERATED_BODY()

	// Designers can leave any option as default (None) to skip that slot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option01 = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option02 = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option03 = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option04 = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option05 = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option06 = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option07 = FTrainingOption();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seeded Selection")
	FTrainingOption Option08 = FTrainingOption();
};

/**
 * Base class for missions.
 * Marking with EditInlineNew allows missions to be created as subobjects in the editor.
 * @note On Triggers:
 * @note If a trigger is specified on the mission instance then the OnMissionStart function will behave a bit different;
 * @note The trigger is init with this mission and the bool started is set to true regardless; this allows the mgr to tick the trigger.
 * @note When the trigger is activated:  OnTriggerActivated is called with finalizes the start of the mission with Notify Mgr and UI updates.
 * @note when no trigger is provided we simply immediately call the notify Mgr and UI update function.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class RTS_SURVIVAL_API UMissionBase : public UObject
{
	GENERATED_BODY()

	// to call OnTriggerActivated.
	friend class RTS_SURVIVAL_API UMissionTrigger;

public:
	UMissionBase();

	/** Starts loading the mission, called by the mission manager. */
	void LoadMission(AMissionManager* MissionManager);

	inline bool GetCanTickMission() const
	{
		return MissionState.bTickOnMission && MissionState.bIsMissionStarted;
	};

	bool GetCanCheckTrigger() const;


	/**
	 * Called when the mission is complete.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "MissionEnding")
	virtual void OnMissionComplete();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "MissionEnding")
	void OnMissionFailed();

	// Starts NextMission immediately and bypasses its trigger requirement for this activation.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "MissionEnding")
	void StartNextMissionIgnoringTrigger();

	/**
	 * @brief Activates one mission from the TriggerableMissions array by index through the mission manager.
	 * @param TriggerableMissionIndex Index in TriggerableMissions that should be activated.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "MissionEnding")
	void TriggerMissionFromArray(const int32 TriggerableMissionIndex);

	void OnEnemyUnitsDestroyedCallback(const int32 ID, const EEnemyUnitQueryType EnemyUnitQueryType);

	/** * Optional tick function for per-frame mission logic.
	 * Derived classes can override if needed.
	 */
	virtual void TickMission(float DeltaTime);

	/**
	 * @brief Creates a mission timer through the manager so missions can start self-managed countdown widgets safely.
	 * @param MissionTimerWidgetClass Widget class derived from W_MissionTimer to instantiate.
	 * @param TimerText Label shown for the timer purpose.
	 * @param TimerInSeconds Initial countdown duration in seconds.
	 * @param LifetimeSettings Optional chained timer entries consumed after each expiration.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Timer")
	UW_MissionTimer* SpawnMissionTimerWidget(TSubclassOf<UW_MissionTimer> MissionTimerWidgetClass,
	                                         const FText& TimerText,
	                                         float TimerInSeconds,
	                                         const FMissionTimerLifetimeSettings& LifetimeSettings);

	/**
	 * @brief Schedules a mission-owned callback through the manager for deterministic shared ticking.
	 * @param Callback Delegate to execute.
	 * @param TotalCalls Total amount of executions requested.
	 * @param IntervalSeconds Interval between calls in scheduler ticks.
	 * @param InitialDelaySeconds Initial delay before first non-immediate callback call.
	 * @param bFireBeforeFirstInterval If true, one call executes immediately before interval ticking starts.
	 * @param bRepeatForever If true, callback keeps running until canceled.
	 * @return Task id used for scheduler tracking, or INDEX_NONE when no scheduled task remains.
	 */
	UFUNCTION(BlueprintCallable, Category="Mission|Scheduler")
	int32 ScheduleRepeatedCallback(
		const FMissionScheduledCallback& Callback,
		const int32 TotalCalls,
		const int32 IntervalSeconds,
		const int32 InitialDelaySeconds,
		const bool bFireBeforeFirstInterval = true,
		const bool bRepeatForever = false
	);
	/**
	 * @brief Schedules a mission-owned callback through the manager for deterministic shared ticking.
	 * @param Callback Delegate to execute.
	 * @param TotalCalls Total amount of executions requested.
	 * @param IntervalSeconds Interval between calls in scheduler ticks.
	 * @param InitialDelaySeconds Initial delay before first non-immediate callback call.
	 * @param bFireBeforeFirstInterval If true, one call executes immediately before interval ticking starts.
	 * @param bRepeatForever If true, callback keeps running until canceled.
	 * @return Task id used for scheduler tracking, or INDEX_NONE when no scheduled task remains.
	 */
	UFUNCTION(BlueprintCallable, Category="Mission|Scheduler")
	int32 ScheduleRepeatedCallbackWithRequirement(
		const FMissionScheduledCallback& Callback,
		const int32 TotalCalls,
		const int32 IntervalSeconds,
		const int32 InitialDelaySeconds,
		const TArray<AActor*>& RequiredActors,
		const bool bFireBeforeFirstInterval = true,
		const bool bRepeatForever = false
	);
	UFUNCTION(BlueprintCallable, Category="Mission|Scheduler")
	int32 ScheduleSingleCallback(
		const FMissionScheduledCallback& Callback,
		const int32 DelaySeconds
	);

	UFUNCTION(BlueprintCallable, Category="Mission|Scheduler")
	int32 ScheduleSingleCallbackWithRequirement(
		const FMissionScheduledCallback& Callback,
		const int32 DelaySeconds,
		const TArray<AActor*>& RequiredActors
	);

	UFUNCTION(BlueprintCallable, Category="Mission|Scheduler")
	void CancelScheduledCallback(const int32 TaskID);

	UFUNCTION(BlueprintCallable, Category="Mission|Scheduler")
	void CancelAllScheduledCallbacks();

	UFUNCTION(BlueprintCallable, Category="Mission|Scheduler")
	bool GetIsScheduledTaskActive(const int32 TaskID) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	FMissionWidgetState MissionWidgetState;

	// Can be left null, or set to the next mission to start when this one is complete.	
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "BaseSettings")
	UMissionBase* NextMission;

	// Missions that can only be manually triggered from this mission through TriggerMissionFromArray.
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "BaseSettings")
	TArray<UMissionBase*> TriggerableMissions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseSettings")
	float LoadToStartDelay = 0.0f;

	// If set to true then this mission will use a widget with the next button which will instantly complete the mission
	// once it is clicked and not play any mission completed sounds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseSettings")
	bool bIsTextOnlyMission = false;

	// How long before the mission as text-only will auto complete.
	// Is not used for regular missions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseSettings")
	float TextOnlyMissionDuration = 5.0f;

	// If set to true the mission will display as title first and the user needs to expand the widget to read the desc.
	// and or to click the next button on TextOnlyMissions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BaseSettings")
	bool bStartAsCollapsedWidget = false;

	// NEW: Optional mission trigger. If non-null and not "None" as type, the mission waits for the trigger.
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Trigger")
	UMissionTrigger* MissionTrigger;

	/** @return Whether the mission was triggered by its trigger or by default when no trigger was specified. */
	inline bool GetIsMissionTriggered() const { return MissionState.bIsTriggered; };

	// Called when the map is unloaded.
	virtual void OnCleanUpMission();

	virtual void BeginDestroy() override;

protected:
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	AMissionManager* GetMissionManagerChecked() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	TArray<FTrainingOption> GetTrainingOptionsDifficultyAdjusted(TArray<FTrainingOption> EasyOptions,
	                                                             TArray<FTrainingOption> NormalOptions,
	                                                             TArray<FTrainingOption> HardOptions,
	                                                             TArray<FTrainingOption> BrutalOptions,
	                                                             TArray<FTrainingOption> IronmanOptions) const;

	// Only creates if the current game difficulty is at least the minimal difficulty specified.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	bool CreateSingleAttackMoveDifficultyConditional(
		ERTSGameDifficulty MinimalDifficulty,
		TArray<FVector> SpawnLocations,
		TArray<FTrainingOption> TrainingOptions,
		TArray<FVector> WayPoints,
		FRotator FinalRotation = FRotator::ZeroRotator,
		const int32 MaxFormationWidth = 2,
		float TimeTillWave = 0.f,
		const float FormationOffsetMultiplier = 1.f,
		const float HelpOffsetRadiusMltMax = 1.5,
		const float HelpOffsetRadiusMltMin = 1.2,
		const float MaxAttackTimeBeforeAdvancingToNextWayPoint = 0.f,
		const int32 MaxTriesFindNavPointForHelpOffset = 3,
		const float ProjectionScale = 1.f);


	UFUNCTION(BlueprintCallable, NotBlueprintable)
	FRTSGameDifficulty GetGameDifficulty() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	int32 GetGameDifficultyPercentage() const;
	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	int32 GetGameDifficultyPercentageDividedBy(const int32 Divisor = 2) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	bool GetIsGameDifficultyNormalOrHigher() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	bool GetIsGameDifficultyHardOrHigher() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	bool GetIsGameDifficultyBrutalOrHigher() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	bool GetIsGameDifficultyIronMan() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, BlueprintPure)
	bool GetIsDifficultyAtLeast(const ERTSGameDifficulty DifficultyLevel) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission")
	void CallbackOnEnemyUnitsDestroyed(
		const EEnemyUnitQueryType EnemyUnitQueryType,
		const int32 ID);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	AEnemyController* GetEnemyControllerChecked() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	UW_Mission* GetMissionWidgetChecked() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Sounds")
	void PlaySound2D(USoundBase* SoundToPlay) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Sounds")
	void PlayPortrait(ERTSPortraitTypes PortraitType, USoundBase* VoiceLine) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Sounds")
	void PlayPlayerAnnouncerLine(EAnnouncerVoiceLineType AnnouncerLineType) const;


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Cinematics")
	bool OnCinematicTakeOverFromMission(const bool bCinematicStarted) const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void AddArchiveItem(const ERTSArchiveItem ItemType, const FTrainingOption OptionalUnit = FTrainingOption(),
	                    const int32 SortingPriority = 0, const float NotificationTime = 20) const;
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="FindUnits")
	AActor* FindActorInRange(
		const FVector Location,
		const float Range,
		const TSubclassOf<AActor> ActorClasses,
		const bool bFindClosest = false, const bool bFindPlayerUnits = true);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RegisterCallbackOnDestructibleCollapse(ADestructableEnvActor* ActorToTrackWhenCollapse);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCallBackDestructibleCollapse(ADestructableEnvActor* ActorCollapsed);

	/**
	 * @brief To be able to execute logic when a specific nomadic vehicle converts to building or to truck state.
	 * @param NomadicVehicle The nomadic vehicle to track the behavior of. 
	 * @param bCallBackOnBuildingConversion Whether to call back when the vehicle converts to building or the other way around.
	 * if true the callback will be called when the vehicle converts to building. False otherwise.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RegisterCallbackOnNomadicVehicle(ANomadicVehicle* NomadicVehicle, const bool bCallBackOnBuildingConversion);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCallBackNomadicVehicle(ANomadicVehicle* NomadicVehicle, const bool bCallBackOnBuildingConversion);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RegisterCallbackOnTankDies(ATankMaster* Tank);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCallBackTankDies(ATankMaster* Tank);

	// Sets a timer to digin the provided vehicle; calls BP_OnCallBackTankDigin with the ID and Tank when finished.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void DiginFortifyVehicleOverTime(
		ATankMaster* Tank, const float TimeTillStartDigIn = 5,
		const int32 ID = 0);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCallBackTankDigIn(ATankMaster* Tank, const int32 ID);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void RegisterCallBackOnPickUpWeapon(ASquadController* SquadController);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCallBackSquadPickupWeapon(ASquadController* SquadController);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnCallBackEnemyActorsDestroyed(const int32 ID, const EEnemyUnitQueryType EnemyUnitQueryType);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Mission")
	void DestroyActorsInRange(
		float Range,
		const TArray<TSubclassOf<AActor>>& ActorClasses,
		const FVector& Location);

	virtual auto OnMissionStart() -> void;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnMissionStart();

	// Called by cpp function in base do NOT CALL DIRECTLY.
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnMissionComplete();

	// Called by cpp function in base do NOT CALL DIRECTLY.
	// For specific mission logic when the mission is failed.
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnMissionFailed();

	// Called by cpp function in base do NOT CALL DIRECTLY.
	// For any cleanup of objects on the map spawned by the mission.
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnMissionCleanUp();

	UPROPERTY(BlueprintReadWrite)
	FMissionState MissionState;

	void DebugMission(const FString& Message) const;
	bool GetIsValidMissionManager() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "SpawnCreateActor")
	void AsyncSpawnActor(const FTrainingOption& TrainingOption, const int32 ID, AActor*
	                     SpawnPointActor);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "SpawnCreateActor")
	void AsyncSpawnActorAtLocation(const FTrainingOption& TrainingOption, const int32 ID, const FVector SpawnLocation,
	                               const FRotator Rotation);


	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "SpawnCreateActor")
	void AsyncSpawnActorAtLocationWithDelay(const FTrainingOption& TrainingOption, const int32 ID,
	                                        const FVector SpawnLocation, const FRotator Rotation,
	                                        const float Delay);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Spawn")
	void SpawnPlayerCommandVehicle(const FVector SpawnLocation, const FRotator SpawnRotation);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Spawn")
	void SpawnPlayerLightMediumVehicle(const FVector SpawnLocation, const FRotator SpawnRotation);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Spawn")
	void SpawnPlayerMediumVehicle(const FVector SpawnLocation, const FRotator SpawnRotation);

	/**
	 * @brief Spawns a tank and team-weapon squad asynchronously and links tow once both are ready.
	 * @param TankSubtype Tank subtype used for the towing vehicle spawn.
	 * @param SquadSubtype Squad subtype expected to be a team weapon controller.
	 * @param SpawnLocation Spawn location for the tank; team weapon squad spawns behind this location.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Spawn")
	void SpawnTowedTeamWeapon(const ETankSubtype TankSubtype, const ESquadSubtype SquadSubtype,
	                          const FVector SpawnLocation);

	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnAsyncSpawnComplete(const FTrainingOption TrainingOption, AActor* SpawnedActor, const int32 ID);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void MoveCamera(FMovePlayerCamera CameraMove);

	/**
	 * @brief Registers an additional camera blocking boundary through mission blueprint logic.
	 * @param BoundaryRegistrationParams Boundary registration payload forwarded to the player camera controller.
	 * @return True if the boundary registration succeeded.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Camera")
	bool RegisterCameraBoundary(const FCameraBoundaryRegistrationParams& BoundaryRegistrationParams);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Camera")
	bool RemoveCameraBoundaryById(FName BoundaryId);

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Camera")
	void RemoveAllCameraBoundaries();

	/**
	 * @brief Depending on mission difficulty get mosin-ptrs squad
	 * @note Hard: large ptrs
	 * @note Brutal: RedHammer Ptrs
	 * @note IronMan: RedHammer Ptrs
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectSquadOnDifficultyAt() const;

	/**
	 * @brief Depending on mission difficulty get Hazmat engineers squad
	 * @note Hard: mosin squad
	 * @note Brutal: RedHammer 
	 * @note IronMan: RedHammer 
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectSquadOnDifficultyAntiInfantry() const;

	/**
	 * @brief Selects an early-war light tank pool that scales into guaranteed T-28 on top difficulties.
	 * @note NewToRTS: ba-12 or bt-7 or bt-7-4
	 * @note Normal: ba-14 or t-26 or bt-7-4
	 * @note Hard: ba-14 or t-70
	 * @note Brutal and IronMan: t-28
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectLightTankOnDifficulty() const;


	/**
	 * @brief Selects medium T-34 variants that progress from early 76 models to late upgrades.
	 * @note NewToRTS: t-34-76
	 * @note Normal: t-34-76-l or t-34e
	 * @note Hard: t-34-85
	 * @note Brutal and IronMan: t-34-85 or t-34-100
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectT34OnDifficulty() const;

	/**
	 * @brief Selects heavy armor options with stronger pools as mission difficulty increases.
	 * @note NewToRTS: kv-1 or t-28
	 * @note Normal: kv-1 or kv-1e
	 * @note Hard: kv-1e or kv-1 or is-1
	 * @note Brutal and IronMan: kv-2 or is-2 or kv-1-arc
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectHeavyOnDifficulty() const;

	/**
	 * @brief Selects tank destroyer options, increasing access to SU-100 and T-34-100 at higher tiers.
	 * @note NewToRTS: su-76
	 * @note Normal: su-85 or t-34-100
	 * @note Hard: su-85 or su-100 or t-34-100
	 * @note Brutal and IronMan: su-100 or t-34-100
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectTDOnDifficulty() const;

	/**
	 * @brief Selects late-game armor options with top-end super-heavy choices on highest difficulties.
	 * @note NewToRTS: is-1 or kv-1e
	 * @note Normal: kv-1-arc or is-2
	 * @note Hard: is-3
	 * @note Brutal and IronMan: is-3 or kv-5
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectSuperHeavyOnDifficulty() const;

	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectTankOptionPerDifficultySeeded(
		TArray<ETankSubtype> NewToRTSTypes,
		TArray<ETankSubtype> NormalDiffTypes,
		TArray<ETankSubtype> HardDiffTypes,
		TArray<ETankSubtype> BrutalAndIronManTypes) const;
	

	


	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectSquadOptionPerDifficultySeeded(
		TArray<ESquadSubtype> NewToRTSTypes,
		TArray<ESquadSubtype> NormalDiffTypes,
		TArray<ESquadSubtype> HardDiffTypes,
		TArray<ESquadSubtype> BrutalAndIronManTypes) const;

	/**
	 * @brief Picks one seeded training option from the difficulty-specific mix pools without requiring blueprint arrays.
	 * @param NewToRTSMix Options used for NewToRTS difficulty.
	 * @param NormalMix Options used for Normal difficulty.
	 * @param HardMix Options used for Hard difficulty.
	 * @param BrutalAndIronManMix Options used for Brutal and Ironman difficulties.
	 * @return Seeded option from the selected difficulty mix, or fallback when all slots are None.
	 */
	UFUNCTION(BlueprintPure, BlueprintCallable, NotBlueprintable, Category = "Seeded Selection")
	FTrainingOption SelectMixOptionPerDifficultySeeded(
		const FSeededDifficultyMixPool NewToRTSMix,
		const FSeededDifficultyMixPool NormalMix,
		const FSeededDifficultyMixPool HardMix,
		const FSeededDifficultyMixPool BrutalAndIronManMix
	) const;
private:
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	// Used so mission cleanup can always cancel every task this mission created.
	UPROPERTY()
	TArray<int32> M_ScheduledTaskIds;

	FTimerHandle M_LoadToStartTimerHandle;

	void CleanUp_TimerHandles(const UWorld* World);

	TWeakObjectPtr<UW_Mission> M_MissionWidget;

	bool GetHasValidTrigger() const;
	bool GetHasConfiguredNextMission() const;
	bool GetHasValidTriggerableMissionIndex(const int32 TriggerableMissionIndex) const;
	bool GetIsValidMissionWidget() const;
	// Free the associated widget so it is able to sever another mission base derived class.
	void MarkWidgetAsFree() const;

	void CompleteMissionStartSetUIAndNotifyMgr();

	// Do not call directly only called by associated tigger.
	void OnTriggerActivated();

	void LoadMissionGetRTSAsyncSpawner();

	// Added to when delay spawning.
	TArray<FTimerHandle> M_DelaySpawnTimerHandles;

	TWeakObjectPtr<ARTSAsyncSpawner> M_RTSAsyncSpawner;
	bool EnsureValidRTSSpawner();

	void OnAsyncSpawnComplete(const FTrainingOption& TrainingOption, AActor* SpawnedActor, const int32 ID);

	bool EnsureSpawnedActorIsValid(const AActor* SpawnedActor) const;

	bool EnsureValidSpawnPoint(AActor* SpawnPointActor, const FTrainingOption& TrainingOption);

	// To keep track of the rotations that we want for spawned actors
	UPROPERTY()
	TMap<int32, FRotator> M_MapIdToRotation;


	TWeakObjectPtr<UPlayerCameraController> M_PlayerCameraController;

	void SetCameraControllerReference();

	bool GetIsValidPlayerCameraController() const;

	bool EnsureNomadicIsValid(const TObjectPtr<ANomadicVehicle>& NomadicVehicle) const;
	bool EnsureTankIsValid(const TObjectPtr<ATankMaster>& Tank) const;
	bool EnsureSquadIsValid(const TObjectPtr<ASquadController>& SquadController) const;

	FSeededDifficultyMixPool SelectDifficultyMixPool(
		const FSeededDifficultyMixPool& NewToRTSMix,
		const FSeededDifficultyMixPool& NormalMix,
		const FSeededDifficultyMixPool& HardMix,
		const FSeededDifficultyMixPool& BrutalAndIronManMix
	) const;

	FTrainingOption SelectSeededOptionFromMixPool(
		const FSeededDifficultyMixPool& DifficultyMixPool,
		const FTrainingOption& FallbackOption
	) const;

	void RegisterScheduledTaskID(const int32 TaskID);
	void RemoveTrackedTaskID(const int32 TaskID);
	void PruneInactiveScheduledTaskIDs();
	int32 GetNextAsyncSpawnId();

	UPROPERTY()
	FTimerHandle M_TextOnlyDurationHandle;

	int32 M_NextAsyncSpawnId = 1;

	void TextOnlyMission_SetAutoCompleteTimer();

	bool bM_IgnoreTriggerOnMissionStart = false;
};
