#include "WorldCampaignGenerationHelper.h"

#include "Containers/Queue.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

namespace CampaignGenerationHelper
{
	namespace
	{
		constexpr int32 InvalidHopCount = INDEX_NONE;
		constexpr float InvalidXYDistance = -1.0f;
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
}
