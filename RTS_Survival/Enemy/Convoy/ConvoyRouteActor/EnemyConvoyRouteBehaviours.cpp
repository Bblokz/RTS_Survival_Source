// Copyright (C) Bas Blokzijl - All rights reserved.

// Members of AEnemyConvoyRoute that turn the route's stop, branch, ambush zone and hunter settings into each
// convoy's FConvoyBehaviourPlan, forward the convoy's behaviour events as typed Blueprint events, and unload the
// hunter squads as an attack wave of the enemy controller.

#include "EnemyConvoyRoute.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyWaves/AttackWave.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace EnemyConvoyRouteBehaviourConstants
{
	// Same spacing as the main drive path.
	constexpr float BranchResampleSpacing = 600.f;
	// A stop or dismount point further than this from the drive path is probably misplaced (logged, still used).
	constexpr float FarFromRouteWarningDistance = 3000.f;

	// Editor drawing.
	constexpr float ShapeLiftZ = 80.f;
	constexpr float BranchThickness = 10.f;
	constexpr float RingThickness = 6.f;
	constexpr float DashSize = 160.f;
	constexpr float DimmedBrightness = 0.75f;
	const FVector StopBoxHalfExtent(320.f, 320.f, 120.f);
}

namespace
{
	FVector LiftBehaviourShape(const FVector& Point)
	{
		return Point + FVector(0.f, 0.f, EnemyConvoyRouteBehaviourConstants::ShapeLiftZ);
	}
}

// ------------------------------------------------------------------
// Public helpers
// ------------------------------------------------------------------

int32 AEnemyConvoyRoute::GetConvoyIndexOf(const AConvoy* Convoy) const
{
	const FEnemyConvoyRouteConvoyTally* Tally = M_Convoys.FindByPredicate(
		[Convoy](const FEnemyConvoyRouteConvoyTally& Candidate)
		{
			return Convoy != nullptr && Candidate.Convoy.Get() == Convoy;
		});
	return Tally != nullptr ? Tally->ConvoyIndex : INDEX_NONE;
}

TArray<AConvoy*> AEnemyConvoyRoute::GetActiveConvoys() const
{
	TArray<AConvoy*> Convoys;
	for (const FEnemyConvoyRouteConvoyTally& Tally : M_Convoys)
	{
		if (AConvoy* Convoy = Tally.Convoy.Get())
		{
			Convoys.Add(Convoy);
		}
	}
	return Convoys;
}

void AEnemyConvoyRoute::SetConvoyDefinition(UConvoyDefinition* NewDefinition)
{
	if (not IsValid(NewDefinition))
	{
		RTSFunctionLibrary::ReportError("SetConvoyDefinition on convoy route " + GetName() +
			" got no definition; it keeps " + (IsValid(Definition) ? Definition->GetName() : FString("none")) + ".");
		return;
	}
	Definition = NewDefinition;
}

// ------------------------------------------------------------------
// Behaviour plan
// ------------------------------------------------------------------

bool AEnemyConvoyRoute::GetAppliesToConvoy(const TArray<int32>& OnlyConvoyIndices, const int32 ConvoyIndex)
{
	return OnlyConvoyIndices.IsEmpty() || OnlyConvoyIndices.Contains(ConvoyIndex);
}

FConvoyBehaviourPlan AEnemyConvoyRoute::BuildBehaviourPlan(const int32 ConvoyIndex, const FConvoyRoute& Route)
{
	FConvoyBehaviourPlan Plan;
	FConvoyRoutePolyline Polyline;
	Polyline.Build(Route.Points);
	if (not Polyline.GetIsValid())
	{
		return Plan;
	}
	if (Stop.bEnabled && GetAppliesToConvoy(Stop.OnlyConvoyIndices, ConvoyIndex))
	{
		Plan.Stop = BuildStopPlan(Polyline);
	}
	if (Branch.bEnabled && GetAppliesToConvoy(Branch.OnlyConvoyIndices, ConvoyIndex))
	{
		Plan.Branch = BuildBranchPlan(Polyline);
	}
	if (GetAppliesToConvoy(Ambush.OnlyConvoyIndices, ConvoyIndex))
	{
		Plan.Ambush = BuildAmbushPlan();
	}
	if (Hunters.bEnabled && GetAppliesToConvoy(Hunters.OnlyConvoyIndices, ConvoyIndex))
	{
		Plan.Hunters = BuildHunterPlan(Polyline);
	}
	return Plan;
}

FVector AEnemyConvoyRoute::GetStopWorldLocation() const
{
	if (IsValid(Stop.StopActor))
	{
		return Stop.StopActor->GetActorLocation();
	}
	return GetActorTransform().TransformPosition(StopPoint);
}

FVector AEnemyConvoyRoute::GetDismountWorldLocation() const
{
	if (IsValid(Hunters.DismountActor))
	{
		return Hunters.DismountActor->GetActorLocation();
	}
	return GetActorTransform().TransformPosition(DismountPoint);
}

FConvoyStopPlan AEnemyConvoyRoute::BuildStopPlan(const FConvoyRoutePolyline& Polyline) const
{
	FConvoyStopPlan Plan;
	Plan.StopLocation = GetStopWorldLocation();
	float LateralOffset = 0.f;
	Plan.StopDistance = Polyline.ProjectLocation(Plan.StopLocation, -1.f, 0.f, LateralOffset);
	if (Plan.StopDistance >= Polyline.GetLength() - GetExitRadius())
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + ": the stop point lies at the exit; the convoy "
			"would leave before it stops. Move the stop point (or StopActor) along the road.");
		return Plan;
	}
	if (FMath::Abs(LateralOffset) > EnemyConvoyRouteBehaviourConstants::FarFromRouteWarningDistance)
	{
		UE_LOG(LogConvoy, Warning, TEXT("Convoy route %s: the stop point is %.0f cm from the drive path."), *GetName(),
		       FMath::Abs(LateralOffset));
	}
	Plan.bEnabled = true;
	Plan.StopSeconds = Stop.StopSeconds;
	Plan.ArriveTolerance = Stop.ArriveTolerance;
	Plan.FloorItSpeedMultiplier = Stop.FloorItSpeedMultiplier;
	Plan.FloorItSeconds = Stop.FloorItSeconds;
	Plan.bFloorItIgnoresHalt = Stop.bFloorItIgnoresHalt;
	return Plan;
}

TArray<FVector> AEnemyConvoyRoute::GetBranchControlPoints() const
{
	TArray<FVector> ControlPoints = GetWorldPoints(BranchPoints);
	if (IsValid(Branch.DestinationActor))
	{
		ControlPoints.Add(Branch.DestinationActor->GetActorLocation());
	}
	return ControlPoints;
}

bool AEnemyConvoyRoute::TryGetBuiltBranchRoute(FConvoyRoute& OutRoute)
{
	if (bM_HasTriedBranchRoute)
	{
		OutRoute = M_BuiltBranchRoute;
		return OutRoute.Points.Num() >= 2;
	}
	bM_HasTriedBranchRoute = true;
	M_BuiltBranchRoute.Points.Reset();
	const TArray<FVector> ControlPoints = GetBranchControlPoints();
	if (ControlPoints.Num() < 2)
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + ": the branch needs a fork point on the road and "
			"a destination (two BranchPoints, or one plus a DestinationActor); no truck branches off.");
		return false;
	}
	bool bReachedEnd = false;
	const FString IdBase = RouteId.IsNone() ? GetName() : RouteId.ToString();
	const FConvoyRoute BuiltRoute = UConvoyRouteLibrary::BuildNavigableRoute(
		this, ControlPoints, EnemyConvoyRouteBehaviourConstants::BranchResampleSpacing, FName(*(IdBase + TEXT("_Branch"))),
		bReachedEnd);
	if (not bReachedEnd || BuiltRoute.Points.Num() < 2)
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + ": the branch path does not reach its destination "
			"on the vehicle navmesh; no truck branches off.");
		return false;
	}
	M_BuiltBranchRoute = BuiltRoute;
	OutRoute = M_BuiltBranchRoute;
	return true;
}

FConvoyBranchPlan AEnemyConvoyRoute::BuildBranchPlan(const FConvoyRoutePolyline& Polyline)
{
	FConvoyBranchPlan Plan;
	FConvoyRoute BranchRoute;
	if (not TryGetBuiltBranchRoute(BranchRoute))
	{
		return Plan;
	}
	float LateralOffset = 0.f;
	Plan.ForkDistance = Polyline.ProjectLocation(BranchRoute.Points[0], -1.f, 0.f, LateralOffset);
	if (FMath::Abs(LateralOffset) > EnemyConvoyRouteBehaviourConstants::FarFromRouteWarningDistance)
	{
		UE_LOG(LogConvoy, Warning, TEXT("Convoy route %s: the branch fork is %.0f cm from the drive path."), *GetName(),
		       FMath::Abs(LateralOffset));
	}
	Plan.bEnabled = true;
	Plan.CargoType = Branch.CargoType;
	Plan.BranchPoints = BranchRoute.Points;
	Plan.ArrivalRadius = Branch.ArrivalRadius;
	Plan.SpeedMultiplier = Branch.SpeedMultiplier;
	Plan.bDespawnOnArrival = Branch.bDespawnOnArrival;
	Plan.MaxWaitAtExitSeconds = Branch.MaxWaitAtExitSeconds;
	return Plan;
}

FConvoyAmbushPlan AEnemyConvoyRoute::BuildAmbushPlan() const
{
	FConvoyAmbushPlan Plan;
	Plan.RadioPost = Ambush.RadioPostActor.Get();
	Plan.SpeedMultiplier = Ambush.SpeedMultiplier;
	TArray<const AActor*> ZoneActorsAdded;
	auto AddZone = [&Plan, this](const FVector& Centre)
	{
		FConvoyAmbushZonePlan& Zone = Plan.Zones.AddDefaulted_GetRef();
		Zone.Centre = Centre;
		Zone.Radius = Ambush.ZoneRadius;
	};
	for (const FVector& ZonePoint : GetWorldPoints(AmbushZonePoints))
	{
		AddZone(ZonePoint);
	}
	for (const TObjectPtr<AActor>& ZoneActor : Ambush.ZoneActors)
	{
		if (IsValid(ZoneActor))
		{
			AddZone(ZoneActor->GetActorLocation());
			ZoneActorsAdded.Add(ZoneActor.Get());
		}
	}
	if (Ambush.ZoneActorTag.IsNone())
	{
		return Plan;
	}
	TArray<AActor*> TaggedActors;
	UGameplayStatics::GetAllActorsWithTag(this, Ambush.ZoneActorTag, TaggedActors);
	for (const AActor* TaggedActor : TaggedActors)
	{
		if (IsValid(TaggedActor) && not ZoneActorsAdded.Contains(TaggedActor))
		{
			AddZone(TaggedActor->GetActorLocation());
		}
	}
	return Plan;
}

FConvoyHunterPlan AEnemyConvoyRoute::BuildHunterPlan(const FConvoyRoutePolyline& Polyline) const
{
	FConvoyHunterPlan Plan;
	Plan.DismountLocation = GetDismountWorldLocation();
	float LateralOffset = 0.f;
	Plan.DismountDistance = Polyline.ProjectLocation(Plan.DismountLocation, -1.f, 0.f, LateralOffset);
	if (FMath::Abs(LateralOffset) > EnemyConvoyRouteBehaviourConstants::FarFromRouteWarningDistance)
	{
		UE_LOG(LogConvoy, Warning, TEXT("Convoy route %s: the dismount point is %.0f cm from the drive path."),
		       *GetName(), FMath::Abs(LateralOffset));
	}
	const bool bHasPerTruckSquads = Hunters.PerTruckSquads.ContainsByPredicate(
		[](const FEnemyConvoyRouteHunterTruckLoad& Load)
		{
			return not Load.Squads.IsEmpty();
		});
	if (Hunters.SquadsPerTruck.IsEmpty() && not bHasPerTruckSquads)
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + " has hunters on but no squads in SquadsPerTruck "
			"or PerTruckSquads; the trucks halt at the dismount point but nobody gets out.");
	}
	Plan.bEnabled = true;
	Plan.DismountRadius = Hunters.DismountRadius;
	Plan.bDismountWhenAttacked = Hunters.bDismountWhenAttacked;
	Plan.DismountHaltSeconds = Hunters.DismountHaltSeconds;
	return Plan;
}

// ------------------------------------------------------------------
// Behaviour events
// ------------------------------------------------------------------

void AEnemyConvoyRoute::HandleConvoyBehaviourEvent(const FConvoyBehaviourEvent& Event)
{
	FEnemyConvoyRouteConvoyTally* FoundTally = FindTally(Event.ConvoyId);
	if (FoundTally == nullptr)
	{
		return;
	}
	if (Event.Type == EConvoyBehaviourEventType::BranchTruckArrived)
	{
		// Counts through the pointer before anything is broadcast; listeners may finish convoys and shrink M_Convoys.
		HandleBranchTruckArrived(Event, *FoundTally);
		return;
	}
	const FEnemyConvoyRouteConvoyTally Tally = *FoundTally;
	switch (Event.Type)
	{
	case EConvoyBehaviourEventType::StopStarted:
	case EConvoyBehaviourEventType::StopEnded:
		HandleStopEvent(Event, Tally);
		break;
	case EConvoyBehaviourEventType::BranchTruckLeftColumn:
		OnBranchTruckLeftColumn.Broadcast(this, Event.Truck, Event.CargoType, Tally.ConvoyIndex);
		break;
	case EConvoyBehaviourEventType::AmbushZoneEntered:
		OnConvoyEnteredAmbushZone.Broadcast(this, Tally.Convoy.Get(), Tally.ConvoyIndex, Event.ZoneIndex,
		                                    Event.bRadioPostAlive);
		break;
	case EConvoyBehaviourEventType::HuntersDismount:
		LaunchHunters(Event, Tally);
		break;
	default:
		break;
	}
}

void AEnemyConvoyRoute::HandleStopEvent(const FConvoyBehaviourEvent& Event, const FEnemyConvoyRouteConvoyTally& Tally)
{
	if (Event.Type == EConvoyBehaviourEventType::StopStarted)
	{
		OnConvoyStopStarted.Broadcast(this, Tally.Convoy.Get(), Tally.ConvoyIndex);
		return;
	}
	OnConvoyStopEnded.Broadcast(this, Tally.Convoy.Get(), Tally.ConvoyIndex, Event.StopEndReason);
}

void AEnemyConvoyRoute::HandleBranchTruckArrived(const FConvoyBehaviourEvent& Event,
                                                 FEnemyConvoyRouteConvoyTally& Tally)
{
	Tally.BranchDeliveredTruckCount++;
	M_BranchDeliveredTruckCount++;
	const int32 ConvoyIndex = Tally.ConvoyIndex;
	// Tally is not touched after this: a listener may end the convoy, which removes it from M_Convoys.
	OnBranchTruckArrived.Broadcast(this, Event.Truck, Event.CargoType, ConvoyIndex);
}

// ------------------------------------------------------------------
// Hunters
// ------------------------------------------------------------------

const TArray<FEnemyPlacementUnit>& AEnemyConvoyRoute::GetHunterLoad(const int32 TruckColumnIndex) const
{
	if (Hunters.PerTruckSquads.IsValidIndex(TruckColumnIndex)
		&& not Hunters.PerTruckSquads[TruckColumnIndex].Squads.IsEmpty())
	{
		return Hunters.PerTruckSquads[TruckColumnIndex].Squads;
	}
	return Hunters.SquadsPerTruck;
}

bool AEnemyConvoyRoute::TryGetHunterWaypoints(TArray<FVector>& OutWaypoints) const
{
	OutWaypoints = GetWorldPoints(HunterWaypoints);
	if (RTSFunctionLibrary::RTSIsValid(Hunters.AttackTarget))
	{
		OutWaypoints.Add(Hunters.AttackTarget->GetActorLocation());
	}
	return not OutWaypoints.IsEmpty();
}

AEnemyController* AEnemyConvoyRoute::FindEnemyControllerForHunters()
{
	if (M_EnemyController.IsValid())
	{
		return M_EnemyController.Get();
	}
	// The route does not wait for a controller (convoys need none), so look it up the way placed actors do.
	const int32 HunterPlayer = GetEnemyPlayer();
	AEnemyController* Found = HunterPlayer == RTSPlayers::FirstAIPlayer
		                          ? FRTS_Statics::GetEnemyController(this)
		                          : FRTS_Statics::GetEnemyControllerForPlayer(this, HunterPlayer);
	if (not IsValid(Found))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_EnemyController",
		                                                     "FindEnemyControllerForHunters", this);
		return nullptr;
	}
	M_EnemyController = Found;
	return Found;
}

void AEnemyConvoyRoute::AppendTruckHunterElements(const FVector& TruckLocation,
                                                  const TArray<FEnemyPlacementUnit>& Load,
                                                  const FVector& FirstWaypoint,
                                                  TArray<FAttackWaveElement>& OutElements) const
{
	// The squads jump out on the side that faces where they are going.
	const FVector UnloadDirection = (FirstWaypoint - TruckLocation).GetSafeNormal2D();
	const FVector UnloadCentre = TruckLocation + UnloadDirection * Hunters.UnloadSideOffset;
	for (int32 SquadIndex = 0; SquadIndex < Load.Num(); ++SquadIndex)
	{
		if (not Load[SquadIndex].GetIsSet())
		{
			continue;
		}
		FAttackWaveElement Element;
		Element.UnitOptions = {Load[SquadIndex].ToTrainingOption()};
		Element.SpawnLocation = EnemyPlacement::ProjectToNavigationIfPossible(
			GetWorld(), UnloadCentre + EnemyPlacement::GetRingSpreadOffset(SquadIndex));
		OutElements.Add(Element);
	}
}

TArray<FAttackWaveElement> AEnemyConvoyRoute::BuildHunterWaveElements(const FConvoyBehaviourEvent& Event,
                                                                      const FVector& FirstWaypoint) const
{
	TArray<FAttackWaveElement> Elements;
	const int32 TruckCount = FMath::Min(Event.TruckLocations.Num(), Event.TruckColumnIndices.Num());
	for (int32 TruckIndex = 0; TruckIndex < TruckCount; ++TruckIndex)
	{
		AppendTruckHunterElements(Event.TruckLocations[TruckIndex], GetHunterLoad(Event.TruckColumnIndices[TruckIndex]),
		                          FirstWaypoint, Elements);
	}
	return Elements;
}

FRotator AEnemyConvoyRoute::GetHunterFinalDirection(const TArray<FVector>& Waypoints, const FVector& StartLocation) const
{
	if (not Hunters.Attack.bFaceAlongLastLeg || Waypoints.IsEmpty())
	{
		return Hunters.Attack.FinalWaypointDirection;
	}
	const FVector LegStart = Waypoints.Num() >= 2 ? Waypoints[Waypoints.Num() - 2] : StartLocation;
	return FRotator(0.f, (Waypoints.Last() - LegStart).Rotation().Yaw, 0.f);
}

void AEnemyConvoyRoute::LaunchHunters(const FConvoyBehaviourEvent& Event, const FEnemyConvoyRouteConvoyTally& Tally)
{
	TArray<FVector> Waypoints;
	if (not TryGetHunterWaypoints(Waypoints))
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + ": the hunters have no AttackTarget and no "
			"HunterWaypoints; nobody gets out.");
		return;
	}
	FAttackWaveRequest Request;
	Request.WaveElements = BuildHunterWaveElements(Event, Waypoints[0]);
	if (Request.WaveElements.IsEmpty())
	{
		UE_LOG(LogConvoy, Warning, TEXT("Convoy route %s: no living truck carries a picked hunter squad."), *GetName());
		return;
	}
	AEnemyController* EnemyController = FindEnemyControllerForHunters();
	if (not IsValid(EnemyController))
	{
		return;
	}
	Request.WaveType = EEnemyWaveType::Wave_NoOwningBuilding;
	Request.IterationCount = 1;
	Request.FirstIterationDelay = 0.f;
	Request.Movement = EAttackWaveMovement::AttackMove;
	Request.Waypoints = Waypoints;
	Request.FinalWaypointDirection = GetHunterFinalDirection(Waypoints, Event.Location);
	Request.MaxFormationWidth = Hunters.Attack.Formation.MaxFormationWidth;
	Request.FormationOffsetMultiplier = Hunters.Attack.Formation.FormationOffsetMultiplier;
	Request.AttackMoveSettings = Hunters.Attack.CombatSupport;
	const int32 HunterRequestId = M_NextHunterRequestId++;
	Request.OnUnitsSpawned.BindUObject(this, &AEnemyConvoyRoute::HandleHunterUnitsSpawned, HunterRequestId,
	                                   Tally.ConvoyIndex, Tally.Convoy);
	Request.OnWaveEnded.BindUObject(this, &AEnemyConvoyRoute::HandleHunterWaveEnded, HunterRequestId,
	                                Tally.ConvoyIndex, Tally.Convoy);
	const int32 WaveId = EnemyController->StartWaveFromRequest(Request);
	if (WaveId == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + " could not start the hunter wave; see the error "
			"above.");
		return;
	}
	UE_LOG(LogConvoy, Log, TEXT("Convoy route %s: convoy %d unloads %d hunter squads (wave %d)."), *GetName(),
	       Tally.ConvoyIndex, Request.WaveElements.Num(), WaveId);
}

void AEnemyConvoyRoute::HandleHunterUnitsSpawned(const int32 WaveId, const TArray<AActor*>& Units,
                                                 const int32 HunterRequestId, const int32 ConvoyIndex,
                                                 TWeakObjectPtr<AConvoy> Convoy)
{
	M_HunterRequestsWithUnits.AddUnique(HunterRequestId);
	OnHuntersDismounted.Broadcast(this, Convoy.Get(), ConvoyIndex, Units);
}

void AEnemyConvoyRoute::HandleHunterWaveEnded(const int32 WaveId, const int32 HunterRequestId, const int32 ConvoyIndex,
                                              TWeakObjectPtr<AConvoy> Convoy)
{
	if (M_HunterRequestsWithUnits.Remove(HunterRequestId) > 0)
	{
		return;
	}
	UE_LOG(LogConvoy, Warning, TEXT("Convoy route %s: the hunter wave of convoy %d (wave %d) spawned no squads; the "
		       "enemy has no wave supply left or every spawn failed."), *GetName(), ConvoyIndex, WaveId);
	OnHuntersDismounted.Broadcast(this, Convoy.Get(), ConvoyIndex, TArray<AActor*>());
}

// ------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------

void AEnemyConvoyRoute::AddBehaviourShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyConvoyRouteBehaviourConstants;
	const FColor Color = EnemyPlacementColors::GetDimmed(GetPlacementColor(), DimmedBrightness);
	if (Stop.bEnabled)
	{
		OutSummary.AddFlatBox(LiftBehaviourShape(GetStopWorldLocation()), StopBoxHalfExtent, GetActorRotation().Yaw,
		                      Color, false);
	}
	if (Branch.bEnabled)
	{
		const TArray<FVector> BranchControlPoints = GetBranchControlPoints();
		for (int32 PointIndex = 1; PointIndex < BranchControlPoints.Num(); ++PointIndex)
		{
			const FVector LegStart = LiftBehaviourShape(BranchControlPoints[PointIndex - 1]);
			const FVector LegEnd = LiftBehaviourShape(BranchControlPoints[PointIndex]);
			OutSummary.Lines.Emplace(LegStart, LegEnd, Color, BranchThickness);
			OutSummary.ArrowLines.Emplace(LegStart, LegEnd, Color);
		}
		if (not BranchControlPoints.IsEmpty())
		{
			OutSummary.AddCircle(LiftBehaviourShape(BranchControlPoints.Last()), Branch.ArrivalRadius, Color,
			                     RingThickness);
		}
	}
	for (const FVector& ZonePoint : GetWorldPoints(AmbushZonePoints))
	{
		OutSummary.AddCircle(LiftBehaviourShape(ZonePoint), Ambush.ZoneRadius, EnemyPlacementColors::Problem,
		                     RingThickness);
	}
	if (not Hunters.bEnabled)
	{
		return;
	}
	const FVector Dismount = GetDismountWorldLocation();
	OutSummary.AddCircle(LiftBehaviourShape(Dismount), Hunters.DismountRadius, Color, RingThickness);
	TArray<FVector> Waypoints;
	if (not TryGetHunterWaypoints(Waypoints))
	{
		return;
	}
	FVector LegStart = Dismount;
	for (const FVector& Waypoint : Waypoints)
	{
		OutDetail.DashedLines.Emplace(LiftBehaviourShape(LegStart), LiftBehaviourShape(Waypoint), Color, DashSize);
		LegStart = Waypoint;
	}
}
