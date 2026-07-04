// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/Game/RTSGameInstance/GameInstCampaignGenerationSettings/GameInstCampaignGenerationSettings.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Turn/WorldTurnType.h"
#include "WorldPrimaryClickContext/WorldPrimaryClickContext.h"
#include "WorldPlayerController.generated.h"

class AWorldNeutralObject;
class AWorldPlayerObject;
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
class UW_AsyncWorldGeneration;
struct FWorldCampaignState;

/**
 * @brief Controller used by the world campaign Blueprint to route Enhanced Input
 * actions into the C++ camera component.
 * @note SetIsWorldCameraMovementDisabled: call in blueprint to enable or block camera input.
 * @note WorldCamera_ZoomIn: call in blueprint to route zoom input.
 * @note WorldCamera_ZoomOut: call in blueprint to route zoom input.
 * @note WorldCamera_ForwardMovement: call in blueprint to route forward/backward axis input.
 * @note WorldCamera_RightMovement: call in blueprint to route right/left axis input.
 * @note WorldCamera_MoveTo: call in blueprint to move the camera to campaign locations.
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

	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	bool IssueWorldDivisionMoveOrder(AWorldDivisionBase* WorldDivision, const FVector& TargetLocation);

	/**
	 * @brief Provides a Blueprint routing point for move-to requests during the campaign map.
	 * @param MoveRequest Move data passed through to the camera controller.
	 */
	UFUNCTION(BlueprintCallable)
	void WorldCamera_MoveTo(const FMovePlayerCamera& MoveRequest);

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

private:
	EWorldPrimaryClickContext M_PrimaryClickContext = EWorldPrimaryClickContext::None;
	void PlayTurn(const EWorldTurnType TurnType); 
	void PlayerTurn();
	void EnemyTurn();
	void MovePlayerDivisions();
	void MoveEnemyDivisions();
	bool PrimaryClick_Regular();
	void OnClicked_EnemyMapObj(AWorldEnemyObject* EnemyMapObj);
	void OnClicked_MissionMapObj(AWorldMissionObject* MissionMapObj);
	void OnClicked_PlayerMapObj(AWorldPlayerObject* PlayerMapObj);
	void OnClicked_NeutralMapObj(AWorldNeutralObject* NeutralMapObj);
	void PrimaryClick_ActiveMissionItem();
	void CollapseMissionMapItemDesc();
	void ShowClickedDifficultyInfluenceRadiiForActor(AActor* Actor);
	void HideClickedDifficultyInfluenceRadii();
	bool GetIsValidWorldCameraController() const;
	bool GetIsValidPlayerWorldOutliner() const;

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
	void InitializeWorldDivisionsForNewCampaign();
	void RestoreWorldDivisionsFromSave(const FWorldCampaignState& LoadedWorldCampaignState);
	void RefreshWorldDivisionInfluence();

	/**
	 * @brief Runs world systems that require generated campaign actors to exist.
	 * @note Guards against duplicate setup if callbacks and load paths race during BeginPlay.
	 */
	void OnAllWorldObjectsAndTheirDataReady();

	void WorldGenerated_InitCountryOccupationRegulator();
	void WorldGenerated_SpawnWorldFowManager();
	bool GetCanPrimaryClickActor(AActor* ClickedActor) const;

	void OnInitialWorldSetupComplete();

	void WorldSetupComplete_MovePlayerToHQ();
	void StartAsyncWorldGenerationProgressTimer();
	void StopAsyncWorldGenerationProgressTimer();
	void UpdateAsyncWorldGenerationTimedProgress();
	void SetAsyncWorldGenerationWidgetProgress(const FText& DescriptionText, float PercentageValue);
	bool GetIsValidAsyncWorldGenerationWidget() const;

	UPROPERTY()
	TWeakObjectPtr<UWorldCameraController> M_WorldCameraController;

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
	bool GetIsValidWorldDivisionManager() const;

	UPROPERTY()
	TObjectPtr<UPlayerResourceManager> M_PlayerResourceManager = nullptr;
	bool GetIsValidPlayerResourceManager() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Campaign|FOW Cloud", meta = (AllowPrivateAccess = "true", DisplayName = "WorldFowManagerClass"))
	TSubclassOf<AWorldFowManager> M_WorldFowManagerClass;

	UPROPERTY()
	TWeakObjectPtr<AWorldFowManager> M_WorldFowManager;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Campaign|Async Generation", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UW_AsyncWorldGeneration> M_AsyncWorldGenerationWidgetClass;

	UPROPERTY()
	TObjectPtr<UW_AsyncWorldGeneration> M_AsyncWorldGenerationWidget = nullptr;

	FTimerHandle M_AsyncWorldGenerationProgressTimerHandle;
	float M_AsyncWorldGenerationProgressElapsedSeconds = 0.f;

	FCampaignGenerationSettings M_CampaignSettings;
	FRTSGameDifficulty M_SelectedDifficulty;
	ERTSFaction M_PlayerFaction;
	bool bM_HasCompletedInitialWorldSetup = false;
};
