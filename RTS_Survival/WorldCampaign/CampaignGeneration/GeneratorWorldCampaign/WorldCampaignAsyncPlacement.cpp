// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/WorldCampaignAsyncPlacement.h"

#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationHelpers/WorldCampaignGenerationHelper.h"

namespace WorldCampaignAsyncPlacement
{
using FWorldCampaignPlayerHQPlacementRulesSnapshot = FPlayerHQPlacementRulesSnapshot;
using FWorldCampaignEnemyHQPlacementRulesSnapshot = FEnemyHQPlacementRulesSnapshot;
using FWorldCampaignEnemyWallPlacementRulesSnapshot = FEnemyWallPlacementRulesSnapshot;
using FWorldCampaignPerMissionRulesSnapshot = FPerMissionRulesSnapshot;
using FWorldCampaignMissionPlacementSnapshot = FMissionPlacementSnapshot;
using FWorldCampaignAnchorSnapshot = FAnchorSnapshot;
using FWorldCampaignConnectionSnapshot = FConnectionSnapshot;
using FWorldCampaignPlacementSnapshot = FPlacementSnapshot;
using FWorldCampaignPlacementDebugEvent = FPlacementDebugEvent;
using FWorldCampaignPlacementResult = FPlacementResult;
using FWorldCampaignAsyncPlacementProgress = FPlacementProgress;

namespace
{
	/*
	 * Keep the worker retry constants local to the pure solver so changes to async retry behavior
	 * are reviewed alongside the backtracking implementation instead of the actor lifecycle code.
	 */
	constexpr int32 MaxStepAttempts = 15000;
	constexpr int32 MaxTotalAttempts = 100000;
	constexpr int32 AttemptSeedMultiplier = 13;
	constexpr int32 MaxRelaxationAttempts = 3;
	constexpr int32 RelaxedHopDistanceMax = TNumericLimits<int32>::Max();
	constexpr float RelaxedDistanceMax = TNumericLimits<float>::Max();
	constexpr int32 MaxChokepointPairSamples = 48;
	constexpr int32 ChokepointSeedOffset = 7919;
	constexpr int32 PlayerHQForcePlacementSeedOffset = 4021;
	constexpr int32 EnemyHQForcePlacementSeedOffset = 4027;
	constexpr uint64 Mix64Increment = 0x9E3779B97F4A7C15ull;
	constexpr uint64 Mix64MultiplierA = 0xBF58476D1CE4E5B9ull;
	constexpr uint64 Mix64MultiplierB = 0x94D049BB133111EBull;
	constexpr int32 Mix64ShiftA = 30;
	constexpr int32 Mix64ShiftB = 27;
	constexpr int32 Mix64ShiftC = 31;
	constexpr uint64 HashCombineSaltA = 0xD6E8FEB86659FD93ull;
	constexpr uint64 HashCombineSaltB = 0xA5A356281F9B0F1Bull;
	constexpr uint64 HashCombineSaltC = 0xC13FA9A902A6328Full;
	constexpr uint64 HashCombineSaltD = 0x91E10DA5C79E7B1Dull;
	constexpr uint64 HashCombineSaltE = 0xF1357AEA2E62A9C5ull;
	constexpr int32 NoRequiredItems = 0;

	using CampaignGenerationHelper::AddChokepointPathContribution;
	using CampaignGenerationHelper::BuildCanonicalFailedEnemyDeclusterSwapPairKey;
	using CampaignGenerationHelper::BuildImpactedEnemyDeclusterAnchorKeys;
	using CampaignGenerationHelper::ComputeEnemyLocalDensity;
	using CampaignGenerationHelper::ComputeImpactedEnemyDeclusterDensitySum;
	using CampaignGenerationHelper::GetEnemyPreferenceScore;
	using CampaignGenerationHelper::GetEnemyWallPreferenceScore;
	using CampaignGenerationHelper::GetHopPreferenceWeight;
	using CampaignGenerationHelper::GetIsSwappableEnemyType;
	using CampaignGenerationHelper::GetOverrideMissionPreferenceScore;
	using CampaignGenerationHelper::HasMinimumAdjacentMatches;

	struct FRuleRelaxationState
	{
		bool bRelaxDistance = false;
		bool bRelaxSpacing = false;
		bool bRelaxPreference = false;
	};

	enum class EFailSafeItemKind : uint8
	{
		Enemy = 0,
		Neutral = 1,
		Mission = 2
	};

	struct FFailSafeItem
	{
		EFailSafeItemKind Kind = EFailSafeItemKind::Enemy;
		uint8 RawEnumValue = 0;
		float MinDistance = 0.f;
		EMapEnemyItem EnemyType = EMapEnemyItem::None;
		EMapNeutralObjectType NeutralType = EMapNeutralObjectType::None;
		EMapMission MissionType = EMapMission::None;
	};

	struct FFailSafePlacementTotals
	{
		int32 EnemyPlaced = 0;
		int32 NeutralPlaced = 0;
		int32 MissionPlaced = 0;
		int32 Discarded = 0;
	};

	struct FEnemyDeclusterAnchorScore
	{
		FGuid AnchorKey;
		EMapEnemyItem EnemyType = EMapEnemyItem::None;
		int32 LocalDensity = 0;
	};

	struct FEnemyDeclusterSwapCandidate
	{
		FGuid AnchorKeyA;
		FGuid AnchorKeyB;
		EMapEnemyItem EnemyTypeA = EMapEnemyItem::None;
		EMapEnemyItem EnemyTypeB = EMapEnemyItem::None;
		int32 Improvement = 0;
		bool bIsValid = false;
	};

	uint64 Mix64(uint64 Value)
	{
		Value += Mix64Increment;
		Value = (Value ^ (Value >> Mix64ShiftA)) * Mix64MultiplierA;
		Value = (Value ^ (Value >> Mix64ShiftB)) * Mix64MultiplierB;
		return Value ^ (Value >> Mix64ShiftC);
	}

	uint64 HashCombine64(uint64 Seed, uint64 V0, uint64 V1, uint64 V2, uint64 V3)
	{
		uint64 Result = Seed;
		Result ^= Mix64(V0 + HashCombineSaltA);
		Result ^= Mix64(V1 + HashCombineSaltB);
		Result ^= Mix64(V2 + HashCombineSaltC);
		Result ^= Mix64(V3 + HashCombineSaltD);
		return Mix64(Result + HashCombineSaltE);
	}

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

	void AppendMissingEnemyItems(const FWorldCampaignCountDifficultyTuning& Tuning,
	                             const FWorldCampaignDerivedData& DerivedData,
	                             TArray<EMapEnemyItem>& OutMissingItems)
	{
		const TMap<EMapEnemyItem, int32> RequiredCounts = BuildRequiredEnemyItemCounts(Tuning);
		const TArray<EMapEnemyItem> EnemyTypes = GetSortedEnemyTypes(RequiredCounts);
		for (const EMapEnemyItem EnemyType : EnemyTypes)
		{
			if (EnemyType == EMapEnemyItem::EnemyHQ || EnemyType == EMapEnemyItem::EnemyWall)
			{
				continue;
			}

			const int32 Required = RequiredCounts.FindRef(EnemyType);
			const int32 Placed = DerivedData.EnemyItemPlacedCounts.FindRef(EnemyType);
			const int32 Missing = FMath::Max(0, Required - Placed);
			for (int32 MissingIndex = 0; MissingIndex < Missing; MissingIndex++)
			{
				OutMissingItems.Add(EnemyType);
			}
		}
	}

	void AppendMissingNeutralItems(const FWorldCampaignCountDifficultyTuning& Tuning,
	                               const FWorldCampaignDerivedData& DerivedData,
	                               TArray<EMapNeutralObjectType>& OutMissingItems)
	{
		const TMap<EMapNeutralObjectType, int32> RequiredCounts = BuildRequiredNeutralItemCounts(Tuning);
		const TArray<EMapNeutralObjectType> NeutralTypes = GetSortedNeutralTypes(RequiredCounts);
		for (const EMapNeutralObjectType NeutralType : NeutralTypes)
		{
			const int32 Required = RequiredCounts.FindRef(NeutralType);
			const int32 Placed = DerivedData.NeutralItemPlacedCounts.FindRef(NeutralType);
			const int32 Missing = FMath::Max(0, Required - Placed);
			for (int32 MissingIndex = 0; MissingIndex < Missing; MissingIndex++)
			{
				OutMissingItems.Add(NeutralType);
			}
		}
	}

	void AppendMissingMissions(const TArray<EMapMission>& PlacementPlan,
	                           const FWorldCampaignDerivedData& DerivedData,
	                           TArray<EMapMission>& OutMissingMissions)
	{
		TMap<EMapMission, int32> RemainingPlacedCounts = DerivedData.MissionPlacedCounts;
		for (const EMapMission MissionType : PlacementPlan)
		{
			int32* RemainingCount = RemainingPlacedCounts.Find(MissionType);
			if (RemainingCount && *RemainingCount > 0)
			{
				(*RemainingCount)--;
				continue;
			}

			OutMissingMissions.Add(MissionType);
		}
	}

	void AppendEnemyFailSafeItems(const TArray<EMapEnemyItem>& MissingEnemyItems,
	                              const TFunctionRef<float(EMapEnemyItem)>& GetEnemyMinDistance,
	                              TArray<FFailSafeItem>& OutItems)
	{
		for (const EMapEnemyItem EnemyType : MissingEnemyItems)
		{
			FFailSafeItem Item;
			Item.Kind = EFailSafeItemKind::Enemy;
			Item.RawEnumValue = static_cast<uint8>(EnemyType);
			Item.MinDistance = GetEnemyMinDistance(EnemyType);
			Item.EnemyType = EnemyType;
			OutItems.Add(Item);
		}
	}

	void AppendNeutralFailSafeItems(const TArray<EMapNeutralObjectType>& MissingNeutralItems,
	                                const TFunctionRef<float(EMapNeutralObjectType)>& GetNeutralMinDistance,
	                                TArray<FFailSafeItem>& OutItems)
	{
		for (const EMapNeutralObjectType NeutralType : MissingNeutralItems)
		{
			FFailSafeItem Item;
			Item.Kind = EFailSafeItemKind::Neutral;
			Item.RawEnumValue = static_cast<uint8>(NeutralType);
			Item.MinDistance = GetNeutralMinDistance(NeutralType);
			Item.NeutralType = NeutralType;
			OutItems.Add(Item);
		}
	}

	void AppendMissionFailSafeItems(const TArray<EMapMission>& MissingMissions,
	                                const TFunctionRef<float(EMapMission)>& GetMissionMinDistance,
	                                TArray<FFailSafeItem>& OutItems)
	{
		for (const EMapMission MissionType : MissingMissions)
		{
			FFailSafeItem Item;
			Item.Kind = EFailSafeItemKind::Mission;
			Item.RawEnumValue = static_cast<uint8>(MissionType);
			Item.MinDistance = GetMissionMinDistance(MissionType);
			Item.MissionType = MissionType;
			OutItems.Add(Item);
		}
	}

	void SortFailSafeItemsByDistanceAndType(TArray<FFailSafeItem>& InOutItems)
	{
		constexpr float DistanceTolerance = 0.001f;
		InOutItems.Sort([](const FFailSafeItem& Left, const FFailSafeItem& Right)
		{
			if (not FMath::IsNearlyEqual(Left.MinDistance, Right.MinDistance, DistanceTolerance))
			{
				return Left.MinDistance < Right.MinDistance;
			}

			if (Left.Kind != Right.Kind)
			{
				return static_cast<uint8>(Left.Kind) < static_cast<uint8>(Right.Kind);
			}

			return Left.RawEnumValue < Right.RawEnumValue;
		});
	}

	void BuildFailSafeItemsToPlace(const FWorldCampaignCountDifficultyTuning& Tuning,
	                               const FWorldCampaignDerivedData& DerivedData,
	                               const TArray<EMapMission>& MissionPlacementPlan,
	                               const TFunctionRef<float(EMapEnemyItem)>& GetEnemyMinDistance,
	                               const TFunctionRef<float(EMapNeutralObjectType)>& GetNeutralMinDistance,
	                               const TFunctionRef<float(EMapMission)>& GetMissionMinDistance,
	                               TArray<FFailSafeItem>& OutItems)
	{
		TArray<EMapEnemyItem> MissingEnemyItems;
		AppendMissingEnemyItems(Tuning, DerivedData, MissingEnemyItems);
		TArray<EMapNeutralObjectType> MissingNeutralItems;
		AppendMissingNeutralItems(Tuning, DerivedData, MissingNeutralItems);
		TArray<EMapMission> MissingMissions;
		AppendMissingMissions(MissionPlacementPlan, DerivedData, MissingMissions);

		OutItems.Reset();
		OutItems.Reserve(MissingEnemyItems.Num() + MissingNeutralItems.Num() + MissingMissions.Num());
		AppendEnemyFailSafeItems(MissingEnemyItems, GetEnemyMinDistance, OutItems);
		AppendNeutralFailSafeItems(MissingNeutralItems, GetNeutralMinDistance, OutItems);
		AppendMissionFailSafeItems(MissingMissions, GetMissionMinDistance, OutItems);
		SortFailSafeItemsByDistanceAndType(OutItems);
	}

	int32 GetCachedHopDistance(const TMap<FGuid, int32>& HopDistancesByAnchorKey, const FGuid& AnchorKey)
	{
		const int32* CachedDistance = HopDistancesByAnchorKey.Find(AnchorKey);
		return CachedDistance ? *CachedDistance : INDEX_NONE;
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

	bool IsTimeoutFailSafeStep(const ECampaignGenerationStep Step)
	{
		return Step == ECampaignGenerationStep::EnemyObjectsPlaced
			|| Step == ECampaignGenerationStep::NeutralObjectsPlaced
			|| Step == ECampaignGenerationStep::MissionsPlaced;
	}

	struct FWorldCampaignPlacementSolverState
	{
		int32 SeedUsed = 0;
		FGuid PlayerHQAnchorKey;
		FGuid EnemyHQAnchorKey;
		int32 PlayerHQAnchorIndex = INDEX_NONE;
		int32 EnemyHQAnchorIndex = INDEX_NONE;
		TMap<FGuid, EMapEnemyItem> EnemyItemsByAnchorKey;
		TMap<FGuid, EMapNeutralObjectType> NeutralItemsByAnchorKey;
		TMap<FGuid, EMapMission> MissionsByAnchorKey;
	};

	struct FWorldCampaignPlacementSolverTransaction
	{
		ECampaignGenerationStep CompletedStep = ECampaignGenerationStep::NotStarted;
		FWorldCampaignPlacementSolverState PreviousPlacementState;
		FWorldCampaignDerivedData PreviousDerivedData;
		bool bIsMicroTransaction = false;
		ECampaignGenerationStep MicroParentStep = ECampaignGenerationStep::NotStarted;
		FGuid MicroAnchorKey;
		EMapItemType MicroItemType = EMapItemType::None;
		EMapEnemyItem MicroEnemyItemType = EMapEnemyItem::None;
		EMapMission MicroMissionType = EMapMission::None;
		int32 MicroIndexWithinParent = INDEX_NONE;
		TArray<FGuid> MicroAdditionalAnchorKeys;
	};

	struct FWorldCampaignKeyPlacementCandidate
	{
		FGuid AnchorKey;
		int32 AnchorIndex = INDEX_NONE;
		float Score = 0.f;
		int32 HopDistanceFromHQ = INDEX_NONE;
		int32 CandidateOrder = INDEX_NONE;
		FGuid CompanionAnchorKey;
		int32 CompanionAnchorIndex = INDEX_NONE;
		uint8 CompanionRawSubtype = 0;
		EMapItemType CompanionItemType = EMapItemType::None;
	};

	struct FWorldCampaignFailSafeAnchorDistance
	{
		FGuid AnchorKey;
		FVector2D LocationXY = FVector2D::ZeroVector;
		float DistanceSquared = 0.f;
	};

	class FWorldCampaignPlacementBacktrackingSolver
	{
	public:
		FWorldCampaignPlacementResult Solve(
			const FWorldCampaignPlacementSnapshot& InSnapshot,
			const TSharedPtr<FWorldCampaignAsyncPlacementProgress, ESPMode::ThreadSafe>& InProgress = nullptr)
		{
			Snapshot = &InSnapshot;
			Progress = InProgress;
			ResetWorkingState();
			InitialTotalAttemptCount = Snapshot->InitialTotalAttemptCount;
			TotalAttemptCount = InitialTotalAttemptCount;
			StepAttemptIndices = Snapshot->InitialStepAttemptIndices;
			UpdateProgress(TEXT("Building anchor lookup"), ECampaignGenerationStep::ConnectionsCreated);

			if (not BuildAnchorLookup())
			{
				return BuildFailureResult(TEXT("Async placement failed: snapshot had no anchors."));
			}

			State.SeedUsed = Snapshot->SeedUsed;
			UpdateProgress(TEXT("Building derived graph caches"), ECampaignGenerationStep::ConnectionsCreated);
			InitializeDerivedDataFromSnapshot();

			UpdateProgress(TEXT("Running placement backtracking"), ECampaignGenerationStep::PlayerHQPlaced);
			if (not ExecutePlacementBacktracking())
			{
				return BuildFailureResult(TEXT("Async placement failed: backtracking exhausted."));
			}

			UpdateProgress(TEXT("Finished placement backtracking"), ECampaignGenerationStep::Finished);
			return BuildSuccessResult();
		}

	private:
		const FWorldCampaignPlacementSnapshot* Snapshot = nullptr;
		TSharedPtr<FWorldCampaignAsyncPlacementProgress, ESPMode::ThreadSafe> Progress;
		TArray<FWorldCampaignAnchorSnapshot> AnchorSnapshots;
		TMap<FGuid, int32> AnchorIndexByKey;
		TArray<int32> AnchorIndicesInSnapshotOrder;
		TArray<int32> SortedAnchorIndices;
		mutable TMap<int32, TMap<int32, int32>> HopDistanceCacheByStartAnchorIndex;
		FWorldCampaignPlacementSolverState State;
		FWorldCampaignDerivedData DerivedData;
		TArray<FWorldCampaignPlacementSolverTransaction> Transactions;
		TMap<ECampaignGenerationStep, int32> StepAttemptIndices;
		TArray<FWorldCampaignPlacementDebugEvent> DebugEvents;
		int32 EnemyMicroPlacedCount = 0;
		int32 MissionMicroPlacedCount = 0;
		int32 InitialTotalAttemptCount = 0;
		int32 TotalAttemptCount = 0;
		ECampaignGenerationStep GenerationStep = ECampaignGenerationStep::ConnectionsCreated;
		bool bRequiresGameThreadBacktrack = false;
		ECampaignGenerationStep GameThreadBacktrackFailedStep = ECampaignGenerationStep::NotStarted;
		int32 GameThreadTransactionsToUndo = 0;
		ECampaignGenerationStep BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;

		void ResetWorkingState()
		{
			AnchorSnapshots.Reset();
			AnchorIndexByKey.Reset();
			AnchorIndicesInSnapshotOrder.Reset();
			SortedAnchorIndices.Reset();
			HopDistanceCacheByStartAnchorIndex.Reset();
			State = FWorldCampaignPlacementSolverState();
			DerivedData = FWorldCampaignDerivedData();
			Transactions.Reset();
			StepAttemptIndices.Reset();
			DebugEvents.Reset();
			EnemyMicroPlacedCount = 0;
			MissionMicroPlacedCount = 0;
			InitialTotalAttemptCount = 0;
			TotalAttemptCount = 0;
			GenerationStep = ECampaignGenerationStep::ConnectionsCreated;
			bRequiresGameThreadBacktrack = false;
			GameThreadBacktrackFailedStep = ECampaignGenerationStep::NotStarted;
			GameThreadTransactionsToUndo = 0;
			BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;
		}

		bool BuildAnchorLookup()
		{
			if (Snapshot == nullptr)
			{
				return false;
			}

			AnchorSnapshots = Snapshot->Anchors;
			AnchorIndexByKey.Reserve(AnchorSnapshots.Num());
			AnchorIndicesInSnapshotOrder.Reserve(Snapshot->AnchorIndicesInCachedOrder.Num() > 0
				                                     ? Snapshot->AnchorIndicesInCachedOrder.Num()
				                                     : AnchorSnapshots.Num());
			SortedAnchorIndices.Reserve(AnchorSnapshots.Num());
			for (int32 AnchorIndex = 0; AnchorIndex < AnchorSnapshots.Num(); AnchorIndex++)
			{
				const FWorldCampaignAnchorSnapshot& Anchor = AnchorSnapshots[AnchorIndex];
				if (not Anchor.AnchorKey.IsValid())
				{
					continue;
				}

				AnchorIndexByKey.Add(Anchor.AnchorKey, AnchorIndex);
				SortedAnchorIndices.Add(AnchorIndex);
			}

			for (const int32 AnchorIndex : Snapshot->AnchorIndicesInCachedOrder)
			{
				if (AnchorSnapshots.IsValidIndex(AnchorIndex)
					&& AnchorSnapshots[AnchorIndex].AnchorKey.IsValid())
				{
					AnchorIndicesInSnapshotOrder.Add(AnchorIndex);
				}
			}

			if (AnchorIndicesInSnapshotOrder.Num() == 0)
			{
				AnchorIndicesInSnapshotOrder = SortedAnchorIndices;
			}

			SortAnchorIndices(SortedAnchorIndices);
			return SortedAnchorIndices.Num() > 0;
		}

		static bool IsAnchorKeyLess(const FGuid& Left, const FGuid& Right)
		{
			if (Left.A != Right.A)
			{
				return Left.A < Right.A;
			}

			if (Left.B != Right.B)
			{
				return Left.B < Right.B;
			}

			if (Left.C != Right.C)
			{
				return Left.C < Right.C;
			}

			return Left.D < Right.D;
		}

		static void SortAnchorKeys(TArray<FGuid>& InOutAnchorKeys)
		{
			InOutAnchorKeys.Sort([](const FGuid& Left, const FGuid& Right)
			{
				return IsAnchorKeyLess(Left, Right);
			});
		}

		void SortAnchorIndices(TArray<int32>& InOutAnchorIndices) const
		{
			InOutAnchorIndices.Sort([this](const int32 Left, const int32 Right)
			{
				const FGuid LeftKey = GetAnchorKey(Left);
				const FGuid RightKey = GetAnchorKey(Right);
				if (LeftKey != RightKey)
				{
					return IsAnchorKeyLess(LeftKey, RightKey);
				}

				return Left < Right;
			});
		}

		void AddDebugEvent(ECampaignGenerationStep Step, const FString& Message)
		{
			FWorldCampaignPlacementDebugEvent Event;
			Event.Step = Step;
			Event.Message = Message;
			DebugEvents.Add(Event);
		}

		void UpdateProgress(const FString& Phase, const ECampaignGenerationStep CurrentStep) const
		{
			if (not Progress.IsValid())
			{
				return;
			}

			FScopeLock ProgressLock(&Progress->CriticalSection);
			Progress->Phase = Phase;
			Progress->CurrentStep = CurrentStep;
			Progress->TotalAttemptCount = TotalAttemptCount;
			Progress->WorkerAttemptDelta = FMath::Max(0, TotalAttemptCount - InitialTotalAttemptCount);
			Progress->CurrentStepAttemptCount = GetStepAttemptIndex(CurrentStep);
			Progress->TransactionCount = Transactions.Num();
			Progress->EnemyMicroPlacedCount = EnemyMicroPlacedCount;
			Progress->MissionMicroPlacedCount = MissionMicroPlacedCount;
		}

		void UpdateNeutralCandidateProgress(const EMapNeutralObjectType NeutralType,
		                                    const int32 EvaluatedAnchorCount,
		                                    const int32 CandidateCount,
		                                    const int32 OccupiedRejectCount,
		                                    const int32 NoHopRejectCount,
		                                    const int32 HopBandRejectCount,
		                                    const int32 SpacingRejectCount) const
		{
			if (not Progress.IsValid())
			{
				return;
			}

			FScopeLock ProgressLock(&Progress->CriticalSection);
			Progress->NeutralTypeBeingEvaluated = static_cast<int32>(NeutralType);
			Progress->NeutralEvaluatedAnchorCount = EvaluatedAnchorCount;
			Progress->NeutralCandidateCount = CandidateCount;
			Progress->NeutralRejectedOccupiedCount = OccupiedRejectCount;
			Progress->NeutralRejectedNoHopCount = NoHopRejectCount;
			Progress->NeutralRejectedHopBandCount = HopBandRejectCount;
			Progress->NeutralRejectedSpacingCount = SpacingRejectCount;
		}

		FWorldCampaignPlacementResult BuildFailureResult(const FString& FailureReason) const
		{
			FWorldCampaignPlacementResult Result = BuildBaseResult();
			Result.bSucceeded = false;
			Result.FailureReason = FailureReason;
			return Result;
		}

		FWorldCampaignPlacementResult BuildSuccessResult() const
		{
			FWorldCampaignPlacementResult Result = BuildBaseResult();
			Result.bSucceeded = true;
			return Result;
		}

		FWorldCampaignPlacementResult BuildBaseResult() const
		{
			FWorldCampaignPlacementResult Result;
			Result.PlayerHQAnchorKey = State.PlayerHQAnchorKey;
			Result.EnemyHQAnchorKey = State.EnemyHQAnchorKey;
			Result.bRequiresGameThreadBacktrack = bRequiresGameThreadBacktrack;
			Result.GameThreadBacktrackFailedStep = GameThreadBacktrackFailedStep;
			Result.GameThreadTransactionsToUndo = GameThreadTransactionsToUndo;
			Result.WorkerTotalAttemptCount = FMath::Max(0, TotalAttemptCount - InitialTotalAttemptCount);
			Result.StepAttemptIndicesAtEnd = StepAttemptIndices;
			Result.EnemyItemsByAnchorKey = State.EnemyItemsByAnchorKey;
			Result.NeutralItemsByAnchorKey = State.NeutralItemsByAnchorKey;
			Result.MissionsByAnchorKey = State.MissionsByAnchorKey;
			Result.DerivedData = DerivedData;
			Result.DebugEvents = DebugEvents;
			return Result;
		}

		void BuildAnchorConnectionDegrees()
		{
			DerivedData.AnchorConnectionDegreesByAnchorKey.Reset();
			for (const int32 AnchorIndex : AnchorIndicesInSnapshotOrder)
			{
				const FWorldCampaignAnchorSnapshot* Anchor = FindAnchor(AnchorIndex);
				if (Anchor == nullptr)
				{
					continue;
				}

				DerivedData.AnchorConnectionDegreesByAnchorKey.Add(Anchor->AnchorKey, Anchor->ConnectionDegree);
			}
		}

		const FWorldCampaignAnchorSnapshot* FindAnchor(const int32 AnchorIndex) const
		{
			return AnchorSnapshots.IsValidIndex(AnchorIndex) ? &AnchorSnapshots[AnchorIndex] : nullptr;
		}

		const FWorldCampaignAnchorSnapshot* FindAnchor(const FGuid& AnchorKey) const
		{
			const int32* AnchorIndex = AnchorIndexByKey.Find(AnchorKey);
			return AnchorIndex != nullptr ? FindAnchor(*AnchorIndex) : nullptr;
		}

		FGuid GetAnchorKey(const int32 AnchorIndex) const
		{
			const FWorldCampaignAnchorSnapshot* Anchor = FindAnchor(AnchorIndex);
			return Anchor != nullptr ? Anchor->AnchorKey : FGuid();
		}

		int32 FindAnchorIndexByKey(const FGuid& AnchorKey) const
		{
			const int32* AnchorIndex = AnchorIndexByKey.Find(AnchorKey);
			return AnchorIndex != nullptr ? *AnchorIndex : INDEX_NONE;
		}

		int32 GetAnchorConnectionDegree(const FGuid& AnchorKey) const
		{
			const int32* CachedDegree = DerivedData.AnchorConnectionDegreesByAnchorKey.Find(AnchorKey);
			return CachedDegree ? *CachedDegree : 0;
		}

		int32 GetAnchorConnectionDegree(const int32 AnchorIndex) const
		{
			const FWorldCampaignAnchorSnapshot* Anchor = FindAnchor(AnchorIndex);
			return Anchor != nullptr ? Anchor->ConnectionDegree : 0;
		}

		bool IsAnchorCached(const FGuid& AnchorKey) const
		{
			return AnchorIndexByKey.Contains(AnchorKey);
		}

		bool IsAnchorCached(const int32 AnchorIndex) const
		{
			return AnchorSnapshots.IsValidIndex(AnchorIndex)
				&& AnchorSnapshots[AnchorIndex].AnchorKey.IsValid();
		}

		bool TryGetLocationXY(const FGuid& AnchorKey, FVector2D& OutLocationXY) const
		{
			const FWorldCampaignAnchorSnapshot* Anchor = FindAnchor(AnchorKey);
			if (Anchor == nullptr)
			{
				return false;
			}

			OutLocationXY = FVector2D(Anchor->Location);
			return true;
		}

		float GetXYDistance(const FGuid& FirstAnchorKey, const FGuid& SecondAnchorKey) const
		{
			FVector2D FirstLocation = FVector2D::ZeroVector;
			FVector2D SecondLocation = FVector2D::ZeroVector;
			if (not TryGetLocationXY(FirstAnchorKey, FirstLocation) || not TryGetLocationXY(
				SecondAnchorKey, SecondLocation))
			{
				return -1.f;
			}

			return FVector2D::Distance(FirstLocation, SecondLocation);
		}

		float GetXYDistance(const int32 FirstAnchorIndex, const int32 SecondAnchorIndex) const
		{
			const FWorldCampaignAnchorSnapshot* FirstAnchor = FindAnchor(FirstAnchorIndex);
			const FWorldCampaignAnchorSnapshot* SecondAnchor = FindAnchor(SecondAnchorIndex);
			if (FirstAnchor == nullptr || SecondAnchor == nullptr)
			{
				return -1.f;
			}

			return FVector2D::Distance(FVector2D(FirstAnchor->Location), FVector2D(SecondAnchor->Location));
		}

		void BuildHopDistanceCache(const int32 StartAnchorIndex, TMap<FGuid, int32>& OutHopDistances) const
		{
			OutHopDistances.Reset();
			if (not IsAnchorCached(StartAnchorIndex))
			{
				return;
			}

			struct FHopQueueEntry
			{
				int32 AnchorIndex = INDEX_NONE;
				int32 HopCount = 0;
			};

			TArray<FHopQueueEntry> Queue;
			Queue.Add({StartAnchorIndex, 0});
			OutHopDistances.Add(GetAnchorKey(StartAnchorIndex), 0);

			int32 QueueIndex = 0;
			while (QueueIndex < Queue.Num())
			{
				const FHopQueueEntry QueueEntry = Queue[QueueIndex];
				QueueIndex++;

				const FWorldCampaignAnchorSnapshot* CurrentAnchor = FindAnchor(QueueEntry.AnchorIndex);
				if (CurrentAnchor == nullptr)
				{
					continue;
				}

				for (const int32 NeighborIndex : CurrentAnchor->NeighborAnchorIndices)
				{
					const FGuid NeighborKey = GetAnchorKey(NeighborIndex);
					if (not NeighborKey.IsValid() || OutHopDistances.Contains(NeighborKey))
					{
						continue;
					}

					const int32 NextHops = QueueEntry.HopCount + 1;
					OutHopDistances.Add(NeighborKey, NextHops);
					Queue.Add({NeighborIndex, NextHops});
				}
			}
		}

		int32 GetHopDistance(const int32 StartAnchorIndex, const int32 TargetAnchorIndex) const
		{
			if (not IsAnchorCached(StartAnchorIndex) || not IsAnchorCached(TargetAnchorIndex))
			{
				return INDEX_NONE;
			}

			if (StartAnchorIndex == TargetAnchorIndex)
			{
				return 0;
			}

			TMap<int32, int32>* HopDistances = HopDistanceCacheByStartAnchorIndex.Find(StartAnchorIndex);
			if (HopDistances == nullptr)
			{
				TMap<int32, int32>& NewHopDistances = HopDistanceCacheByStartAnchorIndex.Add(StartAnchorIndex);
				BuildIndexHopDistanceCache(StartAnchorIndex, NewHopDistances);
				HopDistances = &NewHopDistances;
			}

			const int32* HopDistance = HopDistances->Find(TargetAnchorIndex);
			return HopDistance != nullptr ? *HopDistance : INDEX_NONE;
		}

		int32 GetHopDistance(const FGuid& StartAnchorKey, const FGuid& TargetAnchorKey) const
		{
			const int32 StartAnchorIndex = FindAnchorIndexByKey(StartAnchorKey);
			const int32 TargetAnchorIndex = FindAnchorIndexByKey(TargetAnchorKey);
			return GetHopDistance(StartAnchorIndex, TargetAnchorIndex);
		}

		void BuildIndexHopDistanceCache(const int32 StartAnchorIndex, TMap<int32, int32>& OutHopDistances) const
		{
			OutHopDistances.Reset();
			if (not IsAnchorCached(StartAnchorIndex))
			{
				return;
			}

			TArray<int32> Queue;
			Queue.Add(StartAnchorIndex);
			OutHopDistances.Add(StartAnchorIndex, 0);

			for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); QueueIndex++)
			{
				const int32 CurrentIndex = Queue[QueueIndex];
				const FWorldCampaignAnchorSnapshot* CurrentAnchor = FindAnchor(CurrentIndex);
				if (CurrentAnchor == nullptr)
				{
					continue;
				}

				const int32 CurrentHops = OutHopDistances.FindRef(CurrentIndex);
				for (const int32 NeighborIndex : CurrentAnchor->NeighborAnchorIndices)
				{
					if (not IsAnchorCached(NeighborIndex) || OutHopDistances.Contains(NeighborIndex))
					{
						continue;
					}

					OutHopDistances.Add(NeighborIndex, CurrentHops + 1);
					Queue.Add(NeighborIndex);
				}
			}
		}

		bool BuildShortestPathKeys(const FGuid& StartKey, const FGuid& TargetKey, TArray<FGuid>& OutPathKeys) const
		{
			if (not IsAnchorCached(StartKey) || not IsAnchorCached(TargetKey))
			{
				return false;
			}

			if (StartKey == TargetKey)
			{
				OutPathKeys.Reset();
				OutPathKeys.Add(StartKey);
				return true;
			}

			TArray<FGuid> Queue;
			TSet<FGuid> Visited;
			TMap<FGuid, FGuid> PredecessorByKey;
			Queue.Add(StartKey);
			Visited.Add(StartKey);

			for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
			{
				const FGuid CurrentKey = Queue[QueueIndex];
				if (CurrentKey == TargetKey)
				{
					break;
				}

				const FWorldCampaignAnchorSnapshot* CurrentAnchor = FindAnchor(CurrentKey);
				if (CurrentAnchor == nullptr)
				{
					continue;
				}

				for (const FGuid& NeighborKey : CurrentAnchor->NeighborAnchorKeys)
				{
					if (Visited.Contains(NeighborKey) || not IsAnchorCached(NeighborKey))
					{
						continue;
					}

					Visited.Add(NeighborKey);
					PredecessorByKey.Add(NeighborKey, CurrentKey);
					Queue.Add(NeighborKey);
				}
			}

			return BuildShortestPathFromPredecessors(StartKey, TargetKey, Visited, PredecessorByKey, OutPathKeys);
		}

		bool BuildShortestPathFromPredecessors(const FGuid& StartKey, const FGuid& TargetKey,
		                                       const TSet<FGuid>& Visited,
		                                       const TMap<FGuid, FGuid>& PredecessorByKey,
		                                       TArray<FGuid>& OutPathKeys) const
		{
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
				if (PreviousKey == nullptr)
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

		void BuildChokepointScores(bool bUseHQAnchor, const FGuid& OptionalHQAnchorKey)
		{
			DerivedData.ChokepointScoresByAnchorKey.Reset();
			for (const int32 AnchorIndex : SortedAnchorIndices)
			{
				DerivedData.ChokepointScoresByAnchorKey.Add(GetAnchorKey(AnchorIndex), 0.f);
			}

			if (bUseHQAnchor)
			{
				BuildHQChokepointScores(OptionalHQAnchorKey);
				return;
			}

			BuildGlobalChokepointScores();
		}

		void BuildGlobalChokepointScores()
		{
			constexpr int32 ChokepointDegreeOffset = 1;
			for (const int32 AnchorIndex : AnchorIndicesInSnapshotOrder)
			{
				const FGuid AnchorKey = GetAnchorKey(AnchorIndex);
				const int32 ConnectionDegree = GetAnchorConnectionDegree(AnchorIndex);
				const float DegreeScore = static_cast<float>(FMath::Max(0, ConnectionDegree - ChokepointDegreeOffset));
				DerivedData.ChokepointScoresByAnchorKey.Add(AnchorKey, DegreeScore);
			}

			TArray<int32> ShuffledAnchorIndices = AnchorIndicesInSnapshotOrder;
			if (ShuffledAnchorIndices.Num() < 2)
			{
				return;
			}

			FRandomStream RandomStream(State.SeedUsed + ChokepointSeedOffset);
			CampaignGenerationHelper::DeterministicShuffle(ShuffledAnchorIndices, RandomStream);
			AddSampledChokepointPathScores(ShuffledAnchorIndices);
		}

		void AddSampledChokepointPathScores(const TArray<int32>& ShuffledAnchorIndices)
		{
			int32 SampleCount = 0;
			for (int32 StartIndex = 0; StartIndex < ShuffledAnchorIndices.Num() - 1; StartIndex++)
			{
				if (SampleCount >= MaxChokepointPairSamples)
				{
					return;
				}

				for (int32 TargetIndex = StartIndex + 1; TargetIndex < ShuffledAnchorIndices.Num(); TargetIndex++)
				{
					if (SampleCount >= MaxChokepointPairSamples)
					{
						return;
					}

					TArray<FGuid> PathKeys;
					if (BuildShortestPathKeys(GetAnchorKey(ShuffledAnchorIndices[StartIndex]),
					                          GetAnchorKey(ShuffledAnchorIndices[TargetIndex]),
					                          PathKeys))
					{
						AddChokepointPathContribution(PathKeys, DerivedData.ChokepointScoresByAnchorKey);
						SampleCount++;
					}
				}
			}
		}

		void BuildHQChokepointScores(const FGuid& HQAnchorKey)
		{
			for (const int32 TargetAnchorIndex : AnchorIndicesInSnapshotOrder)
			{
				const FGuid TargetAnchorKey = GetAnchorKey(TargetAnchorIndex);
				if (TargetAnchorKey == HQAnchorKey)
				{
					continue;
				}

				TArray<FGuid> PathKeys;
				if (BuildShortestPathKeys(HQAnchorKey, TargetAnchorKey, PathKeys))
				{
					AddChokepointPathContribution(PathKeys, DerivedData.ChokepointScoresByAnchorKey);
				}
			}
		}

		bool ExecutePlacementBacktracking()
		{
			const TArray<ECampaignGenerationStep> StepOrder = {
				ECampaignGenerationStep::PlayerHQPlaced,
				ECampaignGenerationStep::EnemyHQPlaced,
				ECampaignGenerationStep::EnemyWallPlaced,
				ECampaignGenerationStep::EnemyObjectsPlaced,
				ECampaignGenerationStep::NeutralObjectsPlaced,
				ECampaignGenerationStep::MissionsPlaced
			};

			int32 StepIndex = 0;
			while (StepIndex < StepOrder.Num())
			{
				const ECampaignGenerationStep StepToExecute = StepOrder[StepIndex];
				UpdateProgress(TEXT("Executing placement step"), StepToExecute);
				if (ExecuteStepWithTransaction(StepToExecute))
				{
					StepIndex++;
					continue;
				}

				if (not HandleStepFailure(StepToExecute, StepIndex, StepOrder))
				{
					return false;
				}
			}

			GenerationStep = ECampaignGenerationStep::Finished;
			return true;
		}

		bool ExecuteStepWithTransaction(const ECampaignGenerationStep CompletedStep)
		{
			if (not CanExecuteStep(CompletedStep))
			{
				return false;
			}

			if (CompletedStep == ECampaignGenerationStep::EnemyObjectsPlaced
				|| CompletedStep == ECampaignGenerationStep::MissionsPlaced)
			{
				return ExecuteStepWithMicroTransactions(CompletedStep);
			}

			FWorldCampaignPlacementSolverTransaction Transaction;
			Transaction.CompletedStep = CompletedStep;
			Transaction.PreviousPlacementState = State;
			Transaction.PreviousDerivedData = DerivedData;
			if (not ExecuteStep(CompletedStep))
			{
				State = Transaction.PreviousPlacementState;
				DerivedData = Transaction.PreviousDerivedData;
				UpdateMicroPlacementProgressFromTransactions();
				return false;
			}

			Transactions.Add(Transaction);
			GenerationStep = CompletedStep;
			ResetStepAttemptsFrom(CompletedStep);
			ClearBacktrackedFailedStepAttemptToPreserve(CompletedStep);
			return true;
		}

		bool ExecuteStepWithMicroTransactions(const ECampaignGenerationStep CompletedStep)
		{
			if (not ExecuteStep(CompletedStep))
			{
				return false;
			}

			GenerationStep = CompletedStep;
			ResetStepAttemptsFrom(CompletedStep);
			ClearBacktrackedFailedStepAttemptToPreserve(CompletedStep);
			return true;
		}

		bool ExecuteStep(const ECampaignGenerationStep CompletedStep)
		{
			switch (CompletedStep)
			{
			case ECampaignGenerationStep::PlayerHQPlaced:
				return ExecutePlaceHQ();
			case ECampaignGenerationStep::EnemyHQPlaced:
				return ExecutePlaceEnemyHQ();
			case ECampaignGenerationStep::EnemyWallPlaced:
				return ExecutePlaceEnemyWall();
			case ECampaignGenerationStep::EnemyObjectsPlaced:
				return ExecutePlaceEnemyObjects();
			case ECampaignGenerationStep::NeutralObjectsPlaced:
				return ExecutePlaceNeutralObjects();
			case ECampaignGenerationStep::MissionsPlaced:
				return ExecutePlaceMissions();
			default:
				return false;
			}
		}

		bool CanExecuteStep(const ECampaignGenerationStep CompletedStep) const
		{
			return GenerationStep == GetPrerequisiteStep(CompletedStep);
		}

		ECampaignGenerationStep GetPrerequisiteStep(const ECampaignGenerationStep CompletedStep) const
		{
			switch (CompletedStep)
			{
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
			default:
				return ECampaignGenerationStep::NotStarted;
			}
		}

		ECampaignGenerationStep GetNextStep(const ECampaignGenerationStep CurrentStep) const
		{
			switch (CurrentStep)
			{
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

		int32 GetStepAttemptIndex(const ECampaignGenerationStep CompletedStep) const
		{
			const int32* AttemptIndex = StepAttemptIndices.Find(CompletedStep);
			return AttemptIndex ? *AttemptIndex : 0;
		}

		void IncrementStepAttempt(const ECampaignGenerationStep CompletedStep)
		{
			StepAttemptIndices.Add(CompletedStep, GetStepAttemptIndex(CompletedStep) + 1);
		}

		void ResetStepAttemptsFrom(const ECampaignGenerationStep CompletedStep)
		{
			ECampaignGenerationStep StepToReset = GetNextStep(CompletedStep);
			while (StepToReset != ECampaignGenerationStep::Finished)
			{
				if (StepToReset != BacktrackedFailedStepAttemptToPreserve)
				{
					StepAttemptIndices.Remove(StepToReset);
				}

				StepToReset = GetNextStep(StepToReset);
			}
		}

		void InitializeDerivedDataFromSnapshot()
		{
			DerivedData = Snapshot->InitialDerivedData;
			DerivedData.PlayerHQHopDistancesByAnchorKey.Reset();
			DerivedData.EnemyHQHopDistancesByAnchorKey.Reset();
			DerivedData.EnemyItemPlacedCounts.Reset();
			DerivedData.NeutralItemPlacedCounts.Reset();
			DerivedData.MissionPlacedCounts.Reset();

			const bool bNeedsGraphCaches = DerivedData.AnchorConnectionDegreesByAnchorKey.Num() == 0
				|| DerivedData.ChokepointScoresByAnchorKey.Num() == 0;
			if (not bNeedsGraphCaches)
			{
				return;
			}

			BuildAnchorConnectionDegrees();
			BuildChokepointScores(false, FGuid());
		}

		void ClearBacktrackedFailedStepAttemptToPreserve(const ECampaignGenerationStep CompletedStep)
		{
			if (CompletedStep == BacktrackedFailedStepAttemptToPreserve)
			{
				BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;
			}
		}

		EPlacementFailurePolicy GetFailurePolicyForStep(const ECampaignGenerationStep FailedStep) const
		{
			const EPlacementFailurePolicy GlobalPolicy = Snapshot->PlacementFailurePolicy.GlobalPolicy;
			switch (FailedStep)
			{
			case ECampaignGenerationStep::PlayerHQPlaced:
				return Snapshot->PlacementFailurePolicy.PlaceHQPolicy != EPlacementFailurePolicy::NotSet
					       ? Snapshot->PlacementFailurePolicy.PlaceHQPolicy
					       : GlobalPolicy;
			case ECampaignGenerationStep::EnemyHQPlaced:
				return Snapshot->PlacementFailurePolicy.PlaceEnemyHQPolicy != EPlacementFailurePolicy::NotSet
					       ? Snapshot->PlacementFailurePolicy.PlaceEnemyHQPolicy
					       : GlobalPolicy;
			case ECampaignGenerationStep::EnemyWallPlaced:
				return Snapshot->PlacementFailurePolicy.PlaceEnemyWallPolicy != EPlacementFailurePolicy::NotSet
					       ? Snapshot->PlacementFailurePolicy.PlaceEnemyWallPolicy
					       : GlobalPolicy;
			case ECampaignGenerationStep::EnemyObjectsPlaced:
				return Snapshot->PlacementFailurePolicy.PlaceEnemyObjectsPolicy != EPlacementFailurePolicy::NotSet
					       ? Snapshot->PlacementFailurePolicy.PlaceEnemyObjectsPolicy
					       : GlobalPolicy;
			case ECampaignGenerationStep::NeutralObjectsPlaced:
				return Snapshot->PlacementFailurePolicy.PlaceNeutralObjectsPolicy != EPlacementFailurePolicy::NotSet
					       ? Snapshot->PlacementFailurePolicy.PlaceNeutralObjectsPolicy
					       : GlobalPolicy;
			case ECampaignGenerationStep::MissionsPlaced:
				return Snapshot->PlacementFailurePolicy.PlaceMissionsPolicy != EPlacementFailurePolicy::NotSet
					       ? Snapshot->PlacementFailurePolicy.PlaceMissionsPolicy
					       : GlobalPolicy;
			default:
				return GlobalPolicy;
			}
		}

		int32 GetDesiredMicroUndoDepthForFailure(const ECampaignGenerationStep FailedStep,
		                                         const int32 FailedStepAttemptIndex) const
		{
			if (FailedStep != ECampaignGenerationStep::EnemyObjectsPlaced
				&& FailedStep != ECampaignGenerationStep::MissionsPlaced)
			{
				return 0;
			}

			constexpr int32 MinimumWindow = 1;
			constexpr int32 MinimumDepth = 1;
			const int32 Window = FMath::Max(MinimumWindow, Snapshot->PlacementFailurePolicy.EscalationAttempts);
			return MinimumDepth + (FailedStepAttemptIndex / Window);
		}

		int32 GetMicroUndoDepthForFailure(const ECampaignGenerationStep FailedStep,
		                                  const int32 FailedStepAttemptIndex,
		                                  const int32 TrailingMicroTransactions) const
		{
			if (TrailingMicroTransactions <= 0)
			{
				return 0;
			}

			constexpr int32 MinimumDepth = 1;
			const int32 DesiredDepth = GetDesiredMicroUndoDepthForFailure(FailedStep, FailedStepAttemptIndex);
			return FMath::Clamp(DesiredDepth, MinimumDepth, TrailingMicroTransactions);
		}

		bool HandleStepFailure(const ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
		                       const TArray<ECampaignGenerationStep>& StepOrder)
		{
			IncrementStepAttempt(FailedStep);
			TotalAttemptCount++;
			if (Progress.IsValid())
			{
				FScopeLock ProgressLock(&Progress->CriticalSection);
				Progress->LastFailedStep = FailedStep;
			}
			UpdateProgress(TEXT("Handling placement failure"), FailedStep);

			if (GetStepAttemptIndex(FailedStep) > MaxStepAttempts || TotalAttemptCount > MaxTotalAttempts)
			{
				return HandleAttemptLimitFailure(FailedStep, InOutStepIndex, StepOrder);
			}

			const EPlacementFailurePolicy FailurePolicy = GetFailurePolicyForStep(FailedStep);
			if (FailurePolicy == EPlacementFailurePolicy::NotSet)
			{
				AddDebugEvent(FailedStep, TEXT("Generation failed: no failure policy set for step."));
				return false;
			}

			if (TryHandleMicroBacktracking(FailedStep, InOutStepIndex, StepOrder))
			{
				return true;
			}

			const int32 AttemptIndex = GetStepAttemptIndex(FailedStep);
			if (FailurePolicy == EPlacementFailurePolicy::BreakDistanceRules_ThenBackTrack
				&& AttemptIndex <= MaxRelaxationAttempts)
			{
				InOutStepIndex = GetStepIndexOrZero(FailedStep, StepOrder);
				return true;
			}

			return HandleMacroBacktracking(FailedStep, FailurePolicy, AttemptIndex, InOutStepIndex, StepOrder);
		}

		bool HandleAttemptLimitFailure(const ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
		                               const TArray<ECampaignGenerationStep>& StepOrder)
		{
			UpdateProgress(TEXT("Attempt limit reached"), FailedStep);
			if (Snapshot->PlacementFailurePolicy.bEnableTimeoutFailSafe && IsTimeoutFailSafeStep(FailedStep))
			{
				UpdateProgress(TEXT("Applying timeout fail-safe placement"), FailedStep);
				if (TryApplyTimeoutFailSafePlacement(FailedStep))
				{
					GenerationStep = ECampaignGenerationStep::Finished;
					InOutStepIndex = StepOrder.Num();
					return true;
				}
			}

			AddDebugEvent(FailedStep, TEXT("Generation failed: maximum attempts exceeded."));
			return false;
		}

		bool TryHandleMicroBacktracking(const ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
		                                const TArray<ECampaignGenerationStep>& StepOrder)
		{
			UpdateProgress(TEXT("Checking micro backtracking"), FailedStep);
			const int32 TrailingMicroTransactions = CountTrailingMicroTransactionsForStep(FailedStep);
			const bool bCanUndoMicroTransactions = TrailingMicroTransactions > 0
				&& (FailedStep == ECampaignGenerationStep::EnemyObjectsPlaced
					|| FailedStep == ECampaignGenerationStep::MissionsPlaced);
			if (not bCanUndoMicroTransactions)
			{
				return false;
			}

			const int32 AttemptIndex = GetStepAttemptIndex(FailedStep);
			const int32 DesiredMicroUndoDepth = GetDesiredMicroUndoDepthForFailure(FailedStep, AttemptIndex);
			const int32 UndoDepth = GetMicroUndoDepthForFailure(FailedStep, AttemptIndex, TrailingMicroTransactions);
			for (int32 UndoIndex = 0; UndoIndex < UndoDepth; ++UndoIndex)
			{
				UndoLastTransaction();
			}
			UpdateProgress(TEXT("Applied micro backtracking"), FailedStep);

			if (DesiredMicroUndoDepth > TrailingMicroTransactions)
			{
				return false;
			}

			InOutStepIndex = GetStepIndexOrZero(FailedStep, StepOrder);
			return true;
		}

		bool HandleMacroBacktracking(const ECampaignGenerationStep FailedStep,
		                             const EPlacementFailurePolicy FailurePolicy,
		                             const int32 AttemptIndex, int32& InOutStepIndex,
		                             const TArray<ECampaignGenerationStep>& StepOrder)
		{
			UpdateProgress(TEXT("Applying macro backtracking"), FailedStep);
			TArray<ECampaignGenerationStep> EscalationSteps;
			BuildPlacementBacktrackEscalationSteps(FailedStep, EscalationSteps);
			const int32 AdjustedAttemptIndex = FailurePolicy ==
			                                   EPlacementFailurePolicy::BreakDistanceRules_ThenBackTrack
				                                   ? AttemptIndex - MaxRelaxationAttempts
				                                   : AttemptIndex;
			const int32 TransactionsToUndo = FMath::Clamp(AdjustedAttemptIndex - 1, 0, EscalationSteps.Num()) + 1;
			const int32 UndoCount = FMath::Min(TransactionsToUndo, Transactions.Num());
			for (int32 StepCount = 0; StepCount < UndoCount; StepCount++)
			{
				UndoLastTransaction();
			}

			if (TransactionsToUndo > UndoCount)
			{
				bRequiresGameThreadBacktrack = true;
				GameThreadBacktrackFailedStep = FailedStep;
				GameThreadTransactionsToUndo = TransactionsToUndo - UndoCount;
				AddDebugEvent(FailedStep, FString::Printf(
					              TEXT(
						              "Async placement snapshot exhausted; requesting %d game-thread setup transaction undo(s)."),
					              GameThreadTransactionsToUndo));
				return false;
			}

			const ECampaignGenerationStep RetryStep = GetNextStep(GenerationStep);
			IncrementBacktrackedRetryStepAttempt(FailedStep, RetryStep);
			InOutStepIndex = GetStepIndexOrZero(RetryStep, StepOrder);
			return true;
		}

		void IncrementBacktrackedRetryStepAttempt(const ECampaignGenerationStep FailedStep,
		                                          const ECampaignGenerationStep RetryStep)
		{
			if (RetryStep == FailedStep || RetryStep == ECampaignGenerationStep::Finished)
			{
				return;
			}

			// A downstream failure rewound this earlier step; advance it so deterministic selection explores a new candidate.
			BacktrackedFailedStepAttemptToPreserve = FailedStep;
			IncrementStepAttempt(RetryStep);
		}

		static int32 GetStepIndexOrZero(const ECampaignGenerationStep Step,
		                                const TArray<ECampaignGenerationStep>& StepOrder)
		{
			const int32 StepIndex = StepOrder.IndexOfByKey(Step);
			return StepIndex == INDEX_NONE ? 0 : StepIndex;
		}

		static int32 GetPlacementBacktrackEscalationStartIndex(
			const ECampaignGenerationStep FailedStep,
			bool& bOutIncludeNotStarted)
		{
			bOutIncludeNotStarted = false;
			switch (FailedStep)
			{
			case ECampaignGenerationStep::MissionsPlaced:
				return 0;
			case ECampaignGenerationStep::NeutralObjectsPlaced:
				return 1;
			case ECampaignGenerationStep::EnemyObjectsPlaced:
				return 2;
			case ECampaignGenerationStep::EnemyWallPlaced:
				return 3;
			case ECampaignGenerationStep::EnemyHQPlaced:
				return 4;
			case ECampaignGenerationStep::PlayerHQPlaced:
				return 5;
			case ECampaignGenerationStep::ConnectionsCreated:
				bOutIncludeNotStarted = true;
				return 6;
			default:
				return INDEX_NONE;
			}
		}

		void BuildPlacementBacktrackEscalationSteps(const ECampaignGenerationStep FailedStep,
		                                            TArray<ECampaignGenerationStep>& OutSteps) const
		{
			OutSteps.Reset();
			bool bIncludeNotStarted = false;
			const int32 StartIndex = GetPlacementBacktrackEscalationStartIndex(FailedStep, bIncludeNotStarted);
			if (StartIndex == INDEX_NONE)
			{
				return;
			}

			/*
			 * The table is ordered from the closest downstream placement undo toward setup undo.
			 * StartIndex selects the same suffixes that the old branch list appended by hand.
			 */
			const ECampaignGenerationStep OrderedBacktrackSteps[] =
			{
				ECampaignGenerationStep::NeutralObjectsPlaced,
				ECampaignGenerationStep::EnemyObjectsPlaced,
				ECampaignGenerationStep::EnemyWallPlaced,
				ECampaignGenerationStep::EnemyHQPlaced,
				ECampaignGenerationStep::PlayerHQPlaced,
				ECampaignGenerationStep::ConnectionsCreated,
				ECampaignGenerationStep::AnchorPointsGenerated
			};
			for (int32 StepIndex = StartIndex; StepIndex < UE_ARRAY_COUNT(OrderedBacktrackSteps); StepIndex++)
			{
				OutSteps.Add(OrderedBacktrackSteps[StepIndex]);
			}

			if (bIncludeNotStarted)
			{
				OutSteps.Add(ECampaignGenerationStep::NotStarted);
			}
		}

		void UndoLastTransaction()
		{
			if (Transactions.Num() == 0)
			{
				return;
			}

			const FWorldCampaignPlacementSolverTransaction Transaction = Transactions.Pop();
			State = Transaction.PreviousPlacementState;
			DerivedData = Transaction.PreviousDerivedData;
			GenerationStep = GetPrerequisiteStep(Transaction.CompletedStep);
			UpdateMicroPlacementProgressFromTransactions();
		}

		int32 CountTrailingMicroTransactionsForStep(const ECampaignGenerationStep Step) const
		{
			int32 Count = 0;
			for (int32 TransactionIndex = Transactions.Num() - 1; TransactionIndex >= 0; TransactionIndex--)
			{
				const FWorldCampaignPlacementSolverTransaction& Transaction = Transactions[TransactionIndex];
				if (not Transaction.bIsMicroTransaction || Transaction.MicroParentStep != Step)
				{
					break;
				}

				Count++;
			}

			return Count;
		}

		int32 CountMicroTransactionsForStepSinceLastInvalidation(const ECampaignGenerationStep MacroStep) const
		{
			int32 Count = 0;
			for (int32 TransactionIndex = Transactions.Num() - 1; TransactionIndex >= 0; TransactionIndex--)
			{
				const FWorldCampaignPlacementSolverTransaction& Transaction = Transactions[TransactionIndex];
				if (IsStepEarlierThan(Transaction.CompletedStep, MacroStep))
				{
					break;
				}

				if (Transaction.CompletedStep == MacroStep && Transaction.bIsMicroTransaction)
				{
					Count++;
				}
			}

			return Count;
		}

		bool IsStepEarlierThan(const ECampaignGenerationStep StepToCheck,
		                       const ECampaignGenerationStep ReferenceStep) const
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

		void UpdateMicroPlacementProgressFromTransactions()
		{
			EnemyMicroPlacedCount = CountMicroTransactionsForStepSinceLastInvalidation(
				ECampaignGenerationStep::EnemyObjectsPlaced);
			MissionMicroPlacedCount = CountMicroTransactionsForStepSinceLastInvalidation(
				ECampaignGenerationStep::MissionsPlaced);
		}

		bool IsAnchorOccupied(const FGuid& AnchorKey) const
		{
			if (State.PlayerHQAnchorKey == AnchorKey || State.EnemyHQAnchorKey == AnchorKey)
			{
				return true;
			}

			return State.EnemyItemsByAnchorKey.Contains(AnchorKey)
				|| State.NeutralItemsByAnchorKey.Contains(AnchorKey)
				|| State.MissionsByAnchorKey.Contains(AnchorKey);
		}

		bool IsAnchorOccupiedForMission(const FGuid& AnchorKey, const bool bAllowNeutralStacking) const
		{
			if (State.PlayerHQAnchorKey == AnchorKey || State.EnemyHQAnchorKey == AnchorKey)
			{
				return true;
			}

			if (State.EnemyItemsByAnchorKey.Contains(AnchorKey) || State.MissionsByAnchorKey.Contains(AnchorKey))
			{
				return true;
			}

			return not bAllowNeutralStacking && State.NeutralItemsByAnchorKey.Contains(AnchorKey);
		}

		bool HasNeutralTypeAtAnchor(const FGuid& AnchorKey, const EMapNeutralObjectType RequiredNeutralType) const
		{
			const EMapNeutralObjectType* NeutralType = State.NeutralItemsByAnchorKey.Find(AnchorKey);
			return NeutralType != nullptr && *NeutralType == RequiredNeutralType;
		}

		TArray<int32> GetPlayerHQCandidateSource() const
		{
			return Snapshot->PlayerHQPlacementRules.AnchorCandidateIndices.Num() > 0
				       ? Snapshot->PlayerHQPlacementRules.AnchorCandidateIndices
				       : AnchorIndicesInSnapshotOrder;
		}

		TArray<int32> GetEnemyHQCandidateSource() const
		{
			return Snapshot->EnemyHQPlacementRules.AnchorCandidateIndices.Num() > 0
				       ? Snapshot->EnemyHQPlacementRules.AnchorCandidateIndices
				       : AnchorIndicesInSnapshotOrder;
		}

		bool BuildHQAnchorCandidates(const TArray<int32>& CandidateSource, const int32 MinDegree,
		                             const int32 MaxDegree, const FGuid& AnchorKeyToExclude,
		                             TArray<int32>& OutCandidates) const
		{
			OutCandidates.Reset();
			OutCandidates.Reserve(CandidateSource.Num());
			TMap<int32, int32> SourceOrderByAnchorIndex;
			SourceOrderByAnchorIndex.Reserve(CandidateSource.Num());
			for (int32 SourceIndex = 0; SourceIndex < CandidateSource.Num(); SourceIndex++)
			{
				const int32 CandidateIndex = CandidateSource[SourceIndex];
				const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
				if (not IsAnchorCached(CandidateIndex) || CandidateKey == AnchorKeyToExclude)
				{
					continue;
				}

				SourceOrderByAnchorIndex.FindOrAdd(CandidateIndex, SourceIndex);
				const int32 ConnectionDegree = GetAnchorConnectionDegree(CandidateKey);
				if (ConnectionDegree < MinDegree || ConnectionDegree > MaxDegree)
				{
					continue;
				}

				OutCandidates.Add(CandidateIndex);
			}

			OutCandidates.Sort([this, &SourceOrderByAnchorIndex](const int32 Left, const int32 Right)
			{
				const FGuid LeftKey = GetAnchorKey(Left);
				const FGuid RightKey = GetAnchorKey(Right);
				if (LeftKey != RightKey)
				{
					return IsAnchorKeyLess(LeftKey, RightKey);
				}

				return SourceOrderByAnchorIndex.FindRef(Left) < SourceOrderByAnchorIndex.FindRef(Right);
			});
			return OutCandidates.Num() > 0;
		}

		static int32 SelectAnchorCandidateByAttempt(const TArray<int32>& Candidates, const int32 AttemptIndex)
		{
			if (Candidates.Num() == 0)
			{
				return INDEX_NONE;
			}

			return Candidates[AttemptIndex % Candidates.Num()];
		}

		bool ExecutePlaceHQ()
		{
			if (AnchorIndicesInSnapshotOrder.Num() == 0)
			{
				AddDebugEvent(ECampaignGenerationStep::PlayerHQPlaced, TEXT("Player HQ failed: no anchors."));
				return false;
			}

			constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
			const TArray<int32> CandidateSource = GetPlayerHQCandidateSource();
			const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::PlayerHQPlaced);
			TArray<int32> Candidates;
			bool bShouldForcePlacement = not BuildHQAnchorCandidates(
				CandidateSource,
				Snapshot->PlayerHQPlacementRules.MinAnchorDegreeForHQ,
				MaxAnchorDegreeUnlimited,
				FGuid(),
				Candidates);

			TArray<int32> NeighborhoodCandidates;
			if (not bShouldForcePlacement)
			{
				BuildPlayerHQNeighborhoodCandidates(Candidates, NeighborhoodCandidates);
				bShouldForcePlacement = NeighborhoodCandidates.Num() == 0;
			}

			const int32 SelectedAnchorIndex = bShouldForcePlacement
				                                  ? SelectForcedPlayerHQCandidate(CandidateSource, AttemptIndex)
				                                  : SelectAnchorCandidateByAttempt(NeighborhoodCandidates,
				                                                                   AttemptIndex);
			if (not IsAnchorCached(SelectedAnchorIndex))
			{
				AddDebugEvent(ECampaignGenerationStep::PlayerHQPlaced, TEXT("Player HQ failed: invalid selection."));
				return false;
			}

			const FGuid SelectedAnchorKey = GetAnchorKey(SelectedAnchorIndex);
			State.PlayerHQAnchorKey = SelectedAnchorKey;
			State.PlayerHQAnchorIndex = SelectedAnchorIndex;
			BuildHopDistanceCache(SelectedAnchorIndex, DerivedData.PlayerHQHopDistancesByAnchorKey);
			BuildChokepointScores(true, SelectedAnchorKey);
			return true;
		}

		void BuildPlayerHQNeighborhoodCandidates(const TArray<int32>& Candidates, TArray<int32>& OutCandidates) const
		{
			OutCandidates.Reset();
			OutCandidates.Reserve(Candidates.Num());
			for (const int32 CandidateIndex : Candidates)
			{
				TMap<FGuid, int32> HopDistances;
				BuildHopDistanceCache(CandidateIndex, HopDistances);
				int32 NearbyAnchorCount = 0;
				for (const TPair<FGuid, int32>& Pair : HopDistances)
				{
					if (Pair.Value <= 0 || Pair.Value > Snapshot->PlayerHQPlacementRules.MinAnchorsWithinHopsRange)
					{
						continue;
					}

					NearbyAnchorCount++;
				}

				if (NearbyAnchorCount >= Snapshot->PlayerHQPlacementRules.MinAnchorsWithinHops)
				{
					OutCandidates.Add(CandidateIndex);
				}
			}
		}

		int32 SelectForcedPlayerHQCandidate(const TArray<int32>& CandidateSource, const int32 AttemptIndex) const
		{
			constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
			TArray<int32> ForcedCandidates;
			if (not BuildHQAnchorCandidates(CandidateSource, 0, MaxAnchorDegreeUnlimited, FGuid(), ForcedCandidates))
			{
				return INDEX_NONE;
			}

			const int32 SeedOffset = PlayerHQForcePlacementSeedOffset + (AttemptIndex * AttemptSeedMultiplier);
			FRandomStream RandomStream(State.SeedUsed + SeedOffset);
			return ForcedCandidates[RandomStream.RandRange(0, ForcedCandidates.Num() - 1)];
		}

		bool ExecutePlaceEnemyHQ()
		{
			if (not IsAnchorCached(State.PlayerHQAnchorIndex) || AnchorIndicesInSnapshotOrder.Num() == 0)
			{
				AddDebugEvent(ECampaignGenerationStep::EnemyHQPlaced, TEXT("Enemy HQ failed: prerequisites missing."));
				return false;
			}

			constexpr int32 MinAnchorDegreeFloor = 0;
			constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
			const TArray<int32> CandidateSource = GetEnemyHQCandidateSource();
			const int32 MinAnchorDegree = FMath::Max(MinAnchorDegreeFloor,
			                                         Snapshot->EnemyHQPlacementRules.MinAnchorDegree);
			const int32 MaxAnchorDegree = FMath::Max(MinAnchorDegree, Snapshot->EnemyHQPlacementRules.MaxAnchorDegree);
			TArray<int32> Candidates;
			bool bShouldForcePlacement = not BuildHQAnchorCandidates(
				CandidateSource, MinAnchorDegree, MaxAnchorDegree, State.PlayerHQAnchorKey, Candidates);

			const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyHQPlaced);
			if (not bShouldForcePlacement)
			{
				SortEnemyHQCandidatesByPreference(Candidates);
			}

			const int32 SelectedAnchorIndex = bShouldForcePlacement
				                                  ? SelectForcedEnemyHQCandidate(CandidateSource, AttemptIndex,
				                                                                 MinAnchorDegreeFloor,
				                                                                 MaxAnchorDegreeUnlimited)
				                                  : SelectAnchorCandidateByAttempt(Candidates, AttemptIndex);
			if (not IsAnchorCached(SelectedAnchorIndex))
			{
				AddDebugEvent(ECampaignGenerationStep::EnemyHQPlaced, TEXT("Enemy HQ failed: invalid selection."));
				return false;
			}

			const FGuid SelectedAnchorKey = GetAnchorKey(SelectedAnchorIndex);
			State.EnemyHQAnchorKey = SelectedAnchorKey;
			State.EnemyHQAnchorIndex = SelectedAnchorIndex;
			BuildHopDistanceCache(SelectedAnchorIndex, DerivedData.EnemyHQHopDistancesByAnchorKey);
			return true;
		}

		void SortEnemyHQCandidatesByPreference(TArray<int32>& InOutCandidates) const
		{
			if (Snapshot->EnemyHQPlacementRules.AnchorDegreePreference == ETopologySearchStrategy::NotSet)
			{
				return;
			}

			const bool bPreferMax = Snapshot->EnemyHQPlacementRules.AnchorDegreePreference ==
				ETopologySearchStrategy::PreferMax;
			TMap<int32, int32> SourceOrderByAnchorIndex;
			SourceOrderByAnchorIndex.Reserve(InOutCandidates.Num());
			for (int32 CandidateIndex = 0; CandidateIndex < InOutCandidates.Num(); CandidateIndex++)
			{
				SourceOrderByAnchorIndex.FindOrAdd(InOutCandidates[CandidateIndex], CandidateIndex);
			}

			InOutCandidates.Sort([this, bPreferMax, &SourceOrderByAnchorIndex](const int32 Left, const int32 Right)
			{
				const int32 LeftDegree = GetAnchorConnectionDegree(GetAnchorKey(Left));
				const int32 RightDegree = GetAnchorConnectionDegree(GetAnchorKey(Right));
				if (LeftDegree != RightDegree)
				{
					return bPreferMax ? LeftDegree > RightDegree : LeftDegree < RightDegree;
				}

				const FGuid LeftKey = GetAnchorKey(Left);
				const FGuid RightKey = GetAnchorKey(Right);
				if (LeftKey != RightKey)
				{
					return IsAnchorKeyLess(LeftKey, RightKey);
				}

				return SourceOrderByAnchorIndex.FindRef(Left) < SourceOrderByAnchorIndex.FindRef(Right);
			});
		}

		int32 SelectForcedEnemyHQCandidate(const TArray<int32>& CandidateSource, const int32 AttemptIndex,
		                                   const int32 MinDegree, const int32 MaxDegree) const
		{
			TArray<int32> ForcedCandidates;
			if (not BuildHQAnchorCandidates(CandidateSource, MinDegree, MaxDegree, State.PlayerHQAnchorKey,
			                                ForcedCandidates))
			{
				return INDEX_NONE;
			}

			const int32 SeedOffset = EnemyHQForcePlacementSeedOffset + (AttemptIndex * AttemptSeedMultiplier);
			FRandomStream RandomStream(State.SeedUsed + SeedOffset);
			return ForcedCandidates[RandomStream.RandRange(0, ForcedCandidates.Num() - 1)];
		}

		bool ExecutePlaceEnemyWall()
		{
			if (not IsAnchorCached(State.EnemyHQAnchorIndex) || Snapshot->EnemyWallPlacementRules.AnchorCandidateIndices.
			                                                            Num() == 0)
			{
				AddDebugEvent(ECampaignGenerationStep::EnemyWallPlaced,
				              TEXT("Enemy wall failed: prerequisites missing."));
				return false;
			}

			const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyWallPlaced);
			FWorldCampaignKeyPlacementCandidate SelectedCandidate;
			if (not TrySelectEnemyWallPlacementCandidate(AttemptIndex, SelectedCandidate))
			{
				AddDebugEvent(ECampaignGenerationStep::EnemyWallPlaced, TEXT("Enemy wall failed: no candidates."));
				return false;
			}

			State.EnemyItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, EMapEnemyItem::EnemyWall);
			IncrementEnemyPlacedCount(EMapEnemyItem::EnemyWall);
			return true;
		}

		bool TrySelectEnemyWallPlacementCandidate(const int32 AttemptIndex,
		                                          FWorldCampaignKeyPlacementCandidate& OutCandidate) const
		{
			TArray<FWorldCampaignKeyPlacementCandidate> Candidates;
			for (int32 SourceIndex = 0;
			     SourceIndex < Snapshot->EnemyWallPlacementRules.AnchorCandidateIndices.Num();
			     SourceIndex++)
			{
				const int32 CandidateIndex = Snapshot->EnemyWallPlacementRules.AnchorCandidateIndices[SourceIndex];
				const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
				if (not IsAnchorCached(CandidateIndex) || IsAnchorOccupied(CandidateKey))
				{
					continue;
				}

				FWorldCampaignKeyPlacementCandidate Candidate;
				Candidate.AnchorKey = CandidateKey;
				Candidate.AnchorIndex = CandidateIndex;
				Candidate.CandidateOrder = SourceIndex;
				Candidate.Score = GetEnemyWallPreferenceScore(
					Snapshot->EnemyWallPlacementRules.Preference,
					GetAnchorConnectionDegree(CandidateKey),
					DerivedData.ChokepointScoresByAnchorKey.FindRef(CandidateKey));
				Candidates.Add(Candidate);
			}

			if (Candidates.Num() == 0)
			{
				return false;
			}

			SortPlacementCandidates(Candidates);
			OutCandidate = Candidates[AttemptIndex % Candidates.Num()];
			return true;
		}

		void IncrementEnemyPlacedCount(const EMapEnemyItem EnemyType)
		{
			const int32 ExistingCount = DerivedData.EnemyItemPlacedCounts.FindRef(EnemyType);
			DerivedData.EnemyItemPlacedCounts.Add(EnemyType, ExistingCount + 1);
		}

		void IncrementNeutralPlacedCount(const EMapNeutralObjectType NeutralType)
		{
			const int32 ExistingCount = DerivedData.NeutralItemPlacedCounts.FindRef(NeutralType);
			DerivedData.NeutralItemPlacedCounts.Add(NeutralType, ExistingCount + 1);
		}

		void IncrementMissionPlacedCount(const EMapMission MissionType)
		{
			const int32 ExistingCount = DerivedData.MissionPlacedCounts.FindRef(MissionType);
			DerivedData.MissionPlacedCounts.Add(MissionType, ExistingCount + 1);
		}

		static void SortPlacementCandidates(TArray<FWorldCampaignKeyPlacementCandidate>& InOutCandidates)
		{
			InOutCandidates.Sort([](const FWorldCampaignKeyPlacementCandidate& Left,
			                        const FWorldCampaignKeyPlacementCandidate& Right)
			{
				if (Left.Score != Right.Score)
				{
					return Left.Score > Right.Score;
				}

				if (Left.AnchorKey != Right.AnchorKey)
				{
					return IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
				}

				return Left.CandidateOrder < Right.CandidateOrder;
			});
		}

		bool ExecutePlaceEnemyObjects()
		{
			UpdateProgress(TEXT("Building enemy placement plan"), ECampaignGenerationStep::EnemyObjectsPlaced);
			const int32 RequiredEnemyItems = GetRequiredEnemyItemCount(Snapshot->CountAndDifficultyTuning);
			if (RequiredEnemyItems <= NoRequiredItems)
			{
				return true;
			}

			if (not ValidateEnemyObjectPlacementPrerequisites())
			{
				return false;
			}

			const TMap<EMapEnemyItem, int32> RequiredEnemyCounts = BuildRequiredEnemyItemCounts(
				Snapshot->CountAndDifficultyTuning);
			const TArray<EMapEnemyItem> EnemyPlacementPlan = BuildEnemyPlacementPlan(RequiredEnemyCounts);
			UpdateMicroPlacementProgressFromTransactions();
			const int32 AlreadyPlacedCount = FMath::Clamp(EnemyMicroPlacedCount, 0, EnemyPlacementPlan.Num());
			for (int32 MicroIndex = AlreadyPlacedCount; MicroIndex < EnemyPlacementPlan.Num(); MicroIndex++)
			{
				UpdateProgress(FString::Printf(
					               TEXT("Placing enemy item %d/%d"),
					               MicroIndex + 1,
					               EnemyPlacementPlan.Num()),
				               ECampaignGenerationStep::EnemyObjectsPlaced);
				FWorldCampaignPlacementSolverTransaction MicroTransaction;
				if (not ExecutePlaceSingleEnemyObject(EnemyPlacementPlan[MicroIndex], MicroIndex, MicroTransaction))
				{
					return false;
				}

				Transactions.Add(MicroTransaction);
				EnemyMicroPlacedCount = MicroIndex + 1;
			}

			return true;
		}

		bool ValidateEnemyObjectPlacementPrerequisites() const
		{
			return IsAnchorCached(State.EnemyHQAnchorIndex)
				&& IsAnchorCached(State.PlayerHQAnchorIndex)
				&& AnchorIndicesInSnapshotOrder.Num() > 0
				&& DerivedData.EnemyHQHopDistancesByAnchorKey.Num() > 0;
		}

		bool ExecutePlaceSingleEnemyObject(const EMapEnemyItem EnemyTypeToPlace, const int32 MicroIndexWithinParent,
		                                   FWorldCampaignPlacementSolverTransaction& OutMicroTransaction)
		{
			OutMicroTransaction.PreviousPlacementState = State;
			OutMicroTransaction.PreviousDerivedData = DerivedData;

			FGuid SelectedAnchorKey;
			if (not TrySelectSingleEnemyPlacement(EnemyTypeToPlace, SelectedAnchorKey))
			{
				return false;
			}

			OutMicroTransaction.CompletedStep = ECampaignGenerationStep::EnemyObjectsPlaced;
			OutMicroTransaction.bIsMicroTransaction = true;
			OutMicroTransaction.MicroParentStep = ECampaignGenerationStep::EnemyObjectsPlaced;
			OutMicroTransaction.MicroAnchorKey = SelectedAnchorKey;
			OutMicroTransaction.MicroItemType = EMapItemType::EnemyItem;
			OutMicroTransaction.MicroEnemyItemType = EnemyTypeToPlace;
			OutMicroTransaction.MicroIndexWithinParent = MicroIndexWithinParent;
			return OutMicroTransaction.MicroAnchorKey.IsValid();
		}

		bool TrySelectSingleEnemyPlacement(const EMapEnemyItem EnemyTypeToPlace, FGuid& OutSelectedAnchorKey)
		{
			OutSelectedAnchorKey = FGuid();
			constexpr int32 SinglePlacementCount = 1;
			const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyObjectsPlaced);
			const FRuleRelaxationState RelaxationState = GetRelaxationState(
				GetFailurePolicyForStep(ECampaignGenerationStep::EnemyObjectsPlaced),
				AttemptIndex);
			const int32 SafeZoneMaxHops = RelaxationState.bRelaxDistance
				                              ? 0
				                              : Snapshot->PlayerHQPlacementRules.SafeZoneMaxHops;
			const FEnemyItemRuleset* RulesetPtr = Snapshot->EnemyPlacementRules.RulesByEnemyItem.Find(EnemyTypeToPlace);
			const FEnemyItemRuleset Ruleset = RulesetPtr ? *RulesetPtr : FEnemyItemRuleset();
			return TryPlaceEnemyItemsForType(
				EnemyTypeToPlace,
				SinglePlacementCount,
				Ruleset,
				AttemptIndex,
				RelaxationState,
				SafeZoneMaxHops,
				OutSelectedAnchorKey);
		}

		bool TryPlaceEnemyItemsForType(const EMapEnemyItem EnemyType, const int32 RequiredCount,
		                               const FEnemyItemRuleset& Ruleset, const int32 AttemptIndex,
		                               const FRuleRelaxationState& RelaxationState,
		                               const int32 SafeZoneMaxHops, FGuid& OutLastSelectedAnchorKey)
		{
			OutLastSelectedAnchorKey = FGuid();
			for (int32 PlacementIndex = 0; PlacementIndex < RequiredCount; PlacementIndex++)
			{
				const int32 ExistingCount = DerivedData.EnemyItemPlacedCounts.FindRef(EnemyType);
				FEnemyItemPlacementRules EffectiveRules = BuildEffectiveEnemyRules(Ruleset, ExistingCount);
				ApplyEnemyRuleRelaxation(RelaxationState, EffectiveRules);

				FWorldCampaignKeyPlacementCandidate SelectedCandidate;
				if (not TrySelectEnemyPlacementCandidate(EffectiveRules, EnemyType, AttemptIndex, ExistingCount,
				                                         SafeZoneMaxHops, SelectedCandidate))
				{
					return false;
				}

				State.EnemyItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, EnemyType);
				IncrementEnemyPlacedCount(EnemyType);
				OutLastSelectedAnchorKey = SelectedCandidate.AnchorKey;
			}

			return true;
		}

		static FEnemyItemPlacementRules BuildEffectiveEnemyRules(const FEnemyItemRuleset& Ruleset,
		                                                         const int32 ExistingCount)
		{
			FEnemyItemPlacementRules EffectiveRules = Ruleset.BaseRules;
			if (Ruleset.VariantMode != EEnemyRuleVariantSelectionMode::CycleByPlacementIndex)
			{
				return EffectiveRules;
			}

			TArray<FEnemyItemPlacementVariant> EnabledVariants;
			for (const FEnemyItemPlacementVariant& Variant : Ruleset.Variants)
			{
				if (Variant.bEnabled)
				{
					EnabledVariants.Add(Variant);
				}
			}

			if (EnabledVariants.Num() == 0)
			{
				return EffectiveRules;
			}

			const FEnemyItemPlacementVariant& SelectedVariant = EnabledVariants[ExistingCount % EnabledVariants.Num()];
			return SelectedVariant.bOverrideRules ? SelectedVariant.OverrideRules : EffectiveRules;
		}

		static void ApplyEnemyRuleRelaxation(const FRuleRelaxationState& RelaxationState,
		                                     FEnemyItemPlacementRules& InOutRules)
		{
			if (RelaxationState.bRelaxDistance)
			{
				InOutRules.EnemyHQSpacing.MinHopsFromEnemyHQ = 0;
				InOutRules.EnemyHQSpacing.MaxHopsFromEnemyHQ = RelaxedHopDistanceMax;
			}

			if (RelaxationState.bRelaxSpacing)
			{
				InOutRules.ItemSpacing.MinEnemySeparationHopsOtherType = 0;
				InOutRules.ItemSpacing.MinEnemySeparationHopsSameType = 0;
			}

			if (RelaxationState.bRelaxPreference)
			{
				InOutRules.EnemyHQSpacing.Preference = EEnemyTopologySearchStrategy::None;
			}
		}

		bool TrySelectEnemyPlacementCandidate(const FEnemyItemPlacementRules& EffectiveRules,
		                                      const EMapEnemyItem EnemyType, const int32 AttemptIndex,
		                                      const int32 ExistingCount, const int32 SafeZoneMaxHops,
		                                      FWorldCampaignKeyPlacementCandidate& OutCandidate) const
		{
			UpdateProgress(FString::Printf(
				               TEXT("Evaluating enemy %d candidates from %d anchors"),
				               static_cast<int32>(EnemyType),
				               AnchorIndicesInSnapshotOrder.Num()),
			               ECampaignGenerationStep::EnemyObjectsPlaced);
			TArray<FWorldCampaignKeyPlacementCandidate> Candidates;
			for (int32 SourceIndex = 0; SourceIndex < AnchorIndicesInSnapshotOrder.Num(); SourceIndex++)
			{
				const int32 CandidateIndex = AnchorIndicesInSnapshotOrder[SourceIndex];
				const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
				if (not GetCanUseEnemyCandidate(CandidateIndex, EffectiveRules, EnemyType, SafeZoneMaxHops))
				{
					continue;
				}

				const int32 HopDistanceFromEnemyHQ = GetCachedHopDistance(
					DerivedData.EnemyHQHopDistancesByAnchorKey,
					CandidateKey);
				FWorldCampaignKeyPlacementCandidate Candidate;
				Candidate.AnchorKey = CandidateKey;
				Candidate.AnchorIndex = CandidateIndex;
				Candidate.CandidateOrder = SourceIndex;
				Candidate.Score = GetEnemyPreferenceScore(
					EffectiveRules.EnemyHQSpacing.Preference,
					GetAnchorConnectionDegree(CandidateKey),
					DerivedData.ChokepointScoresByAnchorKey.FindRef(CandidateKey),
					HopDistanceFromEnemyHQ,
					EffectiveRules.EnemyHQSpacing.MinHopsFromEnemyHQ,
					EffectiveRules.EnemyHQSpacing.MaxHopsFromEnemyHQ);
				Candidate.HopDistanceFromHQ = HopDistanceFromEnemyHQ;
				Candidates.Add(Candidate);
			}

			if (Candidates.Num() == 0)
			{
				return false;
			}

			SortPlacementCandidates(Candidates);
			const int32 CandidateIndex = (AttemptIndex + ExistingCount) % Candidates.Num();
			OutCandidate = Candidates[CandidateIndex];
			return true;
		}

		bool GetCanUseEnemyCandidate(const int32 CandidateIndex, const FEnemyItemPlacementRules& EffectiveRules,
		                             const EMapEnemyItem EnemyType, const int32 SafeZoneMaxHops) const
		{
			const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
			if (IsAnchorOccupied(CandidateKey))
			{
				return false;
			}

			const int32 HopDistanceFromEnemyHQ = GetCachedHopDistance(
				DerivedData.EnemyHQHopDistancesByAnchorKey,
				CandidateKey);
			const int32 MinHopsFromEnemyHQ = EffectiveRules.EnemyHQSpacing.MinHopsFromEnemyHQ;
			const int32 MaxHopsFromEnemyHQ = FMath::Max(MinHopsFromEnemyHQ,
			                                            EffectiveRules.EnemyHQSpacing.MaxHopsFromEnemyHQ);
			if (HopDistanceFromEnemyHQ == INDEX_NONE
				|| HopDistanceFromEnemyHQ < MinHopsFromEnemyHQ
				|| HopDistanceFromEnemyHQ > MaxHopsFromEnemyHQ)
			{
				return false;
			}

			if (SafeZoneMaxHops > 0)
			{
				const int32 HopDistanceFromPlayerHQ = GetCachedHopDistance(
					DerivedData.PlayerHQHopDistancesByAnchorKey,
					CandidateKey);
				if (HopDistanceFromPlayerHQ != INDEX_NONE && HopDistanceFromPlayerHQ <= SafeZoneMaxHops)
				{
					return false;
				}
			}

			return PassesEnemySpacingRules(EffectiveRules, CandidateIndex, EnemyType);
		}

		bool PassesEnemySpacingRules(const FEnemyItemPlacementRules& EffectiveRules,
		                             const int32 CandidateIndex, const EMapEnemyItem EnemyType) const
		{
			const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
			TArray<FGuid> SortedEnemyAnchorKeys;
			State.EnemyItemsByAnchorKey.GetKeys(SortedEnemyAnchorKeys);
			SortAnchorKeys(SortedEnemyAnchorKeys);
			for (const FGuid& ExistingAnchorKey : SortedEnemyAnchorKeys)
			{
				const EMapEnemyItem ExistingEnemyType = State.EnemyItemsByAnchorKey.FindRef(ExistingAnchorKey);
				const int32 ExistingAnchorIndex = FindAnchorIndexByKey(ExistingAnchorKey);
				const int32 HopDistance = GetHopDistance(ExistingAnchorIndex, CandidateIndex);
				if (HopDistance == INDEX_NONE)
				{
					return false;
				}

				const int32 RequiredMinHops = ExistingEnemyType == EnemyType
					                              ? EffectiveRules.ItemSpacing.MinEnemySeparationHopsSameType
					                              : EffectiveRules.ItemSpacing.MinEnemySeparationHopsOtherType;
				if (HopDistance < RequiredMinHops)
				{
					return false;
				}
			}

			return true;
		}

		bool ExecutePlaceNeutralObjects()
		{
			UpdateProgress(TEXT("Building neutral placement plan"), ECampaignGenerationStep::NeutralObjectsPlaced);
			const int32 RequiredNeutralItems = GetRequiredNeutralItemCount(Snapshot->CountAndDifficultyTuning);
			if (RequiredNeutralItems <= NoRequiredItems)
			{
				return true;
			}

			if (not ValidateNeutralObjectPlacementPrerequisites())
			{
				return false;
			}

			const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::NeutralObjectsPlaced);
			const FRuleRelaxationState RelaxationState = GetRelaxationState(
				GetFailurePolicyForStep(ECampaignGenerationStep::NeutralObjectsPlaced),
				AttemptIndex);
			const TMap<EMapNeutralObjectType, int32> RequiredNeutralCounts = BuildRequiredNeutralItemCounts(
				Snapshot->CountAndDifficultyTuning);
			const TArray<EMapNeutralObjectType> NeutralTypes = GetSortedNeutralTypes(RequiredNeutralCounts);
			for (const EMapNeutralObjectType NeutralType : NeutralTypes)
			{
				UpdateProgress(FString::Printf(
					               TEXT("Placing neutral type %d"),
					               static_cast<int32>(NeutralType)),
				               ECampaignGenerationStep::NeutralObjectsPlaced);
				if (not TryPlaceNeutralItemsForType(NeutralType, RequiredNeutralCounts.FindRef(NeutralType),
				                                    AttemptIndex, RelaxationState,
				                                    Snapshot->NeutralItemPlacementRules))
				{
					return false;
				}
			}

			return true;
		}

		bool ValidateNeutralObjectPlacementPrerequisites() const
		{
			return IsAnchorCached(State.PlayerHQAnchorIndex)
				&& AnchorIndicesInSnapshotOrder.Num() > 0
				&& DerivedData.PlayerHQHopDistancesByAnchorKey.Num() > 0;
		}

		bool TryPlaceNeutralItemsForType(const EMapNeutralObjectType NeutralType, const int32 RequiredCount,
		                                 const int32 AttemptIndex, const FRuleRelaxationState& RelaxationState,
		                                 const FNeutralItemPlacementRules& BaseRules)
		{
			for (int32 PlacementIndex = 0; PlacementIndex < RequiredCount; PlacementIndex++)
			{
				const int32 ExistingCount = DerivedData.NeutralItemPlacedCounts.FindRef(NeutralType);
				FNeutralItemPlacementRules EffectiveRules = BaseRules;
				ApplyNeutralRuleRelaxation(RelaxationState, EffectiveRules);

				FWorldCampaignKeyPlacementCandidate SelectedCandidate;
				if (not TrySelectNeutralPlacementCandidate(NeutralType, EffectiveRules, AttemptIndex,
				                                           ExistingCount, SelectedCandidate))
				{
					return false;
				}

				State.NeutralItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, NeutralType);
				IncrementNeutralPlacedCount(NeutralType);
			}

			return true;
		}

		static void ApplyNeutralRuleRelaxation(const FRuleRelaxationState& RelaxationState,
		                                       FNeutralItemPlacementRules& InOutRules)
		{
			if (RelaxationState.bRelaxDistance)
			{
				InOutRules.MinHopsFromHQ = 0;
				InOutRules.MaxHopsFromHQ = RelaxedHopDistanceMax;
			}

			if (RelaxationState.bRelaxSpacing)
			{
				InOutRules.MinHopsFromOtherNeutralItems = 0;
				InOutRules.MaxHopsFromOtherNeutralItems = RelaxedHopDistanceMax;
			}

			if (RelaxationState.bRelaxPreference)
			{
				InOutRules.Preference = ETopologySearchStrategy::NotSet;
			}
		}

		struct FNeutralCandidateRejectionStats
		{
			int32 OccupiedRejectCount = 0;
			int32 NoHopRejectCount = 0;
			int32 HopBandRejectCount = 0;
			int32 SpacingRejectCount = 0;
		};

		bool TrySelectNeutralPlacementCandidate(const EMapNeutralObjectType NeutralType,
		                                        const FNeutralItemPlacementRules& PlacementRules,
		                                        const int32 AttemptIndex, const int32 ExistingCount,
		                                        FWorldCampaignKeyPlacementCandidate& OutCandidate) const
		{
			UpdateProgress(FString::Printf(
				               TEXT("Evaluating neutral %d candidates from %d anchors"),
				               static_cast<int32>(NeutralType),
				               AnchorIndicesInSnapshotOrder.Num()),
				               ECampaignGenerationStep::NeutralObjectsPlaced);
			TArray<FWorldCampaignKeyPlacementCandidate> Candidates;
			FNeutralCandidateRejectionStats RejectionStats;
			const int32 MaxHopsFromHQClamped = FMath::Max(PlacementRules.MinHopsFromHQ,
			                                              PlacementRules.MaxHopsFromHQ);
			for (int32 SourceIndex = 0; SourceIndex < AnchorIndicesInSnapshotOrder.Num(); SourceIndex++)
			{
				const int32 CandidateIndex = AnchorIndicesInSnapshotOrder[SourceIndex];
				FWorldCampaignKeyPlacementCandidate Candidate;
				if (not TryBuildNeutralPlacementCandidate(PlacementRules, CandidateIndex, MaxHopsFromHQClamped,
				                                          RejectionStats, Candidate))
				{
					continue;
				}

				Candidate.CandidateOrder = SourceIndex;
				Candidates.Add(Candidate);
			}

			/*
			 * Keep the rejection counters together with the candidate scan so watchdog logs can explain
			 * neutral-placement stalls without the game thread reading partial worker data directly.
			 */
			UpdateNeutralCandidateProgress(NeutralType, AnchorIndicesInSnapshotOrder.Num(), Candidates.Num(),
			                               RejectionStats.OccupiedRejectCount, RejectionStats.NoHopRejectCount,
			                               RejectionStats.HopBandRejectCount, RejectionStats.SpacingRejectCount);

			if (Candidates.Num() == 0)
			{
				return false;
			}

			SortPlacementCandidates(Candidates);
			const int32 CandidateIndex = (AttemptIndex + ExistingCount) % Candidates.Num();
			OutCandidate = Candidates[CandidateIndex];
			return true;
		}

		bool TryBuildNeutralPlacementCandidate(const FNeutralItemPlacementRules& PlacementRules,
		                                       const int32 CandidateIndex,
		                                       const int32 MaxHopsFromHQClamped,
		                                       FNeutralCandidateRejectionStats& InOutRejectionStats,
		                                       FWorldCampaignKeyPlacementCandidate& OutCandidate) const
		{
			const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
			if (IsAnchorOccupied(CandidateKey))
			{
				InOutRejectionStats.OccupiedRejectCount++;
				return false;
			}

			const int32 HopDistanceFromHQ = GetCachedHopDistance(
				DerivedData.PlayerHQHopDistancesByAnchorKey,
				CandidateKey);
			if (HopDistanceFromHQ == INDEX_NONE)
			{
				InOutRejectionStats.NoHopRejectCount++;
				return false;
			}

			if (HopDistanceFromHQ < PlacementRules.MinHopsFromHQ || HopDistanceFromHQ > MaxHopsFromHQClamped)
			{
				InOutRejectionStats.HopBandRejectCount++;
				return false;
			}

			if (not PassesNeutralSpacingRules(PlacementRules, CandidateIndex))
			{
				InOutRejectionStats.SpacingRejectCount++;
				return false;
			}

			OutCandidate.AnchorKey = CandidateKey;
			OutCandidate.AnchorIndex = CandidateIndex;
			OutCandidate.Score = GetTopologyPreferenceScore(PlacementRules.Preference,
			                                                static_cast<float>(HopDistanceFromHQ));
			OutCandidate.HopDistanceFromHQ = HopDistanceFromHQ;
			return true;
		}

		bool PassesNeutralDistanceRules(const FNeutralItemPlacementRules& PlacementRules,
		                                const FGuid& CandidateKey) const
		{
			const int32 HopDistanceFromHQ = GetCachedHopDistance(
				DerivedData.PlayerHQHopDistancesByAnchorKey,
				CandidateKey);
			if (HopDistanceFromHQ == INDEX_NONE)
			{
				return false;
			}

			const int32 MaxHopsFromHQClamped = FMath::Max(PlacementRules.MinHopsFromHQ, PlacementRules.MaxHopsFromHQ);
			return HopDistanceFromHQ >= PlacementRules.MinHopsFromHQ && HopDistanceFromHQ <= MaxHopsFromHQClamped;
		}

		bool PassesNeutralSpacingRules(const FNeutralItemPlacementRules& PlacementRules,
		                               const int32 CandidateIndex) const
		{
			const int32 MaxHopsFromNeutralClamped = FMath::Max(PlacementRules.MinHopsFromOtherNeutralItems,
			                                                   PlacementRules.MaxHopsFromOtherNeutralItems);
			TArray<FGuid> SortedNeutralAnchorKeys;
			State.NeutralItemsByAnchorKey.GetKeys(SortedNeutralAnchorKeys);
			SortAnchorKeys(SortedNeutralAnchorKeys);
			for (const FGuid& ExistingAnchorKey : SortedNeutralAnchorKeys)
			{
				const int32 ExistingAnchorIndex = FindAnchorIndexByKey(ExistingAnchorKey);
				const int32 HopDistance = GetHopDistance(ExistingAnchorIndex, CandidateIndex);
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

		struct FMissionSelectionSettings
		{
			bool bOverridePlacementWithArray = false;
			TArray<int32> CandidateSourceIndices;
			int32 MinConnections = 0;
			int32 MaxConnections = 0;
			ETopologySearchStrategy ConnectionPreference = ETopologySearchStrategy::NotSet;
			int32 MinHopsFromPlayerHQ = 0;
			int32 MaxHopsFromPlayerHQ = TNumericLimits<int32>::Max();
			ETopologySearchStrategy HopsPreference = ETopologySearchStrategy::NotSet;
		};

		bool ExecutePlaceMissions()
		{
			UpdateProgress(TEXT("Building mission placement plan"), ECampaignGenerationStep::MissionsPlaced);
			const TArray<EMapMission> MissionPlacementPlan = BuildMissionPlacementPlanFiltered();
			if (MissionPlacementPlan.Num() <= NoRequiredItems)
			{
				return true;
			}

			if (not ValidateMissionPlacementPrerequisites())
			{
				return false;
			}

			UpdateMicroPlacementProgressFromTransactions();
			const int32 AlreadyPlacedCount = FMath::Clamp(MissionMicroPlacedCount, 0, MissionPlacementPlan.Num());
			for (int32 MicroIndex = AlreadyPlacedCount; MicroIndex < MissionPlacementPlan.Num(); MicroIndex++)
			{
				UpdateProgress(FString::Printf(
					               TEXT("Placing mission %d/%d"),
					               MicroIndex + 1,
					               MissionPlacementPlan.Num()),
				               ECampaignGenerationStep::MissionsPlaced);
				FWorldCampaignPlacementSolverTransaction MicroTransaction;
				if (not ExecutePlaceSingleMission(MissionPlacementPlan[MicroIndex], MicroIndex, MicroTransaction))
				{
					return false;
				}

				Transactions.Add(MicroTransaction);
				MissionMicroPlacedCount = MicroIndex + 1;
			}

			return true;
		}

		TArray<EMapMission> BuildMissionPlacementPlanFiltered() const
		{
			TArray<EMapMission> PlacementPlan;
			TArray<EMapMission> MissionTypes;
			Snapshot->MissionPlacementRules.RulesByMission.GetKeys(MissionTypes);
			MissionTypes.Sort([](const EMapMission Left, const EMapMission Right)
			{
				return static_cast<uint8>(Left) < static_cast<uint8>(Right);
			});

			for (const EMapMission MissionType : MissionTypes)
			{
				if (Snapshot->ExcludedMissionsFromMapPlacement.Contains(MissionType))
				{
					continue;
				}

				PlacementPlan.Add(MissionType);
			}

			return PlacementPlan;
		}

		bool ValidateMissionPlacementPrerequisites() const
		{
			return IsAnchorCached(State.PlayerHQAnchorIndex)
				&& AnchorIndicesInSnapshotOrder.Num() > 0
				&& DerivedData.PlayerHQHopDistancesByAnchorKey.Num() > 0;
		}

		bool ExecutePlaceSingleMission(const EMapMission MissionTypeToPlace, const int32 MicroIndexWithinParent,
		                               FWorldCampaignPlacementSolverTransaction& OutMicroTransaction)
		{
			OutMicroTransaction.PreviousPlacementState = State;
			OutMicroTransaction.PreviousDerivedData = DerivedData;

			FWorldCampaignKeyPlacementCandidate SelectedCandidate;
			if (not TryPlaceMissionForType(MissionTypeToPlace, MicroIndexWithinParent, SelectedCandidate))
			{
				return false;
			}

			OutMicroTransaction.CompletedStep = ECampaignGenerationStep::MissionsPlaced;
			OutMicroTransaction.bIsMicroTransaction = true;
			OutMicroTransaction.MicroParentStep = ECampaignGenerationStep::MissionsPlaced;
			OutMicroTransaction.MicroAnchorKey = SelectedCandidate.AnchorKey;
			OutMicroTransaction.MicroItemType = EMapItemType::Mission;
			OutMicroTransaction.MicroMissionType = MissionTypeToPlace;
			OutMicroTransaction.MicroIndexWithinParent = MicroIndexWithinParent;

			if (SelectedCandidate.CompanionAnchorKey.IsValid()
				&& SelectedCandidate.CompanionAnchorKey != SelectedCandidate.AnchorKey)
			{
				OutMicroTransaction.MicroAdditionalAnchorKeys.Add(SelectedCandidate.CompanionAnchorKey);
			}

			return true;
		}

		bool TryPlaceMissionForType(const EMapMission MissionType, const int32 MissionIndex,
		                            FWorldCampaignKeyPlacementCandidate& OutSelectedCandidate)
		{
			const FWorldCampaignPerMissionRulesSnapshot* MissionRules = Snapshot->MissionPlacementRules.RulesByMission.
				Find(
					MissionType);
			if (MissionRules == nullptr)
			{
				return false;
			}

			FMissionTierRules EffectiveRules;
			if (not TryBuildEffectiveMissionRules(*MissionRules, EffectiveRules))
			{
				return false;
			}

			const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::MissionsPlaced);
			const FRuleRelaxationState RelaxationState = GetRelaxationState(
				GetFailurePolicyForStep(ECampaignGenerationStep::MissionsPlaced),
				AttemptIndex);
			ApplyMissionRuleRelaxation(RelaxationState, EffectiveRules);

			FMissionSelectionSettings SelectionSettings;
			if (not BuildMissionSelectionSettings(*MissionRules, SelectionSettings))
			{
				return false;
			}

			if (not TrySelectMissionPlacementCandidate(MissionType, MissionIndex, AttemptIndex, RelaxationState,
			                                           EffectiveRules, SelectionSettings, OutSelectedCandidate))
			{
				return false;
			}

			State.MissionsByAnchorKey.Add(OutSelectedCandidate.AnchorKey, MissionType);
			IncrementMissionPlacedCount(MissionType);
			ApplyMissionCompanionNeutral(OutSelectedCandidate);
			return true;
		}

		bool TryBuildEffectiveMissionRules(const FWorldCampaignPerMissionRulesSnapshot& MissionRules,
		                                   FMissionTierRules& OutEffectiveRules) const
		{
			if (MissionRules.bOverrideTierRules)
			{
				OutEffectiveRules = MissionRules.OverrideRules;
				return true;
			}

			const FMissionTierRules* TierRules = Snapshot->MissionPlacementRules.RulesByTier.Find(MissionRules.Tier);
			if (TierRules == nullptr)
			{
				return false;
			}

			OutEffectiveRules = *TierRules;
			return true;
		}

		static void ApplyMissionRuleRelaxation(const FRuleRelaxationState& RelaxationState,
		                                       FMissionTierRules& InOutRules)
		{
			if (RelaxationState.bRelaxDistance)
			{
				ApplyMissionDistanceRelaxation(InOutRules);
			}

			if (RelaxationState.bRelaxSpacing)
			{
				ApplyMissionSpacingRelaxation(InOutRules);
			}

			if (RelaxationState.bRelaxPreference)
			{
				InOutRules.ConnectionPreference = ETopologySearchStrategy::NotSet;
			}
		}

		static void ApplyMissionDistanceRelaxation(FMissionTierRules& InOutRules)
		{
			if (InOutRules.bUseHopsDistanceFromHQ)
			{
				InOutRules.MinHopsFromHQ = 0;
				InOutRules.MaxHopsFromHQ = RelaxedHopDistanceMax;
				InOutRules.HopsDistancePreference = ETopologySearchStrategy::NotSet;
			}

			if (InOutRules.bUseXYDistanceFromHQ)
			{
				InOutRules.MinXYDistanceFromHQ = 0.f;
				InOutRules.MaxXYDistanceFromHQ = RelaxedDistanceMax;
				InOutRules.XYDistancePreference = ETopologySearchStrategy::NotSet;
			}
		}

		static void ApplyMissionSpacingRelaxation(FMissionTierRules& InOutRules)
		{
			if (InOutRules.bUseMissionSpacingHops)
			{
				InOutRules.MinMissionSpacingHops = 0;
				InOutRules.MaxMissionSpacingHops = RelaxedHopDistanceMax;
				InOutRules.MissionSpacingHopsPreference = ETopologySearchStrategy::NotSet;
			}

			if (InOutRules.bUseMissionSpacingXY)
			{
				InOutRules.MinMissionSpacingXY = 0.f;
				InOutRules.MaxMissionSpacingXY = RelaxedDistanceMax;
				InOutRules.MissionSpacingXYPreference = ETopologySearchStrategy::NotSet;
			}
		}

		bool BuildMissionSelectionSettings(const FWorldCampaignPerMissionRulesSnapshot& MissionRules,
		                                   FMissionSelectionSettings& OutSettings) const
		{
			OutSettings.bOverridePlacementWithArray = MissionRules.bOverridePlacementWithArray;
			if (OutSettings.bOverridePlacementWithArray
				&& MissionRules.OverridePlacementAnchorCandidateIndices.Num() == 0)
			{
				return false;
			}

			OutSettings.CandidateSourceIndices = OutSettings.bOverridePlacementWithArray
				                                     ? MissionRules.OverridePlacementAnchorCandidateIndices
				                                     : AnchorIndicesInSnapshotOrder;
			OutSettings.MinConnections = FMath::Min(MissionRules.OverrideMinConnections,
			                                        MissionRules.OverrideMaxConnections);
			OutSettings.MaxConnections = FMath::Max(MissionRules.OverrideMinConnections,
			                                        MissionRules.OverrideMaxConnections);
			OutSettings.ConnectionPreference = MissionRules.OverrideConnectionPreference;
			OutSettings.MinHopsFromPlayerHQ = FMath::Min(MissionRules.OverrideMinHopsFromPlayerHQ,
			                                             MissionRules.OverrideMaxHopsFromPlayerHQ);
			OutSettings.MaxHopsFromPlayerHQ = FMath::Max(MissionRules.OverrideMinHopsFromPlayerHQ,
			                                             MissionRules.OverrideMaxHopsFromPlayerHQ);
			OutSettings.HopsPreference = MissionRules.OverrideHopsPreference;
			return true;
		}

		bool TrySelectMissionPlacementCandidate(const EMapMission MissionType, const int32 MissionIndex,
		                                        const int32 AttemptIndex,
		                                        const FRuleRelaxationState& RelaxationState,
		                                        const FMissionTierRules& EffectiveRules,
		                                        const FMissionSelectionSettings& SelectionSettings,
		                                        FWorldCampaignKeyPlacementCandidate& OutCandidate)
		{
			UpdateProgress(FString::Printf(
				               TEXT("Evaluating mission %d candidates from %d anchors"),
				               static_cast<int32>(MissionType),
				               SelectionSettings.CandidateSourceIndices.Num()),
			               ECampaignGenerationStep::MissionsPlaced);
			TArray<FWorldCampaignKeyPlacementCandidate> Candidates;
			for (int32 SourceIndex = 0; SourceIndex < SelectionSettings.CandidateSourceIndices.Num(); SourceIndex++)
			{
				const int32 CandidateIndex = SelectionSettings.CandidateSourceIndices[SourceIndex];
				FWorldCampaignKeyPlacementCandidate Candidate;
				if (not TryBuildMissionCandidate(CandidateIndex, RelaxationState, EffectiveRules, SelectionSettings,
				                                 Candidate))
				{
					continue;
				}

				Candidate.CandidateOrder = SourceIndex;
				Candidates.Add(Candidate);
			}

			if (Candidates.Num() == 0)
			{
				return false;
			}

			SortPlacementCandidates(Candidates);
			const ETopologySearchStrategy HopsPreference = SelectionSettings.bOverridePlacementWithArray
				                                               ? SelectionSettings.HopsPreference
				                                               : EffectiveRules.HopsDistancePreference;
			const int32 CandidateIndex = SelectMissionCandidateIndexDeterministic(
				Candidates,
				EffectiveRules,
				HopsPreference,
				MissionType,
				AttemptIndex,
				MissionIndex);
			OutCandidate = Candidates[CandidateIndex];
			return true;
		}

		bool TryBuildMissionCandidate(const int32 CandidateIndex, const FRuleRelaxationState& RelaxationState,
		                              const FMissionTierRules& EffectiveRules,
		                              const FMissionSelectionSettings& SelectionSettings,
		                              FWorldCampaignKeyPlacementCandidate& OutCandidate)
		{
			if (not IsAnchorCached(CandidateIndex))
			{
				return false;
			}

			const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
			const bool bAllowNeutralStacking = EffectiveRules.bNeutralItemRequired;
			if (IsAnchorOccupiedForMission(CandidateKey, bAllowNeutralStacking))
			{
				return false;
			}

			if (EffectiveRules.bNeutralItemRequired
				&& not HasNeutralTypeAtAnchor(CandidateKey, EffectiveRules.RequiredNeutralItemType))
			{
				return false;
			}

			int32 HopDistanceFromHQ = INDEX_NONE;
			if (not PassesMissionDistanceRules(CandidateIndex, EffectiveRules, SelectionSettings, HopDistanceFromHQ))
			{
				return false;
			}

			const int32 ConnectionDegree = GetAnchorConnectionDegree(CandidateKey);
			if (not PassesMissionConnectionRules(ConnectionDegree, EffectiveRules, SelectionSettings))
			{
				return false;
			}

			float NearestMissionHopDistance = 0.f;
			float NearestMissionXYDistance = 0.f;
			if (not TryGetMissionSpacingData(EffectiveRules, CandidateIndex, NearestMissionHopDistance,
			                                 NearestMissionXYDistance))
			{
				return false;
			}

			OutCandidate.AnchorKey = CandidateKey;
			OutCandidate.AnchorIndex = CandidateIndex;
			OutCandidate.HopDistanceFromHQ = HopDistanceFromHQ;
			OutCandidate.Score = GetMissionPreferenceScore(EffectiveRules, SelectionSettings, CandidateIndex,
			                                               NearestMissionHopDistance, NearestMissionXYDistance,
			                                               ConnectionDegree);
			return TryApplyMissionAdjacencyRequirement(EffectiveRules.AdjacencyRequirement, CandidateIndex,
			                                           RelaxationState, OutCandidate);
		}

		bool PassesMissionDistanceRules(const int32 CandidateIndex, const FMissionTierRules& EffectiveRules,
		                                const FMissionSelectionSettings& SelectionSettings,
		                                int32& OutHopDistanceFromHQ) const
		{
			const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
			OutHopDistanceFromHQ = INDEX_NONE;
			if (SelectionSettings.bOverridePlacementWithArray)
			{
				OutHopDistanceFromHQ = GetCachedHopDistance(DerivedData.PlayerHQHopDistancesByAnchorKey, CandidateKey);
				return OutHopDistanceFromHQ != INDEX_NONE
					&& OutHopDistanceFromHQ >= SelectionSettings.MinHopsFromPlayerHQ
					&& OutHopDistanceFromHQ <= SelectionSettings.MaxHopsFromPlayerHQ
					&& PassesMissionXYDistanceRules(CandidateIndex, EffectiveRules);
			}

			if (EffectiveRules.bUseHopsDistanceFromHQ)
			{
				OutHopDistanceFromHQ = GetCachedHopDistance(DerivedData.PlayerHQHopDistancesByAnchorKey, CandidateKey);
				const int32 MaxHopsFromHQClamped = FMath::Max(EffectiveRules.MinHopsFromHQ,
				                                              EffectiveRules.MaxHopsFromHQ);
				if (OutHopDistanceFromHQ == INDEX_NONE
					|| OutHopDistanceFromHQ < EffectiveRules.MinHopsFromHQ
					|| OutHopDistanceFromHQ > MaxHopsFromHQClamped)
				{
					return false;
				}
			}

			return PassesMissionXYDistanceRules(CandidateIndex, EffectiveRules);
		}

		bool PassesMissionXYDistanceRules(const int32 CandidateIndex, const FMissionTierRules& EffectiveRules) const
		{
			if (not EffectiveRules.bUseXYDistanceFromHQ)
			{
				return true;
			}

			const float XYDistanceFromHQ = GetXYDistance(CandidateIndex, State.PlayerHQAnchorIndex);
			return XYDistanceFromHQ >= EffectiveRules.MinXYDistanceFromHQ
				&& XYDistanceFromHQ <= EffectiveRules.MaxXYDistanceFromHQ;
		}

		static bool PassesMissionConnectionRules(const int32 ConnectionDegree,
		                                         const FMissionTierRules& EffectiveRules,
		                                         const FMissionSelectionSettings& SelectionSettings)
		{
			if (SelectionSettings.bOverridePlacementWithArray)
			{
				return ConnectionDegree >= SelectionSettings.MinConnections
					&& ConnectionDegree <= SelectionSettings.MaxConnections;
			}

			return ConnectionDegree >= EffectiveRules.MinConnections
				&& ConnectionDegree <= EffectiveRules.MaxConnections;
		}

		float GetMissionPreferenceScore(const FMissionTierRules& EffectiveRules,
		                                const FMissionSelectionSettings& SelectionSettings,
		                                const int32 CandidateIndex,
		                                const float NearestMissionHopDistance,
		                                const float NearestMissionXYDistance,
		                                const int32 ConnectionDegree) const
		{
			const FGuid CandidateKey = GetAnchorKey(CandidateIndex);
			if (SelectionSettings.bOverridePlacementWithArray)
			{
				return GetOverrideMissionPreferenceScore(
					SelectionSettings.ConnectionPreference,
					SelectionSettings.HopsPreference,
					ConnectionDegree,
					GetCachedHopDistance(DerivedData.PlayerHQHopDistancesByAnchorKey, CandidateKey));
			}

			float PreferenceScore = 0.f;
			if (EffectiveRules.bUseHopsDistanceFromHQ)
			{
				const int32 HopDistanceFromHQ = GetCachedHopDistance(
					DerivedData.PlayerHQHopDistancesByAnchorKey,
					CandidateKey);
				PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.HopsDistancePreference,
				                                              static_cast<float>(HopDistanceFromHQ));
			}

			if (EffectiveRules.bUseXYDistanceFromHQ)
			{
				PreferenceScore += GetTopologyPreferenceScore(
					EffectiveRules.XYDistancePreference,
					GetXYDistance(CandidateIndex, State.PlayerHQAnchorIndex));
			}

			if (EffectiveRules.bUseMissionSpacingHops && State.MissionsByAnchorKey.Num() > 0)
			{
				PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.MissionSpacingHopsPreference,
				                                              NearestMissionHopDistance);
			}

			if (EffectiveRules.bUseMissionSpacingXY && State.MissionsByAnchorKey.Num() > 0)
			{
				PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.MissionSpacingXYPreference,
				                                              NearestMissionXYDistance);
			}

			PreferenceScore += GetTopologyPreferenceScore(EffectiveRules.ConnectionPreference,
			                                              static_cast<float>(ConnectionDegree));
			return PreferenceScore;
		}

		bool TryGetMissionSpacingData(const FMissionTierRules& EffectiveRules, const int32 CandidateIndex,
		                              float& OutNearestMissionHopDistance,
		                              float& OutNearestMissionXYDistance) const
		{
			OutNearestMissionHopDistance = 0.f;
			OutNearestMissionXYDistance = 0.f;
			if (State.MissionsByAnchorKey.Num() == 0)
			{
				return true;
			}

			OutNearestMissionHopDistance = static_cast<float>(RelaxedHopDistanceMax);
			OutNearestMissionXYDistance = RelaxedDistanceMax;
			TArray<FGuid> SortedMissionAnchorKeys;
			State.MissionsByAnchorKey.GetKeys(SortedMissionAnchorKeys);
			SortAnchorKeys(SortedMissionAnchorKeys);
			for (const FGuid& MissionAnchorKey : SortedMissionAnchorKeys)
			{
				if (not TryApplyMissionHopSpacing(EffectiveRules, CandidateIndex, MissionAnchorKey,
				                                  OutNearestMissionHopDistance))
				{
					return false;
				}

				if (not TryApplyMissionXYSpacing(EffectiveRules, CandidateIndex, MissionAnchorKey,
				                                 OutNearestMissionXYDistance))
				{
					return false;
				}
			}

			return true;
		}

		bool TryApplyMissionHopSpacing(const FMissionTierRules& EffectiveRules, const int32 CandidateIndex,
		                               const FGuid& MissionAnchorKey, float& InOutNearestMissionHopDistance) const
		{
			if (not EffectiveRules.bUseMissionSpacingHops)
			{
				return true;
			}

			const int32 MissionAnchorIndex = FindAnchorIndexByKey(MissionAnchorKey);
			const int32 HopDistance = GetHopDistance(MissionAnchorIndex, CandidateIndex);
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

			InOutNearestMissionHopDistance = FMath::Min(
				InOutNearestMissionHopDistance,
				static_cast<float>(HopDistance));
			return true;
		}

		bool TryApplyMissionXYSpacing(const FMissionTierRules& EffectiveRules, const int32 CandidateIndex,
		                              const FGuid& MissionAnchorKey, float& InOutNearestMissionXYDistance) const
		{
			if (not EffectiveRules.bUseMissionSpacingXY)
			{
				return true;
			}

			const int32 MissionAnchorIndex = FindAnchorIndexByKey(MissionAnchorKey);
			const float XYDistance = GetXYDistance(CandidateIndex, MissionAnchorIndex);
			if (XYDistance < EffectiveRules.MinMissionSpacingXY || XYDistance > EffectiveRules.MaxMissionSpacingXY)
			{
				return false;
			}

			InOutNearestMissionXYDistance = FMath::Min(InOutNearestMissionXYDistance, XYDistance);
			return true;
		}

		bool TryApplyMissionAdjacencyRequirement(const FMapObjectAdjacencyRequirement& Requirement,
		                                         const int32 CandidateIndex,
		                                         const FRuleRelaxationState& RelaxationState,
		                                         FWorldCampaignKeyPlacementCandidate& InOutCandidate) const
		{
			if (not Requirement.bEnabled)
			{
				return true;
			}

			const TArray<int32> MatchingHopDistances = BuildAdjacencyMatchDistances(Requirement, CandidateIndex);
			if (HasMinimumAdjacentMatches(MatchingHopDistances, Requirement.MinMatchingCount))
			{
				return true;
			}

			if (Requirement.Policy != EAdjacencyPolicy::TryAutoPlaceCompanion
				|| Requirement.RequiredItemType != EMapItemType::NeutralItem)
			{
				return false;
			}

			FGuid CompanionAnchorKey;
			int32 CompanionAnchorIndex = INDEX_NONE;
			if (not TrySelectCompanionNeutralAnchor(CandidateIndex, Requirement, RelaxationState, CompanionAnchorKey,
			                                        CompanionAnchorIndex))
			{
				return false;
			}

			InOutCandidate.CompanionAnchorKey = CompanionAnchorKey;
			InOutCandidate.CompanionAnchorIndex = CompanionAnchorIndex;
			InOutCandidate.CompanionRawSubtype = Requirement.RawByteSpecificItemSubtype;
			InOutCandidate.CompanionItemType = EMapItemType::NeutralItem;
			return true;
		}

		TArray<int32> BuildAdjacencyMatchDistances(const FMapObjectAdjacencyRequirement& Requirement,
		                                           const int32 CandidateIndex) const
		{
			TArray<int32> MatchingHopDistances;
			switch (Requirement.RequiredItemType)
			{
			case EMapItemType::NeutralItem:
				AppendNeutralAdjacencyMatches(Requirement, CandidateIndex, MatchingHopDistances);
				break;
			case EMapItemType::Mission:
				AppendMissionAdjacencyMatches(Requirement, CandidateIndex, MatchingHopDistances);
				break;
			case EMapItemType::EnemyItem:
				AppendEnemyAdjacencyMatches(Requirement, CandidateIndex, MatchingHopDistances);
				break;
			case EMapItemType::PlayerItem:
				AppendPlayerAdjacencyMatches(Requirement, CandidateIndex, MatchingHopDistances);
				break;
			case EMapItemType::Empty:
			case EMapItemType::None:
			default:
				break;
			}

			MatchingHopDistances.Sort();
			return MatchingHopDistances;
		}

		void AppendNeutralAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
		                                   const int32 CandidateIndex,
		                                   TArray<int32>& InOutDistances) const
		{
			const EMapNeutralObjectType RequiredNeutralType =
				static_cast<EMapNeutralObjectType>(Requirement.RawByteSpecificItemSubtype);
			TArray<FGuid> SortedNeutralAnchorKeys;
			State.NeutralItemsByAnchorKey.GetKeys(SortedNeutralAnchorKeys);
			SortAnchorKeys(SortedNeutralAnchorKeys);
			for (const FGuid& NeutralAnchorKey : SortedNeutralAnchorKeys)
			{
				if (State.NeutralItemsByAnchorKey.FindRef(NeutralAnchorKey) != RequiredNeutralType)
				{
					continue;
				}

				AppendAdjacencyHopIfInRange(CandidateIndex, NeutralAnchorKey, Requirement.MaxHops, InOutDistances);
			}
		}

		void AppendMissionAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
		                                   const int32 CandidateIndex,
		                                   TArray<int32>& InOutDistances) const
		{
			const EMapMission RequiredMissionType = static_cast<EMapMission>(Requirement.RawByteSpecificItemSubtype);
			TArray<FGuid> SortedMissionAnchorKeys;
			State.MissionsByAnchorKey.GetKeys(SortedMissionAnchorKeys);
			SortAnchorKeys(SortedMissionAnchorKeys);
			for (const FGuid& MissionAnchorKey : SortedMissionAnchorKeys)
			{
				if (State.MissionsByAnchorKey.FindRef(MissionAnchorKey) != RequiredMissionType)
				{
					continue;
				}

				AppendAdjacencyHopIfInRange(CandidateIndex, MissionAnchorKey, Requirement.MaxHops, InOutDistances);
			}
		}

		void AppendEnemyAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
		                                 const int32 CandidateIndex,
		                                 TArray<int32>& InOutDistances) const
		{
			const EMapEnemyItem RequiredEnemyType = static_cast<EMapEnemyItem>(Requirement.RawByteSpecificItemSubtype);
			TArray<FGuid> SortedEnemyAnchorKeys;
			State.EnemyItemsByAnchorKey.GetKeys(SortedEnemyAnchorKeys);
			SortAnchorKeys(SortedEnemyAnchorKeys);
			for (const FGuid& EnemyAnchorKey : SortedEnemyAnchorKeys)
			{
				if (State.EnemyItemsByAnchorKey.FindRef(EnemyAnchorKey) != RequiredEnemyType)
				{
					continue;
				}

				AppendAdjacencyHopIfInRange(CandidateIndex, EnemyAnchorKey, Requirement.MaxHops, InOutDistances);
			}
		}

		void AppendPlayerAdjacencyMatches(const FMapObjectAdjacencyRequirement& Requirement,
		                                  const int32 CandidateIndex,
		                                  TArray<int32>& InOutDistances) const
		{
			const int32 HopDistance = GetHopDistance(State.PlayerHQAnchorIndex, CandidateIndex);
			if (HopDistance != INDEX_NONE && HopDistance <= Requirement.MaxHops)
			{
				InOutDistances.Add(HopDistance);
			}
		}

		void AppendAdjacencyHopIfInRange(const int32 CandidateIndex, const FGuid& TargetKey, const int32 MaxHops,
		                                 TArray<int32>& InOutDistances) const
		{
			const int32 TargetIndex = FindAnchorIndexByKey(TargetKey);
			const int32 HopDistance = GetHopDistance(TargetIndex, CandidateIndex);
			if (HopDistance != INDEX_NONE && HopDistance <= MaxHops)
			{
				InOutDistances.Add(HopDistance);
			}
		}

		bool TrySelectCompanionNeutralAnchor(const int32 CandidateIndex,
		                                     const FMapObjectAdjacencyRequirement& Requirement,
		                                     const FRuleRelaxationState& RelaxationState,
		                                     FGuid& OutCompanionAnchorKey,
		                                     int32& OutCompanionAnchorIndex) const
		{
			OutCompanionAnchorKey = FGuid();
			OutCompanionAnchorIndex = INDEX_NONE;
			FNeutralItemPlacementRules EffectiveRules = Snapshot->NeutralItemPlacementRules;
			ApplyNeutralRuleRelaxation(RelaxationState, EffectiveRules);

			TArray<FWorldCampaignKeyPlacementCandidate> Candidates;
			for (int32 SourceIndex = 0; SourceIndex < AnchorIndicesInSnapshotOrder.Num(); SourceIndex++)
			{
				const int32 NearbyIndex = AnchorIndicesInSnapshotOrder[SourceIndex];
				const FGuid NearbyKey = GetAnchorKey(NearbyIndex);
				if (IsAnchorOccupied(NearbyKey))
				{
					continue;
				}

				const int32 HopDistance = GetHopDistance(CandidateIndex, NearbyIndex);
				if (HopDistance == INDEX_NONE || HopDistance > Requirement.MaxHops)
				{
					continue;
				}

				if (not PassesNeutralDistanceRules(EffectiveRules, NearbyKey)
					|| not PassesNeutralSpacingRules(EffectiveRules, NearbyIndex))
				{
					continue;
				}

				const int32 HopDistanceFromHQ = GetCachedHopDistance(
					DerivedData.PlayerHQHopDistancesByAnchorKey,
					NearbyKey);
				FWorldCampaignKeyPlacementCandidate Candidate;
				Candidate.AnchorKey = NearbyKey;
				Candidate.AnchorIndex = NearbyIndex;
				Candidate.CandidateOrder = SourceIndex;
				Candidate.Score = GetTopologyPreferenceScore(EffectiveRules.Preference,
				                                             static_cast<float>(HopDistanceFromHQ));
				Candidate.HopDistanceFromHQ = HopDistanceFromHQ;
				Candidates.Add(Candidate);
			}

			if (Candidates.Num() == 0)
			{
				return false;
			}

			SortPlacementCandidates(Candidates);
			OutCompanionAnchorKey = Candidates[0].AnchorKey;
			OutCompanionAnchorIndex = Candidates[0].AnchorIndex;
			return OutCompanionAnchorKey.IsValid() && IsAnchorCached(OutCompanionAnchorIndex);
		}

		void ApplyMissionCompanionNeutral(const FWorldCampaignKeyPlacementCandidate& SelectedCandidate)
		{
			if (SelectedCandidate.CompanionItemType != EMapItemType::NeutralItem)
			{
				return;
			}

			const EMapNeutralObjectType CompanionType =
				static_cast<EMapNeutralObjectType>(SelectedCandidate.CompanionRawSubtype);
			State.NeutralItemsByAnchorKey.Add(SelectedCandidate.CompanionAnchorKey, CompanionType);
			IncrementNeutralPlacedCount(CompanionType);
		}

		int32 SelectMissionCandidateIndexDeterministic(
			const TArray<FWorldCampaignKeyPlacementCandidate>& Candidates,
			const FMissionTierRules& EffectiveRules,
			const ETopologySearchStrategy HopsPreference,
			const EMapMission MissionType,
			const int32 AttemptIndex,
			const int32 MissionIndex) const
		{
			const int32 CandidateCount = Candidates.Num();
			if (not ShouldUseHopWeightedMissionSelection(EffectiveRules, HopsPreference))
			{
				return (AttemptIndex + MissionIndex) % CandidateCount;
			}

			TArray<int32> SelectableIndices;
			BuildMissionScoreBandIndices(Candidates, SelectableIndices);
			const int32 TargetHop = GetTargetHopDistanceFromCandidates(Candidates, SelectableIndices, HopsPreference);
			constexpr int32 BaseMaxWeight = 32;
			constexpr int32 BaseFalloffPerDelta = 8;
			constexpr int32 StrengthMaxWeightMultiplier = 16;
			const int32 EffectiveMaxWeight = BaseMaxWeight
				+ Snapshot->MissionPlacementRules.M_HopsPreferenceStrength * StrengthMaxWeightMultiplier;
			const int32 EffectiveFalloff = FMath::Max(1, BaseFalloffPerDelta
			                                          - Snapshot->MissionPlacementRules.M_HopsPreferenceStrength);
			uint64 TotalWeight = 0;
			for (const int32 CandidateIndex : SelectableIndices)
			{
				const int32 Weight = GetHopPreferenceWeight(EffectiveMaxWeight, EffectiveFalloff,
				                                            Candidates[CandidateIndex].HopDistanceFromHQ, TargetHop);
				TotalWeight += static_cast<uint64>(Weight);
			}

			const uint64 SelectionHash = HashCombine64(
				static_cast<uint64>(State.SeedUsed),
				static_cast<uint64>(MissionType),
				static_cast<uint64>(MissionIndex),
				static_cast<uint64>(AttemptIndex),
				static_cast<uint64>(CandidateCount));
			uint64 Roll = TotalWeight > 0 ? SelectionHash % TotalWeight : 0;
			for (const int32 CandidateIndex : SelectableIndices)
			{
				const int32 Weight = GetHopPreferenceWeight(EffectiveMaxWeight, EffectiveFalloff,
				                                            Candidates[CandidateIndex].HopDistanceFromHQ, TargetHop);
				if (Roll < static_cast<uint64>(Weight))
				{
					return CandidateIndex;
				}

				Roll -= static_cast<uint64>(Weight);
			}

			return SelectableIndices.Last();
		}

		bool ShouldUseHopWeightedMissionSelection(const FMissionTierRules& EffectiveRules,
		                                          const ETopologySearchStrategy HopsPreference) const
		{
			if (not EffectiveRules.bUseHopsDistanceFromHQ)
			{
				return false;
			}

			if (HopsPreference != ETopologySearchStrategy::PreferMin
				&& HopsPreference != ETopologySearchStrategy::PreferMax)
			{
				return false;
			}

			return Snapshot->MissionPlacementRules.M_HopsPreferenceStrength > 0;
		}

		static void BuildMissionScoreBandIndices(const TArray<FWorldCampaignKeyPlacementCandidate>& Candidates,
		                                         TArray<int32>& OutSelectableIndices)
		{
			constexpr int32 ScoreBandMinCount = 3;
			constexpr float ScoreBandTolerance = 0.001f;
			const float BestScore = Candidates[0].Score;
			OutSelectableIndices.Reset();
			OutSelectableIndices.Reserve(Candidates.Num());
			for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); CandidateIndex++)
			{
				if (FMath::IsNearlyEqual(Candidates[CandidateIndex].Score, BestScore, ScoreBandTolerance))
				{
					OutSelectableIndices.Add(CandidateIndex);
				}
			}

			if (OutSelectableIndices.Num() >= ScoreBandMinCount)
			{
				return;
			}

			OutSelectableIndices.Reset();
			for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); CandidateIndex++)
			{
				OutSelectableIndices.Add(CandidateIndex);
			}
		}

		static int32 GetTargetHopDistanceFromCandidates(
			const TArray<FWorldCampaignKeyPlacementCandidate>& Candidates,
			const TArray<int32>& SelectableIndices,
			const ETopologySearchStrategy HopsPreference)
		{
			int32 TargetHop = Candidates[SelectableIndices[0]].HopDistanceFromHQ;
			for (const int32 CandidateIndex : SelectableIndices)
			{
				const int32 HopDistance = Candidates[CandidateIndex].HopDistanceFromHQ;
				TargetHop = HopsPreference == ETopologySearchStrategy::PreferMax
					            ? FMath::Max(TargetHop, HopDistance)
					            : FMath::Min(TargetHop, HopDistance);
			}

			return TargetHop;
		}

		bool TryApplyTimeoutFailSafePlacement(const ECampaignGenerationStep FailedStep)
		{
			UpdateProgress(TEXT("Collecting timeout fail-safe anchors"), FailedStep);
			bool bHasValidPlayerHQAnchor = false;
			TArray<FWorldCampaignFailSafeAnchorDistance> EmptyAnchors;
			BuildTimeoutFailSafeAnchors(EmptyAnchors, bHasValidPlayerHQAnchor);
			if (EmptyAnchors.Num() == 0)
			{
				AddDebugEvent(FailedStep, TEXT("Timeout fail-safe: no empty anchors available."));
				return true;
			}

			const TArray<EMapMission> MissionPlacementPlan = BuildMissionPlacementPlanFiltered();
			TArray<FFailSafeItem> ItemsToPlace;
			BuildTimeoutFailSafeItemsToPlace(MissionPlacementPlan, ItemsToPlace);

			if (ItemsToPlace.Num() == 0)
			{
				return true;
			}

			const FWorldCampaignPlacementSolverTransaction FailSafeTransaction =
				BuildTimeoutFailSafeTransaction(FailedStep);
			FFailSafePlacementTotals Totals;
			TArray<FFailSafeItem> RemainingItems;
			TMap<EFailSafeItemKind, TArray<FVector2D>> PlacedByKind;
			UpdateProgress(FString::Printf(
				               TEXT("Assigning timeout fail-safe items: %d items, %d empty anchors"),
				               ItemsToPlace.Num(),
				               EmptyAnchors.Num()),
			               FailedStep);
			AssignFailSafeItemsByDistance(ItemsToPlace, bHasValidPlayerHQAnchor, EmptyAnchors, Totals,
			                              PlacedByKind, RemainingItems);
			AssignFailSafeItemsFallback(RemainingItems, FailedStep, EmptyAnchors, Totals);
			ApplyTimeoutFailSafeEnemyDeclusterSwaps();
			Transactions.Add(FailSafeTransaction);

			AddTimeoutFailSafeDebugEvent(FailedStep, Totals);
			return true;
		}

		void BuildTimeoutFailSafeItemsToPlace(const TArray<EMapMission>& MissionPlacementPlan,
		                                      TArray<FFailSafeItem>& OutItemsToPlace) const
		{
			/*
			 * Distance providers stay as lambdas so the count builder remains a pure utility while this
			 * solver owns how current placement rules translate into fail-safe minimum distances.
			 */
			BuildFailSafeItemsToPlace(
				Snapshot->CountAndDifficultyTuning,
				DerivedData,
				MissionPlacementPlan,
				[this](const EMapEnemyItem EnemyType)
				{
					return GetFailSafeMinDistanceForEnemy(EnemyType);
				},
				[this](const EMapNeutralObjectType NeutralType)
				{
					return GetFailSafeMinDistanceForNeutral(NeutralType);
				},
				[this](const EMapMission MissionType)
				{
					return GetFailSafeMinDistanceForMission(MissionType);
				},
				OutItemsToPlace);
		}

		FWorldCampaignPlacementSolverTransaction BuildTimeoutFailSafeTransaction(
			const ECampaignGenerationStep FailedStep) const
		{
			FWorldCampaignPlacementSolverTransaction FailSafeTransaction;
			FailSafeTransaction.CompletedStep = FailedStep;
			FailSafeTransaction.PreviousPlacementState = State;
			FailSafeTransaction.PreviousDerivedData = DerivedData;
			return FailSafeTransaction;
		}

		void AddTimeoutFailSafeDebugEvent(const ECampaignGenerationStep FailedStep,
		                                  const FFailSafePlacementTotals& Totals)
		{
			AddDebugEvent(FailedStep, FString::Printf(
				              TEXT("Timeout fail-safe placed: Enemy %d, Neutral %d, Missions %d. Discarded: %d."),
				              Totals.EnemyPlaced,
				              Totals.NeutralPlaced,
				              Totals.MissionPlaced,
				              Totals.Discarded));
		}

		void BuildTimeoutFailSafeAnchors(TArray<FWorldCampaignFailSafeAnchorDistance>& OutEmptyAnchors,
		                                 bool& bOutHasValidPlayerHQAnchor) const
		{
			OutEmptyAnchors.Reset();
			bOutHasValidPlayerHQAnchor = IsAnchorCached(State.PlayerHQAnchorIndex);
			FVector2D PlayerHQLocation = FVector2D::ZeroVector;
			if (bOutHasValidPlayerHQAnchor)
			{
				const FWorldCampaignAnchorSnapshot* PlayerHQAnchor = FindAnchor(State.PlayerHQAnchorIndex);
				if (PlayerHQAnchor != nullptr)
				{
					PlayerHQLocation = FVector2D(PlayerHQAnchor->Location);
				}
			}

			for (const int32 AnchorIndex : SortedAnchorIndices)
			{
				const FGuid AnchorKey = GetAnchorKey(AnchorIndex);
				if (IsAnchorOccupied(AnchorKey))
				{
					continue;
				}

				FWorldCampaignFailSafeAnchorDistance Entry;
				Entry.AnchorKey = AnchorKey;
				const FWorldCampaignAnchorSnapshot* Anchor = FindAnchor(AnchorIndex);
				if (Anchor != nullptr)
				{
					Entry.LocationXY = FVector2D(Anchor->Location);
				}
				Entry.DistanceSquared = bOutHasValidPlayerHQAnchor
					                        ? FVector2D::DistSquared(Entry.LocationXY, PlayerHQLocation)
					                        : 0.f;
				OutEmptyAnchors.Add(Entry);
			}

			constexpr float DistanceTolerance = 0.001f;
			OutEmptyAnchors.Sort([](const FWorldCampaignFailSafeAnchorDistance& Left,
			                        const FWorldCampaignFailSafeAnchorDistance& Right)
			{
				if (not FMath::IsNearlyEqual(Left.DistanceSquared, Right.DistanceSquared, DistanceTolerance))
				{
					return Left.DistanceSquared < Right.DistanceSquared;
				}

				return IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
			});
		}

		float GetFailSafeMinDistanceForEnemy(const EMapEnemyItem EnemyType) const
		{
			return Snapshot->PlacementFailurePolicy.MinDistancePlayerHQEnemyItemByType.FindRef(EnemyType);
		}

		float GetFailSafeMinDistanceForNeutral(const EMapNeutralObjectType NeutralType) const
		{
			return Snapshot->PlacementFailurePolicy.MinDistancePlayerHQNeutralByType.FindRef(NeutralType);
		}

		float GetFailSafeMinDistanceForMission(const EMapMission MissionType) const
		{
			const FWorldCampaignPerMissionRulesSnapshot* MissionRules =
				Snapshot->MissionPlacementRules.RulesByMission.Find(MissionType);
			if (MissionRules == nullptr)
			{
				return 0.f;
			}

			switch (MissionRules->Tier)
			{
			case EMissionTier::Tier1:
				return Snapshot->PlacementFailurePolicy.MinDistancePlayerHQTier1Mission;
			case EMissionTier::Tier2:
				return Snapshot->PlacementFailurePolicy.MinDistancePlayerHQTier2Mission;
			case EMissionTier::Tier3:
				return Snapshot->PlacementFailurePolicy.MinDistancePlayerHQTier3Mission;
			case EMissionTier::Tier4:
				return Snapshot->PlacementFailurePolicy.MinDistancePlayerHQTier4Mission;
			case EMissionTier::NotSet:
			default:
				return 0.f;
			}
		}

		void AssignFailSafeItemsByDistance(const TArray<FFailSafeItem>& ItemsToPlace, const bool bUseDistanceFilter,
		                                   TArray<FWorldCampaignFailSafeAnchorDistance>& InOutEmptyAnchors,
		                                   FFailSafePlacementTotals& InOutTotals,
		                                   TMap<EFailSafeItemKind, TArray<FVector2D>>& InOutPlacedByKind,
		                                   TArray<FFailSafeItem>& OutRemainingItems)
		{
			OutRemainingItems.Reset();
			const float MinSameKindSpacingSquared =
				FMath::Square(Snapshot->PlacementFailurePolicy.TimeoutFailSafeMinSameKindXYSpacing);
			for (const FFailSafeItem& Item : ItemsToPlace)
			{
				if (TryAssignFailSafeItemByDistance(Item, bUseDistanceFilter, MinSameKindSpacingSquared,
				                                    InOutEmptyAnchors, InOutTotals, InOutPlacedByKind))
				{
					continue;
				}

				OutRemainingItems.Add(Item);
			}
		}

		bool TryAssignFailSafeItemByDistance(const FFailSafeItem& Item, const bool bUseDistanceFilter,
		                                     const float MinSameKindSpacingSquared,
		                                     TArray<FWorldCampaignFailSafeAnchorDistance>& InOutEmptyAnchors,
		                                     FFailSafePlacementTotals& InOutTotals,
		                                     TMap<EFailSafeItemKind, TArray<FVector2D>>& InOutPlacedByKind)
		{
			const float MinDistanceSquared = bUseDistanceFilter ? FMath::Square(Item.MinDistance) : 0.f;
			for (int32 AnchorIndex = 0; AnchorIndex < InOutEmptyAnchors.Num(); ++AnchorIndex)
			{
				const FWorldCampaignFailSafeAnchorDistance& Candidate = InOutEmptyAnchors[AnchorIndex];
				if (bUseDistanceFilter && Candidate.DistanceSquared < MinDistanceSquared)
				{
					continue;
				}

				if (not GetPassesSameKindSpacing(Item, Candidate, MinSameKindSpacingSquared, InOutPlacedByKind))
				{
					continue;
				}

				PlaceFailSafeItem(Item, Candidate.AnchorKey, InOutTotals);
				InOutPlacedByKind.FindOrAdd(Item.Kind).Add(Candidate.LocationXY);
				InOutEmptyAnchors.RemoveAt(AnchorIndex);
				return true;
			}

			return false;
		}

		static bool GetPassesSameKindSpacing(const FFailSafeItem& Item,
		                                     const FWorldCampaignFailSafeAnchorDistance& Candidate,
		                                     const float MinimumSpacingSquared,
		                                     const TMap<EFailSafeItemKind, TArray<FVector2D>>& PlacedByKind)
		{
			if (MinimumSpacingSquared <= 0.f)
			{
				return true;
			}

			const TArray<FVector2D>* ExistingPlacements = PlacedByKind.Find(Item.Kind);
			if (ExistingPlacements == nullptr)
			{
				return true;
			}

			for (const FVector2D& ExistingLocationXY : *ExistingPlacements)
			{
				if (FVector2D::DistSquared(Candidate.LocationXY, ExistingLocationXY) < MinimumSpacingSquared)
				{
					return false;
				}
			}

			return true;
		}

		void AssignFailSafeItemsFallback(const TArray<FFailSafeItem>& RemainingItems,
		                                 const ECampaignGenerationStep FailedStep,
		                                 const TArray<FWorldCampaignFailSafeAnchorDistance>& EmptyAnchors,
		                                 FFailSafePlacementTotals& InOutTotals)
		{
			if (RemainingItems.Num() == 0)
			{
				return;
			}

			AddDebugEvent(FailedStep, TEXT("Timeout fail-safe: not enough anchors satisfying min-distance rules; "
				              "falling back to random assignment."));
			if (EmptyAnchors.Num() == 0)
			{
				InOutTotals.Discarded += RemainingItems.Num();
				AddDebugEvent(FailedStep, FString::Printf(
					              TEXT(
						              "Timeout fail-safe: not enough empty anchors; %d items could not be placed and were discarded."),
					              RemainingItems.Num()));
				return;
			}

			TArray<FWorldCampaignFailSafeAnchorDistance> ShuffledAnchors = EmptyAnchors;
			const int32 SeedOffset = Snapshot->PlacementFailurePolicy.TimeoutFailSafeSeedOffset;
			FRandomStream RandomStream(State.SeedUsed + SeedOffset + static_cast<int32>(FailedStep));
			CampaignGenerationHelper::DeterministicShuffle(ShuffledAnchors, RandomStream);

			int32 AnchorIndex = 0;
			for (const FFailSafeItem& Item : RemainingItems)
			{
				if (AnchorIndex >= ShuffledAnchors.Num())
				{
					InOutTotals.Discarded++;
					continue;
				}

				PlaceFailSafeItem(Item, ShuffledAnchors[AnchorIndex].AnchorKey, InOutTotals);
				AnchorIndex++;
			}

			if (InOutTotals.Discarded > 0)
			{
				AddDebugEvent(FailedStep, FString::Printf(
					              TEXT(
						              "Timeout fail-safe: not enough empty anchors; %d items could not be placed and were discarded."),
					              InOutTotals.Discarded));
			}
		}

		void PlaceFailSafeItem(const FFailSafeItem& Item, const FGuid& AnchorKey,
		                       FFailSafePlacementTotals& InOutTotals)
		{
			if (Item.Kind == EFailSafeItemKind::Enemy)
			{
				State.EnemyItemsByAnchorKey.Add(AnchorKey, Item.EnemyType);
				IncrementEnemyPlacedCount(Item.EnemyType);
				InOutTotals.EnemyPlaced++;
				return;
			}

			if (Item.Kind == EFailSafeItemKind::Neutral)
			{
				State.NeutralItemsByAnchorKey.Add(AnchorKey, Item.NeutralType);
				IncrementNeutralPlacedCount(Item.NeutralType);
				InOutTotals.NeutralPlaced++;
				return;
			}

			State.MissionsByAnchorKey.Add(AnchorKey, Item.MissionType);
			IncrementMissionPlacedCount(Item.MissionType);
			InOutTotals.MissionPlaced++;
		}

		void ApplyTimeoutFailSafeEnemyDeclusterSwaps()
		{
			const int32 SwapBudget = FMath::Max(
				0, Snapshot->PlacementFailurePolicy.TimeoutFailSafeEnemyDeclusterSwapBudget);
			if (SwapBudget == 0)
			{
				return;
			}

			const int32 TopMCandidates = FMath::Max(
				2, Snapshot->PlacementFailurePolicy.TimeoutFailSafeEnemyDeclusterTopMCandidates);
			TSet<FString> FailedSwapPairKeys;
			for (int32 AppliedSwaps = 0; AppliedSwaps < SwapBudget;)
			{
				const FEnemyDeclusterSwapCandidate BestSwap = FindBestEnemyDeclusterSwap(TopMCandidates,
					FailedSwapPairKeys);
				if (not BestSwap.bIsValid)
				{
					return;
				}

				ApplyEnemyDeclusterSwap(BestSwap);
				AppliedSwaps++;
			}
		}

		FEnemyDeclusterSwapCandidate FindBestEnemyDeclusterSwap(const int32 TopMCandidates,
		                                                        const TSet<FString>& FailedSwapPairKeys) const
		{
			FEnemyDeclusterSwapCandidate BestCandidate;
			TArray<TPair<FGuid, EMapEnemyItem>> SortedSwappableAnchors;
			BuildSortedSwappableEnemyAnchors(SortedSwappableAnchors);
			if (SortedSwappableAnchors.Num() < 2)
			{
				return BestCandidate;
			}

			TMap<FGuid, FVector2D> AnchorLocationsByKey;
			BuildEnemyDeclusterLocationLookup(SortedSwappableAnchors, AnchorLocationsByKey);
			TArray<FEnemyDeclusterAnchorScore> AnchorScores;
			BuildEnemyDeclusterAnchorScores(SortedSwappableAnchors, AnchorLocationsByKey, AnchorScores);
			const int32 MaxBadAnchorsToEvaluate = FMath::Clamp(TopMCandidates, 1, AnchorScores.Num());
			for (int32 IndexA = 0; IndexA < MaxBadAnchorsToEvaluate; ++IndexA)
			{
				EvaluateEnemyDeclusterAnchor(AnchorScores[IndexA], SortedSwappableAnchors, AnchorLocationsByKey,
				                             FailedSwapPairKeys, BestCandidate);
			}

			return BestCandidate;
		}

		void BuildSortedSwappableEnemyAnchors(TArray<TPair<FGuid, EMapEnemyItem>>& OutSwappableAnchors) const
		{
			OutSwappableAnchors.Reset();
			for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : State.EnemyItemsByAnchorKey)
			{
				if (GetIsSwappableEnemyType(EnemyPair.Value))
				{
					OutSwappableAnchors.Add(EnemyPair);
				}
			}

			OutSwappableAnchors.Sort([](const TPair<FGuid, EMapEnemyItem>& Left,
			                            const TPair<FGuid, EMapEnemyItem>& Right)
			{
				return IsAnchorKeyLess(Left.Key, Right.Key);
			});
		}

		void BuildEnemyDeclusterLocationLookup(const TArray<TPair<FGuid, EMapEnemyItem>>& SortedSwappableAnchors,
		                                       TMap<FGuid, FVector2D>& OutAnchorLocationsByKey) const
		{
			OutAnchorLocationsByKey.Reset();
			for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : SortedSwappableAnchors)
			{
				FVector2D AnchorLocationXY = FVector2D::ZeroVector;
				if (TryGetLocationXY(EnemyPair.Key, AnchorLocationXY))
				{
					OutAnchorLocationsByKey.Add(EnemyPair.Key, AnchorLocationXY);
				}
			}
		}

		void BuildEnemyDeclusterAnchorScores(const TArray<TPair<FGuid, EMapEnemyItem>>& SortedSwappableAnchors,
		                                     const TMap<FGuid, FVector2D>& AnchorLocationsByKey,
		                                     TArray<FEnemyDeclusterAnchorScore>& OutAnchorScores) const
		{
			OutAnchorScores.Reset();
			for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : SortedSwappableAnchors)
			{
				const FVector2D* AnchorLocationXY = AnchorLocationsByKey.Find(EnemyPair.Key);
				if (AnchorLocationXY == nullptr)
				{
					continue;
				}

				FEnemyDeclusterAnchorScore Score;
				Score.AnchorKey = EnemyPair.Key;
				Score.EnemyType = EnemyPair.Value;
				Score.LocalDensity = ComputeEnemyLocalDensity(
					EnemyPair.Key,
					EnemyPair.Value,
					*AnchorLocationXY,
					State.EnemyItemsByAnchorKey,
					AnchorLocationsByKey);
				OutAnchorScores.Add(Score);
			}

			OutAnchorScores.Sort([](const FEnemyDeclusterAnchorScore& Left, const FEnemyDeclusterAnchorScore& Right)
			{
				if (Left.LocalDensity != Right.LocalDensity)
				{
					return Left.LocalDensity > Right.LocalDensity;
				}

				return IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
			});
		}

		void EvaluateEnemyDeclusterAnchor(const FEnemyDeclusterAnchorScore& AnchorA,
		                                  const TArray<TPair<FGuid, EMapEnemyItem>>& SortedSwappableAnchors,
		                                  const TMap<FGuid, FVector2D>& AnchorLocationsByKey,
		                                  const TSet<FString>& FailedSwapPairKeys,
		                                  FEnemyDeclusterSwapCandidate& InOutBestCandidate) const
		{
			for (const TPair<FGuid, EMapEnemyItem>& EnemyPairB : SortedSwappableAnchors)
			{
				if (not GetCanEvaluateEnemyDeclusterSwap(AnchorA, EnemyPairB, FailedSwapPairKeys))
				{
					continue;
				}

				UpdateBestEnemyDeclusterSwap(AnchorA, EnemyPairB, AnchorLocationsByKey, InOutBestCandidate);
			}
		}

		bool GetCanEvaluateEnemyDeclusterSwap(const FEnemyDeclusterAnchorScore& AnchorA,
		                                      const TPair<FGuid, EMapEnemyItem>& EnemyPairB,
		                                      const TSet<FString>& FailedSwapPairKeys) const
		{
			if (EnemyPairB.Key == AnchorA.AnchorKey || EnemyPairB.Value == AnchorA.EnemyType)
			{
				return false;
			}

			const FString FailedSwapPairKey = BuildCanonicalFailedEnemyDeclusterSwapPairKey(AnchorA.AnchorKey,
				EnemyPairB.Key);
			if (FailedSwapPairKeys.Contains(FailedSwapPairKey))
			{
				return false;
			}

			return GetPassesEnemyTypeHQDistance(AnchorA.AnchorKey, EnemyPairB.Value)
				&& GetPassesEnemyTypeHQDistance(EnemyPairB.Key, AnchorA.EnemyType);
		}

		bool GetPassesEnemyTypeHQDistance(const FGuid& CandidateAnchorKey, const EMapEnemyItem EnemyType) const
		{
			const float MinDistance = GetFailSafeMinDistanceForEnemy(EnemyType);
			if (MinDistance <= 0.f)
			{
				return true;
			}

			const float Distance = GetXYDistance(CandidateAnchorKey, State.PlayerHQAnchorKey);
			return Distance >= 0.f && Distance >= MinDistance;
		}

		void UpdateBestEnemyDeclusterSwap(const FEnemyDeclusterAnchorScore& AnchorA,
		                                  const TPair<FGuid, EMapEnemyItem>& EnemyPairB,
		                                  const TMap<FGuid, FVector2D>& AnchorLocationsByKey,
		                                  FEnemyDeclusterSwapCandidate& InOutBestCandidate) const
		{
			TArray<FGuid> ImpactedAnchorKeys;
			BuildImpactedEnemyDeclusterAnchorKeys(AnchorA.AnchorKey, EnemyPairB.Key,
			                                      BuildSortedSwappableEnemyAnchorsCopy(),
			                                      AnchorLocationsByKey, ImpactedAnchorKeys);
			if (ImpactedAnchorKeys.Num() == 0)
			{
				return;
			}

			const int32 BeforeSum = ComputeImpactedEnemyDeclusterDensitySum(
				ImpactedAnchorKeys,
				State.EnemyItemsByAnchorKey,
				AnchorLocationsByKey);
			TMap<FGuid, EMapEnemyItem> EnemyTypesAfterSwap = State.EnemyItemsByAnchorKey;
			EnemyTypesAfterSwap.Add(AnchorA.AnchorKey, EnemyPairB.Value);
			EnemyTypesAfterSwap.Add(EnemyPairB.Key, AnchorA.EnemyType);
			const int32 AfterSum = ComputeImpactedEnemyDeclusterDensitySum(ImpactedAnchorKeys, EnemyTypesAfterSwap,
			                                                               AnchorLocationsByKey);
			const int32 Improvement = BeforeSum - AfterSum;
			if (Improvement <= 0 || not GetEnemyDeclusterSwapWins(AnchorA, EnemyPairB, Improvement, InOutBestCandidate))
			{
				return;
			}

			InOutBestCandidate.AnchorKeyA = AnchorA.AnchorKey;
			InOutBestCandidate.AnchorKeyB = EnemyPairB.Key;
			InOutBestCandidate.EnemyTypeA = AnchorA.EnemyType;
			InOutBestCandidate.EnemyTypeB = EnemyPairB.Value;
			InOutBestCandidate.Improvement = Improvement;
			InOutBestCandidate.bIsValid = true;
		}

		TArray<TPair<FGuid, EMapEnemyItem>> BuildSortedSwappableEnemyAnchorsCopy() const
		{
			TArray<TPair<FGuid, EMapEnemyItem>> SortedSwappableAnchors;
			BuildSortedSwappableEnemyAnchors(SortedSwappableAnchors);
			return SortedSwappableAnchors;
		}

		static bool GetEnemyDeclusterSwapWins(const FEnemyDeclusterAnchorScore& AnchorA,
		                                      const TPair<FGuid, EMapEnemyItem>& EnemyPairB,
		                                      const int32 Improvement,
		                                      const FEnemyDeclusterSwapCandidate& BestCandidate)
		{
			if (not BestCandidate.bIsValid || Improvement > BestCandidate.Improvement)
			{
				return true;
			}

			if (Improvement != BestCandidate.Improvement)
			{
				return false;
			}

			if (IsAnchorKeyLess(AnchorA.AnchorKey, BestCandidate.AnchorKeyA))
			{
				return true;
			}

			if (AnchorA.AnchorKey != BestCandidate.AnchorKeyA)
			{
				return false;
			}

			if (IsAnchorKeyLess(EnemyPairB.Key, BestCandidate.AnchorKeyB))
			{
				return true;
			}

			if (EnemyPairB.Key != BestCandidate.AnchorKeyB)
			{
				return false;
			}

			if (static_cast<uint8>(AnchorA.EnemyType) < static_cast<uint8>(BestCandidate.EnemyTypeA))
			{
				return true;
			}

			return AnchorA.EnemyType == BestCandidate.EnemyTypeA
				&& static_cast<uint8>(EnemyPairB.Value) < static_cast<uint8>(BestCandidate.EnemyTypeB);
		}

		void ApplyEnemyDeclusterSwap(const FEnemyDeclusterSwapCandidate& SwapCandidate)
		{
			State.EnemyItemsByAnchorKey.Add(SwapCandidate.AnchorKeyA, SwapCandidate.EnemyTypeB);
			State.EnemyItemsByAnchorKey.Add(SwapCandidate.AnchorKeyB, SwapCandidate.EnemyTypeA);
		}
	};
}

FPlacementResult SolvePlacement(
	const FPlacementSnapshot& Snapshot,
	const TSharedPtr<FPlacementProgress, ESPMode::ThreadSafe>& Progress)
{
	/* The local solver class stays private so the namespace exposes only the immutable data contract. */
	FWorldCampaignPlacementBacktrackingSolver Solver;
	return Solver.Solve(Snapshot, Progress);
}
}
