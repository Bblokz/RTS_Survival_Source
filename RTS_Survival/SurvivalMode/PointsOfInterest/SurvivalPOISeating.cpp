// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPOISeating.h"

#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"

namespace SurvivalPOISeatingConstants
{
	constexpr int32 GridSamples = 5;
	constexpr int32 QuadrantCount = 4;
	// Components farther than this from the pivot or larger than this are hidden helpers, not the visible object.
	constexpr double MaxComponentDistance = 6000.0;
	constexpr double MaxComponentExtent = 8000.0;
	constexpr float TraceBelowGround = 400.f;
	constexpr float TraceAboveTop = 300.f;
	constexpr float MaxSinkCm = 30.f;
	// Samples on the very edge of the box rarely hit; pull them in a little.
	constexpr float EdgeInset = 0.08f;
}

namespace
{
	bool SurvivalPOISeating_IsVisibleMesh(const UPrimitiveComponent* Component)
	{
		if (not IsValid(Component) or not Component->IsVisible() or Component->bHiddenInGame)
		{
			return false;
		}
		if (Component->IsA<UInstancedStaticMeshComponent>())
		{
			return false;
		}
		if (const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Component))
		{
			return IsValid(StaticMesh->GetStaticMesh());
		}
		// Many wreck hulls are geometry collections.
		return Component->IsA<USkeletalMeshComponent>() or Component->IsA<UGeometryCollectionComponent>();
	}

	void SurvivalPOISeating_GetVisibleComponents(const AActor& Actor, TArray<UPrimitiveComponent*>& OutComponents)
	{
		TArray<UPrimitiveComponent*> Components;
		Actor.GetComponents<UPrimitiveComponent>(Components);
		for (UPrimitiveComponent* Component : Components)
		{
			if (SurvivalPOISeating_IsVisibleMesh(Component))
			{
				OutComponents.Add(Component);
			}
		}
	}

	/** @return The lowest underside point of the visible meshes above XY, by tracing up from below the ground. */
	bool SurvivalPOISeating_TraceUnderside(const TArray<UPrimitiveComponent*>& Components, const FVector2D& Position,
	                                       const float GroundZ, const float TopZ, float& OutUndersideZ)
	{
		using namespace SurvivalPOISeatingConstants;
		const FVector Start(Position, GroundZ - TraceBelowGround);
		const FVector End(Position, TopZ + TraceAboveTop);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SurvivalPOIUnderside), true);
		bool bHit = false;
		OutUndersideZ = TNumericLimits<float>::Max();
		for (UPrimitiveComponent* Component : Components)
		{
			FHitResult Hit;
			if (Component->LineTraceComponent(Hit, Start, End, Params))
			{
				OutUndersideZ = FMath::Min(OutUndersideZ, static_cast<float>(Hit.ImpactPoint.Z));
				bHit = true;
			}
		}
		return bHit;
	}

	float SurvivalPOISeating_GroundAt(const FSurvivalGroundProjector& Projector, const FVector2D& Position, const float Fallback)
	{
		float Height = Fallback;
		Projector.TryGetHeight(Position, Height);
		return Height;
	}
}

namespace SurvivalPOISeating
{
	bool GetVisibleLocalBox(const AActor& Actor, FBox& OutLocalBox)
	{
		using namespace SurvivalPOISeatingConstants;
		OutLocalBox = FBox(ForceInit);
		const FTransform WorldToActor = Actor.GetActorTransform().Inverse();
		TArray<UPrimitiveComponent*> Components;
		SurvivalPOISeating_GetVisibleComponents(Actor, Components);
		for (const UPrimitiveComponent* Component : Components)
		{
			FBox LocalBox(ForceInit);
			if (const UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Component))
			{
				const FTransform ComponentToActor = StaticMesh->GetComponentTransform() * WorldToActor;
				LocalBox = StaticMesh->GetStaticMesh()->GetBoundingBox().TransformBy(ComponentToActor);
			}
			else if (Component->IsA<UGeometryCollectionComponent>())
			{
				LocalBox = Component->CalcBounds(Component->GetComponentTransform() * WorldToActor).GetBox();
			}
			else
			{
				LocalBox = Component->Bounds.GetBox().TransformBy(WorldToActor);
			}
			if (not LocalBox.IsValid or LocalBox.GetCenter().Size() > MaxComponentDistance
				or LocalBox.GetExtent().GetMax() > MaxComponentExtent)
			{
				continue;
			}
			OutLocalBox += LocalBox;
		}
		return OutLocalBox.IsValid != 0;
	}

	void MeasureSeat(const AActor& Actor, const FSurvivalGroundProjector& Projector, FSurvivalPOISeatReport& OutReport)
	{
		using namespace SurvivalPOISeatingConstants;
		OutReport = FSurvivalPOISeatReport();
		FBox LocalBox;
		if (not GetVisibleLocalBox(Actor, LocalBox))
		{
			return;
		}
		OutReport.bMeasured = true;
		OutReport.LocalBoxMin = LocalBox.Min;
		OutReport.LocalBoxMax = LocalBox.Max;
		TArray<UPrimitiveComponent*> Components;
		SurvivalPOISeating_GetVisibleComponents(Actor, Components);
		const FTransform ActorTransform = Actor.GetActorTransform();
		const float TopZ = ActorTransform.TransformPosition(FVector(LocalBox.GetCenter().X, LocalBox.GetCenter().Y, LocalBox.Max.Z)).Z;

		OutReport.QuadrantGaps.Init(TNumericLimits<float>::Max(), QuadrantCount);
		OutReport.ContactPoints.Init(FVector::ZeroVector, QuadrantCount);
		OutReport.GroundHeights.Init(0.f, QuadrantCount);
		for (int32 X = 0; X < GridSamples; ++X)
		{
			for (int32 Y = 0; Y < GridSamples; ++Y)
			{
				const float AlphaX = FMath::Lerp(EdgeInset, 1.f - EdgeInset, X / static_cast<float>(GridSamples - 1));
				const float AlphaY = FMath::Lerp(EdgeInset, 1.f - EdgeInset, Y / static_cast<float>(GridSamples - 1));
				const FVector Local(FMath::Lerp(LocalBox.Min.X, LocalBox.Max.X, AlphaX), FMath::Lerp(LocalBox.Min.Y, LocalBox.Max.Y, AlphaY),
				                    LocalBox.Min.Z);
				const FVector World = ActorTransform.TransformPosition(Local);
				const float GroundZ = SurvivalPOISeating_GroundAt(Projector, FVector2D(World), World.Z);
				float UndersideZ = 0.f;
				if (not SurvivalPOISeating_TraceUnderside(Components, FVector2D(World), GroundZ, TopZ, UndersideZ))
				{
					continue;
				}
				++OutReport.UndersideHits;
				const int32 Quadrant = AlphaX < 0.5f ? (AlphaY < 0.5f ? 0 : 3) : (AlphaY < 0.5f ? 1 : 2);
				const float Gap = UndersideZ - GroundZ;
				if (Gap < OutReport.QuadrantGaps[Quadrant])
				{
					OutReport.QuadrantGaps[Quadrant] = Gap;
					OutReport.ContactPoints[Quadrant] = FVector(World.X, World.Y, UndersideZ);
					OutReport.GroundHeights[Quadrant] = GroundZ;
				}
			}
		}
		bool bAllQuadrants = true;
		for (const float Gap : OutReport.QuadrantGaps)
		{
			bAllQuadrants = bAllQuadrants and Gap < TNumericLimits<float>::Max();
		}
		if (not bAllQuadrants)
		{
			// No collision to trace (scenery props): the visible box bottom at the four corners stands in.
			OutReport.bUsedBoxBottom = true;
			const FVector Corners[QuadrantCount] = {
				FVector(LocalBox.Min.X, LocalBox.Min.Y, LocalBox.Min.Z), FVector(LocalBox.Max.X, LocalBox.Min.Y, LocalBox.Min.Z),
				FVector(LocalBox.Max.X, LocalBox.Max.Y, LocalBox.Min.Z), FVector(LocalBox.Min.X, LocalBox.Max.Y, LocalBox.Min.Z)};
			for (int32 Quadrant = 0; Quadrant < QuadrantCount; ++Quadrant)
			{
				const FVector World = ActorTransform.TransformPosition(Corners[Quadrant]);
				const float GroundZ = SurvivalPOISeating_GroundAt(Projector, FVector2D(World), World.Z);
				OutReport.QuadrantGaps[Quadrant] = World.Z - GroundZ;
				OutReport.ContactPoints[Quadrant] = World;
				OutReport.GroundHeights[Quadrant] = GroundZ;
			}
		}
		TArray<float> SortedGaps = OutReport.QuadrantGaps;
		SortedGaps.Sort();
		OutReport.SupportGap = SortedGaps[1];
		OutReport.MaxGap = FMath::Max(0.f, SortedGaps.Last());
		OutReport.MaxSink = FMath::Max(0.f, -SortedGaps[0]);
		FSurvivalGroundHit CenterHit;
		const FVector CenterWorld = ActorTransform.TransformPosition(FVector(LocalBox.GetCenter().X, LocalBox.GetCenter().Y, 0.0));
		if (Projector.TryProject(FVector2D(CenterWorld), CenterHit))
		{
			OutReport.GroundSlopeDegrees = FSurvivalGroundProjector::GetSlopeDegrees(CenterHit.Normal);
			const double Cosine = FVector::DotProduct(ActorTransform.GetUnitAxis(EAxis::Z), CenterHit.Normal);
			OutReport.NormalDeviationDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Cosine, -1.0, 1.0)));
		}
	}

	void SeatActor(AActor& Actor, const ESurvivalPOISeatMode SeatMode, const float SinkCm, const float MaxTiltDegrees,
	               const FSurvivalGroundProjector& Projector, FSurvivalPOISeatReport& OutReport)
	{
		using namespace SurvivalPOISeatingConstants;
		const FVector Pivot = Actor.GetActorLocation();
		if (SeatMode == ESurvivalPOISeatMode::PivotOnGround)
		{
			Actor.SetActorLocation(FVector(Pivot.X, Pivot.Y, SurvivalPOISeating_GroundAt(Projector, FVector2D(Pivot), Pivot.Z) - SinkCm));
			MeasureSeat(Actor, Projector, OutReport);
			return;
		}
		FBox LocalBox;
		if (not GetVisibleLocalBox(Actor, LocalBox))
		{
			MeasureSeat(Actor, Projector, OutReport);
			return;
		}
		const float Yaw = Actor.GetActorRotation().Yaw;
		if (SeatMode == ESurvivalPOISeatMode::AlignToGround)
		{
			// The plane through the ground under the four footprint corners.
			const FTransform Upright(FRotator(0.f, Yaw, 0.f), Pivot);
			const FVector LocalCorners[QuadrantCount] = {
				FVector(LocalBox.Min.X, LocalBox.Min.Y, 0.0), FVector(LocalBox.Max.X, LocalBox.Min.Y, 0.0),
				FVector(LocalBox.Max.X, LocalBox.Max.Y, 0.0), FVector(LocalBox.Min.X, LocalBox.Max.Y, 0.0)};
			FVector Ground[QuadrantCount];
			for (int32 Corner = 0; Corner < QuadrantCount; ++Corner)
			{
				const FVector World = Upright.TransformPosition(LocalCorners[Corner]);
				Ground[Corner] = FVector(World.X, World.Y, SurvivalPOISeating_GroundAt(Projector, FVector2D(World), Pivot.Z));
			}
			FVector Normal = FVector::CrossProduct(Ground[2] - Ground[0], Ground[3] - Ground[1]).GetSafeNormal();
			if (Normal.Z < 0.0)
			{
				Normal = -Normal;
			}
			const float Tilt = FSurvivalGroundProjector::GetSlopeDegrees(Normal);
			if (Tilt > MaxTiltDegrees)
			{
				Normal = FMath::Lerp(FVector::UpVector, Normal, MaxTiltDegrees / Tilt).GetSafeNormal();
			}
			Actor.SetActorRotation(SurvivalStartPlacement::MakeGroundRotation(Normal, Yaw, true));
		}
		MeasureSeat(Actor, Projector, OutReport);
		if (not OutReport.bMeasured)
		{
			return;
		}
		// Rest on the two lowest quadrants (wheels, tracks, legs): a quadrant with nothing under it (an overhang, the wire
		// between two posts) may stand clear, but the object never floats on a single point. Sink none past the limit.
		TArray<float> Sorted = OutReport.QuadrantGaps;
		Sorted.Sort();
		const float Lowest = Sorted[0];
		float Offset = -SinkCm - Sorted[1];
		if (Lowest + Offset < -(MaxSinkCm + SinkCm))
		{
			Offset = -(MaxSinkCm + SinkCm) - Lowest;
		}
		Actor.SetActorLocation(Actor.GetActorLocation() + FVector(0.0, 0.0, Offset));
		MeasureSeat(Actor, Projector, OutReport);
	}
}
