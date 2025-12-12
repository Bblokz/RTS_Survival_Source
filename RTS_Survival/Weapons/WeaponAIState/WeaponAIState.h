#pragma once


#include "CoreMinimal.h"


UENUM()
enum class EWeaponAIState : uint8
{
	None,
	AutoEngage,
	SpecificEngage,
	TargetGround
};