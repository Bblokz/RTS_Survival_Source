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
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Boundary/WorldSplineBoundary.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Connection/Connection.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

namespace
{
	constexpr int32 MaxStepAttempts = 7000;
	constexpr int32 MaxTotalAttempts = 8000;
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
	constexpr uint64 Lower32BitMask = 0xFFFFFFFFull;

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
		int32 MinSameTypeHopDistance = INDEX_NONE;
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

	int32 GetHopPreferenceWeight(const int32 EffectiveMaxWeight,
	                             const int32 EffectiveFalloff,
	                             const int32 CandidateHopDistance,
	                             const int32 TargetHopDistance)
	{
		const int32 Delta = FMath::Abs(CandidateHopDistance - TargetHopDistance);
		return FMath::Max(1, EffectiveMaxWeight - Delta * EffectiveFalloff);
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

	bool SpawnAnchorsInGrid(UWorld* World, const AGeneratorWorldCampaign* Generator,
	                        TSubclassOf<AAnchorPoint> AnchorClass, FRandomStream& RandomStream,
	                        const FAnchorPointGridDefinition& GridDefinition,
	                        const TArray<FVector2D>& BoundaryPolygon,
	                        const TArray<TObjectPtr<AAnchorPoint>>& ExistingAnchors, int32 TargetCount,
	                        int32 StepAttemptIndex, int32 JitterAttemptsPerCell,
	                        FCampaignGenerationStepTransaction& OutTransaction,
	                        TArray<TObjectPtr<AAnchorPoint>>& OutNewAnchors, float MinDistanceSquared)
	{
		if (not IsValid(World))
		{
			return false;
		}

		if (not IsValid(Generator))
		{
			return false;
		}

		if (JitterAttemptsPerCell < AnchorPointJitterAttemptsMin)
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

			const int32 CellIndex = (GridDefinition.StartIndex + OffsetIndex) % GridDefinition.CellCount;
			const int32 CellX = CellIndex % GridDefinition.GridCountX;
			const int32 CellY = CellIndex / GridDefinition.GridCountX;
			const FVector2D CellMin(GridDefinition.BoundsMin.X + CellX * GridDefinition.CellSize,
			                        GridDefinition.BoundsMin.Y + CellY * GridDefinition.CellSize);
			const FVector2D CellMax(CellMin.X + GridDefinition.CellSize, CellMin.Y + GridDefinition.CellSize);
			const FVector2D CellCenter(CellMin.X + GridDefinition.CellSize * 0.5f,
			                           CellMin.Y + GridDefinition.CellSize * 0.5f);
			for (int32 AttemptInCell = 0; AttemptInCell < JitterAttemptsPerCell; AttemptInCell++)
			{
				if (OutNewAnchors.Num() >= TargetCount)
				{
					break;
				}

				const float JitterX = RandomStream.FRandRange(-GridDefinition.JitterRange, GridDefinition.JitterRange);
				const float JitterY = RandomStream.FRandRange(-GridDefinition.JitterRange, GridDefinition.JitterRange);
				FVector2D Candidate(CellCenter.X + JitterX, CellCenter.Y + JitterY);
				Candidate.X = FMath::Clamp(Candidate.X, CellMin.X, CellMax.X);
				Candidate.Y = FMath::Clamp(Candidate.Y, CellMin.Y, CellMax.Y);

				if (not IsPointInsidePolygon(Candidate, BoundaryPolygon))
				{
					continue;
				}

				if (not IsFarEnoughFromAnchors(Candidate, ExistingAnchors, OutNewAnchors, MinDistanceSquared))
				{
					continue;
				}

				const FVector SpawnLocation(Candidate.X, Candidate.Y, AnchorPointSpawnZ);
				const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);
				AAnchorPoint* SpawnedAnchor = World->SpawnActorDeferred<AAnchorPoint>(
					AnchorClass, SpawnTransform, nullptr,
					nullptr,
					ESpawnActorCollisionHandlingMethod::
					AlwaysSpawn);
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

		const int32 CandidateIndex = SelectMissionCandidateIndexDeterministic(
			Candidates,
			EffectiveRules,
			MissionPlacementRules,
			EffectiveRules.HopsDistancePreference,
			MissionType,
			AttemptIndex,
			MissionIndex,
			WorkingPlacementState.SeedUsed);
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
		if (not TrySelectMissionPlacementCandidate(Generator, MissionPlacementRules, EffectiveRules, MissionType,
		                                           AttemptIndex, MissionIndex, RelaxationState, NeutralRules,
		                                           WorkingPlacementState, WorkingDerivedData, AnchorLookup,
		                                           CandidateSourceOverride, bOverridePlacementWithArray,
		                                           EffectiveOverrideMinConnections, EffectiveOverrideMaxConnections,
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

	bool TryPlaceFailSafeItem(const FFailSafeItem& Item, const FEmptyAnchorDistance& AnchorEntry,
	                          const ECampaignGenerationStep FailedStep,
	                          FWorldCampaignPlacementState& InOutPlacementState,
	                          FWorldCampaignDerivedData& InOutDerivedData,
	                          FCampaignGenerationStepTransaction& InOutTransaction,
	                          FFailSafePlacementTotals& InOutTotals)
	{
		AAnchorPoint* AnchorPoint = AnchorEntry.AnchorPoint.Get();
		if (not IsValid(AnchorPoint))
		{
			RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: anchor pointer is invalid."));
			return false;
		}

		const FGuid AnchorKey = AnchorEntry.AnchorKey;
		AWorldMapObject* SpawnedObject = nullptr;
		if (Item.Kind == EFailSafeItemKind::Enemy)
		{
			InOutPlacementState.EnemyItemsByAnchorKey.Add(AnchorKey, Item.EnemyType);
			int32& PlacedCount = InOutDerivedData.EnemyItemPlacedCounts.FindOrAdd(Item.EnemyType);
			PlacedCount++;
			SpawnedObject = AnchorPoint->OnEnemyItemPromotion(Item.EnemyType, FailedStep);
			if (not IsValid(SpawnedObject))
			{
				InOutPlacementState.EnemyItemsByAnchorKey.Remove(AnchorKey);
				PlacedCount = FMath::Max(0, PlacedCount - 1);
				if (PlacedCount == 0)
				{
					InOutDerivedData.EnemyItemPlacedCounts.Remove(Item.EnemyType);
				}

				RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: failed to spawn enemy item."));
				return false;
			}

			InOutTotals.EnemyPlaced++;
		}
		else if (Item.Kind == EFailSafeItemKind::Neutral)
		{
			InOutPlacementState.NeutralItemsByAnchorKey.Add(AnchorKey, Item.NeutralType);
			int32& PlacedCount = InOutDerivedData.NeutralItemPlacedCounts.FindOrAdd(Item.NeutralType);
			PlacedCount++;
			SpawnedObject = AnchorPoint->OnNeutralItemPromotion(Item.NeutralType, FailedStep);
			if (not IsValid(SpawnedObject))
			{
				InOutPlacementState.NeutralItemsByAnchorKey.Remove(AnchorKey);
				PlacedCount = FMath::Max(0, PlacedCount - 1);
				if (PlacedCount == 0)
				{
					InOutDerivedData.NeutralItemPlacedCounts.Remove(Item.NeutralType);
				}

				RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: failed to spawn neutral item."));
				return false;
			}

			InOutTotals.NeutralPlaced++;
		}
		else
		{
			InOutPlacementState.MissionsByAnchorKey.Add(AnchorKey, Item.MissionType);
			int32& PlacedCount = InOutDerivedData.MissionPlacedCounts.FindOrAdd(Item.MissionType);
			PlacedCount++;
			SpawnedObject = AnchorPoint->OnMissionPromotion(Item.MissionType, FailedStep);
			if (not IsValid(SpawnedObject))
			{
				InOutPlacementState.MissionsByAnchorKey.Remove(AnchorKey);
				PlacedCount = FMath::Max(0, PlacedCount - 1);
				if (PlacedCount == 0)
				{
					InOutDerivedData.MissionPlacedCounts.Remove(Item.MissionType);
				}

				RTSFunctionLibrary::ReportError(TEXT("Timeout fail-safe: failed to spawn mission."));
				return false;
			}

			InOutTotals.MissionPlaced++;
		}

		InOutTransaction.SpawnedActors.Add(SpawnedObject);
		return true;
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

		const FVector2D PlacedLocationXY(AnchorEntry.AnchorPoint->GetActorLocation().X, AnchorEntry.AnchorPoint->GetActorLocation().Y);
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
				                             InOutTransaction, InOutTotals))
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
	                                 FFailSafePlacementTotals& InOutTotals)
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
				                         InOutTransaction, InOutTotals))
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

	int32 GetMinSameTypeHopDistanceForEnemyAnchor(const FGuid& AnchorKey, const EMapEnemyItem EnemyType,
	                                              const FWorldCampaignPlacementState& PlacementState,
	                                              const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                              const FGuid& IgnoreAnchorKeyA = FGuid(),
	                                              const FGuid& IgnoreAnchorKeyB = FGuid())
	{
		AAnchorPoint* TargetAnchor = FindAnchorByKey(AnchorLookup, AnchorKey);
		if (not IsValid(TargetAnchor))
		{
			return INDEX_NONE;
		}

		int32 MinHopDistance = INDEX_NONE;
		TArray<FGuid> SortedEnemyAnchorKeys;
		SortedEnemyAnchorKeys.Reserve(PlacementState.EnemyItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : PlacementState.EnemyItemsByAnchorKey)
		{
			SortedEnemyAnchorKeys.Add(EnemyPair.Key);
		}

		Algo::Sort(SortedEnemyAnchorKeys, [](const FGuid& Left, const FGuid& Right)
		{
			return AAnchorPoint::IsAnchorKeyLess(Left, Right);
		});

		for (const FGuid& OtherAnchorKey : SortedEnemyAnchorKeys)
		{
			if (OtherAnchorKey == AnchorKey || OtherAnchorKey == IgnoreAnchorKeyA || OtherAnchorKey == IgnoreAnchorKeyB)
			{
				continue;
			}

			if (PlacementState.EnemyItemsByAnchorKey.FindRef(OtherAnchorKey) != EnemyType)
			{
				continue;
			}

			AAnchorPoint* OtherAnchor = FindAnchorByKey(AnchorLookup, OtherAnchorKey);
			if (not IsValid(OtherAnchor))
			{
				continue;
			}

			const int32 HopDistance = CampaignGenerationHelper::HopsFromHQ(TargetAnchor, OtherAnchor);
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

		TArray<FEnemyDeclusterAnchorScore> AnchorScores;
		AnchorScores.Reserve(PlacementState.EnemyItemsByAnchorKey.Num());
		for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : PlacementState.EnemyItemsByAnchorKey)
		{
			if (EnemyPair.Value == EMapEnemyItem::EnemyHQ || EnemyPair.Value == EMapEnemyItem::EnemyWall
				|| EnemyPair.Value == EMapEnemyItem::None)
			{
				continue;
			}

			FEnemyDeclusterAnchorScore Score;
			Score.AnchorKey = EnemyPair.Key;
			Score.EnemyType = EnemyPair.Value;
			Score.MinSameTypeHopDistance = GetMinSameTypeHopDistanceForEnemyAnchor(
				EnemyPair.Key,
				EnemyPair.Value,
				PlacementState,
				AnchorLookup);
			AnchorScores.Add(Score);
		}

		if (AnchorScores.Num() < 2)
		{
			return BestCandidate;
		}

		AnchorScores.Sort([](const FEnemyDeclusterAnchorScore& Left, const FEnemyDeclusterAnchorScore& Right)
		{
			const int32 LeftSortValue = Left.MinSameTypeHopDistance == INDEX_NONE
				                            ? TNumericLimits<int32>::Max()
				                            : Left.MinSameTypeHopDistance;
			const int32 RightSortValue = Right.MinSameTypeHopDistance == INDEX_NONE
				                             ? TNumericLimits<int32>::Max()
				                             : Right.MinSameTypeHopDistance;
			if (LeftSortValue != RightSortValue)
			{
				return LeftSortValue < RightSortValue;
			}

			return AAnchorPoint::IsAnchorKeyLess(Left.AnchorKey, Right.AnchorKey);
		});

		const int32 MaxCandidatesToEvaluate = FMath::Clamp(TopMCandidates, 2, AnchorScores.Num());
		for (int32 IndexA = 0; IndexA < MaxCandidatesToEvaluate - 1; ++IndexA)
		{
			for (int32 IndexB = IndexA + 1; IndexB < MaxCandidatesToEvaluate; ++IndexB)
			{
				const FEnemyDeclusterAnchorScore& AnchorA = AnchorScores[IndexA];
				const FEnemyDeclusterAnchorScore& AnchorB = AnchorScores[IndexB];
				if (AnchorA.EnemyType == AnchorB.EnemyType)
				{
					continue;
				}

				AAnchorPoint* AnchorPointA = FindAnchorByKey(AnchorLookup, AnchorA.AnchorKey);
				AAnchorPoint* AnchorPointB = FindAnchorByKey(AnchorLookup, AnchorB.AnchorKey);
				if (not IsValid(AnchorPointA) || not IsValid(AnchorPointB))
				{
					continue;
				}

				const float MinDistanceForTypeAtA = GetFailSafeMinDistanceForEnemy(AnchorB.EnemyType);
				const float MinDistanceForTypeAtB = GetFailSafeMinDistanceForEnemy(AnchorA.EnemyType);
				if (not GetPassesEnemyTypeHQDistance(PlayerHQAnchor, AnchorPointA, MinDistanceForTypeAtA)
					|| not GetPassesEnemyTypeHQDistance(PlayerHQAnchor, AnchorPointB, MinDistanceForTypeAtB))
				{
					continue;
				}

				const FString FailedSwapPairKey = AnchorA.AnchorKey.ToString(EGuidFormats::DigitsWithHyphens) + TEXT("|")
					+ AnchorB.AnchorKey.ToString(EGuidFormats::DigitsWithHyphens);
				if (FailedSwapPairKeys.Contains(FailedSwapPairKey))
				{
					continue;
				}

				const int32 OldScoreA = AnchorA.MinSameTypeHopDistance == INDEX_NONE
					                    ? TNumericLimits<int32>::Max()
					                    : AnchorA.MinSameTypeHopDistance;
				const int32 OldScoreB = AnchorB.MinSameTypeHopDistance == INDEX_NONE
					                    ? TNumericLimits<int32>::Max()
					                    : AnchorB.MinSameTypeHopDistance;
				const int32 NewScoreA = GetMinSameTypeHopDistanceForEnemyAnchor(
					AnchorA.AnchorKey,
					AnchorB.EnemyType,
					PlacementState,
					AnchorLookup,
					AnchorA.AnchorKey,
					AnchorB.AnchorKey);
				const int32 NewScoreB = GetMinSameTypeHopDistanceForEnemyAnchor(
					AnchorB.AnchorKey,
					AnchorA.EnemyType,
					PlacementState,
					AnchorLookup,
					AnchorA.AnchorKey,
					AnchorB.AnchorKey);

				const int32 EffectiveNewScoreA = NewScoreA == INDEX_NONE ? TNumericLimits<int32>::Max() : NewScoreA;
				const int32 EffectiveNewScoreB = NewScoreB == INDEX_NONE ? TNumericLimits<int32>::Max() : NewScoreB;
				const int32 Improvement = (EffectiveNewScoreA + EffectiveNewScoreB) - (OldScoreA + OldScoreB);
				if (Improvement <= 0)
				{
					continue;
				}

				if (not BestCandidate.bIsValid || Improvement > BestCandidate.Improvement
					|| (Improvement == BestCandidate.Improvement
						&& (AAnchorPoint::IsAnchorKeyLess(AnchorA.AnchorKey, BestCandidate.AnchorKeyA)
							|| (AnchorA.AnchorKey == BestCandidate.AnchorKeyA
								&& AAnchorPoint::IsAnchorKeyLess(AnchorB.AnchorKey, BestCandidate.AnchorKeyB)))))
				{
					BestCandidate.AnchorKeyA = AnchorA.AnchorKey;
					BestCandidate.AnchorKeyB = AnchorB.AnchorKey;
					BestCandidate.EnemyTypeA = AnchorA.EnemyType;
					BestCandidate.EnemyTypeB = AnchorB.EnemyType;
					BestCandidate.Improvement = Improvement;
					BestCandidate.bIsValid = true;
				}
			}
		}

		return BestCandidate;
	}

	bool TryApplyEnemyDeclusterSwap(const ECampaignGenerationStep FailedStep,
	                                const FEnemyDeclusterSwapCandidate& SwapCandidate,
	                                const TMap<FGuid, TObjectPtr<AAnchorPoint>>& AnchorLookup,
	                                FWorldCampaignPlacementState& InOutPlacementState,
	                                FWorldCampaignDerivedData& InOutDerivedData,
	                                FCampaignGenerationStepTransaction& InOutFailSafeTransaction)
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

		AnchorA->RemovePromotedWorldObject();
		AnchorB->RemovePromotedWorldObject();

		AWorldMapObject* SpawnedObjectA = AnchorA->OnEnemyItemPromotion(SwapCandidate.EnemyTypeB, FailedStep);
		AWorldMapObject* SpawnedObjectB = AnchorB->OnEnemyItemPromotion(SwapCandidate.EnemyTypeA, FailedStep);
		if (IsValid(SpawnedObjectA) && IsValid(SpawnedObjectB))
		{
			InOutFailSafeTransaction.SpawnedActors.Add(SpawnedObjectA);
			InOutFailSafeTransaction.SpawnedActors.Add(SpawnedObjectB);
			return true;
		}

		InOutPlacementState = PlacementStateBeforeSwap;
		InOutDerivedData = DerivedDataBeforeSwap;

		AnchorA->RemovePromotedWorldObject();
		AnchorB->RemovePromotedWorldObject();

		AWorldMapObject* RestoredObjectA = AnchorA->OnEnemyItemPromotion(SwapCandidate.EnemyTypeA, FailedStep);
		AWorldMapObject* RestoredObjectB = AnchorB->OnEnemyItemPromotion(SwapCandidate.EnemyTypeB, FailedStep);
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
	                                            FCampaignGenerationStepTransaction& InOutFailSafeTransaction)
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
			                               InOutDerivedData, InOutFailSafeTransaction))
			{
				AppliedSwaps++;
				continue;
			}

			const FString FailedSwapPairKey = BestSwap.AnchorKeyA.ToString(EGuidFormats::DigitsWithHyphens)
				+ TEXT("|")
				+ BestSwap.AnchorKeyB.ToString(EGuidFormats::DigitsWithHyphens);
			FailedSwapPairKeys.Add(FailedSwapPairKey);
		}
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

	ApplyDebuggerSettingsToComponent();
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
	M_CountAndDifficultyTuning.Seed = CampaignGenerationSettings.GenerationSeed;
	M_CountAndDifficultyTuning.DifficultyLevel = DifficultySettings.DifficultyLevel;
	M_CountAndDifficultyTuning.DifficultyPercentage = DifficultySettings.DifficultyPercentage;
	if (CampaignGenerationSettings.bUsesExtraDifficultyPercentage)
	{
		M_CountAndDifficultyTuning.DifficultyPercentage += M_CountAndDifficultyTuning.AddedDifficultyPercentage;
	}
	if (CampaignGenerationSettings.bNeedsToGenerateCampaign)
	{
		ExecuteAllSteps();
		return;
	}
	RTSFunctionLibrary::DisplayNotification("No campaign generation requested for this map, is this a save?");
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

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		for (TActorIterator<AAnchorPoint> It(World); It; ++It)
		{
			AAnchorPoint* AnchorPoint = *It;
			if (not IsValid(AnchorPoint))
			{
				continue;
			}

			if (AnchorPoint->Tags.Contains(GeneratedAnchorTag))
			{
				AnchorPoint->Destroy();
			}
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

bool AGeneratorWorldCampaign::ExecuteGenerateAnchorPoints(FCampaignGenerationStepTransaction& OutTransaction)
{
	if (not M_AnchorPointGenerationSettings.bM_EnableGeneratedAnchorPoints)
	{
		RTSFunctionLibrary::DisplayNotification(TEXT("Anchor generation disabled; skipping."));
		return true;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}

	FString ErrorMessage;
	if (not ValidateAnchorPointGenerationSettings(M_AnchorPointGenerationSettings, ErrorMessage))
	{
		RTSFunctionLibrary::ReportError(ErrorMessage);
		return false;
	}

	TArray<FVector2D> BoundaryPolygon;
	if (not TryBuildSplineBoundaryPolygon(World, M_AnchorPointGenerationSettings, BoundaryPolygon))
	{
		return false;
	}

	TSubclassOf<AAnchorPoint> AnchorClass = M_AnchorPointGenerationSettings.M_GeneratedAnchorPointClass;
	if (not AnchorClass)
	{
		AnchorClass = AAnchorPoint::StaticClass();
	}

	TArray<TObjectPtr<AAnchorPoint>> ExistingAnchors;
	GatherAnchorPoints(ExistingAnchors);
	TArray<TObjectPtr<AAnchorPoint>> SortedExistingAnchors;
	SortedExistingAnchors.Reserve(ExistingAnchors.Num());
	for (const TObjectPtr<AAnchorPoint>& AnchorPoint : ExistingAnchors)
	{
		if (not IsValid(AnchorPoint))
		{
			continue;
		}

		SortedExistingAnchors.Add(AnchorPoint);
	}

	SortedExistingAnchors.Sort([](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
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

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::AnchorPointsGenerated);
	const int32 SeedOffset = AttemptIndex * AttemptSeedMultiplier;
	FRandomStream RandomStream(M_CountAndDifficultyTuning.Seed + SeedOffset + AnchorPointSeedOffset);

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

	const FString NotificationMessage = FString::Printf(
		TEXT("Anchor generation: spawned %d / preferred %d (min %d)."),
		NewAnchors.Num(),
		PreferredTargetCount,
		RequiredCount);
	RTSFunctionLibrary::DisplayNotification(NotificationMessage);

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

	const int32 AttemptIndex = GetStepAttemptIndex(ECampaignGenerationStep::PlayerHQPlaced);
	TArray<TObjectPtr<AAnchorPoint>> Candidates;
	bool bShouldForcePlacement = false;
	if (not BuildHQAnchorCandidates(CandidateSource, M_PlayerHQPlacementRules.MinAnchorDegreeForHQ,
	                                MaxAnchorDegreeUnlimited, nullptr, Candidates))
	{
		RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: no valid anchor candidates found."));
		bShouldForcePlacement = true;
	}

	TArray<TObjectPtr<AAnchorPoint>> NeighborhoodCandidates;
	if (not bShouldForcePlacement)
	{
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
		TArray<TObjectPtr<AAnchorPoint>> ForcedCandidates;
		if (not BuildHQAnchorCandidates(CandidateSource, 0, MaxAnchorDegreeUnlimited, nullptr, ForcedCandidates))
		{
			RTSFunctionLibrary::ReportError(TEXT("Player HQ placement failed: forced placement had no anchors."));
			return false;
		}

		const int32 SeedOffset = PlayerHQForcePlacementSeedOffset + (AttemptIndex * AttemptSeedMultiplier);
		FRandomStream RandomStream(M_PlacementState.SeedUsed + SeedOffset);
		const int32 ForcedIndex = RandomStream.RandRange(0, ForcedCandidates.Num() - 1);
		AnchorPoint = ForcedCandidates[ForcedIndex].Get();
	}

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
		TArray<TObjectPtr<AAnchorPoint>> ForcedCandidates;
		if (not BuildHQAnchorCandidates(CandidateSource, MinAnchorDegreeFloor, MaxAnchorDegreeUnlimited,
		                                M_PlacementState.PlayerHQAnchor.Get(), ForcedCandidates))
		{
			RTSFunctionLibrary::ReportError(TEXT("Enemy HQ placement failed: forced placement had no anchors."));
			return false;
		}

		const int32 SeedOffset = EnemyHQForcePlacementSeedOffset + (AttemptIndex * AttemptSeedMultiplier);
		FRandomStream RandomStream(M_PlacementState.SeedUsed + SeedOffset);
		const int32 ForcedIndex = RandomStream.RandRange(0, ForcedCandidates.Num() - 1);
		AnchorPoint = ForcedCandidates[ForcedIndex].Get();
	}

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
	StepOrder.Add(ECampaignGenerationStep::AnchorPointsGenerated);
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
		case ECampaignGenerationStep::AnchorPointsGenerated:
			StepFunction = &AGeneratorWorldCampaign::ExecuteGenerateAnchorPoints;
			break;
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
		M_StepAttemptIndices.Remove(StepToReset);
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

int32 AGeneratorWorldCampaign::GetMicroUndoDepthForFailure(ECampaignGenerationStep FailedStep,
                                                           int32 FailedStepAttemptIndex,
                                                           int32 TrailingMicroTransactions) const
{
	if (FailedStep != ECampaignGenerationStep::EnemyObjectsPlaced
		&& FailedStep != ECampaignGenerationStep::MissionsPlaced)
	{
		return 0;
	}

	const int32 MinimumWindow = 1;
	const int32 MinimumDepth = 1;
	const int32 Window = FMath::Max(MinimumWindow, M_PlacementFailurePolicy.EscalationAttempts);
	const int32 Depth = MinimumDepth + (FailedStepAttemptIndex / Window);
	return FMath::Clamp(Depth, MinimumDepth, TrailingMicroTransactions);
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

	const TArray<EMapMission> MissionPlacementPlan = BuildMissionPlacementPlanFiltered();
	TArray<FFailSafeItem> ItemsToPlace;
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
	                          ItemsToPlace);

	if (ItemsToPlace.Num() == 0)
	{
		RTSFunctionLibrary::DisplayNotification(TEXT("Timeout fail-safe placed: Enemy 0, Neutral 0, Missions 0. "
			"Discarded: 0."));
		return true;
	}

	FCampaignGenerationStepTransaction FailSafeTransaction;
	FailSafeTransaction.CompletedStep = FailedStep;
	FailSafeTransaction.PreviousPlacementState = M_PlacementState;
	FailSafeTransaction.PreviousDerivedData = M_DerivedData;

	FFailSafePlacementTotals Totals;
	TArray<FFailSafeItem> RemainingItems;
	TMap<EFailSafeItemKind, TArray<FVector2D>> PlacedByKind;
	AssignFailSafeItemsByDistance(ItemsToPlace, bHasValidPlayerHQAnchor, FailedStep, EmptyAnchors, M_PlacementState,
	                              M_DerivedData, FailSafeTransaction, Totals, PlacedByKind,
	                              M_PlacementFailurePolicy.TimeoutFailSafeMinSameKindXYSpacing, RemainingItems);
	AssignFailSafeItemsFallback(RemainingItems, FailedStep, M_PlacementFailurePolicy, M_PlacementState.SeedUsed,
	                            EmptyAnchors, M_PlacementState, M_DerivedData, FailSafeTransaction, Totals);
	ApplyTimeoutFailSafeEnemyDeclusterSwaps(
		FailedStep,
		M_PlacementFailurePolicy,
		[this](const EMapEnemyItem EnemyType)
		{
			return GetFailSafeMinDistanceForEnemy(EnemyType);
		},
		M_PlacementState,
		M_DerivedData,
		FailSafeTransaction);

	if (FailSafeTransaction.SpawnedActors.Num() > 0)
	{
		M_StepTransactions.Add(FailSafeTransaction);
	}

	RTSFunctionLibrary::DisplayNotification(FString::Printf(
		TEXT("Timeout fail-safe placed: Enemy %d, Neutral %d, Missions %d. Discarded: %d."),
		Totals.EnemyPlaced, Totals.NeutralPlaced, Totals.MissionPlaced, Totals.Discarded));
	return true;
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

	const EPlacementFailurePolicy FailurePolicy = GetFailurePolicyForStep(FailedStep);
	if (FailurePolicy == EPlacementFailurePolicy::NotSet)
	{
		RTSFunctionLibrary::ReportError(TEXT("Generation failed: no failure policy set for step."));
		return false;
	}

	const int32 AttemptIndex = GetStepAttemptIndex(FailedStep);
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

		const int32 UndoDepth = GetMicroUndoDepthForFailure(FailedStep, AttemptIndex, TrailingMicroTransactions);
		for (int32 UndoIndex = 0; UndoIndex < UndoDepth; ++UndoIndex)
		{
			if (M_StepTransactions.Num() == 0)
			{
				break;
			}

			UndoLastTransaction();
		}

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
