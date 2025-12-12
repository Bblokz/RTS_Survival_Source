#pragma once

#include "CoreMinimal.h"

#include "WeaponRangeData.generated.h"

/**
 * @brief Contains the range, squared range and the search range for a weapon
 * @pre Provide AdjustRangeForNewWeapon(const float Range) to see if the radii need to be
 * adjusted depending on the new weapon range.
 
 */
USTRUCT()
struct FWeaponRangeData
{
	GENERATED_BODY()

	inline void AdjustRangeForNewWeapon(const float Range)
	{
		if (Range > M_MaxWeaponRange)
		{
			M_MaxWeaponRange = Range;
			M_MaxWeaponRangeSquared = Range * Range;
			M_WeaponSearchRadius = Range;
		}		
	};

	inline void ForceSetRange(const float Range)
	{
		M_MaxWeaponRange = Range;
		M_MaxWeaponRangeSquared = Range * Range;
		M_WeaponSearchRadius = Range;
	};	

	UPROPERTY()
	float M_MaxWeaponRange = 0.f;

	UPROPERTY()
	float M_MaxWeaponRangeSquared = 0.f;

	UPROPERTY()
	float M_WeaponSearchRadius = 0.f;

};
