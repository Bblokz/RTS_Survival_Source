#pragma once

#include "CoreMinimal.h"

#include "WeaponRangeData.generated.h"

class AActor;
class UWeaponState;

/**
 * @brief Contains the range, squared range and the search range for a weapon
 * @pre Provide AdjustRangeForNewWeapon(const float Range) to see if the radii need to be
 * adjusted depending on the new weapon range.

 */
USTRUCT()
struct FWeaponRangeData
{
	GENERATED_BODY()

	/** @return Whether ground weapons engaging this actor get the anti-air range multiplier. */
	static bool GetIsAntiAirRangeTarget(const AActor* Target);

	/** @return Anti-air range multiplier for aircraft targets, 1 otherwise. */
	static float GetRangeMltForTarget(const bool bIsAircraftTarget);

	/** @return The squared engage range; aircraft targets are reached further out than ground targets. */
	float GetMaxRangeSquaredForTarget(const bool bIsAircraftTarget) const;

	inline void AdjustRangeForNewWeapon(const float Range)
	{
		if (Range > M_MaxWeaponRange)
		{
			M_MaxWeaponRange = Range;
			M_MaxWeaponRangeSquared = Range * Range;
			M_WeaponSearchRadius = Range;
		}		
	};

	void AdjustRangeForWeapon(const UWeaponState* WeaponState);

	void RecalculateRangeFromWeapons(const TArray<UWeaponState*>& Weapons);

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
