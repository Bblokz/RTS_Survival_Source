// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitAnimInstance.h"

void UGunPitAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The curve is from the last evaluated pose; one frame of delay is invisible on a slow loading blend.
	const float LoadingElevationWeight = LoadingElevationCurveName.IsNone()
		                                     ? 0.f
		                                     : FMath::Clamp(GetCurveValue(LoadingElevationCurveName), 0.f, 1.f);
	AimPitchAlpha = 1.f - LoadingElevationWeight;
	TurntableYawRotation = FRotator(0.f, Yaw, 0.f);
	CradlePitchRotation = FRotator(Pitch, 0.f, 0.f);
}
