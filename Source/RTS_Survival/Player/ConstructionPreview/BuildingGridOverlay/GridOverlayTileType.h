#pragma once

#include "CoreMinimal.h"

#include "GridOverlayTileType.generated.h"

UENUM(Blueprintable)
enum class EGridOverlayTileType : uint8
{
	Vacant,
	Valid,
	Invalid
};
