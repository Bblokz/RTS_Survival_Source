// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Turn/WorldTurnType.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "WorldDivisionManager.generated.h"

class AGeneratorWorldCampaign;
class AWorldDivisionBase;
class AWorldPlayerController;
class UWorldDataComponent;

/**
 * @brief Controller-owned runtime system for spawning, moving, saving, and applying world divisions.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldDivisionManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldDivisionManager();

	/**
	 * @brief Spawns authored starting divisions after a fresh campaign map has been generated.
	 * @param WorldPlayerController Controller that owns this manager and save system.
	 * @param WorldGenerator Generated campaign map provider used for anchors and WorldData.
	 * @param GameDifficulty Difficulty used to resolve data-driven division strength.
	 */
	void InitializeNewCampaignDivisions(AWorldPlayerController* WorldPlayerController,
	                                    AGeneratorWorldCampaign* WorldGenerator,
	                                    ERTSGameDifficulty GameDifficulty);

	/**
	 * @brief Reconstructs saved divisions after the campaign generator restores anchors and map objects.
	 * @param WorldPlayerController Controller that owns this manager and save system.
	 * @param WorldGenerator Restored campaign map provider used for influence targets.
	 * @param GameDifficulty Difficulty used to resolve current strength reason text.
	 * @param DivisionSaveData Serialized divisions from FWorldCampaignState.
	 */
	void RestoreSavedCampaignDivisions(AWorldPlayerController* WorldPlayerController,
	                                   AGeneratorWorldCampaign* WorldGenerator,
	                                   ERTSGameDifficulty GameDifficulty,
	                                   const TArray<FWorldDivisionSaveData>& DivisionSaveData);

	/**
	 * @brief Advances all player-owned divisions with pending orders during the player turn end step.
	 * @param GameDifficulty Difficulty forwarded to influence refresh after movement finishes.
	 */
	void MovePlayerDivisions(ERTSGameDifficulty GameDifficulty);

	/**
	 * @brief Advances all enemy-owned divisions with pending orders during the enemy turn end step.
	 * @param GameDifficulty Difficulty forwarded to influence refresh after movement finishes.
	 */
	void MoveEnemyDivisions(ERTSGameDifficulty GameDifficulty);

	/**
	 * @brief Rebuilds field-division strength reports on generated world map objects.
	 * @param GameDifficulty Cached for later damage refreshes and kept aligned with turn difficulty.
	 */
	void RefreshDivisionInfluence(ERTSGameDifficulty GameDifficulty);

	/**
	 * @brief Writes live division save data into the controller's world save manager cache.
	 */
	void CacheDivisionSaveState();

	/**
	 * @brief Issues a point-targeted move order to one registered or externally selected division.
	 * @param WorldDivision Division actor that should receive the move order.
	 * @param TargetLocation Arbitrary world-space point, not an anchor.
	 * @return true when the division accepted and cached a path.
	 */
	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	bool IssueMoveOrderToDivision(AWorldDivisionBase* WorldDivision, const FVector& TargetLocation);

	/**
	 * @brief Serializes all valid registered divisions for FWorldCampaignState.
	 * @return Save data array preserving owner, strength, composition, and pending path state.
	 */
	TArray<FWorldDivisionSaveData> BuildWorldDivisionSaveData() const;
	const TArray<TObjectPtr<AWorldDivisionBase>>& GetWorldDivisions() const { return M_WorldDivisions; }

private:
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;

	UPROPERTY()
	TWeakObjectPtr<UWorldDataComponent> M_WorldDataComponent;

	UPROPERTY()
	TArray<TObjectPtr<AWorldDivisionBase>> M_WorldDivisions;

	EWorldTurnType M_ActiveMovingTurn = EWorldTurnType::None;
	ERTSGameDifficulty M_CurrentGameDifficulty = ERTSGameDifficulty::Normal;
	ERTSGameDifficulty M_ActiveMovementDifficulty = ERTSGameDifficulty::Normal;
	int32 M_ActiveTurnMovementCount = 0;

	/**
	 * @brief Caches required runtime references before spawning, restoring, or moving divisions.
	 * @param WorldPlayerController Controller that owns save state and exposes world turn flow.
	 * @param WorldGenerator Current campaign generator whose placement state is authoritative.
	 * @return true when all references needed by the manager are valid.
	 */
	bool CacheRuntimeReferences(AWorldPlayerController* WorldPlayerController,
	                            AGeneratorWorldCampaign* WorldGenerator);

	/**
	 * @brief Removes live division actors and clears manager registration before reinitializing.
	 */
	void DestroyRegisteredDivisions();

	/**
	 * @brief Spawns divisions from UWorldCampaignSettings::InitialWorldDivisions.
	 * @param GameDifficulty Difficulty used for WorldData strength lookup.
	 */
	void SpawnInitialDivisions(ERTSGameDifficulty GameDifficulty);

	/**
	 * @brief Spawns divisions from serialized save entries.
	 * @param GameDifficulty Difficulty used for WorldData reason text and max strength lookup.
	 * @param DivisionSaveData Saved division array from FWorldCampaignState.
	 */
	void SpawnSavedDivisions(ERTSGameDifficulty GameDifficulty,
	                         const TArray<FWorldDivisionSaveData>& DivisionSaveData);

	/**
	 * @brief Spawns the configured actor class for one field division type.
	 * @param DivisionType Type used to resolve configured or fallback division class.
	 * @param SpawnTransform World transform for the new division actor.
	 * @return Spawned division actor, or nullptr if no valid class/world exists.
	 */
	AWorldDivisionBase* SpawnDivision(EWorldFieldDivisions DivisionType, const FTransform& SpawnTransform) const;

	/**
	 * @brief Tracks a live division and binds manager refresh logic to strength changes.
	 * @param WorldDivision Division actor to manage.
	 */
	void RegisterDivision(AWorldDivisionBase* WorldDivision);

	/**
	 * @brief Resolves designer-configured class with a tank/squad fallback by EWorldFieldDivisions.
	 * @param DivisionType Field division enum to resolve.
	 * @return Actor class to spawn, or nullptr for unsupported types.
	 */
	TSubclassOf<AWorldDivisionBase> ResolveDivisionClass(EWorldFieldDivisions DivisionType) const;

	/**
	 * @brief Applies runtime identity and WorldData strength fields to a spawned division actor.
	 * @param WorldDivision Spawned actor to initialize.
	 * @param DivisionKey Stable key for save/load.
	 * @param DivisionType Field division enum used for WorldData lookup.
	 * @param OwningPlayer Runtime owner id used by turn and influence rules.
	 * @param GameDifficulty Difficulty used for data-driven strength scaling.
	 */
	void InitializeDivisionFromWorldData(AWorldDivisionBase& WorldDivision,
	                                     const FGuid& DivisionKey,
	                                     EWorldFieldDivisions DivisionType,
	                                     int32 OwningPlayer,
	                                     ERTSGameDifficulty GameDifficulty) const;

	/**
	 * @brief Starts one side's turn-end movement for all matching divisions with pending orders.
	 * @param OwningPlayer Owner id to move this turn.
	 * @param TurnType Turn type stored while waiting for movement callbacks.
	 * @param GameDifficulty Difficulty used after movement to refresh influence.
	 */
	void MoveDivisionsForOwner(int32 OwningPlayer, EWorldTurnType TurnType, ERTSGameDifficulty GameDifficulty);

	/**
	 * @brief Receives movement-component completion callbacks and finishes the turn pass when all are done.
	 * @param WorldDivision Division whose visual movement just finished.
	 */
	void OnDivisionMovementFinished(AWorldDivisionBase* WorldDivision);

	/**
	 * @brief Refreshes influence/save state when damage changes a division's current strength.
	 * @param WorldDivision Division whose composition and strength changed.
	 */
	void OnDivisionStrengthChanged(AWorldDivisionBase* WorldDivision);

	/**
	 * @brief Runs the post-movement turn-end work: influence refresh, save cache, and campaign save.
	 */
	void FinishActiveTurnMovement();

	/**
	 * @brief Validates the cached world player controller before save or owner-sensitive work.
	 * @return true when the controller reference is still valid.
	 */
	bool GetIsValidWorldPlayerController() const;

	/**
	 * @brief Validates the cached generator before reading placement state or WorldData.
	 * @return true when the generator reference is still valid.
	 */
	bool GetIsValidWorldGenerator() const;

	/**
	 * @brief Validates the cached WorldData component before resolving division strength definitions.
	 * @return true when WorldData lookups can be attempted.
	 */
	bool GetIsValidWorldDataComponent() const;
};
