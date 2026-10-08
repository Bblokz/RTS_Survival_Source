#include "CoverCombatScoring.h"

float FRTSCombatCoverScoring::GetYawToLocationDegrees(const FRTSCoverPoint& CoverPoint, const FVector& Location)
{
	// The normal points from the cover to the soldier, so a soldier in cover looks against it.
	const FVector TowardCover = -CoverPoint.CoverNormal.GetSafeNormal2D();
	const FVector ToLocation = (Location - CoverPoint.Location).GetSafeNormal2D();
	if (TowardCover.IsNearlyZero() || ToLocation.IsNearlyZero())
	{
		return 0.0f;
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, TowardCover);
	return FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(ToLocation, Right),
		FVector::DotProduct(ToLocation, TowardCover)));
}

bool FRTSCombatCoverScoring::GetCanAimAt(
	const FRTSCoverPoint& CoverPoint,
	const FVector& TargetLocation,
	const FRTSCombatCoverSettings& Settings)
{
	// A target on top of the point has no direction to aim in.
	if (FVector::DistSquared2D(CoverPoint.Location, TargetLocation) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	return FMath::Abs(GetYawToLocationDegrees(CoverPoint, TargetLocation)) <= Settings.MaximumAimYawDegrees;
}

float FRTSCombatCoverScoring::GetProtectedThreatFraction(
	const FRTSCoverPoint& CoverPoint,
	const FRTSCombatCoverThreats& Threats,
	const FRTSCombatCoverSettings& Settings)
{
	if (Threats.ThreatLocations.IsEmpty())
	{
		return 0.0f;
	}
	int32 ProtectedThreatCount = 0;
	for (const FVector& ThreatLocation : Threats.ThreatLocations)
	{
		const bool bCoverIsBetween = FMath::Abs(GetYawToLocationDegrees(CoverPoint, ThreatLocation)) <=
			Settings.ProtectedHalfAngleDegrees;
		ProtectedThreatCount += bCoverIsBetween ? 1 : 0;
	}
	return static_cast<float>(ProtectedThreatCount) / static_cast<float>(Threats.ThreatLocations.Num());
}

bool FRTSCombatCoverScoring::TryScoreCoverPoint(
	const FRTSCoverPoint& CoverPoint,
	const FVector& UnitLocation,
	const FRTSCombatCoverThreats& Threats,
	const FRTSCombatCoverSettings& Settings,
	float& OutScore)
{
	if (not GetCanAimAt(CoverPoint, Threats.PrimaryTargetLocation, Settings))
	{
		return false;
	}
	// 1 when the target is straight behind the cover, 0 when it is level with the wall.
	const float PrimaryYawRadians = FMath::DegreesToRadians(
		GetYawToLocationDegrees(CoverPoint, Threats.PrimaryTargetLocation));
	const float FacingQuality = FMath::Max(0.0f, FMath::Cos(PrimaryYawRadians));
	const float TravelCost = FMath::Clamp(
		FVector::Dist2D(UnitLocation, CoverPoint.Location) / FMath::Max(1.0f, Settings.TravelReferenceDistance),
		0.0f,
		1.0f);
	OutScore = Settings.ProtectionWeight * GetProtectedThreatFraction(CoverPoint, Threats, Settings)
		+ Settings.FacingWeight * FacingQuality
		- Settings.TravelWeight * TravelCost;
	return true;
}

ERTSCoverInvalidTargetDecision FRTSCombatCoverScoring::DecideOnInvalidTarget(
	const bool bTargetIsAttackOrder,
	const float SecondsWithoutValidTarget,
	const float ToleranceSeconds)
{
	const bool bMustLeave = bTargetIsAttackOrder || ToleranceSeconds <= 0.0f ||
		SecondsWithoutValidTarget >= ToleranceSeconds;
	return bMustLeave ? ERTSCoverInvalidTargetDecision::LeaveCover : ERTSCoverInvalidTargetDecision::StayInCover;
}

bool FRTSCombatCoverScoring::GetIsWorthRepositioning(
	const float CurrentPointScore,
	const float CandidateScore,
	const FRTSCombatCoverSettings& Settings)
{
	return CandidateScore >= CurrentPointScore + Settings.MinimumScoreGain;
}
