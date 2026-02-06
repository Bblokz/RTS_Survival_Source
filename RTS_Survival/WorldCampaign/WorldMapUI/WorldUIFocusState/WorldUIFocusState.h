#pragma once

#include "CoreMinimal.h"

#include "WorldUIFocusState.generated.h"

UENUM(BlueprintType)
enum class EWorldUIFocusState : uint8
{
	None,
	CommandPerks,
	ArmyLayout,
	World,
	Archive,
	TechTree
};
