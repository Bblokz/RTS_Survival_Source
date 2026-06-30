#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/EnemyTopologySearchStrategy/Enum_EnemyTopologySearchStrategy.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapEnemyItem.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/TopologySearchStrategy/Enum_TopologySearchStrategy.h"

class AAnchorPoint;

namespace CampaignGenerationHelper
{
	/**
	 * @brief Builds a hop-distance cache for all anchors reachable from the HQ.
	 * @param HQAnchor Anchor that defines the hop-distance origin.
	 * @param OutHopDistances Map of anchor keys to hop distances from the HQ.
	 */
	void BuildHopDistanceCache(const AAnchorPoint* HQAnchor, TMap<FGuid, int32>& OutHopDistances);
	int32 HopsFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor);
	float XYDistanceFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor);
	void AddChokepointPathContribution(const TArray<FGuid>& PathKeys, TMap<FGuid, float>& InOutScores);

	/**
	 * @brief Scores enemy candidates consistently between game-thread and async placement paths.
	 * @param Preference Topology preference requested by the active enemy placement rules.
	 * @param ConnectionDegree Number of connections on the candidate anchor.
	 * @param ChokepointScore Cached chokepoint value for the candidate anchor.
	 * @param HopDistanceFromEnemyHQ Candidate hop distance from the enemy HQ.
	 * @param MinHopsFromEnemyHQ Minimum allowed enemy-HQ hop bound.
	 * @param MaxHopsFromEnemyHQ Maximum allowed enemy-HQ hop bound.
	 * @return Higher values are preferred during deterministic candidate sorting.
	 */
	float GetEnemyPreferenceScore(EEnemyTopologySearchStrategy Preference, int32 ConnectionDegree,
	                              float ChokepointScore, int32 HopDistanceFromEnemyHQ,
	                              int32 MinHopsFromEnemyHQ, int32 MaxHopsFromEnemyHQ);
	float GetEnemyWallPreferenceScore(EEnemyTopologySearchStrategy Preference, int32 ConnectionDegree,
	                                  float ChokepointScore);
	float GetTopologyPreferenceScore(ETopologySearchStrategy Preference, float Value);
	bool HasMinimumAdjacentMatches(const TArray<int32>& HopDistances, int32 RequiredCount);

	/**
	 * @brief Scores mission override candidates from shared connection and hop preferences.
	 * @param OverrideConnectionPreference Preference applied to connection degree.
	 * @param OverrideHopsPreference Preference applied to player-HQ hop distance.
	 * @param ConnectionDegree Number of connections on the candidate anchor.
	 * @param HopDistanceFromHQ Candidate hop distance from the player HQ.
	 * @return Combined preference score, where higher values sort earlier.
	 */
	float GetOverrideMissionPreferenceScore(ETopologySearchStrategy OverrideConnectionPreference,
	                                        ETopologySearchStrategy OverrideHopsPreference,
	                                        int32 ConnectionDegree,
	                                        int32 HopDistanceFromHQ);
	int32 GetHopPreferenceWeight(int32 EffectiveMaxWeight, int32 EffectiveFalloff,
	                             int32 CandidateHopDistance, int32 TargetHopDistance);
	bool GetIsSwappableEnemyType(EMapEnemyItem EnemyType);

	/**
	 * @brief Measures same-type enemy clustering around one anchor for deterministic declustering.
	 * @param TargetAnchorKey Anchor being evaluated.
	 * @param TargetEnemyType Enemy type being compared.
	 * @param TargetLocationXY Cached 2D location for the target anchor.
	 * @param EnemyTypesByAnchorKey Current enemy type assignment by anchor.
	 * @param AnchorLocationsByKey Cached 2D anchor locations.
	 * @return Count of nearby same-type enemies inside the decluster radius.
	 */
	int32 ComputeEnemyLocalDensity(const FGuid& TargetAnchorKey,
	                               EMapEnemyItem TargetEnemyType,
	                               const FVector2D& TargetLocationXY,
	                               const TMap<FGuid, EMapEnemyItem>& EnemyTypesByAnchorKey,
	                               const TMap<FGuid, FVector2D>& AnchorLocationsByKey);
	FString BuildCanonicalFailedEnemyDeclusterSwapPairKey(const FGuid& AnchorKeyA, const FGuid& AnchorKeyB);
	void BuildImpactedEnemyDeclusterAnchorKeys(const FGuid& AnchorKeyA,
	                                           const FGuid& AnchorKeyB,
	                                           const TArray<TPair<FGuid, EMapEnemyItem>>& SortedSwappableAnchors,
	                                           const TMap<FGuid, FVector2D>& AnchorLocationsByKey,
	                                           TArray<FGuid>& OutImpactedAnchorKeys);
	int32 ComputeImpactedEnemyDeclusterDensitySum(const TArray<FGuid>& ImpactedAnchorKeys,
	                                              const TMap<FGuid, EMapEnemyItem>& EnemyTypesByAnchorKey,
	                                              const TMap<FGuid, FVector2D>& AnchorLocationsByKey);

	template<typename ElementType>
    	void DeterministicShuffle(TArray<ElementType>& InOutArray, FRandomStream& RandomStream)
    	{
    		const int32 ElementCount = InOutArray.Num();
    		if (ElementCount < 2)
    		{
    			return;
    		}
    
    		// Fisher-Yates shuffle driven by FRandomStream (deterministic for a given seed).
    		for (int32 LastIndex = ElementCount - 1; LastIndex > 0; --LastIndex)
    		{
    			const int32 SwapIndex = RandomStream.RandRange(0, LastIndex);
    			if (SwapIndex == LastIndex)
    			{
    				continue;
    			}
    
    			InOutArray.Swap(LastIndex, SwapIndex);
    		}
    	}
}
