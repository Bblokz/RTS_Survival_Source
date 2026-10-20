#pragma once

#include "CoreMinimal.h"

#include "GrenadeAbilityTypes.generated.h"

UENUM(BlueprintType)
enum class EGrenadeAbilityType : uint8
{
	DefaultGerBundleGrenade,
	GerAtGrenade,
	GerStickGrenade,
	SmallGrenade,
	BundleSturmKommando,
	StickGrenadeVultures,
	// Soviet molotov cocktail: burning rag in hand and a fire area that keeps burning after impact.
	RusMolotov,
	// Soviet WW2 grenades. The values are card keys in Buttons_DataTableb, so new types are only appended.
	// RGD-33 stick grenade with its fragmentation sleeve: the assault squads' anti-infantry grenade.
	RusRGD33,
	// F-1 "limonka" defensive grenade: wide fragment radius, lower damage per soldier.
	RusF1,
	// RG-42 offensive tin-can grenade: short fuse, tight blast.
	RusRG42,
	// RPG-43 HEAT hand grenade: sticks to vehicles and hits their rear armor.
	RusRPG43,
};
