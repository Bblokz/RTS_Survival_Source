#pragma once

#include "CoreMinimal.h"

#include "RTSOutlineRules.generated.h"

UENUM(BlueprintType)
enum class ERTSOutlineRules : uint8
{
	None,
	// Will only show the outline around radixite and metal resources (if hovered)
	RadixiteMetalOnly,
	// Will only show the outline around scavengable actors (if hovered)
	ScavengeOnly,
	// Will only show the outline around capturable actors (if hovered)
	CaptureOnly,
	DoNotShowAnyOutlines
};
