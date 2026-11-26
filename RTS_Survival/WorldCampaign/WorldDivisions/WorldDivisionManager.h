// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Turn/WorldTurnType.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/SaveData/FWorldCampaignState.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthContribution.h"
#include "WorldDivisionManager.generated.h"

class AGeneratorWorldCampaign;
class AWorldDivisionBase;
class AWorldPlayerController;
class UWorldDataComponent;
struct FWorldEnemyReactionPieceSetup;

/** Native: a division or convoy left the map (destroyed, consumed or removed after a won operation). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldDivisionRemoved, const FGuid& /*DivisionKey*/,
                                     const FVector& /*LastLocation*/);

/** What happens to the actor of a division that leaves the manager. */
enum class EWorldDivisionUnregisterMode : uint8
{
	DestroyActor,
	// The caller owns the actor from now on and destroys it later.
	KeepActor
};

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

	/**
	 * @brief Moves enemy pieces for the turn and reports back only when every movement (of any side) has ended.
	 * @param GameDifficulty Difficulty forwarded to influence refresh after movement finishes.
	 * @param OnMovementFinished Called once all movement finished; may run before this returns when nothing moves.
	 * @note Does not save; the caller saves once its whole turn is done.
	 */
	void MoveEnemyDivisionsThenNotify(ERTSGameDifficulty GameDifficulty, const FSimpleDelegate& OnMovementFinished);

	/** @return true while a turn movement pass waits for division movement to end. */
	bool GetIsTurnMovementInProgress() const { return M_ActiveMovingTurn != EWorldTurnType::None; }

	/**
	 * @brief Spawns an enemy-owned convoy or division during the enemy turn with its configured roster.
	 * @param PieceType Division or convoy type.
	 * @param SpawnLocation World location of the new piece.
	 * @param DivisionKey Stable key; the enemy turn derives it from the campaign seed.
	 * @param GameDifficulty Difficulty used for WorldData strength lookup.
	 * @return The registered piece, or nullptr when it could not be spawned.
	 */
	AWorldDivisionBase* SpawnEnemyReactionPiece(EWorldFieldDivisions PieceType,
	                                            const FVector& SpawnLocation,
	                                            const FGuid& DivisionKey,
	                                            ERTSGameDifficulty GameDifficulty);

	UFUNCTION(BlueprintPure, Category="World Campaign|World Divisions")
	AWorldDivisionBase* FindDivisionByKey(const FGuid& DivisionKey) const;

	/**
	 * @brief Takes a division or convoy off the map for good, e.g. after the player won the operation against it.
	 * @param DivisionKey Key of the piece.
	 * @return false when no piece has this key.
	 * @note Refreshes influence and the division save cache; the caller saves the campaign.
	 */
	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	bool RemoveDivisionByKey(const FGuid& DivisionKey);

	/**
	 * @brief Takes a division off the map's books like RemoveDivisionByKey, but leaves its actor standing so a show
	 * (the battery fire mission) can still blow it up. The actor no longer takes clicks and destroys itself after a
	 * while if the caller does not.
	 * @param DivisionKey Key of the piece.
	 * @return The released actor for the caller to destroy, or nullptr when no piece has this key.
	 * @note Refreshes influence and the division save cache; the caller saves the campaign.
	 */
	AWorldDivisionBase* ReleaseDivisionForDeferredDestroy(const FGuid& DivisionKey);

	/**
	 * @brief Weakens every enemy division and convoy within an XY radius, e.g. for an airfield or battery strike.
	 * A piece that loses its last tank, squad or truck is removed.
	 * @param Center Strike centre in world space.
	 * @param Radius XY radius of the strike.
	 * @param DamageEntryCount Whole tanks, squads or trucks each piece loses.
	 * @return Number of pieces hit.
	 */
	UFUNCTION(BlueprintCallable, Category="World Campaign|World Divisions")
	int32 ApplyDamageToEnemyDivisionsInRadius(const FVector& Center, float Radius, int32 DamageEntryCount);

	/**
	 * @brief Strength ledger for an operation against one division or convoy: its own strength plus every other
	 * enemy piece whose influence radius covers it.
	 * @param DivisionKey Key of the attacked piece.
	 * @return Contributions for FWorldMissionContext::StrengthContributions; empty when the key is unknown.
	 */
	TArray<FWorldStrengthContribution> BuildOperationStrengthContributionsForDivision(const FGuid& DivisionKey) const;

	FOnWorldDivisionRemoved& OnDivisionRemoved() { return M_OnDivisionRemoved; }

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

	// Keys of divisions whose movement still plays. Shared by overlapping player and enemy passes, so a later pass
	// never resets an earlier pass that is still waiting.
	TSet<FGuid> M_DivisionKeysMovingThisPass;

	// Callers waiting for the current movement passes; run once after the last mover reported back.
	TArray<FSimpleDelegate> M_MovementFinishedCallbacks;

	// Set by any pass that asked for a campaign save once movement ends.
	bool bM_ShouldSaveWhenMovementFinished = false;

	// True while a pass starts its movers, so a mover finishing at once cannot end the pass early.
	bool bM_IsStartingMovementPass = false;

	FOnWorldDivisionRemoved M_OnDivisionRemoved;

	/**
	 * @brief Caches required runtime references before spawning, restoring, or moving divisions.
	 * @param WorldPlayerController Controller that owns save state and exposes world turn flow.
	 * @param WorldGenerator Current campaign generator whose placement state is authoritative.
	 * @return true when all references needed by the manager are valid.
	 */
	bool CacheRuntimeReferences(AWorldPlayerController* WorldPlayerController,
	                            AGeneratorWorldCampaign* WorldGenerator);

	/**
	 * @brief The one way a piece leaves the manager: stops its movement, unregisters it, refreshes influence and the
	 * save cache and tells the listeners.
	 * @param DivisionKey Key of the piece.
	 * @param Mode Whether the actor is destroyed here or handed to the caller.
	 * @return The unregistered actor (pending kill in DestroyActor mode), or nullptr when no piece has this key.
	 */
	AWorldDivisionBase* UnregisterDivisionByKey(const FGuid& DivisionKey, EWorldDivisionUnregisterMode Mode);

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
	 * @param bSaveWhenFinished Whether the campaign is saved once all movement ended.
	 * @param OnMovementFinished Optional callback run once all movement ended.
	 */
	void MoveDivisionsForOwner(int32 OwningPlayer, EWorldTurnType TurnType, ERTSGameDifficulty GameDifficulty,
	                           bool bSaveWhenFinished, const FSimpleDelegate& OnMovementFinished);

	/**
	 * @brief Binds the finish callback and starts one division's turn movement.
	 * @param WorldDivision Division with a pending move order.
	 * @return true when movement started and the pass must wait for it.
	 */
	bool TryStartTurnMovement(AWorldDivisionBase& WorldDivision);

	/** @brief Drops moving keys whose division is gone or no longer moving, so a pass can never hang. */
	void PruneStaleMovingDivisionKeys();

	/** @brief Ends the active pass once no division is moving and no pass is still starting. */
	void TryFinishMovementPassIfIdle();

	/**
	 * @brief Applies the configured roster of a reaction piece; an empty roster keeps the class defaults.
	 * @param WorldDivision Freshly spawned and initialized piece.
	 * @param PieceSetup Configured setup, or nullptr.
	 */
	void ApplyReactionPieceRoster(AWorldDivisionBase& WorldDivision, const FWorldEnemyReactionPieceSetup* PieceSetup) const;

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
	 * @brief Runs the post-movement turn-end work: influence refresh, save cache, the campaign save when a pass
	 * asked for it, then the waiting callbacks.
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
