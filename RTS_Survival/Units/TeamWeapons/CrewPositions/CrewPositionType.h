#pragma once

#include "CoreMinimal.h"
#include "CrewPositionType.generated.h"

UENUM(BlueprintType)
enum class ECrewPositionType : uint8
{
	None UMETA(DisplayName = "None"),
	Gunner UMETA(DisplayName = "Gunner"),
	Loader UMETA(DisplayName = "Loader"),
	Spotter UMETA(DisplayName = "Spotter"),
	AdditionalLoader UMETA(DisplayName = "AdditionalLoader"),
	Commander UMETA(DisplayName = "Commander"),
};
