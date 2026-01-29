#include "WeaponRangeData.h"

#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

void FWeaponRangeData::AdjustRangeForWeapon(const UWeaponState* WeaponState)
{
	if (not IsValid(WeaponState))
	{
		return;
	}

	AdjustRangeForNewWeapon(WeaponState->GetWeaponRangeBehaviourAdjusted());
}

void FWeaponRangeData::RecalculateRangeFromWeapons(const TArray<UWeaponState*>& Weapons)
{
	M_MaxWeaponRange = 0.0f;
	M_MaxWeaponRangeSquared = 0.0f;
	M_WeaponSearchRadius = 0.0f;

	for (const UWeaponState* WeaponState : Weapons)
	{
		AdjustRangeForWeapon(WeaponState);
	}
}
