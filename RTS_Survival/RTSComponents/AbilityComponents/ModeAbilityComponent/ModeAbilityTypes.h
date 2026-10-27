// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModeAbilityTypes.generated.h"

UENUM(BlueprintType)
enum class EModeAbilityType : uint8
{
	DefaultSniperOverwatch,
	ArtilleryMode,
	// Added at runtime by Tech_Flak88_GroundRole on the 8.8 cm Flak team weapon.
	Flak88GroundRole,
	// Unit variant batch 4. The values are card keys in Buttons_DataTableb (IdActivateMode / IdDisableMode), so
	// only append.
	// Wespe Camouflaged Nest: dig in under the camo nets; immobile with more range until the mode is left.
	WespeCamouflagedNest
};
