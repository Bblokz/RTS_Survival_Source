// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GunPitSectorArcWatch.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

void FGunPitSectorArcWatch::Sample(const float GunYawFromPitForward, const bool bShotFired)
{
	LastGunYawFromPitForward = GunYawFromPitForward;
	LargestGunYawFromPitForward = FMath::Max(LargestGunYawFromPitForward, FMath::Abs(GunYawFromPitForward));
	ShotsOutsideArc += bShotFired ? 1 : 0;
	++Samples;
}

bool FGunPitSectorArcWatch::GetHoldsAtLimit(const float SectorHalfDegrees) const
{
	return Samples > 0 && FMath::Abs(FMath::Abs(LastGunYawFromPitForward) - SectorHalfDegrees) <= LimitToleranceDegrees;
}

bool FGunPitSectorArcWatch::GetStaysInsideArc(const float SectorHalfDegrees) const
{
	return LargestGunYawFromPitForward <= SectorHalfDegrees + LimitToleranceDegrees;
}

FString FGunPitSectorArcWatch::Describe() const
{
	return FString::Printf(TEXT("gun_yaw=%.1f largest_yaw=%.1f shots=%d samples=%d"), LastGunYawFromPitForward,
	                       LargestGunYawFromPitForward, ShotsOutsideArc, Samples);
}

#endif
