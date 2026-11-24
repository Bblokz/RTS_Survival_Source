// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "GlassHoverAnimator.generated.h"

class UMaterialInstanceDynamic;

/** How fast a glass element (M_UI_GlassPane or a material with its HLSL) reacts to the mouse. */
USTRUCT(BlueprintType)
struct FGlassHoverTimings
{
	GENERATED_BODY()

	// Seconds for the hover glow to fade in.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float HoverInSeconds = 0.12f;

	// Seconds for the hover glow to fade out.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float HoverOutSeconds = 0.22f;

	// Seconds the light band takes to cross the pane once per mouse enter.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float SweepSeconds = 0.26f;

	// Seconds for the pressed shade to come and go.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.01"))
	float PressSeconds = 0.05f;
};

/**
 * @brief The main UI's glass hover for one glass element: the owner ticks it with the element's hover and press
 * state (pause safe, from NativeTick) and writes the result into the element's dynamic materials. The look lives in
 * the shader's Hover, HoverSweep and Pressed scalars; one light band crosses the glass per mouse enter.
 */
USTRUCT()
struct RTS_SURVIVAL_API FGlassHoverAnimator
{
	GENERATED_BODY()

	/**
	 * @brief Advances the ramps by real time.
	 * @param DeltaSeconds Real seconds since the last tick.
	 * @param bIsHovered Whether the mouse is over the element now.
	 * @param bIsPressed Whether the element is held down now.
	 * @param Timings Fade and sweep durations.
	 * @return True when a value changed enough to be worth writing to the materials.
	 */
	bool Tick(const float DeltaSeconds, const bool bIsHovered, const bool bIsPressed, const FGlassHoverTimings& Timings);

	/** Writes Hover, HoverSweep and Pressed into one dynamic material. */
	void ApplyTo(UMaterialInstanceDynamic& Material) const;

	/** @return The eased hover strength the shader gets (0..1). */
	float GetHover() const;

	/** @return The eased light band progress the shader gets (0..1; 1 when no band plays). */
	float GetSweep() const;

	float GetPressed() const { return M_PressRamp; }

	// For tests and captures: holds the light band at this progress; negative lets it run.
	float FrozenSweepProgress = -1.f;

private:
	// Linear 0..1 ramps; the shader gets them eased.
	float M_HoverRamp = 0.f;
	float M_SweepProgress = 1.f;
	float M_PressRamp = 0.f;

	bool bM_WasHovered = false;

	// Last values handed out, so idle elements skip the parameter writes.
	float M_SentHover = -1.f;
	float M_SentSweep = -1.f;
	float M_SentPressed = -1.f;
};
