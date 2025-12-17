#pragma once

#include "CoreMinimal.h"

#include "BehaviourAbilityTypes.generated.h"

UENUM(BlueprintType)
enum class EBehaviourAbilityType : uint8
{
	// This will use the default icon and description from the Action UI data table.
	// Which is the implementation for the sprint ability.
	DefaultSprint,
	WeakSpotAim,
	VultureRapidFire,
	EnhancedShells,
	
};
