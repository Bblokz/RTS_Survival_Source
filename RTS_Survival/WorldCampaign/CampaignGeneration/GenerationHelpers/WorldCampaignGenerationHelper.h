#pragma once

#include "CoreMinimal.h"

class AAnchorPoint;

namespace CampaignGenerationHelper
{
	int32 HopsFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor);
	float XYDistanceFromHQ(const AAnchorPoint* AnchorPoint, const AAnchorPoint* HQAnchor);
}
