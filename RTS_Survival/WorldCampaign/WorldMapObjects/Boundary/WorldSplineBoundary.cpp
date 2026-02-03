// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldMapObjects/Boundary/WorldSplineBoundary.h"

#include "Components/SplineComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace
{
	constexpr float DuplicatePointMinDistance = 1.f;
	constexpr float DuplicatePointSpacingScale = 0.1f;
}

AWorldSplineBoundary::AWorldSplineBoundary()
{
	PrimaryActorTick.bCanEverTick = false;
	M_BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
	SetRootComponent(M_BoundarySpline);
}

USplineComponent* AWorldSplineBoundary::GetBoundarySpline() const
{
	return M_BoundarySpline;
}

void AWorldSplineBoundary::EnsureClosedLoop()
{
	if (not GetIsValidBoundarySpline())
	{
		return;
	}

	if (M_BoundarySpline->IsClosedLoop())
	{
		return;
	}

	M_BoundarySpline->SetClosedLoop(true);
	M_BoundarySpline->UpdateSpline();
}

void AWorldSplineBoundary::GetSampledPolygon2D(float SampleSpacing, TArray<FVector2D>& OutPolygon) const
{
	OutPolygon.Reset();

	if (not GetIsValidBoundarySpline())
	{
		return;
	}

	if (SampleSpacing <= 0.f)
	{
		return;
	}

	const float SplineLength = M_BoundarySpline->GetSplineLength();
	if (SplineLength <= 0.f)
	{
		return;
	}

	const float DuplicateDistanceThreshold = FMath::Max(SampleSpacing * DuplicatePointSpacingScale,
	                                                    DuplicatePointMinDistance);
	const float DuplicateDistanceSquared = FMath::Square(DuplicateDistanceThreshold);

	auto AppendPointIfDistinct = [&OutPolygon, DuplicateDistanceSquared](const FVector2D& Candidate)
	{
		const int32 CurrentCount = OutPolygon.Num();
		if (CurrentCount > 0)
		{
			const float DistanceSquared = FVector2D::DistSquared(OutPolygon.Last(), Candidate);
			if (DistanceSquared <= DuplicateDistanceSquared)
			{
				return;
			}
		}

		OutPolygon.Add(Candidate);
	};

	for (float Distance = 0.f; Distance < SplineLength; Distance += SampleSpacing)
	{
		const FVector Location = M_BoundarySpline->GetLocationAtDistanceAlongSpline(Distance,
		                                                                            ESplineCoordinateSpace::World);
		AppendPointIfDistinct(FVector2D(Location.X, Location.Y));
	}

	const FVector EndLocation = M_BoundarySpline->GetLocationAtDistanceAlongSpline(SplineLength,
	                                                                               ESplineCoordinateSpace::World);
	AppendPointIfDistinct(FVector2D(EndLocation.X, EndLocation.Y));

	if (OutPolygon.Num() >= 2)
	{
		const float StartDistanceSquared = FVector2D::DistSquared(OutPolygon[0], OutPolygon.Last());
		if (StartDistanceSquared <= DuplicateDistanceSquared)
		{
			OutPolygon.RemoveAt(OutPolygon.Num() - 1);
		}
	}

	if (OutPolygon.Num() < 3)
	{
		OutPolygon.Reset();
	}
}

bool AWorldSplineBoundary::GetIsValidBoundarySpline() const
{
	if (IsValid(M_BoundarySpline))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, TEXT("M_BoundarySpline"),
	                                                      TEXT("AWorldSplineBoundary::GetIsValidBoundarySpline"),
	                                                      this);
	return false;
}
