#include "WorldCampaignGenerationHelper.h"

#include "Containers/Queue.h"
#include "WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

namespace CampaignGenerationHelper
{
	namespace
	{
		constexpr int32 InvalidHopCount = INDEX_NONE;
		constexpr float InvalidXYDistance = -1.0f;
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
