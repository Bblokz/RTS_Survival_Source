// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RailwayTurret.h"

float ARailwayTurret::GetMinimumTargetRange() const
{
	return FMath::Max(M_MinimumRange, 0.0f);
}
