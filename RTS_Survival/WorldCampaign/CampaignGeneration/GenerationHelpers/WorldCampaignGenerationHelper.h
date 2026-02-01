#pragma once

#include "CoreMinimal.h"

class AAnchorPoint;

namespace CampaignGenerationHelper
{
	int32 HopsFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor);
	float XYDistanceFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor);

	template<typename ElementType>
    	void DeterministicShuffle(TArray<ElementType>& InOutArray, FRandomStream& RandomStream)
    	{
    		const int32 ElementCount = InOutArray.Num();
    		if (ElementCount < 2)
    		{
    			return;
    		}
    
    		// Fisher–Yates shuffle driven by FRandomStream (deterministic for a given seed).
    		for (int32 LastIndex = ElementCount - 1; LastIndex > 0; --LastIndex)
    		{
    			const int32 SwapIndex = RandomStream.RandRange(0, LastIndex);
    			if (SwapIndex == LastIndex)
    			{
    				continue;
    			}
    
    			InOutArray.Swap(LastIndex, SwapIndex);
    		}
    	}
}
