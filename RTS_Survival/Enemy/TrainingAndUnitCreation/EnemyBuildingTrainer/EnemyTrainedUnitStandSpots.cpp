// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyTrainedUnitStandSpots.h"

#include "NavigationData.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/Units/SquadController.h"

DEFINE_LOG_CATEGORY_STATIC(LogEnemyTrainedUnitStandSpot, Log, All);

namespace EnemyTrainedUnitStandSpotsPrivate
{
	// The first ring lies this far beyond the unit's own radius from the exit.
	constexpr float FirstRingExtraCm = 200.f;
	constexpr float MaxSearchDistanceCm = 5000.f;
	constexpr float MinRingStepCm = 300.f;
	// Spots are tried up to this far around from straight out of the exit (180 would be behind the building).
	constexpr float MaxAngleFromExitDeg = 120.f;
	constexpr float MinAngleStepDeg = 8.f;
	constexpr float MaxAngleStepDeg = 30.f;
	// Neighbouring spots on a ring are this many unit radii apart.
	constexpr float AngleStepRadiusMlt = 1.5f;
	// Score cost per degree off the exit axis: the ground in front of the exit fills before the building's sides.
	constexpr float AnglePenaltyCmPerDeg = 8.f;
	constexpr int32 MaxCandidates = 600;
	// Kept between the unit's footprint and any other unit's or reserved spot's.
	constexpr float ClearanceMarginCm = 100.f;
	const FVector SpotProjectionExtent(150.f, 150.f, 500.f);
	constexpr float SpotProjectionToleranceCm = 150.f;
	// The footprint is on the navmesh when these probes around the spot all are (a spot by a wall or a navmesh edge
	// fails them).
	constexpr int32 FootprintProbeCount = 8;
	constexpr float FootprintProbeRadiusMlt = 0.8f;
	const FVector FootprintProbeExtent(60.f, 60.f, 300.f);
	constexpr float FootprintProbeToleranceCm = 60.f;
	const FVector ExitProjectionExtent(500.f, 500.f, 1000.f);
	constexpr float SquadMemberRadiusCm = 130.f;
	constexpr float MinSquadFootprintRadiusCm = 250.f;
	constexpr float MinFootprintRadiusCm = 250.f;
	constexpr float MaxFootprintRadiusCm = 800.f;
	constexpr float SoldierFootprintRadiusCm = 60.f;

	struct FCandidate
	{
		FVector Location = FVector::ZeroVector;
		float Score = 0.f;
	};

	bool GetIsInKeepClearLane(const FEnemyStandSpotQuery& Query, const FVector& Location)
	{
		for (const FEnemyStandSpotKeepClearLane& Lane : Query.KeepClearLanes)
		{
			const FVector Offset = FVector(Location.X - Lane.Start.X, Location.Y - Lane.Start.Y, 0.f);
			const FVector LaneRight = FVector::CrossProduct(FVector::UpVector, Lane.Forward);
			const float Along = FVector::DotProduct(Offset, Lane.Forward);
			const float Across = FMath::Abs(FVector::DotProduct(Offset, LaneRight));
			// Also the ground just behind the exit start: the unit's footprint must not reach into the lane.
			if (Along >= -Query.UnitRadius && Along <= Lane.Length + Query.UnitRadius
				&& Across <= Lane.HalfWidth + Query.UnitRadius)
			{
				return true;
			}
		}
		return false;
	}

	bool GetIsClearOfOccupied(const FEnemyStandSpotQuery& Query, const FVector& Location)
	{
		for (const FEnemyStandSpotOccupied& Occupied : Query.Occupied)
		{
			const float Needed = Query.UnitRadius + Occupied.Radius + ClearanceMarginCm;
			if (FVector::DistSquared2D(Location, Occupied.Location) < FMath::Square(Needed))
			{
				return false;
			}
		}
		return true;
	}

	bool TryProjectSpot(const ANavigationData& NavData, const FVector& Candidate, FVector& OutSpot)
	{
		FNavLocation Projected;
		if (not NavData.ProjectPoint(Candidate, Projected, SpotProjectionExtent)
			|| FVector::Dist2D(Projected.Location, Candidate) > SpotProjectionToleranceCm)
		{
			return false;
		}
		OutSpot = Projected.Location;
		return true;
	}

	bool GetIsFootprintOnNavMesh(const ANavigationData& NavData, const FVector& Spot, const float UnitRadius)
	{
		const float ProbeRadius = UnitRadius * FootprintProbeRadiusMlt;
		for (int32 Probe = 0; Probe < FootprintProbeCount; ++Probe)
		{
			const float Angle = 2.f * PI * Probe / FootprintProbeCount;
			const FVector ProbeLocation = Spot + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * ProbeRadius;
			FNavLocation Projected;
			if (not NavData.ProjectPoint(ProbeLocation, Projected, FootprintProbeExtent)
				|| FVector::Dist2D(Projected.Location, ProbeLocation) > FootprintProbeToleranceCm)
			{
				return false;
			}
		}
		return true;
	}

	void BuildCandidates(const FEnemyStandSpotQuery& Query, TArray<FCandidate>& OutCandidates)
	{
		const float FirstRing = Query.UnitRadius + FirstRingExtraCm;
		const float RingStep = FMath::Max(MinRingStepCm, Query.UnitRadius);
		for (float Distance = FirstRing; Distance <= MaxSearchDistanceCm + Query.UnitRadius; Distance += RingStep)
		{
			const float AngleStep = FMath::Clamp(
				FMath::RadiansToDegrees(Query.UnitRadius * AngleStepRadiusMlt / Distance), MinAngleStepDeg,
				MaxAngleStepDeg);
			for (float Angle = 0.f; Angle <= MaxAngleFromExitDeg + KINDA_SMALL_NUMBER; Angle += AngleStep)
			{
				for (const float Side : {1.f, -1.f})
				{
					if (Angle <= KINDA_SMALL_NUMBER && Side < 0.f)
					{
						continue;
					}
					const FVector Direction = Query.ExitForward.RotateAngleAxis(Side * Angle, FVector::UpVector);
					FCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
					Candidate.Location = Query.ExitLocation + Direction * Distance;
					Candidate.Score = Distance + Angle * AnglePenaltyCmPerDeg;
				}
			}
		}
		OutCandidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score < B.Score; });
		if (OutCandidates.Num() > MaxCandidates)
		{
			OutCandidates.SetNum(MaxCandidates);
		}
	}

	float GetFacingYaw(const FEnemyStandSpotQuery& Query, const FVector& Spot)
	{
		const FVector Away = FVector(Spot.X - Query.BuildingCenter.X, Spot.Y - Query.BuildingCenter.Y, 0.f);
		return Away.IsNearlyZero() ? Query.ExitForward.Rotation().Yaw : Away.Rotation().Yaw;
	}
}

bool EnemyTrainedUnitStandSpots::FindStandSpot(UWorld* World, const FEnemyStandSpotQuery& Query,
                                               FEnemyStandSpot& OutSpot)
{
	using namespace EnemyTrainedUnitStandSpotsPrivate;
	UNavigationSystemV1* const NavSystem = UNavigationSystemV1::GetCurrent(World);
	if (not IsValid(NavSystem))
	{
		return false;
	}
	// The navmesh the unit paths on: tanks have their own, wider agents.
	const ANavigationData* NavData = Query.NavData.Get();
	if (not IsValid(NavData))
	{
		NavData = NavSystem->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
	}
	if (not IsValid(NavData))
	{
		return false;
	}
	const FSharedConstNavQueryFilter QueryFilter = NavData->GetDefaultQueryFilter();
	FNavLocation ExitOnNavMesh;
	const bool bHasExitOnNavMesh = NavData->ProjectPoint(Query.ExitLocation, ExitOnNavMesh, ExitProjectionExtent);
	TArray<FCandidate> Candidates;
	BuildCandidates(Query, Candidates);

	bool bHasFallback = false;
	FEnemyStandSpot Fallback;
	int32 Tried = 0;
	for (const FCandidate& Candidate : Candidates)
	{
		++Tried;
		// Cheap rejections first, the navmesh queries after.
		if (GetIsInKeepClearLane(Query, Candidate.Location) || not GetIsClearOfOccupied(Query, Candidate.Location))
		{
			continue;
		}
		FVector Spot;
		if (not TryProjectSpot(*NavData, Candidate.Location, Spot)
			|| not GetIsFootprintOnNavMesh(*NavData, Spot, Query.UnitRadius))
		{
			continue;
		}
		FVector HitLocation;
		const bool bHasStraightNavLine = bHasExitOnNavMesh
			&& not NavData->Raycast(ExitOnNavMesh.Location, Spot, HitLocation, QueryFilter);
		if (not bHasStraightNavLine)
		{
			if (not bHasFallback)
			{
				bHasFallback = true;
				Fallback.Location = Spot;
				Fallback.Yaw = GetFacingYaw(Query, Spot);
				Fallback.CandidatesTried = Tried;
				Fallback.bHasStraightNavLine = false;
			}
			continue;
		}
		OutSpot.Location = Spot;
		OutSpot.Yaw = GetFacingYaw(Query, Spot);
		OutSpot.CandidatesTried = Tried;
		OutSpot.bHasStraightNavLine = true;
		UE_LOG(LogEnemyTrainedUnitStandSpot, Log,
		       TEXT("AI_TRAIN stand_spot %s at %s dist=%.0f yaw=%.0f radius=%.0f tried=%d lanes=%d occupied=%d"),
		       *Query.UnitNameForLog, *Spot.ToCompactString(), FVector::Dist2D(Spot, Query.ExitLocation), OutSpot.Yaw,
		       Query.UnitRadius, Tried, Query.KeepClearLanes.Num(), Query.Occupied.Num());
		return true;
	}
	if (bHasFallback)
	{
		OutSpot = Fallback;
		UE_LOG(LogEnemyTrainedUnitStandSpot, Log,
		       TEXT("AI_TRAIN stand_spot %s at %s dist=%.0f yaw=%.0f radius=%.0f tried=%d no_straight_line"),
		       *Query.UnitNameForLog, *Fallback.Location.ToCompactString(),
		       FVector::Dist2D(Fallback.Location, Query.ExitLocation), Fallback.Yaw, Query.UnitRadius,
		       Fallback.CandidatesTried);
		return true;
	}
	UE_LOG(LogEnemyTrainedUnitStandSpot, Warning,
	       TEXT("AI_TRAIN stand_spot %s none within %.0f cm of %s (tried=%d lanes=%d occupied=%d); the unit stays"),
	       *Query.UnitNameForLog, MaxSearchDistanceCm, *Query.ExitLocation.ToCompactString(), Tried,
	       Query.KeepClearLanes.Num(), Query.Occupied.Num());
	return false;
}

float EnemyTrainedUnitStandSpots::GetUnitFootprintRadius(const AActor* Unit)
{
	using namespace EnemyTrainedUnitStandSpotsPrivate;
	if (not IsValid(Unit))
	{
		return MinFootprintRadiusCm;
	}
	if (const ASquadController* const Squad = Cast<ASquadController>(Unit))
	{
		const int32 Members = FMath::Max(1, Squad->GetSquadUnitsCount());
		return FMath::Max(MinSquadFootprintRadiusCm, SquadMemberRadiusCm * FMath::Sqrt(static_cast<float>(Members)));
	}
	// The colliding components in the unit's own frame: the hull however the unit is turned.
	const FBox LocalBox = Unit->CalculateComponentsBoundingBoxInLocalSpace(false, false);
	if (not LocalBox.IsValid)
	{
		return MinFootprintRadiusCm;
	}
	const FVector Extent = LocalBox.GetExtent();
	// Capped: a large colliding helper (a trigger or detection volume) must not make a tank need a field.
	return FMath::Clamp(FVector2D(Extent.X, Extent.Y).Size(), MinFootprintRadiusCm, MaxFootprintRadiusCm);
}

const ANavigationData* EnemyTrainedUnitStandSpots::GetNavDataForUnit(UWorld* World, const AActor* Unit)
{
	const APawn* const Pawn = Cast<APawn>(Unit);
	UNavigationSystemV1* const NavSystem = UNavigationSystemV1::GetCurrent(World);
	if (not IsValid(Pawn) || not IsValid(NavSystem))
	{
		return nullptr;
	}
	return NavSystem->GetNavDataForProps(Pawn->GetNavAgentPropertiesRef(), Pawn->GetActorLocation());
}

float EnemyTrainedUnitStandSpots::GetSoldierFootprintRadius()
{
	return EnemyTrainedUnitStandSpotsPrivate::SoldierFootprintRadiusCm;
}
