#pragma once

#include "CoreMinimal.h"

#include "RTSPrimaryClickContext.generated.h"

UENUM()
enum class ERTSPrimaryClickContext
{
	None,
	RegularPrimaryClick,
	SecondaryActive,
	FormationTypePrimaryClick
};
