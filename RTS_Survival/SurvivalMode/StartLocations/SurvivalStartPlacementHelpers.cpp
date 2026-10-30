// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalStartPlacementHelpers.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartContentSettings.h"

FSurvivalGroundProjector::FSurvivalGroundProjector(UWorld& World, const float TraceUp, const float TraceDown)
	: M_TraceUp(TraceUp)
	, M_TraceDown(TraceDown)
{
	for (TActorIterator<ALandscapeProxy> LandscapeIterator(&World); LandscapeIterator; ++LandscapeIterator)
	{
		if (IsValid(*LandscapeIterator))
		{
			M_Landscapes.Add(*LandscapeIterator);
		}
	}
}

bool FSurvivalGroundProjector::TryGetHeight(const FVector2D& Position, float& OutHeight) const
{
	for (const TWeakObjectPtr<ALandscapeProxy>& Landscape : M_Landscapes)
	{
		if (not Landscape.IsValid())
		{
			continue;
		}
		const TOptional<float> Height = Landscape->GetHeightAtLocation(FVector(Position.X, Position.Y, 0.0));
		if (Height.IsSet())
		{
			OutHeight = Height.GetValue();
			return true;
		}
	}
	return false;
}

bool FSurvivalGroundProjector::TryProject(const FVector2D& Position, FSurvivalGroundHit& OutHit) const
{
	const FCollisionQueryParams QueryParameters(SCENE_QUERY_STAT(SurvivalStartGroundProjection), false);
	for (const TWeakObjectPtr<ALandscapeProxy>& Landscape : M_Landscapes)
	{
		if (not Landscape.IsValid())
		{
			continue;
		}
		const TOptional<float> Height = Landscape->GetHeightAtLocation(FVector(Position.X, Position.Y, 0.0));
		if (not Height.IsSet())
		{
			continue;
		}
		const FVector OnLandscape(Position.X, Position.Y, Height.GetValue());
		FHitResult Hit;
		if (Landscape->ActorLineTraceSingle(Hit, OnLandscape + FVector::UpVector * M_TraceUp,
		                                    OnLandscape - FVector::UpVector * M_TraceDown, ECC_WorldStatic,
		                                    QueryParameters))
		{
			OutHit.Location = Hit.ImpactPoint;
			OutHit.Normal = Hit.ImpactNormal.GetSafeNormal();
			return true;
		}
		// Height without collision (e.g. collision still streaming): usable, but assume flat.
		OutHit.Location = OnLandscape;
		OutHit.Normal = FVector::UpVector;
		return true;
	}
	return false;
}

float FSurvivalGroundProjector::GetSlopeDegrees(const FVector& Normal)
{
	const double CosSlope = FMath::Clamp(Normal.GetSafeNormal().Z, -1.0, 1.0);
	return static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(CosSlope)));
}

void FSurvivalOccupancy::Add(const FVector2D& Center, const float Radius)
{
	FSurvivalReservedCircle& Circle = M_Circles.AddDefaulted_GetRef();
	Circle.Center = Center;
	Circle.Radius = Radius;
}

bool FSurvivalOccupancy::IsFree(const FVector2D& Center, const float Radius, const float Clearance) const
{
	for (const FSurvivalReservedCircle& Circle : M_Circles)
	{
		const float MinDistance = Circle.Radius + Radius + Clearance;
		if (FVector2D::DistSquared(Circle.Center, Center) < FMath::Square(MinDistance))
		{
			return false;
		}
	}
	return true;
}

namespace SurvivalStartPlacement
{
	int32 PickWeightedActorIndex(const TArray<FSurvivalWeightedActorClass>& Entries, FRandomStream& Random)
	{
		float TotalWeight = 0.f;
		for (const FSurvivalWeightedActorClass& Entry : Entries)
		{
			if (not Entry.ActorClass.IsNull())
			{
				TotalWeight += FMath::Max(0.f, Entry.Weight);
			}
		}
		if (TotalWeight <= 0.f)
		{
			return INDEX_NONE;
		}
		float Roll = Random.FRandRange(0.f, TotalWeight);
		int32 LastValidIndex = INDEX_NONE;
		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			const FSurvivalWeightedActorClass& Entry = Entries[EntryIndex];
			if (Entry.ActorClass.IsNull() or Entry.Weight <= 0.f)
			{
				continue;
			}
			LastValidIndex = EntryIndex;
			Roll -= Entry.Weight;
			if (Roll <= 0.f)
			{
				return EntryIndex;
			}
		}
		return LastValidIndex;
	}

	int32 PickWeightedMeshIndex(const TArray<FSurvivalWeightedMesh>& Entries, FRandomStream& Random)
	{
		float TotalWeight = 0.f;
		for (const FSurvivalWeightedMesh& Entry : Entries)
		{
			if (not Entry.Mesh.IsNull())
			{
				TotalWeight += FMath::Max(0.f, Entry.Weight);
			}
		}
		if (TotalWeight <= 0.f)
		{
			return INDEX_NONE;
		}
		float Roll = Random.FRandRange(0.f, TotalWeight);
		int32 LastValidIndex = INDEX_NONE;
		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			const FSurvivalWeightedMesh& Entry = Entries[EntryIndex];
			if (Entry.Mesh.IsNull() or Entry.Weight <= 0.f)
			{
				continue;
			}
			LastValidIndex = EntryIndex;
			Roll -= Entry.Weight;
			if (Roll <= 0.f)
			{
				return EntryIndex;
			}
		}
		return LastValidIndex;
	}

	FVector2D RandomPointInAnnulus(
		const FVector2D& Center,
		const float MinRadius,
		const float MaxRadius,
		FRandomStream& Random)
	{
		const float InnerRadius = FMath::Max(0.f, FMath::Min(MinRadius, MaxRadius));
		const float OuterRadius = FMath::Max(InnerRadius, FMath::Max(MinRadius, MaxRadius));
		// Square-root sampling of the radius keeps the density uniform per area.
		const float Radius = FMath::Sqrt(Random.FRandRange(FMath::Square(InnerRadius), FMath::Square(OuterRadius)));
		const float Angle = Random.FRandRange(0.f, 2.f * PI);
		return Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	}

	FQuat MakeGroundRotation(const FVector& GroundNormal, const float YawDegrees, const bool bAlignToGround)
	{
		const FQuat YawRotation(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
		if (not bAlignToGround)
		{
			return YawRotation;
		}
		const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, GroundNormal.GetSafeNormal());
		return Tilt * YawRotation;
	}
}
