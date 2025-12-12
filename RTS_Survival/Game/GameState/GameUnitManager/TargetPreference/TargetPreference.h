#pragma once
#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class ETargetPreference : uint8
{
	None,
	Infantry,
	Tank,
	Building,
	Other
};

static FString Global_GetTargetPreferenceAsString(const ETargetPreference Preference)
{
	switch (Preference)
	{
	case ETargetPreference::None:
		return "None";
	case ETargetPreference::Infantry:
		return "Infantry";
	case ETargetPreference::Tank:
		return "Tank";
	case ETargetPreference::Building:
		return "Building";
	case ETargetPreference::Other:
		return "Other";
	}
	return "None";
}
