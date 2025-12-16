// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "BehaviourIcon.generated.h"

UENUM()
enum class EBehaviourIcon : uint8
{
	None,
	RifleAccuracy,
	LifeRegen,
	ArmorPiercingBoost,
	InCoverResistanceBoost,
};
