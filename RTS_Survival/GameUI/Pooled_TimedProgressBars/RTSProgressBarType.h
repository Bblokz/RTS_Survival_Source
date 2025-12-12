#pragma once

#include "CoreMinimal.h"
#include "RTSProgressBarType.generated.h"

UENUM(BlueprintType)
enum class ERTSProgressBarType : uint8
{
	Default  UMETA(DisplayName="Default"),
	Detailed UMETA(DisplayName="Detailed"),
};
