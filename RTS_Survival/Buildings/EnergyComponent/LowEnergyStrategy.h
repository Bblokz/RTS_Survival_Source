// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "LowEnergyStrategy.generated.h"

UENUM(BlueprintType)
enum class ELowEnergyStrategy : uint8
{
	LES_Invalid,
	LES_None,
	LES_ApplyBehaviourOnly,
	LES_ChangeMaterialsOnly,
	LES_ApplyBehaviourAndChangeMaterials,
};
