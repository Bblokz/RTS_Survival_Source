#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"
#include "RTS_Survival/Enemy/StrategicAI/StrategicActions/StrategicActions.h"

struct FStrategicAIAction;
class UStrategicAISubAction;
class UEnemyNavigationAIComponent;
struct FBlackboardIdleUnitsResult;

struct FStochasticPathFindingParams
{
	const UObject* WorldContextObject = nullptr;
	const UEnemyNavigationAIComponent* NavComp = nullptr;
	FVector StartLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FStochasticPathFindingSettings Settings;
	bool bUseStartLocationAsFirstPathPoint = true;
};

/**
 * @namespace StochasticHelpers
 * @brief Contains helper functions for weighted stochastic selection of strategic AI actions and subactions.
 *
 * All selection is based on linear probability distribution:
 * Each option contributes its score linearly to the total distribution.
 * 
 * Deterministic mode:
 * If bUseSeed is true, the result is deterministic for identical (Seed, Time).
 */
namespace StochasticHelpers
{
	/**
	 * @brief Picks a weighted strategic action from a list of action pointers.
	 *
	 * Uses linear probability distribution based on action scores.
	 * Example:
	 * Scores [1, 2.5] → probabilities [1/3.5, 2.5/3.5].
	 *
	 * @param ActionDefinitions Array of action pointers to choose from.
	 * @param bUseSeed If true, uses deterministic seeded randomness.
	 * @param Seed Seed value for deterministic selection.
	 * @param Time Time value combined with Seed for deterministic variation.
	 * @return Pointer to the selected action, or nullptr if selection failed.
	 */
	const FStrategicAIAction* PickStrategicAction(
		const TArray<FStrategicAIAction*>& ActionDefinitions,
		const bool bUseSeed,
		const float Seed,
		const float Time);

	/**
	 * @brief Picks a weighted sub-action from a list of subaction pointers.
	 *
	 * Uses linear probability distribution based on subaction scores.
	 *
	 * @param SubActions Array of subaction pointers to choose from.
	 * @param bUseSeed If true, uses deterministic seeded randomness.
	 * @param Seed Seed value for deterministic selection.
	 * @param Time Time value combined with Seed for deterministic variation.
	 * @return Pointer to the selected subaction, or nullptr if selection failed.
	 */
	UStrategicAISubAction* PickSubAction(
		const TArray<UStrategicAISubAction*>& SubActions,
		const bool bUseSeed,
		const float Seed,
		const float Time);

	
	FVector PickRandomLocation(const TArray<FVector>& Locations);

	/**
	 * @brief Picks up to MaxLocations random unique locations without picking the same location twice.
	 *
	 * @param Locations Source locations.
	 * @param MaxLocations Maximum number of locations to return.
	 * @return Randomly picked locations with no duplicates.
	 */
	TArray<FVector> PickRandomMaxLocations(const TArray<FVector>& Locations, int32 MaxLocations);

	/**
	 * @brief Builds closest-neighbour pairs and returns one random pair.
	 *
	 * If only one location exists, it returns an array containing that single location.
	 *
	 * @param Locations Source locations.
	 * @return Either 0, 1, or 2 locations depending on input size.
	 */
	TArray<FVector> PickRandomClosestPair(const TArray<FVector>& Locations);

	/**
	 * @brief Picks up to two locations closest to a tactical anchor.
	 *
	 * If only one candidate exists, that location is returned immediately.
	 *
	 * @param OriginalPoint Anchor used for distance sorting.
	 * @param Locations Candidate locations that do not include OriginalPoint.
	 * @return Zero, one, or two closest locations depending on available candidates.
	 */
	TArray<FVector> PickPairOfPointsClosestTo(const FVector& OriginalPoint, const TArray<FVector>& Locations);

	/**
	 * @brief Randomly picks unique locations until MaxPick is reached or no options remain.
	 * @param Locations Source locations.
	 * @param MaxPick Maximum amount of unique locations to return.
	 * @return Unique random picks in picked order.
	 */
	TArray<FVector> ExhaustivePick(const TArray<FVector>& Locations, int32 MaxPick);

	/**
	 * @brief Resamples an actual nav path so long strategic moves advance through manageable chunks.
	 * @param Params Groups the world, navigation, endpoints, density, projection, cap, and debug settings.
	 * @return Start-to-target path when pathfinding succeeds; empty when no valid nav path could be built.
	 */
	TArray<FVector> BuildNavigablePathPoints(const FStochasticPathFindingParams& Params);

	/**
	 * @brief Computes a shared tactical anchor from picked units so follow-up logic can reason from one stable center.
	 *
	 * Uses one accumulation pass over tanks and squads and one final reciprocal multiply for the average.
	 * Assumes all picked entries are valid by contract and performs no validity checks.
	 *
	 * @param PickedBlackboardUnits Picked tank/squad result that contributes actor world locations.
	 * @return Average world location across all picked units, or ZeroVector when no units were picked.
	 */
	FVector GetAverageLocationPickedBlackboardUnits(const FBlackboardIdleUnitsResult& PickedBlackboardUnits);

	// Specifically for bulk locations which should be projected the same way everytime.
	bool CanProjectNavigable_BulkLocation(
		const UEnemyNavigationAIComponent* NavComp, const FVector& BulkLocation, FVector& OutProjectedLocation);

	// Specifically for avg attacker (player unit groups) locations which should be projected the same way everytime.
	// Uses larger extension as attackers may be spread.
	bool CanProjectNavigable_AverageLocationAttacker(
		const UEnemyNavigationAIComponent* NavComp, const FVector& AvgAttackerLoc, FVector& OutProjectedLocation);
	
	// Specifically for player HQ locations which should be projected the same way everytime.
	bool CanProjectNavigable_PlayerHQLocation(
		const UEnemyNavigationAIComponent* NavComp, const FVector& PlayerHQLocation, FVector& OutProjectedLocation);

	// Specifically for player resource building locations which should be projected the same way everytime.
	bool CanProjectNavigable_PlayerResourceBuildingLocation(
		const UEnemyNavigationAIComponent* NavComp, const FVector& PlayerResourceBuildingLocation,
		FVector& OutProjectedLocation);

	// Specifically for designer-authored attack points used by specific point sub-actions.
	bool CanProjectNavigable_AttackSpecificPoint(
		const UEnemyNavigationAIComponent* NavComp, const FVector& SpecificPoint, FVector& OutProjectedLocation);

	// Specifically for flanking locations generated by the async strategic AI.
	bool CanProjectNavigable_FlankLocation(
		const UEnemyNavigationAIComponent* NavComp, const FVector& FlankLocation, FVector& OutProjectedLocation);

	// Specifically for average picked blackboard units location.
	bool CanProjectNavigable_AveragePickedUnitLocation(
		const UEnemyNavigationAIComponent* NavComp,
		const FVector& AverageUnitLocation,
		FVector& OutProjectedLocation);

	void SortArrayByDistanceToLocation(TArray<FVector>& OutLocations, const FVector& TargetLocation);

	
		
	

/**
 * @brief Retrieves the string representation of an EStrategicAITopLevelAction enum value.
 *
 * @param EnumValue The enum value to convert.
 * @return FString representation of the enum value.
 */
inline FString GetStrategicAITopLevelActionAsString(const EStrategicAITopLevelAction EnumValue)
{
	const UEnum* const EnumPtr = StaticEnum<EStrategicAITopLevelAction>();
	if (EnumPtr == nullptr)
	{
		return TEXT("Invalid Enum");
	}

	return EnumPtr->GetNameStringByValue(static_cast<int64>(EnumValue));
}

}
