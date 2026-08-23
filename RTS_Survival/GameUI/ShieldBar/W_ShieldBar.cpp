// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_ShieldBar.h"

void UW_ShieldBar::UpdateShieldValues(const float CurrentShields, const float MaxShields)
{
	M_MaxShields = FMath::Max(0.0f, MaxShields);
	M_CurrentShields = FMath::Clamp(CurrentShields, 0.0f, M_MaxShields);
	BP_OnShieldValuesChanged(M_CurrentShields, M_MaxShields, GetShieldPercentage());
}

float UW_ShieldBar::GetShieldPercentage() const
{
	if (M_MaxShields <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return M_CurrentShields / M_MaxShields;
}
