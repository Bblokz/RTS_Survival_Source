// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalWaveComposer.h"

#include "RTS_Survival/SurvivalMode/Waves/SurvivalWaveSettings.h"

namespace SurvivalWaveComposerConstants
{
	const TArray<EEnemyAITechLevel> TechStagesInOrder = {
		EEnemyAITechLevel::BasicInfantry,
		EEnemyAITechLevel::LightTanks,
		EEnemyAITechLevel::MediumTanks,
		EEnemyAITechLevel::Tier2,
		EEnemyAITechLevel::AdvancedInfantry,
		EEnemyAITechLevel::Tier3,
		EEnemyAITechLevel::Experimental
	};
}

EEnemyAITechLevel FSurvivalWaveComposer::GetTechStageAtMinute(
	const USurvivalWaveSettings& Settings,
	const float SurvivalMinute,
	const float TechPaceMultiplier)
{
	EEnemyAITechLevel HighestStage = EEnemyAITechLevel::BasicInfantry;
	for (const EEnemyAITechLevel EachStage : SurvivalWaveComposerConstants::TechStagesInOrder)
	{
		if (SurvivalMinute >= Settings.GetTechStageUnlockMinute(EachStage, TechPaceMultiplier))
		{
			HighestStage = EachStage;
		}
	}
	return HighestStage;
}

int32 FSurvivalWaveComposer::ComposeWave(
	const USurvivalWaveSettings& Settings,
	const FSurvivalWaveCompositionRequest& Request,
	FRandomStream& RandomStream,
	TArray<FSurvivalPlannedWaveUnit>& OutUnits)
{
	OutUnits.Reset();
	const TArray<FWeightedPoolEntry> Pool = BuildWeightedPool(Settings, Request);
	if (Pool.IsEmpty())
	{
		return 0;
	}

	const int32 Budget = FMath::Max(0, FMath::RoundToInt(Request.Budget));
	const int32 SpentOnFill = FillBudget(Settings, Pool, Request, RandomStream, OutUnits);
	int32 RemainingBudget = FMath::Max(0, Budget - SpentOnFill);
	if (OutUnits.Num() >= Request.MaxUnits)
	{
		UpgradeCheapestUnits(Settings, Pool, RemainingBudget, RandomStream, OutUnits);
	}

	int32 TotalSpent = 0;
	for (const FSurvivalPlannedWaveUnit& EachUnit : OutUnits)
	{
		TotalSpent += EachUnit.Cost;
	}
	return TotalSpent;
}

float FSurvivalWaveComposer::GetEntryWeight(
	const USurvivalWaveSettings& Settings,
	const FSurvivalWaveUnitEntry& Entry,
	const FSurvivalWaveCompositionRequest& Request)
{
	if (not Entry.GetIsValidEntry())
	{
		return 0.f;
	}
	const float SafePace = FMath::Max(Request.TechPaceMultiplier, KINDA_SMALL_NUMBER);
	const float UnlockMinute = Settings.GetTechStageUnlockMinute(Entry.TechLevel, SafePace);
	if (Request.SurvivalMinute < UnlockMinute)
	{
		return 0.f;
	}

	float Weight = Entry.Weight;
	if (Settings.NewTechStageRampInMinutes > 0.f)
	{
		const float RampAlpha = FMath::Clamp(
			(Request.SurvivalMinute - UnlockMinute) / Settings.NewTechStageRampInMinutes, 0.f, 1.f);
		Weight *= FMath::Lerp(Settings.NewTechStageStartWeightFraction, 1.f, RampAlpha);
	}
	if (Entry.FadeStartMinute > 0.f and Request.SurvivalMinute >= Entry.FadeStartMinute / SafePace)
	{
		Weight *= Entry.FadeWeightMultiplier;
	}
	if (Request.KindDefinition)
	{
		Weight *= Request.KindDefinition->GetRoleMultiplier(Entry.Role);
	}
	return Weight;
}

TArray<FSurvivalWaveComposer::FWeightedPoolEntry> FSurvivalWaveComposer::BuildWeightedPool(
	const USurvivalWaveSettings& Settings,
	const FSurvivalWaveCompositionRequest& Request)
{
	TArray<FWeightedPoolEntry> Pool;
	Pool.Reserve(Settings.UnitPool.Num());
	for (int32 PoolIndex = 0; PoolIndex < Settings.UnitPool.Num(); ++PoolIndex)
	{
		const FSurvivalWaveUnitEntry& Entry = Settings.UnitPool[PoolIndex];
		const float Weight = GetEntryWeight(Settings, Entry, Request);
		if (Weight <= 0.f)
		{
			continue;
		}
		FWeightedPoolEntry WeightedEntry;
		WeightedEntry.PoolIndex = PoolIndex;
		WeightedEntry.Cost = FMath::Max(1, Entry.GetCost());
		WeightedEntry.Weight = Weight;
		Pool.Add(WeightedEntry);
	}
	return Pool;
}

int32 FSurvivalWaveComposer::PickWeighted(
	const TArray<FWeightedPoolEntry>& Pool,
	const int32 MinCostExclusive,
	const int32 MaxCostInclusive,
	FRandomStream& RandomStream)
{
	float TotalWeight = 0.f;
	for (const FWeightedPoolEntry& EachEntry : Pool)
	{
		if (EachEntry.Cost > MinCostExclusive and EachEntry.Cost <= MaxCostInclusive)
		{
			TotalWeight += EachEntry.Weight;
		}
	}
	if (TotalWeight <= 0.f)
	{
		return INDEX_NONE;
	}

	float Roll = RandomStream.FRandRange(0.f, TotalWeight);
	int32 LastCandidate = INDEX_NONE;
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		const FWeightedPoolEntry& EachEntry = Pool[Index];
		if (EachEntry.Cost <= MinCostExclusive or EachEntry.Cost > MaxCostInclusive)
		{
			continue;
		}
		LastCandidate = Index;
		Roll -= EachEntry.Weight;
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return LastCandidate;
}

FSurvivalPlannedWaveUnit FSurvivalWaveComposer::MakePlannedUnit(
	const USurvivalWaveSettings& Settings,
	const FWeightedPoolEntry& PoolEntry)
{
	const FSurvivalWaveUnitEntry& Entry = Settings.UnitPool[PoolEntry.PoolIndex];
	FSurvivalPlannedWaveUnit PlannedUnit;
	PlannedUnit.Option = Entry.GetTrainingOption();
	PlannedUnit.Kind = Entry.Kind;
	PlannedUnit.Role = Entry.Role;
	PlannedUnit.Cost = PoolEntry.Cost;
	PlannedUnit.PoolIndex = PoolEntry.PoolIndex;
	return PlannedUnit;
}

int32 FSurvivalWaveComposer::FillBudget(
	const USurvivalWaveSettings& Settings,
	const TArray<FWeightedPoolEntry>& Pool,
	const FSurvivalWaveCompositionRequest& Request,
	FRandomStream& RandomStream,
	TArray<FSurvivalPlannedWaveUnit>& OutUnits)
{
	const int32 Budget = FMath::Max(0, FMath::RoundToInt(Request.Budget));
	const int32 MaxUnits = FMath::Max(1, Request.MaxUnits);
	int32 Spent = 0;
	while (OutUnits.Num() < MaxUnits)
	{
		const int32 PickedIndex = PickWeighted(Pool, 0, Budget - Spent, RandomStream);
		if (PickedIndex == INDEX_NONE)
		{
			break;
		}
		OutUnits.Add(MakePlannedUnit(Settings, Pool[PickedIndex]));
		Spent += Pool[PickedIndex].Cost;
	}

	if (OutUnits.IsEmpty())
	{
		// Budget below the cheapest unit: a wave always has at least one unit.
		const int32 CheapestIndex = PickWeighted(Pool, 0, TNumericLimits<int32>::Max(), RandomStream);
		if (Pool.IsValidIndex(CheapestIndex))
		{
			OutUnits.Add(MakePlannedUnit(Settings, Pool[CheapestIndex]));
			Spent += Pool[CheapestIndex].Cost;
		}
	}
	return Spent;
}

void FSurvivalWaveComposer::UpgradeCheapestUnits(
	const USurvivalWaveSettings& Settings,
	const TArray<FWeightedPoolEntry>& Pool,
	int32& RemainingBudget,
	FRandomStream& RandomStream,
	TArray<FSurvivalPlannedWaveUnit>& InOutUnits)
{
	for (int32 Replacement = 0; Replacement < Settings.UpgradePassMaxReplacements; ++Replacement)
	{
		if (RemainingBudget <= 0 or InOutUnits.IsEmpty())
		{
			return;
		}
		int32 CheapestUnitIndex = 0;
		for (int32 UnitIndex = 1; UnitIndex < InOutUnits.Num(); ++UnitIndex)
		{
			if (InOutUnits[UnitIndex].Cost < InOutUnits[CheapestUnitIndex].Cost)
			{
				CheapestUnitIndex = UnitIndex;
			}
		}
		const int32 OldCost = InOutUnits[CheapestUnitIndex].Cost;
		const int32 PickedIndex = PickWeighted(Pool, OldCost, OldCost + RemainingBudget, RandomStream);
		if (PickedIndex == INDEX_NONE)
		{
			return;
		}
		RemainingBudget -= Pool[PickedIndex].Cost - OldCost;
		InOutUnits[CheapestUnitIndex] = MakePlannedUnit(Settings, Pool[PickedIndex]);
	}
}
