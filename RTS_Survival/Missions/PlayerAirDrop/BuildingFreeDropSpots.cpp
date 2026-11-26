// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BuildingFreeDropSpots.h"

#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Player/ConstructionPreview/StaticMeshPreview/StaticPreviewMesh.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace BuildingFreeDropSpotsConstants
{
	// Buildings stand on hills and in pits: the query reaches this far above and below the spot.
	constexpr float QueryHalfHeight = 2500.f;
	// Rings never sit closer than this, so a small clearance does not make the search crawl.
	constexpr float MinRingSpacing = 400.f;
	constexpr float MaxSearchRadius = 9000.f;
	constexpr int32 MinCandidatesPerRing = 8;
	// Navmesh projection reach for a candidate (RTSFunctionLibrary::GetLocationProjected scale).
	constexpr float ProjectionScale = 2.f;
	// The human player in the unit registry (FRTSPlayers numbering).
	constexpr uint8 LocalPlayer = 1;
}

namespace
{
	AActor* GetAttachRoot(AActor* Actor)
	{
		AActor* Root = Actor;
		while (IsValid(Root) && IsValid(Root->GetAttachParentActor()))
		{
			Root = Root->GetAttachParentActor();
		}
		return Root;
	}

	bool GetIsBuildingActor(AActor* Actor)
	{
		const AActor* Root = GetAttachRoot(Actor);
		if (not IsValid(Root))
		{
			return false;
		}
		if (Root->IsA<ABuildingExpansion>() || Root->IsA<AStaticPreviewMesh>())
		{
			return true;
		}
		const ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Root);
		return IsValid(Nomadic) && not Nomadic->GetIsInTruckMode();
	}

	bool GetIsBuildingMeshOverlapping(const UWorld& World, const FVector& Location, const float ClearanceRadius)
	{
		using namespace BuildingFreeDropSpotsConstants;
		TArray<FOverlapResult> Overlaps;
		const FCollisionShape Capsule = FCollisionShape::MakeCapsule(ClearanceRadius,
		                                                             QueryHalfHeight + ClearanceRadius);
		World.OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, COLLISION_OBJ_BUILDING_PLACEMENT, Capsule,
		                            FCollisionQueryParams(SCENE_QUERY_STAT(BuildingFreeDropSpot), false));
		return Overlaps.ContainsByPredicate([](const FOverlapResult& Overlap)
		{
			return GetIsBuildingActor(Overlap.GetActor());
		});
	}

	/** @return The 2D reach of the building an unfolding nomadic becomes; 0 for a truck or a finished building. */
	float GetUnfoldingBuildingReach(const ANomadicVehicle& Nomadic)
	{
		const ENomadStatus Status = Nomadic.GetNomadicStatus();
		if (Status == ENomadStatus::Truck || Status == ENomadStatus::Building)
		{
			return 0.f;
		}
		const UStaticMesh* PreviewMesh = Nomadic.GetPreviewMesh();
		if (not IsValid(PreviewMesh))
		{
			return 0.f;
		}
		return PreviewMesh->GetBounds().BoxExtent.Size2D();
	}

	bool GetIsNearUnfoldingNomadic(const UWorld& World, const FVector& Location, const float ClearanceRadius)
	{
		const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(&World);
		if (not IsValid(UnitManager))
		{
			return false;
		}
		for (ATankMaster* Tank : UnitManager->GetTanksOfPlayerView(BuildingFreeDropSpotsConstants::LocalPlayer))
		{
			const ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Tank);
			if (not IsValid(Nomadic))
			{
				continue;
			}
			const float Reach = GetUnfoldingBuildingReach(*Nomadic);
			if (Reach > 0.f && FVector::Dist2D(Nomadic->GetActorLocation(), Location) < Reach + ClearanceRadius)
			{
				return true;
			}
		}
		return false;
	}
}

bool FBuildingFreeDropSpots::GetIsSpotClearOfBuildings(const UWorld& World, const FVector& Location,
                                                       const float ClearanceRadius)
{
	const float Radius = FMath::Max(1.f, ClearanceRadius);
	if (GetIsBuildingMeshOverlapping(World, Location, Radius))
	{
		return false;
	}
	return not GetIsNearUnfoldingNomadic(World, Location, Radius);
}

bool FBuildingFreeDropSpots::FindBuildingFreeSpot(const UObject* WorldContextObject, const FVector& Anchor,
                                                  const float ClearanceRadius,
                                                  const TFunctionRef<bool(const FVector&)> ExtraAccept,
                                                  FVector& OutSpot)
{
	using namespace BuildingFreeDropSpotsConstants;
	OutSpot = Anchor;
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		RTSFunctionLibrary::ReportError("FindBuildingFreeSpot: no world; the anchor is used as it is.");
		return false;
	}
	const float RingSpacing = FMath::Max(MinRingSpacing, ClearanceRadius);
	for (float RingRadius = 0.f; RingRadius <= MaxSearchRadius; RingRadius += RingSpacing)
	{
		const int32 CandidateCount = RingRadius <= 0.f
			                             ? 1
			                             : FMath::Max(MinCandidatesPerRing,
			                                          FMath::CeilToInt(UE_TWO_PI * RingRadius / RingSpacing));
		for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
		{
			const float Angle = UE_TWO_PI * static_cast<float>(CandidateIndex) / static_cast<float>(CandidateCount);
			const FVector Candidate = Anchor + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * RingRadius;
			bool bProjected = false;
			const FVector Projected = RTSFunctionLibrary::GetLocationProjected(WorldContextObject, Candidate, true,
			                                                                   bProjected, ProjectionScale);
			if (not bProjected || not ExtraAccept(Projected))
			{
				continue;
			}
			if (GetIsSpotClearOfBuildings(*World, Projected, ClearanceRadius))
			{
				OutSpot = Projected;
				return true;
			}
		}
	}
	return false;
}

bool FBuildingFreeDropSpots::FindBuildingFreeSpot(const UObject* WorldContextObject, const FVector& Anchor,
                                                  const float ClearanceRadius, FVector& OutSpot)
{
	return FindBuildingFreeSpot(WorldContextObject, Anchor, ClearanceRadius,
	                            [](const FVector&) { return true; }, OutSpot);
}
