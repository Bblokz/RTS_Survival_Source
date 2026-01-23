#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/Archive/ArchiveItemTypes/ArchiveItemTypes.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Missions/MissionWidgets/MissionWidgetState/MissionWidgetState.h"
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
struct FMissionWidgetState;
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

	/** * Optional tick function for per-frame mission logic.
	 * Derived classes can override if needed.
	 */
	virtual void TickMission(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Widget")
	FMissionWidgetState MissionWidgetState;

	// Can be left null, or set to the next mission to start when this one is complete.	
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "BaseSettings")
	UMissionBase* NextMission;

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

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	FRTSGameDifficulty GetGameDifficulty()const;

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
	void AsyncSpawnActorAtLocation(const FTrainingOption& TrainingOption, const int32 ID, const FVector SpawnLocation, const FRotator Rotation);
	

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "SpawnCreateActor")
	void AsyncSpawnActorAtLocationWithDelay(const FTrainingOption& TrainingOption, const int32 ID, const FVector SpawnLocation, const FRotator Rotation,
		const float Delay);
	
	UFUNCTION(BlueprintImplementableEvent)
	void BP_OnAsyncSpawnComplete(const FTrainingOption TrainingOption, AActor* SpawnedActor, const int32 ID);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void MoveCamera(FMovePlayerCamera CameraMove);

private:
	TWeakObjectPtr<AMissionManager> M_MissionManager;

	FTimerHandle M_LoadToStartTimerHandle;

	void CleanUp_TimerHandles(const UWorld* World);

	TWeakObjectPtr<UW_Mission> M_MissionWidget;

	bool GetHasValidTrigger() const;
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

	UPROPERTY()
	FTimerHandle M_TextOnlyDurationHandle;

	void TextOnlyMission_SetAutoCompleteTimer();

};
