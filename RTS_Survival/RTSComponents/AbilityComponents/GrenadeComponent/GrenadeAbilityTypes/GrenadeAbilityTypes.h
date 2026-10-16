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
};
