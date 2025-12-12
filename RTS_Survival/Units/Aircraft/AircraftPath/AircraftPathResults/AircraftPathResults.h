#pragma once

#include "CoreMinimal.h"

#include "AircraftPathResults.generated.h"

UENUM()
enum class EDirectAttackDiveResult
{
	DivePossible,
	// Within angle but too close.
	TooClose,
	// Within angle but too far.
	TooFar,
	// Not within angle; disregard distance.
	NotWithinAngle
};

static FString Global_GetDirectAttackDiveResultString(const EDirectAttackDiveResult Result)
{
	switch (Result)
	{
	case EDirectAttackDiveResult::DivePossible:
		return TEXT("DivePossible");
	case EDirectAttackDiveResult::TooClose:
		return TEXT("TooClose");
	case EDirectAttackDiveResult::TooFar:
		return TEXT("TooFar");
	case EDirectAttackDiveResult::NotWithinAngle:
		return TEXT("NotWithinAngle");
	}
	return TEXT("UnknownResult");
}
