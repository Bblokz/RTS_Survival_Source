#pragma once


#include "CoreMinimal.h"

#include "WeaponDescriptionType.generated.h"

UENUM()
enum class EWeaponDescriptionType :uint8
{
	Regular,
	BombComponent,
	FlameWeapon,
	LaserWeapon,
};
