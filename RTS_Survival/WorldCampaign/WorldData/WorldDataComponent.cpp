// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldData/WorldDataComponent.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationHelpers/WorldCampaignGenerationHelper.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/WorldData/WorldData.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"

namespace
{
	constexpr int32 EnemyPrimaryRewardSeedOffset = 24103;
	constexpr int32 EnemyTypeSeedMultiplier = 97;
	constexpr int32 EnemyBonusObjectiveSeedOffset = 24107;

	struct FEnemyPrimaryRewardRuntimePool
	{
		TArray<FPrimaryReward> PrimaryRewards;
		int32 NextRewardIndex = 0;
		bool bWasInitialized = false;
	};

	bool GetIsAnchorKeyLess(const FGuid& LeftAnchorKey, const FGuid& RightAnchorKey)
	{
		return AAnchorPoint::IsAnchorKeyLess(LeftAnchorKey, RightAnchorKey);
	}

	bool GetIsEnemyObjectLess(const TObjectPtr<AWorldEnemyObject>& LeftObject,
	                          const TObjectPtr<AWorldEnemyObject>& RightObject)
	{
		if (not IsValid(LeftObject))
		{
			return false;
		}

		if (not IsValid(RightObject))
		{
			return true;
		}

		if (LeftObject->GetEnemyItemType() != RightObject->GetEnemyItemType())
		{
			return static_cast<uint8>(LeftObject->GetEnemyItemType())
				< static_cast<uint8>(RightObject->GetEnemyItemType());
		}

		return GetIsAnchorKeyLess(LeftObject->GetAnchorKey(), RightObject->GetAnchorKey());
	}

	TArray<TObjectPtr<AWorldEnemyObject>> BuildSortedEnemyObjects(
		const AGeneratorWorldCampaign& WorldGenerator)
	{
		const FWorldCampaignPlacementState& PlacementState = WorldGenerator.GetPlacementState();
		TArray<TObjectPtr<AWorldEnemyObject>> EnemyObjects;
		EnemyObjects.Reserve(PlacementState.EnemyItemsByAnchorKey.Num());

		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
			if (not PlacementState.EnemyItemsByAnchorKey.Contains(AnchorKey))
			{
				continue;
			}

			AWorldEnemyObject* EnemyObject = Cast<AWorldEnemyObject>(AnchorPoint->GetPromotedWorldObject());
			if (not IsValid(EnemyObject))
			{
				continue;
			}

			EnemyObjects.Add(EnemyObject);
		}

		EnemyObjects.Sort(GetIsEnemyObjectLess);
		return EnemyObjects;
	}

	int32 BuildEnemyPrimaryRewardSeed(const int32 WorldSeed, const EMapEnemyItem EnemyItemType)
	{
		return WorldSeed + EnemyPrimaryRewardSeedOffset
			+ static_cast<int32>(EnemyItemType) * EnemyTypeSeedMultiplier;
	}

	void InitializeEnemyPrimaryRewardRuntimePool(const UWorldData& WorldData,
	                                             const int32 WorldSeed,
	                                             const EMapEnemyItem EnemyItemType,
	                                             FEnemyPrimaryRewardRuntimePool& InOutRuntimePool)
	{
		InOutRuntimePool.bWasInitialized = true;
		const FWorldDataEnemyPrimaryRewardPool* RewardPool = WorldData.FindEnemyPrimaryRewardPool(EnemyItemType);
		if (RewardPool == nullptr || RewardPool->PrimaryRewards.Num() == 0)
		{
			RTSFunctionLibrary::ReportError(TEXT("WorldData missing enemy primary rewards for generated enemy type."));
			return;
		}

		InOutRuntimePool.PrimaryRewards = RewardPool->PrimaryRewards;
		FRandomStream RandomStream(BuildEnemyPrimaryRewardSeed(WorldSeed, EnemyItemType));
		CampaignGenerationHelper::DeterministicShuffle(InOutRuntimePool.PrimaryRewards, RandomStream);
	}

	void ApplyEnemyPrimaryRewardToObject(const UWorldData& WorldData,
	                                     const int32 WorldSeed,
	                                     AWorldEnemyObject* EnemyObject,
	                                     TMap<EMapEnemyItem, FEnemyPrimaryRewardRuntimePool>& InOutRuntimePools)
	{
		if (not IsValid(EnemyObject))
		{
			return;
		}

		const EMapEnemyItem EnemyItemType = EnemyObject->GetEnemyItemType();
		FEnemyPrimaryRewardRuntimePool& RuntimePool = InOutRuntimePools.FindOrAdd(EnemyItemType);
		if (not RuntimePool.bWasInitialized)
		{
			InitializeEnemyPrimaryRewardRuntimePool(WorldData, WorldSeed, EnemyItemType, RuntimePool);
		}

		if (RuntimePool.PrimaryRewards.Num() == 0)
		{
			return;
		}

		const int32 RewardIndex = RuntimePool.NextRewardIndex % RuntimePool.PrimaryRewards.Num();
		EnemyObject->SetPrimaryReward(RuntimePool.PrimaryRewards[RewardIndex]);
		RuntimePool.NextRewardIndex++;
	}

	void BuildShuffledBonusObjectiveDefinitions(const UWorldData& WorldData,
	                                            const int32 WorldSeed,
	                                            TArray<FWorldDataBonusObjectiveDefinition>& OutDefinitions)
	{
		for (const FWorldDataBonusObjectiveDefinition& BonusObjectiveDefinition :
		     WorldData.GetBonusObjectiveDefinitions())
		{
			if (BonusObjectiveDefinition.BonusObjective == EBonusObjective::None)
			{
				continue;
			}

			OutDefinitions.Add(BonusObjectiveDefinition);
		}

		if (OutDefinitions.Num() == 0)
		{
			RTSFunctionLibrary::ReportError(TEXT("WorldData has no non-None bonus objective definitions."));
			return;
		}

		FRandomStream RandomStream(WorldSeed + EnemyBonusObjectiveSeedOffset);
		CampaignGenerationHelper::DeterministicShuffle(OutDefinitions, RandomStream);
	}

	void ApplyEnemyRewardDataIntoObjects(const UWorldData& WorldData,
	                                     const AGeneratorWorldCampaign& WorldGenerator)
	{
		TArray<TObjectPtr<AWorldEnemyObject>> EnemyObjects = BuildSortedEnemyObjects(WorldGenerator);
		TMap<EMapEnemyItem, FEnemyPrimaryRewardRuntimePool> PrimaryRewardRuntimePools;
		TArray<FWorldDataBonusObjectiveDefinition> BonusObjectiveDefinitions;
		const int32 WorldSeed = WorldGenerator.GetPlacementState().SeedUsed;
		BuildShuffledBonusObjectiveDefinitions(WorldData, WorldSeed, BonusObjectiveDefinitions);

		for (int32 EnemyObjectIndex = 0; EnemyObjectIndex < EnemyObjects.Num(); EnemyObjectIndex++)
		{
			AWorldEnemyObject* EnemyObject = EnemyObjects[EnemyObjectIndex].Get();
			ApplyEnemyPrimaryRewardToObject(WorldData, WorldSeed, EnemyObject, PrimaryRewardRuntimePools);
			if (not IsValid(EnemyObject) || BonusObjectiveDefinitions.Num() == 0)
			{
				continue;
			}

			const int32 BonusObjectiveIndex = EnemyObjectIndex % BonusObjectiveDefinitions.Num();
			const FWorldDataBonusObjectiveDefinition& BonusObjectiveDefinition =
				BonusObjectiveDefinitions[BonusObjectiveIndex];
			EnemyObject->SetSecondaryObjectiveData(
				BonusObjectiveDefinition.BonusObjective,
				BonusObjectiveDefinition.SecondaryObjectiveText,
				BonusObjectiveDefinition.SecondaryReward
			);
		}
	}
}

UWorldDataComponent::UWorldDataComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWorldDataComponent::LoadWorldDataIntoObjects(AGeneratorWorldCampaign* WorldGenerator) const
{
	if (not IsValid(WorldGenerator) || not GetIsValidWorldData())
	{
		return;
	}

	ApplyEnemyRewardDataIntoObjects(*M_WorldData, *WorldGenerator);
}

bool UWorldDataComponent::GetIsValidWorldData() const
{
	if (IsValid(M_WorldData))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldData"),
		TEXT("GetIsValidWorldData"),
		this
	);
	return false;
}
