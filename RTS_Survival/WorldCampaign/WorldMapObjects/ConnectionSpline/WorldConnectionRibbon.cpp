// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldConnectionRibbon.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

AWorldConnectionRibbon::AWorldConnectionRibbon()
{
	PrimaryActorTick.bCanEverTick = false;

	M_Spline = CreateDefaultSubobject<USplineComponent>(TEXT("RibbonSpline"));
	SetRootComponent(M_Spline);
	if (IsValid(M_Spline))
	{
		M_Spline->CastShadow = false;
		M_Spline->bCastDynamicShadow = false;
	}
}

void AWorldConnectionRibbon::BuildRibbon(const TArray<FVector>& WorldPathPoints, const FWorldConnectionRibbonStyle& Style)
{
	ClearSplineMeshes();

	if (not IsValid(M_Spline))
	{
		return;
	}

	if (WorldPathPoints.Num() < 2)
	{
		return;
	}

	// Rebuild the underlying spline from the solved world-space path.
	M_Spline->ClearSplinePoints(false);
	for (const FVector& PathPoint : WorldPathPoints)
	{
		M_Spline->AddSplinePoint(PathPoint, ESplineCoordinateSpace::World, false);
	}
	M_Spline->UpdateSpline();
	M_Spline->bReceivesDecals = Style.bAllowDecals;

	if (not IsValid(Style.Mesh))
	{
		// Nothing to render without a mesh; the spline still defines the (invisible) path.
		return;
	}

	const float SafeMeshWidth = FMath::Max(Style.MeshWidthAtUnitScale, KINDA_SMALL_NUMBER);
	const float WidthScale = Style.Width / SafeMeshWidth;
	const int32 NumPoints = M_Spline->GetNumberOfSplinePoints();

	for (int32 SegmentIndex = 0; SegmentIndex < NumPoints - 1; ++SegmentIndex)
	{
		USplineMeshComponent* SplineMesh = NewObject<USplineMeshComponent>(this);
		if (not IsValid(SplineMesh))
		{
			continue;
		}

		SplineMesh->SetMobility(EComponentMobility::Movable);
		SplineMesh->RegisterComponent();
		SplineMesh->AttachToComponent(M_Spline, FAttachmentTransformRules::KeepRelativeTransform);
		SplineMesh->SetForwardAxis(Style.ForwardAxis, false);
		SplineMesh->SetStaticMesh(Style.Mesh);
		if (IsValid(Style.Material))
		{
			SplineMesh->SetMaterial(0, Style.Material);
		}

		const FVector StartPos = M_Spline->GetLocationAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local);
		const FVector EndPos = M_Spline->GetLocationAtSplinePoint(SegmentIndex + 1, ESplineCoordinateSpace::Local);
		const FVector StartTangent = M_Spline->GetTangentAtSplinePoint(SegmentIndex, ESplineCoordinateSpace::Local);
		const FVector EndTangent = M_Spline->GetTangentAtSplinePoint(SegmentIndex + 1, ESplineCoordinateSpace::Local);
		SplineMesh->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, false);
		SplineMesh->SetStartScale(FVector2D(WidthScale, WidthScale), false);
		SplineMesh->SetEndScale(FVector2D(WidthScale, WidthScale), false);
		SplineMesh->UpdateMesh();

		SplineMesh->SetCastShadow(Style.bCastShadow);
		SplineMesh->bCastDynamicShadow = Style.bCastShadow;
		SplineMesh->SetReceivesDecals(Style.bAllowDecals);
		FRTS_CollisionSetup::SetupSplineNoCollision(SplineMesh);

		M_SplineMeshes.Add(SplineMesh);
	}
}

void AWorldConnectionRibbon::ClearSplineMeshes()
{
	for (const TObjectPtr<USplineMeshComponent>& SplineMesh : M_SplineMeshes)
	{
		if (IsValid(SplineMesh))
		{
			SplineMesh->DestroyComponent();
		}
	}

	M_SplineMeshes.Reset();
}
