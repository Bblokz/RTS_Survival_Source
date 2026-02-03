// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"

#include "Algo/Sort.h"
#include "DrawDebugHelpers.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "EngineUtils.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Debugging/WorldCampaignDebugger.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationHelpers/WorldCampaignGenerationHelper.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"

namespace
{
	constexpr int32 MaxStepAttempts = 25;
	constexpr int32 MaxTotalAttempts = 90000;
	constexpr int32 AttemptSeedMultiplier = 13;
	constexpr int32 MaxRelaxationAttempts = 3;
	constexpr int32 RelaxedHopDistanceMax = TNumericLimits<int32>::Max();
	constexpr float RelaxedDistanceMax = TNumericLimits<float>::Max();
	constexpr int32 MaxChokepointPairSamples = 48;
	constexpr int32 ChokepointSeedOffset = 7919;

	struct FAnchorCandidate
	{
		TObjectPtr<AAnchorPoint> AnchorPoint = nullptr;
		float DistanceSquared = 0.f;
		FGuid AnchorKey;
	};

	struct FPlacementCandidate
	{
		TObjectPtr<AAnchorPoint> AnchorPoint = nullptr;
		FGuid AnchorKey;
		float Score = 0.f;
		int32 HopDistanceFromHQ = INDEX_NONE;
		TWeakObjectPtr<AAnchorPoint> CompanionAnchor;
		uint8 CompanionRawSubtype = 0;
		EMapItemType CompanionItemType = EMapItemType::None;
	};

	struct FRuleRelaxationState
	{
		bool bRelaxDistance = false;
		bool bRelaxSpacing = false;
		bool bRelaxPreference = false;
	};

	struct FClosestConnectionCandidate
	{
		TWeakObjectPtr<AConnection> Connection;
		FVector JunctionLocation = FVector::ZeroVector;
		float DistanceSquared = TNumericLimits<float>::Max();
	};

	constexpr float SegmentIntersectionTolerance = 0.01f;
	constexpr int32 NoRequiredItems = 0;

	TArray<EMapEnemyItem> GetSortedEnemyTypes(const TMap<EMapEnemyItem, int32>& EnemyItemCounts)
	{
		TArray<EMapEnemyItem> EnemyTypes;
		EnemyItemCounts.GetKeys(EnemyTypes);
		EnemyTypes.Sort([](const EMapEnemyItem Left, const EMapEnemyItem Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});
		return EnemyTypes;
	}

	TArray<EMapMission> GetSortedMissionTypes(const TMap<EMapMission, FPerMissionRules>& MissionRules)
	{
		TArray<EMapMission> MissionTypes;
		MissionRules.GetKeys(MissionTypes);
		MissionTypes.Sort([](const EMapMission Left, const EMapMission Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});
		return MissionTypes;
	}


	TMap<FGuid, TObjectPtr<AAnchorPoint>> BuildAnchorLookup(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors)
	{
		TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup;
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			AnchorLookup.Add(AnchorPoint->GetAnchorKey(), AnchorPoint);
		}

		return AnchorLookup;
	}

	TArray<EMapEnemyItem> BuildEnemyPlacementPlan(const TMap<EMapEnemyItem, int32>& RequiredEnemyCounts)
	{
		TArray<EMapEnemyItem> PlacementPlan;
		const TArray<EMapEnemyItem> EnemyTypes = GetSortedEnemyTypes(RequiredEnemyCounts);
		for (const EMapEnemyItem EnemyType : EnemyTypes)
		{
			if (EnemyType == EMapEnemyItem::EnemyHQ || EnemyType == EMapEnemyItem::EnemyWall)
			{
				continue;
			}

			const int32 RequiredCount = RequiredEnemyCounts.FindRef(EnemyType);
			for (int32 PlacementIndex = 0; PlacementIndex < RequiredCount; PlacementIndex++)
			{
				PlacementPlan.Add(EnemyType);
			}
		}

		return PlacementPlan;
	}

	void AppendAnchorKeyIfValid(const FGuid& AnchorKey, TSet<FGuid>& OutAnchorKeys)
	{
		if (AnchorKey.IsValid())
		{
			OutAnchorKeys.Add(AnchorKey);
		}
	}

	void AppendAnchorKeysFromTransaction(const FCampaignGenerationStepTransaction& Transaction,
	                                     TSet<FGuid>& OutAnchorKeys)
	{
		AppendAnchorKeyIfValid(Transaction.MicroAnchorKey, OutAnchorKeys);
		for (const FGuid& AnchorKey : Transaction.MicroAdditionalAnchorKeys)
		{
			AppendAnchorKeyIfValid(AnchorKey, OutAnchorKeys);
		}

		for (const TObjectPtr<AActor>& SpawnedActor : Transaction.SpawnedActors)
		{
			if (not IsValid(SpawnedActor))
			{
				continue;
			}

			const AWorldMapObject* MapObject = Cast<AWorldMapObject>(SpawnedActor);
			if (not IsValid(MapObject))
			{
				continue;
			}

			AppendAnchorKeyIfValid(MapObject->GetAnchorKey(), OutAnchorKeys);
		}
	}

	AAnchorPoint* FindAnchorByKey(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup, const FGuid& AnchorKey)
	{
		const TObjectPtr<AAnchorPoint>* AnchorPtr = AnchorLookup.Find(AnchorKey);
		return AnchorPtr ? AnchorPtr->Get() : nullptr;
	}

	void RemovePromotedWorldObjectsForAnchorKeys(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors,
	                                             const TSet<FGuid>& AnchorKeys)
	{
		if (AnchorKeys.Num() == 0)
		{
			return;
		}

		const TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(CachedAnchors);
		for (const FGuid& AnchorKey : AnchorKeys)
		{
			AAnchorPoint* AnchorPoint = FindAnchorByKey(AnchorLookup, AnchorKey);
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			AnchorPoint->RemovePromotedWorldObject();
		}
	}

	bool IsAnchorOccupied(const FGuid& AnchorKey, const FWorldCampaignPlacementState& PlacementState)
	{
		if (PlacementState.PlayerHQAnchorKey == AnchorKey || PlacementState.EnemyHQAnchorKey == AnchorKey)
		{
			return true;
		}

		if (PlacementState.EnemyItemsByAnchorKey.Contains(AnchorKey))
		{
			return true;
		}

		if (PlacementState.NeutralItemsByAnchorKey.Contains(AnchorKey))
		{
			return true;
		}

		if (PlacementState.MissionsByAnchorKey.Contains(AnchorKey))
		{
			return true;
		}

		return false;
	}

	bool IsAnchorOccupiedForMission(const FGuid& AnchorKey, const FWorldCampaignPlacementState& PlacementState,
	                                const bool bAllowNeutralStacking)
	{
		if (PlacementState.PlayerHQAnchorKey == AnchorKey || PlacementState.EnemyHQAnchorKey == AnchorKey)
		{
			return true;
		}

		if (PlacementState.EnemyItemsByAnchorKey.Contains(AnchorKey))
		{
			return true;
		}

		if (PlacementState.MissionsByAnchorKey.Contains(AnchorKey))
		{
			return true;
		}

		if (not bAllowNeutralStacking && PlacementState.NeutralItemsByAnchorKey.Contains(AnchorKey))
		{
			return true;
		}

		return false;
	}

	int32 GetCachedHopDistance(const TMap<FGuid, int32>& HopDistancesByAnchorKey, const FGuid& AnchorKey)
	{
		const int32* CachedDistance = HopDistancesByAnchorKey.Find(AnchorKey);
		return CachedDistance ? *CachedDistance : INDEX_NONE;
	}

	bool BuildShortestPathKeys(const AAnchorPoint* StartAnchor,
	                           const AAnchorPoint* TargetAnchor,
	                           const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                           TArray<FGuid>& OutPathKeys)
	{
		if (not IsValid(StartAnchor) || not IsValid(TargetAnchor))
		{
			return false;
		}

		const FGuid StartKey = StartAnchor->GetAnchorKey();
		const FGuid TargetKey = TargetAnchor->GetAnchorKey();
		if (StartKey == TargetKey)
		{
			OutPathKeys.Reset();
			OutPathKeys.Add(StartKey);
			return true;
		}

		TArray<FGuid> Queue;
		Queue.Add(StartKey);

		TSet<FGuid> Visited;
		Visited.Add(StartKey);

		TMap<FGuid, FGuid> PredecessorByKey;

		int32 QueueIndex = 0;
		while (QueueIndex < Queue.Num())
		{
			const FGuid CurrentKey = Queue[QueueIndex];
			QueueIndex++;

			if (CurrentKey == TargetKey)
			{
				break;
			}

			const TObjectPtr<AAnchorPoint>* CurrentAnchorPtr = AnchorLookup.Find(CurrentKey);
			const AAnchorPoint* CurrentAnchor = CurrentAnchorPtr ? CurrentAnchorPtr->Get() : nullptr;
			if (not IsValid(CurrentAnchor))
			{
				continue;
			}

			for (const TObjectPtr<AAnchorPoint>& Neighbor : CurrentAnchor->GetNeighborAnchors())
			{
				if (not IsValid(Neighbor))
				{
					continue;
				}

				const FGuid NeighborKey = Neighbor->GetAnchorKey();
				if (Visited.Contains(NeighborKey))
				{
					continue;
				}

				Visited.Add(NeighborKey);
				PredecessorByKey.Add(NeighborKey, CurrentKey);
				Queue.Add(NeighborKey);
			}
		}

		if (not Visited.Contains(TargetKey))
		{
			return false;
		}

		TArray<FGuid> ReversePath;
		ReversePath.Add(TargetKey);

		FGuid CurrentKey = TargetKey;
		while (CurrentKey != StartKey)
		{
			const FGuid* PreviousKey = PredecessorByKey.Find(CurrentKey);
			if (not PreviousKey)
			{
				return false;
			}

			CurrentKey = *PreviousKey;
			ReversePath.Add(CurrentKey);
		}

		OutPathKeys.Reset();
		for (int32 Index = ReversePath.Num() - 1; Index >= 0; --Index)
		{
			OutPathKeys.Add(ReversePath[Index]);
		}

		return true;
	}

	void AddChokepointPathContribution(const TArray<FGuid>& PathKeys, TMap<FGuid, float>& InOutScores)
	{
		constexpr int32 MinPathNodes = 3;
		if (PathKeys.Num() < MinPathNodes)
		{
			return;
		}

		for (int32 Index = 1; Index < PathKeys.Num() - 1; Index++)
		{
			const float ExistingScore = InOutScores.FindRef(PathKeys[Index]);
			InOutScores.Add(PathKeys[Index], ExistingScore + 1.f);
		}
	}


	TArray<EMapNeutralObjectType> GetSortedNeutralTypes(const TMap<EMapNeutralObjectType, int32>& NeutralItemCounts)
	{
		TArray<EMapNeutralObjectType> NeutralTypes;
		NeutralItemCounts.GetKeys(NeutralTypes);
		NeutralTypes.Sort([](const EMapNeutralObjectType Left, const EMapNeutralObjectType Right)
		{
			return static_cast<uint8>(Left) < static_cast<uint8>(Right);
		});
		return NeutralTypes;
	}


	float GetEnemyPreferenceScore(EEnemyTopologySearchStrategy Preference, int32 ConnectionDegree,
	                              float ChokepointScore, int32 HopDistanceFromEnemyHQ,
	                              int32 MinHopsFromEnemyHQ, int32 MaxHopsFromEnemyHQ)
	{
		switch (Preference)
		{
		case EEnemyTopologySearchStrategy::PreferLowDegree:
			return -static_cast<float>(ConnectionDegree);
		case EEnemyTopologySearchStrategy::PreferHighDegree:
			return static_cast<float>(ConnectionDegree);
		case EEnemyTopologySearchStrategy::PreferChokepoints:
			return ChokepointScore;
		case EEnemyTopologySearchStrategy::PreferDeadEnds:
			return ConnectionDegree == 1 ? 1.f : 0.f;
		case EEnemyTopologySearchStrategy::PreferNearMinBound:
			return -FMath::Abs(static_cast<float>(HopDistanceFromEnemyHQ - MinHopsFromEnemyHQ));
		case EEnemyTopologySearchStrategy::PreferNearMaxBound:
			return -FMath::Abs(static_cast<float>(HopDistanceFromEnemyHQ - MaxHopsFromEnemyHQ));
		case EEnemyTopologySearchStrategy::None:
		default:
			return 0.f;
		}
	}

	float GetEnemyWallPreferenceScore(EEnemyTopologySearchStrategy Preference, int32 ConnectionDegree,
	                                  float ChokepointScore)
	{
		switch (Preference)
		{
		case EEnemyTopologySearchStrategy::PreferLowDegree:
			return -static_cast<float>(ConnectionDegree);
		case EEnemyTopologySearchStrategy::PreferHighDegree:
			return static_cast<float>(ConnectionDegree);
		case EEnemyTopologySearchStrategy::PreferDeadEnds:
			return ConnectionDegree == 1 ? 1.f : 0.f;
		case EEnemyTopologySearchStrategy::PreferChokepoints:
			return ChokepointScore;
		case EEnemyTopologySearchStrategy::PreferNearMinBound:
		case EEnemyTopologySearchStrategy::PreferNearMaxBound:
		case EEnemyTopologySearchStrategy::None:
		default:
			return 0.f;
		}
	}

	float GetTopologyPreferenceScore(ETopologySearchStrategy Preference, float Value)
	{
		if (Preference == ETopologySearchStrategy::PreferMax)
		{
			return Value;
		}

		if (Preference == ETopologySearchStrategy::PreferMin)
		{
			return -Value;
		}

		return 0.f;
	}

	bool HasNeutralTypeAtAnchor(const FWorldCampaignPlacementState& PlacementState, const FGuid& AnchorKey,
	                            EMapNeutralObjectType RequiredNeutralType)
	{
		const EMapNeutralObjectType* NeutralType = PlacementState.NeutralItemsByAnchorKey.Find(AnchorKey);
		return NeutralType && *NeutralType == RequiredNeutralType;
	}

	bool HasMinimumAdjacentMatches(const TArray<int32>& HopDistances, int32 RequiredCount)
	{
		int32 MatchingCount = 0;
		for (const int32 HopDistance : HopDistances)
		{
			if (HopDistance != INDEX_NONE)
			{
				MatchingCount++;
			}

			if (MatchingCount >= RequiredCount)
			{
				return true;
			}
		}

		return false;
	}

	FRuleRelaxationState GetRelaxationState(EPlacementFailurePolicy Policy, int32 AttemptIndex)
	{
		FRuleRelaxationState RelaxationState;
		if (Policy == EPlacementFailurePolicy::InstantBackTrack)
		{
			return RelaxationState;
		}

		if (Policy != EPlacementFailurePolicy::BreakDistanceRules_ThenBackTrack)
		{
			return RelaxationState;
		}

		if (AttemptIndex == 1)
		{
			RelaxationState.bRelaxDistance = true;
		}
		else if (AttemptIndex == 2)
		{
			RelaxationState.bRelaxSpacing = true;
		}
		else if (AttemptIndex >= 3)
		{
			RelaxationState.bRelaxDistance = true;
			RelaxationState.bRelaxSpacing = true;
		}

		RelaxationState.bRelaxPreference = RelaxationState.bRelaxDistance || RelaxationState.bRelaxSpacing;
		return RelaxationState;
	}

	bool PassesEnemySpacingRules(const FEnemyItemPlacementRules& EffectiveRules,
	                             const AAnchorPoint* CandidateAnchor,
	                             EMapEnemyItem EnemyType,
	                             const FWorldCampaignPlacementState& WorkingPlacementState,
	                             const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
	{
		TArray<FGuid> SortedEnemyAnchorKeys;
		SortedEnemyAnchorKeys.Reserve(WorkingPlacementState.EnemyItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapEnemyItem>& ExistingPair : WorkingPlacementState.EnemyItemsByAnchorKey)
		{
			SortedEnemyAnchorKeys.Add(ExistingPair.Key);
		}

		Algo::Sort(SortedEnemyAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& ExistingAnchorKey : SortedEnemyAnchorKeys)
		{
			const EMapEnemyItem ExistingEnemyType = WorkingPlacementState.EnemyItemsByAnchorKey.FindRef(
				ExistingAnchorKey);
			AAnchorPoint* ExistingAnchor = FindAnchorByKey(AnchorLookup, ExistingAnchorKey);
			if (not IsValid(ExistingAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, ExistingAnchor);
			if (HopDistance == INDEX_NONE)
			{
				return false;
			}

			const int32 RequiredMinHops = (ExistingEnemyType == EnemyType)
				                              ? EffectiveRules.ItemSpacing.MinEnemySeparationHopsSameType
				                              : EffectiveRules.ItemSpacing.MinEnemySeparationHopsOtherType;

			if (HopDistance < RequiredMinHops)
			{
				return false;
			}
		}

		return true;
	}

	bool PassesNeutralDistanceRules(const FNeutralItemPlacementRules& PlacementRules,
	                                const FGuid& CandidateKey,
	                                const FWorldCampaignDerivedData& WorkingDerivedData)
	{
		const int32 HopDistanceFromHQ = GetCachedHopDistance(
			WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
			CandidateKey);
		if (HopDistanceFromHQ == INDEX_NONE)
		{
			return false;
		}

		const int32 MaxHopsFromHQClamped = FMath::Max(PlacementRules.MinHopsFromHQ, PlacementRules.MaxHopsFromHQ);
		return HopDistanceFromHQ >= PlacementRules.MinHopsFromHQ && HopDistanceFromHQ <= MaxHopsFromHQClamped;
	}

	bool PassesNeutralSpacingRules(const FNeutralItemPlacementRules& PlacementRules,
	                               const AAnchorPoint* CandidateAnchor,
	                               const FWorldCampaignPlacementState& WorkingPlacementState,
	                               const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
	{
		const int32 MaxHopsFromNeutralClamped = FMath::Max(PlacementRules.MinHopsFromOtherNeutralItems,
		                                                   PlacementRules.MaxHopsFromOtherNeutralItems);
		TArray<FGuid> SortedNeutralAnchorKeys;
		SortedNeutralAnchorKeys.Reserve(WorkingPlacementState.NeutralItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapNeutralObjectType>& ExistingPair : WorkingPlacementState.NeutralItemsByAnchorKey)
		{
			SortedNeutralAnchorKeys.Add(ExistingPair.Key);
		}

		Algo::Sort(SortedNeutralAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& ExistingAnchorKey : SortedNeutralAnchorKeys)
		{
			AAnchorPoint* ExistingAnchor = FindAnchorByKey(AnchorLookup, ExistingAnchorKey);
			if (not IsValid(ExistingAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, ExistingAnchor);
			if (HopDistance == INDEX_NONE)
			{
				return false;
			}

			if (HopDistance < PlacementRules.MinHopsFromOtherNeutralItems
				|| HopDistance > MaxHopsFromNeutralClamped)
			{
				return false;
			}
		}

		return true;
	}

	int32 GetMinHopDistanceToSameEnemyType(const AAnchorPoint* CandidateAnchor,
	                                       EMapEnemyItem EnemyType,
	                                       const FWorldCampaignPlacementState& WorkingPlacementState,
	                                       const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
	{
		int32 MinHopDistance = INDEX_NONE;
		TArray<FGuid> SortedEnemyAnchorKeys;
		SortedEnemyAnchorKeys.Reserve(WorkingPlacementState.EnemyItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapEnemyItem>& ExistingPair : WorkingPlacementState.EnemyItemsByAnchorKey)
		{
			SortedEnemyAnchorKeys.Add(ExistingPair.Key);
		}

		Algo::Sort(SortedEnemyAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& ExistingAnchorKey : SortedEnemyAnchorKeys)
		{
			const EMapEnemyItem ExistingEnemyType = WorkingPlacementState.EnemyItemsByAnchorKey.FindRef(
				ExistingAnchorKey);
			if (ExistingEnemyType != EnemyType)
			{
				continue;
			}

			AAnchorPoint* ExistingAnchor = FindAnchorByKey(AnchorLookup, ExistingAnchorKey);
			if (not IsValid(ExistingAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, ExistingAnchor);
			if (HopDistance == INDEX_NONE)
			{
				continue;
			}

			if (MinHopDistance == INDEX_NONE || HopDistance < MinHopDistance)
			{
				MinHopDistance = HopDistance;
			}
		}

		return MinHopDistance;
	}

	int32 GetMinHopDistanceToOtherNeutralItems(const AAnchorPoint* CandidateAnchor,
	                                           const FWorldCampaignPlacementState& WorkingPlacementState,
	                                           const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
	{
		int32 MinHopDistance = INDEX_NONE;
		TArray<FGuid> SortedNeutralAnchorKeys;
		SortedNeutralAnchorKeys.Reserve(WorkingPlacementState.NeutralItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapNeutralObjectType>& ExistingPair : WorkingPlacementState.NeutralItemsByAnchorKey)
		{
			SortedNeutralAnchorKeys.Add(ExistingPair.Key);
		}

		Algo::Sort(SortedNeutralAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& ExistingAnchorKey : SortedNeutralAnchorKeys)
		{
			AAnchorPoint* ExistingAnchor = FindAnchorByKey(AnchorLookup, ExistingAnchorKey);
			if (not IsValid(ExistingAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, ExistingAnchor);
			if (HopDistance == INDEX_NONE)
			{
				continue;
			}

			if (MinHopDistance == INDEX_NONE || HopDistance < MinHopDistance)
			{
				MinHopDistance = HopDistance;
			}
		}

		return MinHopDistance;
	}

	FString BuildAdjacencyRequirementSummary(const FMapObjectAdjacencyRequirement& Requirement)
	{
		if (not Requirement.bEnabled)
		{
			return FString();
		}

		const UEnum* ItemTypeEnum = StaticEnum<EMapItemType>();
		const FString ItemTypeName = ItemTypeEnum
			                             ? ItemTypeEnum->GetNameStringByValue(
				                             static_cast<int64>(Requirement.RequiredItemType))
			                             : TEXT("Item");

		FString SubTypeName;
		if (Requirement.RequiredItemType == EMapItemType::NeutralItem)
		{
			const UEnum* NeutralEnum = StaticEnum<EMapNeutralObjectType>();
			SubTypeName = NeutralEnum
				              ? NeutralEnum->GetNameStringByValue(
					              static_cast<int64>(Requirement.RawByteSpecificItemSubtype))
				              : TEXT("Neutral");
		}
		else if (Requirement.RequiredItemType == EMapItemType::Mission)
		{
			const UEnum* MissionEnum = StaticEnum<EMapMission>();
			SubTypeName = MissionEnum
				              ? MissionEnum->GetNameStringByValue(
					              static_cast<int64>(Requirement.RawByteSpecificItemSubtype))
				              : TEXT("Mission");
		}
		else if (Requirement.RequiredItemType == EMapItemType::EnemyItem)
		{
			const UEnum* EnemyEnum = StaticEnum<EMapEnemyItem>();
			SubTypeName = EnemyEnum
				              ? EnemyEnum->GetNameStringByValue(
					              static_cast<int64>(Requirement.RawByteSpecificItemSubtype))
				              : TEXT("Enemy");
		}

		const UEnum* PolicyEnum = StaticEnum<EAdjacencyPolicy>();
		const FString PolicyName = PolicyEnum
			                           ? PolicyEnum->GetNameStringByValue(
				                           static_cast<int64>(Requirement.Policy))
			                           : TEXT("Policy");

		if (SubTypeName.IsEmpty())
		{
			return FString::Printf(TEXT("Adj:%s Hops<=%d %s"), *ItemTypeName, Requirement.MaxHops, *PolicyName);
		}

		return FString::Printf(TEXT("Adj:%s(%s) Hops<=%d %s"), *ItemTypeName, *SubTypeName, Requirement.MaxHops,
		                       *PolicyName);
	}

	bool TrySelectEnemyPlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                      const FEnemyItemPlacementRules& EffectiveRules,
	                                      EMapEnemyItem EnemyType,
	                                      int32 AttemptIndex,
	                                      int32 ExistingCount,
	                                      int32 SafeZoneMaxHops,
	                                      const FWorldCampaignPlacementState& WorkingPlacementState,
	                                      const FWorldCampaignDerivedData& WorkingDerivedData,
	                                      const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                      FPlacementCandidate& OutCandidate)
	{
		TArray<FPlacementCandidate> Candidates;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : WorkingPlacementState.CachedAnchors)
		{
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
			if (IsAnchorOccupied(CandidateKey, WorkingPlacementState))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugEnemyPlacementRejected(CandidateAnchor, EnemyType, TEXT("occupied"));
					}
				}

				continue;
			}

			const int32 HopDistanceFromEnemyHQ = GetCachedHopDistance(
				WorkingDerivedData.EnemyHQHopDistancesByAnchorKey,
				CandidateKey);
			if (HopDistanceFromEnemyHQ == INDEX_NONE)
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugEnemyPlacementRejected(CandidateAnchor, EnemyType, TEXT("no hop cache"));
					}
				}

				continue;
			}

			const int32 MinHopsFromEnemyHQ = EffectiveRules.EnemyHQSpacing.MinHopsFromEnemyHQ;
			const int32 MaxHopsFromEnemyHQ = FMath::Max(MinHopsFromEnemyHQ,
			                                            EffectiveRules.EnemyHQSpacing.MaxHopsFromEnemyHQ);
			if (HopDistanceFromEnemyHQ < MinHopsFromEnemyHQ || HopDistanceFromEnemyHQ > MaxHopsFromEnemyHQ)
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugEnemyPlacementRejected(CandidateAnchor, EnemyType,
						                                              TEXT("enemyHQ hops out of bounds"));
					}
				}

				continue;
			}

			if (SafeZoneMaxHops > 0)
			{
				const int32 HopDistanceFromPlayerHQ = GetCachedHopDistance(
					WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
					CandidateKey);
				if (HopDistanceFromPlayerHQ != INDEX_NONE && HopDistanceFromPlayerHQ <= SafeZoneMaxHops)
				{
					if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
					{
						if (Generator.GetIsValidCampaignDebugger())
						{
							UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
							CampaignDebugger->DebugEnemyPlacementRejected(CandidateAnchor, EnemyType,
							                                              TEXT("safe zone"));
						}
					}

					continue;
				}
			}

			if (not PassesEnemySpacingRules(EffectiveRules, CandidateAnchor, EnemyType, WorkingPlacementState,
			                                AnchorLookup))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugEnemyPlacementRejected(CandidateAnchor, EnemyType,
						                                              TEXT("intersects spacing"));
					}
				}

				continue;
			}

			const int32 ConnectionDegree = Generator.GetAnchorConnectionDegree(CandidateAnchor);
			const float ChokepointScore = WorkingDerivedData.ChokepointScoresByAnchorKey.FindRef(CandidateKey);
			const float PreferenceScore = GetEnemyPreferenceScore(EffectiveRules.EnemyHQSpacing.Preference,
			                                                      ConnectionDegree,
			                                                      ChokepointScore,
			                                                      HopDistanceFromEnemyHQ,
			                                                      EffectiveRules.EnemyHQSpacing.MinHopsFromEnemyHQ,
			                                                      EffectiveRules.EnemyHQSpacing.MaxHopsFromEnemyHQ);

			FPlacementCandidate Candidate;
			Candidate.AnchorPoint = CandidateAnchor;
			Candidate.AnchorKey = CandidateKey;
			Candidate.Score = PreferenceScore;
			Candidate.HopDistanceFromHQ = HopDistanceFromEnemyHQ;
			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, [](const FPlacementCandidate& Left, const FPlacementCandidate& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		const int32 CandidateIndex = (AttemptIndex + ExistingCount) % Candidates.Num();
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	bool TrySelectEnemyWallPlacementCandidate(const AGeneratorWorldCampaign& Generator,
	                                          const FEnemyWallPlacementRules& PlacementRules,
	                                          const FWorldCampaignPlacementState& PlacementState,
	                                          const FWorldCampaignDerivedData& DerivedData,
	                                          int32 AttemptIndex,
	                                          FPlacementCandidate& OutCandidate)
	{
		TArray<FPlacementCandidate> Candidates;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : PlacementRules.AnchorCandidates)
		{
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			if (not Generator.IsAnchorCached(CandidateAnchor))
			{
				continue;
			}

			const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
			if (IsAnchorOccupied(CandidateKey, PlacementState))
			{
				continue;
			}

			const int32 ConnectionDegree = Generator.GetAnchorConnectionDegree(CandidateAnchor);
			const float ChokepointScore = DerivedData.ChokepointScoresByAnchorKey.FindRef(CandidateKey);
			const float PreferenceScore = GetEnemyWallPreferenceScore(PlacementRules.Preference, ConnectionDegree,
			                                                          ChokepointScore);

			FPlacementCandidate Candidate;
			Candidate.AnchorPoint = CandidateAnchor;
			Candidate.AnchorKey = CandidateKey;
			Candidate.Score = PreferenceScore;
			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, [](const FPlacementCandidate& Left, const FPlacementCandidate& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		const int32 CandidateIndex = AttemptIndex % Candidates.Num();
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	bool TryPlaceEnemyItemsForType(AGeneratorWorldCampaign& Generator,
	                               EMapEnemyItem EnemyType,
	                               int32 RequiredCount,
	                               const FEnemyItemRuleset& Ruleset,
	                               int32 AttemptIndex,
	                               const FRuleRelaxationState& RelaxationState,
	                               int32 SafeZoneMaxHops,
	                               FWorldCampaignPlacementState& WorkingPlacementState,
	                               FWorldCampaignDerivedData& WorkingDerivedData,
	                               const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                               TArray<TPair<TObjectPtr<AAnchorPoint>, EMapEnemyItem>>& OutPromotions)
	{
		for (int32 PlacementIndex = 0; PlacementIndex < RequiredCount; PlacementIndex++)
		{
			const int32 ExistingCount = WorkingDerivedData.EnemyItemPlacedCounts.FindRef(EnemyType);
			FEnemyItemPlacementRules EffectiveRules = Ruleset.BaseRules;
			int32 SelectedVariantIndex = INDEX_NONE;
			int32 EnabledVariantCount = 0;
			bool bUsedVariant = false;

			if (Ruleset.VariantMode == EEnemyRuleVariantSelectionMode::CycleByPlacementIndex)
			{
				TArray<FEnemyItemPlacementVariant> EnabledVariants;
				for (const FEnemyItemPlacementVariant& Variant : Ruleset.Variants)
				{
					if (Variant.bEnabled)
					{
						EnabledVariants.Add(Variant);
					}
				}

				if (EnabledVariants.Num() > 0)
				{
					EnabledVariantCount = EnabledVariants.Num();
					SelectedVariantIndex = ExistingCount % EnabledVariantCount;
					bUsedVariant = true;

					const FEnemyItemPlacementVariant& SelectedVariant = EnabledVariants[SelectedVariantIndex];
					if (SelectedVariant.bOverrideRules)
					{
						EffectiveRules = SelectedVariant.OverrideRules;
					}
				}
			}

			if (RelaxationState.bRelaxDistance)
			{
				EffectiveRules.EnemyHQSpacing.MinHopsFromEnemyHQ = 0;
				EffectiveRules.EnemyHQSpacing.MaxHopsFromEnemyHQ = RelaxedHopDistanceMax;
			}

			if (RelaxationState.bRelaxSpacing)
			{
				EffectiveRules.ItemSpacing.MinEnemySeparationHopsOtherType = 0;
				EffectiveRules.ItemSpacing.MinEnemySeparationHopsSameType = 0;
			}

			if (RelaxationState.bRelaxPreference)
			{
				EffectiveRules.EnemyHQSpacing.Preference = EEnemyTopologySearchStrategy::None;
			}

			FPlacementCandidate SelectedCandidate;
			if (not TrySelectEnemyPlacementCandidate(Generator, EffectiveRules, EnemyType, AttemptIndex, ExistingCount,
			                                         SafeZoneMaxHops, WorkingPlacementState, WorkingDerivedData,
			                                         AnchorLookup, SelectedCandidate))
			{
				return false;
			}

			if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
			{
				if (Generator.GetIsValidCampaignDebugger())
				{
					UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
					FWorldCampaignEnemyPlacementDebugInfo DebugInfo;
					DebugInfo.EnemyType = EnemyType;
					DebugInfo.bSafeZoneRelevant = SafeZoneMaxHops > 0;
					DebugInfo.bIncludeAnchorDegree =
						EffectiveRules.EnemyHQSpacing.Preference == EEnemyTopologySearchStrategy::PreferLowDegree
						|| EffectiveRules.EnemyHQSpacing.Preference == EEnemyTopologySearchStrategy::PreferHighDegree
						|| EffectiveRules.EnemyHQSpacing.Preference == EEnemyTopologySearchStrategy::PreferDeadEnds;

					if (Generator.bM_DebugEnemyHQHops)
					{
						DebugInfo.HopFromEnemyHQ = GetCachedHopDistance(
							WorkingDerivedData.EnemyHQHopDistancesByAnchorKey,
							SelectedCandidate.AnchorKey);
					}

					if (Generator.bM_DebugPlayerHQHops && DebugInfo.bSafeZoneRelevant)
					{
						DebugInfo.HopFromPlayerHQ = GetCachedHopDistance(
							WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
							SelectedCandidate.AnchorKey);
					}

					if (Generator.bM_DebugAnchorDegree && DebugInfo.bIncludeAnchorDegree)
					{
						DebugInfo.AnchorDegree = Generator.GetAnchorConnectionDegree(SelectedCandidate.AnchorPoint);
					}

					if (Generator.bM_DisplayVariationEnemyObjectPlacement && bUsedVariant)
					{
						DebugInfo.bHasVariant = true;
						DebugInfo.VariantIndex = SelectedVariantIndex;
						DebugInfo.VariantCount = EnabledVariantCount;
					}

					if (Generator.bM_DisplayHopsFromSameEnemyItems)
					{
						DebugInfo.MinSameTypeHopDistance = GetMinHopDistanceToSameEnemyType(
							SelectedCandidate.AnchorPoint,
							EnemyType,
							WorkingPlacementState,
							AnchorLookup);
					}

					CampaignDebugger->DebugEnemyPlacementAccepted(SelectedCandidate.AnchorPoint, DebugInfo);
				}
			}

			WorkingPlacementState.EnemyItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, EnemyType);
			WorkingDerivedData.EnemyItemPlacedCounts.Add(EnemyType, ExistingCount + 1);
			OutPromotions.Add({SelectedCandidate.AnchorPoint, EnemyType});
		}

		return true;
	}

	bool TrySelectNeutralPlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                        EMapNeutralObjectType NeutralType,
	                                        const FNeutralItemPlacementRules& PlacementRules,
	                                        int32 AttemptIndex,
	                                        int32 ExistingCount,
	                                        const FWorldCampaignPlacementState& WorkingPlacementState,
	                                        const FWorldCampaignDerivedData& WorkingDerivedData,
	                                        const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                        FPlacementCandidate& OutCandidate)
	{
		TArray<FPlacementCandidate> Candidates;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : WorkingPlacementState.CachedAnchors)
		{
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
			if (IsAnchorOccupied(CandidateKey, WorkingPlacementState))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugNeutralPlacementRejected(CandidateAnchor, NeutralType, TEXT("occupied"));
					}
				}

				continue;
			}

			const int32 HopDistanceFromHQ = GetCachedHopDistance(
				WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
				CandidateKey);
			if (HopDistanceFromHQ == INDEX_NONE)
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugNeutralPlacementRejected(CandidateAnchor, NeutralType,
						                                                TEXT("no hop cache"));
					}
				}

				continue;
			}

			const int32 MaxHopsFromHQClamped = FMath::Max(PlacementRules.MinHopsFromHQ, PlacementRules.MaxHopsFromHQ);
			if (HopDistanceFromHQ < PlacementRules.MinHopsFromHQ || HopDistanceFromHQ > MaxHopsFromHQClamped)
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugNeutralPlacementRejected(CandidateAnchor, NeutralType, TEXT("hop band"));
					}
				}

				continue;
			}

			if (not PassesNeutralSpacingRules(PlacementRules, CandidateAnchor, WorkingPlacementState, AnchorLookup))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DebugNeutralPlacementRejected(CandidateAnchor, NeutralType, TEXT("spacing"));
					}
				}

				continue;
			}

			FPlacementCandidate Candidate;
			Candidate.AnchorPoint = CandidateAnchor;
			Candidate.AnchorKey = CandidateKey;
			Candidate.Score = GetTopologyPreferenceScore(PlacementRules.Preference,
			                                             static_cast<float>(HopDistanceFromHQ));
			Candidate.HopDistanceFromHQ = HopDistanceFromHQ;
			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, [](const FPlacementCandidate& Left, const FPlacementCandidate& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		const int32 CandidateIndex = (AttemptIndex + ExistingCount) % Candidates.Num();
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	bool TryPlaceNeutralItemsForType(EMapNeutralObjectType NeutralType,
	                                 int32 RequiredCount,
	                                 int32 AttemptIndex,
	                                 const FRuleRelaxationState& RelaxationState,
	                                 const FNeutralItemPlacementRules& BaseRules,
	                                 FWorldCampaignPlacementState& WorkingPlacementState,
	                                 FWorldCampaignDerivedData& WorkingDerivedData,
	                                 const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                 TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& OutPromotions,
	                                 AGeneratorWorldCampaign& Generator)
	{
		for (int32 PlacementIndex = 0; PlacementIndex < RequiredCount; PlacementIndex++)
		{
			const int32 ExistingCount = WorkingDerivedData.NeutralItemPlacedCounts.FindRef(NeutralType);
			FNeutralItemPlacementRules PlacementRules = BaseRules;

			if (RelaxationState.bRelaxDistance)
			{
				PlacementRules.MinHopsFromHQ = 0;
				PlacementRules.MaxHopsFromHQ = RelaxedHopDistanceMax;
			}

			if (RelaxationState.bRelaxSpacing)
			{
				PlacementRules.MinHopsFromOtherNeutralItems = 0;
				PlacementRules.MaxHopsFromOtherNeutralItems = RelaxedHopDistanceMax;
			}

			if (RelaxationState.bRelaxPreference)
			{
				PlacementRules.Preference = ETopologySearchStrategy::NotSet;
			}

			FPlacementCandidate SelectedCandidate;
			if (not TrySelectNeutralPlacementCandidate(Generator, NeutralType, PlacementRules, AttemptIndex,
			                                           ExistingCount,
			                                           WorkingPlacementState, WorkingDerivedData, AnchorLookup,
			                                           SelectedCandidate))
			{
				return false;
			}

			if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
			{
				if (Generator.GetIsValidCampaignDebugger())
				{
					UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
					FWorldCampaignNeutralPlacementDebugInfo DebugInfo;
					DebugInfo.NeutralType = NeutralType;

					if (Generator.bM_DebugPlayerHQHops)
					{
						DebugInfo.HopFromPlayerHQ = GetCachedHopDistance(
							WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
							SelectedCandidate.AnchorKey);
					}

					if (Generator.bM_DisplayHopsFromOtherNeutralItems)
					{
						DebugInfo.MinHopFromOtherNeutral = GetMinHopDistanceToOtherNeutralItems(
							SelectedCandidate.AnchorPoint,
							WorkingPlacementState,
							AnchorLookup);
					}

					CampaignDebugger->DebugNeutralPlacementAccepted(SelectedCandidate.AnchorPoint, DebugInfo);
				}
			}

			WorkingPlacementState.NeutralItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, NeutralType);
			WorkingDerivedData.NeutralItemPlacedCounts.Add(NeutralType, ExistingCount + 1);
			OutPromotions.Add({SelectedCandidate.AnchorPoint, NeutralType});
		}

		return true;
	}

	void AppendNeutralAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
	                                   const AAnchorPoint* CandidateAnchor,
	                                   const FWorldCampaignPlacementState& WorkingPlacementState,
	                                   const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                   TArray<int32>& InOutDistances)
	{
		const EMapNeutralObjectType RequiredNeutralType =
			static_cast<EMapNeutralObjectType>(Requirement.RawByteSpecificItemSubtype);
		TArray<FGuid> SortedNeutralAnchorKeys;
		SortedNeutralAnchorKeys.Reserve(WorkingPlacementState.NeutralItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapNeutralObjectType>& NeutralPair : WorkingPlacementState.NeutralItemsByAnchorKey)
		{
			SortedNeutralAnchorKeys.Add(NeutralPair.Key);
		}

		Algo::Sort(SortedNeutralAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& NeutralAnchorKey : SortedNeutralAnchorKeys)
		{
			const EMapNeutralObjectType NeutralTypeAtAnchor =
				WorkingPlacementState.NeutralItemsByAnchorKey.FindRef(NeutralAnchorKey);

			if (NeutralTypeAtAnchor != RequiredNeutralType)
			{
				continue;
			}

			AAnchorPoint* NeutralAnchor = FindAnchorByKey(AnchorLookup, NeutralAnchorKey);
			if (not IsValid(NeutralAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, NeutralAnchor);
			if (HopDistance != INDEX_NONE && HopDistance <= Requirement.MaxHops)
			{
				InOutDistances.Add(HopDistance);
			}
		}

		// Optional: keep the output stable too (if later code ever uses ordering).
		InOutDistances.Sort();
	}

	void AppendMissionAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
	                                   const AAnchorPoint* CandidateAnchor,
	                                   const FWorldCampaignPlacementState& WorkingPlacementState,
	                                   const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                   TArray<int32>& InOutDistances)
	{
		const EMapMission RequiredMissionType =
			static_cast<EMapMission>(Requirement.RawByteSpecificItemSubtype);
		TArray<FGuid> SortedMissionAnchorKeys;
		SortedMissionAnchorKeys.Reserve(WorkingPlacementState.MissionsByAnchorKey.Num());
		for (const TPair<FGuid, EMapMission>& MissionPair : WorkingPlacementState.MissionsByAnchorKey)
		{
			SortedMissionAnchorKeys.Add(MissionPair.Key);
		}

		Algo::Sort(SortedMissionAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& MissionAnchorKey : SortedMissionAnchorKeys)
		{
			const EMapMission MissionTypeAtAnchor = WorkingPlacementState.MissionsByAnchorKey.FindRef(MissionAnchorKey);
			if (MissionTypeAtAnchor != RequiredMissionType)
			{
				continue;
			}

			AAnchorPoint* MissionAnchor = FindAnchorByKey(AnchorLookup, MissionAnchorKey);
			if (not IsValid(MissionAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, MissionAnchor);
			if (HopDistance != INDEX_NONE && HopDistance <= Requirement.MaxHops)
			{
				InOutDistances.Add(HopDistance);
			}
		}

		InOutDistances.Sort();
	}

	void AppendEnemyAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
	                                 const AAnchorPoint* CandidateAnchor,
	                                 const FWorldCampaignPlacementState& WorkingPlacementState,
	                                 const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                 TArray<int32>& InOutDistances)
	{
		const EMapEnemyItem RequiredEnemyType =
			static_cast<EMapEnemyItem>(Requirement.RawByteSpecificItemSubtype);
		TArray<FGuid> SortedEnemyAnchorKeys;
		SortedEnemyAnchorKeys.Reserve(WorkingPlacementState.EnemyItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : WorkingPlacementState.EnemyItemsByAnchorKey)
		{
			SortedEnemyAnchorKeys.Add(EnemyPair.Key);
		}

		Algo::Sort(SortedEnemyAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& EnemyAnchorKey : SortedEnemyAnchorKeys)
		{
			const EMapEnemyItem EnemyTypeAtAnchor = WorkingPlacementState.EnemyItemsByAnchorKey.FindRef(EnemyAnchorKey);
			if (EnemyTypeAtAnchor != RequiredEnemyType)
			{
				continue;
			}

			AAnchorPoint* EnemyAnchor = FindAnchorByKey(AnchorLookup, EnemyAnchorKey);
			if (not IsValid(EnemyAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, EnemyAnchor);
			if (HopDistance != INDEX_NONE && HopDistance <= Requirement.MaxHops)
			{
				InOutDistances.Add(HopDistance);
			}
		}

		InOutDistances.Sort();
	}

	void AppendPlayerAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
	                                  const AAnchorPoint* CandidateAnchor,
	                                  const FWorldCampaignPlacementState& WorkingPlacementState,
	                                  TArray<int32>& InOutDistances)
	{
		const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(
			CandidateAnchor,
			WorkingPlacementState.PlayerHQAnchor);
		if (HopDistance != INDEX_NONE && HopDistance <= Requirement.MaxHops)
		{
			InOutDistances.Add(HopDistance);
		}
	}

	TArray<int32> BuildAdjacencyMatchDistances(const FMapObjectAdjacencyRequirement& Requirement,
	                                           const AAnchorPoint* CandidateAnchor,
	                                           const FWorldCampaignPlacementState& WorkingPlacementState,
	                                           const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
	{
		TArray<int32> MatchingHopDistances;
		if (not IsValid(CandidateAnchor))
		{
			return MatchingHopDistances;
		}

		switch (Requirement.RequiredItemType)
		{
		case EMapItemType::NeutralItem:
			AppendNeutralAdjacencyMatches(Requirement, CandidateAnchor, WorkingPlacementState, AnchorLookup,
			                              MatchingHopDistances);
			break;
		case EMapItemType::Mission:
			AppendMissionAdjacencyMatches(Requirement, CandidateAnchor, WorkingPlacementState, AnchorLookup,
			                              MatchingHopDistances);
			break;
		case EMapItemType::EnemyItem:
			AppendEnemyAdjacencyMatches(Requirement, CandidateAnchor, WorkingPlacementState, AnchorLookup,
			                            MatchingHopDistances);
			break;
		case EMapItemType::PlayerItem:
			AppendPlayerAdjacencyMatches(Requirement, CandidateAnchor, WorkingPlacementState, MatchingHopDistances);
			break;
		case EMapItemType::Empty:
		case EMapItemType::None:
		default:
			break;
		}

		return MatchingHopDistances;
	}

	bool TrySelectCompanionNeutralAnchor(const AAnchorPoint* CandidateAnchor,
	                                     const FMapObjectAdjacencyRequirement& Requirement,
	                                     const FNeutralItemPlacementRules& NeutralRules,
	                                     const FRuleRelaxationState& RelaxationState,
	                                     const FWorldCampaignPlacementState& WorkingPlacementState,
	                                     const FWorldCampaignDerivedData& WorkingDerivedData,
	                                     const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                     AAnchorPoint*& OutCompanionAnchor)
	{
		OutCompanionAnchor = nullptr;
		if (not IsValid(CandidateAnchor))
		{
			return false;
		}

		FNeutralItemPlacementRules EffectiveRules = NeutralRules;
		if (RelaxationState.bRelaxDistance)
		{
			EffectiveRules.MinHopsFromHQ = 0;
			EffectiveRules.MaxHopsFromHQ = RelaxedHopDistanceMax;
		}

		if (RelaxationState.bRelaxSpacing)
		{
			EffectiveRules.MinHopsFromOtherNeutralItems = 0;
			EffectiveRules.MaxHopsFromOtherNeutralItems = RelaxedHopDistanceMax;
		}

		if (RelaxationState.bRelaxPreference)
		{
			EffectiveRules.Preference = ETopologySearchStrategy::NotSet;
		}

		TArray<FPlacementCandidate> Candidates;
		for (const TObjectPtr<AAnchorPoint>& NearbyAnchor : WorkingPlacementState.CachedAnchors)
		{
			if (not IsValid(NearbyAnchor))
			{
				continue;
			}

			const FGuid NearbyKey = NearbyAnchor->GetAnchorKey();
			if (IsAnchorOccupied(NearbyKey, WorkingPlacementState))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, NearbyAnchor);
			if (HopDistance == INDEX_NONE || HopDistance > Requirement.MaxHops)
			{
				continue;
			}

			if (not PassesNeutralDistanceRules(EffectiveRules, NearbyKey, WorkingDerivedData))
			{
				continue;
			}

			if (not PassesNeutralSpacingRules(EffectiveRules, NearbyAnchor, WorkingPlacementState, AnchorLookup))
			{
				continue;
			}

			const int32 HopDistanceFromHQ = GetCachedHopDistance(
				WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
				NearbyKey);

			FPlacementCandidate Candidate;
			Candidate.AnchorPoint = NearbyAnchor;
			Candidate.AnchorKey = NearbyKey;
			Candidate.Score = GetTopologyPreferenceScore(EffectiveRules.Preference,
			                                             static_cast<float>(HopDistanceFromHQ));
			Candidate.HopDistanceFromHQ = HopDistanceFromHQ;
			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, [](const FPlacementCandidate& Left, const FPlacementCandidate& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		OutCompanionAnchor = Candidates[0].AnchorPoint.Get();
		return IsValid(OutCompanionAnchor);
	}

	float GetMissionPreferenceScore(const FMissionTierRules& EffectiveRules,
	                                const AAnchorPoint* CandidateAnchor,
	                                const FWorldCampaignPlacementState& WorkingPlacementState,
	                                const FWorldCampaignDerivedData& WorkingDerivedData,
	                                float NearestMissionHopDistance,
	                                float NearestMissionXYDistance,
	                                int32 ConnectionDegree)
	{
		float PreferenceScore = 0.f;
		if (EffectiveRules.bUseHopsDistanceFromHQ)
		{
			const int32 HopDistanceFromHQ = GetCachedHopDistance(
				WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
				CandidateAnchor->GetAnchorKey());
			PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.HopsDistancePreference,
			                                              static_cast<float>(HopDistanceFromHQ));
		}

		if (EffectiveRules.bUseXYDistanceFromHQ)
		{
			const float XYDistance = CampaignGenerationHelper::XYDistanceFromHQ(
				CandidateAnchor,
				WorkingPlacementState.PlayerHQAnchor);
			PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.XYDistancePreference, XYDistance);
		}

		if (EffectiveRules.bUseMissionSpacingHops && WorkingPlacementState.MissionsByAnchorKey.Num() > 0)
		{
			PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.MissionSpacingHopsPreference,
			                                              NearestMissionHopDistance);
		}

		if (EffectiveRules.bUseMissionSpacingXY && WorkingPlacementState.MissionsByAnchorKey.Num() > 0)
		{
			PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.MissionSpacingXYPreference,
			                                              NearestMissionXYDistance);
		}

		PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.ConnectionPreference,
		                                              static_cast<float>(ConnectionDegree));
		return PreferenceScore;
	}

	float GetOverrideMissionPreferenceScore(ETopologySearchStrategy OverrideConnectionPreference,
	                                        ETopologySearchStrategy OverrideHopsPreference,
	                                        int32 ConnectionDegree,
	                                        int32 HopDistanceFromHQ)
	{
		float PreferenceScore = 0.f;
		PreferenceScore += GetTopologyPreferenceScore(OverrideConnectionPreference,
		                                              static_cast<float>(ConnectionDegree));
		PreferenceScore += GetTopologyPreferenceScore(OverrideHopsPreference,
		                                              static_cast<float>(HopDistanceFromHQ));
		return PreferenceScore;
	}

	bool PassesMissionTopologyRules(const FMissionTierRules& EffectiveRules, int32 ConnectionDegree)
	{
		return ConnectionDegree >= EffectiveRules.MinConnections
			&& ConnectionDegree <= EffectiveRules.MaxConnections;
	}

	bool TryGetMissionSpacingData(const FMissionTierRules& EffectiveRules,
	                              const AAnchorPoint* CandidateAnchor,
	                              const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                              const FWorldCampaignPlacementState& WorkingPlacementState,
	                              float& OutNearestMissionHopDistance,
	                              float& OutNearestMissionXYDistance)
	{
		OutNearestMissionHopDistance = 0.f;
		OutNearestMissionXYDistance = 0.f;
		if (WorkingPlacementState.MissionsByAnchorKey.Num() == 0)
		{
			return true;
		}

		OutNearestMissionHopDistance = static_cast<float>(RelaxedHopDistanceMax);
		OutNearestMissionXYDistance = RelaxedDistanceMax;

		TArray<FGuid> SortedMissionAnchorKeys;
		SortedMissionAnchorKeys.Reserve(WorkingPlacementState.MissionsByAnchorKey.Num());
		for (const TPair<FGuid, EMapMission>& MissionPair : WorkingPlacementState.MissionsByAnchorKey)
		{
			SortedMissionAnchorKeys.Add(MissionPair.Key);
		}

		Algo::Sort(SortedMissionAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& MissionAnchorKey : SortedMissionAnchorKeys)
		{
			AAnchorPoint* MissionAnchor = FindAnchorByKey(AnchorLookup, MissionAnchorKey);
			if (not IsValid(MissionAnchor))
			{
				continue;
			}

			if (EffectiveRules.bUseMissionSpacingHops)
			{
				const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, MissionAnchor);
				if (HopDistance == INDEX_NONE)
				{
					return false;
				}

				const int32 MaxSpacingClamped = FMath::Max(EffectiveRules.MinMissionSpacingHops,
				                                           EffectiveRules.MaxMissionSpacingHops);
				if (HopDistance < EffectiveRules.MinMissionSpacingHops || HopDistance > MaxSpacingClamped)
				{
					return false;
				}

				OutNearestMissionHopDistance =
					FMath::Min(OutNearestMissionHopDistance, static_cast<float>(HopDistance));
			}

			if (EffectiveRules.bUseMissionSpacingXY)
			{
				const float XYDistance = CampaignGenerationHelper::XYDistanceFromHQ(CandidateAnchor, MissionAnchor);
				if (XYDistance < EffectiveRules.MinMissionSpacingXY || XYDistance > EffectiveRules.MaxMissionSpacingXY)
				{
					return false;
				}

				OutNearestMissionXYDistance = FMath::Min(OutNearestMissionXYDistance, XYDistance);
			}
		}

		return true;
	}

	bool TryApplyMissionAdjacencyRequirement(const FMapObjectAdjacencyRequirement& Requirement,
	                                         const AAnchorPoint* CandidateAnchor,
	                                         const FNeutralItemPlacementRules& NeutralRules,
	                                         const FRuleRelaxationState& RelaxationState,
	                                         const FWorldCampaignPlacementState& WorkingPlacementState,
	                                         const FWorldCampaignDerivedData& WorkingDerivedData,
	                                         const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                         FPlacementCandidate& InOutCandidate)
	{
		if (not Requirement.bEnabled)
		{
			return true;
		}

		const TArray<int32> MatchingHopDistances = BuildAdjacencyMatchDistances(
			Requirement,
			CandidateAnchor,
			WorkingPlacementState,
			AnchorLookup);
		const bool bHasAdjacency = HasMinimumAdjacentMatches(MatchingHopDistances,
		                                                     Requirement.MinMatchingCount);
		if (bHasAdjacency)
		{
			return true;
		}

		if (Requirement.Policy == EAdjacencyPolicy::RejectIfMissing)
		{
			return false;
		}

		if (Requirement.Policy != EAdjacencyPolicy::TryAutoPlaceCompanion
			|| Requirement.RequiredItemType != EMapItemType::NeutralItem)
		{
			return false;
		}

		AAnchorPoint* CompanionAnchor = nullptr;
		if (not TrySelectCompanionNeutralAnchor(CandidateAnchor, Requirement, NeutralRules, RelaxationState,
		                                        WorkingPlacementState, WorkingDerivedData, AnchorLookup,
		                                        CompanionAnchor))
		{
			return false;
		}

		InOutCandidate.CompanionAnchor = CompanionAnchor;
		InOutCandidate.CompanionRawSubtype = Requirement.RawByteSpecificItemSubtype;
		InOutCandidate.CompanionItemType = EMapItemType::NeutralItem;
		return true;
	}

	bool TrySelectMissionPlacementCandidateOverride(AGeneratorWorldCampaign& Generator,
	                                                const FMissionTierRules& EffectiveRules,
	                                                int32 AttemptIndex,
	                                                int32 MissionIndex,
	                                                const FRuleRelaxationState& RelaxationState,
	                                                const FNeutralItemPlacementRules& NeutralRules,
	                                                const FWorldCampaignPlacementState& WorkingPlacementState,
	                                                const FWorldCampaignDerivedData& WorkingDerivedData,
	                                                const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                                const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
	                                                int32 OverrideMinConnections,
	                                                int32 OverrideMaxConnections,
	                                                ETopologySearchStrategy OverrideConnectionPreference,
	                                                int32 OverrideMinHopsFromPlayerHQ,
	                                                int32 OverrideMaxHopsFromPlayerHQ,
	                                                ETopologySearchStrategy OverrideHopsPreference,
	                                                FPlacementCandidate& OutCandidate)
	{
		TArray<FPlacementCandidate> Candidates;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : CandidateSource)
		{
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			if (not Generator.IsAnchorCached(CandidateAnchor))
			{
				continue;
			}

			const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
			const bool bAllowNeutralStacking = EffectiveRules.bNeutralItemRequired;
			if (IsAnchorOccupiedForMission(CandidateKey, WorkingPlacementState, bAllowNeutralStacking))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("occupied"));
					}
				}

				continue;
			}

			if (EffectiveRules.bNeutralItemRequired)
			{
				if (not HasNeutralTypeAtAnchor(WorkingPlacementState, CandidateKey,
				                               EffectiveRules.RequiredNeutralItemType))
				{
					if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
					{
						if (Generator.GetIsValidCampaignDebugger())
						{
							UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
							CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor,
							                                       TEXT("neutral requirement missing"));
						}
					}

					continue;
				}
			}

			int32 HopDistanceFromHQ = GetCachedHopDistance(
				WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
				CandidateKey);
			if (HopDistanceFromHQ == INDEX_NONE)
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("no hop cache"));
					}
				}

				continue;
			}

			if (HopDistanceFromHQ < OverrideMinHopsFromPlayerHQ
				|| HopDistanceFromHQ > OverrideMaxHopsFromPlayerHQ)
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("hop range"));
					}
				}

				continue;
			}

			if (EffectiveRules.bUseXYDistanceFromHQ)
			{
				const float XYDistanceFromHQ = CampaignGenerationHelper::XYDistanceFromHQ(
					CandidateAnchor,
					WorkingPlacementState.PlayerHQAnchor);
				if (XYDistanceFromHQ < EffectiveRules.MinXYDistanceFromHQ
					|| XYDistanceFromHQ > EffectiveRules.MaxXYDistanceFromHQ)
				{
					if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
					{
						if (Generator.GetIsValidCampaignDebugger())
						{
							UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
							CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("xy range"));
						}
					}

					continue;
				}
			}

			const int32 ConnectionDegree = Generator.GetAnchorConnectionDegree(CandidateAnchor);
			if (ConnectionDegree < OverrideMinConnections || ConnectionDegree > OverrideMaxConnections)
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("topology violation"));
					}
				}

				continue;
			}

			float NearestMissionHopDistance = 0.f;
			float NearestMissionXYDistance = 0.f;
			if (not TryGetMissionSpacingData(EffectiveRules, CandidateAnchor, AnchorLookup,
			                                 WorkingPlacementState, NearestMissionHopDistance,
			                                 NearestMissionXYDistance))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("spacing violation"));
					}
				}

				continue;
			}

			FPlacementCandidate Candidate;
			Candidate.AnchorPoint = CandidateAnchor;
			Candidate.AnchorKey = CandidateKey;
			Candidate.Score = GetOverrideMissionPreferenceScore(OverrideConnectionPreference,
			                                                    OverrideHopsPreference,
			                                                    ConnectionDegree,
			                                                    HopDistanceFromHQ);
			Candidate.HopDistanceFromHQ = HopDistanceFromHQ;

			const FMapObjectAdjacencyRequirement& AdjacencyRequirement = EffectiveRules.AdjacencyRequirement;
			if (not TryApplyMissionAdjacencyRequirement(AdjacencyRequirement, CandidateAnchor, NeutralRules,
			                                            RelaxationState, WorkingPlacementState, WorkingDerivedData,
			                                            AnchorLookup, Candidate))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("adjacency missing"));
					}
				}

				continue;
			}

			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, [](const FPlacementCandidate& Left, const FPlacementCandidate& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		const int32 CandidateIndex = (AttemptIndex + MissionIndex) % Candidates.Num();
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	bool TrySelectMissionPlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                        const FMissionTierRules& EffectiveRules,
	                                        EMapMission MissionType,
	                                        int32 AttemptIndex,
	                                        int32 MissionIndex,
	                                        const FRuleRelaxationState& RelaxationState,
	                                        const FNeutralItemPlacementRules& NeutralRules,
	                                        const FWorldCampaignPlacementState& WorkingPlacementState,
	                                        const FWorldCampaignDerivedData& WorkingDerivedData,
	                                        const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                        const TArray<TObjectPtr<AAnchorPoint>>* CandidateSourceOverride,
	                                        bool bOverridePlacementWithArray,
	                                        int32 OverrideMinConnections,
	                                        int32 OverrideMaxConnections,
	                                        ETopologySearchStrategy OverrideConnectionPreference,
	                                        int32 OverrideMinHopsFromPlayerHQ,
	                                        int32 OverrideMaxHopsFromPlayerHQ,
	                                        ETopologySearchStrategy OverrideHopsPreference,
	                                        FPlacementCandidate& OutCandidate)
	{
		if (bOverridePlacementWithArray)
		{
			if (CandidateSourceOverride == nullptr)
			{
				return false;
			}

			return TrySelectMissionPlacementCandidateOverride(Generator, EffectiveRules, AttemptIndex,
			                                                  MissionIndex, RelaxationState, NeutralRules,
			                                                  WorkingPlacementState, WorkingDerivedData, AnchorLookup,
			                                                  *CandidateSourceOverride, OverrideMinConnections,
			                                                  OverrideMaxConnections, OverrideConnectionPreference,
			                                                  OverrideMinHopsFromPlayerHQ,
			                                                  OverrideMaxHopsFromPlayerHQ, OverrideHopsPreference,
			                                                  OutCandidate);
		}

		const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource = CandidateSourceOverride
			                                                          ? *CandidateSourceOverride
			                                                          : WorkingPlacementState.CachedAnchors;
		TArray<FPlacementCandidate> Candidates;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : CandidateSource)
		{
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			if (CandidateSourceOverride != nullptr && not Generator.IsAnchorCached(CandidateAnchor))
			{
				continue;
			}

			const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
			const bool bAllowNeutralStacking = EffectiveRules.bNeutralItemRequired;
			if (IsAnchorOccupiedForMission(CandidateKey, WorkingPlacementState, bAllowNeutralStacking))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("occupied"));
					}
				}

				continue;
			}

			if (EffectiveRules.bNeutralItemRequired)
			{
				if (not HasNeutralTypeAtAnchor(WorkingPlacementState, CandidateKey,
				                               EffectiveRules.RequiredNeutralItemType))
				{
					if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
					{
						if (Generator.GetIsValidCampaignDebugger())
						{
							UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
							CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor,
							                                       TEXT("neutral requirement missing"));
						}
					}

					continue;
				}
			}

			int32 HopDistanceFromHQ = INDEX_NONE;
			if (EffectiveRules.bUseHopsDistanceFromHQ)
			{
				HopDistanceFromHQ = GetCachedHopDistance(
					WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
					CandidateKey);
				if (HopDistanceFromHQ == INDEX_NONE)
				{
					if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
					{
						if (Generator.GetIsValidCampaignDebugger())
						{
							UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
							CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("no hop cache"));
						}
					}

					continue;
				}

				const int32 MaxHopsFromHQClamped = FMath::Max(EffectiveRules.MinHopsFromHQ,
				                                              EffectiveRules.MaxHopsFromHQ);
				if (HopDistanceFromHQ < EffectiveRules.MinHopsFromHQ
					|| HopDistanceFromHQ > MaxHopsFromHQClamped)
				{
					if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
					{
						if (Generator.GetIsValidCampaignDebugger())
						{
							UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
							CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("hop range"));
						}
					}

					continue;
				}
			}

			if (EffectiveRules.bUseXYDistanceFromHQ)
			{
				const float XYDistanceFromHQ = CampaignGenerationHelper::XYDistanceFromHQ(
					CandidateAnchor,
					WorkingPlacementState.PlayerHQAnchor);
				if (XYDistanceFromHQ < EffectiveRules.MinXYDistanceFromHQ
					|| XYDistanceFromHQ > EffectiveRules.MaxXYDistanceFromHQ)
				{
					if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
					{
						if (Generator.GetIsValidCampaignDebugger())
						{
							UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
							CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("xy range"));
						}
					}

					continue;
				}
			}

			const int32 ConnectionDegree = Generator.GetAnchorConnectionDegree(CandidateAnchor);
			if (not PassesMissionTopologyRules(EffectiveRules, ConnectionDegree))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("topology violation"));
					}
				}

				continue;
			}

			float NearestMissionHopDistance = 0.f;
			float NearestMissionXYDistance = 0.f;
			if (not TryGetMissionSpacingData(EffectiveRules, CandidateAnchor, AnchorLookup,
			                                 WorkingPlacementState, NearestMissionHopDistance,
			                                 NearestMissionXYDistance))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("spacing violation"));
					}
				}

				continue;
			}

			FPlacementCandidate Candidate;
			Candidate.AnchorPoint = CandidateAnchor;
			Candidate.AnchorKey = CandidateKey;
			Candidate.Score = GetMissionPreferenceScore(EffectiveRules, CandidateAnchor, WorkingPlacementState,
			                                            WorkingDerivedData, NearestMissionHopDistance,
			                                            NearestMissionXYDistance, ConnectionDegree);
			Candidate.HopDistanceFromHQ = HopDistanceFromHQ;

			const FMapObjectAdjacencyRequirement& AdjacencyRequirement = EffectiveRules.AdjacencyRequirement;
			if (not TryApplyMissionAdjacencyRequirement(AdjacencyRequirement, CandidateAnchor, NeutralRules,
			                                            RelaxationState, WorkingPlacementState, WorkingDerivedData,
			                                            AnchorLookup, Candidate))
			{
				if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
				{
					if (Generator.GetIsValidCampaignDebugger())
					{
						UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
						CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, TEXT("adjacency missing"));
					}
				}

				continue;
			}

			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, [](const FPlacementCandidate& Left, const FPlacementCandidate& Right)
		{
			if (Left.Score != Right.Score)
			{
				return Left.Score > Right.Score;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		const int32 CandidateIndex = (AttemptIndex + MissionIndex) % Candidates.Num();
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	bool TryPlaceMissionForType(AGeneratorWorldCampaign& Generator,
	                            EMapMission MissionType,
	                            int32 MissionIndex,
	                            int32 AttemptIndex,
	                            const FRuleRelaxationState& RelaxationState,
	                            const FMissionPlacement& MissionPlacementRules,
	                            const FNeutralItemPlacementRules& NeutralRules,
	                            FWorldCampaignPlacementState& WorkingPlacementState,
	                            FWorldCampaignDerivedData& WorkingDerivedData,
	                            const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                            TArray<TPair<TObjectPtr<AAnchorPoint>, EMapMission>>& Promotions,
	                            TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& CompanionPromotions)
	{
		const FPerMissionRules* MissionRulesPtr = MissionPlacementRules.RulesByMission.Find(MissionType);
		if (not MissionRulesPtr)
		{
			return false;
		}

		FMissionTierRules EffectiveRules;
		if (MissionRulesPtr->bOverrideTierRules)
		{
			EffectiveRules = MissionRulesPtr->OverrideRules;
		}
		else
		{
			const FMissionTierRules* TierRules = MissionPlacementRules.RulesByTier.Find(MissionRulesPtr->Tier);
			if (not TierRules)
			{
				return false;
			}

			EffectiveRules = *TierRules;
		}

		if (RelaxationState.bRelaxDistance)
		{
			if (EffectiveRules.bUseHopsDistanceFromHQ)
			{
				EffectiveRules.MinHopsFromHQ = 0;
				EffectiveRules.MaxHopsFromHQ = RelaxedHopDistanceMax;
				EffectiveRules.HopsDistancePreference = ETopologySearchStrategy::NotSet;
			}

			if (EffectiveRules.bUseXYDistanceFromHQ)
			{
				EffectiveRules.MinXYDistanceFromHQ = 0.f;
				EffectiveRules.MaxXYDistanceFromHQ = RelaxedDistanceMax;
				EffectiveRules.XYDistancePreference = ETopologySearchStrategy::NotSet;
			}
		}

		if (RelaxationState.bRelaxSpacing)
		{
			if (EffectiveRules.bUseMissionSpacingHops)
			{
				EffectiveRules.MinMissionSpacingHops = 0;
				EffectiveRules.MaxMissionSpacingHops = RelaxedHopDistanceMax;
				EffectiveRules.MissionSpacingHopsPreference = ETopologySearchStrategy::NotSet;
			}

			if (EffectiveRules.bUseMissionSpacingXY)
			{
				EffectiveRules.MinMissionSpacingXY = 0.f;
				EffectiveRules.MaxMissionSpacingXY = RelaxedDistanceMax;
				EffectiveRules.MissionSpacingXYPreference = ETopologySearchStrategy::NotSet;
			}
		}

		if (RelaxationState.bRelaxPreference)
		{
			EffectiveRules.ConnectionPreference = ETopologySearchStrategy::NotSet;
		}

		const bool bOverridePlacementWithArray = MissionRulesPtr->bOverridePlacementWithArray;
		if (bOverridePlacementWithArray && MissionRulesPtr->OverridePlacementAnchorCandidates.Num() == 0)
		{
			const UEnum* MissionEnum = StaticEnum<EMapMission>();
			const FString MissionName = MissionEnum
				                            ? MissionEnum->GetNameStringByValue(
					                            static_cast<int64>(MissionType))
				                            : TEXT("Mission");
			const FString ErrorMessage = FString::Printf(
				TEXT("Mission placement failed: override anchor list empty for %s."),
				*MissionName);
			RTSFunctionLibrary::ReportError(ErrorMessage);
			return false;
		}

		const int32 EffectiveOverrideMinConnections = FMath::Min(MissionRulesPtr->OverrideMinConnections,
		                                                         MissionRulesPtr->OverrideMaxConnections);
		const int32 EffectiveOverrideMaxConnections = FMath::Max(MissionRulesPtr->OverrideMinConnections,
		                                                         MissionRulesPtr->OverrideMaxConnections);
		const ETopologySearchStrategy EffectiveOverrideConnectionPreference =
			MissionRulesPtr->OverrideConnectionPreference;
		const int32 EffectiveOverrideMinHopsFromPlayerHQ = FMath::Min(
			MissionRulesPtr->OverrideMinHopsFromPlayerHQ,
			MissionRulesPtr->OverrideMaxHopsFromPlayerHQ);
		const int32 EffectiveOverrideMaxHopsFromPlayerHQ = FMath::Max(
			MissionRulesPtr->OverrideMinHopsFromPlayerHQ,
			MissionRulesPtr->OverrideMaxHopsFromPlayerHQ);
		const ETopologySearchStrategy EffectiveOverrideHopsPreference = MissionRulesPtr->OverrideHopsPreference;

		const TArray<TObjectPtr<AAnchorPoint>>* CandidateSourceOverride = bOverridePlacementWithArray
			                                                                  ? &MissionRulesPtr->
			                                                                  OverridePlacementAnchorCandidates
			                                                                  : nullptr;

		FPlacementCandidate SelectedCandidate;
		if (not TrySelectMissionPlacementCandidate(Generator, EffectiveRules, MissionType, AttemptIndex, MissionIndex,
		                                           RelaxationState, NeutralRules, WorkingPlacementState,
		                                           WorkingDerivedData, AnchorLookup, CandidateSourceOverride,
		                                           bOverridePlacementWithArray, EffectiveOverrideMinConnections,
		                                           EffectiveOverrideMaxConnections,
		                                           EffectiveOverrideConnectionPreference,
		                                           EffectiveOverrideMinHopsFromPlayerHQ,
		                                           EffectiveOverrideMaxHopsFromPlayerHQ,
		                                           EffectiveOverrideHopsPreference, SelectedCandidate))
		{
			if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
			{
				if (not Generator.GetIsValidCampaignDebugger())
				{
					return false;
				}

				UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
				if (not Generator.bM_DebugFailedMissionPlacement)
				{
					return false;
				}

				AAnchorPoint* DebugAnchor = WorkingPlacementState.PlayerHQAnchor.Get();
				if (not IsValid(DebugAnchor))
				{
					return false;
				}

				const FString Reason = FString::Printf(
					TEXT("no candidates (attempt %d, relax D:%d S:%d P:%d)"),
					AttemptIndex,
					RelaxationState.bRelaxDistance ? 1 : 0,
					RelaxationState.bRelaxSpacing ? 1 : 0,
					RelaxationState.bRelaxPreference ? 1 : 0);
				CampaignDebugger->DebugMissionPlacementFailed(DebugAnchor, MissionType, Reason);
			}

			return false;
		}

		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (Generator.GetIsValidCampaignDebugger())
			{
				UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
				FWorldCampaignMissionPlacementDebugInfo DebugInfo;
				DebugInfo.MissionType = MissionType;

				if (Generator.bM_DisplayHopsFromHQForMissions)
				{
					DebugInfo.HopFromHQ = GetCachedHopDistance(
						WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
						SelectedCandidate.AnchorKey);
				}

				if (Generator.bM_DebugMissionSpacingHops && EffectiveRules.bUseMissionSpacingHops
					&& WorkingPlacementState.MissionsByAnchorKey.Num() > 0)
				{
					float NearestMissionHopDistance = -1.f;
					float NearestMissionXYDistance = 0.f;
					if (TryGetMissionSpacingData(EffectiveRules, SelectedCandidate.AnchorPoint, AnchorLookup,
					                             WorkingPlacementState, NearestMissionHopDistance,
					                             NearestMissionXYDistance))
					{
						DebugInfo.NearestMissionHopDistance = NearestMissionHopDistance;
					}

					DebugInfo.bUsesMissionSpacingHops = true;
				}

				if (Generator.bM_DisplayMinMaxConnectionsForMissionPlacement)
				{
					DebugInfo.bUsesConnectionRules = EffectiveRules.MinConnections > 0
						|| EffectiveRules.MaxConnections > 0;
					DebugInfo.MinConnections = EffectiveRules.MinConnections;
					DebugInfo.MaxConnections = EffectiveRules.MaxConnections;
				}

				if (Generator.bM_DisplayMissionAdjacencyRequirements
					&& EffectiveRules.AdjacencyRequirement.bEnabled)
				{
					DebugInfo.bHasAdjacencyRequirement = true;
					DebugInfo.AdjacencySummary = BuildAdjacencyRequirementSummary(
						EffectiveRules.AdjacencyRequirement);
				}

				if (Generator.bM_DisplayNeutralItemRequirementForMission && EffectiveRules.bNeutralItemRequired)
				{
					DebugInfo.bHasNeutralRequirement = true;
					DebugInfo.RequiredNeutralType = EffectiveRules.RequiredNeutralItemType;
				}

				if (bOverridePlacementWithArray)
				{
					DebugInfo.bUsesOverrideArray = true;
					DebugInfo.bOverrideArrayUsesConnectionBounds = true;
					DebugInfo.OverrideMinConnections = EffectiveOverrideMinConnections;
					DebugInfo.OverrideMaxConnections = EffectiveOverrideMaxConnections;
					DebugInfo.bOverrideArrayUsesHopsBounds = true;
					DebugInfo.OverrideMinHopsFromHQ = EffectiveOverrideMinHopsFromPlayerHQ;
					DebugInfo.OverrideMaxHopsFromHQ = EffectiveOverrideMaxHopsFromPlayerHQ;
					DebugInfo.OverrideConnectionPreference = EffectiveOverrideConnectionPreference;
					DebugInfo.OverrideHopsPreference = EffectiveOverrideHopsPreference;
				}

				CampaignDebugger->DebugMissionPlacementAccepted(SelectedCandidate.AnchorPoint, DebugInfo);
			}
		}

		WorkingPlacementState.MissionsByAnchorKey.Add(SelectedCandidate.AnchorKey, MissionType);
		const int32 ExistingMissionCount = WorkingDerivedData.MissionPlacedCounts.FindRef(MissionType);
		WorkingDerivedData.MissionPlacedCounts.Add(MissionType, ExistingMissionCount + 1);
		Promotions.Add({SelectedCandidate.AnchorPoint, MissionType});

		if (SelectedCandidate.CompanionItemType == EMapItemType::NeutralItem)
		{
			AAnchorPoint* CompanionAnchor = SelectedCandidate.CompanionAnchor.Get();
			if (not IsValid(CompanionAnchor))
			{
				return false;
			}

			const FGuid CompanionKey = CompanionAnchor->GetAnchorKey();
			const EMapNeutralObjectType CompanionType =
				static_cast<EMapNeutralObjectType>(SelectedCandidate.CompanionRawSubtype);
			WorkingPlacementState.NeutralItemsByAnchorKey.Add(CompanionKey, CompanionType);
			const int32 ExistingNeutralCount = WorkingDerivedData.NeutralItemPlacedCounts.FindRef(CompanionType);
			WorkingDerivedData.NeutralItemPlacedCounts.Add(CompanionType, ExistingNeutralCount + 1);
			CompanionPromotions.Add({CompanionAnchor, CompanionType});
		}

		return true;
	}

	TArray<FAnchorCandidate> BuildAndSortCandidates(AAnchorPoint* AnchorPoint,
	                                                const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
	                                                const int32 MaxConnections,
	                                                const float MaxPreferredDistance, const bool bIgnoreDistance)
	{
		TArray<FAnchorCandidate> Candidates;
		if (not IsValid(AnchorPoint))
		{
			return Candidates;
		}

		const FVector AnchorLocation = AnchorPoint->GetActorLocation();
		const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
		const float MaxDistanceSquared = FMath::Square(MaxPreferredDistance);

		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : AnchorPoints)
		{
			if (not IsValid(CandidateAnchor) || CandidateAnchor == AnchorPoint)
			{
				continue;
			}

			if (AnchorPoint->GetNeighborAnchors().Contains(CandidateAnchor))
			{
				continue;
			}

			if (CandidateAnchor->GetConnectionCount() >= MaxConnections)
			{
				continue;
			}

			const FVector CandidateLocation = CandidateAnchor->GetActorLocation();
			const FVector2D CandidateLocation2D(CandidateLocation.X, CandidateLocation.Y);
			const float DistanceSquared = FVector2D::DistSquared(AnchorLocation2D, CandidateLocation2D);
			if (not bIgnoreDistance && DistanceSquared > MaxDistanceSquared)
			{
				continue;
			}

			FAnchorCandidate Candidate;
			Candidate.AnchorPoint = CandidateAnchor;
			Candidate.DistanceSquared = DistanceSquared;
			Candidate.AnchorKey = CandidateAnchor->GetAnchorKey();
			Candidates.Add(Candidate);
		}

		Algo::Sort(Candidates, [](const FAnchorCandidate& Left, const FAnchorCandidate& Right)
		{
			if (Left.DistanceSquared != Right.DistanceSquared)
			{
				return Left.DistanceSquared < Right.DistanceSquared;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		return Candidates;
	}

	FClosestConnectionCandidate FindClosestConnectionCandidate(AAnchorPoint* AnchorPoint,
	                                                           const TArray<TObjectPtr<AConnection>>& Connections)
	{
		FClosestConnectionCandidate Result;
		if (not IsValid(AnchorPoint))
		{
			return Result;
		}

		const FVector AnchorLocation = AnchorPoint->GetActorLocation();
		const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);

		for (const TObjectPtr<AConnection>& Connection : Connections)
		{
			if (not IsValid(Connection))
			{
				continue;
			}

			if (Connection->GetIsThreeWayConnection())
			{
				continue;
			}

			const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
			if (ConnectedAnchors.Num() < 2)
			{
				continue;
			}

			AAnchorPoint* FirstAnchor = ConnectedAnchors[0];
			AAnchorPoint* SecondAnchor = ConnectedAnchors[1];
			if (not IsValid(FirstAnchor) || not IsValid(SecondAnchor))
			{
				continue;
			}

			if (ConnectedAnchors.Contains(AnchorPoint))
			{
				continue;
			}

			const FVector AnchorLocationA = FirstAnchor->GetActorLocation();
			const FVector AnchorLocationB = SecondAnchor->GetActorLocation();
			const FVector2D AnchorLocationA2D(AnchorLocationA.X, AnchorLocationA.Y);
			const FVector2D AnchorLocationB2D(AnchorLocationB.X, AnchorLocationB.Y);

			const FVector2D SegmentVector = AnchorLocationB2D - AnchorLocationA2D;
			const FVector2D AnchorVector = AnchorLocation2D - AnchorLocationA2D;
			const float SegmentLengthSquared = SegmentVector.SizeSquared();
			if (SegmentLengthSquared <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const float Projection = FVector2D::DotProduct(AnchorVector, SegmentVector) / SegmentLengthSquared;
			const float ClampedProjection = FMath::Clamp(Projection, 0.f, 1.f);
			const FVector2D ProjectedPoint2D = AnchorLocationA2D + SegmentVector * ClampedProjection;
			const float CandidateDistanceSquared = FVector2D::DistSquared(AnchorLocation2D, ProjectedPoint2D);

			if (CandidateDistanceSquared >= Result.DistanceSquared)
			{
				continue;
			}

			constexpr float HalfValue = 0.5f;
			const float JunctionZ = (AnchorLocationA.Z + AnchorLocationB.Z) * HalfValue;
			Result.DistanceSquared = CandidateDistanceSquared;
			Result.JunctionLocation = FVector(ProjectedPoint2D.X, ProjectedPoint2D.Y, JunctionZ);
			Result.Connection = Connection;
		}

		return Result;
	}

	const FDifficultyEnumItemOverrides& GetDifficultyOverrides(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		switch (Tuning.DifficultyLevel)
		{
		case ERTSGameDifficulty::NewToRTS:
			return Tuning.DifficultyEnumOverrides.NewToRTS;
		case ERTSGameDifficulty::Hard:
			return Tuning.DifficultyEnumOverrides.Hard;
		case ERTSGameDifficulty::Brutal:
			return Tuning.DifficultyEnumOverrides.Brutal;
		case ERTSGameDifficulty::Ironman:
			return Tuning.DifficultyEnumOverrides.Ironman;
		case ERTSGameDifficulty::Normal:
		default:
			return Tuning.DifficultyEnumOverrides.Normal;
		}
	}

	TMap<EMapEnemyItem, int32> BuildRequiredEnemyItemCounts(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		TMap<EMapEnemyItem, int32> RequiredCounts = Tuning.BaseEnemyItemsByType;
		const FDifficultyEnumItemOverrides& Overrides = GetDifficultyOverrides(Tuning);
		for (const TPair<EMapEnemyItem, int32>& Pair : Overrides.ExtraEnemyItemsByType)
		{
			RequiredCounts.FindOrAdd(Pair.Key) += Pair.Value;
		}

		return RequiredCounts;
	}

	TMap<EMapNeutralObjectType, int32> BuildRequiredNeutralItemCounts(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		TMap<EMapNeutralObjectType, int32> RequiredCounts = Tuning.BaseNeutralItemsByType;
		const FDifficultyEnumItemOverrides& Overrides = GetDifficultyOverrides(Tuning);
		for (const TPair<EMapNeutralObjectType, int32>& Pair : Overrides.ExtraNeutralItemsByType)
		{
			RequiredCounts.FindOrAdd(Pair.Key) += Pair.Value;
		}

		return RequiredCounts;
	}

	int32 GetRequiredEnemyItemCount(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		int32 TotalRequired = 0;
		for (const TPair<EMapEnemyItem, int32>& Pair : Tuning.BaseEnemyItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		const FDifficultyEnumItemOverrides& Overrides = GetDifficultyOverrides(Tuning);
		for (const TPair<EMapEnemyItem, int32>& Pair : Overrides.ExtraEnemyItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		return TotalRequired;
	}

	int32 GetRequiredNeutralItemCount(const FWorldCampaignCountDifficultyTuning& Tuning)
	{
		int32 TotalRequired = 0;
		for (const TPair<EMapNeutralObjectType, int32>& Pair : Tuning.BaseNeutralItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		const FDifficultyEnumItemOverrides& Overrides = GetDifficultyOverrides(Tuning);
		for (const TPair<EMapNeutralObjectType, int32>& Pair : Overrides.ExtraNeutralItemsByType)
		{
			TotalRequired += Pair.Value;
		}

		return TotalRequired;
	}

	void BuildBacktrackEscalationSteps(ECampaignGenerationStep FailedStep,
	                                   TArray<ECampaignGenerationStep>& OutSteps)
	{
		OutSteps.Reset();
		if (FailedStep == ECampaignGenerationStep::MissionsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::NeutralObjectsPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyWallPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::NeutralObjectsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyWallPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyObjectsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyWallPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyWallPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyHQPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::PlayerHQPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
		}
	}

	bool IsPointOnSegment(const FVector2D& SegmentStart, const FVector2D& SegmentEnd, const FVector2D& Point)
	{
		const float MinX = FMath::Min(SegmentStart.X, SegmentEnd.X) - SegmentIntersectionTolerance;
		const float MaxX = FMath::Max(SegmentStart.X, SegmentEnd.X) + SegmentIntersectionTolerance;
		const float MinY = FMath::Min(SegmentStart.Y, SegmentEnd.Y) - SegmentIntersectionTolerance;
		const float MaxY = FMath::Max(SegmentStart.Y, SegmentEnd.Y) + SegmentIntersectionTolerance;
		return Point.X >= MinX && Point.X <= MaxX && Point.Y >= MinY && Point.Y <= MaxY;
	}

	float CalculateOrientation(const FVector2D& PointA, const FVector2D& PointB, const FVector2D& PointC)
	{
		return (PointB.Y - PointA.Y) * (PointC.X - PointB.X) - (PointB.X - PointA.X) * (PointC.Y - PointB.Y);
	}

	bool DoSegmentsIntersect(const FVector2D& SegmentAStart, const FVector2D& SegmentAEnd,
	                         const FVector2D& SegmentBStart, const FVector2D& SegmentBEnd)
	{
		const float Orientation1 = CalculateOrientation(SegmentAStart, SegmentAEnd, SegmentBStart);
		const float Orientation2 = CalculateOrientation(SegmentAStart, SegmentAEnd, SegmentBEnd);
		const float Orientation3 = CalculateOrientation(SegmentBStart, SegmentBEnd, SegmentAStart);
		const float Orientation4 = CalculateOrientation(SegmentBStart, SegmentBEnd, SegmentAEnd);

		if (Orientation1 * Orientation2 < 0.f && Orientation3 * Orientation4 < 0.f)
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation1) && IsPointOnSegment(SegmentAStart, SegmentAEnd, SegmentBStart))
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation2) && IsPointOnSegment(SegmentAStart, SegmentAEnd, SegmentBEnd))
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation3) && IsPointOnSegment(SegmentBStart, SegmentBEnd, SegmentAStart))
		{
			return true;
		}

		if (FMath::IsNearlyZero(Orientation4) && IsPointOnSegment(SegmentBStart, SegmentBEnd, SegmentAEnd))
		{
			return true;
		}

		return false;
	}
}

AGeneratorWorldCampaign::AGeneratorWorldCampaign()
{
	PrimaryActorTick.bCanEverTick = false;
	M_ConnectionClass = AConnection::StaticClass();
	M_WorldCampaignDebugger = CreateDefaultSubobject<UWorldCampaignDebugger>(TEXT("WorldCampaignDebugger"));
	M_ExcludedMissionsFromMapPlacement.Add(EMapMission::FirstCampaignClearRoad);
	M_ExcludedMissionsFromMapPlacement.Add(EMapMission::SecondCampaignBuildBase);
	ApplyDebuggerSettingsToComponent();
}

void AGeneratorWorldCampaign::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyDebuggerSettingsToComponent();
}

void AGeneratorWorldCampaign::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	ApplyDebuggerSettingsToComponent();
}

#if WITH_EDITOR
void AGeneratorWorldCampaign::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyDebuggerSettingsToComponent();
}
#endif

void AGeneratorWorldCampaign::CreateConnectionsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::ConnectionsCreated))
	{
		RTSFunctionLibrary::ReportError(TEXT("CreateConnectionsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::ConnectionsCreated,
	                           &AGeneratorWorldCampaign::ExecuteCreateConnections);
}

void AGeneratorWorldCampaign::PlaceHQStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::PlayerHQPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceHQStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::PlayerHQPlaced, &AGeneratorWorldCampaign::ExecutePlaceHQ);
}

void AGeneratorWorldCampaign::PlaceEnemyHQStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::EnemyHQPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceEnemyHQStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::EnemyHQPlaced, &AGeneratorWorldCampaign::ExecutePlaceEnemyHQ);
}

void AGeneratorWorldCampaign::PlaceEnemyWallStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::EnemyWallPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceEnemyWallStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::EnemyWallPlaced,
	                           &AGeneratorWorldCampaign::ExecutePlaceEnemyWall);
}

void AGeneratorWorldCampaign::PlaceEnemyObjectsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::EnemyObjectsPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceEnemyObjectsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::EnemyObjectsPlaced,
	                           &AGeneratorWorldCampaign::ExecutePlaceEnemyObjects);
}

void AGeneratorWorldCampaign::PlaceNeutralObjectsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::NeutralObjectsPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceNeutralObjectsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::NeutralObjectsPlaced,
	                           &AGeneratorWorldCampaign::ExecutePlaceNeutralObjects);
}

void AGeneratorWorldCampaign::PlaceMissionsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::MissionsPlaced))
	{
		RTSFunctionLibrary::ReportError(TEXT("PlaceMissionsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::MissionsPlaced, &AGeneratorWorldCampaign::ExecutePlaceMissions);
}

void AGeneratorWorldCampaign::ExecuteAllSteps()
{
	if (M_GenerationStep != ECampaignGenerationStep::NotStarted)
	{
		EraseAllGeneration();
	}

	ExecuteAllStepsWithBacktracking();
}

void AGeneratorWorldCampaign::EraseAllGeneration()
{
	for (const FCampaignGenerationStepTransaction& Transaction : M_StepTransactions)
	{
		for (const TObjectPtr<AActor>& SpawnedActor : Transaction.SpawnedActors)
		{
			if (not IsValid(SpawnedActor))
			{
				continue;
			}

			SpawnedActor->Destroy();
		}

		for (const TObjectPtr<AConnection>& SpawnedConnection : Transaction.SpawnedConnections)
		{
			if (not IsValid(SpawnedConnection))
			{
				continue;
			}

			SpawnedConnection->Destroy();
		}
	}

	ClearExistingConnections();
	ClearPlacementState();
	ClearDerivedData();
	M_StepTransactions.Reset();
	M_StepAttemptIndices.Reset();
	M_TotalAttemptCount = 0;
	M_EnemyMicroPlacedCount = 0;
	M_MissionMicroPlacedCount = 0;
	M_GenerationStep = ECampaignGenerationStep::NotStarted;

	TArray<TObjectPtr<AAnchorPoint>> AnchorPoints;
	GatherAnchorPoints(AnchorPoints);
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->RemovePromotedWorldObject();
		AnchorPoint->ClearConnections();
	}

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		if (not GetIsValidCampaignDebugger())
		{
			return;
		}

		UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
		CampaignDebugger->ClearAllDebugState();
	}

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (not GetIsValidCampaignDebugger())
		{
			return;
		}

		UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
		CampaignDebugger->Report_ResetPlacementReport();
	}
}

void AGeneratorWorldCampaign::DebugDrawAllConnections() const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const TArray<TObjectPtr<AConnection>>* ConnectionsToDraw = &M_GeneratedConnections;
	if (ConnectionsToDraw->Num() == 0)
	{
		ConnectionsToDraw = &M_PlacementState.CachedConnections;
	}

	if (ConnectionsToDraw->Num() == 0)
	{
		return;
	}

	const FVector HeightOffset(0.f, 0.f, M_DebugConnectionDrawHeightOffset);
	const FColor BaseConnectionColor = FColor::Cyan;
	const FColor ThreeWayConnectionColor = FColor::Yellow;

	for (const TObjectPtr<AConnection>& Connection : *ConnectionsToDraw)
	{
		DrawDebugConnectionForActor(World, Connection, HeightOffset, BaseConnectionColor, ThreeWayConnectionColor);
	}
}

void AGeneratorWorldCampaign::ShowPlacementReport()
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (not GetIsValidCampaignDebugger())
		{
			return;
		}

		UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
		CampaignDebugger->Report_PrintPlacementReport(*this);
	}
}

void AGeneratorWorldCampaign::ApplyDebuggerSettingsToComponent()
{
	if (not GetIsValidCampaignDebugger())
	{
		return;
	}

	M_WorldCampaignDebugger->ApplySettingsFromGenerator(*this);
}

bool AGeneratorWorldCampaign::GetIsValidPlayerHQAnchor() const
{
	if (not IsValid(M_PlacementState.PlayerHQAnchor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlacementState.PlayerHQAnchor"),
		                                                      TEXT("AGeneratorWorldCampaign::GetIsValidPlayerHQAnchor"),
		                                                      this);
		return false;
	}

	return true;
}

bool AGeneratorWorldCampaign::GetIsValidEnemyHQAnchor() const
{
	if (not IsValid(M_PlacementState.EnemyHQAnchor))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_PlacementState.EnemyHQAnchor"),
		                                                      TEXT("AGeneratorWorldCampaign::GetIsValidEnemyHQAnchor"),
		                                                      this);
		return false;
	}

	return true;
}

bool AGeneratorWorldCampaign::GetIsValidCampaignDebugger() const
{
	if (not IsValid(M_WorldCampaignDebugger))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_WorldCampaignDebugger"),
		                                                      TEXT(
			                                                      "AGeneratorWorldCampaign::GetIsValidCampaignDebugger"),
		                                                      this);
		return false;
	}

	return true;
}

UWorldCampaignDebugger* AGeneratorWorldCampaign::GetCampaignDebugger() const
{
	return M_WorldCampaignDebugger;
}

bool AGeneratorWorldCampaign::ExecuteStepWithTransaction(ECampaignGenerationStep CompletedStep,
                                                         bool (AGeneratorWorldCampaign::*StepFunction)(
	                                                         FCampaignGenerationStepTransaction&))
{
	if (not CanExecuteStep(CompletedStep))
	{
		return false;
	}

	if (CompletedStep == ECampaignGenerationStep::EnemyObjectsPlaced
		|| CompletedStep == ECampaignGenerationStep::MissionsPlaced)
	{
		return ExecuteStepWithMicroTransactions(CompletedStep, StepFunction);
	}

	FCampaignGenerationStepTransaction Transaction;
	Transaction.CompletedStep = CompletedStep;
	Transaction.PreviousPlacementState = M_PlacementState;
	Transaction.PreviousDerivedData = M_DerivedData;

	if (not(this->*StepFunction)(Transaction))
	{
		return false;
	}

	M_StepTransactions.Add(Transaction);
	M_GenerationStep = CompletedStep;
	ResetStepAttemptsFrom(CompletedStep);
	return true;
}

bool AGeneratorWorldCampaign::ExecuteStepWithMicroTransactions(ECampaignGenerationStep CompletedStep,
                                                               bool (AGeneratorWorldCampaign::*StepFunction)(
	                                                               FCampaignGenerationStepTransaction&))
{
	FCampaignGenerationStepTransaction UnusedTransaction;
	if (not(this->*StepFunction)(UnusedTransaction))
	{
		return false;
	}

	M_GenerationStep = CompletedStep;
	ResetStepAttemptsFrom(CompletedStep);
	return true;
}

bool AGeneratorWorldCampaign::ExecuteCreateConnections(FCampaignGenerationStepTransaction& OutTransaction)
{
	// NOTE: If this step spawns non-connection actors, add them to OutTransaction.SpawnedActors for rollback.
	if (not ValidateGenerationRules())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	TArray<TObjectPtr<AAnchorPoint>> AnchorPoints;
	if (not PrepareAnchorsForConnectionGeneration(AnchorPoints))
	{
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::ConnectionsCreated);
	const int32 SeedOffset = AttemptIndex * AttemptSeedMultiplier;
	FRandomStream RandomStream(M_CountAndDifficultyTuning.Seed + SeedOffset);
	TMap<TObjectPtr<AAnchorPoint>, int32> DesiredConnections;
	GenerateConnectionsForAnchors(AnchorPoints, RandomStream, DesiredConnections);
	CacheGeneratedState(AnchorPoints);
	OutTransaction.SpawnedConnections = M_GeneratedConnections;
	return true;
}

bool AGeneratorWorldCampaign::PrepareAnchorsForConnectionGeneration(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints)
{
	ClearExistingConnections();
	ClearPlacementState();
	ClearDerivedData();

	GatherAnchorPoints(OutAnchorPoints);
	if (OutAnchorPoints.Num() < 2)
	{
		return false;
	}

	Algo::Sort(OutAnchorPoints, [](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
	{
		if (not IsValid(Left))
		{
			return false;
		}

		if (not IsValid(Right))
		{
			return true;
		}

		return AAnchorPoint::IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
	});

	const UWorldCampaignSettings* CampaignSettings = UWorldCampaignSettings::Get();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : OutAnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->InitializeCampaignSettings(CampaignSettings);
		AnchorPoint->RemovePromotedWorldObject();
		AnchorPoint->ClearConnections();
	}

	return true;
}

void AGeneratorWorldCampaign::GenerateConnectionsForAnchors(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
                                                            FRandomStream& RandomStream,
                                                            TMap<TObjectPtr<AAnchorPoint>, int32>&
                                                            OutDesiredConnections)
{
	AssignDesiredConnections(AnchorPoints, RandomStream, OutDesiredConnections);

	TArray<TObjectPtr<AAnchorPoint>> ShuffledAnchors = AnchorPoints;
	CampaignGenerationHelper::DeterministicShuffle(ShuffledAnchors, RandomStream);

	TArray<FConnectionSegment> ExistingSegments;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : ShuffledAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if constexpr (DeveloperSettings::Debugging::GCampaignConnectionGeneration_Compile_DebugSymbols)
			{
				DebugNotifyAnchorProcessing(AnchorPoint, TEXT("Processing"), FColor::Cyan);
			}
		}

		GeneratePhasePreferredConnections(AnchorPoint, AnchorPoints, OutDesiredConnections, ExistingSegments);
		GeneratePhaseExtendedConnections(AnchorPoint, AnchorPoints, ExistingSegments);
		GeneratePhaseThreeWayConnections(AnchorPoint, ExistingSegments);
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->SortNeighborsByKey();
	}
}

void AGeneratorWorldCampaign::CacheGeneratedState(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints)
{
	M_PlacementState.SeedUsed = M_CountAndDifficultyTuning.Seed;
	M_PlacementState.CachedAnchors.Reset();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		M_PlacementState.CachedAnchors.Add(AnchorPoint);
	}

	M_PlacementState.CachedConnections.Reset();
	for (const TObjectPtr<AConnection>& Connection : M_GeneratedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		M_PlacementState.CachedConnections.Add(Connection);
	}

	CacheAnchorConnectionDegrees();
	BuildChokepointScoresCache(nullptr);
}

bool AGeneratorWorldCampaign::ExecutePlaceHQ(FCampaignGenerationStepTransaction& OutTransaction)
{
	// NOTE: If this step spawns actors, add them to OutTransaction.SpawnedActors for rollback.

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no cached anchors available."));
		return false;
	}

	constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
	const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource = M_PlayerHQPlacementRules.AnchorCandidates.Num() > 0
		                                                          ? M_PlayerHQPlacementRules.AnchorCandidates
		                                                          : M_PlacementState.CachedAnchors;

	TArray<TObjectPtr<AAnchorPoint>> Candidates;
	if (not BuildHQAnchorCandidates(CandidateSource, M_PlayerHQPlacementRules.MinAnchorDegreeForHQ,
	                                MaxAnchorDegreeUnlimited, nullptr, Candidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no valid anchor candidates found."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::PlayerHQPlaced);
	TArray<TObjectPtr<AAnchorPoint>> NeighborhoodCandidates;
	NeighborhoodCandidates.Reserve(Candidates.Num());
	for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : Candidates)
	{
		if (not IsValid(CandidateAnchor))
		{
			continue;
		}

		TMap<FGuid, int32> HopDistances;
		CampaignGenerationHelper::BuildHopDistanceCache(CandidateAnchor, HopDistances);

		int32 NearbyAnchorCount = 0;
		for (const TPair<FGuid, int32>& Pair : HopDistances)
		{
			if (Pair.Value <= 0)
			{
				continue;
			}

			if (Pair.Value > M_PlayerHQPlacementRules.MinAnchorsWithinHopsRange)
			{
				continue;
			}

			NearbyAnchorCount++;
		}

		if (NearbyAnchorCount < M_PlayerHQPlacementRules.MinAnchorsWithinHops)
		{
			continue;
		}

		NeighborhoodCandidates.Add(CandidateAnchor);
	}

	if (NeighborhoodCandidates.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no candidates met neighborhood rules."));
		return false;
	}

	AAnchorPoint* AnchorPoint = SelectAnchorCandidateByAttempt(NeighborhoodCandidates, AttemptIndex);
	if (not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: selected anchor was invalid."));
		return false;
	}

	M_PlacementState.PlayerHQAnchor = AnchorPoint;
	M_PlacementState.PlayerHQAnchorKey = AnchorPoint->GetAnchorKey();
	CampaignGenerationHelper::BuildHopDistanceCache(AnchorPoint, M_DerivedData.PlayerHQHopDistancesByAnchorKey);
	BuildChokepointScoresCache(AnchorPoint);

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DebugNotifyAnchorPicked(AnchorPoint, TEXT("Player HQ"), FColor::Green);
	}

	if (IsValid(AnchorPoint))
	{
		AWorldMapObject* SpawnedObject = AnchorPoint->OnPlayerItemPromotion(EMapPlayerItem::PlayerHQ,
		                                                                    ECampaignGenerationStep::PlayerHQPlaced);
		if (IsValid(SpawnedObject))
		{
			OutTransaction.SpawnedActors.Add(SpawnedObject);
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceEnemyHQ(FCampaignGenerationStepTransaction& OutTransaction)
{
	// NOTE: If this step spawns actors, add them to OutTransaction.SpawnedActors for rollback.

	if (not GetIsValidPlayerHQAnchor())
	{
		return false;
	}

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: no cached anchors available."));
		return false;
	}

	constexpr int32 MinAnchorDegreeFloor = 0;
	const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource = M_EnemyHQPlacementRules.AnchorCandidates.Num() > 0
		                                                          ? M_EnemyHQPlacementRules.AnchorCandidates
		                                                          : M_PlacementState.CachedAnchors;

	TArray<TObjectPtr<AAnchorPoint>> Candidates;
	const int32 MinAnchorDegree = FMath::Max(MinAnchorDegreeFloor, M_EnemyHQPlacementRules.MinAnchorDegree);
	const int32 MaxAnchorDegree = FMath::Max(MinAnchorDegree, M_EnemyHQPlacementRules.MaxAnchorDegree);
	if (not BuildHQAnchorCandidates(CandidateSource, MinAnchorDegree, MaxAnchorDegree,
	                                M_PlacementState.PlayerHQAnchor.Get(), Candidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: no valid anchor candidates found."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyHQPlaced);
	if (M_EnemyHQPlacementRules.AnchorDegreePreference != ETopologySearchStrategy::NotSet)
	{
		const bool bPreferMax = M_EnemyHQPlacementRules.AnchorDegreePreference == ETopologySearchStrategy::PreferMax;
		Algo::Sort(Candidates, [this, bPreferMax](const TObjectPtr<AAnchorPoint>& Left,
		                                          const TObjectPtr<AAnchorPoint>& Right)
		{
			if (not IsValid(Left))
			{
				return false;
			}

			if (not IsValid(Right))
			{
				return true;
			}

			const int32 LeftDegree = GetAnchorConnectionDegree(Left);
			const int32 RightDegree = GetAnchorConnectionDegree(Right);
			if (LeftDegree != RightDegree)
			{
				return bPreferMax ? LeftDegree > RightDegree : LeftDegree < RightDegree;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
		});
	}

	AAnchorPoint* AnchorPoint = SelectAnchorCandidateByAttempt(Candidates, AttemptIndex);
	if (not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: selected anchor was invalid."));
		return false;
	}

	M_PlacementState.EnemyHQAnchor = AnchorPoint;
	M_PlacementState.EnemyHQAnchorKey = AnchorPoint->GetAnchorKey();
	CampaignGenerationHelper::BuildHopDistanceCache(AnchorPoint, M_DerivedData.EnemyHQHopDistancesByAnchorKey);

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DebugNotifyAnchorPicked(AnchorPoint, TEXT("Enemy HQ"), FColor::Red);
	}

	if (IsValid(AnchorPoint))
	{
		AWorldMapObject* SpawnedObject = AnchorPoint->OnEnemyItemPromotion(EMapEnemyItem::EnemyHQ,
		                                                                   ECampaignGenerationStep::EnemyHQPlaced);
		if (IsValid(SpawnedObject))
		{
			OutTransaction.SpawnedActors.Add(SpawnedObject);
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceEnemyWall(FCampaignGenerationStepTransaction& OutTransaction)
{
	// NOTE: If this step spawns actors, add them to OutTransaction.SpawnedActors for rollback.

	if (not GetIsValidEnemyHQAnchor())
	{
		return false;
	}

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy wall placement failed: no cached anchors available."));
		return false;
	}

	if (M_EnemyWallPlacementRules.AnchorCandidates.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy wall placement failed: no anchor candidates provided."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyWallPlaced);
	FPlacementCandidate SelectedCandidate;
	if (not TrySelectEnemyWallPlacementCandidate(*this, M_EnemyWallPlacementRules, M_PlacementState, M_DerivedData,
	                                             AttemptIndex, SelectedCandidate))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy wall placement failed: no valid anchor candidates found."));
		return false;
	}

	M_PlacementState.EnemyItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, EMapEnemyItem::EnemyWall);
	const int32 ExistingCount = M_DerivedData.EnemyItemPlacedCounts.FindRef(EMapEnemyItem::EnemyWall);
	M_DerivedData.EnemyItemPlacedCounts.Add(EMapEnemyItem::EnemyWall, ExistingCount + 1);

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DebugNotifyAnchorPicked(SelectedCandidate.AnchorPoint, TEXT("Enemy Wall"), FColor::Orange);
	}

	AWorldMapObject* SpawnedObject = SelectedCandidate.AnchorPoint->OnEnemyItemPromotion(
		EMapEnemyItem::EnemyWall,
		ECampaignGenerationStep::EnemyWallPlaced);
	if (not IsValid(SpawnedObject))
	{
		return false;
	}

	OutTransaction.SpawnedActors.Add(SpawnedObject);
	return true;
}

bool AGeneratorWorldCampaign::ValidateEnemyObjectPlacementPrerequisites() const
{
	if (not GetIsValidEnemyHQAnchor())
	{
		return false;
	}

	if (not GetIsValidPlayerHQAnchor())
	{
		return false;
	}

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy object placement failed: no cached anchors available."));
		return false;
	}

	if (M_DerivedData.EnemyHQHopDistancesByAnchorKey.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy object placement failed: enemy HQ hop cache is empty."));
		return false;
	}

	return true;
}

bool AGeneratorWorldCampaign::ValidateMissionPlacementPrerequisites() const
{
	if (not GetIsValidPlayerHQAnchor())
	{
		return false;
	}

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Mission placement failed: no cached anchors available."));
		return false;
	}

	if (M_DerivedData.PlayerHQHopDistancesByAnchorKey.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Mission placement failed: player HQ hop cache is empty."));
		return false;
	}

	return true;
}

bool AGeneratorWorldCampaign::GetIsMissionExcludedFromPlacement(const EMapMission MissionType) const
{
	if (M_ExcludedMissionsFromMapPlacement.Num() == 0)
	{
		return false;
	}

	return M_ExcludedMissionsFromMapPlacement.Contains(MissionType);
}

TArray<EMapMission> AGeneratorWorldCampaign::BuildMissionPlacementPlanFiltered() const
{
	TArray<EMapMission> PlacementPlan;
	const TArray<EMapMission> MissionTypes = GetSortedMissionTypes(M_MissionPlacementRules.RulesByMission);
	for (const EMapMission MissionType : MissionTypes)
	{
		if (GetIsMissionExcludedFromPlacement(MissionType))
		{
			continue;
		}

		PlacementPlan.Add(MissionType);
	}

	return PlacementPlan;
}

void AGeneratorWorldCampaign::RollbackMicroPlacementAndDestroyActors(
	FCampaignGenerationStepTransaction& InOutTransaction)
{
	TSet<FGuid> AnchorKeysToClear;
	for (const TObjectPtr<AActor>& SpawnedActor : InOutTransaction.SpawnedActors)
	{
		if (not IsValid(SpawnedActor))
		{
			continue;
		}

		SpawnedActor->Destroy();
	}
	InOutTransaction.SpawnedActors.Reset();

	AppendAnchorKeyIfValid(InOutTransaction.MicroAnchorKey, AnchorKeysToClear);
	for (const FGuid& AnchorKey : InOutTransaction.MicroAdditionalAnchorKeys)
	{
		AppendAnchorKeyIfValid(AnchorKey, AnchorKeysToClear);
	}

	M_PlacementState = InOutTransaction.PreviousPlacementState;
	M_DerivedData = InOutTransaction.PreviousDerivedData;
	RemovePromotedWorldObjectsForAnchorKeys(M_PlacementState.CachedAnchors, AnchorKeysToClear);
}

bool AGeneratorWorldCampaign::TrySelectSingleEnemyPlacement(EMapEnemyItem EnemyTypeToPlace,
                                                            FWorldCampaignPlacementState& WorkingPlacementState,
                                                            FWorldCampaignDerivedData& WorkingDerivedData,
                                                            TPair<TObjectPtr<AAnchorPoint>, EMapEnemyItem>&
                                                            OutPromotion)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_OnAttemptEnemy(EnemyTypeToPlace);
		}
	}

	constexpr int32 SinglePlacementCount = 1;
	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyObjectsPlaced);
	const FRuleRelaxationState RelaxationState = GetRelaxationState(
		GetFailurePolicyForStep(ECampaignGenerationStep::EnemyObjectsPlaced),
		AttemptIndex);
	const int32 SafeZoneMaxHops = RelaxationState.bRelaxDistance ? 0 : M_PlayerHQPlacementRules.SafeZoneMaxHops;

	const TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(WorkingPlacementState.CachedAnchors);
	const FEnemyItemRuleset* RulesetPtr = M_EnemyPlacementRules.RulesByEnemyItem.Find(EnemyTypeToPlace);
	const FEnemyItemRuleset Ruleset = RulesetPtr ? *RulesetPtr : FEnemyItemRuleset();
	TArray<TPair<TObjectPtr<AAnchorPoint>, EMapEnemyItem>> Promotions;

	if (not TryPlaceEnemyItemsForType(*this, EnemyTypeToPlace, SinglePlacementCount, Ruleset, AttemptIndex,
	                                  RelaxationState, SafeZoneMaxHops, WorkingPlacementState, WorkingDerivedData,
	                                  AnchorLookup, Promotions))
	{
		return false;
	}

	if (Promotions.Num() != SinglePlacementCount)
	{
		return false;
	}

	OutPromotion = Promotions[0];
	return IsValid(OutPromotion.Key);
}

bool AGeneratorWorldCampaign::TrySelectSingleMissionPlacement(EMapMission MissionTypeToPlace,
                                                              int32 MicroIndexWithinParent,
                                                              FWorldCampaignPlacementState& WorkingPlacementState,
                                                              FWorldCampaignDerivedData& WorkingDerivedData,
                                                              TPair<TObjectPtr<AAnchorPoint>, EMapMission>&
                                                              OutPromotion,
                                                              TArray<TPair<TObjectPtr<AAnchorPoint>,
                                                                           EMapNeutralObjectType>>&
                                                              OutCompanionPromotions)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_OnAttemptMission(MissionTypeToPlace);
		}
	}

	constexpr int32 SinglePlacementCount = 1;
	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::MissionsPlaced);
	const FRuleRelaxationState RelaxationState = GetRelaxationState(
		GetFailurePolicyForStep(ECampaignGenerationStep::MissionsPlaced),
		AttemptIndex);

	const TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(WorkingPlacementState.CachedAnchors);
	TArray<TPair<TObjectPtr<AAnchorPoint>, EMapMission>> Promotions;

	if (not TryPlaceMissionForType(*this, MissionTypeToPlace, MicroIndexWithinParent, AttemptIndex, RelaxationState,
	                               M_MissionPlacementRules, M_NeutralItemPlacementRules, WorkingPlacementState,
	                               WorkingDerivedData, AnchorLookup, Promotions, OutCompanionPromotions))
	{
		return false;
	}

	if (Promotions.Num() != SinglePlacementCount)
	{
		return false;
	}

	OutPromotion = Promotions[0];
	return IsValid(OutPromotion.Key);
}

bool AGeneratorWorldCampaign::TryFinalizeMissionMicroPlacement(
	const TPair<TObjectPtr<AAnchorPoint>, EMapMission>& Promotion,
	const TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& CompanionPromotions,
	FCampaignGenerationStepTransaction& OutMicroTransaction)
{
	for (const TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>& CompanionPromotion : CompanionPromotions)
	{
		if (not IsValid(CompanionPromotion.Key))
		{
			RollbackMicroPlacementAndDestroyActors(OutMicroTransaction);
			return false;
		}

		const FGuid CompanionKey = CompanionPromotion.Key->GetAnchorKey();
		if (CompanionKey != OutMicroTransaction.MicroAnchorKey)
		{
			OutMicroTransaction.MicroAdditionalAnchorKeys.Add(CompanionKey);
		}
	}

	AWorldMapObject* SpawnedMissionObject = Promotion.Key->OnMissionPromotion(
		Promotion.Value,
		ECampaignGenerationStep::MissionsPlaced);
	if (not IsValid(SpawnedMissionObject))
	{
		RollbackMicroPlacementAndDestroyActors(OutMicroTransaction);
		return false;
	}

	OutMicroTransaction.SpawnedActors.Add(SpawnedMissionObject);
	for (const TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>& CompanionPromotion : CompanionPromotions)
	{
		if (not IsValid(CompanionPromotion.Key))
		{
			RollbackMicroPlacementAndDestroyActors(OutMicroTransaction);
			return false;
		}

		AWorldMapObject* SpawnedCompanionObject = CompanionPromotion.Key->OnNeutralItemPromotion(
			CompanionPromotion.Value,
			ECampaignGenerationStep::MissionsPlaced);
		if (not IsValid(SpawnedCompanionObject))
		{
			RollbackMicroPlacementAndDestroyActors(OutMicroTransaction);
			return false;
		}

		OutMicroTransaction.SpawnedActors.Add(SpawnedCompanionObject);
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceSingleEnemyObject(EMapEnemyItem EnemyTypeToPlace,
                                                            int32 PlacementOrdinalWithinType,
                                                            int32 MicroIndexWithinParent,
                                                            FCampaignGenerationStepTransaction& OutMicroTransaction)
{
	// NOTE: If this step spawns actors, add them to OutMicroTransaction.SpawnedActors for rollback.
	(void)PlacementOrdinalWithinType;

	if (not ValidateEnemyObjectPlacementPrerequisites())
	{
		return false;
	}

	FWorldCampaignPlacementState WorkingPlacementState = M_PlacementState;
	FWorldCampaignDerivedData WorkingDerivedData = M_DerivedData;
	TPair<TObjectPtr<AAnchorPoint>, EMapEnemyItem> Promotion;

	OutMicroTransaction.PreviousPlacementState = M_PlacementState;
	OutMicroTransaction.PreviousDerivedData = M_DerivedData;
	if (not TrySelectSingleEnemyPlacement(EnemyTypeToPlace, WorkingPlacementState, WorkingDerivedData, Promotion))
	{
		return false;
	}

	if (not IsValid(Promotion.Key))
	{
		return false;
	}

	OutMicroTransaction.CompletedStep = ECampaignGenerationStep::EnemyObjectsPlaced;
	OutMicroTransaction.bIsMicroTransaction = true;
	OutMicroTransaction.MicroParentStep = ECampaignGenerationStep::EnemyObjectsPlaced;
	OutMicroTransaction.MicroAnchorKey = Promotion.Key->GetAnchorKey();
	OutMicroTransaction.MicroItemType = EMapItemType::EnemyItem;
	OutMicroTransaction.MicroEnemyItemType = EnemyTypeToPlace;
	OutMicroTransaction.MicroIndexWithinParent = MicroIndexWithinParent;

	M_PlacementState = WorkingPlacementState;
	M_DerivedData = WorkingDerivedData;

	AWorldMapObject* SpawnedObject = Promotion.Key->OnEnemyItemPromotion(
		Promotion.Value,
		ECampaignGenerationStep::EnemyObjectsPlaced);
	if (not IsValid(SpawnedObject))
	{
		RollbackMicroPlacementAndDestroyActors(OutMicroTransaction);
		return false;
	}

	OutMicroTransaction.SpawnedActors.Add(SpawnedObject);
	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceEnemyObjects(FCampaignGenerationStepTransaction& OutTransaction)
{
	// NOTE: If this step spawns actors, add them to OutTransaction.SpawnedActors for rollback.
	(void)OutTransaction;
	const int32 RequiredEnemyItems = GetRequiredEnemyItemCount(M_CountAndDifficultyTuning);
	if (RequiredEnemyItems <= NoRequiredItems)
	{
		return true;
	}

	const TMap<EMapEnemyItem, int32> RequiredEnemyCounts = BuildRequiredEnemyItemCounts(M_CountAndDifficultyTuning);
	const TArray<EMapEnemyItem> EnemyPlacementPlan = BuildEnemyPlacementPlan(RequiredEnemyCounts);
	if (EnemyPlacementPlan.Num() == 0)
	{
		return true;
	}

	UpdateMicroPlacementProgressFromTransactions();
	const int32 AlreadyPlacedCount = FMath::Clamp(M_EnemyMicroPlacedCount, 0, EnemyPlacementPlan.Num());
	for (int32 MicroIndex = AlreadyPlacedCount; MicroIndex < EnemyPlacementPlan.Num(); MicroIndex++)
	{
		const EMapEnemyItem EnemyTypeToPlace = EnemyPlacementPlan[MicroIndex];
		const int32 PlacementOrdinalWithinType = M_DerivedData.EnemyItemPlacedCounts.FindRef(EnemyTypeToPlace);
		FCampaignGenerationStepTransaction MicroTransaction;
		if (not ExecutePlaceSingleEnemyObject(EnemyTypeToPlace, PlacementOrdinalWithinType, MicroIndex,
		                                      MicroTransaction))
		{
			return false;
		}

		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
		{
			if (GetIsValidCampaignDebugger())
			{
				UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
				CampaignDebugger->Report_OnPlacedEnemy(EnemyTypeToPlace);
			}
		}

		M_StepTransactions.Add(MicroTransaction);
		M_EnemyMicroPlacedCount = MicroIndex + 1;
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceNeutralObjects(FCampaignGenerationStepTransaction& OutTransaction)
{
	// NOTE: If this step spawns actors, add them to OutTransaction.SpawnedActors for rollback.

	const int32 RequiredNeutralItems = GetRequiredNeutralItemCount(M_CountAndDifficultyTuning);
	if (RequiredNeutralItems <= NoRequiredItems)
	{
		return true;
	}

	if (not GetIsValidPlayerHQAnchor())
	{
		return false;
	}

	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Neutral object placement failed: no cached anchors available."));
		return false;
	}

	if (M_DerivedData.PlayerHQHopDistancesByAnchorKey.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Neutral object placement failed: player HQ hop cache is empty."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::NeutralObjectsPlaced);
	const FRuleRelaxationState RelaxationState = GetRelaxationState(
		GetFailurePolicyForStep(ECampaignGenerationStep::NeutralObjectsPlaced),
		AttemptIndex);

	FWorldCampaignPlacementState WorkingPlacementState = M_PlacementState;
	FWorldCampaignDerivedData WorkingDerivedData = M_DerivedData;
	const TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(WorkingPlacementState.CachedAnchors);
	const TMap<EMapNeutralObjectType, int32> RequiredNeutralCounts = BuildRequiredNeutralItemCounts(
		M_CountAndDifficultyTuning);
	const TArray<EMapNeutralObjectType> NeutralTypes = GetSortedNeutralTypes(RequiredNeutralCounts);
	TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>> Promotions;

	for (const EMapNeutralObjectType NeutralType : NeutralTypes)
	{
		const int32* RequiredCountPtr = RequiredNeutralCounts.Find(NeutralType);
		const int32 RequiredCount = RequiredCountPtr ? *RequiredCountPtr : 0;
		if (RequiredCount <= 0)
		{
			continue;
		}

		if (not TryPlaceNeutralItemsForType(NeutralType, RequiredCount, AttemptIndex, RelaxationState,
		                                    M_NeutralItemPlacementRules, WorkingPlacementState, WorkingDerivedData,
		                                    AnchorLookup, Promotions, *this))
		{
			return false;
		}
	}

	M_PlacementState = WorkingPlacementState;
	M_DerivedData = WorkingDerivedData;
	for (const TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>& Promotion : Promotions)
	{
		if (not IsValid(Promotion.Key))
		{
			continue;
		}

		AWorldMapObject* SpawnedObject = Promotion.Key->OnNeutralItemPromotion(
			Promotion.Value,
			ECampaignGenerationStep::NeutralObjectsPlaced);
		if (IsValid(SpawnedObject))
		{
			OutTransaction.SpawnedActors.Add(SpawnedObject);
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecutePlaceSingleMission(EMapMission MissionTypeToPlace, int32 MicroIndexWithinParent,
                                                        FCampaignGenerationStepTransaction& OutMicroTransaction)
{
	// NOTE: If this step spawns actors, add them to OutMicroTransaction.SpawnedActors for rollback.
	if (not ValidateMissionPlacementPrerequisites())
	{
		return false;
	}

	FWorldCampaignPlacementState WorkingPlacementState = M_PlacementState;
	FWorldCampaignDerivedData WorkingDerivedData = M_DerivedData;
	TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>> CompanionPromotions;
	TPair<TObjectPtr<AAnchorPoint>, EMapMission> Promotion;

	OutMicroTransaction.PreviousPlacementState = M_PlacementState;
	OutMicroTransaction.PreviousDerivedData = M_DerivedData;
	if (not TrySelectSingleMissionPlacement(MissionTypeToPlace, MicroIndexWithinParent, WorkingPlacementState,
	                                        WorkingDerivedData, Promotion, CompanionPromotions))
	{
		return false;
	}

	if (not IsValid(Promotion.Key))
	{
		return false;
	}

	OutMicroTransaction.CompletedStep = ECampaignGenerationStep::MissionsPlaced;
	OutMicroTransaction.bIsMicroTransaction = true;
	OutMicroTransaction.MicroParentStep = ECampaignGenerationStep::MissionsPlaced;
	OutMicroTransaction.MicroAnchorKey = Promotion.Key->GetAnchorKey();
	OutMicroTransaction.MicroItemType = EMapItemType::Mission;
	OutMicroTransaction.MicroMissionType = MissionTypeToPlace;
	OutMicroTransaction.MicroIndexWithinParent = MicroIndexWithinParent;

	M_PlacementState = WorkingPlacementState;
	M_DerivedData = WorkingDerivedData;
	return TryFinalizeMissionMicroPlacement(Promotion, CompanionPromotions, OutMicroTransaction);
}

bool AGeneratorWorldCampaign::ExecutePlaceMissions(FCampaignGenerationStepTransaction& OutTransaction)
{
	// NOTE: If this step spawns actors, add them to OutTransaction.SpawnedActors for rollback.
	(void)OutTransaction;
	const TArray<EMapMission> MissionPlacementPlan = BuildMissionPlacementPlanFiltered();
	const int32 RequiredMissions = MissionPlacementPlan.Num();
	if (RequiredMissions <= NoRequiredItems)
	{
		return true;
	}

	UpdateMicroPlacementProgressFromTransactions();
	const int32 AlreadyPlacedCount = FMath::Clamp(M_MissionMicroPlacedCount, 0, MissionPlacementPlan.Num());
	for (int32 MicroIndex = AlreadyPlacedCount; MicroIndex < MissionPlacementPlan.Num(); MicroIndex++)
	{
		const EMapMission MissionTypeToPlace = MissionPlacementPlan[MicroIndex];
		FCampaignGenerationStepTransaction MicroTransaction;
		if (not ExecutePlaceSingleMission(MissionTypeToPlace, MicroIndex, MicroTransaction))
		{
			return false;
		}

		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
		{
			if (GetIsValidCampaignDebugger())
			{
				UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
				CampaignDebugger->Report_OnPlacedMission(MissionTypeToPlace);
			}
		}

		M_StepTransactions.Add(MicroTransaction);
		M_MissionMicroPlacedCount = MicroIndex + 1;
	}

	return true;
}

bool AGeneratorWorldCampaign::ExecuteAllStepsWithBacktracking()
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_ResetPlacementReport();
		}
	}

	TArray<ECampaignGenerationStep> StepOrder;
	StepOrder.Add(ECampaignGenerationStep::ConnectionsCreated);
	StepOrder.Add(ECampaignGenerationStep::PlayerHQPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyHQPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyWallPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
	StepOrder.Add(ECampaignGenerationStep::NeutralObjectsPlaced);
	StepOrder.Add(ECampaignGenerationStep::MissionsPlaced);

	int32 StepIndex = 0;
	while (StepIndex < StepOrder.Num())
	{
		const ECampaignGenerationStep StepToExecute = StepOrder[StepIndex];
		bool (AGeneratorWorldCampaign::*StepFunction)(FCampaignGenerationStepTransaction&) = nullptr;
		switch (StepToExecute)
		{
		case ECampaignGenerationStep::ConnectionsCreated:
			StepFunction = &AGeneratorWorldCampaign::ExecuteCreateConnections;
			break;
		case ECampaignGenerationStep::PlayerHQPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceHQ;
			break;
		case ECampaignGenerationStep::EnemyHQPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyHQ;
			break;
		case ECampaignGenerationStep::EnemyWallPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyWall;
			break;
		case ECampaignGenerationStep::EnemyObjectsPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyObjects;
			break;
		case ECampaignGenerationStep::NeutralObjectsPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceNeutralObjects;
			break;
		case ECampaignGenerationStep::MissionsPlaced:
			StepFunction = &AGeneratorWorldCampaign::ExecutePlaceMissions;
			break;
		default:
			return false;
		}

		const bool bDidExecute = ExecuteStepWithTransaction(StepToExecute, StepFunction);
		if (bDidExecute)
		{
			StepIndex++;
			continue;
		}

		if (not HandleStepFailure(StepToExecute, StepIndex, StepOrder))
		{
			if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
			{
				if (GetIsValidCampaignDebugger())
				{
					UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
					CampaignDebugger->Report_PrintPlacementReport(*this);
				}
			}

			return false;
		}
	}

	M_GenerationStep = ECampaignGenerationStep::Finished;
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_PrintPlacementReport(*this);
		}
	}
	return true;
}

bool AGeneratorWorldCampaign::CanExecuteStep(ECampaignGenerationStep CompletedStep) const
{
	return M_GenerationStep == GetPrerequisiteStep(CompletedStep);
}

ECampaignGenerationStep AGeneratorWorldCampaign::GetPrerequisiteStep(ECampaignGenerationStep CompletedStep) const
{
	switch (CompletedStep)
	{
	case ECampaignGenerationStep::ConnectionsCreated:
		return ECampaignGenerationStep::NotStarted;
	case ECampaignGenerationStep::PlayerHQPlaced:
		return ECampaignGenerationStep::ConnectionsCreated;
	case ECampaignGenerationStep::EnemyHQPlaced:
		return ECampaignGenerationStep::PlayerHQPlaced;
	case ECampaignGenerationStep::EnemyWallPlaced:
		return ECampaignGenerationStep::EnemyHQPlaced;
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		return ECampaignGenerationStep::EnemyWallPlaced;
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		return ECampaignGenerationStep::EnemyObjectsPlaced;
	case ECampaignGenerationStep::MissionsPlaced:
		return ECampaignGenerationStep::NeutralObjectsPlaced;
	case ECampaignGenerationStep::Finished:
		return ECampaignGenerationStep::MissionsPlaced;
	case ECampaignGenerationStep::NotStarted:
	default:
		return ECampaignGenerationStep::NotStarted;
	}
}

ECampaignGenerationStep AGeneratorWorldCampaign::GetNextStep(ECampaignGenerationStep CurrentStep) const
{
	switch (CurrentStep)
	{
	case ECampaignGenerationStep::NotStarted:
		return ECampaignGenerationStep::ConnectionsCreated;
	case ECampaignGenerationStep::ConnectionsCreated:
		return ECampaignGenerationStep::PlayerHQPlaced;
	case ECampaignGenerationStep::PlayerHQPlaced:
		return ECampaignGenerationStep::EnemyHQPlaced;
	case ECampaignGenerationStep::EnemyHQPlaced:
		return ECampaignGenerationStep::EnemyWallPlaced;
	case ECampaignGenerationStep::EnemyWallPlaced:
		return ECampaignGenerationStep::EnemyObjectsPlaced;
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		return ECampaignGenerationStep::NeutralObjectsPlaced;
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		return ECampaignGenerationStep::MissionsPlaced;
	case ECampaignGenerationStep::MissionsPlaced:
		return ECampaignGenerationStep::Finished;
	default:
		return ECampaignGenerationStep::Finished;
	}
}

int32 AGeneratorWorldCampaign::GetStepAttemptIndex(ECampaignGenerationStep CompletedStep) const
{
	const int32* AttemptIndex = M_StepAttemptIndices.Find(CompletedStep);
	return AttemptIndex ? *AttemptIndex : 0;
}

void AGeneratorWorldCampaign::IncrementStepAttempt(ECampaignGenerationStep CompletedStep)
{
	const int32 CurrentAttempt = GetStepAttemptIndex(CompletedStep);
	M_StepAttemptIndices.Add(CompletedStep, CurrentAttempt + 1);
}

void AGeneratorWorldCampaign::ResetStepAttemptsFrom(ECampaignGenerationStep CompletedStep)
{
	ECampaignGenerationStep StepToReset = GetNextStep(CompletedStep);
	while (StepToReset != ECampaignGenerationStep::Finished)
	{
		M_StepAttemptIndices.Remove(StepToReset);
		StepToReset = GetNextStep(StepToReset);
	}
}

EPlacementFailurePolicy AGeneratorWorldCampaign::GetFailurePolicyForStep(ECampaignGenerationStep FailedStep) const
{
	const EPlacementFailurePolicy GlobalPolicy = M_PlacementFailurePolicy.GlobalPolicy;
	switch (FailedStep)
	{
	case ECampaignGenerationStep::ConnectionsCreated:
		return M_PlacementFailurePolicy.CreateConnectionsPolicy != EPlacementFailurePolicy::NotSet
			       ? M_PlacementFailurePolicy.CreateConnectionsPolicy
			       : GlobalPolicy;
	case ECampaignGenerationStep::PlayerHQPlaced:
		return M_PlacementFailurePolicy.PlaceHQPolicy != EPlacementFailurePolicy::NotSet
			       ? M_PlacementFailurePolicy.PlaceHQPolicy
			       : GlobalPolicy;
	case ECampaignGenerationStep::EnemyHQPlaced:
		return M_PlacementFailurePolicy.PlaceEnemyHQPolicy != EPlacementFailurePolicy::NotSet
			       ? M_PlacementFailurePolicy.PlaceEnemyHQPolicy
			       : GlobalPolicy;
	case ECampaignGenerationStep::EnemyWallPlaced:
		return M_PlacementFailurePolicy.PlaceEnemyWallPolicy != EPlacementFailurePolicy::NotSet
			       ? M_PlacementFailurePolicy.PlaceEnemyWallPolicy
			       : GlobalPolicy;
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		return M_PlacementFailurePolicy.PlaceEnemyObjectsPolicy != EPlacementFailurePolicy::NotSet
			       ? M_PlacementFailurePolicy.PlaceEnemyObjectsPolicy
			       : GlobalPolicy;
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		return M_PlacementFailurePolicy.PlaceNeutralObjectsPolicy != EPlacementFailurePolicy::NotSet
			       ? M_PlacementFailurePolicy.PlaceNeutralObjectsPolicy
			       : GlobalPolicy;
	case ECampaignGenerationStep::MissionsPlaced:
		return M_PlacementFailurePolicy.PlaceMissionsPolicy != EPlacementFailurePolicy::NotSet
			       ? M_PlacementFailurePolicy.PlaceMissionsPolicy
			       : GlobalPolicy;
	default:
		return GlobalPolicy;
	}
}

bool AGeneratorWorldCampaign::HandleStepFailure(ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
                                                const TArray<ECampaignGenerationStep>& StepOrder)
{
	IncrementStepAttempt(FailedStep);
	M_TotalAttemptCount++;

	if (GetStepAttemptIndex(FailedStep) > MaxStepAttempts)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: maximum step attempts exceeded."));
		return false;
	}

	if (M_TotalAttemptCount > MaxTotalAttempts)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: maximum total attempts exceeded."));
		return false;
	}

	const EPlacementFailurePolicy FailurePolicy = GetFailurePolicyForStep(FailedStep);
	if (FailurePolicy == EPlacementFailurePolicy::NotSet)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: no failure policy set for step."));
		return false;
	}

	const int32 TrailingMicroTransactions = CountTrailingMicroTransactionsForStep(FailedStep);
	if (TrailingMicroTransactions > 0
		&& (FailedStep == ECampaignGenerationStep::EnemyObjectsPlaced
			|| FailedStep == ECampaignGenerationStep::MissionsPlaced))
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
		{
			bM_Report_UndoContextActive = true;
			M_Report_CurrentFailureStep = FailedStep;
		}

		UndoLastTransaction();

		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
		{
			bM_Report_UndoContextActive = false;
			M_Report_CurrentFailureStep = ECampaignGenerationStep::NotStarted;
		}

		InOutStepIndex = StepOrder.IndexOfByKey(FailedStep);
		if (InOutStepIndex == INDEX_NONE)
		{
			InOutStepIndex = 0;
		}

		return true;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(FailedStep);
	if (FailurePolicy == EPlacementFailurePolicy::BreakDistanceRules_ThenBackTrack
		&& AttemptIndex <= MaxRelaxationAttempts)
	{
		InOutStepIndex = StepOrder.IndexOfByKey(FailedStep);
		if (InOutStepIndex == INDEX_NONE)
		{
			InOutStepIndex = 0;
		}

		return true;
	}

	TArray<ECampaignGenerationStep> EscalationSteps;
	BuildBacktrackEscalationSteps(FailedStep, EscalationSteps);
	const int32 MaxBacktrackSteps = EscalationSteps.Num();
	const int32 AdjustedAttemptIndex = FailurePolicy == EPlacementFailurePolicy::BreakDistanceRules_ThenBackTrack
		                                   ? AttemptIndex - MaxRelaxationAttempts
		                                   : AttemptIndex;
	const int32 TransactionsToUndo = FMath::Clamp(AdjustedAttemptIndex - 1, 0, MaxBacktrackSteps) + 1;
	const int32 AvailableTransactions = M_StepTransactions.Num();
	const int32 UndoCount = FMath::Min(TransactionsToUndo, AvailableTransactions);

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		bM_Report_UndoContextActive = true;
		M_Report_CurrentFailureStep = FailedStep;
	}

	for (int32 StepCount = 0; StepCount < UndoCount; StepCount++)
	{
		UndoLastTransaction();
	}

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		bM_Report_UndoContextActive = false;
		M_Report_CurrentFailureStep = ECampaignGenerationStep::NotStarted;
	}

	InOutStepIndex = StepOrder.IndexOfByKey(GetNextStep(M_GenerationStep));
	if (InOutStepIndex == INDEX_NONE)
	{
		InOutStepIndex = 0;
	}

	return true;
}

void AGeneratorWorldCampaign::UndoLastTransaction()
{
	if (M_StepTransactions.Num() == 0)
	{
		M_GenerationStep = ECampaignGenerationStep::NotStarted;
		return;
	}

	const FCampaignGenerationStepTransaction Transaction = M_StepTransactions.Pop();
	TSet<FGuid> AnchorKeysToClear;
	AppendAnchorKeysFromTransaction(Transaction, AnchorKeysToClear);
	for (const TObjectPtr<AActor>& SpawnedActor : Transaction.SpawnedActors)
	{
		if (not IsValid(SpawnedActor))
		{
			continue;
		}

		SpawnedActor->Destroy();
	}

	M_PlacementState = Transaction.PreviousPlacementState;
	M_DerivedData = Transaction.PreviousDerivedData;
	RemovePromotedWorldObjectsForAnchorKeys(M_PlacementState.CachedAnchors, AnchorKeysToClear);

	if (Transaction.CompletedStep == ECampaignGenerationStep::ConnectionsCreated)
	{
		UndoConnections(Transaction);
	}

	M_GenerationStep = GetPrerequisiteStep(Transaction.CompletedStep);
	UpdateMicroPlacementProgressFromTransactions();

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (bM_Report_UndoContextActive && GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_OnUndoneMicro(Transaction, M_Report_CurrentFailureStep);
		}
	}
}

void AGeneratorWorldCampaign::UndoConnections(const FCampaignGenerationStepTransaction& Transaction)
{
	for (const TObjectPtr<AConnection>& Connection : Transaction.SpawnedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		Connection->Destroy();
	}

	TArray<TObjectPtr<AAnchorPoint>> AnchorPoints;
	GatherAnchorPoints(AnchorPoints);
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		AnchorPoint->ClearConnections();
	}

	M_GeneratedConnections.Reset();
}

int32 AGeneratorWorldCampaign::CountTrailingMicroTransactionsForStep(ECampaignGenerationStep Step) const
{
	int32 TrailingCount = 0;
	for (int32 TransactionIndex = M_StepTransactions.Num() - 1; TransactionIndex >= 0; TransactionIndex--)
	{
		const FCampaignGenerationStepTransaction& Transaction = M_StepTransactions[TransactionIndex];
		if (Transaction.CompletedStep != Step || not Transaction.bIsMicroTransaction)
		{
			break;
		}

		TrailingCount++;
	}

	return TrailingCount;
}

bool AGeneratorWorldCampaign::IsStepEarlierThan(ECampaignGenerationStep StepToCheck,
                                                ECampaignGenerationStep ReferenceStep) const
{
	if (StepToCheck == ReferenceStep)
	{
		return false;
	}

	ECampaignGenerationStep CurrentStep = StepToCheck;
	while (CurrentStep != ECampaignGenerationStep::Finished)
	{
		CurrentStep = GetNextStep(CurrentStep);
		if (CurrentStep == ReferenceStep)
		{
			return true;
		}
	}

	return false;
}

int32 AGeneratorWorldCampaign::CountMicroTransactionsForStepSinceLastInvalidation(
	ECampaignGenerationStep MacroStep) const
{
	int32 MicroTransactionCount = 0;
	for (int32 TransactionIndex = M_StepTransactions.Num() - 1; TransactionIndex >= 0; TransactionIndex--)
	{
		const FCampaignGenerationStepTransaction& Transaction = M_StepTransactions[TransactionIndex];
		if (IsStepEarlierThan(Transaction.CompletedStep, MacroStep))
		{
			break;
		}

		if (Transaction.CompletedStep == MacroStep && Transaction.bIsMicroTransaction)
		{
			MicroTransactionCount++;
		}
	}

	return MicroTransactionCount;
}

void AGeneratorWorldCampaign::UpdateMicroPlacementProgressFromTransactions()
{
	M_EnemyMicroPlacedCount =
		CountMicroTransactionsForStepSinceLastInvalidation(ECampaignGenerationStep::EnemyObjectsPlaced);
	M_MissionMicroPlacedCount =
		CountMicroTransactionsForStepSinceLastInvalidation(ECampaignGenerationStep::MissionsPlaced);
}

void AGeneratorWorldCampaign::ClearPlacementState()
{
	M_PlacementState = FWorldCampaignPlacementState();
}

void AGeneratorWorldCampaign::ClearDerivedData()
{
	M_DerivedData = FWorldCampaignDerivedData();
}

void AGeneratorWorldCampaign::CacheAnchorConnectionDegrees()
{
	M_DerivedData.AnchorConnectionDegreesByAnchorKey.Reset();
	for (const TWeakObjectPtr<AAnchorPoint>& AnchorPointWeak : M_PlacementState.CachedAnchors)
	{
		AAnchorPoint* AnchorPoint = AnchorPointWeak.Get();
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		M_DerivedData.AnchorConnectionDegreesByAnchorKey.Add(AnchorPoint->GetAnchorKey(),
		                                                     AnchorPoint->GetConnectionCount());
	}
}

void AGeneratorWorldCampaign::BuildChokepointScoresCache(const AAnchorPoint* OptionalHQAnchor)
{
	M_DerivedData.ChokepointScoresByAnchorKey.Reset();

	const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors = M_PlacementState.CachedAnchors;
	if (CachedAnchors.Num() == 0)
	{
		return;
	}

	TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(CachedAnchors);
	if (AnchorLookup.Num() == 0)
	{
		return;
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		M_DerivedData.ChokepointScoresByAnchorKey.Add(AnchorPoint->GetAnchorKey(), 0.f);
	}

	if (not IsValid(OptionalHQAnchor))
	{
		constexpr int32 ChokepointDegreeOffset = 1;
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const int32 ConnectionDegree = AnchorPoint->GetConnectionCount();
			const float DegreeScore = static_cast<float>(FMath::Max(0, ConnectionDegree - ChokepointDegreeOffset));
			M_DerivedData.ChokepointScoresByAnchorKey.Add(AnchorPoint->GetAnchorKey(), DegreeScore);
		}

		TArray<TObjectPtr<AAnchorPoint>> ShuffledAnchors;
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			ShuffledAnchors.Add(AnchorPoint);
		}

		if (ShuffledAnchors.Num() < 2)
		{
			return;
		}

		FRandomStream RandomStream(M_PlacementState.SeedUsed + ChokepointSeedOffset);
		CampaignGenerationHelper::DeterministicShuffle(ShuffledAnchors, RandomStream);

		int32 SampleCount = 0;
		for (int32 StartIndex = 0; StartIndex < ShuffledAnchors.Num() - 1; StartIndex++)
		{
			if (SampleCount >= MaxChokepointPairSamples)
			{
				break;
			}

			const TObjectPtr<AAnchorPoint> StartAnchor = ShuffledAnchors[StartIndex];
			if (not IsValid(StartAnchor))
			{
				continue;
			}

			for (int32 TargetIndex = StartIndex + 1; TargetIndex < ShuffledAnchors.Num(); TargetIndex++)
			{
				if (SampleCount >= MaxChokepointPairSamples)
				{
					break;
				}

				const TObjectPtr<AAnchorPoint> TargetAnchor = ShuffledAnchors[TargetIndex];
				if (not IsValid(TargetAnchor))
				{
					continue;
				}

				TArray<FGuid> PathKeys;
				if (not BuildShortestPathKeys(StartAnchor, TargetAnchor, AnchorLookup, PathKeys))
				{
					continue;
				}

				AddChokepointPathContribution(PathKeys, M_DerivedData.ChokepointScoresByAnchorKey);
				SampleCount++;
			}
		}

		return;
	}

	for (const TObjectPtr<AAnchorPoint>& TargetAnchor : CachedAnchors)
	{
		if (not IsValid(TargetAnchor) || TargetAnchor == OptionalHQAnchor)
		{
			continue;
		}

		TArray<FGuid> PathKeys;
		if (not BuildShortestPathKeys(OptionalHQAnchor, TargetAnchor, AnchorLookup, PathKeys))
		{
			continue;
		}

		AddChokepointPathContribution(PathKeys, M_DerivedData.ChokepointScoresByAnchorKey);
	}
}

void AGeneratorWorldCampaign::DebugNotifyAnchorPicked(AAnchorPoint* AnchorPoint, const FString& Label,
                                                      const FColor& Color) const
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	if (not GetIsValidCampaignDebugger())
	{
		return;
	}

	UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();

	CampaignDebugger->DrawAcceptedAtAnchor(AnchorPoint, Label, Color);
}

bool AGeneratorWorldCampaign::ValidateGenerationRules() const
{
	if (ConnectionGenerationRules.MinConnections < 0)
	{
		return false;
	}

	if (ConnectionGenerationRules.MaxConnections < ConnectionGenerationRules.MinConnections)
	{
		return false;
	}

	if (ConnectionGenerationRules.MaxPreferredDistance < 0.f)
	{
		return false;
	}

	return ConnectionGenerationRules.MaxConnections >= 0;
}

int32 AGeneratorWorldCampaign::GetAnchorConnectionDegree(const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(AnchorPoint))
	{
		return 0;
	}

	const int32* CachedDegree = M_DerivedData.AnchorConnectionDegreesByAnchorKey.Find(AnchorPoint->GetAnchorKey());
	return CachedDegree ? *CachedDegree : AnchorPoint->GetConnectionCount();
}

bool AGeneratorWorldCampaign::IsAnchorCached(const AAnchorPoint* AnchorPoint) const
{
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	for (TWeakObjectPtr<AAnchorPoint> CachedAnchor : M_PlacementState.CachedAnchors)
	{
		if (CachedAnchor.Get() == AnchorPoint)
		{
			return true;
		}
	}

	return false;
}

bool AGeneratorWorldCampaign::BuildHQAnchorCandidates(const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
                                                      int32 MinDegree, int32 MaxDegree,
                                                      const AAnchorPoint* AnchorToExclude,
                                                      TArray<TObjectPtr<AAnchorPoint>>& OutCandidates) const
{
	OutCandidates.Reset();
	OutCandidates.Reserve(CandidateSource.Num());

	for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : CandidateSource)
	{
		if (not IsValid(CandidateAnchor))
		{
			continue;
		}

		if (not IsAnchorCached(CandidateAnchor))
		{
			continue;
		}

		if (AnchorToExclude != nullptr && CandidateAnchor == AnchorToExclude)
		{
			continue;
		}

		const int32 ConnectionDegree = GetAnchorConnectionDegree(CandidateAnchor);
		if (ConnectionDegree < MinDegree || ConnectionDegree > MaxDegree)
		{
			continue;
		}

		OutCandidates.Add(CandidateAnchor);
	}

	if (OutCandidates.Num() == 0)
	{
		return false;
	}

	Algo::Sort(OutCandidates, [](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
	{
		if (not IsValid(Left))
		{
			return false;
		}

		if (not IsValid(Right))
		{
			return true;
		}

		return AAnchorPoint::IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
	});

	return true;
}

AAnchorPoint* AGeneratorWorldCampaign::SelectAnchorCandidateByAttempt(
	const TArray<TObjectPtr<AAnchorPoint>>& Candidates,
	int32 AttemptIndex) const
{
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	const int32 CandidateIndex = AttemptIndex % Candidates.Num();
	return Candidates[CandidateIndex].Get();
}

bool AGeneratorWorldCampaign::HasSharedEndpoint(const FConnectionSegment& Segment, const AAnchorPoint* AnchorA,
                                                const AAnchorPoint* AnchorB) const
{
	return Segment.StartAnchor.Get() == AnchorA || Segment.EndAnchor.Get() == AnchorA
		|| Segment.StartAnchor.Get() == AnchorB || Segment.EndAnchor.Get() == AnchorB;
}

bool AGeneratorWorldCampaign::IsSegmentIntersectingExisting(const FVector2D& StartPoint, const FVector2D& EndPoint,
                                                            const AAnchorPoint* StartAnchor,
                                                            const AAnchorPoint* EndAnchor,
                                                            const TArray<FConnectionSegment>& ExistingSegments,
                                                            const AConnection* ConnectionToIgnore) const
{
	for (const FConnectionSegment& Segment : ExistingSegments)
	{
		if (Segment.OwningConnection.Get() == ConnectionToIgnore)
		{
			continue;
		}

		if (HasSharedEndpoint(Segment, StartAnchor, EndAnchor))
		{
			continue;
		}

		if (DoSegmentsIntersect(StartPoint, EndPoint, Segment.StartPoint, Segment.EndPoint))
		{
			return true;
		}
	}

	return false;
}

void AGeneratorWorldCampaign::ClearExistingConnections()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	for (TActorIterator<AConnection> ConnectionIterator(World); ConnectionIterator; ++ConnectionIterator)
	{
		AConnection* Connection = *ConnectionIterator;
		if (not IsValid(Connection))
		{
			continue;
		}

		Connection->Destroy();
	}

	M_GeneratedConnections.Reset();
}

void AGeneratorWorldCampaign::GatherAnchorPoints(TArray<TObjectPtr<AAnchorPoint>>& OutAnchorPoints) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	OutAnchorPoints.Reset();
	for (TActorIterator<AAnchorPoint> AnchorIterator(World); AnchorIterator; ++AnchorIterator)
	{
		AAnchorPoint* AnchorPoint = *AnchorIterator;
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		OutAnchorPoints.Add(AnchorPoint);
	}
}

void AGeneratorWorldCampaign::AssignDesiredConnections(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
                                                       FRandomStream& RandomStream,
                                                       TMap<TObjectPtr<AAnchorPoint>, int32>& OutDesiredConnections)
const
{
	OutDesiredConnections.Reset();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const int32 DesiredConnections = RandomStream.RandRange(ConnectionGenerationRules.MinConnections,
		                                                        ConnectionGenerationRules.MaxConnections);
		OutDesiredConnections.Add(AnchorPoint, DesiredConnections);
	}
}

void AGeneratorWorldCampaign::GeneratePhasePreferredConnections(AAnchorPoint* AnchorPoint,
                                                                const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
                                                                const TMap<TObjectPtr<AAnchorPoint>, int32>&
                                                                DesiredConnections,
                                                                TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	const int32* DesiredConnectionsPtr = DesiredConnections.Find(AnchorPoint);
	const int32 DesiredConnectionCount = DesiredConnectionsPtr
		                                     ? *DesiredConnectionsPtr
		                                     : ConnectionGenerationRules.MinConnections;
	if (AnchorPoint->GetConnectionCount() >= DesiredConnectionCount)
	{
		return;
	}

	TArray<FAnchorCandidate> Candidates = BuildAndSortCandidates(AnchorPoint, AnchorPoints,
	                                                             ConnectionGenerationRules.MaxConnections,
	                                                             ConnectionGenerationRules.MaxPreferredDistance, false);

	const FColor RegularConnectionColor = FColor::Green;
	for (const FAnchorCandidate& Candidate : Candidates)
	{
		if (AnchorPoint->GetConnectionCount() >= DesiredConnectionCount)
		{
			break;
		}

		if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
		{
			break;
		}

		if (not IsValid(Candidate.AnchorPoint))
		{
			continue;
		}

		TryCreateConnection(AnchorPoint, Candidate.AnchorPoint, ExistingSegments, RegularConnectionColor);
	}
}

void AGeneratorWorldCampaign::GeneratePhaseExtendedConnections(AAnchorPoint* AnchorPoint,
                                                               const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints,
                                                               TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MinConnections)
	{
		return;
	}

	TArray<FAnchorCandidate> Candidates = BuildAndSortCandidates(AnchorPoint, AnchorPoints,
	                                                             ConnectionGenerationRules.MaxConnections,
	                                                             ConnectionGenerationRules.MaxPreferredDistance, true);

	const FColor ExtendedConnectionColor = FColor::Yellow;
	for (const FAnchorCandidate& Candidate : Candidates)
	{
		if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MinConnections)
		{
			break;
		}

		if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
		{
			break;
		}

		if (not IsValid(Candidate.AnchorPoint))
		{
			continue;
		}

		TryCreateConnection(AnchorPoint, Candidate.AnchorPoint, ExistingSegments, ExtendedConnectionColor);
	}
}

void AGeneratorWorldCampaign::GeneratePhaseThreeWayConnections(AAnchorPoint* AnchorPoint,
                                                               TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	while (AnchorPoint->GetConnectionCount() < ConnectionGenerationRules.MinConnections)
	{
		if (not TryAddThreeWayConnection(AnchorPoint, ExistingSegments))
		{
			break;
		}
	}
}

bool AGeneratorWorldCampaign::TryCreateConnection(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
                                                  TArray<FConnectionSegment>& ExistingSegments,
                                                  const FColor& DebugColor)
{
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	if (not IsValid(CandidateAnchor))
	{
		return false;
	}

	const FVector AnchorLocation = AnchorPoint->GetActorLocation();
	const FVector CandidateLocation = CandidateAnchor->GetActorLocation();
	const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
	const FVector2D CandidateLocation2D(CandidateLocation.X, CandidateLocation.Y);

	if (not IsConnectionAllowed(AnchorPoint, CandidateAnchor, AnchorLocation2D, CandidateLocation2D, ExistingSegments,
	                            nullptr))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	constexpr float HalfValue = 0.5f;
	const FVector SpawnLocation = (AnchorLocation + CandidateLocation) * HalfValue;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;

	UClass* ConnectionClassToSpawn = nullptr;
	if (M_ConnectionClass)
	{
		ConnectionClassToSpawn = *M_ConnectionClass; // UClass* from TSubclassOf
	}
	else
	{
		ConnectionClassToSpawn = AConnection::StaticClass();
	}
	AConnection* NewConnection = World->SpawnActor<AConnection>(
		ConnectionClassToSpawn,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);

	if (not IsValid(NewConnection))
	{
		return false;
	}

	NewConnection->InitializeConnection(AnchorPoint, CandidateAnchor);
	RegisterConnectionOnAnchors(NewConnection, AnchorPoint, CandidateAnchor);
	AddConnectionSegment(NewConnection, AnchorPoint, CandidateAnchor, ExistingSegments);
	M_GeneratedConnections.Add(NewConnection);

	if constexpr (DeveloperSettings::Debugging::GCampaignConnectionGeneration_Compile_DebugSymbols)
	{
		DebugDrawConnection(NewConnection, DebugColor);
	}

	return true;
}

bool AGeneratorWorldCampaign::IsConnectionAllowed(AAnchorPoint* AnchorPoint, AAnchorPoint* CandidateAnchor,
                                                  const FVector2D& StartPoint, const FVector2D& EndPoint,
                                                  const TArray<FConnectionSegment>& ExistingSegments,
                                                  const AConnection* ConnectionToIgnore) const
{
	if (AnchorPoint->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
	{
		return false;
	}

	if (CandidateAnchor->GetConnectionCount() >= ConnectionGenerationRules.MaxConnections)
	{
		return false;
	}

	return not IsSegmentIntersectingExisting(StartPoint, EndPoint, AnchorPoint, CandidateAnchor,
	                                         ExistingSegments, ConnectionToIgnore);
}

bool AGeneratorWorldCampaign::TryAddThreeWayConnection(AAnchorPoint* AnchorPoint,
                                                       TArray<FConnectionSegment>& ExistingSegments)
{
	if (not IsValid(AnchorPoint))
	{
		return false;
	}

	if (M_GeneratedConnections.Num() == 0)
	{
		return false;
	}

	const FClosestConnectionCandidate ClosestCandidate = FindClosestConnectionCandidate(
		AnchorPoint, M_GeneratedConnections);
	AConnection* ClosestConnection = ClosestCandidate.Connection.Get();
	if (not IsValid(ClosestConnection))
	{
		return false;
	}

	const FVector AnchorLocation = AnchorPoint->GetActorLocation();
	const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
	const FVector2D JunctionLocation2D(ClosestCandidate.JunctionLocation.X, ClosestCandidate.JunctionLocation.Y);
	if (IsSegmentIntersectingExisting(AnchorLocation2D, JunctionLocation2D, AnchorPoint, nullptr,
	                                  ExistingSegments, ClosestConnection))
	{
		return false;
	}

	if (not ClosestConnection->TryAddThirdAnchor(AnchorPoint))
	{
		return false;
	}

	RegisterThirdAnchorOnConnection(ClosestConnection, AnchorPoint);
	AddThirdConnectionSegment(ClosestConnection, AnchorPoint, ClosestCandidate.JunctionLocation, ExistingSegments);

	if constexpr (DeveloperSettings::Debugging::GCampaignConnectionGeneration_Compile_DebugSymbols)
	{
		const FColor ThreeWayConnectionColor = FColor::Red;
		DebugDrawThreeWay(ClosestConnection, ThreeWayConnectionColor);
	}

	return true;
}

void AGeneratorWorldCampaign::RegisterConnectionOnAnchors(AConnection* Connection, AAnchorPoint* AnchorA,
                                                          AAnchorPoint* AnchorB) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(AnchorA))
	{
		return;
	}

	if (not IsValid(AnchorB))
	{
		return;
	}

	AnchorA->AddConnection(Connection, AnchorB);
	AnchorB->AddConnection(Connection, AnchorA);
}

void AGeneratorWorldCampaign::RegisterThirdAnchorOnConnection(AConnection* Connection, AAnchorPoint* ThirdAnchor) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(ThirdAnchor))
	{
		return;
	}

	const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
	if (ConnectedAnchors.Num() < 2)
	{
		return;
	}

	AAnchorPoint* FirstAnchor = ConnectedAnchors[0];
	AAnchorPoint* SecondAnchor = ConnectedAnchors[1];
	if (not IsValid(FirstAnchor) || not IsValid(SecondAnchor))
	{
		return;
	}

	ThirdAnchor->AddConnection(Connection, FirstAnchor);
	ThirdAnchor->AddConnection(Connection, SecondAnchor);

	FirstAnchor->AddConnection(Connection, ThirdAnchor);
	SecondAnchor->AddConnection(Connection, ThirdAnchor);
}

void AGeneratorWorldCampaign::AddConnectionSegment(AConnection* Connection, AAnchorPoint* AnchorA,
                                                   AAnchorPoint* AnchorB,
                                                   TArray<FConnectionSegment>& ExistingSegments) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(AnchorA) || not IsValid(AnchorB))
	{
		return;
	}

	const FVector LocationA = AnchorA->GetActorLocation();
	const FVector LocationB = AnchorB->GetActorLocation();

	FConnectionSegment Segment;
	Segment.StartPoint = FVector2D(LocationA.X, LocationA.Y);
	Segment.EndPoint = FVector2D(LocationB.X, LocationB.Y);
	Segment.StartAnchor = AnchorA;
	Segment.EndAnchor = AnchorB;
	Segment.OwningConnection = Connection;
	ExistingSegments.Add(Segment);
}

void AGeneratorWorldCampaign::AddThirdConnectionSegment(AConnection* Connection, AAnchorPoint* ThirdAnchor,
                                                        const FVector& JunctionLocation,
                                                        TArray<FConnectionSegment>& ExistingSegments) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not IsValid(ThirdAnchor))
	{
		return;
	}

	const FVector ThirdAnchorLocation = ThirdAnchor->GetActorLocation();
	FConnectionSegment Segment;
	Segment.StartPoint = FVector2D(ThirdAnchorLocation.X, ThirdAnchorLocation.Y);
	Segment.EndPoint = FVector2D(JunctionLocation.X, JunctionLocation.Y);
	Segment.StartAnchor = ThirdAnchor;
	Segment.EndAnchor = nullptr;
	Segment.OwningConnection = Connection;
	ExistingSegments.Add(Segment);
}

void AGeneratorWorldCampaign::DebugNotifyAnchorProcessing(AAnchorPoint* AnchorPoint, const FString& Label,
                                                          const FColor& Color) const
{
	if (not IsValid(AnchorPoint))
	{
		return;
	}

	if (not GetIsValidCampaignDebugger())
	{
		return;
	}

	UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();

	constexpr float DebugDuration = 10.f;
	CampaignDebugger->DrawInfoAtAnchor(AnchorPoint, Label, DebugDuration, Color);
}

void AGeneratorWorldCampaign::DebugDrawConnection(const AConnection* Connection, const FColor& Color) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	constexpr float DebugDuration = 10.f;
	Connection->DebugDrawBaseConnection(Color, DebugDuration);
}

void AGeneratorWorldCampaign::DebugDrawThreeWay(const AConnection* Connection, const FColor& Color) const
{
	if (not IsValid(Connection))
	{
		return;
	}

	constexpr float DebugDuration = 10.f;
	Connection->DebugDrawThirdConnection(Color, DebugDuration);
}

void AGeneratorWorldCampaign::DrawDebugConnectionLine(UWorld* World, const FVector& Start, const FVector& End,
                                                      const FColor& Color) const
{
	if (not IsValid(World))
	{
		return;
	}

	DrawDebugLine(World,
	              Start,
	              End,
	              Color,
	              false,
	              M_DebugConnectionDrawDurationSeconds,
	              0,
	              M_DebugConnectionLineThickness);
}

void AGeneratorWorldCampaign::DrawDebugConnectionForActor(UWorld* World, const AConnection* Connection,
                                                          const FVector& HeightOffset,
                                                          const FColor& BaseConnectionColor,
                                                          const FColor& ThreeWayConnectionColor) const
{
	if (not IsValid(World))
	{
		return;
	}

	if (not IsValid(Connection))
	{
		return;
	}

	const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
	if (ConnectedAnchors.Num() < 2)
	{
		return;
	}

	AAnchorPoint* FirstAnchor = ConnectedAnchors[0];
	AAnchorPoint* SecondAnchor = ConnectedAnchors[1];
	if (not IsValid(FirstAnchor) || not IsValid(SecondAnchor))
	{
		return;
	}

	DrawDebugConnectionLine(World,
	                        FirstAnchor->GetActorLocation() + HeightOffset,
	                        SecondAnchor->GetActorLocation() + HeightOffset,
	                        BaseConnectionColor);

	if (not Connection->GetIsThreeWayConnection())
	{
		return;
	}

	if (ConnectedAnchors.Num() < 3)
	{
		return;
	}

	AAnchorPoint* ThirdAnchor = ConnectedAnchors[2];
	if (not IsValid(ThirdAnchor))
	{
		return;
	}

	DrawDebugConnectionLine(World,
	                        Connection->GetJunctionLocation() + HeightOffset,
	                        ThirdAnchor->GetActorLocation() + HeightOffset,
	                        ThreeWayConnectionColor);
}
