// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Game/Difficulty/GameDifficulty.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "WorldEnemyTurnDirector.generated.h"

class AAnchorPoint;
class AGeneratorWorldCampaign;
class AWorldDivisionBase;
class AWorldMapObject;
class AWorldPlayerController;
class UWorldCampaignProgressManager;
class UWorldDivisionManager;
class UWorldFortificationModificationsComponent;
class UWorldStrengthEstimationComponent;
struct FWorldEnemyTurnPlace;
struct FWorldEnemyTurnState;

/**
 * @brief What the last enemy turn did, for a future enemy-turn UI. Built from saved state, so it survives a reload.
 */
USTRUCT(BlueprintType)
struct RTS_SURVIVAL_API FWorldEnemyTurnSummary
{
	GENERATED_BODY()

	/** Enemy turns played including this one; 0 when none was played yet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 EnemyTurnNumber = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	float Alert = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 PointsToReaction = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 PointsToFreeze = 0;

	/** Reaction points the enemy keeps for later turns. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 PointsBanked = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 ConvoysSpawned = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 DivisionsSpawned = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 PiecesSpawned = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 ConvoysDelivered = 0;

	/** Finished freeze stages after this turn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 FreezeStage = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	int32 FreezeStagesGained = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Enemy Turn")
	bool bIsFreezeComplete = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldEnemyAlertChanged, float, NewAlert);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWorldFreezeProgress, int32, Stage, int32, Points, int32,
                                               PointsNeeded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldFreezeStageReached, int32, Stage);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldFreezeComplete);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldEnemyReactionSpawned, AWorldDivisionBase*, Piece);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWorldConvoyDelivered, FGuid, TargetAnchorKey,
                                             EWorldFortificationStrength, AddedFortification);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldEnemyTurnFinished, const FWorldEnemyTurnSummary&, Summary);

/**
 * @brief Plays the enemy turn on the world map: decays or keeps the alert, splits the budget, sends convoys and
 * divisions toward where the player struck, pays the freeze, moves every enemy piece and reports back once all
 * movement ended. Lives on AWorldPlayerController; the controller calls PlayEnemyTurn and saves in the callback.
 * All choices derive from the campaign seed and EnemyTurnsPlayed.
 */
UCLASS(ClassGroup=(WorldCampaign), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UWorldEnemyTurnDirector : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldEnemyTurnDirector();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief Caches the sibling managers. Call after the map exists (new campaign or load); PlayEnemyTurn also
	 * calls it when the owner is the world player controller.
	 * @param WorldPlayerController Controller that owns the progress, division and save managers.
	 */
	void InitializeEnemyTurnDirector(AWorldPlayerController* WorldPlayerController);

	/**
	 * @brief Plays one enemy turn. Does not save and does not advance the turn counter: the caller does both in
	 * OnFinished, after which the save holds the whole turn.
	 * @param GameDifficulty Campaign difficulty for strength lookups.
	 * @param OnFinished Called once, after all enemy movement ended. May run before this returns.
	 */
	void PlayEnemyTurn(ERTSGameDifficulty GameDifficulty, const FSimpleDelegate& OnFinished);

	/**
	 * @brief The player won the operation against a division or convoy: records where, so the next reaction goes
	 * there, and takes the piece off the map.
	 * @param DivisionKey Key of the defeated piece (FWorldPendingOperation::DivisionKey).
	 * @return false when no piece has this key.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Campaign|Enemy Turn")
	bool RemoveEnemyPieceDefeatedByPlayer(const FGuid& DivisionKey);

	UFUNCTION(BlueprintPure, Category = "World Campaign|Enemy Turn")
	bool GetIsEnemyTurnInProgress() const { return bM_IsEnemyTurnInProgress; }

	UFUNCTION(BlueprintPure, Category = "World Campaign|Enemy Turn")
	FWorldEnemyTurnSummary GetLastEnemyTurnSummary() const;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Enemy Turn")
	FOnWorldEnemyAlertChanged OnAlertChanged;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Enemy Turn")
	FOnWorldFreezeProgress OnFreezeProgress;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Enemy Turn")
	FOnWorldFreezeStageReached OnFreezeStageReached;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Enemy Turn")
	FOnWorldFreezeComplete OnFreezeComplete;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Enemy Turn")
	FOnWorldEnemyReactionSpawned OnEnemyReactionSpawned;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Enemy Turn")
	FOnWorldConvoyDelivered OnConvoyDelivered;

	UPROPERTY(BlueprintAssignable, Category = "World Campaign|Enemy Turn")
	FOnWorldEnemyTurnFinished OnEnemyTurnFinished;

private:
	UPROPERTY()
	TWeakObjectPtr<AWorldPlayerController> M_WorldPlayerController;

	UPROPERTY()
	TWeakObjectPtr<UWorldCampaignProgressManager> M_ProgressManager;

	UPROPERTY()
	TWeakObjectPtr<UWorldDivisionManager> M_DivisionManager;

	UPROPERTY()
	TWeakObjectPtr<AGeneratorWorldCampaign> M_WorldGenerator;

	// Held from PlayEnemyTurn until all enemy movement ended.
	FSimpleDelegate M_OnTurnFinished;

	ERTSGameDifficulty M_ActiveDifficulty = ERTSGameDifficulty::Normal;
	int32 M_ActiveTurnSeed = 0;
	bool bM_IsEnemyTurnInProgress = false;

	/** @return true when every sibling reference is cached, initializing from the owner when needed. */
	bool EnsureInitialized();

	FWorldEnemyTurnState* GetMutableEnemyTurnState() const;
	int32 GetCampaignSeed() const;

	/** @brief Decays the alert after a quiet player turn and tells listeners about the alert this turn uses. */
	void UpdateAlertForTurn(FWorldEnemyTurnState& EnemyTurnState);

	/**
	 * @brief Drops orders of pieces that are gone, retargets pieces whose target was cleared and re-paths
	 * pieces that stopped short of their target.
	 * @param EnemyTurnState State holding the piece orders.
	 */
	void RefreshExistingReactionOrders(FWorldEnemyTurnState& EnemyTurnState);

	/**
	 * @brief Brings one piece's order up to date.
	 * @param EnemyTurnState State holding the piece orders.
	 * @param PieceKey Key of the piece.
	 * @param TargetCandidates Non-cleared targets on the map.
	 */
	void RefreshReactionOrder(FWorldEnemyTurnState& EnemyTurnState, const FGuid& PieceKey,
	                          const TArray<FWorldEnemyTurnPlace>& TargetCandidates);

	/**
	 * @brief Buys convoys and divisions with this turn's reaction share plus banked points and sends them out;
	 * points that buy nothing are banked.
	 * @param EnemyTurnState State to update.
	 * @param ReactionPoints This turn's reaction share.
	 */
	void SpendReactionPoints(FWorldEnemyTurnState& EnemyTurnState, int32 ReactionPoints);

	/**
	 * @brief Spawns one piece at the source nearest its target and orders it there.
	 * @param EnemyTurnState State that records the order.
	 * @param PieceType Convoy or division type.
	 * @param Target Where the piece goes.
	 * @param PieceIndex Index of the piece this turn; part of its key.
	 * @param Sources Places that can send pieces.
	 * @param InOutSpawnCountBySource Pieces already sent from each source this turn, to fan them out.
	 * @return true when the piece is on the map with an order.
	 */
	bool TrySendReactionPiece(FWorldEnemyTurnState& EnemyTurnState, EWorldFieldDivisions PieceType,
	                          const FWorldEnemyTurnPlace& Target, int32 PieceIndex,
	                          const TArray<FWorldEnemyTurnPlace>& Sources,
	                          TMap<FGuid, int32>& InOutSpawnCountBySource);

	/**
	 * @brief Pays the freeze share and fires the progress, stage and completion events.
	 * @param EnemyTurnState State to update.
	 * @param FreezePoints This turn's freeze share.
	 */
	void PayFreeze(FWorldEnemyTurnState& EnemyTurnState, int32 FreezePoints);

	void StartEnemyMovement();
	void OnEnemyMovementFinished();

	/** @brief Consumes every convoy that arrived and fortifies its target. */
	void ResolveConvoyArrivals(FWorldEnemyTurnState& EnemyTurnState);

	/**
	 * @brief Fortifies a target: adds the first fortification modifier it lacks, else raises its base strength.
	 * Both are part of the map item save data, so the bonus lasts.
	 * @param TargetObject Enemy item or mission the convoy reached.
	 * @return The modifier added, or None when base strength was raised instead.
	 */
	EWorldFortificationStrength ApplyConvoyDelivery(AWorldMapObject& TargetObject) const;

	/**
	 * @brief Rebuilds the fortification modifier lines of the strength report from the modifier component.
	 * @param FortificationComponent Modifiers of the target.
	 * @param StrengthComponent Strength report of the target.
	 */
	void RebuildFortificationModifierReport(const UWorldFortificationModificationsComponent& FortificationComponent,
	                                        UWorldStrengthEstimationComponent& StrengthComponent) const;

	/** @brief Clears the strike records, counts the turn, stores the summary, caches the save and reports back. */
	void FinishEnemyTurn(FWorldEnemyTurnState& EnemyTurnState);

	/** @brief Re-caches map items (for convoy deliveries) and divisions in the save manager. */
	void CacheTurnResultsForSave() const;

	void OnDivisionRemoved(const FGuid& DivisionKey, const FVector& LastLocation);

	// --- Map queries ---

	AAnchorPoint* FindAnchorByKey(const FGuid& AnchorKey) const;
	AWorldMapObject* FindMapObjectByAnchorKey(const FGuid& AnchorKey) const;
	bool GetIsAnchorCleared(const FGuid& AnchorKey) const;

	/** @return Non-cleared enemy items and missions: what reaction pieces may reinforce. */
	TArray<FWorldEnemyTurnPlace> BuildTargetCandidates() const;

	/** @return The enemy HQ and the non-cleared enemy items configured as sources. */
	TArray<FWorldEnemyTurnPlace> BuildSourceCandidates() const;

	/** @return Where the player struck since the last enemy turn; the player HQ when there was no strike. */
	TArray<FVector2D> BuildStruckLocations(const FWorldEnemyTurnState& EnemyTurnState) const;

	bool GetIsValidWorldPlayerController() const;
	bool GetIsValidProgressManager() const;
	bool GetIsValidDivisionManager() const;
	bool GetIsValidWorldGenerator() const;
};
