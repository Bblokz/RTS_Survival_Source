#include "WeaponRangeData.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

bool FWeaponRangeData::GetIsAntiAirRangeTarget(const AActor* Target)
{
	return IsValid(Target) && Target->IsA<AAircraftMaster>();
}

float FWeaponRangeData::GetRangeMltForTarget(const bool bIsAircraftTarget)
{
	return bIsAircraftTarget ? DeveloperSettings::GameBalance::Ranges::AntiAirRangeMlt : 1.0f;
}

float FWeaponRangeData::GetMaxRangeSquaredForTarget(const bool bIsAircraftTarget) const
{
	return M_MaxWeaponRangeSquared * FMath::Square(GetRangeMltForTarget(bIsAircraftTarget));
}

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
