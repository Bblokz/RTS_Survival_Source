#include "WorldCampaignGenerationHelper.h"

#include "Containers/Queue.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

namespace CampaignGenerationHelper
{
	namespace
	{
		constexpr int32 InvalidHopCount = INDEX_NONE;
		constexpr float InvalidXYDistance = -1.0f;
		constexpr float FailSafeEnemyDeclusterRadius = 6000.f;
		constexpr float FailSafeEnemyDeclusterRadiusSquared =
			FailSafeEnemyDeclusterRadius * FailSafeEnemyDeclusterRadius;
	}

	void BuildHopDistanceCache(const AAnchorPoint* HQAnchor, TMap<FGuid, int32>& OutHopDistances)
	{
		OutHopDistances.Reset();
		if (not IsValid(HQAnchor))
		{
			return;
		}

		struct FHopQueueEntry
		{
			const AAnchorPoint* AnchorPoint = nullptr;
			int32 HopCount = 0;
		};

		TQueue<FHopQueueEntry> AnchorQueue;
		AnchorQueue.Enqueue({HQAnchor, 0});
		OutHopDistances.Add(HQAnchor->GetAnchorKey(), 0);

		while (not AnchorQueue.IsEmpty())
		{
			FHopQueueEntry Entry;
			AnchorQueue.Dequeue(Entry);

			const AAnchorPoint* CurrentAnchor = Entry.AnchorPoint;
			if (not IsValid(CurrentAnchor))
			{
				continue;
			}

			const TArray<TObjectPtr<AAnchorPoint>>& NeighborAnchors = CurrentAnchor->GetNeighborAnchors();
			for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : NeighborAnchors)
			{
				if (not IsValid(NeighborAnchor))
				{
					continue;
				}

				const FGuid NeighborKey = NeighborAnchor->GetAnchorKey();
				if (OutHopDistances.Contains(NeighborKey))
				{
					continue;
				}

				const int32 NextHops = Entry.HopCount + 1;
				OutHopDistances.Add(NeighborKey, NextHops);
				AnchorQueue.Enqueue({NeighborAnchor.Get(), NextHops});
			}
		}
	}

	int32 HopsFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor)
	{
		if (not IsValid(AnchorPoint) || not IsValid(HQAnchor))
		{
			return InvalidHopCount;
		}

		if (AnchorPoint == HQAnchor)
		{
			return 0;
		}

		TQueue<const AAnchorPoint*> AnchorQueue;
		TMap<const AAnchorPoint*, int32> HopsByAnchor;

		AnchorQueue.Enqueue(HQAnchor);
		HopsByAnchor.Add(HQAnchor, 0);

		while (not AnchorQueue.IsEmpty())
		{
			const AAnchorPoint* CurrentAnchor = nullptr;
			AnchorQueue.Dequeue(CurrentAnchor);

			if (not IsValid(CurrentAnchor))
			{
				continue;
			}

			const int32 CurrentHops = HopsByAnchor.FindRef(CurrentAnchor);
			const TArray<TObjectPtr<AAnchorPoint>>& NeighborAnchors = CurrentAnchor->GetNeighborAnchors();
			for (const TObjectPtr<AAnchorPoint>& NeighborAnchor : NeighborAnchors)
			{
				if (not IsValid(NeighborAnchor))
				{
					continue;
				}

				if (HopsByAnchor.Contains(NeighborAnchor))
				{
					continue;
				}

				const int32 NextHops = CurrentHops + 1;
				if (NeighborAnchor == AnchorPoint)
				{
					return NextHops;
				}

				HopsByAnchor.Add(NeighborAnchor, NextHops);
				AnchorQueue.Enqueue(NeighborAnchor);
			}
		}

		return InvalidHopCount;
	}

	float XYDistanceFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor)
	{
		if (not IsValid(AnchorPoint) || not IsValid(HQAnchor))
		{
			return InvalidXYDistance;
		}

		const FVector2D AnchorLocation(AnchorPoint->GetActorLocation());
		const FVector2D HQLocation(HQAnchor->GetActorLocation());
		return FVector2D::Distance(AnchorLocation, HQLocation);
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

	int32 GetHopPreferenceWeight(const int32 EffectiveMaxWeight,
	                             const int32 EffectiveFalloff,
	                             const int32 CandidateHopDistance,
	                             const int32 TargetHopDistance)
	{
		const int32 Delta = FMath::Abs(CandidateHopDistance - TargetHopDistance);
		return FMath::Max(1, EffectiveMaxWeight - Delta * EffectiveFalloff);
	}

	bool GetIsSwappableEnemyType(const EMapEnemyItem EnemyType)
	{
		return EnemyType != EMapEnemyItem::EnemyHQ
			&& EnemyType != EMapEnemyItem::EnemyWall
			&& EnemyType != EMapEnemyItem::None;
	}

	int32 ComputeEnemyLocalDensity(const FGuid& TargetAnchorKey,
	                               const EMapEnemyItem TargetEnemyType,
	                               const FVector2D& TargetLocationXY,
	                               const TMap<FGuid, EMapEnemyItem>& EnemyTypesByAnchorKey,
	                               const TMap<FGuid, FVector2D>& AnchorLocationsByKey)
	{
		int32 LocalDensity = 0;
		for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : EnemyTypesByAnchorKey)
		{
			if (EnemyPair.Key == TargetAnchorKey || EnemyPair.Value != TargetEnemyType)
			{
				continue;
			}

			const FVector2D* OtherLocationXY = AnchorLocationsByKey.Find(EnemyPair.Key);
			if (OtherLocationXY == nullptr)
			{
				continue;
			}

			if (FVector2D::DistSquared(TargetLocationXY, *OtherLocationXY) <= FailSafeEnemyDeclusterRadiusSquared)
			{
				LocalDensity++;
			}
		}

		return LocalDensity;
	}

	FString BuildCanonicalFailedEnemyDeclusterSwapPairKey(const FGuid& AnchorKeyA, const FGuid& AnchorKeyB)
	{
		FGuid CanonicalAnchorKeyA = AnchorKeyA;
		FGuid CanonicalAnchorKeyB = AnchorKeyB;
		if (AAnchorPoint::IsAnchorKeyLess(CanonicalAnchorKeyB, CanonicalAnchorKeyA))
		{
			Swap(CanonicalAnchorKeyA, CanonicalAnchorKeyB);
		}

		return CanonicalAnchorKeyA.ToString(EGuidFormats::DigitsWithHyphens)
			+ TEXT("|")
			+ CanonicalAnchorKeyB.ToString(EGuidFormats::DigitsWithHyphens);
	}

	void BuildImpactedEnemyDeclusterAnchorKeys(const FGuid& AnchorKeyA,
	                                           const FGuid& AnchorKeyB,
	                                           const TArray<TPair<FGuid, EMapEnemyItem>>& SortedSwappableAnchors,
	                                           const TMap<FGuid, FVector2D>& AnchorLocationsByKey,
	                                           TArray<FGuid>& OutImpactedAnchorKeys)
	{
		OutImpactedAnchorKeys.Reset();
		const FVector2D* AnchorLocationA = AnchorLocationsByKey.Find(AnchorKeyA);
		const FVector2D* AnchorLocationB = AnchorLocationsByKey.Find(AnchorKeyB);
		if (AnchorLocationA == nullptr || AnchorLocationB == nullptr)
		{
			return;
		}

		for (const TPair<FGuid, EMapEnemyItem>& EnemyPair : SortedSwappableAnchors)
		{
			const FVector2D* CandidateLocation = AnchorLocationsByKey.Find(EnemyPair.Key);
			if (CandidateLocation == nullptr)
			{
				continue;
			}

			if (FVector2D::DistSquared(*CandidateLocation, *AnchorLocationA) <= FailSafeEnemyDeclusterRadiusSquared
				|| FVector2D::DistSquared(*CandidateLocation, *AnchorLocationB) <= FailSafeEnemyDeclusterRadiusSquared)
			{
				OutImpactedAnchorKeys.Add(EnemyPair.Key);
			}
		}
	}

	int32 ComputeImpactedEnemyDeclusterDensitySum(const TArray<FGuid>& ImpactedAnchorKeys,
	                                              const TMap<FGuid, EMapEnemyItem>& EnemyTypesByAnchorKey,
	                                              const TMap<FGuid, FVector2D>& AnchorLocationsByKey)
	{
		int32 DensitySum = 0;
		for (const FGuid& ImpactedAnchorKey : ImpactedAnchorKeys)
		{
			const EMapEnemyItem* EnemyType = EnemyTypesByAnchorKey.Find(ImpactedAnchorKey);
			if (EnemyType == nullptr || not GetIsSwappableEnemyType(*EnemyType))
			{
				continue;
			}

			const FVector2D* AnchorLocationXY = AnchorLocationsByKey.Find(ImpactedAnchorKey);
			if (AnchorLocationXY == nullptr)
			{
				continue;
			}

			DensitySum += ComputeEnemyLocalDensity(
				ImpactedAnchorKey,
				*EnemyType,
				*AnchorLocationXY,
				EnemyTypesByAnchorKey,
				AnchorLocationsByKey);
		}

		return DensitySum;
	}
}
