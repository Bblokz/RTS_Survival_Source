#pragma once

#include "CoreMinimal.h"
#include "RTSGroundFireSettings.generated.h"

/**
 * @brief Per-weapon lingering ground fire left by an incendiary projectile on impact.
 * Off by default: a weapon only burns the ground when radius, duration and damage per second are all above zero.
 * The area is owned by the fire subsystem (URTSFireSubsystem::RequestGroundFire), which pools the Niagara fire,
 * merges overlapping fires of the same player, caps the number of fires and deals all burn damage from one tick.
 */
USTRUCT(BlueprintType)
struct FRTSGroundFireSettings
{
	GENERATED_BODY()

	// Radius of the burning area in cm. 0 disables the ground fire.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float RadiusCm = 0.f;

	// Seconds the ground keeps burning after the impact. 0 disables the ground fire.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float DurationSeconds = 0.f;

	// Fire damage per second to every unit of the other side inside the radius. 0 disables the ground fire.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float DamagePerSecond = 0.f;

	// Exponent of the damage falloff towards the edge; 0 burns evenly over the whole area.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float DamageFalloffExponent = 0.f;

	// Rear armor that still takes the full burn damage.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float FullArmorPen = 10.f;

	// How quickly the burn damage drops as the rear armor approaches MaxArmorPen.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float ArmorPenFallOff = 5.f;

	// Rear armor at which the burn does no damage.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float MaxArmorPen = 20.f;

	// Value of the pooled fire's Niagara "Scale" user parameter (flame size); the spread follows the radius.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Ground Fire")
	float VfxScale = 1.f;

	bool GetIsEnabled() const
	{
		return RadiusCm > 0.f && DurationSeconds > 0.f && DamagePerSecond > 0.f;
	}
};
