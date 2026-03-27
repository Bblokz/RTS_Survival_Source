#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MissionSounds/MissionSounds.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "MissionTowTeamWeaponSpawnState.h"
#include "MissionScheduler/MissionScheduler.h"
#include "MissionManager.generated.h"


class UW_Mission;
class UW_MissionTimer;
struct FMissionWidgetState;
struct FMissionTimerLifetimeSettings;
class ACPPController;
class UW_MissionWidgetManager;
class UMissionBase;
class UW_GameDifficultyPicker;
class AActor;
class UObject;

USTRUCT(BlueprintType)
struct FMissionStartingResources
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Starting Resources", meta=(ClampMin="0"))
	int32 M_Radixite = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Starting Resources", meta=(ClampMin="0"))
	int32 M_Metal = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Starting Resources", meta=(ClampMin="0"))
	int32 M_VehicleParts = 0;

	bool GetHasAnyStartingResources() const
	{
		return M_Radixite > 0 || M_Metal > 0 || M_VehicleParts > 0;
	}
};

/**
 * @brief Keeps track of the currently active missions with the active array.
 * Missions can have sub missions and can be added dynamically at runtime.
 * @note Overview of mission lifecycle:
 * @note - On BP the mission widget manager is created.
 * @note - Then we activate each mission set from the BP instance.
 * @note - OnActivate starts the loading which uses a set loading time by the user.
 * @note - When the loading is done the mission notifies the Start of the mission on this manager
 * and asks for a free mission widget.
 * @note - OnMissionCompleted Removes the active mission and plays the mission completed sound, idem for failed.
 * @note ******************************
 * @note On text only missions:
 * @note - There are special types of missions that will only have the mission UI with a next button.
 * @note - Those missions are instantly completed and do not play sound by clicking the next button on them.
 * @note - This is handled through the weak ptr to the mission object that is stored on the mission widget after init.
 * @note ******************************
 * @note On Triggers for missions:
 * @note - If a trigger is specified then the start function of the mission will enable the tick on the trigger but not
 * init the mission UI until the trigger is activated.
 * @note - Triggers do also support the start delay that can be specified; when not zero, after trigger activation,
 * we use a timer to call the final part of the start function to update the UI.
 */
UCLASS()
class RTS_SURVIVAL_API AMissionManager : public AActor
{
	GENERATED_BODY()

public:
	AMissionManager();

	void OnAnyMissionCompleted(UMissionBase* CompletedMission, const bool bPlaySound = true);
	void OnAnyMissionStarted(UMissionBase* LoadedMission);
	void OnAnyMissionFailed(UMissionBase* FailedMission);
	void SetMissionDifficulty(const int32 NewDifficultyPercentage, const ERTSGameDifficulty GameDifficulty);
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	FRTSGameDifficulty GetCurrentGameDifficulty() const;
	UW_Mission* OnMissionRequestSetupWidget(UMissionBase* RequestingMission);
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Mission|Timer")
	UW_MissionTimer* CreateAndInitMissionTimerWidget(TSubclassOf<UW_MissionTimer> MissionTimerWidgetClass,
	                                                 const FText& TimerText,
	                                                 float TimerInSeconds,
	                                                 const FMissionTimerLifetimeSettings& LifetimeSettings);
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void PlaySound2DForMission(USoundBase* SoundToPlay) const;
	/**
	 * @brief Routes mission callback scheduling to the dedicated scheduler component.
	 * @param Callback Delegate to execute on schedule.
	 * @param TotalCalls Total amount of executions requested.
	 * @param IntervalSeconds Interval between calls in scheduler ticks.
	 * @param CallbackOwner Owner object used for safety and cleanup.
	 * @param bFireBeforeFirstInterval If true, one callback call happens immediately.
	 * @return Task id when the callback stays scheduled, or INDEX_NONE when scheduling fails or finishes instantly.
	 */
	int32 ScheduleMissionCallback(
		const FMissionScheduledCallback& Callback,
		const int32 TotalCalls,
		const int32 IntervalSeconds,
		UObject* CallbackOwner,
		const bool bFireBeforeFirstInterval = true
	);
	void CancelAllCallbacksForObject(const UObject* CallbackOwner);

	void SpawnTowedTeamWeapon(const ETankSubtype TankSubtype, const ESquadSubtype SquadSubtype,
		const FVector& TankSpawnLocation);
	void HandleSpawnTowedTeamWeaponTankSpawned(int32 RequestId, AActor* SpawnedTankActor);
	void HandleSpawnTowedTeamWeaponSquadSpawned(int32 RequestId, AActor* SpawnedSquadActor);
	void HandleSpawnTowedTeamWeaponSquadReady(int32 RequestId);
	FMissionStartingResources GetMissionStartingResources() const { return M_MissionStartingResources; }

	/**
	 * @brief Calls load on the mission and adds it to the active missions array. 
	 * @param NewMission The mission to activate either from bp provided missions or dynamically added
	 * at runtime.
	 */
	void ActivateNewMission(UMissionBase* NewMission);

	/**
	 * Array of mission objects created as subobjects of the manager.
	 * With the Instanced specifier, level designers can add and configure missions in the editor.
	 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Missions")
	TArray<UMissionBase*> Missions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission|Starting Resources")
	FMissionStartingResources M_MissionStartingResources;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Difficulty")
	bool bSetGameDifficultyWithWidget = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Difficulty")
	TSubclassOf<UW_GameDifficultyPicker> M_GameDifficultyPickerWidgetClass;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void InitMissionSounds(const FMissionSoundSettings MissionSettings);

	
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetMissionManagerWidgetClass(TSubclassOf<UW_MissionWidgetManager> WidgetClass);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Optional: Tick to update missions if needed.
	virtual void Tick(float DeltaSeconds) override;


private:

	// All missions that are currently active.
	// Missions provided by the blueprint edit anywhere array.
	UPROPERTY()
	TArray<UMissionBase*> M_ActiveMissions;

	TSubclassOf<UW_MissionWidgetManager> M_WidgetManagerClass;

	void InitMissionManagerWidget();
	void OnCouldNotInitWidgetManager() const;

	UPROPERTY()
	TWeakObjectPtr<ACPPController> M_PlayerController;

	UPROPERTY()
	TObjectPtr<UMissionScheduler> M_MissionScheduler;

	void BeginPlay_InitPlayerController();
	void BeginPlay_InitMissionWidgetManager();
	void BeginPlay_InitGameDifficultyPickerWidget();
	void BeginPlay_InitMissionScheduler();
	bool EnsureValidPlayerController() const;
	bool GetIsValidMissionScheduler() const;

	UPROPERTY()
	UW_MissionWidgetManager* M_MissionWidgetManager;

	void RemoveActiveMission(UMissionBase* Mission);

	bool EnsureMissionIsValid(UMissionBase* Mission);
	bool EnsureMissionWidgetIsValid()const;
	/** @return True when this mission in not in the active array and valid. */
	bool EnsureMissionIsNotAlreadyActivated(UMissionBase* Mission);
	
	FMissionSoundSettings M_MissionSoundSettings;

	void PlayMissionSound(EMissionSoundType SoundType);
	void PlayMissionLoaded();
	void PlayMissionCompleted();
	void PlayMissionFailed();

	void ProvideMissionWidgetToMainGameUI() const;

	UPROPERTY()
	FRTSGameDifficulty M_GameDifficulty;

	UPROPERTY()
	TArray<FMissionTowTeamWeaponSpawnState> M_TowedTeamWeaponSpawnStates;

	int32 M_NextTowedTeamWeaponSpawnRequestId = 1;

	FMissionTowTeamWeaponSpawnState* FindTowedTeamWeaponSpawnState(const int32 RequestId);
	void TryCompleteTowedTeamWeaponSpawnRequest(const int32 RequestId);
	void TickTowSpawnRequests();
	void RemoveFinishedTowSpawnRequests();
	bool EnsureValidTowedTeamWeaponSpawnState(FMissionTowTeamWeaponSpawnState* TowSpawnState) const;
	bool EnsureValidTowAsyncSpawner(class ARTSAsyncSpawner* RTSAsyncSpawner) const;

};
