#pragma once

#include "CoreMinimal.h"
#include "RemoveDontLoseCommanderPost.generated.h"

/** Determines what the mission manager does after removing the active commander-loss mission. */
UENUM(BlueprintType)
enum class ERemoveDontLoseCommanderPost : uint8
{
	NoAction UMETA(DisplayName = "No Action"),
	ActivateNewMission UMETA(DisplayName = "Activate New Mission")
};
