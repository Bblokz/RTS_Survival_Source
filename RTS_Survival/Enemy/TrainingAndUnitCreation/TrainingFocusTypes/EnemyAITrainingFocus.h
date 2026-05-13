#pragma once

#include "CoreMinimal.h"


UENUM(BlueprintType)
enum class EAITrainingFocus : uint8
{
	NoFocus,
	Infantry,
	LightTanks,
	MediumTanks,
	HeavyTanks
};

UENUM(BlueprintType)
enum class EAITrainingFocusSpecialty: uint8
{
	NoSpecialty,
	AntiTank,
	AntiInfantry,
	SpecialWeaponCarrier
};
