// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/CampaignGeneration/GeneratorWorldCampaign/GeneratorWorldCampaign.h"

#include "Algo/Sort.h"
#include "Async/Async.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Debugging/WorldCampaignDebugger.h"
#include "RTS_Survival/WorldCampaign/DeveloperSettings/WorldCampaignSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationHelpers/WorldCampaignGenerationHelper.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/GenerationHelpers/WorldCampaignPruningHelper.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldFortificationModificationsComponent.h"
#include "RTS_Survival/WorldCampaign/StrengthTypes/WorldStrengthEstimationComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMapObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldNeutralObject/WorldNeutralObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldPlayerObject/WorldPlayerObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Boundary/WorldSplineBoundary.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"
#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryOccupationRegulator.h"
#include "RTS_Survival/WorldCampaign/WorldData/WorldDataComponent.h"
#include "RTS_Survival/WorldCampaign/WorldDifficulty/WorldStrategicSupportArea.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace
{
	/**
	 * @brief Per-step retry ceiling that prevents one generation phase from retrying forever.
	 * A step attempt is incremented each time a specific step fails and enters HandleStepFailure.
	 * When exceeded, timeout fail-safe placement can run for supported steps before generation is aborted.
	 */
	constexpr int32 MaxStepAttempts = 15000;
	/**
	 * @brief Whole-generation retry ceiling shared by every step in the current generation run.
	 * This catches cross-step backtracking loops where no single step reaches MaxStepAttempts on its own.
	 * When exceeded, timeout fail-safe placement can run for supported steps before generation is aborted.
	 */
	constexpr int32 MaxTotalAttempts = 100000;
	constexpr int32 MaxPruningValidationAsyncSetupPasses = 16;
	constexpr int32 AttemptSeedMultiplier = 13;
	constexpr int32 MaxRelaxationAttempts = 3;
	constexpr int32 RelaxedHopDistanceMax = TNumericLimits<int32>::Max();
	constexpr float RelaxedDistanceMax = TNumericLimits<float>::Max();
	constexpr int32 MaxChokepointPairSamples = 48;
	constexpr int32 ChokepointSeedOffset = 7919;
	constexpr int32 AnchorPointSeedOffset = 15401;
	constexpr float AnchorPointGridJitterFraction = 0.49f;
	constexpr int32 AnchorPointJitterAttemptsMin = 1;
	constexpr int32 AnchorPointJitterAttemptsMax = 32;
	constexpr int32 MaxDebugGridCells = 400;
	constexpr float AnchorBoundaryLineThickness = 2.f;
	constexpr float AnchorGridLineThickness = 0.5f;
	constexpr float AnchorGridBoundsLineThickness = 1.f;
	constexpr float AnchorPointSpawnZ = 0.f;
	constexpr int32 PlayerHQForcePlacementSeedOffset = 4021;
	constexpr int32 EnemyHQForcePlacementSeedOffset = 4027;
	const FName GeneratedAnchorTag(TEXT("WC_GeneratedAnchor"));
	const TCHAR* const RepairedAnchorKeyPrefix = TEXT("WorldCampaign.RepairedAnchorKey");
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
	constexpr uint64 AnchorKeySeedSaltA = 0xA9B4C3D2E1F0ABCDull;
	constexpr uint64 AnchorKeySeedSaltB = 0x1D2C3B4A59687766ull;
	constexpr uint64 RepairedAnchorKeySeedSalt = 0x7658B42D9F13C0EAull;
	constexpr uint64 Lower32BitMask = 0xFFFFFFFFull;
	const TCHAR* const WorldCampaignSeedOverrideArgument = TEXT("RTSWorldCampaignSeed=");

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
	using CampaignGenerationHelper::GetTopologyPreferenceScore;
	using CampaignGenerationHelper::HasMinimumAdjacentMatches;

	struct FWorldDifficultyMapObjects
	{
		TArray<AWorldMapObject*> WorldObjects;
		TArray<AWorldEnemyObject*> EnemyObjects;
		TArray<AWorldMissionObject*> MissionObjects;
	};

	FWorldDifficultyMapObjects BuildWorldDifficultyMapObjects(
		const FWorldCampaignPlacementState& PlacementState)
	{
		FWorldDifficultyMapObjects MapObjects;
		MapObjects.WorldObjects.Reserve(PlacementState.CachedAnchors.Num());
		MapObjects.EnemyObjects.Reserve(PlacementState.EnemyItemsByAnchorKey.Num());
		MapObjects.MissionObjects.Reserve(PlacementState.MissionsByAnchorKey.Num());

		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : PlacementState.CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			AWorldMapObject* PromotedWorldObject = AnchorPoint->GetPromotedWorldObject();
			if (not IsValid(PromotedWorldObject))
			{
				continue;
			}

			MapObjects.WorldObjects.Add(PromotedWorldObject);

			if (AWorldEnemyObject* EnemyObject = Cast<AWorldEnemyObject>(PromotedWorldObject))
			{
				MapObjects.EnemyObjects.Add(EnemyObject);
				continue;
			}

			if (AWorldMissionObject* MissionObject = Cast<AWorldMissionObject>(PromotedWorldObject))
			{
				MapObjects.MissionObjects.Add(MissionObject);
			}
		}

		return MapObjects;
	}

	bool GetIsWithinXYRadius(const FVector& SourceLocation,
	                         const FVector& TargetLocation,
	                         const float XYRadius)
	{
		if (XYRadius <= 0.f)
		{
			return false;
		}

		const FVector2D SourceXY(SourceLocation.X, SourceLocation.Y);
		const FVector2D TargetXY(TargetLocation.X, TargetLocation.Y);
		return FVector2D::DistSquared(SourceXY, TargetXY) <= FMath::Square(XYRadius);
	}

	UWorldStrengthEstimationComponent* GetStrengthEstimationComponent(AWorldMapObject* WorldMapObject)
	{
		if (not IsValid(WorldMapObject))
		{
			return nullptr;
		}

		return WorldMapObject->GetWorldStrengthEstimationComponent();
	}

	void ResetStrategicReport(const FWorldDifficultyMapObjects& MapObjects)
	{
		for (AWorldEnemyObject* EnemyObject : MapObjects.EnemyObjects)
		{
			if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				GetStrengthEstimationComponent(EnemyObject))
			{
				StrengthEstimationComponent->ResetStrategicSupportReport();
			}
		}

		for (AWorldMissionObject* MissionObject : MapObjects.MissionObjects)
		{
			if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				GetStrengthEstimationComponent(MissionObject))
			{
				StrengthEstimationComponent->ResetStrategicSupportReport();
			}
		}
	}

	void ResetFieldDivisionReport(const FWorldDifficultyMapObjects& MapObjects)
	{
		for (AWorldEnemyObject* EnemyObject : MapObjects.EnemyObjects)
		{
			if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				GetStrengthEstimationComponent(EnemyObject))
			{
				StrengthEstimationComponent->ResetFieldDivisionReport();
			}
		}

		for (AWorldMissionObject* MissionObject : MapObjects.MissionObjects)
		{
			if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				GetStrengthEstimationComponent(MissionObject))
			{
				StrengthEstimationComponent->ResetFieldDivisionReport();
			}
		}
	}

	bool GetCanApplyStrategicSupportToTarget(const AWorldMapObject* SourceObject,
	                                         const AWorldMapObject* TargetObject,
	                                         const float XYRadius)
	{
		if (not IsValid(SourceObject) || not IsValid(TargetObject))
		{
			return false;
		}

		return GetIsWithinXYRadius(SourceObject->GetActorLocation(), TargetObject->GetActorLocation(), XYRadius);
	}

	void ApplyStrategicSupportToEnemyTargets(
		const AWorldMapObject* SourceObject,
		const EWorldStrategicSupport StrategicSupport,
		const FWorldStrengthReason& StrengthReason,
		const float XYRadius,
		const TArray<AWorldEnemyObject*>& EnemyObjects)
	{
		for (AWorldEnemyObject* EnemyObject : EnemyObjects)
		{
			if (not GetCanApplyStrategicSupportToTarget(SourceObject, EnemyObject, XYRadius))
			{
				continue;
			}

			if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				GetStrengthEstimationComponent(EnemyObject))
			{
				StrengthEstimationComponent->AddStrategicSupportReason(StrategicSupport, StrengthReason);
			}
		}
	}

	void ApplyStrategicSupportToMissionTargets(
		const AWorldMapObject* SourceObject,
		const EWorldStrategicSupport StrategicSupport,
		const FWorldStrengthReason& StrengthReason,
		const float XYRadius,
		const TArray<AWorldMissionObject*>& MissionObjects)
	{
		for (AWorldMissionObject* MissionObject : MissionObjects)
		{
			if (not GetCanApplyStrategicSupportToTarget(SourceObject, MissionObject, XYRadius))
			{
				continue;
			}

			if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				GetStrengthEstimationComponent(MissionObject))
			{
				StrengthEstimationComponent->AddStrategicSupportReason(StrategicSupport, StrengthReason);
			}
		}
	}

	void ApplyStrategicSupportToTargets(const AWorldMapObject* SourceObject,
	                                    UWorldStrategicSupportArea& StrategicSupportArea,
	                                    const FWorldDifficultyMapObjects& MapObjects,
	                                    const UWorldDataComponent& WorldDataComponent,
	                                    const ERTSGameDifficulty GameDifficulty)
	{
		/*
		 * The support component stores only an enum and radius. The actual percentage and player-facing reason are
		 * resolved through WorldData each turn so difficulty multipliers and data-asset edits stay centralized.
		 */
		const EWorldStrategicSupport StrategicSupport = StrategicSupportArea.GetStrategicSupport();
		FWorldStrengthReason StrengthReason;
		if (not WorldDataComponent.TryBuildStrategicSupportReason(
			StrategicSupport,
			GameDifficulty,
			StrengthReason))
		{
			StrategicSupportArea.SetCachedStrategicSupportPercentage(0);
			return;
		}

		StrategicSupportArea.SetCachedStrategicSupportPercentage(StrengthReason.StrengthPercent);
		if (not StrengthReason.GetHasStrength())
		{
			return;
		}

		const float XYRadius = StrategicSupportArea.GetXYRadius();
		ApplyStrategicSupportToEnemyTargets(
			SourceObject,
			StrategicSupport,
			StrengthReason,
			XYRadius,
			MapObjects.EnemyObjects);
		ApplyStrategicSupportToMissionTargets(
			SourceObject,
			StrategicSupport,
			StrengthReason,
			XYRadius,
			MapObjects.MissionObjects);
	}

	void ApplyStrategicSupportFromSource(AWorldMapObject* SourceObject,
	                                     const FWorldDifficultyMapObjects& MapObjects,
	                                     const UWorldDataComponent& WorldDataComponent,
	                                     const ERTSGameDifficulty GameDifficulty)
	{
		if (not IsValid(SourceObject))
		{
			return;
		}

		TArray<UWorldStrategicSupportArea*> StrategicSupportAreas;
		SourceObject->GetComponents<UWorldStrategicSupportArea>(StrategicSupportAreas);
		for (UWorldStrategicSupportArea* StrategicSupportArea : StrategicSupportAreas)
		{
			if (not IsValid(StrategicSupportArea))
			{
				continue;
			}

			ApplyStrategicSupportToTargets(
				SourceObject,
				*StrategicSupportArea,
				MapObjects,
				WorldDataComponent,
				GameDifficulty);
		}
	}

	void ApplyStrategicSupport(const FWorldDifficultyMapObjects& MapObjects,
	                           const UWorldDataComponent& WorldDataComponent,
	                           const ERTSGameDifficulty GameDifficulty)
	{
		for (AWorldMapObject* WorldObject : MapObjects.WorldObjects)
		{
			ApplyStrategicSupportFromSource(WorldObject, MapObjects, WorldDataComponent, GameDifficulty);
		}
	}

	struct FAnchorCandidate
	{
		TObjectPtr<AAnchorPoint> AnchorPoint = nullptr;
		float DistanceSquared = 0.f;
		FGuid AnchorKey;
	};

	struct FAnchorKeyRepairSource
	{
		TMap<FGuid, int32> AnchorKeyCounts;
		TArray<TObjectPtr<AAnchorPoint>> SortedAnchors;
	};

	struct FAnchorKeyRepairPlan
	{
		TSet<FGuid> UsedAnchorKeys;
		TArray<TObjectPtr<AAnchorPoint>> AnchorsNeedingKeyRepair;
		int32 MissingKeyCount = 0;
		int32 DuplicateKeyCount = 0;
	};

	struct FPlacementCandidate
	{
		TObjectPtr<AAnchorPoint> AnchorPoint = nullptr;
		FGuid AnchorKey;
		float Score = 0.f;
		int32 HopDistanceFromHQ = INDEX_NONE;
		int32 CandidateOrder = INDEX_NONE;
		TWeakObjectPtr<AAnchorPoint> CompanionAnchor;
		uint8 CompanionRawSubtype = 0;
		EMapItemType CompanionItemType = EMapItemType::None;
	};

	bool IsPlacementCandidateLess(const FPlacementCandidate& Left, const FPlacementCandidate& Right)
	{
		if (Left.Score != Right.Score)
		{
			return Left.Score > Right.Score;
		}

		if (Left.AnchorKey != Right.AnchorKey)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		}

		return Left.CandidateOrder < Right.CandidateOrder;
	}

	void SortAnchorsByDeterministicOrder(TArray<TObjectPtr<AAnchorPoint>>& InOutAnchors)
	{
		Algo::Sort(InOutAnchors, [](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
		{
			return AAnchorPoint::IsDeterministicAnchorOrderLess(Left.Get(), Right.Get());
		});
	}

	FGuid BuildRepairedAnchorKey(const AAnchorPoint* AnchorPoint, int32 RepairOrdinal, int32 CollisionOrdinal)
	{
		const FString KeySource = FString::Printf(
			TEXT("%s|%s|RepairOrdinal=%d|CollisionOrdinal=%d"),
			RepairedAnchorKeyPrefix,
			*AAnchorPoint::BuildStableIdentityString(AnchorPoint),
			RepairOrdinal,
			CollisionOrdinal);
		return FGuid::NewDeterministicGuid(KeySource, RepairedAnchorKeySeedSalt);
	}

	FGuid BuildUniqueRepairedAnchorKey(const AAnchorPoint* AnchorPoint, int32 RepairOrdinal,
	                                   const TSet<FGuid>& UsedAnchorKeys)
	{
		int32 CollisionOrdinal = 0;
		FGuid RepairedAnchorKey = BuildRepairedAnchorKey(AnchorPoint, RepairOrdinal, CollisionOrdinal);
		while (not RepairedAnchorKey.IsValid() || UsedAnchorKeys.Contains(RepairedAnchorKey))
		{
			CollisionOrdinal++;
			RepairedAnchorKey = BuildRepairedAnchorKey(AnchorPoint, RepairOrdinal, CollisionOrdinal);
		}

		return RepairedAnchorKey;
	}

	FAnchorKeyRepairSource BuildAnchorKeyRepairSource(const TArray<TObjectPtr<AAnchorPoint>>& AnchorPoints)
	{
		FAnchorKeyRepairSource RepairSource;
		RepairSource.SortedAnchors.Reserve(AnchorPoints.Num());
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorPoints)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			RepairSource.SortedAnchors.Add(AnchorPoint);
			const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
			if (AnchorKey.IsValid())
			{
				RepairSource.AnchorKeyCounts.FindOrAdd(AnchorKey)++;
			}
		}

		SortAnchorsByDeterministicOrder(RepairSource.SortedAnchors);
		return RepairSource;
	}

	void AddAnchorToRepairPlan(const TObjectPtr<AAnchorPoint>& AnchorPoint,
	                           const FAnchorKeyRepairSource& RepairSource,
	                           TSet<FGuid>& InOutKeptDuplicateAnchorKeys,
	                           FAnchorKeyRepairPlan& InOutRepairPlan)
	{
		const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
		if (not AnchorKey.IsValid())
		{
			InOutRepairPlan.MissingKeyCount++;
			InOutRepairPlan.AnchorsNeedingKeyRepair.Add(AnchorPoint);
			return;
		}

		if (RepairSource.AnchorKeyCounts.FindRef(AnchorKey) <= 1)
		{
			InOutRepairPlan.UsedAnchorKeys.Add(AnchorKey);
			return;
		}

		if (not InOutKeptDuplicateAnchorKeys.Contains(AnchorKey))
		{
			InOutKeptDuplicateAnchorKeys.Add(AnchorKey);
			InOutRepairPlan.UsedAnchorKeys.Add(AnchorKey);
			return;
		}

		InOutRepairPlan.DuplicateKeyCount++;
		InOutRepairPlan.AnchorsNeedingKeyRepair.Add(AnchorPoint);
	}

	FAnchorKeyRepairPlan BuildAnchorKeyRepairPlan(const FAnchorKeyRepairSource& RepairSource)
	{
		FAnchorKeyRepairPlan RepairPlan;
		TSet<FGuid> KeptDuplicateAnchorKeys;
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : RepairSource.SortedAnchors)
		{
			AddAnchorToRepairPlan(AnchorPoint, RepairSource, KeptDuplicateAnchorKeys, RepairPlan);
		}

		return RepairPlan;
	}

	void ApplyAnchorKeyRepairPlan(FAnchorKeyRepairPlan& InOutRepairPlan)
	{
		for (int32 RepairIndex = 0; RepairIndex < InOutRepairPlan.AnchorsNeedingKeyRepair.Num(); RepairIndex++)
		{
			AAnchorPoint* AnchorPoint = InOutRepairPlan.AnchorsNeedingKeyRepair[RepairIndex].Get();
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FGuid RepairedAnchorKey =
				BuildUniqueRepairedAnchorKey(AnchorPoint, RepairIndex, InOutRepairPlan.UsedAnchorKeys);
			AnchorPoint->SetAnchorKey(RepairedAnchorKey, true);
			InOutRepairPlan.UsedAnchorKeys.Add(RepairedAnchorKey);
		}
	}

	void LogAnchorKeyRepairPlan(const FAnchorKeyRepairPlan& RepairPlan)
	{
		if (RepairPlan.AnchorsNeedingKeyRepair.Num() == 0)
		{
			return;
		}

		UE_LOG(LogRTS, Warning,
		       TEXT("World campaign repaired %d anchor key(s) before connection generation. Missing=%d Duplicate=%d."),
		       RepairPlan.AnchorsNeedingKeyRepair.Num(),
		       RepairPlan.MissingKeyCount,
		       RepairPlan.DuplicateKeyCount);
	}

	int32 GetCampaignGenerationSeedWithCommandLineOverride(const int32 SettingsSeed)
	{
		int32 CommandLineSeed = SettingsSeed;
		if (not FParse::Value(FCommandLine::Get(), WorldCampaignSeedOverrideArgument, CommandLineSeed))
		{
			return SettingsSeed;
		}

		UE_LOG(LogRTS, Display,
		       TEXT("World campaign generation seed overridden from command line: %d -> %d."),
		       SettingsSeed,
		       CommandLineSeed);
		return CommandLineSeed;
	}

	struct FRuleRelaxationState
	{
		bool bRelaxDistance = false;
		bool bRelaxSpacing = false;
		bool bRelaxPreference = false;
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

	struct FClosestConnectionCandidate
	{
		TWeakObjectPtr<AConnection> Connection;
		FVector JunctionLocation = FVector::ZeroVector;
		float DistanceSquared = TNumericLimits<float>::Max();
	};

	struct FAsyncPlacementProgressLogState
	{
		FString Phase;
		ECampaignGenerationStep CurrentStep = ECampaignGenerationStep::NotStarted;
		ECampaignGenerationStep LastFailedStep = ECampaignGenerationStep::NotStarted;
		int32 TotalAttemptCount = 0;
		int32 WorkerAttemptDelta = 0;
		int32 WorkerPlacementAttemptCount = 0;
		int32 WorkerBacktrackCount = 0;
		int32 WorkerMicroBacktrackCount = 0;
		int32 WorkerMacroBacktrackCount = 0;
		int32 WorkerSetupBacktrackRequestCount = 0;
		int32 CurrentStepAttemptCount = 0;
		int32 TransactionCount = 0;
		int32 EnemyMicroPlacedCount = 0;
		int32 MissionMicroPlacedCount = 0;
		int32 NeutralTypeBeingEvaluated = 0;
		int32 NeutralEvaluatedAnchorCount = 0;
		int32 NeutralCandidateCount = 0;
		int32 NeutralRejectedOccupiedCount = 0;
		int32 NeutralRejectedNoHopCount = 0;
		int32 NeutralRejectedHopBandCount = 0;
		int32 NeutralRejectedSpacingCount = 0;
	};

	constexpr float SegmentIntersectionTolerance = 0.01f;
	constexpr int32 NoRequiredItems = 0;

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

	FGuid MakeDeterministicGuid(uint64 SeedA, uint64 SeedB)
	{
		const uint32 A = static_cast<uint32>(SeedA >> 32);
		const uint32 B = static_cast<uint32>(SeedA & Lower32BitMask);
		const uint32 C = static_cast<uint32>(SeedB >> 32);
		const uint32 D = static_cast<uint32>(SeedB & Lower32BitMask);
		return FGuid(A, B, C, D);
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

	bool ShouldUseHopWeightedMissionSelection(const FMissionTierRules& EffectiveRules,
	                                          const FMissionPlacement& MissionPlacementRules,
	                                          const ETopologySearchStrategy HopsPreference)
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

		return MissionPlacementRules.M_HopsPreferenceStrength > 0;
	}

	void BuildMissionScoreBandIndices(const TArray<FPlacementCandidate>& Candidates,
	                                  TArray<int32>& OutSelectableIndices)
	{
		const int32 ScoreBandMinCount = 3;
		const float ScoreBandTolerance = 0.001f;
		const int32 CandidateCount = Candidates.Num();
		const float BestScore = Candidates[0].Score;
		OutSelectableIndices.Reset();
		OutSelectableIndices.Reserve(CandidateCount);

		for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; CandidateIndex++)
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
		for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; CandidateIndex++)
		{
			OutSelectableIndices.Add(CandidateIndex);
		}
	}

	int32 GetTargetHopDistanceFromCandidates(const TArray<FPlacementCandidate>& Candidates,
	                                         const TArray<int32>& SelectableIndices,
	                                         const ETopologySearchStrategy HopsPreference)
	{
		int32 TargetHop = Candidates[SelectableIndices[0]].HopDistanceFromHQ;
		for (const int32 CandidateIndex : SelectableIndices)
		{
			const int32 HopDistance = Candidates[CandidateIndex].HopDistanceFromHQ;
			if (HopsPreference == ETopologySearchStrategy::PreferMin)
			{
				TargetHop = FMath::Min(TargetHop, HopDistance);
			}
			else
			{
				TargetHop = FMath::Max(TargetHop, HopDistance);
			}
		}

		return TargetHop;
	}

	int32 SelectMissionCandidateIndexDeterministic(const TArray<FPlacementCandidate>& Candidates,
	                                               const FMissionTierRules& EffectiveRules,
	                                               const FMissionPlacement& MissionPlacementRules,
	                                               const ETopologySearchStrategy HopsPreference,
	                                               const EMapMission MissionType,
	                                               const int32 AttemptIndex,
	                                               const int32 MissionIndex,
	                                               const int32 SeedUsed)
	{
		const int32 CandidateCount = Candidates.Num();
		if (not ShouldUseHopWeightedMissionSelection(EffectiveRules, MissionPlacementRules, HopsPreference))
		{
			return (AttemptIndex + MissionIndex) % CandidateCount;
		}

		TArray<int32> SelectableIndices;
		BuildMissionScoreBandIndices(Candidates, SelectableIndices);
		const int32 TargetHop = GetTargetHopDistanceFromCandidates(Candidates, SelectableIndices, HopsPreference);

		const int32 BaseMaxWeight = 32;
		const int32 BaseFalloffPerDelta = 8;
		const int32 StrengthMaxWeightMultiplier = 16;
		const int32 EffectiveMaxWeight = BaseMaxWeight
			+ MissionPlacementRules.M_HopsPreferenceStrength * StrengthMaxWeightMultiplier;
		const int32 EffectiveFalloff = FMath::Max(1, BaseFalloffPerDelta
		                                          - MissionPlacementRules.M_HopsPreferenceStrength);

		uint64 TotalWeight = 0;
		for (const int32 CandidateIndex : SelectableIndices)
		{
			const int32 Weight = GetHopPreferenceWeight(EffectiveMaxWeight, EffectiveFalloff,
			                                            Candidates[CandidateIndex].HopDistanceFromHQ, TargetHop);
			TotalWeight += static_cast<uint64>(Weight);
		}

		const uint64 HashValue = HashCombine64(static_cast<uint64>(SeedUsed),
		                                       static_cast<uint64>(MissionType),
		                                       static_cast<uint64>(MissionIndex),
		                                       static_cast<uint64>(AttemptIndex),
		                                       static_cast<uint64>(CandidateCount));
		const uint64 Pick = TotalWeight > 0 ? HashValue % TotalWeight : 0;

		uint64 CumulativeWeight = 0;
		for (const int32 CandidateIndex : SelectableIndices)
		{
			const int32 Weight = GetHopPreferenceWeight(EffectiveMaxWeight, EffectiveFalloff,
			                                            Candidates[CandidateIndex].HopDistanceFromHQ, TargetHop);
			CumulativeWeight += static_cast<uint64>(Weight);
			if (Pick < CumulativeWeight)
			{
				return CandidateIndex;
			}
		}

		return SelectableIndices.Last();
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

	bool TryGetAnchorForVirtualRealization(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                       const FGuid& AnchorKey,
	                                       const TCHAR* ErrorMessage,
	                                       AAnchorPoint*& OutAnchorPoint)
	{
		OutAnchorPoint = FindAnchorByKey(AnchorLookup, AnchorKey);
		if (IsValid(OutAnchorPoint))
		{
			return true;
		}

		RTSFunctionLibrary::ReportError(ErrorMessage);
		return false;
	}

	bool TryRealizeEnemyPlacement(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                              const TPair<FGuid, EMapEnemyItem>& EnemyPlacement)
	{
		AAnchorPoint* AnchorPoint = nullptr;
		if (not TryGetAnchorForVirtualRealization(
			AnchorLookup,
			EnemyPlacement.Key,
			TEXT("Virtual layout realization failed: enemy item anchor is invalid."),
			AnchorPoint))
		{
			return false;
		}

		AWorldMapObject* PromotedObject = AnchorPoint->OnEnemyItemPromotion(
			EnemyPlacement.Value,
			ECampaignGenerationStep::Finished);
		return IsValid(PromotedObject);
	}

	bool TryRealizeNeutralPlacement(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                const TPair<FGuid, EMapNeutralObjectType>& NeutralPlacement)
	{
		AAnchorPoint* AnchorPoint = nullptr;
		if (not TryGetAnchorForVirtualRealization(
			AnchorLookup,
			NeutralPlacement.Key,
			TEXT("Virtual layout realization failed: neutral item anchor is invalid."),
			AnchorPoint))
		{
			return false;
		}

		AWorldMapObject* PromotedObject = AnchorPoint->OnNeutralItemPromotion(
			NeutralPlacement.Value,
			ECampaignGenerationStep::Finished);
		return IsValid(PromotedObject);
	}

	bool TryRealizeMissionPlacement(const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                const TPair<FGuid, EMapMission>& MissionPlacement)
	{
		AAnchorPoint* AnchorPoint = nullptr;
		if (not TryGetAnchorForVirtualRealization(
			AnchorLookup,
			MissionPlacement.Key,
			TEXT("Virtual layout realization failed: mission anchor is invalid."),
			AnchorPoint))
		{
			return false;
		}

		AWorldMapObject* PromotedObject = AnchorPoint->OnMissionPromotion(
			MissionPlacement.Value,
			ECampaignGenerationStep::Finished);
		return IsValid(PromotedObject);
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

	bool IsTimeoutFailSafeStep(const ECampaignGenerationStep Step)
	{
		return Step == ECampaignGenerationStep::EnemyObjectsPlaced
			|| Step == ECampaignGenerationStep::NeutralObjectsPlaced
			|| Step == ECampaignGenerationStep::MissionsPlaced;
	}

	FString GetCampaignGenerationStepName(const ECampaignGenerationStep Step)
	{
		const UEnum* StepEnum = StaticEnum<ECampaignGenerationStep>();
		return StepEnum
			       ? StepEnum->GetNameStringByValue(static_cast<int64>(Step))
			       : FString::Printf(TEXT("Step_%d"), static_cast<int32>(Step));
	}

	void CopyAsyncPlacementProgressLogState(FWorldCampaignAsyncPlacementProgress& Progress,
	                                        FAsyncPlacementProgressLogState& OutState)
	{
		FScopeLock ProgressLock(&Progress.CriticalSection);
		OutState.Phase = Progress.Phase;
		OutState.CurrentStep = Progress.CurrentStep;
		OutState.LastFailedStep = Progress.LastFailedStep;
		OutState.TotalAttemptCount = Progress.TotalAttemptCount;
		OutState.WorkerAttemptDelta = Progress.WorkerAttemptDelta;
		OutState.WorkerPlacementAttemptCount = Progress.WorkerPlacementAttemptCount;
		OutState.WorkerBacktrackCount = Progress.WorkerBacktrackCount;
		OutState.WorkerMicroBacktrackCount = Progress.WorkerMicroBacktrackCount;
		OutState.WorkerMacroBacktrackCount = Progress.WorkerMacroBacktrackCount;
		OutState.WorkerSetupBacktrackRequestCount = Progress.WorkerSetupBacktrackRequestCount;
		OutState.CurrentStepAttemptCount = Progress.CurrentStepAttemptCount;
		OutState.TransactionCount = Progress.TransactionCount;
		OutState.EnemyMicroPlacedCount = Progress.EnemyMicroPlacedCount;
		OutState.MissionMicroPlacedCount = Progress.MissionMicroPlacedCount;
		OutState.NeutralTypeBeingEvaluated = Progress.NeutralTypeBeingEvaluated;
		OutState.NeutralEvaluatedAnchorCount = Progress.NeutralEvaluatedAnchorCount;
		OutState.NeutralCandidateCount = Progress.NeutralCandidateCount;
		OutState.NeutralRejectedOccupiedCount = Progress.NeutralRejectedOccupiedCount;
		OutState.NeutralRejectedNoHopCount = Progress.NeutralRejectedNoHopCount;
		OutState.NeutralRejectedHopBandCount = Progress.NeutralRejectedHopBandCount;
		OutState.NeutralRejectedSpacingCount = Progress.NeutralRejectedSpacingCount;
	}

	void LogAsyncPlacementProgressState(const FAsyncPlacementProgressLogState& ProgressState,
	                                    const double ElapsedSeconds)
	{
		UE_LOG(LogRTS, Warning,
		       TEXT(
			       "Async world campaign placement still running after %.1fs. Phase='%s', Step=%s, LastFailed=%s, FailureRetries=%d, WorkerFailureRetries=%d, PlacementAttempts=%d, Backtracks=%d, MicroBacktracks=%d, MacroBacktracks=%d, SetupBacktrackRequests=%d, StepAttempts=%d, Transactions=%d, EnemyMicro=%d, MissionMicro=%d, NeutralType=%d, NeutralEvaluated=%d, NeutralCandidates=%d, NeutralRejectedOccupied=%d, NeutralRejectedNoHop=%d, NeutralRejectedHopBand=%d, NeutralRejectedSpacing=%d."
		       ),
		       ElapsedSeconds,
		       *ProgressState.Phase,
		       *GetCampaignGenerationStepName(ProgressState.CurrentStep),
		       *GetCampaignGenerationStepName(ProgressState.LastFailedStep),
		       ProgressState.TotalAttemptCount,
		       ProgressState.WorkerAttemptDelta,
		       ProgressState.WorkerPlacementAttemptCount,
		       ProgressState.WorkerBacktrackCount,
		       ProgressState.WorkerMicroBacktrackCount,
		       ProgressState.WorkerMacroBacktrackCount,
		       ProgressState.WorkerSetupBacktrackRequestCount,
		       ProgressState.CurrentStepAttemptCount,
		       ProgressState.TransactionCount,
		       ProgressState.EnemyMicroPlacedCount,
		       ProgressState.MissionMicroPlacedCount,
		       ProgressState.NeutralTypeBeingEvaluated,
		       ProgressState.NeutralEvaluatedAnchorCount,
		       ProgressState.NeutralCandidateCount,
		       ProgressState.NeutralRejectedOccupiedCount,
		       ProgressState.NeutralRejectedNoHopCount,
		       ProgressState.NeutralRejectedHopBandCount,
		       ProgressState.NeutralRejectedSpacingCount);
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

	void BuildFailSafeEmptyAnchors(const TArray<TObjectPtr<AAnchorPoint>>& CandidateAnchors,
	                               const FWorldCampaignPlacementState& PlacementState,
	                               const AAnchorPoint* PlayerHQAnchor,
	                               TArray<FEmptyAnchorDistance>& OutEmptyAnchors,
	                               bool& bOutHasValidHQ)
	{
		bOutHasValidHQ = IsValid(PlayerHQAnchor);
		OutEmptyAnchors.Reset();
		if (CandidateAnchors.Num() == 0)
		{
			return;
		}

		const FVector PlayerHQWorldLocation = bOutHasValidHQ
			                                      ? PlayerHQAnchor->GetActorLocation()
			                                      : FVector::ZeroVector;
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CandidateAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
			if (IsAnchorOccupied(AnchorKey, PlacementState))
			{
				continue;
			}

			FEmptyAnchorDistance Entry;
			Entry.AnchorPoint = AnchorPoint;
			Entry.AnchorKey = AnchorKey;
			if (bOutHasValidHQ)
			{
				const FVector AnchorLocation = AnchorPoint->GetActorLocation();
				const float DeltaX = AnchorLocation.X - PlayerHQWorldLocation.X;
				const float DeltaY = AnchorLocation.Y - PlayerHQWorldLocation.Y;
				Entry.DistanceSquared = DeltaX * DeltaX + DeltaY * DeltaY;
			}

			OutEmptyAnchors.Add(Entry);
		}

		constexpr float DistanceTolerance = 0.001f;
		OutEmptyAnchors.Sort([](const FEmptyAnchorDistance& Left, const FEmptyAnchorDistance& Right)
		{
			if (not FMath::IsNearlyEqual(Left.DistanceSquared, Right.DistanceSquared, DistanceTolerance))
			{
				return Left.DistanceSquared < Right.DistanceSquared;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});
	}

	int32 GetCachedHopDistance(const TMap<FGuid, int32>& HopDistancesByAnchorKey, const FGuid& AnchorKey)
	{
		const int32* CachedDistance = HopDistancesByAnchorKey.Find(AnchorKey);
		return CachedDistance ? *CachedDistance : INDEX_NONE;
	}

	bool IsPointInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
	{
		const int32 PointCount = Polygon.Num();
		if (PointCount < 3)
		{
			return false;
		}

		bool bIsInside = false;
		for (int32 PointIndex = 0, PreviousIndex = PointCount - 1; PointIndex < PointCount; PreviousIndex = PointIndex
		     ++)
		{
			const FVector2D& CurrentPoint = Polygon[PointIndex];
			const FVector2D& PreviousPoint = Polygon[PreviousIndex];
			const bool bShouldTest = (CurrentPoint.Y > Point.Y) != (PreviousPoint.Y > Point.Y);
			if (not bShouldTest)
			{
				continue;
			}

			const float Denominator = PreviousPoint.Y - CurrentPoint.Y;
			if (FMath::IsNearlyZero(Denominator))
			{
				continue;
			}

			const float IntersectionX = (PreviousPoint.X - CurrentPoint.X)
				* (Point.Y - CurrentPoint.Y) / Denominator + CurrentPoint.X;
			if (Point.X < IntersectionX)
			{
				bIsInside = not bIsInside;
			}
		}

		return bIsInside;
	}

	void GetPolygonBounds(const TArray<FVector2D>& Polygon, FVector2D& OutMin, FVector2D& OutMax)
	{
		OutMin = FVector2D::ZeroVector;
		OutMax = FVector2D::ZeroVector;
		if (Polygon.Num() == 0)
		{
			return;
		}

		OutMin = Polygon[0];
		OutMax = Polygon[0];
		for (int32 PointIndex = 1; PointIndex < Polygon.Num(); PointIndex++)
		{
			const FVector2D& Point = Polygon[PointIndex];
			OutMin.X = FMath::Min(OutMin.X, Point.X);
			OutMin.Y = FMath::Min(OutMin.Y, Point.Y);
			OutMax.X = FMath::Max(OutMax.X, Point.X);
			OutMax.Y = FMath::Max(OutMax.Y, Point.Y);
		}
	}

	bool IsFarEnoughFromAnchors(const FVector2D& Candidate, const TArray<TObjectPtr<AAnchorPoint>>& ExistingAnchors,
	                            const TArray<TObjectPtr<AAnchorPoint>>& NewAnchors, float MinDistanceSquared)
	{
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : ExistingAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FVector AnchorLocation = AnchorPoint->GetActorLocation();
			const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
			if (FVector2D::DistSquared(AnchorLocation2D, Candidate) < MinDistanceSquared)
			{
				return false;
			}
		}

		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : NewAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FVector AnchorLocation = AnchorPoint->GetActorLocation();
			const FVector2D AnchorLocation2D(AnchorLocation.X, AnchorLocation.Y);
			if (FVector2D::DistSquared(AnchorLocation2D, Candidate) < MinDistanceSquared)
			{
				return false;
			}
		}

		return true;
	}

	bool TryGetSingleSplineBoundary(UWorld* World, AWorldSplineBoundary*& OutBoundary)
	{
		OutBoundary = nullptr;
		if (not IsValid(World))
		{
			return false;
		}

		int32 BoundaryCount = 0;
		AWorldSplineBoundary* SingleBoundary = nullptr;
		TArray<FString> BoundaryNames;
		for (TActorIterator<AWorldSplineBoundary> It(World); It; ++It)
		{
			AWorldSplineBoundary* Boundary = *It;
			if (not IsValid(Boundary))
			{
				continue;
			}

			BoundaryCount++;
			BoundaryNames.Add(Boundary->GetName());
			if (BoundaryCount == 1)
			{
				SingleBoundary = Boundary;
			}
		}

		if (BoundaryCount == 1 && IsValid(SingleBoundary))
		{
			OutBoundary = SingleBoundary;
			return true;
		}

		if (BoundaryCount == 0)
		{
			RTSFunctionLibrary::ReportError(TEXT("Anchor point generation failed: no WorldSplineBoundary found."));
			return false;
		}

		const FString BoundaryList = BoundaryNames.Num() > 0
			                             ? FString::Join(BoundaryNames, TEXT(", "))
			                             : TEXT("UnknownBoundaries");
		const FString ErrorMessage = FString::Printf(
			TEXT("Anchor point generation failed: multiple WorldSplineBoundary actors found (%s)."),
			*BoundaryList);
		RTSFunctionLibrary::ReportError(ErrorMessage);
		OutBoundary = nullptr;
		return false;
	}

	struct FAnchorPointGridDefinition
	{
		FVector2D BoundsMin = FVector2D::ZeroVector;
		FVector2D BoundsMax = FVector2D::ZeroVector;
		int32 GridCountX = 0;
		int32 GridCountY = 0;
		int32 CellCount = 0;
		float CellSize = 0.f;
		float JitterRange = 0.f;
		int32 StartIndex = 0;
	};

	bool ValidateAnchorPointGenerationSettings(const FAnchorPointGenerationSettings& Settings, FString& OutError)
	{
		if (Settings.M_MinNewAnchorPoints < 0)
		{
			OutError = TEXT("Anchor point generation failed: MinNewAnchorPoints must be >= 0.");
			return false;
		}

		if (Settings.M_MaxNewAnchorPoints < Settings.M_MinNewAnchorPoints)
		{
			OutError = TEXT("Anchor point generation failed: MaxNewAnchorPoints < MinNewAnchorPoints.");
			return false;
		}

		if (Settings.M_GridCellSize <= 0.f)
		{
			OutError = TEXT("Anchor point generation failed: GridCellSize must be > 0.");
			return false;
		}

		if (Settings.M_MinDistanceBetweenAnchorPoints < 0.f)
		{
			OutError = TEXT("Anchor point generation failed: MinDistanceBetweenAnchorPoints must be >= 0.");
			return false;
		}

		if (Settings.M_JitterAttemptsPerCell < AnchorPointJitterAttemptsMin
			|| Settings.M_JitterAttemptsPerCell > AnchorPointJitterAttemptsMax)
		{
			OutError = FString::Printf(
				TEXT("Anchor point generation failed: JitterAttemptsPerCell must be between %d and %d."),
				AnchorPointJitterAttemptsMin,
				AnchorPointJitterAttemptsMax);
			return false;
		}

		if (Settings.M_SplineSampleSpacing <= 0.f)
		{
			OutError = TEXT("Anchor point generation failed: SplineSampleSpacing must be > 0.");
			return false;
		}

		return true;
	}

	bool TryBuildSplineBoundaryPolygon(UWorld* World, const FAnchorPointGenerationSettings& Settings,
	                                   TArray<FVector2D>& OutPolygon)
	{
		OutPolygon.Reset();
		AWorldSplineBoundary* Boundary = nullptr;
		if (not TryGetSingleSplineBoundary(World, Boundary))
		{
			return false;
		}

		Boundary->EnsureClosedLoop();
		Boundary->GetSampledPolygon2D(Settings.M_SplineSampleSpacing, OutPolygon);
		if (OutPolygon.Num() < 3)
		{
			RTSFunctionLibrary::ReportError(
				TEXT("Anchor point generation failed: spline boundary polygon is invalid."));
			return false;
		}

		return true;
	}

	bool BuildAnchorPointGridDefinition(const TArray<FVector2D>& Polygon, float CellSize, FRandomStream& RandomStream,
	                                    FAnchorPointGridDefinition& OutGrid)
	{
		if (CellSize <= 0.f)
		{
			return false;
		}

		GetPolygonBounds(Polygon, OutGrid.BoundsMin, OutGrid.BoundsMax);
		OutGrid.CellSize = CellSize;
		OutGrid.GridCountX = FMath::Max(1, FMath::CeilToInt((OutGrid.BoundsMax.X - OutGrid.BoundsMin.X) / CellSize));
		OutGrid.GridCountY = FMath::Max(1, FMath::CeilToInt((OutGrid.BoundsMax.Y - OutGrid.BoundsMin.Y) / CellSize));
		OutGrid.CellCount = OutGrid.GridCountX * OutGrid.GridCountY;
		if (OutGrid.CellCount <= 0)
		{
			return false;
		}

		OutGrid.StartIndex = RandomStream.RandRange(0, OutGrid.CellCount - 1);
		OutGrid.JitterRange = CellSize * AnchorPointGridJitterFraction;
		return true;
	}

	struct FAnchorPointGridCellInfo
	{
		int32 CellIndex = INDEX_NONE;
		FVector2D CellMin = FVector2D::ZeroVector;
		FVector2D CellMax = FVector2D::ZeroVector;
		FVector2D CellCenter = FVector2D::ZeroVector;
	};

	FAnchorPointGridCellInfo BuildAnchorPointGridCellInfo(const FAnchorPointGridDefinition& GridDefinition,
	                                                      int32 OffsetIndex)
	{
		FAnchorPointGridCellInfo CellInfo;
		CellInfo.CellIndex = (GridDefinition.StartIndex + OffsetIndex) % GridDefinition.CellCount;
		const int32 CellX = CellInfo.CellIndex % GridDefinition.GridCountX;
		const int32 CellY = CellInfo.CellIndex / GridDefinition.GridCountX;
		CellInfo.CellMin = FVector2D(GridDefinition.BoundsMin.X + CellX * GridDefinition.CellSize,
		                             GridDefinition.BoundsMin.Y + CellY * GridDefinition.CellSize);
		CellInfo.CellMax = FVector2D(CellInfo.CellMin.X + GridDefinition.CellSize,
		                             CellInfo.CellMin.Y + GridDefinition.CellSize);
		CellInfo.CellCenter = FVector2D(CellInfo.CellMin.X + GridDefinition.CellSize * 0.5f,
		                                CellInfo.CellMin.Y + GridDefinition.CellSize * 0.5f);
		return CellInfo;
	}

	FVector2D BuildJitteredAnchorCandidate(const FAnchorPointGridDefinition& GridDefinition,
	                                       const FAnchorPointGridCellInfo& CellInfo,
	                                       FRandomStream& RandomStream)
	{
		const float JitterX = RandomStream.FRandRange(-GridDefinition.JitterRange, GridDefinition.JitterRange);
		const float JitterY = RandomStream.FRandRange(-GridDefinition.JitterRange, GridDefinition.JitterRange);
		FVector2D Candidate(CellInfo.CellCenter.X + JitterX, CellInfo.CellCenter.Y + JitterY);
		Candidate.X = FMath::Clamp(Candidate.X, CellInfo.CellMin.X, CellInfo.CellMax.X);
		Candidate.Y = FMath::Clamp(Candidate.Y, CellInfo.CellMin.Y, CellInfo.CellMax.Y);
		return Candidate;
	}

	bool TrySpawnGeneratedAnchorAtCandidate(UWorld* World,
	                                        const AGeneratorWorldCampaign* Generator,
	                                        TSubclassOf<AAnchorPoint> AnchorClass,
	                                        const FVector2D& Candidate,
	                                        int32 StepAttemptIndex,
	                                        int32 CellIndex,
	                                        int32 SpawnOrdinal,
	                                        FCampaignGenerationStepTransaction& OutTransaction,
	                                        TArray<TObjectPtr<AAnchorPoint>>& OutNewAnchors)
	{
		const FVector SpawnLocation(Candidate.X, Candidate.Y, AnchorPointSpawnZ);
		const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
		AAnchorPoint* SpawnedAnchor = World->SpawnActorDeferred<AAnchorPoint>(
			AnchorClass, SpawnTransform, nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (not IsValid(SpawnedAnchor))
		{
			RTSFunctionLibrary::ReportError(TEXT("Anchor point generation failed: spawn failed."));
			return false;
		}

		const FGuid DeterministicKey = Generator->BuildGeneratedAnchorKey_Deterministic(
			StepAttemptIndex,
			CellIndex,
			SpawnOrdinal);
		SpawnedAnchor->SetAnchorKey(DeterministicKey, true);
		SpawnedAnchor->FinishSpawning(SpawnTransform);
		SpawnedAnchor->Tags.AddUnique(GeneratedAnchorTag);
		OutTransaction.SpawnedActors.Add(SpawnedAnchor);
		OutNewAnchors.Add(SpawnedAnchor);
		return true;
	}

	bool CanSpawnAnchorsInGrid(UWorld* World, const AGeneratorWorldCampaign* Generator, int32 JitterAttemptsPerCell)
	{
		if (not IsValid(World))
		{
			return false;
		}

		if (not IsValid(Generator))
		{
			return false;
		}

		return JitterAttemptsPerCell >= AnchorPointJitterAttemptsMin;
	}

	bool SpawnAnchorsInGrid(UWorld* World, const AGeneratorWorldCampaign* Generator,
	                        TSubclassOf<AAnchorPoint> AnchorClass, FRandomStream& RandomStream,
	                        const FAnchorPointGridDefinition& GridDefinition,
	                        const TArray<FVector2D>& BoundaryPolygon,
	                        const TArray<TObjectPtr<AAnchorPoint>>& ExistingAnchors, int32 TargetCount,
	                        int32 StepAttemptIndex, int32 JitterAttemptsPerCell,
	                        FCampaignGenerationStepTransaction& OutTransaction,
	                        TArray<TObjectPtr<AAnchorPoint>>& OutNewAnchors, float MinDistanceSquared)
	{
		if (not CanSpawnAnchorsInGrid(World, Generator, JitterAttemptsPerCell))
		{
			return false;
		}

		int32 SpawnOrdinal = 0;
		for (int32 OffsetIndex = 0; OffsetIndex < GridDefinition.CellCount; OffsetIndex++)
		{
			if (OutNewAnchors.Num() >= TargetCount)
			{
				break;
			}

			const FAnchorPointGridCellInfo CellInfo = BuildAnchorPointGridCellInfo(GridDefinition, OffsetIndex);
			for (int32 AttemptInCell = 0; AttemptInCell < JitterAttemptsPerCell; AttemptInCell++)
			{
				if (OutNewAnchors.Num() >= TargetCount)
				{
					break;
				}

				const FVector2D Candidate = BuildJitteredAnchorCandidate(GridDefinition, CellInfo, RandomStream);
				if (not IsPointInsidePolygon(Candidate, BoundaryPolygon))
				{
					continue;
				}

				if (not IsFarEnoughFromAnchors(Candidate, ExistingAnchors, OutNewAnchors, MinDistanceSquared))
				{
					continue;
				}

				if (not TrySpawnGeneratedAnchorAtCandidate(World, Generator, AnchorClass, Candidate, StepAttemptIndex,
				                                           CellInfo.CellIndex, SpawnOrdinal, OutTransaction,
				                                           OutNewAnchors))
				{
					return false;
				}

				SpawnOrdinal++;
				break;
			}
		}

		return true;
	}

	void DrawBoundaryPolygon(UWorld* World, const TArray<FVector2D>& BoundaryPolygon, float HeightOffset,
	                         float Duration, const FColor& LineColor)
	{
		if (not IsValid(World))
		{
			return;
		}

		for (int32 PointIndex = 0; PointIndex < BoundaryPolygon.Num(); PointIndex++)
		{
			const int32 NextIndex = (PointIndex + 1) % BoundaryPolygon.Num();
			const FVector Start(BoundaryPolygon[PointIndex].X, BoundaryPolygon[PointIndex].Y, HeightOffset);
			const FVector End(BoundaryPolygon[NextIndex].X, BoundaryPolygon[NextIndex].Y, HeightOffset);
			DrawDebugLine(World, Start, End, LineColor, false, Duration, 0, AnchorBoundaryLineThickness);
		}
	}

	void DrawAnchorGrid(UWorld* World, const TArray<FVector2D>& BoundaryPolygon, float CellSize, float HeightOffset,
	                    float Duration)
	{
		if (not IsValid(World))
		{
			return;
		}

		if (CellSize <= 0.f)
		{
			return;
		}

		FVector2D BoundsMin;
		FVector2D BoundsMax;
		GetPolygonBounds(BoundaryPolygon, BoundsMin, BoundsMax);

		const int32 GridCountX = FMath::Max(1, FMath::CeilToInt((BoundsMax.X - BoundsMin.X) / CellSize));
		const int32 GridCountY = FMath::Max(1, FMath::CeilToInt((BoundsMax.Y - BoundsMin.Y) / CellSize));
		const int32 CellCount = GridCountX * GridCountY;

		const FColor GridColor = FColor::Cyan;
		if (CellCount > MaxDebugGridCells)
		{
			const FVector BoxMin(BoundsMin.X, BoundsMin.Y, HeightOffset);
			const FVector BoxMax(BoundsMax.X, BoundsMax.Y, HeightOffset);
			DrawDebugLine(World, BoxMin, FVector(BoxMax.X, BoxMin.Y, HeightOffset), GridColor, false, Duration, 0,
			              AnchorGridBoundsLineThickness);
			DrawDebugLine(World, FVector(BoxMax.X, BoxMin.Y, HeightOffset), BoxMax, GridColor, false, Duration, 0,
			              AnchorGridBoundsLineThickness);
			DrawDebugLine(World, BoxMax, FVector(BoxMin.X, BoxMax.Y, HeightOffset), GridColor, false, Duration, 0,
			              AnchorGridBoundsLineThickness);
			DrawDebugLine(World, FVector(BoxMin.X, BoxMax.Y, HeightOffset), BoxMin, GridColor, false, Duration, 0,
			              AnchorGridBoundsLineThickness);
			return;
		}

		for (int32 CellIndex = 0; CellIndex < CellCount; CellIndex++)
		{
			const int32 CellX = CellIndex % GridCountX;
			const int32 CellY = CellIndex / GridCountX;
			const FVector2D CellMin(BoundsMin.X + CellX * CellSize, BoundsMin.Y + CellY * CellSize);
			const FVector2D CellMax(CellMin.X + CellSize, CellMin.Y + CellSize);
			const FVector BottomLeft(CellMin.X, CellMin.Y, HeightOffset);
			const FVector BottomRight(CellMax.X, CellMin.Y, HeightOffset);
			const FVector TopRight(CellMax.X, CellMax.Y, HeightOffset);
			const FVector TopLeft(CellMin.X, CellMax.Y, HeightOffset);

			DrawDebugLine(World, BottomLeft, BottomRight, GridColor, false, Duration, 0, AnchorGridLineThickness);
			DrawDebugLine(World, BottomRight, TopRight, GridColor, false, Duration, 0, AnchorGridLineThickness);
			DrawDebugLine(World, TopRight, TopLeft, GridColor, false, Duration, 0, AnchorGridLineThickness);
			DrawDebugLine(World, TopLeft, BottomLeft, GridColor, false, Duration, 0, AnchorGridLineThickness);
		}
	}

	void BuildShortestPathPredecessors(const FGuid& StartKey,
	                                   const FGuid& TargetKey,
	                                   const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                   TSet<FGuid>& OutVisited,
	                                   TMap<FGuid, FGuid>& OutPredecessorByKey)
	{
		TArray<FGuid> Queue;
		Queue.Add(StartKey);
		OutVisited.Reset();
		OutVisited.Add(StartKey);
		OutPredecessorByKey.Reset();

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
				if (OutVisited.Contains(NeighborKey))
				{
					continue;
				}

				OutVisited.Add(NeighborKey);
				OutPredecessorByKey.Add(NeighborKey, CurrentKey);
				Queue.Add(NeighborKey);
			}
		}
	}

	bool BuildPathFromPredecessors(const FGuid& StartKey,
	                               const FGuid& TargetKey,
	                               const TMap<FGuid, FGuid>& PredecessorByKey,
	                               TArray<FGuid>& OutPathKeys)
	{
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

		TSet<FGuid> Visited;
		TMap<FGuid, FGuid> PredecessorByKey;
		BuildShortestPathPredecessors(StartKey, TargetKey, AnchorLookup, Visited, PredecessorByKey);

		if (not Visited.Contains(TargetKey))
		{
			return false;
		}

		return BuildPathFromPredecessors(StartKey, TargetKey, PredecessorByKey, OutPathKeys);
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


	bool HasNeutralTypeAtAnchor(const FWorldCampaignPlacementState& PlacementState, const FGuid& AnchorKey,
	                            EMapNeutralObjectType RequiredNeutralType)
	{
		const EMapNeutralObjectType* NeutralType = PlacementState.NeutralItemsByAnchorKey.Find(AnchorKey);
		return NeutralType && *NeutralType == RequiredNeutralType;
	}

	TArray<FGuid> BuildSortedMissionAnchorKeys(const FWorldCampaignPlacementState& PlacementState)
	{
		TArray<FGuid> SortedMissionAnchorKeys;
		SortedMissionAnchorKeys.Reserve(PlacementState.MissionsByAnchorKey.Num());
		for (const TPair<FGuid, EMapMission>& MissionPair : PlacementState.MissionsByAnchorKey)
		{
			SortedMissionAnchorKeys.Add(MissionPair.Key);
		}

		Algo::Sort(SortedMissionAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});
		return SortedMissionAnchorKeys;
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

	void DebugEnemyPlacementRejectedIfEnabled(AGeneratorWorldCampaign& Generator,
	                                          AAnchorPoint* CandidateAnchor,
	                                          EMapEnemyItem EnemyType,
	                                          const TCHAR* Reason)
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (not Generator.GetIsValidCampaignDebugger())
			{
				return;
			}

			UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
			CampaignDebugger->DebugEnemyPlacementRejected(CandidateAnchor, EnemyType, Reason);
		}
	}

	bool TryBuildEnemyPlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                     const FEnemyItemPlacementRules& EffectiveRules,
	                                     EMapEnemyItem EnemyType,
	                                     int32 CandidateOrder,
	                                     int32 SafeZoneMaxHops,
	                                     AAnchorPoint* CandidateAnchor,
	                                     const FWorldCampaignPlacementState& WorkingPlacementState,
	                                     const FWorldCampaignDerivedData& WorkingDerivedData,
	                                     const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                     FPlacementCandidate& OutCandidate)
	{
		const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
		if (IsAnchorOccupied(CandidateKey, WorkingPlacementState))
		{
			DebugEnemyPlacementRejectedIfEnabled(Generator, CandidateAnchor, EnemyType, TEXT("occupied"));
			return false;
		}

		const int32 HopDistanceFromEnemyHQ = GetCachedHopDistance(
			WorkingDerivedData.EnemyHQHopDistancesByAnchorKey,
			CandidateKey);
		if (HopDistanceFromEnemyHQ == INDEX_NONE)
		{
			DebugEnemyPlacementRejectedIfEnabled(Generator, CandidateAnchor, EnemyType, TEXT("no hop cache"));
			return false;
		}

		const int32 MinHopsFromEnemyHQ = EffectiveRules.EnemyHQSpacing.MinHopsFromEnemyHQ;
		const int32 MaxHopsFromEnemyHQ = FMath::Max(MinHopsFromEnemyHQ,
		                                            EffectiveRules.EnemyHQSpacing.MaxHopsFromEnemyHQ);
		if (HopDistanceFromEnemyHQ < MinHopsFromEnemyHQ || HopDistanceFromEnemyHQ > MaxHopsFromEnemyHQ)
		{
			DebugEnemyPlacementRejectedIfEnabled(Generator, CandidateAnchor, EnemyType,
			                                     TEXT("enemyHQ hops out of bounds"));
			return false;
		}

		if (SafeZoneMaxHops > 0)
		{
			const int32 HopDistanceFromPlayerHQ = GetCachedHopDistance(
				WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
				CandidateKey);
			if (HopDistanceFromPlayerHQ != INDEX_NONE && HopDistanceFromPlayerHQ <= SafeZoneMaxHops)
			{
				DebugEnemyPlacementRejectedIfEnabled(Generator, CandidateAnchor, EnemyType, TEXT("safe zone"));
				return false;
			}
		}

		if (not PassesEnemySpacingRules(EffectiveRules, CandidateAnchor, EnemyType, WorkingPlacementState,
		                                AnchorLookup))
		{
			DebugEnemyPlacementRejectedIfEnabled(Generator, CandidateAnchor, EnemyType,
			                                     TEXT("intersects spacing"));
			return false;
		}

		const int32 ConnectionDegree = Generator.GetAnchorConnectionDegree(CandidateAnchor);
		const float ChokepointScore = WorkingDerivedData.ChokepointScoresByAnchorKey.FindRef(CandidateKey);
		OutCandidate = FPlacementCandidate();
		OutCandidate.AnchorPoint = CandidateAnchor;
		OutCandidate.AnchorKey = CandidateKey;
		OutCandidate.Score = GetEnemyPreferenceScore(EffectiveRules.EnemyHQSpacing.Preference, ConnectionDegree,
		                                             ChokepointScore, HopDistanceFromEnemyHQ,
		                                             MinHopsFromEnemyHQ, MaxHopsFromEnemyHQ);
		OutCandidate.HopDistanceFromHQ = HopDistanceFromEnemyHQ;
		OutCandidate.CandidateOrder = CandidateOrder;
		return true;
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
		int32 SourceOrder = 0;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : WorkingPlacementState.CachedAnchors)
		{
			const int32 CandidateOrder = SourceOrder++;
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			FPlacementCandidate Candidate;
			if (not TryBuildEnemyPlacementCandidate(Generator, EffectiveRules, EnemyType, CandidateOrder,
			                                        SafeZoneMaxHops, CandidateAnchor, WorkingPlacementState,
			                                        WorkingDerivedData, AnchorLookup, Candidate))
			{
				continue;
			}

			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, IsPlacementCandidateLess);

		const int32 CandidateIndex = (AttemptIndex + ExistingCount) % Candidates.Num();
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	struct FEnemyPlacementVariantDebugData
	{
		int32 SelectedVariantIndex = INDEX_NONE;
		int32 EnabledVariantCount = 0;
		bool bUsedVariant = false;
	};

	FEnemyItemPlacementRules BuildEffectiveEnemyPlacementRules(const FEnemyItemRuleset& Ruleset,
	                                                           int32 ExistingCount,
	                                                           const FRuleRelaxationState& RelaxationState,
	                                                           FEnemyPlacementVariantDebugData& OutVariantDebugData)
	{
		FEnemyItemPlacementRules EffectiveRules = Ruleset.BaseRules;
		OutVariantDebugData = FEnemyPlacementVariantDebugData();

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
				OutVariantDebugData.EnabledVariantCount = EnabledVariants.Num();
				OutVariantDebugData.SelectedVariantIndex = ExistingCount % OutVariantDebugData.EnabledVariantCount;
				OutVariantDebugData.bUsedVariant = true;

				const FEnemyItemPlacementVariant& SelectedVariant =
					EnabledVariants[OutVariantDebugData.SelectedVariantIndex];
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

		return EffectiveRules;
	}

	void DebugEnemyPlacementAcceptedIfEnabled(AGeneratorWorldCampaign& Generator,
	                                          const FEnemyItemPlacementRules& EffectiveRules,
	                                          const FPlacementCandidate& SelectedCandidate,
	                                          EMapEnemyItem EnemyType,
	                                          int32 SafeZoneMaxHops,
	                                          const FEnemyPlacementVariantDebugData& VariantDebugData,
	                                          const FWorldCampaignPlacementState& WorkingPlacementState,
	                                          const FWorldCampaignDerivedData& WorkingDerivedData,
	                                          const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (not Generator.GetIsValidCampaignDebugger())
			{
				return;
			}

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

			if (Generator.bM_DisplayVariationEnemyObjectPlacement && VariantDebugData.bUsedVariant)
			{
				DebugInfo.bHasVariant = true;
				DebugInfo.VariantIndex = VariantDebugData.SelectedVariantIndex;
				DebugInfo.VariantCount = VariantDebugData.EnabledVariantCount;
			}

			if (Generator.bM_DisplayHopsFromSameEnemyItems)
			{
				DebugInfo.MinSameTypeHopDistance = GetMinHopDistanceToSameEnemyType(
					SelectedCandidate.AnchorPoint,
					EnemyType,
					WorkingPlacementState,
					AnchorLookup);
			}

			UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
			CampaignDebugger->DebugEnemyPlacementAccepted(SelectedCandidate.AnchorPoint, DebugInfo);
		}
	}

	void ApplySelectedEnemyPlacement(EMapEnemyItem EnemyType,
	                                 int32 ExistingCount,
	                                 const FPlacementCandidate& SelectedCandidate,
	                                 FWorldCampaignPlacementState& WorkingPlacementState,
	                                 FWorldCampaignDerivedData& WorkingDerivedData,
	                                 TArray<TPair<TObjectPtr<AAnchorPoint>, EMapEnemyItem>>& OutPromotions)
	{
		WorkingPlacementState.EnemyItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, EnemyType);
		WorkingDerivedData.EnemyItemPlacedCounts.Add(EnemyType, ExistingCount + 1);
		OutPromotions.Add({SelectedCandidate.AnchorPoint, EnemyType});
	}

	bool TrySelectEnemyWallPlacementCandidate(const AGeneratorWorldCampaign& Generator,
	                                          const FEnemyWallPlacementRules& PlacementRules,
	                                          const FWorldCampaignPlacementState& PlacementState,
	                                          const FWorldCampaignDerivedData& DerivedData,
	                                          int32 AttemptIndex,
	                                          FPlacementCandidate& OutCandidate)
	{
		TArray<FPlacementCandidate> Candidates;
		int32 SourceOrder = 0;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : PlacementRules.AnchorCandidates)
		{
			const int32 CandidateOrder = SourceOrder++;
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
			Candidate.CandidateOrder = CandidateOrder;
			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, IsPlacementCandidateLess);

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
			FEnemyPlacementVariantDebugData VariantDebugData;
			const FEnemyItemPlacementRules EffectiveRules = BuildEffectiveEnemyPlacementRules(
				Ruleset,
				ExistingCount,
				RelaxationState,
				VariantDebugData);

			FPlacementCandidate SelectedCandidate;
			if (not TrySelectEnemyPlacementCandidate(Generator, EffectiveRules, EnemyType, AttemptIndex, ExistingCount,
			                                         SafeZoneMaxHops, WorkingPlacementState, WorkingDerivedData,
			                                         AnchorLookup, SelectedCandidate))
			{
				return false;
			}

			DebugEnemyPlacementAcceptedIfEnabled(Generator, EffectiveRules, SelectedCandidate, EnemyType,
			                                     SafeZoneMaxHops, VariantDebugData, WorkingPlacementState,
			                                     WorkingDerivedData, AnchorLookup);
			ApplySelectedEnemyPlacement(EnemyType, ExistingCount, SelectedCandidate, WorkingPlacementState,
			                            WorkingDerivedData, OutPromotions);
		}

		return true;
	}

	void DebugNeutralPlacementRejectedIfEnabled(AGeneratorWorldCampaign& Generator,
	                                            AAnchorPoint* CandidateAnchor,
	                                            EMapNeutralObjectType NeutralType,
	                                            const TCHAR* Reason)
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (not Generator.GetIsValidCampaignDebugger())
			{
				return;
			}

			UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
			CampaignDebugger->DebugNeutralPlacementRejected(CandidateAnchor, NeutralType, Reason);
		}
	}

	bool TryBuildNeutralPlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                       EMapNeutralObjectType NeutralType,
	                                       const FNeutralItemPlacementRules& PlacementRules,
	                                       AAnchorPoint* CandidateAnchor,
	                                       int32 CandidateOrder,
	                                       const FWorldCampaignPlacementState& WorkingPlacementState,
	                                       const FWorldCampaignDerivedData& WorkingDerivedData,
	                                       const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                       FPlacementCandidate& OutCandidate)
	{
		const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
		if (IsAnchorOccupied(CandidateKey, WorkingPlacementState))
		{
			DebugNeutralPlacementRejectedIfEnabled(Generator, CandidateAnchor, NeutralType, TEXT("occupied"));
			return false;
		}

		const int32 HopDistanceFromHQ = GetCachedHopDistance(
			WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
			CandidateKey);
		if (HopDistanceFromHQ == INDEX_NONE)
		{
			DebugNeutralPlacementRejectedIfEnabled(Generator, CandidateAnchor, NeutralType, TEXT("no hop cache"));
			return false;
		}

		const int32 MaxHopsFromHQClamped = FMath::Max(PlacementRules.MinHopsFromHQ, PlacementRules.MaxHopsFromHQ);
		if (HopDistanceFromHQ < PlacementRules.MinHopsFromHQ || HopDistanceFromHQ > MaxHopsFromHQClamped)
		{
			DebugNeutralPlacementRejectedIfEnabled(Generator, CandidateAnchor, NeutralType, TEXT("hop band"));
			return false;
		}

		if (not PassesNeutralSpacingRules(PlacementRules, CandidateAnchor, WorkingPlacementState, AnchorLookup))
		{
			DebugNeutralPlacementRejectedIfEnabled(Generator, CandidateAnchor, NeutralType, TEXT("spacing"));
			return false;
		}

		OutCandidate = FPlacementCandidate();
		OutCandidate.AnchorPoint = CandidateAnchor;
		OutCandidate.AnchorKey = CandidateKey;
		OutCandidate.Score = GetTopologyPreferenceScore(PlacementRules.Preference,
		                                                static_cast<float>(HopDistanceFromHQ));
		OutCandidate.HopDistanceFromHQ = HopDistanceFromHQ;
		OutCandidate.CandidateOrder = CandidateOrder;
		return true;
	}

	FNeutralItemPlacementRules BuildEffectiveNeutralPlacementRules(const FNeutralItemPlacementRules& BaseRules,
	                                                               const FRuleRelaxationState& RelaxationState)
	{
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

		return PlacementRules;
	}

	void DebugNeutralPlacementAcceptedIfEnabled(AGeneratorWorldCampaign& Generator,
	                                            EMapNeutralObjectType NeutralType,
	                                            const FPlacementCandidate& SelectedCandidate,
	                                            const FWorldCampaignPlacementState& WorkingPlacementState,
	                                            const FWorldCampaignDerivedData& WorkingDerivedData,
	                                            const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (not Generator.GetIsValidCampaignDebugger())
			{
				return;
			}

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

			UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
			CampaignDebugger->DebugNeutralPlacementAccepted(SelectedCandidate.AnchorPoint, DebugInfo);
		}
	}

	void ApplySelectedNeutralPlacement(EMapNeutralObjectType NeutralType,
	                                   int32 ExistingCount,
	                                   const FPlacementCandidate& SelectedCandidate,
	                                   FWorldCampaignPlacementState& WorkingPlacementState,
	                                   FWorldCampaignDerivedData& WorkingDerivedData,
	                                   TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& OutPromotions)
	{
		WorkingPlacementState.NeutralItemsByAnchorKey.Add(SelectedCandidate.AnchorKey, NeutralType);
		WorkingDerivedData.NeutralItemPlacedCounts.Add(NeutralType, ExistingCount + 1);
		OutPromotions.Add({SelectedCandidate.AnchorPoint, NeutralType});
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
		int32 SourceOrder = 0;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : WorkingPlacementState.CachedAnchors)
		{
			const int32 CandidateOrder = SourceOrder++;
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			FPlacementCandidate Candidate;
			if (not TryBuildNeutralPlacementCandidate(Generator, NeutralType, PlacementRules, CandidateAnchor,
			                                          CandidateOrder, WorkingPlacementState, WorkingDerivedData,
			                                          AnchorLookup, Candidate))
			{
				continue;
			}

			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, IsPlacementCandidateLess);

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
			const FNeutralItemPlacementRules PlacementRules =
				BuildEffectiveNeutralPlacementRules(BaseRules, RelaxationState);

			FPlacementCandidate SelectedCandidate;
			if (not TrySelectNeutralPlacementCandidate(Generator, NeutralType, PlacementRules, AttemptIndex,
			                                           ExistingCount,
			                                           WorkingPlacementState, WorkingDerivedData, AnchorLookup,
			                                           SelectedCandidate))
			{
				return false;
			}

			DebugNeutralPlacementAcceptedIfEnabled(Generator, NeutralType, SelectedCandidate, WorkingPlacementState,
			                                       WorkingDerivedData, AnchorLookup);
			ApplySelectedNeutralPlacement(NeutralType, ExistingCount, SelectedCandidate, WorkingPlacementState,
			                              WorkingDerivedData, OutPromotions);
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
		const TArray<FGuid> SortedMissionAnchorKeys = BuildSortedMissionAnchorKeys(WorkingPlacementState);
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

	bool TryBuildCompanionNeutralCandidate(const AAnchorPoint* CandidateAnchor,
	                                       const FMapObjectAdjacencyRequirement& Requirement,
	                                       const FNeutralItemPlacementRules& EffectiveRules,
	                                       AAnchorPoint* NearbyAnchor,
	                                       int32 CandidateOrder,
	                                       const FWorldCampaignPlacementState& WorkingPlacementState,
	                                       const FWorldCampaignDerivedData& WorkingDerivedData,
	                                       const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                       FPlacementCandidate& OutCandidate)
	{
		const FGuid NearbyKey = NearbyAnchor->GetAnchorKey();
		if (IsAnchorOccupied(NearbyKey, WorkingPlacementState))
		{
			return false;
		}

		const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(CandidateAnchor, NearbyAnchor);
		if (HopDistance == INDEX_NONE || HopDistance > Requirement.MaxHops)
		{
			return false;
		}

		if (not PassesNeutralDistanceRules(EffectiveRules, NearbyKey, WorkingDerivedData))
		{
			return false;
		}

		if (not PassesNeutralSpacingRules(EffectiveRules, NearbyAnchor, WorkingPlacementState, AnchorLookup))
		{
			return false;
		}

		const int32 HopDistanceFromHQ = GetCachedHopDistance(
			WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
			NearbyKey);
		OutCandidate = FPlacementCandidate();
		OutCandidate.AnchorPoint = NearbyAnchor;
		OutCandidate.AnchorKey = NearbyKey;
		OutCandidate.Score = GetTopologyPreferenceScore(EffectiveRules.Preference,
		                                                static_cast<float>(HopDistanceFromHQ));
		OutCandidate.HopDistanceFromHQ = HopDistanceFromHQ;
		OutCandidate.CandidateOrder = CandidateOrder;
		return true;
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

		const FNeutralItemPlacementRules EffectiveRules =
			BuildEffectiveNeutralPlacementRules(NeutralRules, RelaxationState);

		TArray<FPlacementCandidate> Candidates;
		int32 SourceOrder = 0;
		for (const TObjectPtr<AAnchorPoint>& NearbyAnchor : WorkingPlacementState.CachedAnchors)
		{
			const int32 CandidateOrder = SourceOrder++;
			if (not IsValid(NearbyAnchor))
			{
				continue;
			}

			FPlacementCandidate Candidate;
			if (not TryBuildCompanionNeutralCandidate(CandidateAnchor, Requirement, EffectiveRules, NearbyAnchor,
			                                          CandidateOrder, WorkingPlacementState, WorkingDerivedData,
			                                          AnchorLookup, Candidate))
			{
				continue;
			}

			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, IsPlacementCandidateLess);

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

		const TArray<FGuid> SortedMissionAnchorKeys = BuildSortedMissionAnchorKeys(WorkingPlacementState);
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

	void DrawRejectedAtAnchorIfEnabled(AGeneratorWorldCampaign& Generator, AAnchorPoint* CandidateAnchor,
	                                   const TCHAR* Reason)
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (not Generator.GetIsValidCampaignDebugger())
			{
				return;
			}

			UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
			CampaignDebugger->DrawRejectedAtAnchor(CandidateAnchor, Reason);
		}
	}

	bool PassesMissionOccupancyAndNeutralRequirements(AGeneratorWorldCampaign& Generator,
	                                                  const FMissionTierRules& EffectiveRules,
	                                                  const FWorldCampaignPlacementState& WorkingPlacementState,
	                                                  AAnchorPoint* CandidateAnchor,
	                                                  const FGuid& CandidateKey)
	{
		const bool bAllowNeutralStacking = EffectiveRules.bNeutralItemRequired;
		if (IsAnchorOccupiedForMission(CandidateKey, WorkingPlacementState, bAllowNeutralStacking))
		{
			DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("occupied"));
			return false;
		}

		if (not EffectiveRules.bNeutralItemRequired)
		{
			return true;
		}

		if (HasNeutralTypeAtAnchor(WorkingPlacementState, CandidateKey, EffectiveRules.RequiredNeutralItemType))
		{
			return true;
		}

		DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("neutral requirement missing"));
		return false;
	}

	bool TryGetMissionHopDistanceForCandidate(AGeneratorWorldCampaign& Generator,
	                                          AAnchorPoint* CandidateAnchor,
	                                          const FGuid& CandidateKey,
	                                          const FWorldCampaignDerivedData& WorkingDerivedData,
	                                          bool bRequireHopDistance,
	                                          int32 MinHopsFromHQ,
	                                          int32 MaxHopsFromHQ,
	                                          int32& OutHopDistanceFromHQ)
	{
		OutHopDistanceFromHQ = INDEX_NONE;
		if (not bRequireHopDistance)
		{
			return true;
		}

		OutHopDistanceFromHQ = GetCachedHopDistance(
			WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
			CandidateKey);
		if (OutHopDistanceFromHQ == INDEX_NONE)
		{
			DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("no hop cache"));
			return false;
		}

		const int32 MaxHopsFromHQClamped = FMath::Max(MinHopsFromHQ, MaxHopsFromHQ);
		if (OutHopDistanceFromHQ >= MinHopsFromHQ && OutHopDistanceFromHQ <= MaxHopsFromHQClamped)
		{
			return true;
		}

		DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("hop range"));
		return false;
	}

	bool PassesMissionXYDistanceRules(AGeneratorWorldCampaign& Generator,
	                                  const FMissionTierRules& EffectiveRules,
	                                  const FWorldCampaignPlacementState& WorkingPlacementState,
	                                  AAnchorPoint* CandidateAnchor)
	{
		if (not EffectiveRules.bUseXYDistanceFromHQ)
		{
			return true;
		}

		const float XYDistanceFromHQ = CampaignGenerationHelper::XYDistanceFromHQ(
			CandidateAnchor,
			WorkingPlacementState.PlayerHQAnchor);
		if (XYDistanceFromHQ >= EffectiveRules.MinXYDistanceFromHQ
			&& XYDistanceFromHQ <= EffectiveRules.MaxXYDistanceFromHQ)
		{
			return true;
		}

		DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("xy range"));
		return false;
	}

	bool TryGetMissionSpacingDataWithDebug(AGeneratorWorldCampaign& Generator,
	                                       const FMissionTierRules& EffectiveRules,
	                                       AAnchorPoint* CandidateAnchor,
	                                       const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                       const FWorldCampaignPlacementState& WorkingPlacementState,
	                                       float& OutNearestMissionHopDistance,
	                                       float& OutNearestMissionXYDistance)
	{
		if (TryGetMissionSpacingData(EffectiveRules, CandidateAnchor, AnchorLookup, WorkingPlacementState,
		                             OutNearestMissionHopDistance, OutNearestMissionXYDistance))
		{
			return true;
		}

		DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("spacing violation"));
		return false;
	}

	bool TryApplyMissionAdjacencyWithDebug(AGeneratorWorldCampaign& Generator,
	                                       const FMapObjectAdjacencyRequirement& AdjacencyRequirement,
	                                       AAnchorPoint* CandidateAnchor,
	                                       const FNeutralItemPlacementRules& NeutralRules,
	                                       const FRuleRelaxationState& RelaxationState,
	                                       const FWorldCampaignPlacementState& WorkingPlacementState,
	                                       const FWorldCampaignDerivedData& WorkingDerivedData,
	                                       const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                       FPlacementCandidate& InOutCandidate)
	{
		if (TryApplyMissionAdjacencyRequirement(AdjacencyRequirement, CandidateAnchor, NeutralRules,
		                                        RelaxationState, WorkingPlacementState, WorkingDerivedData,
		                                        AnchorLookup, InOutCandidate))
		{
			return true;
		}

		DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("adjacency missing"));
		return false;
	}

	bool TryBuildMissionOverridePlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                               const FMissionTierRules& EffectiveRules,
	                                               AAnchorPoint* CandidateAnchor,
	                                               int32 CandidateOrder,
	                                               const FNeutralItemPlacementRules& NeutralRules,
	                                               const FRuleRelaxationState& RelaxationState,
	                                               const FWorldCampaignPlacementState& WorkingPlacementState,
	                                               const FWorldCampaignDerivedData& WorkingDerivedData,
	                                               const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                               int32 OverrideMinConnections,
	                                               int32 OverrideMaxConnections,
	                                               ETopologySearchStrategy OverrideConnectionPreference,
	                                               int32 OverrideMinHopsFromPlayerHQ,
	                                               int32 OverrideMaxHopsFromPlayerHQ,
	                                               ETopologySearchStrategy OverrideHopsPreference,
	                                               FPlacementCandidate& OutCandidate)
	{
		const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
		if (not PassesMissionOccupancyAndNeutralRequirements(Generator, EffectiveRules, WorkingPlacementState,
		                                                     CandidateAnchor, CandidateKey))
		{
			return false;
		}

		int32 HopDistanceFromHQ = INDEX_NONE;
		if (not TryGetMissionHopDistanceForCandidate(Generator, CandidateAnchor, CandidateKey,
		                                             WorkingDerivedData, true,
		                                             OverrideMinHopsFromPlayerHQ, OverrideMaxHopsFromPlayerHQ,
		                                             HopDistanceFromHQ))
		{
			return false;
		}

		if (not PassesMissionXYDistanceRules(Generator, EffectiveRules, WorkingPlacementState, CandidateAnchor))
		{
			return false;
		}

		const int32 ConnectionDegree = Generator.GetAnchorConnectionDegree(CandidateAnchor);
		if (ConnectionDegree < OverrideMinConnections || ConnectionDegree > OverrideMaxConnections)
		{
			DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("topology violation"));
			return false;
		}

		float NearestMissionHopDistance = 0.f;
		float NearestMissionXYDistance = 0.f;
		if (not TryGetMissionSpacingDataWithDebug(Generator, EffectiveRules, CandidateAnchor, AnchorLookup,
		                                          WorkingPlacementState, NearestMissionHopDistance,
		                                          NearestMissionXYDistance))
		{
			return false;
		}

		OutCandidate = FPlacementCandidate();
		OutCandidate.AnchorPoint = CandidateAnchor;
		OutCandidate.AnchorKey = CandidateKey;
		OutCandidate.Score = GetOverrideMissionPreferenceScore(OverrideConnectionPreference, OverrideHopsPreference,
		                                                       ConnectionDegree, HopDistanceFromHQ);
		OutCandidate.HopDistanceFromHQ = HopDistanceFromHQ;
		OutCandidate.CandidateOrder = CandidateOrder;
		return TryApplyMissionAdjacencyWithDebug(Generator, EffectiveRules.AdjacencyRequirement, CandidateAnchor,
		                                         NeutralRules, RelaxationState, WorkingPlacementState,
		                                         WorkingDerivedData, AnchorLookup, OutCandidate);
	}

	bool TryBuildMissionPlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                       const FMissionTierRules& EffectiveRules,
	                                       AAnchorPoint* CandidateAnchor,
	                                       int32 CandidateOrder,
	                                       const FNeutralItemPlacementRules& NeutralRules,
	                                       const FRuleRelaxationState& RelaxationState,
	                                       const FWorldCampaignPlacementState& WorkingPlacementState,
	                                       const FWorldCampaignDerivedData& WorkingDerivedData,
	                                       const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                       FPlacementCandidate& OutCandidate)
	{
		const FGuid CandidateKey = CandidateAnchor->GetAnchorKey();
		if (not PassesMissionOccupancyAndNeutralRequirements(Generator, EffectiveRules, WorkingPlacementState,
		                                                     CandidateAnchor, CandidateKey))
		{
			return false;
		}

		int32 HopDistanceFromHQ = INDEX_NONE;
		if (not TryGetMissionHopDistanceForCandidate(Generator, CandidateAnchor, CandidateKey, WorkingDerivedData,
		                                             EffectiveRules.bUseHopsDistanceFromHQ,
		                                             EffectiveRules.MinHopsFromHQ, EffectiveRules.MaxHopsFromHQ,
		                                             HopDistanceFromHQ))
		{
			return false;
		}

		if (not PassesMissionXYDistanceRules(Generator, EffectiveRules, WorkingPlacementState, CandidateAnchor))
		{
			return false;
		}

		const int32 ConnectionDegree = Generator.GetAnchorConnectionDegree(CandidateAnchor);
		if (not PassesMissionTopologyRules(EffectiveRules, ConnectionDegree))
		{
			DrawRejectedAtAnchorIfEnabled(Generator, CandidateAnchor, TEXT("topology violation"));
			return false;
		}

		float NearestMissionHopDistance = 0.f;
		float NearestMissionXYDistance = 0.f;
		if (not TryGetMissionSpacingDataWithDebug(Generator, EffectiveRules, CandidateAnchor, AnchorLookup,
		                                          WorkingPlacementState, NearestMissionHopDistance,
		                                          NearestMissionXYDistance))
		{
			return false;
		}

		OutCandidate = FPlacementCandidate();
		OutCandidate.AnchorPoint = CandidateAnchor;
		OutCandidate.AnchorKey = CandidateKey;
		OutCandidate.Score = GetMissionPreferenceScore(EffectiveRules, CandidateAnchor, WorkingPlacementState,
		                                               WorkingDerivedData, NearestMissionHopDistance,
		                                               NearestMissionXYDistance, ConnectionDegree);
		OutCandidate.HopDistanceFromHQ = HopDistanceFromHQ;
		OutCandidate.CandidateOrder = CandidateOrder;
		return TryApplyMissionAdjacencyWithDebug(Generator, EffectiveRules.AdjacencyRequirement, CandidateAnchor,
		                                         NeutralRules, RelaxationState, WorkingPlacementState,
		                                         WorkingDerivedData, AnchorLookup, OutCandidate);
	}

	struct FMissionOverrideSelectionData
	{
		bool bOverridePlacementWithArray = false;
		const TArray<TObjectPtr<AAnchorPoint>>* CandidateSourceOverride = nullptr;
		int32 MinConnections = 0;
		int32 MaxConnections = 0;
		ETopologySearchStrategy ConnectionPreference = ETopologySearchStrategy::NotSet;
		int32 MinHopsFromPlayerHQ = 0;
		int32 MaxHopsFromPlayerHQ = 0;
		ETopologySearchStrategy HopsPreference = ETopologySearchStrategy::NotSet;
	};

	FMissionOverrideSelectionData BuildMissionOverrideSelectionData(const FPerMissionRules& MissionRules)
	{
		FMissionOverrideSelectionData SelectionData;
		SelectionData.bOverridePlacementWithArray = MissionRules.bOverridePlacementWithArray;
		SelectionData.CandidateSourceOverride = SelectionData.bOverridePlacementWithArray
			                                        ? &MissionRules.OverridePlacementAnchorCandidates
			                                        : nullptr;
		SelectionData.MinConnections = FMath::Min(MissionRules.OverrideMinConnections,
		                                          MissionRules.OverrideMaxConnections);
		SelectionData.MaxConnections = FMath::Max(MissionRules.OverrideMinConnections,
		                                          MissionRules.OverrideMaxConnections);
		SelectionData.ConnectionPreference = MissionRules.OverrideConnectionPreference;
		SelectionData.MinHopsFromPlayerHQ = FMath::Min(MissionRules.OverrideMinHopsFromPlayerHQ,
		                                               MissionRules.OverrideMaxHopsFromPlayerHQ);
		SelectionData.MaxHopsFromPlayerHQ = FMath::Max(MissionRules.OverrideMinHopsFromPlayerHQ,
		                                               MissionRules.OverrideMaxHopsFromPlayerHQ);
		SelectionData.HopsPreference = MissionRules.OverrideHopsPreference;
		return SelectionData;
	}

	bool TrySelectMissionPlacementCandidateOverride(AGeneratorWorldCampaign& Generator,
	                                                const FMissionPlacement& MissionPlacementRules,
	                                                const FMissionTierRules& EffectiveRules,
	                                                EMapMission MissionType,
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
		int32 SourceOrder = 0;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : CandidateSource)
		{
			const int32 CandidateOrder = SourceOrder++;
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			if (not Generator.IsAnchorCached(CandidateAnchor))
			{
				continue;
			}

			FPlacementCandidate Candidate;
			if (not TryBuildMissionOverridePlacementCandidate(
				Generator, EffectiveRules, CandidateAnchor, CandidateOrder, NeutralRules, RelaxationState,
				WorkingPlacementState, WorkingDerivedData, AnchorLookup, OverrideMinConnections,
				OverrideMaxConnections, OverrideConnectionPreference, OverrideMinHopsFromPlayerHQ,
				OverrideMaxHopsFromPlayerHQ, OverrideHopsPreference, Candidate))
			{
				continue;
			}

			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, IsPlacementCandidateLess);

		const int32 CandidateIndex = SelectMissionCandidateIndexDeterministic(
			Candidates,
			EffectiveRules,
			MissionPlacementRules,
			OverrideHopsPreference,
			MissionType,
			AttemptIndex,
			MissionIndex,
			WorkingPlacementState.SeedUsed);
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	bool TrySelectMissionPlacementCandidateFromSource(AGeneratorWorldCampaign& Generator,
	                                                  const FMissionPlacement& MissionPlacementRules,
	                                                  const FMissionTierRules& EffectiveRules,
	                                                  EMapMission MissionType,
	                                                  int32 AttemptIndex,
	                                                  int32 MissionIndex,
	                                                  const FRuleRelaxationState& RelaxationState,
	                                                  const FNeutralItemPlacementRules& NeutralRules,
	                                                  const FWorldCampaignPlacementState& WorkingPlacementState,
	                                                  const FWorldCampaignDerivedData& WorkingDerivedData,
	                                                  const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                                  const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
	                                                  FPlacementCandidate& OutCandidate)
	{
		TArray<FPlacementCandidate> Candidates;
		int32 SourceOrder = 0;
		for (const TObjectPtr<AAnchorPoint>& CandidateAnchor : CandidateSource)
		{
			const int32 CandidateOrder = SourceOrder++;
			if (not IsValid(CandidateAnchor))
			{
				continue;
			}

			FPlacementCandidate Candidate;
			if (not TryBuildMissionPlacementCandidate(Generator, EffectiveRules, CandidateAnchor, CandidateOrder,
			                                          NeutralRules, RelaxationState, WorkingPlacementState,
			                                          WorkingDerivedData, AnchorLookup, Candidate))
			{
				continue;
			}

			Candidates.Add(Candidate);
		}

		if (Candidates.Num() == 0)
		{
			return false;
		}

		Algo::Sort(Candidates, IsPlacementCandidateLess);
		const int32 CandidateIndex = SelectMissionCandidateIndexDeterministic(
			Candidates, EffectiveRules, MissionPlacementRules, EffectiveRules.HopsDistancePreference, MissionType,
			AttemptIndex, MissionIndex, WorkingPlacementState.SeedUsed);
		OutCandidate = Candidates[CandidateIndex];
		return IsValid(OutCandidate.AnchorPoint);
	}

	bool TrySelectMissionPlacementCandidate(AGeneratorWorldCampaign& Generator,
	                                        const FMissionPlacement& MissionPlacementRules,
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

			return TrySelectMissionPlacementCandidateOverride(Generator, MissionPlacementRules, EffectiveRules,
			                                                  MissionType, AttemptIndex,
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
		return TrySelectMissionPlacementCandidateFromSource(
			Generator, MissionPlacementRules, EffectiveRules, MissionType, AttemptIndex, MissionIndex,
			RelaxationState, NeutralRules, WorkingPlacementState, WorkingDerivedData, AnchorLookup,
			CandidateSource, OutCandidate);
	}

	bool TryResolveMissionEffectiveRules(const FMissionPlacement& MissionPlacementRules,
	                                     const FPerMissionRules& MissionRules,
	                                     const FRuleRelaxationState& RelaxationState,
	                                     FMissionTierRules& OutEffectiveRules)
	{
		if (MissionRules.bOverrideTierRules)
		{
			OutEffectiveRules = MissionRules.OverrideRules;
		}
		else
		{
			const FMissionTierRules* TierRules = MissionPlacementRules.RulesByTier.Find(MissionRules.Tier);
			if (not TierRules)
			{
				return false;
			}

			OutEffectiveRules = *TierRules;
		}

		if (RelaxationState.bRelaxDistance)
		{
			if (OutEffectiveRules.bUseHopsDistanceFromHQ)
			{
				OutEffectiveRules.MinHopsFromHQ = 0;
				OutEffectiveRules.MaxHopsFromHQ = RelaxedHopDistanceMax;
				OutEffectiveRules.HopsDistancePreference = ETopologySearchStrategy::NotSet;
			}

			if (OutEffectiveRules.bUseXYDistanceFromHQ)
			{
				OutEffectiveRules.MinXYDistanceFromHQ = 0.f;
				OutEffectiveRules.MaxXYDistanceFromHQ = RelaxedDistanceMax;
				OutEffectiveRules.XYDistancePreference = ETopologySearchStrategy::NotSet;
			}
		}

		if (RelaxationState.bRelaxSpacing)
		{
			if (OutEffectiveRules.bUseMissionSpacingHops)
			{
				OutEffectiveRules.MinMissionSpacingHops = 0;
				OutEffectiveRules.MaxMissionSpacingHops = RelaxedHopDistanceMax;
				OutEffectiveRules.MissionSpacingHopsPreference = ETopologySearchStrategy::NotSet;
			}

			if (OutEffectiveRules.bUseMissionSpacingXY)
			{
				OutEffectiveRules.MinMissionSpacingXY = 0.f;
				OutEffectiveRules.MaxMissionSpacingXY = RelaxedDistanceMax;
				OutEffectiveRules.MissionSpacingXYPreference = ETopologySearchStrategy::NotSet;
			}
		}

		if (RelaxationState.bRelaxPreference)
		{
			OutEffectiveRules.ConnectionPreference = ETopologySearchStrategy::NotSet;
		}

		return true;
	}

	bool TryValidateMissionOverrideCandidateSource(EMapMission MissionType, const FPerMissionRules& MissionRules)
	{
		if (not MissionRules.bOverridePlacementWithArray
			|| MissionRules.OverridePlacementAnchorCandidates.Num() > 0)
		{
			return true;
		}

		const UEnum* MissionEnum = StaticEnum<EMapMission>();
		const FString MissionName = MissionEnum
			                            ? MissionEnum->GetNameStringByValue(static_cast<int64>(MissionType))
			                            : TEXT("Mission");
		const FString ErrorMessage = FString::Printf(
			TEXT("Mission placement failed: override anchor list empty for %s."),
			*MissionName);
		RTSFunctionLibrary::ReportError(ErrorMessage);
		return false;
	}

	void DebugMissionPlacementFailedIfEnabled(AGeneratorWorldCampaign& Generator,
	                                          const FWorldCampaignPlacementState& WorkingPlacementState,
	                                          EMapMission MissionType,
	                                          int32 AttemptIndex,
	                                          const FRuleRelaxationState& RelaxationState)
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (not Generator.bM_DebugFailedMissionPlacement || not Generator.GetIsValidCampaignDebugger())
			{
				return;
			}

			AAnchorPoint* DebugAnchor = WorkingPlacementState.PlayerHQAnchor.Get();
			if (not IsValid(DebugAnchor))
			{
				return;
			}

			const FString Reason = FString::Printf(
				TEXT("no candidates (attempt %d, relax D:%d S:%d P:%d)"),
				AttemptIndex,
				RelaxationState.bRelaxDistance ? 1 : 0,
				RelaxationState.bRelaxSpacing ? 1 : 0,
				RelaxationState.bRelaxPreference ? 1 : 0);
			UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
			CampaignDebugger->DebugMissionPlacementFailed(DebugAnchor, MissionType, Reason);
		}
	}

	void AddMissionSpacingDebugInfo(const FMissionTierRules& EffectiveRules,
	                                const FPlacementCandidate& SelectedCandidate,
	                                const FWorldCampaignPlacementState& WorkingPlacementState,
	                                const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                FWorldCampaignMissionPlacementDebugInfo& InOutDebugInfo)
	{
		if (not EffectiveRules.bUseMissionSpacingHops || WorkingPlacementState.MissionsByAnchorKey.Num() == 0)
		{
			return;
		}

		float NearestMissionHopDistance = -1.f;
		float NearestMissionXYDistance = 0.f;
		if (TryGetMissionSpacingData(EffectiveRules, SelectedCandidate.AnchorPoint, AnchorLookup,
		                             WorkingPlacementState, NearestMissionHopDistance, NearestMissionXYDistance))
		{
			InOutDebugInfo.NearestMissionHopDistance = NearestMissionHopDistance;
		}

		InOutDebugInfo.bUsesMissionSpacingHops = true;
	}

	void AddMissionOverrideDebugInfo(bool bOverridePlacementWithArray,
	                                 int32 EffectiveOverrideMinConnections,
	                                 int32 EffectiveOverrideMaxConnections,
	                                 int32 EffectiveOverrideMinHopsFromPlayerHQ,
	                                 int32 EffectiveOverrideMaxHopsFromPlayerHQ,
	                                 ETopologySearchStrategy EffectiveOverrideConnectionPreference,
	                                 ETopologySearchStrategy EffectiveOverrideHopsPreference,
	                                 FWorldCampaignMissionPlacementDebugInfo& InOutDebugInfo)
	{
		if (not bOverridePlacementWithArray)
		{
			return;
		}

		InOutDebugInfo.bUsesOverrideArray = true;
		InOutDebugInfo.bOverrideArrayUsesConnectionBounds = true;
		InOutDebugInfo.OverrideMinConnections = EffectiveOverrideMinConnections;
		InOutDebugInfo.OverrideMaxConnections = EffectiveOverrideMaxConnections;
		InOutDebugInfo.bOverrideArrayUsesHopsBounds = true;
		InOutDebugInfo.OverrideMinHopsFromHQ = EffectiveOverrideMinHopsFromPlayerHQ;
		InOutDebugInfo.OverrideMaxHopsFromHQ = EffectiveOverrideMaxHopsFromPlayerHQ;
		InOutDebugInfo.OverrideConnectionPreference = EffectiveOverrideConnectionPreference;
		InOutDebugInfo.OverrideHopsPreference = EffectiveOverrideHopsPreference;
	}

	void DebugMissionPlacementAcceptedIfEnabled(AGeneratorWorldCampaign& Generator,
	                                            const FMissionTierRules& EffectiveRules,
	                                            const FPlacementCandidate& SelectedCandidate,
	                                            const FWorldCampaignPlacementState& WorkingPlacementState,
	                                            const FWorldCampaignDerivedData& WorkingDerivedData,
	                                            const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                            EMapMission MissionType,
	                                            bool bOverridePlacementWithArray,
	                                            int32 EffectiveOverrideMinConnections,
	                                            int32 EffectiveOverrideMaxConnections,
	                                            int32 EffectiveOverrideMinHopsFromPlayerHQ,
	                                            int32 EffectiveOverrideMaxHopsFromPlayerHQ,
	                                            ETopologySearchStrategy EffectiveOverrideConnectionPreference,
	                                            ETopologySearchStrategy EffectiveOverrideHopsPreference)
	{
		if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
		{
			if (not Generator.GetIsValidCampaignDebugger())
			{
				return;
			}

			FWorldCampaignMissionPlacementDebugInfo DebugInfo;
			DebugInfo.MissionType = MissionType;
			if (Generator.bM_DisplayHopsFromHQForMissions)
			{
				DebugInfo.HopFromHQ = GetCachedHopDistance(
					WorkingDerivedData.PlayerHQHopDistancesByAnchorKey,
					SelectedCandidate.AnchorKey);
			}

			if (Generator.bM_DebugMissionSpacingHops)
			{
				AddMissionSpacingDebugInfo(EffectiveRules, SelectedCandidate, WorkingPlacementState, AnchorLookup,
				                           DebugInfo);
			}

			if (Generator.bM_DisplayMinMaxConnectionsForMissionPlacement)
			{
				DebugInfo.bUsesConnectionRules = EffectiveRules.MinConnections > 0 || EffectiveRules.MaxConnections > 0;
				DebugInfo.MinConnections = EffectiveRules.MinConnections;
				DebugInfo.MaxConnections = EffectiveRules.MaxConnections;
			}

			if (Generator.bM_DisplayMissionAdjacencyRequirements && EffectiveRules.AdjacencyRequirement.bEnabled)
			{
				DebugInfo.bHasAdjacencyRequirement = true;
				DebugInfo.AdjacencySummary = BuildAdjacencyRequirementSummary(EffectiveRules.AdjacencyRequirement);
			}

			if (Generator.bM_DisplayNeutralItemRequirementForMission && EffectiveRules.bNeutralItemRequired)
			{
				DebugInfo.bHasNeutralRequirement = true;
				DebugInfo.RequiredNeutralType = EffectiveRules.RequiredNeutralItemType;
			}

			AddMissionOverrideDebugInfo(bOverridePlacementWithArray, EffectiveOverrideMinConnections,
			                            EffectiveOverrideMaxConnections, EffectiveOverrideMinHopsFromPlayerHQ,
			                            EffectiveOverrideMaxHopsFromPlayerHQ, EffectiveOverrideConnectionPreference,
			                            EffectiveOverrideHopsPreference, DebugInfo);

			UWorldCampaignDebugger* CampaignDebugger = Generator.GetCampaignDebugger();
			CampaignDebugger->DebugMissionPlacementAccepted(SelectedCandidate.AnchorPoint, DebugInfo);
		}
	}

	bool ApplySelectedMissionPlacement(EMapMission MissionType,
	                                   const FPlacementCandidate& SelectedCandidate,
	                                   FWorldCampaignPlacementState& WorkingPlacementState,
	                                   FWorldCampaignDerivedData& WorkingDerivedData,
	                                   TArray<TPair<TObjectPtr<AAnchorPoint>, EMapMission>>& Promotions,
	                                   TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>&
	                                   CompanionPromotions)
	{
		WorkingPlacementState.MissionsByAnchorKey.Add(SelectedCandidate.AnchorKey, MissionType);
		const int32 ExistingMissionCount = WorkingDerivedData.MissionPlacedCounts.FindRef(MissionType);
		WorkingDerivedData.MissionPlacedCounts.Add(MissionType, ExistingMissionCount + 1);
		Promotions.Add({SelectedCandidate.AnchorPoint, MissionType});

		if (SelectedCandidate.CompanionItemType != EMapItemType::NeutralItem)
		{
			return true;
		}

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
		return true;
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
		if (not TryResolveMissionEffectiveRules(MissionPlacementRules, *MissionRulesPtr, RelaxationState,
		                                        EffectiveRules))
		{
			return false;
		}

		if (not TryValidateMissionOverrideCandidateSource(MissionType, *MissionRulesPtr))
		{
			return false;
		}

		const FMissionOverrideSelectionData OverrideSelectionData =
			BuildMissionOverrideSelectionData(*MissionRulesPtr);

		FPlacementCandidate SelectedCandidate;
		if (not TrySelectMissionPlacementCandidate(Generator, MissionPlacementRules, EffectiveRules, MissionType,
		                                           AttemptIndex, MissionIndex, RelaxationState, NeutralRules,
		                                           WorkingPlacementState, WorkingDerivedData, AnchorLookup,
		                                           OverrideSelectionData.CandidateSourceOverride,
		                                           OverrideSelectionData.bOverridePlacementWithArray,
		                                           OverrideSelectionData.MinConnections,
		                                           OverrideSelectionData.MaxConnections,
		                                           OverrideSelectionData.ConnectionPreference,
		                                           OverrideSelectionData.MinHopsFromPlayerHQ,
		                                           OverrideSelectionData.MaxHopsFromPlayerHQ,
		                                           OverrideSelectionData.HopsPreference, SelectedCandidate))
		{
			DebugMissionPlacementFailedIfEnabled(Generator, WorkingPlacementState, MissionType, AttemptIndex,
			                                     RelaxationState);
			return false;
		}

		DebugMissionPlacementAcceptedIfEnabled(Generator, EffectiveRules, SelectedCandidate, WorkingPlacementState,
		                                       WorkingDerivedData, AnchorLookup, MissionType,
		                                       OverrideSelectionData.bOverridePlacementWithArray,
		                                       OverrideSelectionData.MinConnections,
		                                       OverrideSelectionData.MaxConnections,
		                                       OverrideSelectionData.MinHopsFromPlayerHQ,
		                                       OverrideSelectionData.MaxHopsFromPlayerHQ,
		                                       OverrideSelectionData.ConnectionPreference,
		                                       OverrideSelectionData.HopsPreference);
		return ApplySelectedMissionPlacement(MissionType, SelectedCandidate, WorkingPlacementState, WorkingDerivedData,
		                                     Promotions, CompanionPromotions);
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
			for (int32 MissingIndex = 0; MissingIndex < Missing; ++MissingIndex)
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
			for (int32 MissingIndex = 0; MissingIndex < Missing; ++MissingIndex)
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
				--(*RemainingCount);
				continue;
			}

			OutMissingMissions.Add(MissionType);
		}
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
		for (const EMapEnemyItem EnemyType : MissingEnemyItems)
		{
			FFailSafeItem Item;
			Item.Kind = EFailSafeItemKind::Enemy;
			Item.RawEnumValue = static_cast<uint8>(EnemyType);
			Item.MinDistance = GetEnemyMinDistance(EnemyType);
			Item.EnemyType = EnemyType;
			OutItems.Add(Item);
		}

		for (const EMapNeutralObjectType NeutralType : MissingNeutralItems)
		{
			FFailSafeItem Item;
			Item.Kind = EFailSafeItemKind::Neutral;
			Item.RawEnumValue = static_cast<uint8>(NeutralType);
			Item.MinDistance = GetNeutralMinDistance(NeutralType);
			Item.NeutralType = NeutralType;
			OutItems.Add(Item);
		}

		for (const EMapMission MissionType : MissingMissions)
		{
			FFailSafeItem Item;
			Item.Kind = EFailSafeItemKind::Mission;
			Item.RawEnumValue = static_cast<uint8>(MissionType);
			Item.MinDistance = GetMissionMinDistance(MissionType);
			Item.MissionType = MissionType;
			OutItems.Add(Item);
		}

		constexpr float DistanceTolerance = 0.001f;
		OutItems.Sort([](const FFailSafeItem& Left, const FFailSafeItem& Right)
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

	bool TryPlaceFailSafeEnemyItem(AAnchorPoint* AnchorPoint,
	                               const FGuid& AnchorKey,
	                               EMapEnemyItem EnemyType,
	                               ECampaignGenerationStep FailedStep,
	                               FWorldCampaignPlacementState& InOutPlacementState,
	                               FWorldCampaignDerivedData& InOutDerivedData,
	                               FCampaignGenerationStepTransaction& InOutTransaction,
	                               FFailSafePlacementTotals& InOutTotals,
	                               bool bDeferWorldObjectPromotion)
	{
		InOutPlacementState.EnemyItemsByAnchorKey.Add(AnchorKey, EnemyType);
		int32& PlacedCount = InOutDerivedData.EnemyItemPlacedCounts.FindOrAdd(EnemyType);
		PlacedCount++;
		if (bDeferWorldObjectPromotion)
		{
			InOutTotals.EnemyPlaced++;
			return true;
		}

		AWorldMapObject* SpawnedObject = AnchorPoint->OnEnemyItemPromotion(EnemyType, FailedStep);
		if (IsValid(SpawnedObject))
		{
			InOutTotals.EnemyPlaced++;
			InOutTransaction.SpawnedActors.Add(SpawnedObject);
			return true;
		}

		InOutPlacementState.EnemyItemsByAnchorKey.Remove(AnchorKey);
		PlacedCount = FMath::Max(0, PlacedCount - 1);
		if (PlacedCount == 0)
		{
			InOutDerivedData.EnemyItemPlacedCounts.Remove(EnemyType);
		}

		RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: failed to spawn enemy item."));
		return false;
	}

	bool TryPlaceFailSafeNeutralItem(AAnchorPoint* AnchorPoint,
	                                 const FGuid& AnchorKey,
	                                 EMapNeutralObjectType NeutralType,
	                                 ECampaignGenerationStep FailedStep,
	                                 FWorldCampaignPlacementState& InOutPlacementState,
	                                 FWorldCampaignDerivedData& InOutDerivedData,
	                                 FCampaignGenerationStepTransaction& InOutTransaction,
	                                 FFailSafePlacementTotals& InOutTotals,
	                                 bool bDeferWorldObjectPromotion)
	{
		InOutPlacementState.NeutralItemsByAnchorKey.Add(AnchorKey, NeutralType);
		int32& PlacedCount = InOutDerivedData.NeutralItemPlacedCounts.FindOrAdd(NeutralType);
		PlacedCount++;
		if (bDeferWorldObjectPromotion)
		{
			InOutTotals.NeutralPlaced++;
			return true;
		}

		AWorldMapObject* SpawnedObject = AnchorPoint->OnNeutralItemPromotion(NeutralType, FailedStep);
		if (IsValid(SpawnedObject))
		{
			InOutTotals.NeutralPlaced++;
			InOutTransaction.SpawnedActors.Add(SpawnedObject);
			return true;
		}

		InOutPlacementState.NeutralItemsByAnchorKey.Remove(AnchorKey);
		PlacedCount = FMath::Max(0, PlacedCount - 1);
		if (PlacedCount == 0)
		{
			InOutDerivedData.NeutralItemPlacedCounts.Remove(NeutralType);
		}

		RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: failed to spawn neutral item."));
		return false;
	}

	bool TryPlaceFailSafeMission(AAnchorPoint* AnchorPoint,
	                             const FGuid& AnchorKey,
	                             EMapMission MissionType,
	                             ECampaignGenerationStep FailedStep,
	                             FWorldCampaignPlacementState& InOutPlacementState,
	                             FWorldCampaignDerivedData& InOutDerivedData,
	                             FCampaignGenerationStepTransaction& InOutTransaction,
	                             FFailSafePlacementTotals& InOutTotals,
	                             bool bDeferWorldObjectPromotion)
	{
		InOutPlacementState.MissionsByAnchorKey.Add(AnchorKey, MissionType);
		int32& PlacedCount = InOutDerivedData.MissionPlacedCounts.FindOrAdd(MissionType);
		PlacedCount++;
		if (bDeferWorldObjectPromotion)
		{
			InOutTotals.MissionPlaced++;
			return true;
		}

		AWorldMapObject* SpawnedObject = AnchorPoint->OnMissionPromotion(MissionType, FailedStep);
		if (IsValid(SpawnedObject))
		{
			InOutTotals.MissionPlaced++;
			InOutTransaction.SpawnedActors.Add(SpawnedObject);
			return true;
		}

		InOutPlacementState.MissionsByAnchorKey.Remove(AnchorKey);
		PlacedCount = FMath::Max(0, PlacedCount - 1);
		if (PlacedCount == 0)
		{
			InOutDerivedData.MissionPlacedCounts.Remove(MissionType);
		}

		RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: failed to spawn mission."));
		return false;
	}

	bool TryPlaceFailSafeItem(const FFailSafeItem& Item, const FEmptyAnchorDistance& AnchorEntry,
	                          const ECampaignGenerationStep FailedStep,
	                          FWorldCampaignPlacementState& InOutPlacementState,
	                          FWorldCampaignDerivedData& InOutDerivedData,
	                          FCampaignGenerationStepTransaction& InOutTransaction,
	                          FFailSafePlacementTotals& InOutTotals,
	                          const bool bDeferWorldObjectPromotion)
	{
		AAnchorPoint* AnchorPoint = AnchorEntry.AnchorPoint.Get();
		if (not IsValid(AnchorPoint))
		{
			RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: anchor pointer is invalid."));
			return false;
		}

		const FGuid AnchorKey = AnchorEntry.AnchorKey;
		if (Item.Kind == EFailSafeItemKind::Enemy)
		{
			return TryPlaceFailSafeEnemyItem(AnchorPoint, AnchorKey, Item.EnemyType, FailedStep, InOutPlacementState,
			                                 InOutDerivedData, InOutTransaction, InOutTotals,
			                                 bDeferWorldObjectPromotion);
		}

		if (Item.Kind == EFailSafeItemKind::Neutral)
		{
			return TryPlaceFailSafeNeutralItem(AnchorPoint, AnchorKey, Item.NeutralType, FailedStep,
			                                   InOutPlacementState, InOutDerivedData, InOutTransaction, InOutTotals,
			                                   bDeferWorldObjectPromotion);
		}

		return TryPlaceFailSafeMission(AnchorPoint, AnchorKey, Item.MissionType, FailedStep, InOutPlacementState,
		                               InOutDerivedData, InOutTransaction, InOutTotals, bDeferWorldObjectPromotion);
	}

	bool GetPassesSameKindSpacing(const FFailSafeItem& Item, const FEmptyAnchorDistance& Candidate,
	                              const float MinimumSpacingSquared,
	                              const TMap<EFailSafeItemKind, TArray<FVector2D>>& PlacedByKind)
	{
		if (not IsValid(Candidate.AnchorPoint))
		{
			return false;
		}

		if (MinimumSpacingSquared <= 0.0f)
		{
			return true;
		}

		const TArray<FVector2D>* ExistingPlacements = PlacedByKind.Find(Item.Kind);
		if (not ExistingPlacements)
		{
			return true;
		}

		const FVector2D CandidateLocationXY(Candidate.AnchorPoint->GetActorLocation().X,
		                                    Candidate.AnchorPoint->GetActorLocation().Y);
		for (const FVector2D& ExistingLocationXY : *ExistingPlacements)
		{
			if (FVector2D::DistSquared(CandidateLocationXY, ExistingLocationXY) < MinimumSpacingSquared)
			{
				return false;
			}
		}

		return true;
	}

	void AddPlacedAnchorByKind(const FFailSafeItem& Item, const FEmptyAnchorDistance& AnchorEntry,
	                           TMap<EFailSafeItemKind, TArray<FVector2D>>& InOutPlacedByKind)
	{
		if (not IsValid(AnchorEntry.AnchorPoint))
		{
			return;
		}

		const FVector2D PlacedLocationXY(AnchorEntry.AnchorPoint->GetActorLocation().X,
		                                 AnchorEntry.AnchorPoint->GetActorLocation().Y);
		InOutPlacedByKind.FindOrAdd(Item.Kind).Add(PlacedLocationXY);
	}

	void AssignFailSafeItemsByDistance(const TArray<FFailSafeItem>& ItemsToPlace, const bool bUseDistanceFilter,
	                                   const ECampaignGenerationStep FailedStep,
	                                   TArray<FEmptyAnchorDistance>& InOutEmptyAnchors,
	                                   FWorldCampaignPlacementState& InOutPlacementState,
	                                   FWorldCampaignDerivedData& InOutDerivedData,
	                                   FCampaignGenerationStepTransaction& InOutTransaction,
	                                   FFailSafePlacementTotals& InOutTotals,
	                                   TMap<EFailSafeItemKind, TArray<FVector2D>>& InOutPlacedByKind,
	                                   const float MinSameKindXYSpacing,
	                                   const bool bDeferWorldObjectPromotion,
	                                   TArray<FFailSafeItem>& OutRemainingItems)
	{
		OutRemainingItems.Reset();
		const float MinSameKindSpacingSquared = MinSameKindXYSpacing * MinSameKindXYSpacing;
		for (const FFailSafeItem& Item : ItemsToPlace)
		{
			bool bPlaced = false;
			const float MinDistanceSquared = bUseDistanceFilter ? Item.MinDistance * Item.MinDistance : 0.0f;
			for (int32 AnchorIndex = 0; AnchorIndex < InOutEmptyAnchors.Num(); ++AnchorIndex)
			{
				const FEmptyAnchorDistance& Candidate = InOutEmptyAnchors[AnchorIndex];
				if (bUseDistanceFilter && Candidate.DistanceSquared < MinDistanceSquared)
				{
					continue;
				}

				if (not GetPassesSameKindSpacing(Item, Candidate, MinSameKindSpacingSquared, InOutPlacedByKind))
				{
					continue;
				}

				if (not TryPlaceFailSafeItem(Item, Candidate, FailedStep, InOutPlacementState, InOutDerivedData,
				                             InOutTransaction, InOutTotals, bDeferWorldObjectPromotion))
				{
					continue;
				}

				AddPlacedAnchorByKind(Item, Candidate, InOutPlacedByKind);
				InOutEmptyAnchors.RemoveAt(AnchorIndex);
				bPlaced = true;
				break;
			}

			if (not bPlaced)
			{
				OutRemainingItems.Add(Item);
			}
		}
	}

	void AssignFailSafeItemsFallback(const TArray<FFailSafeItem>& RemainingItems,
	                                 const ECampaignGenerationStep FailedStep,
	                                 const FWorldCampaignPlacementFailurePolicy& FailurePolicy, const int32 SeedUsed,
	                                 const TArray<FEmptyAnchorDistance>& EmptyAnchors,
	                                 FWorldCampaignPlacementState& InOutPlacementState,
	                                 FWorldCampaignDerivedData& InOutDerivedData,
	                                 FCampaignGenerationStepTransaction& InOutTransaction,
	                                 FFailSafePlacementTotals& InOutTotals,
	                                 const bool bDeferWorldObjectPromotion)
	{
		if (RemainingItems.Num() == 0)
		{
			return;
		}

		RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: not enough anchors satisfying min-distance rules; "
			"falling back to random assignment."));
		if (EmptyAnchors.Num() == 0)
		{
			InOutTotals.Discarded += RemainingItems.Num();
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Timeout fail-safe: not enough empty anchors; %d items could not be placed and were discarded."),
				RemainingItems.Num()));
			return;
		}

		TArray<FEmptyAnchorDistance> ShuffledAnchors = EmptyAnchors;
		const int32 SeedOffset = FailurePolicy.TimeoutFailSafeSeedOffset;
		FRandomStream RandomStream(SeedUsed + SeedOffset + static_cast<int32>(FailedStep));
		CampaignGenerationHelper::DeterministicShuffle(ShuffledAnchors, RandomStream);

		int32 AnchorIndex = 0;
		for (const FFailSafeItem& Item : RemainingItems)
		{
			bool bPlaced = false;
			while (AnchorIndex < ShuffledAnchors.Num())
			{
				const FEmptyAnchorDistance& Candidate = ShuffledAnchors[AnchorIndex];
				if (TryPlaceFailSafeItem(Item, Candidate, FailedStep, InOutPlacementState, InOutDerivedData,
				                         InOutTransaction, InOutTotals, bDeferWorldObjectPromotion))
				{
					AnchorIndex++;
					bPlaced = true;
					break;
				}

				AnchorIndex++;
			}

			if (not bPlaced)
			{
				InOutTotals.Discarded++;
			}
		}

		if (InOutTotals.Discarded > 0)
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("Timeout fail-safe: not enough empty anchors; %d items could not be placed and were discarded."),
				InOutTotals.Discarded));
		}
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

	bool TryGetAnchorLocationXY(const FGuid& AnchorKey,
	                            const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                            FVector2D& OutLocationXY)
	{
		AAnchorPoint* AnchorPoint = FindAnchorByKey(AnchorLookup, AnchorKey);
		if (not IsValid(AnchorPoint))
		{
			return false;
		}

		OutLocationXY = FVector2D(AnchorPoint->GetActorLocation());
		return true;
	}

	void BuildSortedSwappableEnemyAnchors(const FWorldCampaignPlacementState& PlacementState,
	                                      TArray<TPair<FGuid, EMapEnemyItem>>& OutSwappableAnchors)
	{
		OutSwappableAnchors.Reset();
		OutSwappableAnchors.Reserve(PlacementState.EnemyItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : PlacementState.EnemyItemsByAnchorKey)
		{
			if (not GetIsSwappableEnemyType(EnemyPair.Value))
			{
				continue;
			}

			OutSwappableAnchors.Add(EnemyPair);
		}

		OutSwappableAnchors.Sort([](const TPair<FGuid, EMapEnemyItem>& Left, const TPair<FGuid, EMapEnemyItem>& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left.Key, Right.Key);
		});
	}

	void AddSpawnedActorToFailSafeTransactionIfValid(AWorldMapObject* SpawnedActor,
	                                                 FCampaignGenerationStepTransaction& InOutFailSafeTransaction)
	{
		if (not IsValid(SpawnedActor))
		{
			return;
		}

		InOutFailSafeTransaction.SpawnedActors.Add(SpawnedActor);
	}

	bool GetPassesEnemyTypeHQDistance(const AAnchorPoint* PlayerHQAnchor, const AAnchorPoint* CandidateAnchor,
	                                  const float MinDistance)
	{
		if (MinDistance <= 0.0f)
		{
			return true;
		}

		if (not IsValid(PlayerHQAnchor) || not IsValid(CandidateAnchor))
		{
			return false;
		}

		const FVector2D PlayerHQLocation = FVector2D(PlayerHQAnchor->GetActorLocation());
		const FVector2D CandidateLocation = FVector2D(CandidateAnchor->GetActorLocation());
		const float MinDistanceSquared = MinDistance * MinDistance;
		return FVector2D::DistSquared(PlayerHQLocation, CandidateLocation) >= MinDistanceSquared;
	}

	FEnemyDeclusterSwapCandidate FindBestEnemyDeclusterSwap(
		const FWorldCampaignPlacementState& PlacementState,
		const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
		const AAnchorPoint* PlayerHQAnchor,
		const int32 TopMCandidates,
		const TFunctionRef<float(EMapEnemyItem)>& GetFailSafeMinDistanceForEnemy,
		const TSet<FString>& FailedSwapPairKeys)
	{
		FEnemyDeclusterSwapCandidate BestCandidate;

		TArray<TPair<FGuid, EMapEnemyItem>> SortedSwappableAnchors;
		BuildSortedSwappableEnemyAnchors(PlacementState, SortedSwappableAnchors);
		if (SortedSwappableAnchors.Num() < 2)
		{
			return BestCandidate;
		}

		TMap<FGuid, FVector2D> AnchorLocationsByKey;
		AnchorLocationsByKey.Reserve(SortedSwappableAnchors.Num());

		TArray<FEnemyDeclusterAnchorScore> AnchorScores;
		AnchorScores.Reserve(SortedSwappableAnchors.Num());
		for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : SortedSwappableAnchors)
		{
			FVector2D AnchorLocationXY = FVector2D::ZeroVector;
			if (not TryGetAnchorLocationXY(EnemyPair.Key, AnchorLookup, AnchorLocationXY))
			{
				continue;
			}

			AnchorLocationsByKey.Add(EnemyPair.Key, AnchorLocationXY);
		}

		if (AnchorLocationsByKey.Num() < 2)
		{
			return BestCandidate;
		}

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
				PlacementState.EnemyItemsByAnchorKey,
				AnchorLocationsByKey);
			AnchorScores.Add(Score);
		}

		if (AnchorScores.Num() < 2)
		{
			return BestCandidate;
		}

		AnchorScores.Sort([](const FEnemyDeclusterAnchorScore& Left, const FEnemyDeclusterAnchorScore& Right)
		{
			if (Left.LocalDensity != Right.LocalDensity)
			{
				return Left.LocalDensity > Right.LocalDensity;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		const int32 MaxBadAnchorsToEvaluate = FMath::Clamp(TopMCandidates, 1, AnchorScores.Num());
		for (int32 IndexA = 0; IndexA < MaxBadAnchorsToEvaluate; ++IndexA)
		{
			const FEnemyDeclusterAnchorScore& AnchorA = AnchorScores[IndexA];
			for (const TPair<FGuid, EMapEnemyItem>& EnemyPairB : SortedSwappableAnchors)
			{
				if (EnemyPairB.Key == AnchorA.AnchorKey || EnemyPairB.Value == AnchorA.EnemyType)
				{
					continue;
				}

				const FString FailedSwapPairKey = BuildCanonicalFailedEnemyDeclusterSwapPairKey(
					AnchorA.AnchorKey,
					EnemyPairB.Key);
				if (FailedSwapPairKeys.Contains(FailedSwapPairKey))
				{
					continue;
				}


				AAnchorPoint* AnchorPointA = FindAnchorByKey(AnchorLookup, AnchorA.AnchorKey);
				AAnchorPoint* AnchorPointB = FindAnchorByKey(AnchorLookup, EnemyPairB.Key);
				if (not IsValid(AnchorPointA) || not IsValid(AnchorPointB))
				{
					continue;
				}

				const float MinDistanceForTypeAtA = GetFailSafeMinDistanceForEnemy(EnemyPairB.Value);
				const float MinDistanceForTypeAtB = GetFailSafeMinDistanceForEnemy(AnchorA.EnemyType);
				if (not GetPassesEnemyTypeHQDistance(PlayerHQAnchor, AnchorPointA, MinDistanceForTypeAtA)
					|| not GetPassesEnemyTypeHQDistance(PlayerHQAnchor, AnchorPointB, MinDistanceForTypeAtB))
				{
					continue;
				}

				TArray<FGuid> ImpactedAnchorKeys;
				BuildImpactedEnemyDeclusterAnchorKeys(
					AnchorA.AnchorKey,
					EnemyPairB.Key,
					SortedSwappableAnchors,
					AnchorLocationsByKey,
					ImpactedAnchorKeys);
				if (ImpactedAnchorKeys.Num() == 0)
				{
					continue;
				}

				const int32 BeforeSum = ComputeImpactedEnemyDeclusterDensitySum(
					ImpactedAnchorKeys,
					PlacementState.EnemyItemsByAnchorKey,
					AnchorLocationsByKey);

				TMap<FGuid, EMapEnemyItem> EnemyTypesAfterSwap = PlacementState.EnemyItemsByAnchorKey;
				EnemyTypesAfterSwap.Add(AnchorA.AnchorKey, EnemyPairB.Value);
				EnemyTypesAfterSwap.Add(EnemyPairB.Key, AnchorA.EnemyType);
				const int32 AfterSum = ComputeImpactedEnemyDeclusterDensitySum(
					ImpactedAnchorKeys,
					EnemyTypesAfterSwap,
					AnchorLocationsByKey);

				const int32 Improvement = BeforeSum - AfterSum;
				if (Improvement <= 0)
				{
					continue;
				}

				const bool bImprovementWins = not BestCandidate.bIsValid || Improvement > BestCandidate.Improvement;
				const bool bTieBreakWins = Improvement == BestCandidate.Improvement
					&& (
						AAnchorPoint::IsAnchorKeyLess(AnchorA.AnchorKey, BestCandidate.AnchorKeyA)
						|| (AnchorA.AnchorKey == BestCandidate.AnchorKeyA
							&& (AAnchorPoint::IsAnchorKeyLess(EnemyPairB.Key, BestCandidate.AnchorKeyB)
								|| (EnemyPairB.Key == BestCandidate.AnchorKeyB
									&& (static_cast<uint8>(AnchorA.EnemyType) < static_cast<uint8>(BestCandidate.
											EnemyTypeA)
										|| (AnchorA.EnemyType == BestCandidate.EnemyTypeA
											&& static_cast<uint8>(EnemyPairB.Value) < static_cast<uint8>(BestCandidate.
												EnemyTypeB)))))));
				if (not bImprovementWins && not bTieBreakWins)
				{
					continue;
				}

				BestCandidate.AnchorKeyA = AnchorA.AnchorKey;
				BestCandidate.AnchorKeyB = EnemyPairB.Key;
				BestCandidate.EnemyTypeA = AnchorA.EnemyType;
				BestCandidate.EnemyTypeB = EnemyPairB.Value;
				BestCandidate.Improvement = Improvement;
				BestCandidate.bIsValid = true;
			}
		}

		return BestCandidate;
	}

	bool TryApplyEnemyDeclusterSwap(const ECampaignGenerationStep FailedStep,
	                                const FEnemyDeclusterSwapCandidate& SwapCandidate,
	                                const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                FWorldCampaignPlacementState& InOutPlacementState,
	                                FWorldCampaignDerivedData& InOutDerivedData,
	                                FCampaignGenerationStepTransaction& InOutFailSafeTransaction,
	                                const bool bDeferWorldObjectPromotion)
	{
		AAnchorPoint* AnchorA = FindAnchorByKey(AnchorLookup, SwapCandidate.AnchorKeyA);
		AAnchorPoint* AnchorB = FindAnchorByKey(AnchorLookup, SwapCandidate.AnchorKeyB);
		if (not IsValid(AnchorA) || not IsValid(AnchorB))
		{
			return false;
		}

		const FWorldCampaignPlacementState PlacementStateBeforeSwap = InOutPlacementState;
		const FWorldCampaignDerivedData DerivedDataBeforeSwap = InOutDerivedData;

		InOutPlacementState.EnemyItemsByAnchorKey.Add(SwapCandidate.AnchorKeyA, SwapCandidate.EnemyTypeB);
		InOutPlacementState.EnemyItemsByAnchorKey.Add(SwapCandidate.AnchorKeyB, SwapCandidate.EnemyTypeA);
		if (bDeferWorldObjectPromotion)
		{
			return true;
		}

		AnchorA->RemovePromotedWorldObject();
		AnchorB->RemovePromotedWorldObject();

		AWorldMapObject* SpawnedObjectA = AnchorA->OnEnemyItemPromotion(SwapCandidate.EnemyTypeB, FailedStep);
		AWorldMapObject* SpawnedObjectB = AnchorB->OnEnemyItemPromotion(SwapCandidate.EnemyTypeA, FailedStep);
		const bool bSpawnedSwapObjects = IsValid(SpawnedObjectA) && IsValid(SpawnedObjectB);
		if (bSpawnedSwapObjects)
		{
			AddSpawnedActorToFailSafeTransactionIfValid(SpawnedObjectA, InOutFailSafeTransaction);
			AddSpawnedActorToFailSafeTransactionIfValid(SpawnedObjectB, InOutFailSafeTransaction);
			return true;
		}

		InOutPlacementState = PlacementStateBeforeSwap;
		InOutDerivedData = DerivedDataBeforeSwap;
		AnchorA->RemovePromotedWorldObject();
		AnchorB->RemovePromotedWorldObject();

		AWorldMapObject* RestoredObjectA = AnchorA->OnEnemyItemPromotion(SwapCandidate.EnemyTypeA, FailedStep);
		AWorldMapObject* RestoredObjectB = AnchorB->OnEnemyItemPromotion(SwapCandidate.EnemyTypeB, FailedStep);
		AddSpawnedActorToFailSafeTransactionIfValid(RestoredObjectA, InOutFailSafeTransaction);
		AddSpawnedActorToFailSafeTransactionIfValid(RestoredObjectB, InOutFailSafeTransaction);

		if (not IsValid(RestoredObjectA) || not IsValid(RestoredObjectB))
		{
			RTSFunctionLibrary::ReportError(TEXT(
				"Timeout fail-safe decluster swap rollback failed to restore original anchor promotions."));
		}

		return false;
	}

	void ApplyTimeoutFailSafeEnemyDeclusterSwaps(const ECampaignGenerationStep FailedStep,
	                                             const FWorldCampaignPlacementFailurePolicy& FailurePolicy,
	                                             const TFunctionRef<float(EMapEnemyItem)>&
	                                             GetFailSafeMinDistanceForEnemy,
	                                             FWorldCampaignPlacementState& InOutPlacementState,
	                                             FWorldCampaignDerivedData& InOutDerivedData,
	                                             FCampaignGenerationStepTransaction& InOutFailSafeTransaction,
	                                             const bool bDeferWorldObjectPromotion)
	{
		const int32 SwapBudget = FMath::Max(0, FailurePolicy.TimeoutFailSafeEnemyDeclusterSwapBudget);
		if (SwapBudget == 0)
		{
			return;
		}

		TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup;
		AnchorLookup.Reserve(InOutPlacementState.CachedAnchors.Num());
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : InOutPlacementState.CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			AnchorLookup.Add(AnchorPoint->GetAnchorKey(), AnchorPoint);
		}

		const int32 TopMCandidates = FMath::Max(2, FailurePolicy.TimeoutFailSafeEnemyDeclusterTopMCandidates);
		TSet<FString> FailedSwapPairKeys;
		int32 AppliedSwaps = 0;
		while (AppliedSwaps < SwapBudget)
		{
			const FEnemyDeclusterSwapCandidate BestSwap = FindBestEnemyDeclusterSwap(
				InOutPlacementState,
				AnchorLookup,
				InOutPlacementState.PlayerHQAnchor.Get(),
				TopMCandidates,
				GetFailSafeMinDistanceForEnemy,
				FailedSwapPairKeys);
			if (not BestSwap.bIsValid)
			{
				return;
			}

			if (TryApplyEnemyDeclusterSwap(FailedStep, BestSwap, AnchorLookup, InOutPlacementState,
			                               InOutDerivedData, InOutFailSafeTransaction,
			                               bDeferWorldObjectPromotion))
			{
				AppliedSwaps++;
				continue;
			}

			const FString FailedSwapPairKey = BuildCanonicalFailedEnemyDeclusterSwapPairKey(
				BestSwap.AnchorKeyA,
				BestSwap.AnchorKeyB);
			FailedSwapPairKeys.Add(FailedSwapPairKey);
		}
	}

	struct FScopedRTSModalDialogSuppression
	{
		FScopedRTSModalDialogSuppression()
		{
			RTSFunctionLibrary::PushModalDialogSuppression();
		}

		~FScopedRTSModalDialogSuppression()
		{
			RTSFunctionLibrary::PopModalDialogSuppression();
		}
	};

	FString BuildParityKeyString(const FGuid& Key)
	{
		return Key.ToString(EGuidFormats::DigitsWithHyphens);
	}

	template <typename EnumType>
	FString BuildParityEnumKeyString(const EnumType Key)
	{
		return FString::Printf(TEXT("%d"), static_cast<int32>(Key));
	}

	template <typename ValueType>
	void LogParityPlacementMap(const TCHAR* Prefix, const TCHAR* Label, const TMap<FGuid, ValueType>& PlacementMap)
	{
		TArray<FGuid> AnchorKeys;
		PlacementMap.GetKeys(AnchorKeys);
		AnchorKeys.Sort([](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& AnchorKey : AnchorKeys)
		{
			UE_LOG(LogTemp, Display,
			       TEXT("World campaign placement parity: %s %s %s=%d."),
			       Prefix,
			       Label,
			       *BuildParityKeyString(AnchorKey),
			       static_cast<int32>(PlacementMap.FindRef(AnchorKey)));
		}
	}

	void LogPlacementParityResult(const TCHAR* Prefix, const FWorldCampaignPlacementResult& Result)
	{
		UE_LOG(LogTemp, Display,
		       TEXT("World campaign placement parity: %s PlayerHQ=%s EnemyHQ=%s Enemy=%d Neutral=%d Missions=%d."),
		       Prefix,
		       *BuildParityKeyString(Result.PlayerHQAnchorKey),
		       *BuildParityKeyString(Result.EnemyHQAnchorKey),
		       Result.EnemyItemsByAnchorKey.Num(),
		       Result.NeutralItemsByAnchorKey.Num(),
		       Result.MissionsByAnchorKey.Num());
		LogParityPlacementMap(Prefix, TEXT("Enemy"), Result.EnemyItemsByAnchorKey);
		LogParityPlacementMap(Prefix, TEXT("Neutral"), Result.NeutralItemsByAnchorKey);
		LogParityPlacementMap(Prefix, TEXT("Mission"), Result.MissionsByAnchorKey);
	}

	void LogAsyncPlacementResultSummary(const TCHAR* Context, const int32 Seed, const int32 SetupPassIndex,
	                                    const FWorldCampaignPlacementResult& Result)
	{
		UE_LOG(LogTemp, Display,
		       TEXT(
			       "World campaign async placement summary: Context=%s Seed=%d SetupPass=%d Succeeded=%s FailureRetries=%d PlacementAttempts=%d Backtracks=%d MicroBacktracks=%d MacroBacktracks=%d SetupBacktrackRequests=%d SetupTransactionsToUndo=%d StepFailureRetries(PlayerHQ/EnemyHQ/Wall/Enemy/Neutral/Mission)=%d/%d/%d/%d/%d/%d."
		       ),
		       Context,
		       Seed,
		       SetupPassIndex,
		       Result.bSucceeded ? TEXT("true") : TEXT("false"),
		       Result.WorkerTotalAttemptCount,
		       Result.WorkerPlacementAttemptCount,
		       Result.WorkerBacktrackCount,
		       Result.WorkerMicroBacktrackCount,
		       Result.WorkerMacroBacktrackCount,
		       Result.WorkerSetupBacktrackRequestCount,
		       Result.GameThreadTransactionsToUndo,
		       Result.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::PlayerHQPlaced),
		       Result.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::EnemyHQPlaced),
		       Result.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::EnemyWallPlaced),
		       Result.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::EnemyObjectsPlaced),
		       Result.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::NeutralObjectsPlaced),
		       Result.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::MissionsPlaced));
	}

	template <typename KeyType, typename ValueType, typename KeyStringFunctionType>
	bool GetParityMapMatches(const TMap<KeyType, ValueType>& SynchronousMap,
	                         const TMap<KeyType, ValueType>& AsyncMap,
	                         const TCHAR* Label,
	                         KeyStringFunctionType BuildKeyString,
	                         FString& OutFailureReason)
	{
		if (SynchronousMap.Num() != AsyncMap.Num())
		{
			OutFailureReason = FString::Printf(
				TEXT("%s count mismatch. Sync=%d Async=%d."),
				Label,
				SynchronousMap.Num(),
				AsyncMap.Num());
			return false;
		}

		for (const TPair<KeyType, ValueType>& Pair : SynchronousMap)
		{
			const ValueType* AsyncValue = AsyncMap.Find(Pair.Key);
			if (AsyncValue == nullptr)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s missing async key %s."),
					Label,
					*BuildKeyString(Pair.Key));
				return false;
			}

			if (*AsyncValue != Pair.Value)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s value mismatch for key %s."),
					Label,
					*BuildKeyString(Pair.Key));
				return false;
			}
		}

		return true;
	}

	bool GetParityFloatMapMatches(const TMap<FGuid, float>& SynchronousMap,
	                              const TMap<FGuid, float>& AsyncMap,
	                              const TCHAR* Label,
	                              FString& OutFailureReason)
	{
		if (SynchronousMap.Num() != AsyncMap.Num())
		{
			OutFailureReason = FString::Printf(
				TEXT("%s count mismatch. Sync=%d Async=%d."),
				Label,
				SynchronousMap.Num(),
				AsyncMap.Num());
			return false;
		}

		constexpr float FloatTolerance = 0.001f;
		for (const TPair<FGuid, float>& Pair : SynchronousMap)
		{
			const float* AsyncValue = AsyncMap.Find(Pair.Key);
			if (AsyncValue == nullptr)
			{
				OutFailureReason = FString::Printf(
					TEXT("%s missing async key %s."),
					Label,
					*BuildParityKeyString(Pair.Key));
				return false;
			}

			if (not FMath::IsNearlyEqual(*AsyncValue, Pair.Value, FloatTolerance))
			{
				OutFailureReason = FString::Printf(
					TEXT("%s value mismatch for key %s. Sync=%.3f Async=%.3f."),
					Label,
					*BuildParityKeyString(Pair.Key),
					Pair.Value,
					*AsyncValue);
				return false;
			}
		}

		return true;
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
			OutSteps.Add(ECampaignGenerationStep::AnchorPointsGenerated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::NeutralObjectsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyWallPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			OutSteps.Add(ECampaignGenerationStep::AnchorPointsGenerated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyObjectsPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyWallPlaced);
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			OutSteps.Add(ECampaignGenerationStep::AnchorPointsGenerated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyWallPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::EnemyHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			OutSteps.Add(ECampaignGenerationStep::AnchorPointsGenerated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::EnemyHQPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::PlayerHQPlaced);
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			OutSteps.Add(ECampaignGenerationStep::AnchorPointsGenerated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::PlayerHQPlaced)
		{
			OutSteps.Add(ECampaignGenerationStep::ConnectionsCreated);
			OutSteps.Add(ECampaignGenerationStep::AnchorPointsGenerated);
			return;
		}

		if (FailedStep == ECampaignGenerationStep::ConnectionsCreated)
		{
			OutSteps.Add(ECampaignGenerationStep::AnchorPointsGenerated);
			OutSteps.Add(ECampaignGenerationStep::NotStarted);
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
	M_AnchorPointGenerationSettings.M_GeneratedAnchorPointClass = AAnchorPoint::StaticClass();
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

	M_WorldDataComponent = FindComponentByClass<UWorldDataComponent>();
	M_WorldCountryOccupationRegulator = FindComponentByClass<UWorldCountryOccupationRegulator>();
	ApplyDebuggerSettingsToComponent();
}

void AGeneratorWorldCampaign::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAsyncPlacementBacktracking();
	Super::EndPlay(EndPlayReason);
}

FWorldCampaignGenerationFinishedDelegate& AGeneratorWorldCampaign::OnGenerationFinished()
{
	return M_OnGenerationFinished;
}

bool AGeneratorWorldCampaign::GetIsGenerationFinished() const
{
	return M_GenerationStep == ECampaignGenerationStep::Finished;
}

void AGeneratorWorldCampaign::InitializeCountryOccupationRegulator()
{
	if (not GetIsValidWorldCountryOccupationRegulator())
	{
		return;
	}

	M_WorldCountryOccupationRegulator->InitializeCountryOccupation(this);
}

void AGeneratorWorldCampaign::LoadWorldDataIntoObjects()
{
	if (not GetIsValidWorldDataComponent())
	{
		return;
	}

	M_WorldDataComponent->LoadWorldDataIntoObjects(this);
}

void AGeneratorWorldCampaign::InitMapObjectsBaseFortificationStrength(const ERTSGameDifficulty GameDifficulty)
{
	if (not GetIsValidWorldDataComponent())
	{
		return;
	}

	/*
	 * Base fortification strength is stored separately from modifier reasons. This helper refreshes only the modifier
	 * portion so generation and restore can rebuild FortificationStrength without disturbing strategic support/field data.
	 */
	const auto ApplyFortificationModifierReasons =
		[this, GameDifficulty](AWorldMapObject* WorldObject,
		                        const UWorldFortificationModificationsComponent* FortificationModificationsComponent)
		{
			if (not IsValid(WorldObject))
			{
				return;
			}

			UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				WorldObject->GetWorldStrengthEstimationComponent();
			if (not IsValid(StrengthEstimationComponent))
			{
				return;
			}

			StrengthEstimationComponent->ResetFortificationModifierReport();
			if (not IsValid(FortificationModificationsComponent))
			{
				return;
			}

			for (const EWorldFortificationStrength FortificationStrength :
			     FortificationModificationsComponent->GetFortificationStrengths())
			{
				FWorldStrengthReason FortificationReason;
				if (M_WorldDataComponent->TryBuildFortificationStrengthReason(
					FortificationStrength,
					GameDifficulty,
					FortificationReason))
				{
					StrengthEstimationComponent->AddFortificationModifierReason(FortificationReason);
				}
			}
		};

	const FWorldDifficultyMapObjects MapObjects = BuildWorldDifficultyMapObjects(M_PlacementState);
	for (AWorldEnemyObject* EnemyObject : MapObjects.EnemyObjects)
	{
		if (not IsValid(EnemyObject))
		{
			continue;
		}

		FWorldStrengthReason BaseFortificationStrengthReason;
		M_WorldDataComponent->TryBuildEnemyBaseDifficultyReason(
			EnemyObject->GetEnemyItemType(),
			GameDifficulty,
			BaseFortificationStrengthReason);

		/*
		 * Write base strength directly to the shared component. Enemy/mission objects keep their identity data,
		 * while UWorldStrengthEstimationComponent owns the strength report.
		 */
		if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
			EnemyObject->GetWorldStrengthEstimationComponent())
		{
			StrengthEstimationComponent->SetBaseFortificationStrengthReason(BaseFortificationStrengthReason);
		}
		ApplyFortificationModifierReasons(EnemyObject, EnemyObject->GetFortificationModificationsComponent());
	}

	for (AWorldMissionObject* MissionObject : MapObjects.MissionObjects)
	{
		if (not IsValid(MissionObject))
		{
			continue;
		}

		FWorldStrengthReason BaseFortificationStrengthReason;
		M_WorldDataComponent->TryBuildMissionBaseDifficultyReason(
			MissionObject->GetMissionType(),
			GameDifficulty,
			BaseFortificationStrengthReason);

		/*
		 * Missions use the same report component path as enemies; only the WorldData lookup key differs.
		 */
		if (UWorldStrengthEstimationComponent* StrengthEstimationComponent =
			MissionObject->GetWorldStrengthEstimationComponent())
		{
			StrengthEstimationComponent->SetBaseFortificationStrengthReason(BaseFortificationStrengthReason);
		}
		ApplyFortificationModifierReasons(MissionObject, MissionObject->GetFortificationModificationsComponent());
	}
}

void AGeneratorWorldCampaign::AdjustDifficultyPercentagesForStrategicSupport(
	const ERTSGameDifficulty GameDifficulty)
{
	if (not GetIsValidWorldDataComponent())
	{
		return;
	}

	const FWorldDifficultyMapObjects MapObjects = BuildWorldDifficultyMapObjects(M_PlacementState);
	ResetStrategicReport(MapObjects);
	ApplyStrategicSupport(MapObjects, *M_WorldDataComponent, GameDifficulty);
}

void AGeneratorWorldCampaign::AdjustDifficultyPercentagesForFieldDivisions(
	const ERTSGameDifficulty)
{
	const FWorldDifficultyMapObjects MapObjects = BuildWorldDifficultyMapObjects(M_PlacementState);
	ResetFieldDivisionReport(MapObjects);
}

const UWorldDataComponent* AGeneratorWorldCampaign::GetWorldDataComponent() const
{
	if (not GetIsValidWorldDataComponent())
	{
		return nullptr;
	}

	return M_WorldDataComponent;
}

void AGeneratorWorldCampaign::InitializeWorldGenerator(AWorldPlayerController* WorldPlayerController,
                                                       const FCampaignGenerationSettings CampaignGenerationSettings,
                                                       const FRTSGameDifficulty DifficultySettings)
{
	if (not IsValid(WorldPlayerController))
	{
		RTSFunctionLibrary::ReportError(TEXT("InitializeWorldGenerator called with invalid WorldPlayerController."));
		return;
	}
	M_WorldPlayerController = WorldPlayerController;
	/*
	 * Automated async map runs can pass -RTSWorldCampaignSeed=N so the real BeginPlay path is tested
	 * with the same seed range as the parity command, without changing saved/default generator settings.
	 */
	M_CountAndDifficultyTuning.Seed = GetCampaignGenerationSeedWithCommandLineOverride(
		CampaignGenerationSettings.GenerationSeed);
	M_CountAndDifficultyTuning.DifficultyLevel = DifficultySettings.DifficultyLevel;
	M_CountAndDifficultyTuning.DifficultyPercentage = DifficultySettings.DifficultyPercentage;
	if (CampaignGenerationSettings.bUsesExtraDifficultyPercentage)
	{
		M_CountAndDifficultyTuning.DifficultyPercentage += M_CountAndDifficultyTuning.AddedDifficultyPercentage;
	}
}

void AGeneratorWorldCampaign::StartWorldGeneration()
{
	if (not M_WorldPlayerController.IsValid())
	{
		RTSFunctionLibrary::ReportError(TEXT("StartWorldGeneration called before InitializeWorldGenerator."));
		return;
	}

	M_WorldPlayerController->UpdateAsyncWorldGenerationWidget_AnchorPlacementStarted();
	ExecuteAllSteps();
}

FWorldCampaignState AGeneratorWorldCampaign::BuildWorldCampaignStateFromCurrentGeneration() const
{
	FWorldCampaignState WorldCampaignState;
	WorldCampaignState.GenerationStep = M_GenerationStep;
	WorldCampaignState.PlayerHQAnchorKey = M_PlacementState.PlayerHQAnchorKey;
	WorldCampaignState.EnemyHQAnchorKey = M_PlacementState.EnemyHQAnchorKey;
	AddSavedAnchorsAndMapItems(WorldCampaignState);
	AddSavedConnections(WorldCampaignState);
	return WorldCampaignState;
}

void AGeneratorWorldCampaign::RestoreWorldStateFromSave(const FWorldCampaignState& WorldCampaignState)
{
	EraseAllGeneration();
	if (not WorldCampaignState.GetIsValidForRestore())
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot restore world campaign state because the save data is invalid."));
		return;
	}

	if (not IsValid(GetWorld()))
	{
		RTSFunctionLibrary::ReportError(TEXT("Cannot restore world campaign state because the world is invalid."));
		return;
	}

	TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorsByKey;
	TArray<TObjectPtr<AAnchorPoint>> RestoredAnchors;
	RestoreSavedAnchors(WorldCampaignState, AnchorsByKey, RestoredAnchors);
	RestoreSavedConnections(WorldCampaignState, AnchorsByKey);
	RestoreSavedMapItems(WorldCampaignState, AnchorsByKey);
	ApplyRestoredPlacementState(WorldCampaignState, AnchorsByKey, RestoredAnchors);
}

void AGeneratorWorldCampaign::PruneUnusedAnchorsAndRepairConnectivity()
{
	WorldCampaignPruningHelper::FWorldCampaignPruningContext PruningContext;
	PruningContext.World = GetWorld();
	PruningContext.Owner = this;
	PruningContext.PlayerHQAnchor = M_PlacementState.PlayerHQAnchor;
	PruningContext.ConnectionClass = M_ConnectionClass;
	PruningContext.GeneratedAnchorTag = GeneratedAnchorTag;
	PruningContext.GameplayAnchorKeys = BuildGameplayAnchorKeysForPruning();
	PruningContext.CachedAnchors = &M_PlacementState.CachedAnchors;
	PruningContext.CachedConnections = &M_PlacementState.CachedConnections;
	PruningContext.GeneratedConnections = &M_GeneratedConnections;

	const WorldCampaignPruningHelper::FWorldCampaignPruningResult PruningResult =
		WorldCampaignPruningHelper::PruneUnusedAnchorsAndRepairConnectivity(PruningContext);
	RemovePrunedPlacementStateEntries();
	if (not PruningResult.bDidChange)
	{
		RefreshCampaignGraphAfterPruning();
		return;
	}

	RefreshCampaignGraphAfterPruning();
	RefreshConnectionTransactionAfterPruning();
}

bool AGeneratorWorldCampaign::GenerateAndValidatePrunedWorldForSeed(const int32 Seed, FString& OutFailureReason)
{
	const FScopedRTSModalDialogSuppression DialogSuppression;
	EraseAllGeneration();
	M_CountAndDifficultyTuning.Seed = Seed;

	FString PlacementFailureReason;
	if (not GenerateWorldWithAsyncPlacementForPruningValidation(PlacementFailureReason))
	{
		OutFailureReason = FString::Printf(
			TEXT("generation failed for seed %d: %s"),
			Seed,
			*PlacementFailureReason);
		return false;
	}

	PruneUnusedAnchorsAndRepairConnectivity();
	if (ValidateCurrentPrunedWorld(OutFailureReason))
	{
		return true;
	}

	OutFailureReason = FString::Printf(TEXT("seed %d failed pruning validation: %s"), Seed, *OutFailureReason);
	return false;
}

bool AGeneratorWorldCampaign::ValidateCurrentPrunedWorld(FString& OutFailureReason) const
{
	if (not GetPrunedCachedAnchorsHaveOnlyGameplay(OutFailureReason))
	{
		return false;
	}

	if (not GetPrunedConnectionsAreValid(OutFailureReason))
	{
		return false;
	}

	if (not GetPrunedWorldActorsMatchCachedGraph(OutFailureReason))
	{
		return false;
	}

	return GetPrunedAnchorsReachableFromPlayerHQ(OutFailureReason);
}

void AGeneratorWorldCampaign::AddSavedAnchorsAndMapItems(FWorldCampaignState& WorldCampaignState) const
{
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
		if (not AnchorKey.IsValid())
		{
			RTSFunctionLibrary::ReportError(TEXT("World campaign anchor has an invalid save key."));
		}

		FWorldCampaignAnchorSaveData AnchorSaveData;
		AnchorSaveData.AnchorKey = AnchorKey;
		AnchorSaveData.Transform = AnchorPoint->GetActorTransform();
		WorldCampaignState.Anchors.Add(AnchorSaveData);

		AWorldMapObject* PromotedWorldObject = AnchorPoint->GetPromotedWorldObject();
		if (not IsValid(PromotedWorldObject))
		{
			continue;
		}

		FWorldCampaignMapItemSaveData MapItemSaveData;
		MapItemSaveData.AnchorKey = AnchorKey;
		if (const AWorldEnemyObject* EnemyObject = Cast<AWorldEnemyObject>(PromotedWorldObject))
		{
			MapItemSaveData.MapItemType = EMapItemType::EnemyItem;
			MapItemSaveData.EnemyItemType = EnemyObject->GetEnemyItemType();
			if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				EnemyObject->GetWorldStrengthEstimationComponent())
			{
				MapItemSaveData.BaseFortificationStrength =
					StrengthEstimationComponent->GetBaseFortificationStrengthPercentage();
			}
			if (const UWorldFortificationModificationsComponent* FortificationModificationsComponent =
				EnemyObject->GetFortificationModificationsComponent())
			{
				MapItemSaveData.FortificationStrengthModifiers =
					FortificationModificationsComponent->GetFortificationStrengths();
				MapItemSaveData.bHasSavedFortificationStrengthModifiers = true;
			}
		}
		else if (const AWorldMissionObject* MissionObject = Cast<AWorldMissionObject>(PromotedWorldObject))
		{
			MapItemSaveData.MapItemType = EMapItemType::Mission;
			MapItemSaveData.MissionType = MissionObject->GetMissionType();
			if (const UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				MissionObject->GetWorldStrengthEstimationComponent())
			{
				MapItemSaveData.BaseFortificationStrength =
					StrengthEstimationComponent->GetBaseFortificationStrengthPercentage();
			}
			if (const UWorldFortificationModificationsComponent* FortificationModificationsComponent =
				MissionObject->GetFortificationModificationsComponent())
			{
				MapItemSaveData.FortificationStrengthModifiers =
					FortificationModificationsComponent->GetFortificationStrengths();
				MapItemSaveData.bHasSavedFortificationStrengthModifiers = true;
			}
		}
		else if (const AWorldNeutralObject* NeutralObject = Cast<AWorldNeutralObject>(PromotedWorldObject))
		{
			MapItemSaveData.MapItemType = EMapItemType::NeutralItem;
			MapItemSaveData.NeutralObjectType = NeutralObject->GetNeutralObjectType();
		}
		else if (const AWorldPlayerObject* PlayerObject = Cast<AWorldPlayerObject>(PromotedWorldObject))
		{
			MapItemSaveData.MapItemType = EMapItemType::PlayerItem;
			MapItemSaveData.PlayerItemType = PlayerObject->GetPlayerItemType();
		}

		if (MapItemSaveData.MapItemType != EMapItemType::None)
		{
			WorldCampaignState.MapItems.Add(MapItemSaveData);
		}
	}
}

void AGeneratorWorldCampaign::AddSavedConnections(FWorldCampaignState& WorldCampaignState) const
{
	for (const TObjectPtr<AConnection>& Connection : M_PlacementState.CachedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		FWorldCampaignConnectionSaveData ConnectionSaveData;
		ConnectionSaveData.bM_IsThreeWayConnection = Connection->GetIsThreeWayConnection();
		ConnectionSaveData.JunctionLocation = Connection->GetJunctionLocation();
		for (const TObjectPtr<AAnchorPoint>& ConnectedAnchor : Connection->GetConnectedAnchors())
		{
			if (not IsValid(ConnectedAnchor))
			{
				continue;
			}

			const FGuid AnchorKey = ConnectedAnchor->GetAnchorKey();
			if (not AnchorKey.IsValid())
			{
				RTSFunctionLibrary::ReportError(
					TEXT("World campaign connection endpoint has an invalid anchor save key."));
			}

			ConnectionSaveData.ConnectedAnchorKeys.Add(AnchorKey);
		}

		if (ConnectionSaveData.ConnectedAnchorKeys.Num() >= 2)
		{
			WorldCampaignState.Connections.Add(ConnectionSaveData);
		}
	}
}

void AGeneratorWorldCampaign::RestoreSavedAnchors(const FWorldCampaignState& WorldCampaignState,
                                                  TMap<FGuid, TObjectPtr<AAnchorPoint>>& OutAnchorsByKey,
                                                  TArray<TObjectPtr<AAnchorPoint>>& OutRestoredAnchors)
{
	UWorld* World = GetWorld();
	const UWorldCampaignSettings* WorldCampaignSettings = UWorldCampaignSettings::Get();
	TSubclassOf<AAnchorPoint> AnchorClass = M_AnchorPointGenerationSettings.M_GeneratedAnchorPointClass;
	if (not IsValid(AnchorClass.Get()))
	{
		AnchorClass = AAnchorPoint::StaticClass();
	}

	for (const FWorldCampaignAnchorSaveData& AnchorSaveData : WorldCampaignState.Anchors)
	{
		if (not AnchorSaveData.AnchorKey.IsValid())
		{
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		AAnchorPoint* SpawnedAnchor = World->SpawnActor<AAnchorPoint>(AnchorClass, AnchorSaveData.Transform,
		                                                              SpawnParameters);
		if (not IsValid(SpawnedAnchor))
		{
			RTSFunctionLibrary::ReportError(TEXT("Failed to restore saved campaign anchor."));
			continue;
		}

		SpawnedAnchor->SetAnchorKey(AnchorSaveData.AnchorKey, true);
		SpawnedAnchor->InitializeCampaignSettings(WorldCampaignSettings);
		SpawnedAnchor->Tags.AddUnique(GeneratedAnchorTag);
		OutAnchorsByKey.Add(AnchorSaveData.AnchorKey, SpawnedAnchor);
		OutRestoredAnchors.Add(SpawnedAnchor);
	}
}

void AGeneratorWorldCampaign::RestoreSavedConnections(const FWorldCampaignState& WorldCampaignState,
                                                      const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorsByKey)
{
	UWorld* World = GetWorld();
	TSubclassOf<AConnection> ConnectionClass = M_ConnectionClass;
	if (not IsValid(ConnectionClass.Get()))
	{
		ConnectionClass = AConnection::StaticClass();
	}

	M_GeneratedConnections.Reset();
	for (const FWorldCampaignConnectionSaveData& ConnectionSaveData : WorldCampaignState.Connections)
	{
		if (ConnectionSaveData.ConnectedAnchorKeys.Num() < 2)
		{
			continue;
		}

		TObjectPtr<AAnchorPoint> const* AnchorA = AnchorsByKey.Find(ConnectionSaveData.ConnectedAnchorKeys[0]);
		TObjectPtr<AAnchorPoint> const* AnchorB = AnchorsByKey.Find(ConnectionSaveData.ConnectedAnchorKeys[1]);
		if (not AnchorA || not AnchorB || not IsValid(AnchorA->Get()) || not IsValid(AnchorB->Get()))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("Failed to restore saved campaign connection because an anchor is missing."));
			continue;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		AConnection* SpawnedConnection = World->SpawnActor<AConnection>(ConnectionClass, FTransform::Identity,
		                                                                SpawnParameters);
		if (not IsValid(SpawnedConnection))
		{
			RTSFunctionLibrary::ReportError(TEXT("Failed to restore saved campaign connection."));
			continue;
		}

		SpawnedConnection->InitializeConnection(AnchorA->Get(), AnchorB->Get());
		RegisterConnectionOnAnchors(SpawnedConnection, AnchorA->Get(), AnchorB->Get());
		if (ConnectionSaveData.bM_IsThreeWayConnection && ConnectionSaveData.ConnectedAnchorKeys.IsValidIndex(2))
		{
			TObjectPtr<AAnchorPoint> const* ThirdAnchor = AnchorsByKey.Find(ConnectionSaveData.ConnectedAnchorKeys[2]);
			if (ThirdAnchor && IsValid(ThirdAnchor->Get()) && SpawnedConnection->TryAddThirdAnchor(ThirdAnchor->Get()))
			{
				RegisterThirdAnchorOnConnection(SpawnedConnection, ThirdAnchor->Get());
			}
		}

		M_GeneratedConnections.Add(SpawnedConnection);
	}
}

void AGeneratorWorldCampaign::RestoreSavedMapItems(const FWorldCampaignState& WorldCampaignState,
                                                   const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorsByKey)
{
	const auto RestoreStrengthData =
		[this](AWorldMapObject* WorldObject,
		       UWorldFortificationModificationsComponent* FortificationModificationsComponent,
		       const FWorldCampaignMapItemSaveData& MapItemSaveData)
		{
			if (not IsValid(WorldObject))
			{
				return;
			}

			UWorldStrengthEstimationComponent* StrengthEstimationComponent =
				WorldObject->GetWorldStrengthEstimationComponent();
			if (IsValid(StrengthEstimationComponent))
			{
				StrengthEstimationComponent->SetBaseFortificationStrengthPercentage(
					FMath::RoundToInt(MapItemSaveData.BaseFortificationStrength));
				StrengthEstimationComponent->ResetFortificationModifierReport();
			}

			TArray<EWorldFortificationStrength> FortificationStrengthModifiersToApply;
			const bool bShouldUseSavedFortificationStrengthModifiers =
				MapItemSaveData.bHasSavedFortificationStrengthModifiers
				|| MapItemSaveData.FortificationStrengthModifiers.Num() > 0;

			/*
			 * Old saves did not serialize the modifier array. If the flag is missing and the array is empty, keep the
			 * component's current/default modifiers instead of interpreting that as "designer intentionally saved none."
			 */
			if (IsValid(FortificationModificationsComponent))
			{
				if (bShouldUseSavedFortificationStrengthModifiers)
				{
					FortificationModificationsComponent->SetFortificationStrengths(
						MapItemSaveData.FortificationStrengthModifiers);
				}

				FortificationStrengthModifiersToApply =
					FortificationModificationsComponent->GetFortificationStrengths();
			}
			else if (bShouldUseSavedFortificationStrengthModifiers)
			{
				FortificationStrengthModifiersToApply = MapItemSaveData.FortificationStrengthModifiers;
			}

			if (not IsValid(StrengthEstimationComponent) || not IsValid(M_WorldDataComponent))
			{
				return;
			}

			for (const EWorldFortificationStrength FortificationStrength :
			     FortificationStrengthModifiersToApply)
			{
				FWorldStrengthReason FortificationReason;
				if (M_WorldDataComponent->TryBuildFortificationStrengthReason(
					FortificationStrength,
					M_CountAndDifficultyTuning.DifficultyLevel,
					FortificationReason))
				{
					StrengthEstimationComponent->AddFortificationModifierReason(FortificationReason);
				}
			}
		};

	for (const FWorldCampaignMapItemSaveData& MapItemSaveData : WorldCampaignState.MapItems)
	{
		TObjectPtr<AAnchorPoint> const* AnchorPoint = AnchorsByKey.Find(MapItemSaveData.AnchorKey);
		if (not AnchorPoint || not IsValid(AnchorPoint->Get()))
		{
			RTSFunctionLibrary::ReportError(
				TEXT("Failed to restore saved campaign map item because the anchor is missing."));
			continue;
		}

		switch (MapItemSaveData.MapItemType)
		{
		case EMapItemType::EnemyItem:
			if (AWorldEnemyObject* EnemyObject = Cast<AWorldEnemyObject>(
				AnchorPoint->Get()->OnEnemyItemPromotion(MapItemSaveData.EnemyItemType, ECampaignGenerationStep::Finished)))
			{
				RestoreStrengthData(
					EnemyObject,
					EnemyObject->GetFortificationModificationsComponent(),
					MapItemSaveData);
			}
			M_PlacementState.EnemyItemsByAnchorKey.Add(MapItemSaveData.AnchorKey, MapItemSaveData.EnemyItemType);
			M_DerivedData.EnemyItemPlacedCounts.FindOrAdd(MapItemSaveData.EnemyItemType)++;
			break;
		case EMapItemType::Mission:
			if (AWorldMissionObject* MissionObject = Cast<AWorldMissionObject>(
				AnchorPoint->Get()->OnMissionPromotion(MapItemSaveData.MissionType, ECampaignGenerationStep::Finished)))
			{
				RestoreStrengthData(
					MissionObject,
					MissionObject->GetFortificationModificationsComponent(),
					MapItemSaveData);
			}
			M_PlacementState.MissionsByAnchorKey.Add(MapItemSaveData.AnchorKey, MapItemSaveData.MissionType);
			break;
		case EMapItemType::NeutralItem:
			AnchorPoint->Get()->OnNeutralItemPromotion(MapItemSaveData.NeutralObjectType,
			                                           ECampaignGenerationStep::Finished);
			M_PlacementState.NeutralItemsByAnchorKey.Add(MapItemSaveData.AnchorKey, MapItemSaveData.NeutralObjectType);
			break;
		case EMapItemType::PlayerItem:
			AnchorPoint->Get()->
			             OnPlayerItemPromotion(MapItemSaveData.PlayerItemType, ECampaignGenerationStep::Finished);
			break;
		default:
			break;
		}
	}
}

void AGeneratorWorldCampaign::ApplyRestoredPlacementState(const FWorldCampaignState& WorldCampaignState,
                                                          const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorsByKey,
                                                          const TArray<TObjectPtr<AAnchorPoint>>& RestoredAnchors)
{
	CacheGeneratedState(RestoredAnchors);
	M_PlacementState.PlayerHQAnchorKey = WorldCampaignState.PlayerHQAnchorKey;
	M_PlacementState.EnemyHQAnchorKey = WorldCampaignState.EnemyHQAnchorKey;
	if (TObjectPtr<AAnchorPoint> const* PlayerHQAnchor = AnchorsByKey.Find(WorldCampaignState.PlayerHQAnchorKey))
	{
		M_PlacementState.PlayerHQAnchor = PlayerHQAnchor->Get();
	}
	if (TObjectPtr<AAnchorPoint> const* EnemyHQAnchor = AnchorsByKey.Find(WorldCampaignState.EnemyHQAnchorKey))
	{
		M_PlacementState.EnemyHQAnchor = EnemyHQAnchor->Get();
	}
	M_GenerationStep = WorldCampaignState.GenerationStep;
}


#if WITH_EDITOR

void AGeneratorWorldCampaign::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ApplyDebuggerSettingsToComponent();
}
#endif

void AGeneratorWorldCampaign::GenerateAnchorPointsStep()
{
	if (not CanExecuteStep(ECampaignGenerationStep::AnchorPointsGenerated))
	{
		RTSFunctionLibrary::ReportError(TEXT("GenerateAnchorPointsStep called out of order."));
		return;
	}

	ExecuteStepWithTransaction(ECampaignGenerationStep::AnchorPointsGenerated,
	                           &AGeneratorWorldCampaign::ExecuteGenerateAnchorPoints);
}

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

	if (not ExecuteGameThreadSetupStepsBeforeAsyncPlacement())
	{
		return;
	}

	StartAsyncPlacementBacktracking();
}

bool AGeneratorWorldCampaign::ExecuteGameThreadSetupStepsBeforeAsyncPlacement()
{
	TArray<ECampaignGenerationStep> StepOrder;
	StepOrder.Add(ECampaignGenerationStep::AnchorPointsGenerated);
	StepOrder.Add(ECampaignGenerationStep::ConnectionsCreated);

	int32 StepIndex = StepOrder.IndexOfByKey(GetNextStep(M_GenerationStep));
	if (M_GenerationStep == ECampaignGenerationStep::NotStarted)
	{
		StepIndex = 0;
	}

	if (M_GenerationStep == ECampaignGenerationStep::ConnectionsCreated)
	{
		return true;
	}

	if (StepIndex == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("Async setup failed: generation step cannot resume setup."));
		return false;
	}

	while (StepIndex < StepOrder.Num())
	{
		const ECampaignGenerationStep StepToExecute = StepOrder[StepIndex];
		bool (AGeneratorWorldCampaign::*StepFunction)(FCampaignGenerationStepTransaction&) = nullptr;
		if (StepToExecute == ECampaignGenerationStep::AnchorPointsGenerated)
		{
			StepFunction = &AGeneratorWorldCampaign::ExecuteGenerateAnchorPoints;
		}
		else
		{
			StepFunction = &AGeneratorWorldCampaign::ExecuteCreateConnections;
		}

		if (ExecuteStepWithTransaction(StepToExecute, StepFunction))
		{
			if (StepToExecute == ECampaignGenerationStep::AnchorPointsGenerated && M_WorldPlayerController.IsValid())
			{
				M_WorldPlayerController->UpdateAsyncWorldGenerationWidget_AnchorPlacementComplete();
			}

			if (StepToExecute == ECampaignGenerationStep::ConnectionsCreated && M_WorldPlayerController.IsValid())
			{
				M_WorldPlayerController->UpdateAsyncWorldGenerationWidget_ConnectionGenerationComplete();
			}

			StepIndex++;
			continue;
		}

		if (not HandleStepFailure(StepToExecute, StepIndex, StepOrder))
		{
			return false;
		}
	}

	return true;
}

void AGeneratorWorldCampaign::EraseAllGeneration()
{
	CancelAsyncPlacementBacktracking();

	/* Tear down spawned world actors before clearing the bookkeeping that owns their rollback history. */
	DestroyActorsFromStepTransactions();
	DestroyGeneratedAnchorActors();
	ResetGenerationRuntimeState();
	ClearAllAnchorWorldState();
	ResetCampaignDebuggerState();
}

void AGeneratorWorldCampaign::DestroyActorsFromStepTransactions()
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
}

void AGeneratorWorldCampaign::DestroyGeneratedAnchorActors()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	for (TActorIterator<AAnchorPoint> It(World); It; ++It)
	{
		AAnchorPoint* AnchorPoint = *It;
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		if (not AnchorPoint->Tags.Contains(GeneratedAnchorTag))
		{
			continue;
		}

		AnchorPoint->RemovePromotedWorldObject();
		AnchorPoint->Destroy();
	}
}

void AGeneratorWorldCampaign::ResetGenerationRuntimeState()
{
	ClearExistingConnections();
	ClearPlacementState();
	ClearDerivedData();
	M_StepTransactions.Reset();
	M_StepAttemptIndices.Reset();
	M_TotalAttemptCount = 0;
	M_EnemyMicroPlacedCount = 0;
	M_MissionMicroPlacedCount = 0;
	M_BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;
	M_GenerationStep = ECampaignGenerationStep::NotStarted;
}

void AGeneratorWorldCampaign::ClearAllAnchorWorldState()
{
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
}

void AGeneratorWorldCampaign::ResetCampaignDebuggerState()
{
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

	TArray<TWeakObjectPtr<AConnection>> ConnectionsToDraw;
	for (TActorIterator<AConnection> ConnectionIterator(World); ConnectionIterator; ++ConnectionIterator)
	{
		AConnection* Connection = *ConnectionIterator;
		if (not IsValid(Connection))
		{
			continue;
		}

		ConnectionsToDraw.Add(Connection);
	}

	if (ConnectionsToDraw.Num() == 0)
	{
		return;
	}

	const FVector HeightOffset(0.f, 0.f, M_DebugConnectionDrawHeightOffset);
	const FColor BaseConnectionColor = FColor::Cyan;
	const FColor ThreeWayConnectionColor = FColor::Yellow;

	for (const TWeakObjectPtr<AConnection>& Connection : ConnectionsToDraw)
	{
		DrawDebugConnectionForActor(World, Connection.Get(), HeightOffset, BaseConnectionColor, ThreeWayConnectionColor);
	}
}

void AGeneratorWorldCampaign::DebugDrawPlayerHQHops() const
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return;
		}

		if (not GetIsValidCampaignDebugger())
		{
			return;
		}

		if (M_DerivedData.PlayerHQHopDistancesByAnchorKey.Num() == 0)
		{
			return;
		}

		UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
			const int32* HopCount = M_DerivedData.PlayerHQHopDistancesByAnchorKey.Find(AnchorKey);
			if (HopCount == nullptr || *HopCount == INDEX_NONE)
			{
				continue;
			}

			CampaignDebugger->DebugDrawPlayerHQHopAtAnchor(AnchorPoint, *HopCount);
		}
	}
}

void AGeneratorWorldCampaign::DebugDrawEnemyHQHops() const
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		UWorld* World = GetWorld();
		if (not IsValid(World))
		{
			return;
		}

		if (not GetIsValidCampaignDebugger())
		{
			return;
		}

		if (M_DerivedData.EnemyHQHopDistancesByAnchorKey.Num() == 0)
		{
			return;
		}

		UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
		for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
		{
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
			const int32* HopCount = M_DerivedData.EnemyHQHopDistancesByAnchorKey.Find(AnchorKey);
			if (HopCount == nullptr || *HopCount == INDEX_NONE)
			{
				continue;
			}

			CampaignDebugger->DebugDrawEnemyHQHopAtAnchor(AnchorPoint, *HopCount);
		}
	}
}

void AGeneratorWorldCampaign::DebugDrawMissionPathsToPlayerHQ() const
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		if (not GetIsValidCampaignDebugger())
		{
			return;
		}

		UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
		CampaignDebugger->DebugDrawMissionPathsToPlayerHQ(*this);
	}
}

void AGeneratorWorldCampaign::DebugDrawSplineBoundaryArea()
{
	if (not M_AnchorPointGenerationSettings.bM_DebugDrawSplineBoundary)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	AWorldSplineBoundary* Boundary = nullptr;
	if (not TryGetSingleSplineBoundary(World, Boundary))
	{
		return;
	}

	Boundary->EnsureClosedLoop();

	TArray<FVector2D> BoundaryPolygon;
	Boundary->GetSampledPolygon2D(M_AnchorPointGenerationSettings.M_SplineSampleSpacing, BoundaryPolygon);
	if (BoundaryPolygon.Num() < 3)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("DebugDrawSplineBoundaryArea failed: spline boundary polygon is invalid."));
		return;
	}

	const float HeightOffset = M_AnchorPointGenerationSettings.M_DebugDrawBoundaryZOffset;
	const float Duration = M_AnchorPointGenerationSettings.M_DebugDrawBoundaryDuration;
	DrawBoundaryPolygon(World, BoundaryPolygon, HeightOffset, Duration, FColor::Green);

	if (not M_AnchorPointGenerationSettings.bM_DebugDrawGeneratedGrid)
	{
		return;
	}

	DrawAnchorGrid(World, BoundaryPolygon, M_AnchorPointGenerationSettings.M_GridCellSize, HeightOffset, Duration);
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

void AGeneratorWorldCampaign::ValidateAsyncPlacementParityForCurrentSetup()
{
	FString FailureReason;
	if (ValidateAsyncPlacementParityForCurrentSetup_Internal(FailureReason))
	{
		RTSFunctionLibrary::DisplayNotification(
			TEXT("Async placement parity validation passed for current setup."));
		return;
	}

	RTSFunctionLibrary::ReportError(FString::Printf(
		TEXT("Async placement parity validation failed: %s"),
		*FailureReason));
}

bool AGeneratorWorldCampaign::ValidateAsyncPlacementParityForSeedRange(const int32 FirstSeed,
                                                                       const int32 SeedCount,
                                                                       FString& OutFailureReason)
{
	const FScopedRTSModalDialogSuppression DialogSuppression;
	if (SeedCount <= 0)
	{
		OutFailureReason = TEXT("seed count must be greater than zero.");
		return false;
	}

	const int32 SavedSeed = M_CountAndDifficultyTuning.Seed;
	auto RestoreSeedAndErase = [this, SavedSeed]()
	{
		EraseAllGeneration();
		M_CountAndDifficultyTuning.Seed = SavedSeed;
	};

	for (int32 SeedIndex = 0; SeedIndex < SeedCount; SeedIndex++)
	{
		const int32 SeedToValidate = FirstSeed + SeedIndex;
		UE_LOG(LogTemp, Display,
		       TEXT("World campaign placement parity: validating seed %d (%d/%d)."),
		       SeedToValidate,
		       SeedIndex + 1,
		       SeedCount);
		EraseAllGeneration();
		M_CountAndDifficultyTuning.Seed = SeedToValidate;
		if (not ExecuteGameThreadSetupStepsBeforeAsyncPlacement())
		{
			OutFailureReason = FString::Printf(TEXT("setup generation failed for seed %d."), SeedToValidate);
			RestoreSeedAndErase();
			return false;
		}

		UE_LOG(LogTemp, Display,
		       TEXT("World campaign placement parity: setup ready for seed %d with %d anchors and %d connections."),
		       SeedToValidate,
		       M_PlacementState.CachedAnchors.Num(),
		       M_PlacementState.CachedConnections.Num());

		FString CurrentFailureReason;
		if (ValidateAsyncPlacementParityForCurrentSetup_Internal(CurrentFailureReason))
		{
			continue;
		}

		OutFailureReason = FString::Printf(
			TEXT("seed %d failed parity: %s"),
			SeedToValidate,
			*CurrentFailureReason);
		RestoreSeedAndErase();
		return false;
	}

	RestoreSeedAndErase();
	return true;
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

bool AGeneratorWorldCampaign::GetIsValidWorldDataComponent() const
{
	if (IsValid(M_WorldDataComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldDataComponent"),
		TEXT("AGeneratorWorldCampaign::GetIsValidWorldDataComponent"),
		this
	);
	return false;
}

bool AGeneratorWorldCampaign::GetIsValidWorldCountryOccupationRegulator() const
{
	if (IsValid(M_WorldCountryOccupationRegulator))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_WorldCountryOccupationRegulator"),
		TEXT("AGeneratorWorldCampaign::GetIsValidWorldCountryOccupationRegulator"),
		this);
	return false;
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
	if (CompletedStep == M_BacktrackedFailedStepAttemptToPreserve)
	{
		M_BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;
	}
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
	if (CompletedStep == M_BacktrackedFailedStepAttemptToPreserve)
	{
		M_BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;
	}
	return true;
}

bool AGeneratorWorldCampaign::ExecuteGenerateAnchorPoints(FCampaignGenerationStepTransaction& OutTransaction)
{
	if (not M_AnchorPointGenerationSettings.bM_EnableGeneratedAnchorPoints)
	{
		RTSFunctionLibrary::DisplayNotification(TEXT("Anchor generation disabled; skipping."));
		return true;
	}

	UWorld* World = nullptr;
	TSubclassOf<AAnchorPoint> AnchorClass;
	TArray<FVector2D> BoundaryPolygon;
	if (not TryPrepareAnchorPointGeneration(World, AnchorClass, BoundaryPolygon))
	{
		return false;
	}

	TArray<TObjectPtr<AAnchorPoint>> SortedExistingAnchors;
	GatherSortedExistingAnchors(SortedExistingAnchors);

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::AnchorPointsGenerated);
	FRandomStream RandomStream = CreateAnchorPointRandomStream(AttemptIndex);

	const int32 PreferredTargetCount = RandomStream.RandRange(M_AnchorPointGenerationSettings.M_MinNewAnchorPoints,
	                                                          M_AnchorPointGenerationSettings.M_MaxNewAnchorPoints);
	const int32 RequiredCount = M_AnchorPointGenerationSettings.M_MinNewAnchorPoints;
	if (PreferredTargetCount <= 0 && RequiredCount == 0)
	{
		return true;
	}

	const int32 JitterAttemptsPerCell = FMath::Clamp(
		M_AnchorPointGenerationSettings.M_JitterAttemptsPerCell,
		AnchorPointJitterAttemptsMin,
		AnchorPointJitterAttemptsMax);
	const float MinDistanceSquared = FMath::Square(M_AnchorPointGenerationSettings.M_MinDistanceBetweenAnchorPoints);
	FAnchorPointGridDefinition GridDefinition;
	if (not BuildAnchorPointGridDefinition(BoundaryPolygon, M_AnchorPointGenerationSettings.M_GridCellSize,
	                                       RandomStream, GridDefinition))
	{
		RTSFunctionLibrary::ReportError(TEXT("Anchor point generation failed: grid had no cells."));
		return false;
	}

	TArray<TObjectPtr<AAnchorPoint>> NewAnchors;

	if (not SpawnAnchorsInGrid(World, this, AnchorClass, RandomStream, GridDefinition, BoundaryPolygon,
	                           SortedExistingAnchors, PreferredTargetCount, AttemptIndex, JitterAttemptsPerCell,
	                           OutTransaction, NewAnchors, MinDistanceSquared))
	{
		return false;
	}

	DisplayAnchorGenerationResult(NewAnchors.Num(), PreferredTargetCount, RequiredCount);

	return true;
}

bool AGeneratorWorldCampaign::TryPrepareAnchorPointGeneration(UWorld*& OutWorld,
                                                              TSubclassOf<AAnchorPoint>& OutAnchorClass,
                                                              TArray<FVector2D>& OutBoundaryPolygon) const
{
	OutWorld = GetWorld();
	if (not IsValid(OutWorld))
	{
		return false;
	}

	FString ErrorMessage;
	if (not ValidateAnchorPointGenerationSettings(M_AnchorPointGenerationSettings, ErrorMessage))
	{
		RTSFunctionLibrary::ReportError(ErrorMessage);
		return false;
	}

	if (not TryBuildSplineBoundaryPolygon(OutWorld, M_AnchorPointGenerationSettings, OutBoundaryPolygon))
	{
		return false;
	}

	OutAnchorClass = M_AnchorPointGenerationSettings.M_GeneratedAnchorPointClass;
	if (not OutAnchorClass)
	{
		OutAnchorClass = AAnchorPoint::StaticClass();
	}

	return true;
}

void AGeneratorWorldCampaign::GatherSortedExistingAnchors(
	TArray<TObjectPtr<AAnchorPoint>>& OutSortedExistingAnchors) const
{
	TArray<TObjectPtr<AAnchorPoint>> ExistingAnchors;
	GatherAnchorPoints(ExistingAnchors);

	OutSortedExistingAnchors.Reset();
	OutSortedExistingAnchors.Reserve(ExistingAnchors.Num());
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : ExistingAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		OutSortedExistingAnchors.Add(AnchorPoint);
	}

	SortAnchorsByDeterministicOrder(OutSortedExistingAnchors);
}

FRandomStream AGeneratorWorldCampaign::CreateAnchorPointRandomStream(int32 AttemptIndex) const
{
	const int32 SeedOffset = AttemptIndex * AttemptSeedMultiplier;
	return FRandomStream(M_CountAndDifficultyTuning.Seed + SeedOffset + AnchorPointSeedOffset);
}

void AGeneratorWorldCampaign::DisplayAnchorGenerationResult(int32 NewAnchorCount, int32 PreferredTargetCount,
                                                            int32 RequiredCount) const
{
	const FString NotificationMessage = FString::Printf(
		TEXT("Anchor generation: spawned %d / preferred %d (min %d)."),
		NewAnchorCount,
		PreferredTargetCount,
		RequiredCount);
	RTSFunctionLibrary::DisplayNotification(NotificationMessage);
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

	NormalizeAnchorKeysForConnectionGeneration(OutAnchorPoints);
	SortAnchorsByDeterministicOrder(OutAnchorPoints);

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

void AGeneratorWorldCampaign::NormalizeAnchorKeysForConnectionGeneration(
	TArray<TObjectPtr<AAnchorPoint>>& InOutAnchorPoints) const
{
	const FAnchorKeyRepairSource RepairSource = BuildAnchorKeyRepairSource(InOutAnchorPoints);
	FAnchorKeyRepairPlan RepairPlan = BuildAnchorKeyRepairPlan(RepairSource);
	ApplyAnchorKeyRepairPlan(RepairPlan);
	LogAnchorKeyRepairPlan(RepairPlan);
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

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::PlayerHQPlaced);
	TArray<TObjectPtr<AAnchorPoint>> Candidates;
	bool bShouldForcePlacement = false;
	/* Strict placement is tried first so editor/manual runs still match the worker solver. */
	if (not BuildHQAnchorCandidates(CandidateSource, M_PlayerHQPlacementRules.MinAnchorDegreeForHQ,
	                                MaxAnchorDegreeUnlimited, nullptr, Candidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no valid anchor candidates found."));
		bShouldForcePlacement = true;
	}

	TArray<TObjectPtr<AAnchorPoint>> NeighborhoodCandidates;
	if (not bShouldForcePlacement)
	{
		BuildPlayerHQNeighborhoodCandidates(Candidates, NeighborhoodCandidates);
	}

	if (not bShouldForcePlacement && NeighborhoodCandidates.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no candidates met neighborhood rules."));
		bShouldForcePlacement = true;
	}

	AAnchorPoint* AnchorPoint = nullptr;
	if (not bShouldForcePlacement)
	{
		AnchorPoint = SelectAnchorCandidateByAttempt(NeighborhoodCandidates, AttemptIndex);
	}

	if (not bShouldForcePlacement && not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: selected anchor was invalid."));
		bShouldForcePlacement = true;
	}

	if (bShouldForcePlacement)
	{
		AnchorPoint = SelectForcedPlayerHQAnchor(CandidateSource, AttemptIndex);
	}

	return TryFinalizePlayerHQPlacement(AnchorPoint, OutTransaction);
}

void AGeneratorWorldCampaign::BuildPlayerHQNeighborhoodCandidates(
	const TArray<TObjectPtr<AAnchorPoint>>& Candidates,
	TArray<TObjectPtr<AAnchorPoint>>& OutNeighborhoodCandidates) const
{
	OutNeighborhoodCandidates.Reset();
	OutNeighborhoodCandidates.Reserve(Candidates.Num());
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
			if (Pair.Value <= 0 || Pair.Value > M_PlayerHQPlacementRules.MinAnchorsWithinHopsRange)
			{
				continue;
			}

			NearbyAnchorCount++;
		}

		if (NearbyAnchorCount >= M_PlayerHQPlacementRules.MinAnchorsWithinHops)
		{
			OutNeighborhoodCandidates.Add(CandidateAnchor);
		}
	}
}

AAnchorPoint* AGeneratorWorldCampaign::SelectForcedPlayerHQAnchor(
	const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
	const int32 AttemptIndex) const
{
	constexpr int32 MinAnchorDegreeFloor = 0;
	constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
	TArray<TObjectPtr<AAnchorPoint>> ForcedCandidates;
	if (not BuildHQAnchorCandidates(CandidateSource, MinAnchorDegreeFloor, MaxAnchorDegreeUnlimited, nullptr,
	                                ForcedCandidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: forced placement had no anchors."));
		return nullptr;
	}

	const int32 SeedOffset = PlayerHQForcePlacementSeedOffset + (AttemptIndex * AttemptSeedMultiplier);
	FRandomStream RandomStream(M_PlacementState.SeedUsed + SeedOffset);
	const int32 ForcedIndex = RandomStream.RandRange(0, ForcedCandidates.Num() - 1);
	return ForcedCandidates[ForcedIndex].Get();
}

bool AGeneratorWorldCampaign::TryFinalizePlayerHQPlacement(AAnchorPoint* AnchorPoint,
                                                           FCampaignGenerationStepTransaction& OutTransaction)
{
	if (not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: forced selection was invalid."));
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

	if (GetShouldDeferWorldObjectPromotion())
	{
		return true;
	}

	AWorldMapObject* SpawnedObject = AnchorPoint->OnPlayerItemPromotion(EMapPlayerItem::PlayerHQ,
	                                                                    ECampaignGenerationStep::PlayerHQPlaced);
	if (not IsValid(SpawnedObject))
	{
		return true;
	}

	OutTransaction.SpawnedActors.Add(SpawnedObject);

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
	constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
	const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource = M_EnemyHQPlacementRules.AnchorCandidates.Num() > 0
		                                                          ? M_EnemyHQPlacementRules.AnchorCandidates
		                                                          : M_PlacementState.CachedAnchors;

	TArray<TObjectPtr<AAnchorPoint>> Candidates;
	bool bShouldForcePlacement = false;
	const int32 MinAnchorDegree = FMath::Max(MinAnchorDegreeFloor, M_EnemyHQPlacementRules.MinAnchorDegree);
	const int32 MaxAnchorDegree = FMath::Max(MinAnchorDegree, M_EnemyHQPlacementRules.MaxAnchorDegree);
	if (not BuildHQAnchorCandidates(CandidateSource, MinAnchorDegree, MaxAnchorDegree,
	                                M_PlacementState.PlayerHQAnchor.Get(), Candidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: no valid anchor candidates found."));
		bShouldForcePlacement = true;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::EnemyHQPlaced);
	if (not bShouldForcePlacement
		&& M_EnemyHQPlacementRules.AnchorDegreePreference != ETopologySearchStrategy::NotSet)
	{
		SortEnemyHQCandidatesByPreference(Candidates);
	}

	AAnchorPoint* AnchorPoint = nullptr;
	if (not bShouldForcePlacement)
	{
		AnchorPoint = SelectAnchorCandidateByAttempt(Candidates, AttemptIndex);
	}

	if (not bShouldForcePlacement && not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: selected anchor was invalid."));
		bShouldForcePlacement = true;
	}

	if (bShouldForcePlacement)
	{
		AnchorPoint = SelectForcedEnemyHQAnchor(CandidateSource, AttemptIndex);
	}

	return TryFinalizeEnemyHQPlacement(AnchorPoint, OutTransaction);
}

void AGeneratorWorldCampaign::SortEnemyHQCandidatesByPreference(
	TArray<TObjectPtr<AAnchorPoint>>& InOutCandidates) const
{
	const bool bPreferMax = M_EnemyHQPlacementRules.AnchorDegreePreference == ETopologySearchStrategy::PreferMax;

	Algo::Sort(InOutCandidates, [this, bPreferMax](const TObjectPtr<AAnchorPoint>& Left,
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

		return AAnchorPoint::IsDeterministicAnchorOrderLess(Left.Get(), Right.Get());
	});
}

AAnchorPoint* AGeneratorWorldCampaign::SelectForcedEnemyHQAnchor(
	const TArray<TObjectPtr<AAnchorPoint>>& CandidateSource,
	const int32 AttemptIndex) const
{
	constexpr int32 MinAnchorDegreeFloor = 0;
	constexpr int32 MaxAnchorDegreeUnlimited = TNumericLimits<int32>::Max();
	TArray<TObjectPtr<AAnchorPoint>> ForcedCandidates;
	if (not BuildHQAnchorCandidates(CandidateSource, MinAnchorDegreeFloor, MaxAnchorDegreeUnlimited,
	                                M_PlacementState.PlayerHQAnchor.Get(), ForcedCandidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: forced placement had no anchors."));
		return nullptr;
	}

	const int32 SeedOffset = EnemyHQForcePlacementSeedOffset + (AttemptIndex * AttemptSeedMultiplier);
	FRandomStream RandomStream(M_PlacementState.SeedUsed + SeedOffset);
	const int32 ForcedIndex = RandomStream.RandRange(0, ForcedCandidates.Num() - 1);
	return ForcedCandidates[ForcedIndex].Get();
}

bool AGeneratorWorldCampaign::TryFinalizeEnemyHQPlacement(AAnchorPoint* AnchorPoint,
                                                          FCampaignGenerationStepTransaction& OutTransaction)
{
	if (not IsValid(AnchorPoint))
	{
		RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: forced selection was invalid."));
		return false;
	}

	M_PlacementState.EnemyHQAnchor = AnchorPoint;
	M_PlacementState.EnemyHQAnchorKey = AnchorPoint->GetAnchorKey();
	CampaignGenerationHelper::BuildHopDistanceCache(AnchorPoint, M_DerivedData.EnemyHQHopDistancesByAnchorKey);

	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Compile_DebugSymbols)
	{
		DebugNotifyAnchorPicked(AnchorPoint, TEXT("Enemy HQ"), FColor::Red);
	}

	if (GetShouldDeferWorldObjectPromotion())
	{
		return true;
	}

	AWorldMapObject* SpawnedObject = AnchorPoint->OnEnemyItemPromotion(EMapEnemyItem::EnemyHQ,
	                                                                   ECampaignGenerationStep::EnemyHQPlaced);
	if (not IsValid(SpawnedObject))
	{
		return true;
	}

	OutTransaction.SpawnedActors.Add(SpawnedObject);

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

	if (GetShouldDeferWorldObjectPromotion())
	{
		return true;
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

	if (GetShouldDeferWorldObjectPromotion())
	{
		return true;
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

	if (GetShouldDeferWorldObjectPromotion())
	{
		return true;
	}

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

	if (not ValidateNeutralObjectPlacementPrerequisites())
	{
		return false;
	}

	FWorldCampaignPlacementState WorkingPlacementState = M_PlacementState;
	FWorldCampaignDerivedData WorkingDerivedData = M_DerivedData;
	TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>> Promotions;
	if (not BuildNeutralPlacementPromotions(WorkingPlacementState, WorkingDerivedData, Promotions))
	{
		return false;
	}

	M_PlacementState = WorkingPlacementState;
	M_DerivedData = WorkingDerivedData;
	if (GetShouldDeferWorldObjectPromotion())
	{
		return true;
	}

	ApplyNeutralPromotionsToWorld(Promotions, OutTransaction);
	return true;
}

bool AGeneratorWorldCampaign::ValidateNeutralObjectPlacementPrerequisites() const
{
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

	return true;
}

bool AGeneratorWorldCampaign::BuildNeutralPlacementPromotions(
	FWorldCampaignPlacementState& WorkingPlacementState,
	FWorldCampaignDerivedData& WorkingDerivedData,
	TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& OutPromotions)
{
	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::NeutralObjectsPlaced);
	const FRuleRelaxationState RelaxationState = GetRelaxationState(
		GetFailurePolicyForStep(ECampaignGenerationStep::NeutralObjectsPlaced),
		AttemptIndex);
	const TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(WorkingPlacementState.CachedAnchors);
	const TMap<EMapNeutralObjectType, int32> RequiredNeutralCounts = BuildRequiredNeutralItemCounts(
		M_CountAndDifficultyTuning);
	const TArray<EMapNeutralObjectType> NeutralTypes = GetSortedNeutralTypes(RequiredNeutralCounts);
	OutPromotions.Reset();
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
		                                    AnchorLookup, OutPromotions, *this))
		{
			return false;
		}
	}

	return true;
}

void AGeneratorWorldCampaign::ApplyNeutralPromotionsToWorld(
	const TArray<TPair<TObjectPtr<AAnchorPoint>, EMapNeutralObjectType>>& Promotions,
	FCampaignGenerationStepTransaction& OutTransaction)
{
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


bool AGeneratorWorldCampaign::GetShouldDeferWorldObjectPromotion() const
{
	return bM_DeferWorldObjectPromotionDuringBacktracking;
}

bool AGeneratorWorldCampaign::RealizeVirtualPlacementState()
{
	const TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(M_PlacementState.CachedAnchors);
	return RealizeVirtualPlayerHQ(AnchorLookup)
		&& RealizeVirtualEnemyPlacements(AnchorLookup)
		&& RealizeVirtualNeutralPlacements(AnchorLookup)
		&& RealizeVirtualMissionPlacements(AnchorLookup);
}

bool AGeneratorWorldCampaign::RealizeVirtualPlayerHQ(
	const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
{
	if (M_PlacementState.PlayerHQAnchorKey.IsValid())
	{
		AAnchorPoint* PlayerHQAnchor = nullptr;
		if (not TryGetAnchorForVirtualRealization(
			AnchorLookup,
			M_PlacementState.PlayerHQAnchorKey,
			TEXT("Virtual layout realization failed: player HQ anchor is invalid."),
			PlayerHQAnchor))
		{
			return false;
		}

		AWorldMapObject* PromotedObject = PlayerHQAnchor->OnPlayerItemPromotion(
			EMapPlayerItem::PlayerHQ,
			ECampaignGenerationStep::Finished);
		if (not IsValid(PromotedObject))
		{
			return false;
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::RealizeVirtualEnemyPlacements(
	const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
{
	if (M_PlacementState.EnemyHQAnchorKey.IsValid())
	{
		const TPair<FGuid, EMapEnemyItem> EnemyHQPlacement(
			M_PlacementState.EnemyHQAnchorKey,
			EMapEnemyItem::EnemyHQ);
		if (not TryRealizeEnemyPlacement(AnchorLookup, EnemyHQPlacement))
		{
			return false;
		}
	}

	for (const TPair<FGuid, EMapEnemyItem>& EnemyPlacement : M_PlacementState.EnemyItemsByAnchorKey)
	{
		if (not TryRealizeEnemyPlacement(AnchorLookup, EnemyPlacement))
		{
			return false;
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::RealizeVirtualNeutralPlacements(
	const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
{
	for (const TPair<FGuid, EMapNeutralObjectType>& NeutralPlacement : M_PlacementState.NeutralItemsByAnchorKey)
	{
		if (not TryRealizeNeutralPlacement(AnchorLookup, NeutralPlacement))
		{
			return false;
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::RealizeVirtualMissionPlacements(
	const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
{
	for (const TPair<FGuid, EMapMission>& MissionPlacement : M_PlacementState.MissionsByAnchorKey)
	{
		if (not TryRealizeMissionPlacement(AnchorLookup, MissionPlacement))
		{
			return false;
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::BuildPlacementSnapshot(FWorldCampaignPlacementSnapshot& OutSnapshot) const
{
	if (M_PlacementState.CachedAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Async placement snapshot failed: no cached anchors."));
		return false;
	}

	OutSnapshot = FWorldCampaignPlacementSnapshot();
	CopyPlacementSnapshotSettings(OutSnapshot);
	BuildMissionRuleSnapshots(OutSnapshot);
	AppendPlacementSnapshotCandidateKeys(OutSnapshot);

	TMap<FGuid, int32> AnchorIndexByKey;
	TMap<const AAnchorPoint*, int32> AnchorIndexByActor;
	BuildAnchorSnapshots(OutSnapshot, AnchorIndexByKey, AnchorIndexByActor);
	AppendPlacementSnapshotCandidateIndices(OutSnapshot, AnchorIndexByActor);
	BuildConnectionSnapshots(OutSnapshot);
	MarkAnchorSnapshotCandidateMembership(OutSnapshot, AnchorIndexByKey);
	return OutSnapshot.Anchors.Num() > 0;
}

void AGeneratorWorldCampaign::CopyPlacementSnapshotSettings(FWorldCampaignPlacementSnapshot& OutSnapshot) const
{
	OutSnapshot.SeedUsed = M_PlacementState.SeedUsed;
	OutSnapshot.InitialTotalAttemptCount = M_TotalAttemptCount;
	OutSnapshot.InitialStepAttemptIndices = M_StepAttemptIndices;
	OutSnapshot.InitialDerivedData = M_DerivedData;
	OutSnapshot.CountAndDifficultyTuning = M_CountAndDifficultyTuning;
	OutSnapshot.EnemyPlacementRules = M_EnemyPlacementRules;
	OutSnapshot.NeutralItemPlacementRules = M_NeutralItemPlacementRules;
	OutSnapshot.ExcludedMissionsFromMapPlacement = M_ExcludedMissionsFromMapPlacement;
	OutSnapshot.PlacementFailurePolicy = M_PlacementFailurePolicy;

	OutSnapshot.PlayerHQPlacementRules.MinAnchorDegreeForHQ = M_PlayerHQPlacementRules.MinAnchorDegreeForHQ;
	OutSnapshot.PlayerHQPlacementRules.MinAnchorsWithinHops = M_PlayerHQPlacementRules.MinAnchorsWithinHops;
	OutSnapshot.PlayerHQPlacementRules.MinAnchorsWithinHopsRange =
		M_PlayerHQPlacementRules.MinAnchorsWithinHopsRange;
	OutSnapshot.PlayerHQPlacementRules.SafeZoneMaxHops = M_PlayerHQPlacementRules.SafeZoneMaxHops;
	OutSnapshot.EnemyHQPlacementRules.MinAnchorDegree = M_EnemyHQPlacementRules.MinAnchorDegree;
	OutSnapshot.EnemyHQPlacementRules.MaxAnchorDegree = M_EnemyHQPlacementRules.MaxAnchorDegree;
	OutSnapshot.EnemyHQPlacementRules.AnchorDegreePreference = M_EnemyHQPlacementRules.AnchorDegreePreference;
	OutSnapshot.EnemyWallPlacementRules.Preference = M_EnemyWallPlacementRules.Preference;
}

void AGeneratorWorldCampaign::BuildMissionRuleSnapshots(FWorldCampaignPlacementSnapshot& OutSnapshot) const
{
	OutSnapshot.MissionPlacementRules.M_HopsPreferenceStrength = M_MissionPlacementRules.M_HopsPreferenceStrength;
	OutSnapshot.MissionPlacementRules.RulesByTier = M_MissionPlacementRules.RulesByTier;
	for (const TPair<EMapMission, FPerMissionRules>& MissionRulePair : M_MissionPlacementRules.RulesByMission)
	{
		FWorldCampaignPerMissionRulesSnapshot MissionRuleSnapshot;
		BuildMissionRuleSnapshot(MissionRulePair.Value, MissionRuleSnapshot);
		OutSnapshot.MissionPlacementRules.RulesByMission.Add(MissionRulePair.Key, MissionRuleSnapshot);
	}
}

void AGeneratorWorldCampaign::AppendPlacementSnapshotCandidateKeys(
	FWorldCampaignPlacementSnapshot& OutSnapshot) const
{
	AppendAnchorCandidateKeys(M_PlayerHQPlacementRules.AnchorCandidates,
	                          OutSnapshot.PlayerHQPlacementRules.AnchorCandidateKeys);
	AppendAnchorCandidateKeys(M_EnemyHQPlacementRules.AnchorCandidates,
	                          OutSnapshot.EnemyHQPlacementRules.AnchorCandidateKeys);
	AppendAnchorCandidateKeys(M_EnemyWallPlacementRules.AnchorCandidates,
	                          OutSnapshot.EnemyWallPlacementRules.AnchorCandidateKeys);
}

void AGeneratorWorldCampaign::AppendPlacementSnapshotCandidateIndices(
	FWorldCampaignPlacementSnapshot& OutSnapshot,
	const TMap<const AAnchorPoint*, int32>& AnchorIndexByActor) const
{
	AppendAnchorCandidateIndices(M_PlayerHQPlacementRules.AnchorCandidates,
	                             AnchorIndexByActor,
	                             OutSnapshot.PlayerHQPlacementRules.AnchorCandidateIndices);
	AppendAnchorCandidateIndices(M_EnemyHQPlacementRules.AnchorCandidates,
	                             AnchorIndexByActor,
	                             OutSnapshot.EnemyHQPlacementRules.AnchorCandidateIndices);
	AppendAnchorCandidateIndices(M_EnemyWallPlacementRules.AnchorCandidates,
	                             AnchorIndexByActor,
	                             OutSnapshot.EnemyWallPlacementRules.AnchorCandidateIndices);
	for (const TPair<EMapMission, FPerMissionRules>& MissionRulePair : M_MissionPlacementRules.RulesByMission)
	{
		FWorldCampaignPerMissionRulesSnapshot* MissionRuleSnapshot =
			OutSnapshot.MissionPlacementRules.RulesByMission.Find(MissionRulePair.Key);
		if (MissionRuleSnapshot == nullptr)
		{
			continue;
		}

		AppendAnchorCandidateIndices(
			MissionRulePair.Value.OverridePlacementAnchorCandidates,
			AnchorIndexByActor,
			MissionRuleSnapshot->OverridePlacementAnchorCandidateIndices);
	}
}

void AGeneratorWorldCampaign::BuildMissionRuleSnapshot(const FPerMissionRules& MissionRules,
                                                       FWorldCampaignPerMissionRulesSnapshot& OutMissionRules) const
{
	OutMissionRules.Tier = MissionRules.Tier;
	OutMissionRules.bOverrideTierRules = MissionRules.bOverrideTierRules;
	OutMissionRules.bOverridePlacementWithArray = MissionRules.bOverridePlacementWithArray;
	OutMissionRules.OverrideMinConnections = MissionRules.OverrideMinConnections;
	OutMissionRules.OverrideMaxConnections = MissionRules.OverrideMaxConnections;
	OutMissionRules.OverrideConnectionPreference = MissionRules.OverrideConnectionPreference;
	OutMissionRules.OverrideMinHopsFromPlayerHQ = MissionRules.OverrideMinHopsFromPlayerHQ;
	OutMissionRules.OverrideMaxHopsFromPlayerHQ = MissionRules.OverrideMaxHopsFromPlayerHQ;
	OutMissionRules.OverrideHopsPreference = MissionRules.OverrideHopsPreference;
	OutMissionRules.OverrideRules = MissionRules.OverrideRules;
	AppendAnchorCandidateKeys(MissionRules.OverridePlacementAnchorCandidates,
	                          OutMissionRules.OverridePlacementAnchorCandidateKeys);
}

void AGeneratorWorldCampaign::AppendAnchorCandidateKeys(const TArray<TObjectPtr<AAnchorPoint>>& AnchorCandidates,
                                                        TArray<FGuid>& OutAnchorCandidateKeys) const
{
	OutAnchorCandidateKeys.Reset();
	OutAnchorCandidateKeys.Reserve(AnchorCandidates.Num());
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorCandidates)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
		if (AnchorKey.IsValid())
		{
			OutAnchorCandidateKeys.Add(AnchorKey);
		}
	}
}

void AGeneratorWorldCampaign::AppendAnchorCandidateIndices(
	const TArray<TObjectPtr<AAnchorPoint>>& AnchorCandidates,
	const TMap<const AAnchorPoint*, int32>& AnchorIndexByActor,
	TArray<int32>& OutAnchorCandidateIndices) const
{
	OutAnchorCandidateIndices.Reset();
	OutAnchorCandidateIndices.Reserve(AnchorCandidates.Num());
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : AnchorCandidates)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const int32* AnchorIndex = AnchorIndexByActor.Find(AnchorPoint.Get());
		if (AnchorIndex != nullptr)
		{
			OutAnchorCandidateIndices.Add(*AnchorIndex);
		}
	}
}

void AGeneratorWorldCampaign::BuildAnchorSnapshots(FWorldCampaignPlacementSnapshot& OutSnapshot,
                                                   TMap<FGuid, int32>& OutAnchorIndexByKey,
                                                   TMap<const AAnchorPoint*, int32>& OutAnchorIndexByActor) const
{
	TArray<TObjectPtr<AAnchorPoint>> SnapshotAnchorsByIndex;
	BuildAnchorSnapshotRecords(OutSnapshot, OutAnchorIndexByKey, OutAnchorIndexByActor, SnapshotAnchorsByIndex);
	BuildAnchorSnapshotNeighbors(OutSnapshot, SnapshotAnchorsByIndex, OutAnchorIndexByActor);

	/*
	 * Keep AnchorIndicesInCachedOrder in CachedAnchors order. Chokepoint sampling shuffles this order
	 * before path selection, so sorting here makes async placement diverge from editor step-by-step placement.
	 */
}

void AGeneratorWorldCampaign::BuildAnchorSnapshotRecords(
	FWorldCampaignPlacementSnapshot& OutSnapshot,
	TMap<FGuid, int32>& OutAnchorIndexByKey,
	TMap<const AAnchorPoint*, int32>& OutAnchorIndexByActor,
	TArray<TObjectPtr<AAnchorPoint>>& OutSnapshotAnchorsByIndex) const
{
	OutSnapshot.Anchors.Reset();
	OutSnapshot.AnchorIndicesInCachedOrder.Reset();
	OutAnchorIndexByKey.Reset();
	OutAnchorIndexByActor.Reset();
	OutSnapshotAnchorsByIndex.Reset();
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
		if (not AnchorKey.IsValid())
		{
			continue;
		}

		const int32* ExistingAnchorIndex = OutAnchorIndexByActor.Find(AnchorPoint.Get());
		if (ExistingAnchorIndex != nullptr)
		{
			OutSnapshot.AnchorIndicesInCachedOrder.Add(*ExistingAnchorIndex);
			OutAnchorIndexByKey.Add(AnchorKey, *ExistingAnchorIndex);
			continue;
		}

		FWorldCampaignAnchorSnapshot AnchorSnapshot;
		AnchorSnapshot.AnchorKey = AnchorKey;
		AnchorSnapshot.Location = AnchorPoint->GetActorLocation();
		AnchorSnapshot.ConnectionDegree = AnchorPoint->GetConnectionCount();

		const int32 NewAnchorIndex = OutSnapshot.Anchors.Num();
		OutAnchorIndexByActor.Add(AnchorPoint.Get(), NewAnchorIndex);
		OutAnchorIndexByKey.Add(AnchorKey, NewAnchorIndex);
		OutSnapshot.AnchorIndicesInCachedOrder.Add(NewAnchorIndex);
		OutSnapshot.Anchors.Add(AnchorSnapshot);
		OutSnapshotAnchorsByIndex.Add(AnchorPoint);
	}
}

void AGeneratorWorldCampaign::BuildAnchorSnapshotNeighbors(
	FWorldCampaignPlacementSnapshot& OutSnapshot,
	const TArray<TObjectPtr<AAnchorPoint>>& SnapshotAnchorsByIndex,
	const TMap<const AAnchorPoint*, int32>& AnchorIndexByActor) const
{
	for (int32 AnchorIndex = 0; AnchorIndex < SnapshotAnchorsByIndex.Num(); AnchorIndex++)
	{
		const TObjectPtr<AAnchorPoint>& AnchorPoint = SnapshotAnchorsByIndex[AnchorIndex];
		if (not IsValid(AnchorPoint) || not OutSnapshot.Anchors.IsValidIndex(AnchorIndex))
		{
			continue;
		}

		FWorldCampaignAnchorSnapshot& AnchorSnapshot = OutSnapshot.Anchors[AnchorIndex];
		for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : AnchorPoint->GetNeighborAnchors())
		{
			if (not IsValid(NeighborAnchor) || not NeighborAnchor->GetAnchorKey().IsValid())
			{
				continue;
			}

			AnchorSnapshot.NeighborAnchorKeys.Add(NeighborAnchor->GetAnchorKey());
			const int32* NeighborAnchorIndex = AnchorIndexByActor.Find(NeighborAnchor.Get());
			if (NeighborAnchorIndex != nullptr)
			{
				AnchorSnapshot.NeighborAnchorIndices.Add(*NeighborAnchorIndex);
			}
		}
	}
}

void AGeneratorWorldCampaign::BuildConnectionSnapshots(FWorldCampaignPlacementSnapshot& OutSnapshot) const
{
	OutSnapshot.Connections.Reset();
	for (const TObjectPtr<AConnection>& Connection : M_PlacementState.CachedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		FWorldCampaignConnectionSnapshot ConnectionSnapshot;
		ConnectionSnapshot.bIsThreeWayConnection = Connection->GetIsThreeWayConnection();
		ConnectionSnapshot.JunctionLocation = Connection->GetJunctionLocation();
		for (const TObjectPtr<AAnchorPoint>& ConnectedAnchor : Connection->GetConnectedAnchors())
		{
			if (IsValid(ConnectedAnchor) && ConnectedAnchor->GetAnchorKey().IsValid())
			{
				ConnectionSnapshot.ConnectedAnchorKeys.Add(ConnectedAnchor->GetAnchorKey());
			}
		}

		OutSnapshot.Connections.Add(ConnectionSnapshot);
	}
}

void AGeneratorWorldCampaign::MarkAnchorSnapshotCandidateMembership(
	FWorldCampaignPlacementSnapshot& OutSnapshot,
	const TMap<FGuid, int32>& AnchorIndexByKey) const
{
	for (FWorldCampaignAnchorSnapshot& AnchorSnapshot : OutSnapshot.Anchors)
	{
		AnchorSnapshot.bIsPlayerHQCandidate =
			OutSnapshot.PlayerHQPlacementRules.AnchorCandidateKeys.Num() == 0
			|| OutSnapshot.PlayerHQPlacementRules.AnchorCandidateKeys.Contains(AnchorSnapshot.AnchorKey);
		AnchorSnapshot.bIsEnemyHQCandidate =
			OutSnapshot.EnemyHQPlacementRules.AnchorCandidateKeys.Num() == 0
			|| OutSnapshot.EnemyHQPlacementRules.AnchorCandidateKeys.Contains(AnchorSnapshot.AnchorKey);
		AnchorSnapshot.bIsEnemyWallCandidate =
			OutSnapshot.EnemyWallPlacementRules.AnchorCandidateKeys.Contains(AnchorSnapshot.AnchorKey);
	}

	for (const TPair<EMapMission, FWorldCampaignPerMissionRulesSnapshot>& MissionRulePair
	     : OutSnapshot.MissionPlacementRules.RulesByMission)
	{
		for (const FGuid& AnchorKey : MissionRulePair.Value.OverridePlacementAnchorCandidateKeys)
		{
			const int32* AnchorIndex = AnchorIndexByKey.Find(AnchorKey);
			if (AnchorIndex != nullptr && OutSnapshot.Anchors.IsValidIndex(*AnchorIndex))
			{
				OutSnapshot.Anchors[*AnchorIndex].MissionOverrideCandidateMembership.Add(MissionRulePair.Key);
			}
		}
	}
}

void AGeneratorWorldCampaign::StartAsyncPlacementBacktracking()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError(TEXT("Async placement failed: world was invalid for completion polling."));
		return;
	}

	FWorldCampaignPlacementSnapshot Snapshot;
	if (not BuildPlacementSnapshot(Snapshot))
	{
		return;
	}

	const int32 GenerationRequestId = ++M_AsyncPlacementGenerationRequestId;
	TSharedRef<FWorldCampaignPlacementSnapshot, ESPMode::ThreadSafe> SnapshotRef =
		MakeShared<FWorldCampaignPlacementSnapshot, ESPMode::ThreadSafe>();
	*SnapshotRef = MoveTemp(Snapshot);
	const int32 RequiredEnemyItems = GetRequiredEnemyItemCount(SnapshotRef->CountAndDifficultyTuning);
	const int32 RequiredNeutralItems = GetRequiredNeutralItemCount(SnapshotRef->CountAndDifficultyTuning);
	const int32 RequiredMissionItems = BuildMissionPlacementPlanFiltered().Num();
	UE_LOG(LogRTS, Display,
	       TEXT(
		       "Async world campaign placement snapshot: Seed=%d, Anchors=%d, Connections=%d, RequiredEnemy=%d, RequiredNeutral=%d, RequiredMissions=%d, NeutralHQHops=[%d,%d], NeutralSpacingHops=[%d,%d], NeutralPreference=%d."
	       ),
	       SnapshotRef->SeedUsed,
	       SnapshotRef->Anchors.Num(),
	       SnapshotRef->Connections.Num(),
	       RequiredEnemyItems,
	       RequiredNeutralItems,
	       RequiredMissionItems,
	       SnapshotRef->NeutralItemPlacementRules.MinHopsFromHQ,
	       SnapshotRef->NeutralItemPlacementRules.MaxHopsFromHQ,
	       SnapshotRef->NeutralItemPlacementRules.MinHopsFromOtherNeutralItems,
	       SnapshotRef->NeutralItemPlacementRules.MaxHopsFromOtherNeutralItems,
	       static_cast<int32>(SnapshotRef->NeutralItemPlacementRules.Preference));
	TSharedRef<FWorldCampaignAsyncPlacementProgress, ESPMode::ThreadSafe> ProgressRef =
		MakeShared<FWorldCampaignAsyncPlacementProgress, ESPMode::ThreadSafe>();
	{
		FScopeLock ProgressLock(&ProgressRef->CriticalSection);
		ProgressRef->Phase = TEXT("Queued async placement");
		ProgressRef->CurrentStep = ECampaignGenerationStep::ConnectionsCreated;
		ProgressRef->TotalAttemptCount = SnapshotRef->InitialTotalAttemptCount;
	}

	M_AsyncPlacementFutureRequestId = GenerationRequestId;
	M_AsyncPlacementProgress = ProgressRef;
	M_AsyncPlacementStartTimeSeconds = FPlatformTime::Seconds();
	M_AsyncPlacementLastProgressLogTimeSeconds = M_AsyncPlacementStartTimeSeconds;
	M_AsyncPlacementFuture = Async(EAsyncExecution::ThreadPool, [SnapshotRef, ProgressRef]()
	{
		return WorldCampaignAsyncPlacement::SolvePlacement(*SnapshotRef, ProgressRef);
	});

	World->GetTimerManager().SetTimer(
		M_AsyncPlacementPollTimerHandle,
		this,
		&AGeneratorWorldCampaign::PollAsyncPlacementBacktracking,
		0.05f,
		true);
}

void AGeneratorWorldCampaign::CancelAsyncPlacementBacktracking()
{
	M_AsyncPlacementGenerationRequestId++;
	M_AsyncPlacementFutureRequestId = 0;
	M_AsyncPlacementProgress.Reset();
	M_AsyncPlacementStartTimeSeconds = 0.0;
	M_AsyncPlacementLastProgressLogTimeSeconds = 0.0;
	if (UWorld* ExistingWorld = GetWorld())
	{
		ExistingWorld->GetTimerManager().ClearTimer(M_AsyncPlacementPollTimerHandle);
	}
}

void AGeneratorWorldCampaign::PollAsyncPlacementBacktracking()
{
	if (M_AsyncPlacementFutureRequestId != M_AsyncPlacementGenerationRequestId)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_AsyncPlacementPollTimerHandle);
		}

		return;
	}

	if (not M_AsyncPlacementFuture.IsValid() || not M_AsyncPlacementFuture.IsReady())
	{
		LogAsyncPlacementProgressIfNeeded();
		return;
	}

	FWorldCampaignPlacementResult Result = M_AsyncPlacementFuture.Get();
	M_AsyncPlacementProgress.Reset();
	M_AsyncPlacementStartTimeSeconds = 0.0;
	M_AsyncPlacementLastProgressLogTimeSeconds = 0.0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_AsyncPlacementPollTimerHandle);
	}

	HandleAsyncPlacementCompleted(MoveTemp(Result), M_AsyncPlacementFutureRequestId);
}

void AGeneratorWorldCampaign::LogAsyncPlacementProgressIfNeeded()
{
	constexpr double ProgressLogIntervalSeconds = 5.0;
	const double NowSeconds = FPlatformTime::Seconds();
	if (NowSeconds - M_AsyncPlacementLastProgressLogTimeSeconds < ProgressLogIntervalSeconds)
	{
		return;
	}

	M_AsyncPlacementLastProgressLogTimeSeconds = NowSeconds;
	if (not M_AsyncPlacementProgress.IsValid())
	{
		return;
	}

	FAsyncPlacementProgressLogState ProgressState;
	CopyAsyncPlacementProgressLogState(*M_AsyncPlacementProgress, ProgressState);
	const double ElapsedSeconds = M_AsyncPlacementStartTimeSeconds > 0.0
		                              ? NowSeconds - M_AsyncPlacementStartTimeSeconds
		                              : 0.0;
	LogAsyncPlacementProgressState(ProgressState, ElapsedSeconds);
}

void AGeneratorWorldCampaign::HandleAsyncPlacementCompleted(FWorldCampaignPlacementResult Result,
                                                            const int32 GenerationRequestId)
{
	if (GenerationRequestId != M_AsyncPlacementGenerationRequestId)
	{
		return;
	}

	ReportPlacementDebugEvents(Result);
	M_TotalAttemptCount += Result.WorkerTotalAttemptCount;
	M_StepAttemptIndices = Result.StepAttemptIndicesAtEnd;
	if (M_TotalAttemptCount > MaxTotalAttempts && not Result.bSucceeded)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Generation failed: maximum total attempts exceeded during async placement."));
		return;
	}

	if (Result.bRequiresGameThreadBacktrack)
	{
		if (ApplyAsyncPlacementGameThreadBacktrack(Result))
		{
			StartAsyncPlacementBacktracking();
		}

		return;
	}

	if (not Result.bSucceeded)
	{
		RTSFunctionLibrary::ReportError(Result.FailureReason);
		return;
	}

	if (not ApplyPlacementResultToWorld(Result))
	{
		RTSFunctionLibrary::ReportError(TEXT("Async placement failed while applying result to the world."));
		return;
	}

	if (M_WorldPlayerController.IsValid())
	{
		M_WorldPlayerController->UpdateAsyncWorldGenerationWidget_AsyncGenerationComplete();
	}

	M_GenerationStep = ECampaignGenerationStep::Finished;
	M_OnGenerationFinished.Broadcast();
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_PrintPlacementReport(*this);
		}
	}
}

bool AGeneratorWorldCampaign::ApplyAsyncPlacementGameThreadBacktrack(const FWorldCampaignPlacementResult& Result)
{
	const int32 UndoCount = FMath::Min(Result.GameThreadTransactionsToUndo, M_StepTransactions.Num());
	if (UndoCount <= 0)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("Async placement requested setup backtracking, but no setup transactions were available."));
		return false;
	}

	for (int32 UndoIndex = 0; UndoIndex < UndoCount; ++UndoIndex)
	{
		UndoLastTransaction();
	}

	return ExecuteGameThreadSetupStepsBeforeAsyncPlacement();
}

bool AGeneratorWorldCampaign::ApplyPlacementResultToWorld(const FWorldCampaignPlacementResult& Result)
{
	const TMap<FGuid, TObjectPtr<AAnchorPoint>> AnchorLookup = BuildAnchorLookup(M_PlacementState.CachedAnchors);
	AAnchorPoint* PlayerHQAnchor = FindAnchorByKey(AnchorLookup, Result.PlayerHQAnchorKey);
	AAnchorPoint* EnemyHQAnchor = FindAnchorByKey(AnchorLookup, Result.EnemyHQAnchorKey);
	if (not IsValid(PlayerHQAnchor) || not IsValid(EnemyHQAnchor))
	{
		return false;
	}

	bM_DeferWorldObjectPromotionDuringBacktracking = false;
	M_PlacementState.PlayerHQAnchor = PlayerHQAnchor;
	M_PlacementState.PlayerHQAnchorKey = Result.PlayerHQAnchorKey;
	M_PlacementState.EnemyHQAnchor = EnemyHQAnchor;
	M_PlacementState.EnemyHQAnchorKey = Result.EnemyHQAnchorKey;
	M_PlacementState.EnemyItemsByAnchorKey = Result.EnemyItemsByAnchorKey;
	M_PlacementState.NeutralItemsByAnchorKey = Result.NeutralItemsByAnchorKey;
	M_PlacementState.MissionsByAnchorKey = Result.MissionsByAnchorKey;
	M_DerivedData = Result.DerivedData;
	return RealizeVirtualPlacementState();
}

void AGeneratorWorldCampaign::ReportPlacementDebugEvents(const FWorldCampaignPlacementResult& Result) const
{
	for (const FWorldCampaignPlacementDebugEvent& DebugEvent : Result.DebugEvents)
	{
		if (DebugEvent.Message.IsEmpty())
		{
			continue;
		}

		RTSFunctionLibrary::ReportError(DebugEvent.Message);
	}
}

bool AGeneratorWorldCampaign::ValidateAsyncPlacementParityForCurrentSetup_Internal(FString& OutFailureReason)
{
	if (M_GenerationStep != ECampaignGenerationStep::ConnectionsCreated)
	{
		OutFailureReason = TEXT("generator must be at ConnectionsCreated so both solvers use the same snapshot.");
		return false;
	}

	FWorldCampaignPlacementSnapshot Snapshot;
	if (not BuildPlacementSnapshot(Snapshot))
	{
		OutFailureReason = TEXT("could not build placement snapshot.");
		return false;
	}

	FWorldCampaignPlacementResult SynchronousResult;
	const double SyncStartTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("World campaign placement parity: synchronous reference solve started."));
	if (not RunSynchronousPlacementBacktrackingForParity(SynchronousResult, OutFailureReason))
	{
		UE_LOG(LogTemp, Display,
		       TEXT("World campaign placement parity: synchronous reference solve failed after %.2fs: %s"),
		       FPlatformTime::Seconds() - SyncStartTime,
		       *OutFailureReason);
		return false;
	}
	UE_LOG(LogTemp, Display,
	       TEXT("World campaign placement parity: synchronous reference solve finished in %.2fs."),
	       FPlatformTime::Seconds() - SyncStartTime);
	LogPlacementParityResult(TEXT("sync"), SynchronousResult);
	UE_LOG(LogTemp, Display,
	       TEXT(
		       "World campaign placement parity: sync failure retries total=%d enemyStep=%d neutralStep=%d missionStep=%d."
	       ),
	       SynchronousResult.WorkerTotalAttemptCount,
	       SynchronousResult.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::EnemyObjectsPlaced),
	       SynchronousResult.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::NeutralObjectsPlaced),
	       SynchronousResult.StepAttemptIndicesAtEnd.FindRef(ECampaignGenerationStep::MissionsPlaced));

	const double AsyncStartTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("World campaign placement parity: async snapshot solve started."));
	const FWorldCampaignPlacementResult AsyncResult = WorldCampaignAsyncPlacement::SolvePlacement(Snapshot);
	UE_LOG(LogTemp, Display,
	       TEXT("World campaign placement parity: async snapshot solve finished in %.2fs with %d failure retries."),
	       FPlatformTime::Seconds() - AsyncStartTime,
	       AsyncResult.WorkerTotalAttemptCount);
	LogAsyncPlacementResultSummary(TEXT("ParityAsync"), Snapshot.SeedUsed, 1, AsyncResult);
	LogPlacementParityResult(TEXT("async"), AsyncResult);
	if (AsyncResult.bRequiresGameThreadBacktrack)
	{
		OutFailureReason = TEXT(
			"async solver requested setup backtracking; current setup is not a fixed-snapshot parity case.");
		return false;
	}

	if (not AsyncResult.bSucceeded)
	{
		OutFailureReason = AsyncResult.FailureReason;
		return false;
	}

	return ComparePlacementParityResults(SynchronousResult, AsyncResult, OutFailureReason);
}

FWorldCampaignPlacementParitySavedState AGeneratorWorldCampaign::CapturePlacementParityState() const
{
	FWorldCampaignPlacementParitySavedState SavedState;
	SavedState.PlacementState = M_PlacementState;
	SavedState.DerivedData = M_DerivedData;
	SavedState.StepTransactions = M_StepTransactions;
	SavedState.StepAttemptIndices = M_StepAttemptIndices;
	SavedState.TotalAttemptCount = M_TotalAttemptCount;
	SavedState.EnemyMicroPlacedCount = M_EnemyMicroPlacedCount;
	SavedState.MissionMicroPlacedCount = M_MissionMicroPlacedCount;
	SavedState.GenerationStep = M_GenerationStep;
	SavedState.BacktrackedFailedStepAttemptToPreserve = M_BacktrackedFailedStepAttemptToPreserve;
	SavedState.bDeferWorldObjectPromotionDuringBacktracking = bM_DeferWorldObjectPromotionDuringBacktracking;
	SavedState.bReportUndoContextActive = bM_Report_UndoContextActive;
	SavedState.ReportCurrentFailureStep = M_Report_CurrentFailureStep;
	return SavedState;
}

void AGeneratorWorldCampaign::RestorePlacementParityState(
	const FWorldCampaignPlacementParitySavedState& SavedState)
{
	M_PlacementState = SavedState.PlacementState;
	M_DerivedData = SavedState.DerivedData;
	M_StepTransactions = SavedState.StepTransactions;
	M_StepAttemptIndices = SavedState.StepAttemptIndices;
	M_TotalAttemptCount = SavedState.TotalAttemptCount;
	M_EnemyMicroPlacedCount = SavedState.EnemyMicroPlacedCount;
	M_MissionMicroPlacedCount = SavedState.MissionMicroPlacedCount;
	M_GenerationStep = SavedState.GenerationStep;
	M_BacktrackedFailedStepAttemptToPreserve = SavedState.BacktrackedFailedStepAttemptToPreserve;
	bM_DeferWorldObjectPromotionDuringBacktracking = SavedState.bDeferWorldObjectPromotionDuringBacktracking;
	bM_Report_UndoContextActive = SavedState.bReportUndoContextActive;
	M_Report_CurrentFailureStep = SavedState.ReportCurrentFailureStep;
}

void AGeneratorWorldCampaign::ResetPlacementStateForParityRun(
	const FWorldCampaignDerivedData& SetupDerivedData)
{
	M_PlacementState.PlayerHQAnchor = nullptr;
	M_PlacementState.PlayerHQAnchorKey = FGuid();
	M_PlacementState.EnemyHQAnchor = nullptr;
	M_PlacementState.EnemyHQAnchorKey = FGuid();
	M_PlacementState.EnemyItemsByAnchorKey.Reset();
	M_PlacementState.NeutralItemsByAnchorKey.Reset();
	M_PlacementState.MissionsByAnchorKey.Reset();
	M_DerivedData = SetupDerivedData;
	M_DerivedData.PlayerHQHopDistancesByAnchorKey.Reset();
	M_DerivedData.EnemyHQHopDistancesByAnchorKey.Reset();
	M_DerivedData.EnemyItemPlacedCounts.Reset();
	M_DerivedData.NeutralItemPlacedCounts.Reset();
	M_DerivedData.MissionPlacedCounts.Reset();
	M_StepTransactions.Reset();
	M_StepAttemptIndices.Reset();
	M_TotalAttemptCount = 0;
	M_EnemyMicroPlacedCount = 0;
	M_MissionMicroPlacedCount = 0;
	M_BacktrackedFailedStepAttemptToPreserve = ECampaignGenerationStep::NotStarted;
	M_GenerationStep = ECampaignGenerationStep::ConnectionsCreated;
	bM_DeferWorldObjectPromotionDuringBacktracking = true;
}

bool AGeneratorWorldCampaign::ExecuteSynchronousPlacementBacktrackingForParity(FString& OutFailureReason)
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
		if (ExecuteSynchronousPlacementStepForParity(StepToExecute))
		{
			StepIndex++;
			continue;
		}

		if (not HandleStepFailure(StepToExecute, StepIndex, StepOrder))
		{
			OutFailureReason = TEXT("synchronous placement failed while validating parity.");
			return false;
		}

		if (M_GenerationStep == ECampaignGenerationStep::NotStarted
			|| M_GenerationStep == ECampaignGenerationStep::AnchorPointsGenerated)
		{
			OutFailureReason = TEXT(
				"synchronous placement requested setup backtracking; current setup is not a fixed-snapshot parity case.");
			return false;
		}
	}

	M_GenerationStep = ECampaignGenerationStep::Finished;
	return true;
}

bool AGeneratorWorldCampaign::RunSynchronousPlacementBacktrackingForParity(FWorldCampaignPlacementResult& OutResult,
                                                                           FString& OutFailureReason)
{
	const FScopedRTSModalDialogSuppression DialogSuppression;
	const FWorldCampaignPlacementParitySavedState SavedState = CapturePlacementParityState();
	ResetPlacementStateForParityRun(SavedState.DerivedData);
	if (not ExecuteSynchronousPlacementBacktrackingForParity(OutFailureReason))
	{
		RestorePlacementParityState(SavedState);
		return false;
	}

	OutResult = BuildPlacementResultFromCurrentState();
	RestorePlacementParityState(SavedState);
	return true;
}

bool AGeneratorWorldCampaign::ExecuteSynchronousPlacementStepForParity(
	const ECampaignGenerationStep StepToExecute)
{
	switch (StepToExecute)
	{
	case ECampaignGenerationStep::PlayerHQPlaced:
		return ExecuteStepWithTransaction(StepToExecute, &AGeneratorWorldCampaign::ExecutePlaceHQ);
	case ECampaignGenerationStep::EnemyHQPlaced:
		return ExecuteStepWithTransaction(StepToExecute, &AGeneratorWorldCampaign::ExecutePlaceEnemyHQ);
	case ECampaignGenerationStep::EnemyWallPlaced:
		return ExecuteStepWithTransaction(StepToExecute, &AGeneratorWorldCampaign::ExecutePlaceEnemyWall);
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		return ExecuteStepWithTransaction(StepToExecute, &AGeneratorWorldCampaign::ExecutePlaceEnemyObjects);
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		return ExecuteStepWithTransaction(StepToExecute, &AGeneratorWorldCampaign::ExecutePlaceNeutralObjects);
	case ECampaignGenerationStep::MissionsPlaced:
		return ExecuteStepWithTransaction(StepToExecute, &AGeneratorWorldCampaign::ExecutePlaceMissions);
	default:
		return false;
	}
}

FWorldCampaignPlacementResult AGeneratorWorldCampaign::BuildPlacementResultFromCurrentState() const
{
	FWorldCampaignPlacementResult Result;
	Result.bSucceeded = true;
	Result.PlayerHQAnchorKey = M_PlacementState.PlayerHQAnchorKey;
	Result.EnemyHQAnchorKey = M_PlacementState.EnemyHQAnchorKey;
	Result.EnemyItemsByAnchorKey = M_PlacementState.EnemyItemsByAnchorKey;
	Result.NeutralItemsByAnchorKey = M_PlacementState.NeutralItemsByAnchorKey;
	Result.MissionsByAnchorKey = M_PlacementState.MissionsByAnchorKey;
	Result.DerivedData = M_DerivedData;
	Result.WorkerTotalAttemptCount = M_TotalAttemptCount;
	Result.StepAttemptIndicesAtEnd = M_StepAttemptIndices;
	return Result;
}

bool AGeneratorWorldCampaign::ComparePlacementParityResults(
	const FWorldCampaignPlacementResult& SynchronousResult,
	const FWorldCampaignPlacementResult& AsyncResult,
	FString& OutFailureReason) const
{
	if (SynchronousResult.PlayerHQAnchorKey != AsyncResult.PlayerHQAnchorKey)
	{
		OutFailureReason = TEXT("player HQ anchor mismatch.");
		return false;
	}

	if (SynchronousResult.EnemyHQAnchorKey != AsyncResult.EnemyHQAnchorKey)
	{
		OutFailureReason = TEXT("enemy HQ anchor mismatch.");
		return false;
	}

	if (not GetParityMapMatches(SynchronousResult.EnemyItemsByAnchorKey, AsyncResult.EnemyItemsByAnchorKey,
	                            TEXT("enemy placements"), BuildParityKeyString, OutFailureReason))
	{
		return false;
	}

	if (not GetParityMapMatches(SynchronousResult.NeutralItemsByAnchorKey, AsyncResult.NeutralItemsByAnchorKey,
	                            TEXT("neutral placements"), BuildParityKeyString, OutFailureReason))
	{
		return false;
	}

	if (not GetParityMapMatches(SynchronousResult.MissionsByAnchorKey, AsyncResult.MissionsByAnchorKey,
	                            TEXT("mission placements"), BuildParityKeyString, OutFailureReason))
	{
		return false;
	}

	return ComparePlacementParityDerivedData(SynchronousResult.DerivedData, AsyncResult.DerivedData,
	                                         OutFailureReason);
}

bool AGeneratorWorldCampaign::ComparePlacementParityDerivedData(
	const FWorldCampaignDerivedData& SynchronousDerivedData,
	const FWorldCampaignDerivedData& AsyncDerivedData,
	FString& OutFailureReason) const
{
	if (not GetParityMapMatches(SynchronousDerivedData.PlayerHQHopDistancesByAnchorKey,
	                            AsyncDerivedData.PlayerHQHopDistancesByAnchorKey, TEXT("player HQ hop cache"),
	                            BuildParityKeyString,
	                            OutFailureReason))
	{
		return false;
	}

	if (not GetParityMapMatches(SynchronousDerivedData.EnemyHQHopDistancesByAnchorKey,
	                            AsyncDerivedData.EnemyHQHopDistancesByAnchorKey, TEXT("enemy HQ hop cache"),
	                            BuildParityKeyString,
	                            OutFailureReason))
	{
		return false;
	}

	if (not GetParityMapMatches(SynchronousDerivedData.AnchorConnectionDegreesByAnchorKey,
	                            AsyncDerivedData.AnchorConnectionDegreesByAnchorKey,
	                            TEXT("anchor connection degree cache"),
	                            BuildParityKeyString, OutFailureReason))
	{
		return false;
	}

	if (not GetParityFloatMapMatches(SynchronousDerivedData.ChokepointScoresByAnchorKey,
	                                 AsyncDerivedData.ChokepointScoresByAnchorKey, TEXT("chokepoint score cache"),
	                                 OutFailureReason))
	{
		return false;
	}

	if (not GetParityMapMatches(SynchronousDerivedData.EnemyItemPlacedCounts,
	                            AsyncDerivedData.EnemyItemPlacedCounts, TEXT("enemy placed counts"),
	                            BuildParityEnumKeyString<EMapEnemyItem>, OutFailureReason))
	{
		return false;
	}

	if (not GetParityMapMatches(SynchronousDerivedData.NeutralItemPlacedCounts,
	                            AsyncDerivedData.NeutralItemPlacedCounts, TEXT("neutral placed counts"),
	                            BuildParityEnumKeyString<EMapNeutralObjectType>, OutFailureReason))
	{
		return false;
	}

	return GetParityMapMatches(SynchronousDerivedData.MissionPlacedCounts,
	                           AsyncDerivedData.MissionPlacedCounts, TEXT("mission placed counts"),
	                           BuildParityEnumKeyString<EMapMission>, OutFailureReason);
}

bool AGeneratorWorldCampaign::ExecuteAllStepsWithBacktracking()
{
	BeginPlacementBacktrackingRun();
	const TArray<ECampaignGenerationStep> StepOrder = BuildBacktrackingStepOrder();
	int32 StepIndex = 0;
	while (StepIndex < StepOrder.Num())
	{
		const ECampaignGenerationStep StepToExecute = StepOrder[StepIndex];
		bool (AGeneratorWorldCampaign::*StepFunction)(FCampaignGenerationStepTransaction&) = nullptr;
		if (not TryGetBacktrackingStepFunction(StepToExecute, StepFunction))
		{
			bM_DeferWorldObjectPromotionDuringBacktracking = false;
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
			ReportPlacementBacktrackingIfEnabled();
			bM_DeferWorldObjectPromotionDuringBacktracking = false;
			return false;
		}
	}

	bM_DeferWorldObjectPromotionDuringBacktracking = false;
	if (not RealizeVirtualPlacementState())
	{
		return false;
	}

	M_GenerationStep = ECampaignGenerationStep::Finished;
	ReportPlacementBacktrackingIfEnabled();
	return true;
}

void AGeneratorWorldCampaign::BeginPlacementBacktrackingRun()
{
	bM_DeferWorldObjectPromotionDuringBacktracking = true;
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_ResetPlacementReport();
		}
	}
}

TArray<ECampaignGenerationStep> AGeneratorWorldCampaign::BuildBacktrackingStepOrder() const
{
	TArray<ECampaignGenerationStep> StepOrder;
	StepOrder.Add(ECampaignGenerationStep::AnchorPointsGenerated);
	StepOrder.Add(ECampaignGenerationStep::ConnectionsCreated);
	StepOrder.Add(ECampaignGenerationStep::PlayerHQPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyHQPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyWallPlaced);
	StepOrder.Add(ECampaignGenerationStep::EnemyObjectsPlaced);
	StepOrder.Add(ECampaignGenerationStep::NeutralObjectsPlaced);
	StepOrder.Add(ECampaignGenerationStep::MissionsPlaced);
	return StepOrder;
}

bool AGeneratorWorldCampaign::TryGetBacktrackingStepFunction(
	const ECampaignGenerationStep StepToExecute,
	bool (AGeneratorWorldCampaign::*&OutStepFunction)(FCampaignGenerationStepTransaction&)) const
{
	switch (StepToExecute)
	{
	case ECampaignGenerationStep::AnchorPointsGenerated:
		OutStepFunction = &AGeneratorWorldCampaign::ExecuteGenerateAnchorPoints;
		return true;
	case ECampaignGenerationStep::ConnectionsCreated:
		OutStepFunction = &AGeneratorWorldCampaign::ExecuteCreateConnections;
		return true;
	case ECampaignGenerationStep::PlayerHQPlaced:
		OutStepFunction = &AGeneratorWorldCampaign::ExecutePlaceHQ;
		return true;
	case ECampaignGenerationStep::EnemyHQPlaced:
		OutStepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyHQ;
		return true;
	case ECampaignGenerationStep::EnemyWallPlaced:
		OutStepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyWall;
		return true;
	case ECampaignGenerationStep::EnemyObjectsPlaced:
		OutStepFunction = &AGeneratorWorldCampaign::ExecutePlaceEnemyObjects;
		return true;
	case ECampaignGenerationStep::NeutralObjectsPlaced:
		OutStepFunction = &AGeneratorWorldCampaign::ExecutePlaceNeutralObjects;
		return true;
	case ECampaignGenerationStep::MissionsPlaced:
		OutStepFunction = &AGeneratorWorldCampaign::ExecutePlaceMissions;
		return true;
	default:
		OutStepFunction = nullptr;
		return false;
	}
}

void AGeneratorWorldCampaign::ReportPlacementBacktrackingIfEnabled() const
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		if (GetIsValidCampaignDebugger())
		{
			UWorldCampaignDebugger* CampaignDebugger = GetCampaignDebugger();
			CampaignDebugger->Report_PrintPlacementReport(*this);
		}
	}
}

bool AGeneratorWorldCampaign::CanExecuteStep(ECampaignGenerationStep CompletedStep) const
{
	return M_GenerationStep == GetPrerequisiteStep(CompletedStep);
}

ECampaignGenerationStep AGeneratorWorldCampaign::GetPrerequisiteStep(ECampaignGenerationStep CompletedStep) const
{
	switch (CompletedStep)
	{
	case ECampaignGenerationStep::AnchorPointsGenerated:
		return ECampaignGenerationStep::NotStarted;
	case ECampaignGenerationStep::ConnectionsCreated:
		return ECampaignGenerationStep::AnchorPointsGenerated;
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
		return ECampaignGenerationStep::AnchorPointsGenerated;
	case ECampaignGenerationStep::AnchorPointsGenerated:
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
		if (StepToReset != M_BacktrackedFailedStepAttemptToPreserve)
		{
			M_StepAttemptIndices.Remove(StepToReset);
		}

		StepToReset = GetNextStep(StepToReset);
	}
}

EPlacementFailurePolicy AGeneratorWorldCampaign::GetFailurePolicyForStep(ECampaignGenerationStep FailedStep) const
{
	const EPlacementFailurePolicy GlobalPolicy = M_PlacementFailurePolicy.GlobalPolicy;
	switch (FailedStep)
	{
	case ECampaignGenerationStep::AnchorPointsGenerated:
		return GlobalPolicy;
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

int32 AGeneratorWorldCampaign::GetDesiredMicroUndoDepthForFailure(ECampaignGenerationStep FailedStep,
                                                                  int32 FailedStepAttemptIndex) const
{
	if (FailedStep != ECampaignGenerationStep::EnemyObjectsPlaced
		&& FailedStep != ECampaignGenerationStep::MissionsPlaced)
	{
		return 0;
	}

	const int32 MinimumWindow = 1;
	const int32 MinimumDepth = 1;
	const int32 Window = FMath::Max(MinimumWindow, M_PlacementFailurePolicy.EscalationAttempts);
	return MinimumDepth + (FailedStepAttemptIndex / Window);
}

int32 AGeneratorWorldCampaign::GetMicroUndoDepthForFailure(ECampaignGenerationStep FailedStep,
                                                           int32 FailedStepAttemptIndex,
                                                           int32 TrailingMicroTransactions) const
{
	if (TrailingMicroTransactions <= 0)
	{
		return 0;
	}

	const int32 MinimumDepth = 1;
	const int32 DesiredDepth = GetDesiredMicroUndoDepthForFailure(FailedStep, FailedStepAttemptIndex);
	return FMath::Clamp(DesiredDepth, MinimumDepth, TrailingMicroTransactions);
}

float AGeneratorWorldCampaign::GetFailSafeMinDistanceForEnemy(const EMapEnemyItem EnemyType) const
{
	return M_PlacementFailurePolicy.MinDistancePlayerHQEnemyItemByType.FindRef(EnemyType);
}

float AGeneratorWorldCampaign::GetFailSafeMinDistanceForNeutral(const EMapNeutralObjectType NeutralType) const
{
	return M_PlacementFailurePolicy.MinDistancePlayerHQNeutralByType.FindRef(NeutralType);
}

float AGeneratorWorldCampaign::GetFailSafeMinDistanceForMission(const EMapMission MissionType) const
{
	const FPerMissionRules* MissionRulesPtr = M_MissionPlacementRules.RulesByMission.Find(MissionType);
	if (MissionRulesPtr == nullptr)
	{
		return 0.0f;
	}

	const EMissionTier MissionTier = MissionRulesPtr->Tier;
	switch (MissionTier)
	{
	case EMissionTier::Tier1:
		return M_PlacementFailurePolicy.MinDistancePlayerHQTier1Mission;
	case EMissionTier::Tier2:
		return M_PlacementFailurePolicy.MinDistancePlayerHQTier2Mission;
	case EMissionTier::Tier3:
		return M_PlacementFailurePolicy.MinDistancePlayerHQTier3Mission;
	case EMissionTier::Tier4:
		return M_PlacementFailurePolicy.MinDistancePlayerHQTier4Mission;
	case EMissionTier::NotSet:
	default:
		return 0.0f;
	}
}

bool AGeneratorWorldCampaign::TryBuildTimeoutFailSafeAnchors(TArray<FEmptyAnchorDistance>& OutEmptyAnchors,
                                                             bool& bOutHasValidPlayerHQAnchor) const
{
	TArray<TObjectPtr<AAnchorPoint>> CandidateAnchors = M_PlacementState.CachedAnchors;
	if (CandidateAnchors.Num() == 0)
	{
		GatherAnchorPoints(CandidateAnchors);
	}

	if (CandidateAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: no anchors available for placement."));
		return false;
	}

	BuildFailSafeEmptyAnchors(CandidateAnchors, M_PlacementState, M_PlacementState.PlayerHQAnchor.Get(),
	                          OutEmptyAnchors, bOutHasValidPlayerHQAnchor);
	return true;
}

bool AGeneratorWorldCampaign::TryApplyTimeoutFailSafePlacement(const ECampaignGenerationStep FailedStep)
{
	bool bHasValidPlayerHQAnchor = false;
	TArray<FEmptyAnchorDistance> EmptyAnchors;
	if (not TryBuildTimeoutFailSafeAnchors(EmptyAnchors, bHasValidPlayerHQAnchor))
	{
		return false;
	}

	if (EmptyAnchors.Num() == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: no empty anchors available for placement."));
		return true;
	}

	TArray<FFailSafeItem> ItemsToPlace;
	BuildTimeoutFailSafeItemsToPlace(ItemsToPlace);

	if (ItemsToPlace.Num() == 0)
	{
		RTSFunctionLibrary::DisplayNotification(TEXT("Timeout fail-safe placed: Enemy 0, Neutral 0, Missions 0. "
			"Discarded: 0."));
		return true;
	}

	/*
	 * Capture rollback state before assigning missing items. The fail-safe can mutate placement maps,
	 * derived counts, and spawned actors before it reports final totals.
	 */
	FCampaignGenerationStepTransaction FailSafeTransaction = BuildTimeoutFailSafeTransaction(FailedStep);
	FFailSafePlacementTotals Totals;
	ApplyTimeoutFailSafeAssignments(FailedStep, bHasValidPlayerHQAnchor, EmptyAnchors, ItemsToPlace,
	                                FailSafeTransaction, Totals);
	FinalizeTimeoutFailSafePlacement(FailSafeTransaction, Totals);
	return true;
}

void AGeneratorWorldCampaign::BuildTimeoutFailSafeItemsToPlace(TArray<FFailSafeItem>& OutItemsToPlace) const
{
	const TArray<EMapMission> MissionPlacementPlan = BuildMissionPlacementPlanFiltered();
	BuildFailSafeItemsToPlace(M_CountAndDifficultyTuning, M_DerivedData, MissionPlacementPlan,
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

FCampaignGenerationStepTransaction AGeneratorWorldCampaign::BuildTimeoutFailSafeTransaction(
	const ECampaignGenerationStep FailedStep) const
{
	FCampaignGenerationStepTransaction FailSafeTransaction;
	FailSafeTransaction.CompletedStep = FailedStep;
	FailSafeTransaction.PreviousPlacementState = M_PlacementState;
	FailSafeTransaction.PreviousDerivedData = M_DerivedData;
	return FailSafeTransaction;
}

void AGeneratorWorldCampaign::ApplyTimeoutFailSafeAssignments(
	const ECampaignGenerationStep FailedStep,
	const bool bHasValidPlayerHQAnchor,
	TArray<FEmptyAnchorDistance>& EmptyAnchors,
	const TArray<FFailSafeItem>& ItemsToPlace,
	FCampaignGenerationStepTransaction& InOutFailSafeTransaction,
	FFailSafePlacementTotals& OutTotals)
{
	TArray<FFailSafeItem> RemainingItems;
	TMap<EFailSafeItemKind, TArray<FVector2D>> PlacedByKind;
	AssignFailSafeItemsByDistance(ItemsToPlace, bHasValidPlayerHQAnchor, FailedStep, EmptyAnchors, M_PlacementState,
	                              M_DerivedData, InOutFailSafeTransaction, OutTotals, PlacedByKind,
	                              M_PlacementFailurePolicy.TimeoutFailSafeMinSameKindXYSpacing,
	                              GetShouldDeferWorldObjectPromotion(), RemainingItems);
	AssignFailSafeItemsFallback(RemainingItems, FailedStep, M_PlacementFailurePolicy, M_PlacementState.SeedUsed,
	                            EmptyAnchors, M_PlacementState, M_DerivedData, InOutFailSafeTransaction, OutTotals,
	                            GetShouldDeferWorldObjectPromotion());
	ApplyTimeoutFailSafeEnemyDeclusterSwaps(
		FailedStep,
		M_PlacementFailurePolicy,
		[this](const EMapEnemyItem EnemyType)
		{
			return GetFailSafeMinDistanceForEnemy(EnemyType);
		},
		M_PlacementState,
		M_DerivedData,
		InOutFailSafeTransaction,
		GetShouldDeferWorldObjectPromotion());
}

void AGeneratorWorldCampaign::FinalizeTimeoutFailSafePlacement(
	const FCampaignGenerationStepTransaction& FailSafeTransaction,
	const FFailSafePlacementTotals& Totals)
{
	if (FailSafeTransaction.SpawnedActors.Num() > 0 || GetShouldDeferWorldObjectPromotion())
	{
		M_StepTransactions.Add(FailSafeTransaction);
	}

	RTSFunctionLibrary::DisplayNotification(FString::Printf(
		TEXT("Timeout fail-safe placed: Enemy %d, Neutral %d, Missions %d. Discarded: %d."),
		Totals.EnemyPlaced, Totals.NeutralPlaced, Totals.MissionPlaced, Totals.Discarded));
}

bool AGeneratorWorldCampaign::HandleStepFailure(ECampaignGenerationStep FailedStep, int32& InOutStepIndex,
                                                const TArray<ECampaignGenerationStep>& StepOrder)
{
	IncrementStepAttempt(FailedStep);
	M_TotalAttemptCount++;

	const bool bStepAttemptsExceeded = GetStepAttemptIndex(FailedStep) > MaxStepAttempts;
	const bool bTotalAttemptsExceeded = M_TotalAttemptCount > MaxTotalAttempts;
	if (bStepAttemptsExceeded || bTotalAttemptsExceeded)
	{
		return HandleAttemptLimitFailure(FailedStep, bStepAttemptsExceeded, StepOrder, InOutStepIndex);
	}

	const EPlacementFailurePolicy FailurePolicy = GetFailurePolicyForStep(FailedStep);
	if (FailurePolicy == EPlacementFailurePolicy::NotSet)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: no failure policy set for step."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(FailedStep);
	if (TryHandleMicroBacktracking(FailedStep, AttemptIndex, StepOrder, InOutStepIndex))
	{
		return true;
	}

	if (TryHandleRuleRelaxationRetry(FailedStep, FailurePolicy, AttemptIndex, StepOrder, InOutStepIndex))
	{
		return true;
	}

	return HandleMacroBacktracking(FailedStep, FailurePolicy, AttemptIndex, StepOrder, InOutStepIndex);
}

bool AGeneratorWorldCampaign::HandleAttemptLimitFailure(const ECampaignGenerationStep FailedStep,
                                                        const bool bStepAttemptsExceeded,
                                                        const TArray<ECampaignGenerationStep>& StepOrder,
                                                        int32& InOutStepIndex)
{
	if (M_PlacementFailurePolicy.bEnableTimeoutFailSafe && IsTimeoutFailSafeStep(FailedStep))
	{
		if (TryApplyTimeoutFailSafePlacement(FailedStep))
		{
			M_GenerationStep = ECampaignGenerationStep::Finished;
			InOutStepIndex = StepOrder.Num();
			return true;
		}
	}

	RTSFunctionLibrary::ReportError(bStepAttemptsExceeded
		                                ? TEXT("Generation failed: maximum step attempts exceeded.")
		                                : TEXT("Generation failed: maximum total attempts exceeded."));
	return false;
}

bool AGeneratorWorldCampaign::TryHandleMicroBacktracking(const ECampaignGenerationStep FailedStep,
                                                         const int32 AttemptIndex,
                                                         const TArray<ECampaignGenerationStep>& StepOrder,
                                                         int32& InOutStepIndex)
{
	const int32 TrailingMicroTransactions = CountTrailingMicroTransactionsForStep(FailedStep);
	const bool bCanUndoMicroTransactions = TrailingMicroTransactions > 0
		&& (FailedStep == ECampaignGenerationStep::EnemyObjectsPlaced
			|| FailedStep == ECampaignGenerationStep::MissionsPlaced);
	if (not bCanUndoMicroTransactions)
	{
		return false;
	}

	/*
	 * Micro rollback explores a different local placement before macro undo invalidates earlier steps.
	 * If the requested depth exceeds available history, macro backtracking continues below.
	 */
	BeginUndoReportContext(FailedStep);
	const int32 DesiredMicroUndoDepth = GetDesiredMicroUndoDepthForFailure(FailedStep, AttemptIndex);
	const int32 UndoDepth = GetMicroUndoDepthForFailure(FailedStep, AttemptIndex, TrailingMicroTransactions);
	for (int32 UndoIndex = 0; UndoIndex < UndoDepth; ++UndoIndex)
	{
		if (M_StepTransactions.Num() == 0)
		{
			break;
		}

		UndoLastTransaction();
	}
	EndUndoReportContext();

	if (DesiredMicroUndoDepth > TrailingMicroTransactions)
	{
		return false;
	}

	SetStepIndexOrZero(FailedStep, StepOrder, InOutStepIndex);
	return true;
}

bool AGeneratorWorldCampaign::TryHandleRuleRelaxationRetry(
	const ECampaignGenerationStep FailedStep,
	const EPlacementFailurePolicy FailurePolicy,
	const int32 AttemptIndex,
	const TArray<ECampaignGenerationStep>& StepOrder,
	int32& InOutStepIndex) const
{
	if (FailurePolicy != EPlacementFailurePolicy::BreakDistanceRules_ThenBackTrack
		|| AttemptIndex > MaxRelaxationAttempts)
	{
		return false;
	}

	SetStepIndexOrZero(FailedStep, StepOrder, InOutStepIndex);
	return true;
}

bool AGeneratorWorldCampaign::HandleMacroBacktracking(const ECampaignGenerationStep FailedStep,
                                                      const EPlacementFailurePolicy FailurePolicy,
                                                      const int32 AttemptIndex,
                                                      const TArray<ECampaignGenerationStep>& StepOrder,
                                                      int32& InOutStepIndex)
{
	TArray<ECampaignGenerationStep> EscalationSteps;
	BuildBacktrackEscalationSteps(FailedStep, EscalationSteps);
	const int32 MaxBacktrackSteps = EscalationSteps.Num();
	const int32 AdjustedAttemptIndex = FailurePolicy == EPlacementFailurePolicy::BreakDistanceRules_ThenBackTrack
		                                   ? AttemptIndex - MaxRelaxationAttempts
		                                   : AttemptIndex;
	const int32 TransactionsToUndo = FMath::Clamp(AdjustedAttemptIndex - 1, 0, MaxBacktrackSteps) + 1;
	const int32 AvailableTransactions = M_StepTransactions.Num();
	const int32 UndoCount = FMath::Min(TransactionsToUndo, AvailableTransactions);

	BeginUndoReportContext(FailedStep);
	for (int32 StepCount = 0; StepCount < UndoCount; StepCount++)
	{
		UndoLastTransaction();
	}
	EndUndoReportContext();

	const ECampaignGenerationStep RetryStep = GetNextStep(M_GenerationStep);
	IncrementBacktrackedRetryStepAttempt(FailedStep, RetryStep);
	SetStepIndexOrZero(RetryStep, StepOrder, InOutStepIndex);
	return true;
}

void AGeneratorWorldCampaign::SetStepIndexOrZero(const ECampaignGenerationStep Step,
                                                 const TArray<ECampaignGenerationStep>& StepOrder,
                                                 int32& OutStepIndex) const
{
	OutStepIndex = StepOrder.IndexOfByKey(Step);
	if (OutStepIndex == INDEX_NONE)
	{
		OutStepIndex = 0;
	}
}

void AGeneratorWorldCampaign::BeginUndoReportContext(const ECampaignGenerationStep FailedStep)
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		bM_Report_UndoContextActive = true;
		M_Report_CurrentFailureStep = FailedStep;
	}
}

void AGeneratorWorldCampaign::EndUndoReportContext()
{
	if constexpr (DeveloperSettings::Debugging::GCampaignBacktracking_Report_Compile_DebugSymbols)
	{
		bM_Report_UndoContextActive = false;
		M_Report_CurrentFailureStep = ECampaignGenerationStep::NotStarted;
	}
}

void AGeneratorWorldCampaign::IncrementBacktrackedRetryStepAttempt(const ECampaignGenerationStep FailedStep,
                                                                   const ECampaignGenerationStep RetryStep)
{
	if (RetryStep == FailedStep || RetryStep == ECampaignGenerationStep::Finished)
	{
		return;
	}

	// A downstream failure rewound this earlier step; advance it so deterministic selection explores a new candidate.
	M_BacktrackedFailedStepAttemptToPreserve = FailedStep;
	IncrementStepAttempt(RetryStep);
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

bool AGeneratorWorldCampaign::GenerateWorldWithAsyncPlacementForPruningValidation(FString& OutFailureReason)
{
	for (int32 SetupPassIndex = 0;
	     SetupPassIndex < MaxPruningValidationAsyncSetupPasses;
	     SetupPassIndex++)
	{
		if (not ExecuteGameThreadSetupStepsBeforeAsyncPlacement())
		{
			OutFailureReason = TEXT("game-thread setup failed before async placement.");
			return false;
		}

		FWorldCampaignPlacementSnapshot Snapshot;
		if (not BuildPlacementSnapshot(Snapshot))
		{
			OutFailureReason = TEXT("could not build async placement snapshot.");
			return false;
		}

		FWorldCampaignPlacementResult Result = WorldCampaignAsyncPlacement::SolvePlacement(Snapshot);
		LogAsyncPlacementResultSummary(TEXT("PruningValidation"), M_CountAndDifficultyTuning.Seed,
		                               SetupPassIndex + 1, Result);
		ReportPlacementDebugEvents(Result);
		M_TotalAttemptCount += Result.WorkerTotalAttemptCount;
		M_StepAttemptIndices = Result.StepAttemptIndicesAtEnd;

		if (Result.bRequiresGameThreadBacktrack)
		{
			if (not ApplyAsyncPlacementGameThreadBacktrack(Result))
			{
				OutFailureReason = TEXT("async placement requested game-thread backtracking that could not be applied.");
				return false;
			}

			continue;
		}

		if (not Result.bSucceeded)
		{
			OutFailureReason = Result.FailureReason;
			return false;
		}

		if (not ApplyPlacementResultToWorld(Result))
		{
			OutFailureReason = TEXT("async placement result could not be applied to world actors.");
			return false;
		}

		M_GenerationStep = ECampaignGenerationStep::Finished;
		return true;
	}

	OutFailureReason = TEXT("async placement exceeded the pruning validation setup-backtrack pass limit.");
	return false;
}

TSet<FGuid> AGeneratorWorldCampaign::BuildGameplayAnchorKeysForPruning() const
{
	TSet<FGuid> GameplayAnchorKeys;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint) || not AnchorPoint->GetHasPromotedWorldObject())
		{
			continue;
		}

		GameplayAnchorKeys.Add(AnchorPoint->GetAnchorKey());
	}

	return GameplayAnchorKeys;
}

void AGeneratorWorldCampaign::RemovePrunedPlacementStateEntries()
{
	TSet<FGuid> CachedAnchorKeys;
	CachedAnchorKeys.Reserve(M_PlacementState.CachedAnchors.Num());
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		CachedAnchorKeys.Add(AnchorPoint->GetAnchorKey());
	}

	for (auto EnemyItemIterator = M_PlacementState.EnemyItemsByAnchorKey.CreateIterator();
	     EnemyItemIterator;
	     ++EnemyItemIterator)
	{
		if (CachedAnchorKeys.Contains(EnemyItemIterator.Key()))
		{
			continue;
		}

		EnemyItemIterator.RemoveCurrent();
	}

	for (auto NeutralItemIterator = M_PlacementState.NeutralItemsByAnchorKey.CreateIterator();
	     NeutralItemIterator;
	     ++NeutralItemIterator)
	{
		if (CachedAnchorKeys.Contains(NeutralItemIterator.Key()))
		{
			continue;
		}

		NeutralItemIterator.RemoveCurrent();
	}

	for (auto MissionIterator = M_PlacementState.MissionsByAnchorKey.CreateIterator(); MissionIterator; ++MissionIterator)
	{
		if (CachedAnchorKeys.Contains(MissionIterator.Key()))
		{
			continue;
		}

		MissionIterator.RemoveCurrent();
	}
}

void AGeneratorWorldCampaign::RefreshCampaignGraphAfterPruning()
{
	WorldCampaignPruningHelper::RebuildAnchorConnectionReferences(
		M_PlacementState.CachedAnchors,
		M_PlacementState.CachedConnections);
	M_GeneratedConnections = M_PlacementState.CachedConnections;

	CacheAnchorConnectionDegrees();
	CampaignGenerationHelper::BuildHopDistanceCache(
		M_PlacementState.PlayerHQAnchor,
		M_DerivedData.PlayerHQHopDistancesByAnchorKey);
	CampaignGenerationHelper::BuildHopDistanceCache(
		M_PlacementState.EnemyHQAnchor,
		M_DerivedData.EnemyHQHopDistancesByAnchorKey);
	BuildChokepointScoresCache(M_PlacementState.PlayerHQAnchor);
}

void AGeneratorWorldCampaign::RefreshConnectionTransactionAfterPruning()
{
	for (FCampaignGenerationStepTransaction& Transaction : M_StepTransactions)
	{
		if (Transaction.CompletedStep != ECampaignGenerationStep::ConnectionsCreated)
		{
			continue;
		}

		Transaction.SpawnedConnections = M_GeneratedConnections;
	}
}

bool AGeneratorWorldCampaign::GetPrunedCachedAnchorsHaveOnlyGameplay(FString& OutFailureReason) const
{
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			OutFailureReason = TEXT("cached anchors contain an invalid anchor pointer.");
			return false;
		}

		const FGuid AnchorKey = AnchorPoint->GetAnchorKey();
		if (AnchorPoint->GetHasPromotedWorldObject())
		{
			continue;
		}

		OutFailureReason = FString::Printf(
			TEXT("cached anchor %s has no gameplay object after pruning."),
			*AnchorKey.ToString());
		return false;
	}

	return true;
}

bool AGeneratorWorldCampaign::GetPrunedConnectionsAreValid(FString& OutFailureReason) const
{
	TSet<FGuid> CachedAnchorKeys;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		CachedAnchorKeys.Add(AnchorPoint->GetAnchorKey());
	}

	for (const TObjectPtr<AConnection>& Connection : M_PlacementState.CachedConnections)
	{
		if (not IsValid(Connection))
		{
			OutFailureReason = TEXT("cached connections contain an invalid connection pointer.");
			return false;
		}

		const TArray<TObjectPtr<AAnchorPoint>>& ConnectedAnchors = Connection->GetConnectedAnchors();
		if (ConnectedAnchors.Num() != 2)
		{
			OutFailureReason = TEXT("pruned connections must directly connect exactly two kept anchors.");
			return false;
		}

		for (const TObjectPtr<AAnchorPoint>& ConnectedAnchor : ConnectedAnchors)
		{
			if (not IsValid(ConnectedAnchor) || not CachedAnchorKeys.Contains(ConnectedAnchor->GetAnchorKey()))
			{
				OutFailureReason = TEXT("pruned connection references an anchor outside the cached pruned graph.");
				return false;
			}
		}
	}

	return true;
}

TSet<FGuid> AGeneratorWorldCampaign::BuildPrunedCachedAnchorKeys() const
{
	TSet<FGuid> CachedAnchorKeys;
	CachedAnchorKeys.Reserve(M_PlacementState.CachedAnchors.Num());
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		CachedAnchorKeys.Add(AnchorPoint->GetAnchorKey());
	}

	return CachedAnchorKeys;
}

TSet<const AConnection*> AGeneratorWorldCampaign::BuildPrunedCachedConnectionSet() const
{
	TSet<const AConnection*> CachedConnections;
	CachedConnections.Reserve(M_PlacementState.CachedConnections.Num());
	for (const TObjectPtr<AConnection>& Connection : M_PlacementState.CachedConnections)
	{
		if (not IsValid(Connection))
		{
			continue;
		}

		CachedConnections.Add(Connection.Get());
	}

	return CachedConnections;
}

bool AGeneratorWorldCampaign::GetPrunedWorldActorsMatchCachedGraph(FString& OutFailureReason) const
{
	if (not IsValid(GetWorld()))
	{
		OutFailureReason = TEXT("world is invalid while validating pruned actors.");
		return false;
	}

	const TSet<FGuid> CachedAnchorKeys = BuildPrunedCachedAnchorKeys();
	if (not GetPrunedAnchorActorsMatchCachedGraph(CachedAnchorKeys, OutFailureReason))
	{
		return false;
	}

	const TSet<const AConnection*> CachedConnections = BuildPrunedCachedConnectionSet();
	return GetPrunedConnectionActorsMatchCachedGraph(CachedConnections, OutFailureReason);
}

bool AGeneratorWorldCampaign::GetPrunedAnchorActorsMatchCachedGraph(
	const TSet<FGuid>& CachedAnchorKeys,
	FString& OutFailureReason) const
{
	for (TActorIterator<AAnchorPoint> AnchorIterator(GetWorld()); AnchorIterator; ++AnchorIterator)
	{
		const AAnchorPoint* AnchorPoint = *AnchorIterator;
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		if (CachedAnchorKeys.Contains(AnchorPoint->GetAnchorKey()))
		{
			continue;
		}

		if (AnchorPoint->GetHasPromotedWorldObject())
		{
			OutFailureReason = FString::Printf(
				TEXT("promoted anchor actor %s exists outside the pruned cached graph."),
				*AnchorPoint->GetName());
			return false;
		}

		if (AnchorPoint->Tags.Contains(GeneratedAnchorTag))
		{
			OutFailureReason = FString::Printf(
				TEXT("generated empty anchor actor %s still exists after pruning."),
				*AnchorPoint->GetName());
			return false;
		}
	}

	return true;
}

bool AGeneratorWorldCampaign::GetPrunedConnectionActorsMatchCachedGraph(
	const TSet<const AConnection*>& CachedConnections,
	FString& OutFailureReason) const
{
	for (TActorIterator<AConnection> ConnectionIterator(GetWorld()); ConnectionIterator; ++ConnectionIterator)
	{
		const AConnection* Connection = *ConnectionIterator;
		if (not IsValid(Connection) || Connection->GetOwner() != this)
		{
			continue;
		}

		if (CachedConnections.Contains(Connection))
		{
			continue;
		}

		OutFailureReason = FString::Printf(
			TEXT("generator-owned connection actor %s exists outside the pruned cached graph."),
			*Connection->GetName());
		return false;
	}

	return true;
}

bool AGeneratorWorldCampaign::GetPrunedAnchorsReachableFromPlayerHQ(FString& OutFailureReason) const
{
	if (not IsValid(M_PlacementState.PlayerHQAnchor))
	{
		OutFailureReason = TEXT("player HQ anchor is invalid after pruning.");
		return false;
	}

	TQueue<AAnchorPoint*> AnchorQueue;
	TSet<FGuid> ReachableAnchorKeys;
	AnchorQueue.Enqueue(M_PlacementState.PlayerHQAnchor);
	ReachableAnchorKeys.Add(M_PlacementState.PlayerHQAnchor->GetAnchorKey());

	while (not AnchorQueue.IsEmpty())
	{
		AAnchorPoint* CurrentAnchor = nullptr;
		AnchorQueue.Dequeue(CurrentAnchor);
		if (not IsValid(CurrentAnchor))
		{
			continue;
		}

		for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : CurrentAnchor->GetNeighborAnchors())
		{
			if (not IsValid(NeighborAnchor) || ReachableAnchorKeys.Contains(NeighborAnchor->GetAnchorKey()))
			{
				continue;
			}

			ReachableAnchorKeys.Add(NeighborAnchor->GetAnchorKey());
			AnchorQueue.Enqueue(NeighborAnchor.Get());
		}
	}

	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : M_PlacementState.CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		if (ReachableAnchorKeys.Contains(AnchorPoint->GetAnchorKey()))
		{
			continue;
		}

		OutFailureReason = FString::Printf(
			TEXT("cached anchor %s is not reachable from Player HQ after pruning."),
			*AnchorPoint->GetAnchorKey().ToString());
		return false;
	}

	return true;
}

void AGeneratorWorldCampaign::CacheAnchorConnectionDegrees()
{
	M_DerivedData.AnchorConnectionDegreesByAnchorKey.Reset();
	for (TWeakObjectPtr<AAnchorPoint> AnchorPointWeak : M_PlacementState.CachedAnchors)
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

	InitializeChokepointScores(CachedAnchors);

	if (not IsValid(OptionalHQAnchor))
	{
		AddDegreeChokepointScores(CachedAnchors);
		AddSampledChokepointPathScores(CachedAnchors, AnchorLookup);
		return;
	}

	AddHQChokepointPathScores(OptionalHQAnchor, CachedAnchors, AnchorLookup);
}

void AGeneratorWorldCampaign::InitializeChokepointScores(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors)
{
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		M_DerivedData.ChokepointScoresByAnchorKey.Add(AnchorPoint->GetAnchorKey(), 0.f);
	}
}

void AGeneratorWorldCampaign::AddDegreeChokepointScores(const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors)
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
}

void AGeneratorWorldCampaign::AddSampledChokepointPathScores(
	const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors,
	const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
{
	TArray<TObjectPtr<AAnchorPoint>> ShuffledAnchors;
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : CachedAnchors)
	{
		if (IsValid(AnchorPoint))
		{
			ShuffledAnchors.Add(AnchorPoint);
		}
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
		for (int32 TargetIndex = StartIndex + 1; TargetIndex < ShuffledAnchors.Num(); TargetIndex++)
		{
			if (SampleCount >= MaxChokepointPairSamples)
			{
				return;
			}

			TArray<FGuid> PathKeys;
			if (BuildShortestPathKeys(ShuffledAnchors[StartIndex], ShuffledAnchors[TargetIndex], AnchorLookup,
			                          PathKeys))
			{
				AddChokepointPathContribution(PathKeys, M_DerivedData.ChokepointScoresByAnchorKey);
				SampleCount++;
			}
		}
	}
}

void AGeneratorWorldCampaign::AddHQChokepointPathScores(
	const AAnchorPoint* HQAnchor,
	const TArray<TObjectPtr<AAnchorPoint>>& CachedAnchors,
	const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup)
{
	for (const TObjectPtr<AAnchorPoint>& TargetAnchor : CachedAnchors)
	{
		if (not IsValid(TargetAnchor) || TargetAnchor == HQAnchor)
		{
			continue;
		}

		TArray<FGuid> PathKeys;
		if (BuildShortestPathKeys(HQAnchor, TargetAnchor, AnchorLookup, PathKeys))
		{
			AddChokepointPathContribution(PathKeys, M_DerivedData.ChokepointScoresByAnchorKey);
		}
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

	for (int32 SourceIndex = 0; SourceIndex < CandidateSource.Num(); SourceIndex++)
	{
		const TObjectPtr<AAnchorPoint>& CandidateAnchor = CandidateSource[SourceIndex];
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

	SortAnchorsByDeterministicOrder(OutCandidates);

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

FGuid AGeneratorWorldCampaign::BuildGeneratedAnchorKey_Deterministic(int32 StepAttemptIndex, int32 CellIndex,
                                                                     int32 SpawnOrdinal) const
{
	const uint64 BaseSeed = static_cast<uint64>(M_CountAndDifficultyTuning.Seed);
	const uint64 SeedA = HashCombine64(
		BaseSeed,
		AnchorKeySeedSaltA,
		static_cast<uint64>(StepAttemptIndex),
		static_cast<uint64>(CellIndex),
		static_cast<uint64>(SpawnOrdinal));
	const uint64 SeedB = HashCombine64(
		BaseSeed,
		AnchorKeySeedSaltB,
		static_cast<uint64>(StepAttemptIndex),
		static_cast<uint64>(CellIndex),
		static_cast<uint64>(SpawnOrdinal));
	return MakeDeterministicGuid(SeedA, SeedB);
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

	AConnection* NewConnection = SpawnConnectionBetweenAnchors(AnchorPoint, CandidateAnchor);
	return FinalizeCreatedConnection(NewConnection, AnchorPoint, CandidateAnchor, ExistingSegments, DebugColor);
}

TSubclassOf<AConnection> AGeneratorWorldCampaign::GetConnectionClassToSpawn() const
{
	if (M_ConnectionClass)
	{
		return M_ConnectionClass;
	}

	return AConnection::StaticClass();
}

AConnection* AGeneratorWorldCampaign::SpawnConnectionBetweenAnchors(AAnchorPoint* AnchorPoint,
                                                                    AAnchorPoint* CandidateAnchor)
{
	UWorld* World = GetWorld();
	if (not IsValid(World) || not IsValid(AnchorPoint) || not IsValid(CandidateAnchor))
	{
		return nullptr;
	}

	constexpr float HalfValue = 0.5f;
	const FVector SpawnLocation = (AnchorPoint->GetActorLocation() + CandidateAnchor->GetActorLocation()) * HalfValue;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;

	return World->SpawnActor<AConnection>(
		GetConnectionClassToSpawn(),
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParameters);
}

bool AGeneratorWorldCampaign::FinalizeCreatedConnection(
	AConnection* NewConnection,
	AAnchorPoint* AnchorPoint,
	AAnchorPoint* CandidateAnchor,
	TArray<FConnectionSegment>& ExistingSegments,
	const FColor& DebugColor)
{
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

namespace
{
	void ValidateWorldCampaignPlacementParityCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (not IsValid(World))
		{
			UE_LOG(LogTemp, Error, TEXT("World campaign placement parity validation failed: invalid world."));
			return;
		}

		AGeneratorWorldCampaign* Generator = nullptr;
		for (TActorIterator<AGeneratorWorldCampaign> GeneratorIterator(World); GeneratorIterator; ++GeneratorIterator)
		{
			Generator = *GeneratorIterator;
			break;
		}

		if (not IsValid(Generator))
		{
			UE_LOG(LogTemp, Error, TEXT("World campaign placement parity validation failed: no generator found."));
			return;
		}

		int32 FirstSeed = 0;
		int32 SeedCount = 5;
		if (Args.Num() >= 1)
		{
			FirstSeed = FCString::Atoi(*Args[0]);
		}

		if (Args.Num() >= 2)
		{
			SeedCount = FCString::Atoi(*Args[1]);
		}

		FString FailureReason;
		if (Generator->ValidateAsyncPlacementParityForSeedRange(FirstSeed, SeedCount, FailureReason))
		{
			UE_LOG(LogTemp, Display,
			       TEXT("World campaign placement parity validation passed for %d seed(s), starting at %d."),
			       SeedCount,
			       FirstSeed);
			return;
		}

		UE_LOG(LogTemp, Error,
	       TEXT("World campaign placement parity validation failed: %s"),
	       *FailureReason);
	}

	void LogWorldCampaignPruningValidationResult(AGeneratorWorldCampaign* Generator, const int32 Seed)
	{
		if (not IsValid(Generator))
		{
			UE_LOG(LogTemp, Error, TEXT("World campaign pruning validation failed: invalid generator."));
			return;
		}

		FString FailureReason;
		if (Generator->GenerateAndValidatePrunedWorldForSeed(Seed, FailureReason))
		{
			UE_LOG(LogTemp, Display,
			       TEXT("World campaign pruning validation passed for seed %d."),
			       Seed);
			return;
		}

		UE_LOG(LogTemp, Error,
		       TEXT("World campaign pruning validation failed: %s"),
		       *FailureReason);
	}

	bool GetShouldRequestExitAfterWorldCampaignValidation(const TArray<FString>& Args)
	{
		for (const FString& Arg : Args)
		{
			FString NormalizedArg = Arg;
			NormalizedArg.ReplaceInline(TEXT(";"), TEXT(""));
			if (NormalizedArg.Equals(TEXT("Quit"), ESearchCase::IgnoreCase)
				|| NormalizedArg.Equals(TEXT("Exit"), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}

		return false;
	}

	void RequestExitAfterWorldCampaignValidationIfNeeded(const bool bShouldRequestExit)
	{
		if (not bShouldRequestExit)
		{
			return;
		}

		FPlatformMisc::RequestExit(false);
	}

	void ValidateWorldCampaignPruningCommand(const TArray<FString>& Args, UWorld* World)
	{
		const bool bShouldRequestExit = GetShouldRequestExitAfterWorldCampaignValidation(Args);
		if (not IsValid(World))
		{
			UE_LOG(LogTemp, Error, TEXT("World campaign pruning validation failed: invalid world."));
			RequestExitAfterWorldCampaignValidationIfNeeded(bShouldRequestExit);
			return;
		}

		AGeneratorWorldCampaign* Generator = nullptr;
		for (TActorIterator<AGeneratorWorldCampaign> GeneratorIterator(World); GeneratorIterator; ++GeneratorIterator)
		{
			Generator = *GeneratorIterator;
			break;
		}

		if (not IsValid(Generator))
		{
			UE_LOG(LogTemp, Error, TEXT("World campaign pruning validation failed: no generator found."));
			RequestExitAfterWorldCampaignValidationIfNeeded(bShouldRequestExit);
			return;
		}

		int32 Seed = 0;
		if (Args.Num() >= 1)
		{
			Seed = FCString::Atoi(*Args[0]);
		}

		AWorldPlayerController* WorldPlayerController = nullptr;
		for (TActorIterator<AWorldPlayerController> ControllerIterator(World); ControllerIterator; ++ControllerIterator)
		{
			WorldPlayerController = *ControllerIterator;
			break;
		}

		URTSGameInstance* GameInstance = World->GetGameInstance<URTSGameInstance>();
		if (not IsValid(WorldPlayerController) || not IsValid(GameInstance))
		{
			LogWorldCampaignPruningValidationResult(Generator, Seed);
			RequestExitAfterWorldCampaignValidationIfNeeded(bShouldRequestExit);
			return;
		}

		FCampaignGenerationSettings CampaignSettings = GameInstance->GetCampaignGenerationSettings();
		CampaignSettings.GenerationSeed = Seed;
		Generator->InitializeWorldGenerator(
			WorldPlayerController,
			CampaignSettings,
			GameInstance->GetSelectedGameDifficulty());

		LogWorldCampaignPruningValidationResult(Generator, Seed);
		RequestExitAfterWorldCampaignValidationIfNeeded(bShouldRequestExit);
	}

	static FAutoConsoleCommandWithWorldAndArgs GValidateWorldCampaignPlacementParityCommand(
		TEXT("RTS.WorldCampaign.ValidatePlacementParity"),
		TEXT("Validates synchronous placement against async snapshot placement. Args: [FirstSeed] [SeedCount]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ValidateWorldCampaignPlacementParityCommand));

	static FAutoConsoleCommandWithWorldAndArgs GValidateWorldCampaignPruningCommand(
		TEXT("RTS.WorldCampaign.ValidatePruning"),
		TEXT("Generates one campaign seed, prunes unused anchors, and validates pruned graph invariants. Args: [Seed]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ValidateWorldCampaignPruningCommand));
}
