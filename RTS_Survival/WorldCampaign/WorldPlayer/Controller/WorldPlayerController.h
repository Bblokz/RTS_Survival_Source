// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Turn/WorldTurnType.h"
#include "WorldCameraController/WorldCameraController.h"
#include "PlayerTurnContext/FPlayerTurnContext.h"
#include "WorldPrimaryClickContext/WorldPrimaryClickContext.h"
#include "WorldPlayerController.generated.h"

class AWorldNeutralObject;
class AWorldPlayerObject;
class AWorldMapObject;
class AWorldMissionObject;
class AWorldEnemyObject;
class AWorldDivisionBase;
class UWorldStateAndSaveManager;
class UWorldDivisionManager;
class UWorldProfileAndUIManager;
class UPlayerResourceManager;
enum class ERTSFaction : uint8;
class AGeneratorWorldCampaign;
class UWorldCameraController;
class UPlayerWorldOutliner;
class AWorldFowManager;
class AWorldConnectionSplineRenderer;
class UW_AsyncWorldGeneration;
class UWorldPlayerAudioController;
class USoundBase;
class UWorld;
struct FWorldCampaignState;

USTRUCT(BlueprintType)
struct FWorldOperationMapSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Campaign|Operations")
	TMap<EMapMission, TSoftObjectPtr<UWorld>> MissionMaps;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Campaign|Operations")
	TArray<TSoftObjectPtr<UWorld>> EnemyObjectProceduralMaps;
};

/**
 * @brief Controller used by the world campaign Blueprint to route Enhanced Input
 * actions into the C++ camera component.
 * @note SetIsWorldCameraMovementDisabled: call in blueprint to enable or block camera input.
 * @note WorldCamera_ZoomIn: call in blueprint to route zoom input.
 * @note WorldCamera_ZoomOut: call in blueprint to route zoom input.
 * @note WorldCamera_ForwardMovement: call in blueprint to route forward/backward axis input.
 * @note WorldCamera_RightMovement: call in blueprint to route right/left axis input.
 * @note WorldCamera_MoveTo: call in blueprint to move the camera to campaign locations.
 * @note WorldCamera_StartCameraOvertake: call in blueprint to run a campaign camera overtake sequence.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWorldPlayerController();

	AGeneratorWorldCampaign* GetWorldGenerator() const;
	UWorldStateAndSaveManager* GetWorldStateAndSaveManager() const;
	UPlayerResourceManager* GetPlayerResourceManager() const;

	UFUNCTION(BlueprintCallable, Category = "World Campaign|Turns")
	FPlayerTurnContext GetPlayerTurnContext() const;

	/**
	 * @brief Gets the controller-owned division manager used by turn flow and Blueprint move orders.
	 * @return Division manager, or nullptr when the component is unavailable.
	 */
	UWorldDivisionManager* GetWorldDivisionManager() const;

	UFUNCTION(BlueprintCallable)
	void SetIsWorldCameraMovementDisabled(bool bIsDisabled);

	UFUNCTION(BlueprintCallable)
	bool GetIsWorldCameraMovementDisabled() const;

	UFUNCTION(BlueprintCallable)
	void WorldCamera_ZoomIn();

	UFUNCTION(BlueprintCallable)
	void WorldCamera_ZoomOut();

	UFUNCTION(BlueprintCallable)
	void WorldCamera_ForwardMovement(float AxisValue);

	UFUNCTION(BlueprintCallable)
	void WorldCamera_RightMovement(float AxisValue);

	/**
	 * @brief Blueprint entry point for point-targeted world division move orders.
	 * @param WorldDivision Division actor that should receive the order.
	 * @param TargetLocation Arbitrary world-space target point, not an anchor.
	 * @return true when the division accepted and cached the order.
	 */
	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	bool IssueWorldDivisionMoveOrder(AWorldDivisionBase* WorldDivision, const FVector& TargetLocation);

	/**
	 * @brief Provides a Blueprint routing point for move-to requests during the campaign map.
	 * @param MoveRequest Move data passed through to the camera controller.
	 */
	UFUNCTION(BlueprintCallable)
	void WorldCamera_MoveTo(const FMovePlayerCamera& MoveRequest);

	/**
	 * @brief Runs a UI-hidden camera overtake path and reports the final reached camera position.
	 * @param CameraOvertakeSettings Blueprint-authored points, timings, and optional sounds.
	 * @param OnCameraOvertakeFinished Callback invoked when the path ends or cannot start.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Camera")
	void WorldCamera_StartCameraOvertake(
		const FCameraOvertakeSettings& CameraOvertakeSettings,
		FWorldCameraOvertakeFinishedDelegate OnCameraOvertakeFinished);

	UFUNCTION(BlueprintCallable, Category = "World Campaign|Operations")
	void LaunchOperationForWorldObject(AWorldMapObject* OperationWorldObject);

	void ShowAsyncWorldGenerationWidget();
	void HideAsyncWorldGenerationWidget();
	void UpdateAsyncWorldGenerationWidget_AnchorPlacementStarted();
	void UpdateAsyncWorldGenerationWidget_AnchorPlacementComplete();
	void UpdateAsyncWorldGenerationWidget_ConnectionGenerationComplete();
	void UpdateAsyncWorldGenerationWidget_AsyncGenerationComplete();
	void UpdateAsyncWorldGenerationWidget_PruningCompleted();

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** @brief called on single Primary click */
	UFUNCTION(BlueprintCallable)
	void PrimaryClick();

	/** @brief called on end secondary button click*/
	UFUNCTION(BlueprintCallable)
	void SecondaryClick();

	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Turns")
	void BP_OnPlayerTurnStarted(const FPlayerTurnContext& PlayerTurnContext);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Operations")
	void BP_OnMissionMapLaunch(AWorldMissionObject* MissionMapObject, const TSoftObjectPtr<UWorld>& MissionMap);

	UFUNCTION(BlueprintImplementableEvent, Category = "World Campaign|Operations")
	void BP_OnEnemyMapLaunch(AWorldEnemyObject* EnemyMapObject, const TSoftObjectPtr<UWorld>& EnemyMap);

private:
	EWorldPrimaryClickContext M_PrimaryClickContext = EWorldPrimaryClickContext::None;
	void PlayTurn(const EWorldTurnType TurnType); 
	void PlayerTurn();
	void EnemyTurn();
	void OnEndEnemyTurn();
	void UpdateTurnCounter(int32 CurrentTurn) const;

	/**
	 * @brief Runs the player-owned division movement pass at the beginning of the enemy turn.
	 */
	void MovePlayerDivisions();

	/**
	 * @brief Runs the enemy-owned division movement pass at the end of the enemy turn.
	 */
	void MoveEnemyDivisions();
	bool PrimaryClick_Regular();
	void OnClicked_EnemyMapObj(AWorldEnemyObject* EnemyMapObj);
	void OnClicked_MissionMapObj(AWorldMissionObject* MissionMapObj);
	void OnClicked_PlayerMapObj(AWorldPlayerObject* PlayerMapObj);
	void OnClicked_NeutralMapObj(AWorldNeutralObject* NeutralMapObj);
	void PrimaryClick_ActiveMissionItem();
	void CollapseMissionMapItemDesc();
	void InitializeOperationMapsForCampaignSeed(int32 WorldGenerationSeed);
	void LaunchMissionMap(AWorldMissionObject* MissionMapObject);
	void LaunchEnemyMap(AWorldEnemyObject* EnemyMapObject);
	bool TryGetMissionMap(EMapMission MissionType, TSoftObjectPtr<UWorld>& OutMissionMap) const;
	bool TryGetEnemyMap(TSoftObjectPtr<UWorld>& OutEnemyMap);
	void OpenOperationMap(const TSoftObjectPtr<UWorld>& OperationMap) const;
	void ShowClickedDifficultyInfluenceRadiiForActor(AActor* Actor);
	void HideClickedDifficultyInfluenceRadii();
	bool GetIsValidWorldCameraController() const;
	bool GetIsValidPlayerWorldOutliner() const;
	bool GetIsValidWorldPlayerAudioController() const;

	void BeginPlay_SetupWorldGenerator();
	void BeginPlay_SetupWorldMenu();
	void BeginPlay_GameState_Faction_CampaignSettings();
	void BeginPlay_InitializeWorldGenerator();

	/**
	 * @brief Starts new-campaign generation after the generator has been initialized.
	 * @note FOW and camera setup must wait until this path has fully completed.
	 */
	void BeginPlay_GenerateNewWorld();

	/**
	 * @brief Restores saved campaign actors and player UI from the active campaign save.
	 * @note FOW and camera setup must wait until this path has fully completed.
	 */
	void BeginPlay_LoadSavedWorld();

	/**
	 * @brief Finishes new-campaign save/UI setup after async generation is complete.
	 * @note Bound before generation starts so immediate and async completion both land here.
	 */
	void OnGeneratedCampaignAsyncWorkFinished();

	void LoadWorldDataIntoObjects();

	/**
	 * @brief Spawns configured starting divisions after a newly generated campaign is save-ready.
	 */
	void InitializeWorldDivisionsForNewCampaign();

	/**
	 * @brief Restores saved divisions after generated anchors and map objects have been reconstructed.
	 * @param LoadedWorldCampaignState Loaded state whose WorldDivisions array is authoritative.
	 */
	void RestoreWorldDivisionsFromSave(const FWorldCampaignState& LoadedWorldCampaignState);

	/**
	 * @brief Reapplies division influence using the selected campaign difficulty.
	 */
	void RefreshWorldDivisionInfluence();

	/**
	 * @brief Runs world systems that require generated campaign actors to exist.
	 * @note Guards against duplicate setup if callbacks and load paths race during BeginPlay.
	 */
	void OnAllWorldObjectsAndTheirDataReady();

	void WorldGenerated_InitCountryOccupationRegulator();
	void WorldGenerated_SpawnWorldFowManager();

	/**
	 * @brief Spawns the connection spline renderer after generation so connected anchors are drawn as ribbons.
	 * @note Requires the generated connection graph to exist; runs alongside the FOW manager setup.
	 */
	void WorldGenerated_SpawnConnectionSplineRenderer();
	bool GetCanPrimaryClickActor(AActor* ClickedActor) const;

	void OnInitialWorldSetupComplete();

	void WorldSetupComplete_MovePlayerToHQ();
	void StartAsyncWorldGenerationProgressTimer();
	void StopAsyncWorldGenerationProgressTimer();
	void UpdateAsyncWorldGenerationTimedProgress();
	void SetAsyncWorldGenerationWidgetProgress(const FText& DescriptionText, float PercentageValue);
	bool GetIsValidAsyncWorldGenerationWidget() const;
	void BeginCameraOvertakeControl(
		const FCameraOvertakeSettings& CameraOvertakeSettings,
		FWorldCameraOvertakeFinishedDelegate OnCameraOvertakeFinished);
	void OnCameraOvertakePointReached(int32 ReachedPointIndex, const FVector& ReachedCameraLocation);
	void OnCameraOvertakeFinished(const FVector& LastCameraPosition);
	void HideWorldUIForCameraOvertake();
	void RestoreWorldUIAfterCameraOvertake();
	void SetIsWorldPlayerInputDisabled(bool bIsDisabled);
	void PlayCameraOvertakeSound(USoundBase* Sound) const;

	UPROPERTY()
	TWeakObjectPtr<UWorldCameraController> M_WorldCameraController;

	UPROPERTY()
	TObjectPtr<UWorldPlayerAudioController> M_WorldPlayerAudioController = nullptr;

	UPROPERTY()
	TObjectPtr<UPlayerWorldOutliner> M_PlayerWorldOutliner = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UWorldProfileAndUIManager> M_WorldProfileAndUIManager;
	bool GetIsValidWorldProfileAndUIManager() const;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;
	bool GetIsValidWorldGenerator() const;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_ClickedDifficultyInfluenceRadiusActor;

	UPROPERTY()
	TObjectPtr<UWorldStateAndSaveManager> M_WorldStateAndSaveManager = nullptr;
	bool GetIsValidWorldStateAndSaveManager() const;

	UPROPERTY()
	TObjectPtr<UWorldDivisionManager> M_WorldDivisionManager = nullptr;

	/**
	 * @brief Validates the controller-owned division manager component before turn or Blueprint use.
	 * @return true when the manager exists.
	 */
	bool GetIsValidWorldDivisionManager() const;

	UPROPERTY()
	TObjectPtr<UPlayerResourceManager> M_PlayerResourceManager = nullptr;
	bool GetIsValidPlayerResourceManager() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW Cloud", meta = (AllowPrivateAccess = "true", DisplayName = "WorldFowManagerClass"))
	TSubclassOf<AWorldFowManager> M_WorldFowManagerClass;

	UPROPERTY()
	TWeakObjectPtr<AWorldFowManager> M_WorldFowManager;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|Connection Spline", meta = (AllowPrivateAccess = "true", DisplayName = "ConnectionSplineRendererClass"))
	TSubclassOf<AWorldConnectionSplineRenderer> M_ConnectionSplineRendererClass;

	UPROPERTY()
	TWeakObjectPtr<AWorldConnectionSplineRenderer> M_ConnectionSplineRenderer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Campaign|Async Generation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UW_AsyncWorldGeneration> M_AsyncWorldGenerationWidgetClass;

	UPROPERTY()
	TObjectPtr<UW_AsyncWorldGeneration> M_AsyncWorldGenerationWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Campaign|Operations", meta = (AllowPrivateAccess = "true"))
	FWorldOperationMapSettings M_WorldOperationMapSettings;

	UPROPERTY()
	TArray<TSoftObjectPtr<UWorld>> M_ShuffledEnemyObjectProceduralMaps;

	FTimerHandle M_AsyncWorldGenerationProgressTimerHandle;
	float M_AsyncWorldGenerationProgressElapsedSeconds = 0.f;

	UPROPERTY()
	TObjectPtr<USoundBase> M_CameraOvertakeSoundPerSequentialPoint = nullptr;

	UPROPERTY()
	FWorldCameraOvertakeFinishedDelegate M_CameraOvertakeFinishedDelegate;

	ESlateVisibility M_WorldMenuVisibilityBeforeCameraOvertake = ESlateVisibility::Visible;

	FCampaignGenerationSettings M_CampaignSettings;
	FRTSGameDifficulty M_SelectedDifficulty;
	ERTSFaction M_PlayerFaction;
	bool bM_HasCompletedInitialWorldSetup = false;
	bool bM_IsCameraOvertakeControlActive = false;
	bool bM_IsWorldPlayerInputDisabled = false;
};
