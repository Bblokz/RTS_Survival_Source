// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/StripFlight/GA_StripFlight.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareProjectile.h"
#include "GA_FlareCurtain.generated.h"

USTRUCT(BlueprintType)
struct FFlareCurtainSettings
{
	GENERATED_BODY()

	// Height above the ground at which a dropped flare ignites; keep it below the RTS camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100"))
	float IgnitionHeight = 1200.f;

	// Seconds a flare burns and gives vision while it sinks to the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float HangTimeSeconds = 30.f;

	// Fall speed of the unlit cartridge from the bomber to its ignition height, cm/s.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100"))
	float FallSpeed = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFlareVisualSettings Visuals;
};

/**
 * @brief A bomber flies the strip and drops evenly spaced parachute flares that light it up. The number of flares is
 * the aim area's ShaderMarkerCount and their vision radius its AreaRadius, so the target area shows the real curtain.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_FlareCurtain : public UGA_StripFlight
{
	GENERATED_BODY()

protected:
	virtual void OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
	                                   const float NewDistance) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FFlareCurtainSettings M_FlareCurtainSettings;

	void DropFlare(const FStripFlightRun& Run, const FVector& LocationOnStrip) const;
};
