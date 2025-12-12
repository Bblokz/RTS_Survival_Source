// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "FMoveUnitsFromConstruction.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"

namespace
{
	// Keep target picking bounded and deterministic enough.
	constexpr int32 GKMaxProjectionTries = 6;
	constexpr float GKMinDistFromBounds = 200.f;
	constexpr float GKMaxDistFromBounds = 500.f;
}

void FMoveUnitsFromConstruction::MoveOverlappingUnitsForPlayer1AwayFromConstruction(
	const UObject* WorldContextObject,
	const AActor* PreviewActor,
	const FRotator& FinishedMoveRotation, ANomadicVehicle* NomadicToIgnore)
{
	if (not IsValid(WorldContextObject) || not IsValid(PreviewActor))
	{
		return;
	}

	const FBox BoundsWS = GetWorldBoundsOfPreview(PreviewActor);
	if (BoundsWS.IsValid == 0)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	GatherOverlappingMoveUnitActors(WorldContextObject, PreviewActor, BoundsWS, OverlappingActors);
	TArray<ASquadController*> MovedSquadControllers = {};

	for (AActor* EachActor : OverlappingActors)
	{
		if (not IsValid(EachActor))
		{
			continue;
		}
		if (not IsOwnedByPlayer1MoveUnit(EachActor) || EachActor == NomadicToIgnore)
		{
			continue;
		}
		IssueMoveOrderIfPossible(EachActor, WorldContextObject, BoundsWS, FinishedMoveRotation, MovedSquadControllers);
	}
}

FBox FMoveUnitsFromConstruction::GetWorldBoundsOfPreview(const AActor* PreviewActor)
{
	if (not IsValid(PreviewActor))
	{
		return FBox(EForceInit::ForceInitToZero);
	}

	// Axis-aligned world bounds of all components on the preview actor.
	const FBox BoundsWS = PreviewActor->GetComponentsBoundingBox(true);
	return BoundsWS;
}

void FMoveUnitsFromConstruction::GatherOverlappingMoveUnitActors(
	const UObject* WorldContextObject,
	const AActor* PreviewActor,
	const FBox& BoundsWS,
	TArray<AActor*>& OutActors)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return;
	}

	const FVector Center = BoundsWS.GetCenter();
	const FVector Extent = BoundsWS.GetExtent();

	FCollisionShape BoxShape = FCollisionShape::MakeBox(Extent);

	// We want to query objects that represent **movable units** on the player side.
	// Based on your collision setup:
	// - Vehicle/nomadic movement meshes and infantry capsules use COLLISION_OBJ_PLAYER for player units
	//   (see SetupPlayerVehicleMovementMeshCollision / SetupNomadicMvtPlayer / SetupInfantryCapsuleCollision).
	// - Some auxiliary unit sub-meshes can be ECC_Pawn (tracks/mounts), so include it for robustness.
	FCollisionObjectQueryParams ObjQuery;
	ObjQuery.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	ObjQuery.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MoveUnitsFromConstruction), /*bTraceComplex=*/false);
	if (IsValid(PreviewActor))
	{
		Params.AddIgnoredActor(PreviewActor);
	}

	TArray<FOverlapResult> Overlaps;
	const bool bHit = World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		ObjQuery,
		BoxShape,
		Params);

	if (not bHit)
	{
		return;
	}

	// Unique actors only.
	TSet<AActor*> Unique;
	for (const FOverlapResult& Hit : Overlaps)
	{
		AActor* HitActor = Hit.GetActor();
		if (IsValid(HitActor))
		{
			Unique.Add(HitActor);
		}
	}

	OutActors = Unique.Array();
}

bool FMoveUnitsFromConstruction::IsOwnedByPlayer1MoveUnit(const AActor* Actor)
{
	if (not IsValid(Actor))
	{
		return false;
	}

	const URTSComponent* RTS = Cast<URTSComponent>(Actor->GetComponentByClass(URTSComponent::StaticClass()));
	if (not IsValid(RTS))
	{
		return false;
	}

	// Filter to player 1 only (per requirement).
	return RTS->GetOwningPlayer() == 1;
}

void FMoveUnitsFromConstruction::IssueMoveOrderIfPossible(
	AActor* Actor,
	const UObject* WorldContextObject,
	const FBox& BoundsWS,
	const FRotator& FinishedMoveRotation, TArray<ASquadController*>& SquadControllers)
{
	if (not IsValid(Actor) || not IsValid(WorldContextObject))
	{
		return;
	}

	ICommands* Commands = Cast<ICommands>(Actor);
	if (not Commands)
	{
		ASquadController* SqCtrl = AttemptGetSquadControllerFromUnit(Actor);
		if (not SqCtrl)
		{
			// Not a commandable unit nor a squad unit.
			return;
		}
		Commands = SqCtrl;
		if (not SquadControllers.Contains(SqCtrl))
		{
			SquadControllers.Add(SqCtrl);
			Actor = SqCtrl;
		}
		else
		{
			// Do not order the same squad to move twice.
			return;
		}
	}
	const bool bIsMoving = Commands->GetActiveCommandID() == EAbilityID::IdMove || Commands->GetActiveCommandID() == EAbilityID::IdReverseMove;
	const bool bIsPatrolling = Commands->GetActiveCommandID() == EAbilityID::IdPatrol;
	const bool bIsConstructing = Commands->GetActiveCommandID() == EAbilityID::IdCreateBuilding;
	if(bIsConstructing || bIsMoving || bIsPatrolling)
	{
		// Already moving or constructing, skip.
		return;
	}

	// Try a few random projected points outside the bounds.
	for (int32 Try = 0; Try < GKMaxProjectionTries; ++Try)
	{
		FVector Candidate;
		PickRandomOutsidePointOnPlane(BoundsWS, GKMinDistFromBounds, GKMaxDistFromBounds, Candidate);

		FVector Projected;
		const bool bIsLastTry = (Try == GKMaxProjectionTries - 1);
		if (not bIsLastTry && not TryProject(WorldContextObject, Candidate, Projected))
		{
			continue;
		}

		// Reset queue per spec (bSetUnitToIdle = true).
		constexpr bool bResetQueue = true;
		const ECommandQueueError Result = Commands->MoveToLocation(Projected, bResetQueue, FinishedMoveRotation);

		if (Result == ECommandQueueError::NoError)
		{
			return;
		}
	}

	// Optional: log once if all tries failed.
	RTSFunctionLibrary::PrintString(
		FString::Printf(TEXT("MoveUnitsFromConstruction: failed to find projected target for %s"),
		                *Actor->GetName()),
		FColor::Yellow);
}

FVector2D FMoveUnitsFromConstruction::RandomUnitDir2D()
{
	// Uniform direction on unit circle.
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
}

void FMoveUnitsFromConstruction::PickRandomOutsidePointOnPlane(
	const FBox& BoundsWS,
	const float MinDistFromBounds,
	const float MaxDistFromBounds,
	FVector& OutPointXZ)
{
	const FVector Center = BoundsWS.GetCenter();
	const FVector Extent = BoundsWS.GetExtent();

	const FVector2D Dir2D = RandomUnitDir2D();
	const float SupportAlongDir =
		FMath::Abs(Dir2D.X) * Extent.X +
		FMath::Abs(Dir2D.Y) * Extent.Y;

	const float Extra = FMath::FRandRange(MinDistFromBounds, MaxDistFromBounds);
	const float RadiusFromCenter = SupportAlongDir + Extra;

	OutPointXZ = FVector(
		Center.X + Dir2D.X * RadiusFromCenter,
		Center.Y + Dir2D.Y * RadiusFromCenter,
		Center.Z // keep same Z as preview
	);
}

bool FMoveUnitsFromConstruction::TryProject(
	const UObject* WorldContextObject,
	const FVector& Original,
	FVector& OutProjected)
{
	bool bOk = false;
	// We want a broader projection area -> scale 2, keep flat (no Z extent).
	constexpr bool bExtentInZ = true;
	constexpr float ProjectionScale = 10.f;

	const FVector Projected =
		RTSFunctionLibrary::GetLocationProjected(WorldContextObject, Original, bExtentInZ, bOk, ProjectionScale);

	if (not bOk)
	{
		return false;
	}
	OutProjected = Projected;
	return true;
}

ASquadController* FMoveUnitsFromConstruction::AttemptGetSquadControllerFromUnit(AActor* Unit)
{
	if (not IsValid(Unit))
	{
		return nullptr;
	}
	ASquadUnit* SquadUnit = Cast<ASquadUnit>(Unit);
	if (not SquadUnit)
	{
		return nullptr;
	}
	return SquadUnit->GetSquadControllerChecked();
}
