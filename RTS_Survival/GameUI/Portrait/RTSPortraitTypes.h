#pragma once

#include "CoreMinimal.h"

#include "RTSPortraitTypes.generated.h"

UENUM()
enum class ERTSPortraitTypes :uint8
{
	None,
	GermanAnnouncer,
	SovietHazmat,
	SovietCortexCommander,
	SovietCyborgEmissive,
};
