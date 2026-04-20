#pragma once

#include "MissionTrackingType.generated.h"

UENUM(BlueprintType)
enum class EMissionTrackingType : uint8
{
	NoTracking UMETA(DisplayName = "NoTracking"),
	TrackDestructablesCollapse UMETA(DisplayName = "TrackDestructablesCollapse"),
	TrackOnActorsDestroyed UMETA(DisplayName = "TrackOnActorsDestroyed")
};
