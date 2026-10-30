// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveTypes.h"

class USurvivalWaveSettings;

/**
 * @brief One unit the composer bought for a wave.
 */
struct FSurvivalPlannedWaveUnit
{
	FTrainingOption Option;
	ESurvivalWaveUnitKind Kind = ESurvivalWaveUnitKind::Tank;
	ESurvivalWaveUnitRole Role = ESurvivalWaveUnitRole::Infantry;
	int32 Cost = 0;
	int32 PoolIndex = INDEX_NONE;
};

/**
 * @brief Inputs of one composition: the minute decides the tech stage, the kind biases roles.
 */
struct FSurvivalWaveCompositionRequest
{
	float SurvivalMinute = 0.f;
	float Budget = 0.f;
	int32 MaxUnits = 1;
	float TechPaceMultiplier = 1.f;
	const FSurvivalWaveKindDefinition* KindDefinition = nullptr;
};

/**
 * @brief Stateless budget-to-units logic of the survival waves, kept apart from scheduling so the growth model
 * can be reasoned about (and mirrored in the docs' simulation) in isolation.
 */
class RTS_SURVIVAL_API FSurvivalWaveComposer
{
public:
	/** @return Highest tech stage whose unlock minute has passed. */
	static EEnemyAITechLevel GetTechStageAtMinute(
		const USurvivalWaveSettings& Settings,
		const float SurvivalMinute,
		const float TechPaceMultiplier);

	/**
	 * @brief Fills the budget with weighted random picks from the unlocked pool; once MaxUnits is reached,
	 * leftover budget swaps the cheapest units for pricier ones so late waves get stronger, not only bigger.
	 * @param Settings Pool, tech stages and ramp/fade rules.
	 * @param Request Minute, budget, unit cap, pace and wave kind.
	 * @param RandomStream Source of all randomness.
	 * @param OutUnits Receives the bought units (at least one when the pool has an unlocked entry).
	 * @return Points spent.
	 */
	static int32 ComposeWave(
		const USurvivalWaveSettings& Settings,
		const FSurvivalWaveCompositionRequest& Request,
		FRandomStream& RandomStream,
		TArray<FSurvivalPlannedWaveUnit>& OutUnits);

	/**
	 * @brief Pick weight of one pool entry at this minute for this wave kind (0 = not available).
	 * @param Settings Tech stage unlocks and ramp settings.
	 * @param Entry Pool entry.
	 * @param Request Minute, pace and kind.
	 * @return Effective weight.
	 */
	static float GetEntryWeight(
		const USurvivalWaveSettings& Settings,
		const FSurvivalWaveUnitEntry& Entry,
		const FSurvivalWaveCompositionRequest& Request);

private:
	struct FWeightedPoolEntry
	{
		int32 PoolIndex = INDEX_NONE;
		int32 Cost = 0;
		float Weight = 0.f;
	};

	static TArray<FWeightedPoolEntry> BuildWeightedPool(
		const USurvivalWaveSettings& Settings,
		const FSurvivalWaveCompositionRequest& Request);

	static int32 PickWeighted(
		const TArray<FWeightedPoolEntry>& Pool,
		const int32 MinCostExclusive,
		const int32 MaxCostInclusive,
		FRandomStream& RandomStream);

	static FSurvivalPlannedWaveUnit MakePlannedUnit(
		const USurvivalWaveSettings& Settings,
		const FWeightedPoolEntry& PoolEntry);

	static int32 FillBudget(
		const USurvivalWaveSettings& Settings,
		const TArray<FWeightedPoolEntry>& Pool,
		const FSurvivalWaveCompositionRequest& Request,
		FRandomStream& RandomStream,
		TArray<FSurvivalPlannedWaveUnit>& OutUnits);

	/**
	 * @brief Swaps the cheapest planned units for pricier pool entries while budget remains.
	 * @param Settings Pool and the max replacement count.
	 * @param Pool Weighted unlocked entries.
	 * @param RemainingBudget Points still unspent; reduced by every swap.
	 * @param RandomStream Source of randomness.
	 * @param InOutUnits Planned units, modified in place.
	 */
	static void UpgradeCheapestUnits(
		const USurvivalWaveSettings& Settings,
		const TArray<FWeightedPoolEntry>& Pool,
		int32& RemainingBudget,
		FRandomStream& RandomStream,
		TArray<FSurvivalPlannedWaveUnit>& InOutUnits);
};
