#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EAimAbilityType : uint8
{
	DefaultBrummbarFire,
	PanzerIVArtilleryExplosiveShot,
	HeArtilleryBarrage,
	SmallRocketBarrage,
	MortarBarrage,
	// Fires a parachute flare that lights up an area in the fog of war (UFlareAimAbilityComponent).
	// The values are card keys in Buttons_DataTableb (IdAimAbility / IdCancelAimAbility), so only append.
	FlareLaunch,
	// Unit variant batch 4: JagdPanther Bollwerk's sFH 18 HE barrage; its own card row and icon.
	JagdPantherBollwerkBarrage,
};
