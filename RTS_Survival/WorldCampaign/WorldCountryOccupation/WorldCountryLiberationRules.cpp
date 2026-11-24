// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldCountryOccupation/WorldCountryLiberationRules.h"

namespace WorldCountryLiberationRulesConstants
{
	// Twice the triangle area below which a triangle counts as degenerate (square world units).
	constexpr double MinimumDoubledTriangleArea = 1.e-3;
}

namespace WorldCountryLiberationRulesHelpers
{
	double GetSignedDoubledArea(const FVector2D& Origin, const FVector2D& EdgeEnd, const FVector2D& Point)
	{
		return (EdgeEnd.X - Origin.X) * (Point.Y - Origin.Y) - (EdgeEnd.Y - Origin.Y) * (Point.X - Origin.X);
	}
}

bool WorldCountryLiberationRules::TryMakeCountryTriangle2D(const FVector2D& A, const FVector2D& B, const FVector2D& C,
                                                           FWorldCountryTriangle2D& OutTriangle)
{
	const double DoubledArea = WorldCountryLiberationRulesHelpers::GetSignedDoubledArea(A, B, C);
	if (FMath::Abs(DoubledArea) < WorldCountryLiberationRulesConstants::MinimumDoubledTriangleArea)
	{
		return false;
	}

	OutTriangle.A = A;
	OutTriangle.B = B;
	OutTriangle.C = C;
	OutTriangle.Bounds = FBox2D(ForceInit);
	OutTriangle.Bounds += A;
	OutTriangle.Bounds += B;
	OutTriangle.Bounds += C;
	return true;
}

bool WorldCountryLiberationRules::GetIsPointInTriangle2D(const FVector2D& Point,
                                                         const FWorldCountryTriangle2D& Triangle)
{
	if (not Triangle.Bounds.bIsValid || not Triangle.Bounds.IsInsideOrOn(Point))
	{
		return false;
	}

	using WorldCountryLiberationRulesHelpers::GetSignedDoubledArea;
	const double SideAB = GetSignedDoubledArea(Triangle.A, Triangle.B, Point);
	const double SideBC = GetSignedDoubledArea(Triangle.B, Triangle.C, Point);
	const double SideCA = GetSignedDoubledArea(Triangle.C, Triangle.A, Point);
	const bool bHasNegativeSide = SideAB < 0.0 || SideBC < 0.0 || SideCA < 0.0;
	const bool bHasPositiveSide = SideAB > 0.0 || SideBC > 0.0 || SideCA > 0.0;
	return not (bHasNegativeSide && bHasPositiveSide);
}

bool WorldCountryLiberationRules::GetIsPointInCountryTriangles(const FVector2D& Point,
                                                               const TArray<FWorldCountryTriangle2D>& CountryTriangles)
{
	for (const FWorldCountryTriangle2D& CountryTriangle : CountryTriangles)
	{
		if (GetIsPointInTriangle2D(Point, CountryTriangle))
		{
			return true;
		}
	}
	return false;
}

FWorldCountryTargetTally WorldCountryLiberationRules::TallyCountryTargets(
	const TArray<FWorldCountryAnchorTargetInput>& AnchorInputs)
{
	FWorldCountryTargetTally TargetTally;
	for (const FWorldCountryAnchorTargetInput& AnchorInput : AnchorInputs)
	{
		if (not AnchorInput.bIsTarget)
		{
			continue;
		}

		if (AnchorInput.bIsStoryMission)
		{
			TargetTally.StoryMissionsTotal++;
			TargetTally.StoryMissionsCleared += AnchorInput.bIsCleared ? 1 : 0;
			continue;
		}

		TargetTally.OtherTargetsTotal++;
		TargetTally.OtherTargetsCleared += AnchorInput.bIsCleared ? 1 : 0;
	}
	return TargetTally;
}

FWorldCountryMaterialParameterSet WorldCountryLiberationRules::BlendParameterSets(
	const FWorldCountryMaterialParameterSet& OccupiedSet,
	const FWorldCountryMaterialParameterSet& LiberatedSet,
	const float Liberation)
{
	const float BlendAlpha = FMath::Clamp(Liberation, 0.f, 1.f);
	FWorldCountryMaterialParameterSet BlendedSet;

	for (const TPair<FName, FLinearColor>& OccupiedVector : OccupiedSet.VectorValues)
	{
		const FLinearColor* LiberatedVector = LiberatedSet.VectorValues.Find(OccupiedVector.Key);
		if (LiberatedVector == nullptr)
		{
			continue;
		}

		BlendedSet.VectorValues.Add(OccupiedVector.Key,
		                            FMath::Lerp(OccupiedVector.Value, *LiberatedVector, BlendAlpha));
	}

	for (const TPair<FName, float>& OccupiedScalar : OccupiedSet.ScalarValues)
	{
		const float* LiberatedScalar = LiberatedSet.ScalarValues.Find(OccupiedScalar.Key);
		if (LiberatedScalar == nullptr)
		{
			continue;
		}

		BlendedSet.ScalarValues.Add(OccupiedScalar.Key,
		                            FMath::Lerp(OccupiedScalar.Value, *LiberatedScalar, BlendAlpha));
	}

	return BlendedSet;
}
