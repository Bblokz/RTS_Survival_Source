// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/StripFlight/GA_StripFlight.h"
#include "GA_ReconFlight.generated.h"

USTRUCT(BlueprintType)
struct FReconFlightSettings
{
	GENERATED_BODY()

	// Distance flown between two vision bubbles; keep it below the strip width or the revealed strip has gaps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "100.0"))
	float RevealSpacing = 1200.0f;

	// How long the fog stays lifted behind the aircraft.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float RevealHoldSeconds = 25.0f;
};

/**
 * @brief An unarmed aircraft flies the strip and lifts the fog behind it; the reveal radius is the aim area's
 * AreaRadius, so the target area shows what will be seen. Shooting it down ends the reveal where it fell.
 */
UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced)
class RTS_SURVIVAL_API UGA_ReconFlight : public UGA_StripFlight
{
	GENERATED_BODY()

protected:
	virtual void OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
	                                   const float NewDistance) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FReconFlightSettings M_ReconSettings;

	void SpawnVisionBubble(const FVector& LocationOnStrip) const;
};
