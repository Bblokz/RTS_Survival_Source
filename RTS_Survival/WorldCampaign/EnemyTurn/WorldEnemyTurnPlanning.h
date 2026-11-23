// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressState.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"

/**
 * @brief What the enemy buys with its reaction points in one turn.
 */
struct RTS_SURVIVAL_API FWorldEnemyReactionPurchase
{
	int32 DivisionCount = 0;
	int32 ConvoyCount = 0;
	int32 PointsSpent = 0;
	int32 PointsLeft = 0;

	int32 GetPieceCount() const { return DivisionCount + ConvoyCount; }
};

/**
 * @brief A place on the map the enemy turn can choose: a target to reinforce or a source to send pieces from.
 */
struct RTS_SURVIVAL_API FWorldEnemyTurnPlace
{
	FGuid Key;
	FVector2D Location = FVector2D::ZeroVector;
};

/**
 * @brief The enemy turn's choices as pure functions of plain data, so the director only gathers map data and
 * applies the result, and every choice can be unit tested. All choices are deterministic for equal input.
 */
namespace WorldEnemyTurnPlanning
{
	/**
	 * @brief A won operation raises MissionsCompleted, also for division targets that record no anchor.
	 * @param State Enemy turn state as the progress manager holds it.
	 * @param MissionsCompleted Current number of won operations.
	 * @return true when the alert must not decay this enemy turn.
	 */
	RTS_SURVIVAL_API bool GetHasPlayerStruckSinceLastEnemyTurn(const FWorldEnemyTurnState& State,
	                                                          int32 MissionsCompleted);

	/**
	 * @brief Buys divisions first (they stay and hold ground), then convoys with what is left.
	 * @param AvailablePoints This turn's reaction share plus banked points.
	 * @param ConvoyCost Reaction points one convoy costs.
	 * @param DivisionCost Reaction points one division costs.
	 * @param MaxPieces Most pieces bought this turn.
	 * @return Counts bought, points spent and points left over.
	 */
	RTS_SURVIVAL_API FWorldEnemyReactionPurchase PlanReactionPurchase(int32 AvailablePoints, int32 ConvoyCost,
	                                                                  int32 DivisionCost, int32 MaxPieces);

	/** @return Leftover points the enemy keeps for later, capped at MaxBankedPoints. */
	RTS_SURVIVAL_API int32 GetBankedPointsAfterPurchase(const FWorldEnemyReactionPurchase& Purchase,
	                                                   int32 MaxBankedPoints);

	/**
	 * @brief Ranks candidates by their distance to the nearest struck location, ties broken by key.
	 * @param StruckLocations Where the player struck; empty returns no targets.
	 * @param Candidates Non-cleared enemy items and missions.
	 * @param MaxTargets Most targets returned.
	 * @return Indices into Candidates, nearest first.
	 */
	RTS_SURVIVAL_API TArray<int32> SelectReactionTargetIndices(const TArray<FVector2D>& StruckLocations,
	                                                           const TArray<FWorldEnemyTurnPlace>& Candidates,
	                                                           int32 MaxTargets);

	/**
	 * @brief Nearest place to a location, ties broken by key.
	 * @return Index into Places, or INDEX_NONE when Places is empty.
	 */
	RTS_SURVIVAL_API int32 SelectNearestPlaceIndex(const FVector2D& Location, const TArray<FWorldEnemyTurnPlace>& Places);

	/** @return The target index for the n-th piece: pieces are dealt round-robin over the targets. */
	RTS_SURVIVAL_API int32 GetTargetIndexForPiece(int32 PieceIndex, int32 TargetCount);

	/**
	 * @brief Lists the piece types to spawn: divisions first (type picked by the turn seed), then convoys.
	 * @param Purchase What was bought.
	 * @param DivisionTypes Types a division may be; empty yields no divisions.
	 * @param TurnSeed Seed of this enemy turn.
	 * @return One entry per piece.
	 */
	RTS_SURVIVAL_API TArray<EWorldFieldDivisions> PlanPieceTypes(const FWorldEnemyReactionPurchase& Purchase,
	                                                             const TArray<EWorldFieldDivisions>& DivisionTypes,
	                                                             int32 TurnSeed);

	RTS_SURVIVAL_API int32 BuildEnemyTurnSeed(int32 CampaignSeed, int32 EnemyTurnsPlayed);

	/** @return A valid key that is the same for the same campaign, enemy turn and piece index. */
	RTS_SURVIVAL_API FGuid BuildReactionPieceKey(int32 CampaignSeed, int32 EnemyTurnsPlayed, int32 PieceIndex);

	/**
	 * @brief Picks the fortification a convoy adds: the first missing one, starting at a seeded offset.
	 * @param ExistingModifiers Modifiers the target already has.
	 * @param TurnSeed Seed of this enemy turn.
	 * @return The modifier to add, or None when the target already has all of them.
	 */
	RTS_SURVIVAL_API EWorldFortificationStrength SelectConvoyDeliveryModifier(
		const TArray<EWorldFortificationStrength>& ExistingModifiers, int32 TurnSeed);

	/**
	 * @brief Direction from a source toward a target, fanned out per piece so pieces from one source do not stack.
	 * @param SourceLocation Where the piece is sent from.
	 * @param TargetLocation Where the piece is sent to.
	 * @param PieceIndexAtSource 0 for the first piece from this source this turn.
	 * @return Unit direction in XY.
	 */
	RTS_SURVIVAL_API FVector2D BuildSpawnDirection(const FVector2D& SourceLocation, const FVector2D& TargetLocation,
	                                               int32 PieceIndexAtSource);
}
