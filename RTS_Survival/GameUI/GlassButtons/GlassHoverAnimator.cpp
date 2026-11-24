// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GlassHoverAnimator.h"

#include "Materials/MaterialInstanceDynamic.h"

namespace GlassHoverAnimatorConstants
{
	const FName HoverParameter(TEXT("Hover"));
	const FName HoverSweepParameter(TEXT("HoverSweep"));
	const FName PressedParameter(TEXT("Pressed"));
	constexpr float MinParameterChange = 0.001f;
}

bool FGlassHoverAnimator::Tick(const float DeltaSeconds, const bool bIsHovered, const bool bIsPressed,
                               const FGlassHoverTimings& Timings)
{
	using namespace GlassHoverAnimatorConstants;
	if (bIsHovered && not bM_WasHovered)
	{
		// One light band across the glass per mouse enter.
		M_SweepProgress = 0.f;
	}
	bM_WasHovered = bIsHovered;
	const float HoverSeconds = bIsHovered ? Timings.HoverInSeconds : Timings.HoverOutSeconds;
	M_HoverRamp = FMath::FInterpConstantTo(M_HoverRamp, bIsHovered ? 1.f : 0.f, DeltaSeconds, 1.f / HoverSeconds);
	M_SweepProgress = FrozenSweepProgress >= 0.f
		                  ? FrozenSweepProgress
		                  : FMath::Min(1.f, M_SweepProgress + DeltaSeconds / Timings.SweepSeconds);
	M_PressRamp = FMath::FInterpConstantTo(M_PressRamp, bIsPressed ? 1.f : 0.f, DeltaSeconds,
	                                       1.f / Timings.PressSeconds);

	const bool bChanged = not FMath::IsNearlyEqual(GetHover(), M_SentHover, MinParameterChange)
		|| not FMath::IsNearlyEqual(GetSweep(), M_SentSweep, MinParameterChange)
		|| not FMath::IsNearlyEqual(M_PressRamp, M_SentPressed, MinParameterChange);
	if (bChanged)
	{
		M_SentHover = GetHover();
		M_SentSweep = GetSweep();
		M_SentPressed = M_PressRamp;
	}
	return bChanged;
}

void FGlassHoverAnimator::ApplyTo(UMaterialInstanceDynamic& Material) const
{
	using namespace GlassHoverAnimatorConstants;
	Material.SetScalarParameterValue(HoverParameter, GetHover());
	Material.SetScalarParameterValue(HoverSweepParameter, GetSweep());
	Material.SetScalarParameterValue(PressedParameter, M_PressRamp);
}

float FGlassHoverAnimator::GetHover() const
{
	return FMath::SmoothStep(0.f, 1.f, M_HoverRamp);
}

float FGlassHoverAnimator::GetSweep() const
{
	// Ease out: the band enters quickly and settles.
	return 1.f - FMath::Square(1.f - M_SweepProgress);
}
