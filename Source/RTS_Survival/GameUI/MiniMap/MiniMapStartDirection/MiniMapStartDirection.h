#pragma once

#include "CoreMinimal.h"
#include "Slate/WidgetTransform.h"

// Defines in what way the mini map should be rotated.
UENUM(BlueprintType)
enum class EMiniMapStartDirection : uint8
{
	None,
	North,
	East,
	South,
	West,
};


static FWidgetTransform Global_GetMiniMapTransform(const EMiniMapStartDirection
                                                   Direction, FWidgetTransform OriginalTransform)
{
	switch (Direction)
	{
	case EMiniMapStartDirection::None:
		break;
	case EMiniMapStartDirection::North:
		OriginalTransform.Angle = 90;
		break;
	case EMiniMapStartDirection::East:
		break;
	case EMiniMapStartDirection::South:
		OriginalTransform.Angle = -90.f;
		break;
	case EMiniMapStartDirection::West:
		OriginalTransform.Angle = 180.f;
	}
	return OriginalTransform;
}

