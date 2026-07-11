// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldData/WorldDataComponent.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationHelpers/WorldCampaignGenerationHelper.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthTypes.h"
#include "RTS_Survival/WorldCampaign/WorldData/WorldData.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapUI/MapObjects/W_EnemyMapItem/StrengthEstimation/DataAndUtils/RTSStrengthEstimationTypes.h"

namespace
{
	constexpr int32 EnemyPrimaryRewardSeedOffset = 24103;
	constexpr int32 EnemyTypeSeedMultiplier = 97;
	constexpr int32 EnemyBonusObjectiveSeedOffset = 24107;
	const TCHAR BaseDifficultyReasonText[] = TEXT("Base Fortification Strength");

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

	FString GetEnumValueName(const UEnum* Enum, const int64 EnumValue, const TCHAR* FallbackName)
	{
		if (not IsValid(Enum))
		{
			return FString(FallbackName);
		}

		return Enum->GetNameStringByValue(EnumValue);
	}

	FWorldStrengthReason BuildBaseDifficultyReason(const int32 DifficultyPercentage)
	{
		return FWorldStrengthReason(
			FText::FromString(FString(BaseDifficultyReasonText)),
			DifficultyPercentage);
	}

	FText GetStrengthEnumReasonText(const UEnum* Enum, const int64 EnumValue, const TCHAR* FallbackName)
	{
		if (not IsValid(Enum))
		{
			return FText::FromString(FString(FallbackName));
		}

		return Enum->GetDisplayNameTextByValue(EnumValue);
	}

	FWorldStrengthReason BuildStrengthDefinitionReason(
		const FWorldDataStrengthReasonDefinition& Definition,
		const UEnum* Enum,
		const int64 EnumValue,
		const TCHAR* FallbackName)
	{
		/*
		 * WorldData may provide only a percentage while designers are still filling content. Falling back to the enum
		 * display name keeps the strength panel readable and avoids silent empty reason lines.
		 */
		const FText ReasonText = Definition.ReasonText.IsEmpty()
			                         ? GetStrengthEnumReasonText(Enum, EnumValue, FallbackName)
			                         : Definition.ReasonText;
		return FWorldStrengthReason(ReasonText, Definition.StrengthPercentage);
	}

	void ReportMissingMissionBaseDifficulty(const EMapMission MissionType)
	{
		const FString MissionName = GetEnumValueName(
			StaticEnum<EMapMission>(),
			static_cast<int64>(MissionType),
			TEXT("UnknownMission"));
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("WorldData missing base difficulty percentage for mission type %s."),
			                *MissionName));
	}

	void ReportMissingEnemyBaseDifficulty(const EMapEnemyItem EnemyItemType)
	{
		const FString EnemyName = GetEnumValueName(
			StaticEnum<EMapEnemyItem>(),
			static_cast<int64>(EnemyItemType),
			TEXT("UnknownEnemyItem"));
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("WorldData missing base difficulty percentage for enemy item type %s."),
			                *EnemyName));
	}

	void ReportMissingFortificationStrengthDefinition(const EWorldFortificationStrength FortificationStrength)
	{
		const FString FortificationName = GetEnumValueName(
			StaticEnum<EWorldFortificationStrength>(),
			static_cast<int64>(FortificationStrength),
			TEXT("UnknownFortificationStrength"));
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("WorldData missing strength definition for fortification modifier %s."),
			                *FortificationName));
	}

	void ReportMissingStrategicSupportDefinition(const EWorldStrategicSupport StrategicSupport)
	{
		const FString StrategicSupportName = GetEnumValueName(
			StaticEnum<EWorldStrategicSupport>(),
			static_cast<int64>(StrategicSupport),
			TEXT("UnknownStrategicSupport"));
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("WorldData missing strength definition for strategic support %s."),
			                *StrategicSupportName));
	}

	void ReportMissingFieldDivisionDefinition(const EWorldFieldDivisions FieldDivision)
	{
		const FString FieldDivisionName = GetEnumValueName(
			StaticEnum<EWorldFieldDivisions>(),
			static_cast<int64>(FieldDivision),
			TEXT("UnknownFieldDivision"));
		RTSFunctionLibrary::ReportError(
			FString::Printf(TEXT("WorldData missing strength definition for field division %s."),
			                *FieldDivisionName));
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

bool UWorldDataComponent::TryBuildMissionBaseDifficultyReason(
	const EMapMission MissionType,
	const ERTSGameDifficulty GameDifficulty,
	FWorldStrengthReason& OutStrengthReason) const
{
	OutStrengthReason = BuildBaseDifficultyReason(0);
	if (not GetIsValidWorldData())
	{
		return false;
	}

	int32 DifficultyPercentage = 0;
	const bool bFoundDifficultyPercentage = M_WorldData->TryGetMissionBaseDifficultyPercentage(
		MissionType,
		GameDifficulty,
		DifficultyPercentage);
	OutStrengthReason = BuildBaseDifficultyReason(DifficultyPercentage);
	if (bFoundDifficultyPercentage)
	{
		return true;
	}

	ReportMissingMissionBaseDifficulty(MissionType);
	return false;
}

bool UWorldDataComponent::TryBuildEnemyBaseDifficultyReason(
	const EMapEnemyItem EnemyItemType,
	const ERTSGameDifficulty GameDifficulty,
	FWorldStrengthReason& OutStrengthReason) const
{
	OutStrengthReason = BuildBaseDifficultyReason(0);
	if (not GetIsValidWorldData())
	{
		return false;
	}

	int32 DifficultyPercentage = 0;
	const bool bFoundDifficultyPercentage = M_WorldData->TryGetEnemyBaseDifficultyPercentage(
		EnemyItemType,
		GameDifficulty,
		DifficultyPercentage);
	OutStrengthReason = BuildBaseDifficultyReason(DifficultyPercentage);
	if (bFoundDifficultyPercentage)
	{
		return true;
	}

	ReportMissingEnemyBaseDifficulty(EnemyItemType);
	return false;
}

bool UWorldDataComponent::TryBuildFortificationStrengthReason(
	const EWorldFortificationStrength FortificationStrength,
	const ERTSGameDifficulty GameDifficulty,
	FWorldStrengthReason& OutStrengthReason) const
{
	OutStrengthReason = FWorldStrengthReason();
	if (FortificationStrength == EWorldFortificationStrength::None || not GetIsValidWorldData())
	{
		return false;
	}

	FWorldDataStrengthReasonDefinition Definition;
	const bool bFoundDefinition = M_WorldData->TryGetFortificationStrengthDefinition(
		FortificationStrength,
		GameDifficulty,
		Definition);
	OutStrengthReason = BuildStrengthDefinitionReason(
		Definition,
		StaticEnum<EWorldFortificationStrength>(),
		static_cast<int64>(FortificationStrength),
		TEXT("Fortification"));
	if (bFoundDefinition)
	{
		return true;
	}

	ReportMissingFortificationStrengthDefinition(FortificationStrength);
	return false;
}

bool UWorldDataComponent::TryBuildStrategicSupportReason(
	const EWorldStrategicSupport StrategicSupport,
	const ERTSGameDifficulty GameDifficulty,
	FWorldStrengthReason& OutStrengthReason) const
{
	OutStrengthReason = FWorldStrengthReason();
	if (StrategicSupport == EWorldStrategicSupport::None || not GetIsValidWorldData())
	{
		return false;
	}

	FWorldDataStrengthReasonDefinition Definition;
	const bool bFoundDefinition = M_WorldData->TryGetStrategicSupportDefinition(
		StrategicSupport,
		GameDifficulty,
		Definition);
	OutStrengthReason = BuildStrengthDefinitionReason(
		Definition,
		StaticEnum<EWorldStrategicSupport>(),
		static_cast<int64>(StrategicSupport),
		TEXT("Strategic Support"));
	if (bFoundDefinition)
	{
		return true;
	}

	ReportMissingStrategicSupportDefinition(StrategicSupport);
	return false;
}

bool UWorldDataComponent::TryBuildFieldDivisionReason(
	const EWorldFieldDivisions FieldDivision,
	const ERTSGameDifficulty GameDifficulty,
	FWorldStrengthReason& OutStrengthReason) const
{
	OutStrengthReason = FWorldStrengthReason();
	if (FieldDivision == EWorldFieldDivisions::None || not GetIsValidWorldData())
	{
		return false;
	}

	FWorldDataStrengthReasonDefinition Definition;
	const bool bFoundDefinition = M_WorldData->TryGetFieldDivisionDefinition(
		FieldDivision,
		GameDifficulty,
		Definition);
	OutStrengthReason = BuildStrengthDefinitionReason(
		Definition,
		StaticEnum<EWorldFieldDivisions>(),
		static_cast<int64>(FieldDivision),
		TEXT("Field Division"));
	if (bFoundDefinition)
	{
		return true;
	}

	ReportMissingFieldDivisionDefinition(FieldDivision);
	return false;
}

bool UWorldDataComponent::GetIsValidWorldData() const
{
	if (not IsValid(M_WorldData))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
			this,
			TEXT("M_WorldData"),
			TEXT("GetIsValidWorldData"),
			this
		);
		return false;
	}

	return true;
}
