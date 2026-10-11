// Copyright (C) Bas Blokzijl - All rights reserved.

// The part of UTrackPathFollowingComponent that keeps vehicles out of each other before their hulls touch.

#include "TrackPathFollowingComponent.h"

#include "AISystem.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Tanks/AITankMaster.h"
#include "RTS_Survival/Units/Tanks/FRTSOverlapEvasion/RTSOverlapEvasionComponent.h"
#include "RTS_Survival/Units/Tanks/VehicleAvoidance/RTSVehicleAvoidanceSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace TrackFollowingAvoidance
{
	/** How often a driving vehicle looks up who is near; in between it follows the vehicles it already knows. */
	constexpr float NeighbourRefreshSeconds = 0.15f;
	/** Centre distance within which other vehicles are kept as neighbours. Covers the longest look-ahead. */
	constexpr float NeighbourRange = 2600.f;

	/** Seconds of driving a vehicle looks ahead. Higher makes it react to others earlier and steer wider. */
	constexpr float LookAheadSeconds = 1.6f;
	constexpr float MinimumLookAhead = 450.f;
	constexpr float MaximumLookAhead = 1500.f;
	/** Furthest a vehicle looks ahead for one that drives towards it. Stays inside the neighbour range. */
	constexpr float MaximumOncomingLookAhead = 2200.f;

	/** Gap kept to a vehicle ahead while waiting behind it. Lower packs queues tighter but touches more often. */
	constexpr float WaitingStopGap = 90.f;
	/** Speed allowed per centimetre of free gap beyond the stop gap. Higher closes up faster and brakes later. */
	constexpr float GapClosingRate = 1.4f;
	/** Sideways room wanted next to a vehicle that is being passed. */
	constexpr float PassingSideMargin = 70.f;
	/** How hard a vehicle ahead pulls the steering aside. Higher turns away sooner and sharper. */
	constexpr float PassingStrength = 1.7f;
	/** Part of the passing pull that applies even when only the edge of the other vehicle is in the way. */
	constexpr float PassingMinimumDepthWeight = 0.35f;
	/** Passing pull that remains for a vehicle driving the same way: follow it rather than overtake it. */
	constexpr float FollowingSteeringScale = 0.2f;
	/** A vehicle this close to dead ahead is passed on the right, so two facing each other choose opposite sides. */
	constexpr float CenteredTolerance = 40.f;
	/** Part of the clearance a vehicle has to be off to one side before a kept passing side is given up. */
	constexpr float PassSideSwitchFraction = 0.6f;
	/** How long a chosen passing side is kept whoever is nearest. Higher weaves less but reacts later to a change. */
	constexpr float PassSideHoldSeconds = 2.5f;
	/** Speed and direction match at which another vehicle counts as leading the same way. */
	constexpr float SameWayDirectionDot = 0.8f;
	constexpr float SameWayMinimumSpeed = 60.f;

	/** Hull gap below which vehicles drift apart sideways, like a flock keeping its spacing. */
	constexpr float SeparationRange = 220.f;
	constexpr float SeparationStrength = 0.7f;

	/** Largest sideways pull while driving normally; 1 is 45 degrees off the path. */
	constexpr float MaximumPush = 1.5f;
	constexpr float MaximumFlockingPush = 3.f;

	/** Avoidance fades out this close to the goal, in hull corner radii, so the exact spot is still reached. */
	constexpr float GoalFadeStartRadii = 1.f;
	constexpr float GoalFadeRangeRadii = 2.f;

	/** Below this speed, held back by other vehicles, a vehicle counts as waiting. */
	constexpr float WaitingSpeed = 30.f;
	constexpr float WaitingSpeedCap = 60.f;
	/** How long a vehicle waits before it starts to flock. Every vehicle adds its own bit so they do not all go at once. */
	constexpr float WaitingPatienceSeconds = 2.5f;
	constexpr float WaitingPatienceStaggerSeconds = 0.25f;
	/** Patience of two vehicles that wait for each other: one starts to squeeze past soon, the other a bit later. */
	constexpr float MutualWaitPatienceSeconds = 0.5f;
	constexpr float MutualWaitYieldSeconds = 1.5f;
	/** Least sideways pull of a flocking vehicle with a hull dead ahead, so it turns off instead of leaning on it. */
	constexpr float FlockingMinimumPush = 1.2f;
	constexpr uint32 WaitingPatienceStaggerSteps = 8;
	/** How fast waiting time is forgotten once the vehicle drives again. */
	constexpr float WaitingRecoveryRate = 2.f;

	/** How long a vehicle keeps flocking before it goes back to waiting. */
	constexpr float FlockingSeconds = 7.f;
	/** Speed a flocking vehicle keeps even with something right in front of it; this is what pushes. */
	constexpr float FlockingCreepSpeed = 110.f;
	constexpr float FlockingStopGap = 15.f;
	/** Throttle allowed while a flocking vehicle leans on another one. Higher pushes faster. */
	constexpr float PushingThrottleLimit = 0.1f;
	constexpr float PushingContactGap = 45.f;
	/** Part of its drive force a vehicle pushes with. Higher shoves waiting vehicles further and turns parked ones. */
	constexpr float PushingDriveStrength = 0.4f;
	/** Part of its braking a waiting vehicle keeps. Lower lets a flocking vehicle shove it aside more easily. */
	constexpr float WaitingHoldStrength = 0.15f;

	/** Standing against a hull this long, a flocking vehicle backs off to try another way past. */
	constexpr float WedgedSecondsBeforeBackingOff = 1.2f;
	constexpr float WedgedSpeed = 12.f;
	constexpr float BackOffSeconds = 1.4f;
	constexpr float SecondsBetweenBackOffs = 4.f;
	constexpr float BackOffSteering = 0.7f;
	/** Room needed behind the hull to back off into, and how far behind to look for it. */
	constexpr float BackOffMinimumRoom = 60.f;
	constexpr float BackOffLookBehind = 220.f;

	/** Held up this long by a parked vehicle, the path is planned again; parked vehicles are cut out of the navmesh. */
	constexpr float RepathAfterBlockedSeconds = 2.f;
	constexpr float SecondsBetweenRepaths = 5.f;

	/** How long an idle ally may hold a vehicle up before it is told to move aside. */
	constexpr float SecondsBeforeDisplacingIdleAlly = 1.f;
	constexpr float SecondsBetweenDisplacements = 3.f;
	constexpr float DisplacementGap = 220.f;

	/** How long a goal may stay out of reach behind vehicles that stay, before the vehicle settles where it is. */
	constexpr float SettleSecondsWhenSentToSameSpot = 1.f;
	/** Vehicles that all drive to one spot settle around it once they are this close, in hull corner radii. */
	constexpr float ContestedGoalRadii = 3.5f;
	constexpr float SettleSecondsAroundContestedGoal = 2.5f;
	constexpr float SettleSecondsWhenGoalIsCovered = 4.f;
	constexpr float SettleSecondsNearGoal = 8.f;
	constexpr float NearGoalRadii = 2.5f;

	/** Without getting anywhere for this long the engine may end the move as blocked again. */
	constexpr float MaximumBlockedSeconds = 25.f;
	constexpr float BlockedProgressDistance = 150.f;

	constexpr float StationarySpeed = 25.f;
	constexpr float MinimumDirectionLength = 1.f;
	constexpr float DebugZOffset = 520.f;
}

void UTrackPathFollowingComponent::RegisterVehicleForAvoidance()
{
	APawn* Pawn = GetValidControlledPawn();
	const UWorld* World = GetWorld();
	if (not IsValid(Pawn) || not IsValid(World))
	{
		return;
	}
	URTSVehicleAvoidanceSubsystem* AvoidanceSubsystem = World->GetSubsystem<URTSVehicleAvoidanceSubsystem>();
	if (not IsValid(AvoidanceSubsystem))
	{
		return;
	}

	AvoidanceSubsystem->RegisterVehicle(this, Pawn);
	M_VehicleAvoidance.bIsRegistered = true;
	M_VehicleAvoidance.Footprint = FRTSVehicleFootprint();
}

bool UTrackPathFollowingComponent::GetIsWaitingForVehicles() const
{
	return GetIsFollowingPath() && M_VehicleAvoidance.LastSpeedCap < MAX_flt &&
		M_VehicleAvoidance.BlockedSeconds < TrackFollowingAvoidance::MaximumBlockedSeconds;
}

bool UTrackPathFollowingComponent::GetHasGoalNear(const FVector& Location, const float Tolerance) const
{
	return M_VehicleAvoidance.bHasLastGoalLocation &&
		FVector::DistSquared2D(M_VehicleAvoidance.LastGoalLocation, Location) <= FMath::Square(Tolerance);
}

bool UTrackPathFollowingComponent::GetHasStoppedForGoalNear(const FVector& Location, const float Tolerance) const
{
	return not GetIsFollowingPath() && GetHasGoalNear(Location, Tolerance);
}

void UTrackPathFollowingComponent::RefreshVehicleAvoidanceNeighbours(const float DeltaTime)
{
	M_VehicleAvoidance.SecondsUntilNeighbourRefresh -= DeltaTime;
	if (M_VehicleAvoidance.SecondsUntilNeighbourRefresh > 0.f)
	{
		return;
	}
	M_VehicleAvoidance.SecondsUntilNeighbourRefresh = TrackFollowingAvoidance::NeighbourRefreshSeconds;

	URTSVehicleAvoidanceSubsystem* AvoidanceSubsystem = GetWorld()->GetSubsystem<URTSVehicleAvoidanceSubsystem>();
	if (not IsValid(AvoidanceSubsystem))
	{
		M_VehicleAvoidance.Neighbours.Reset();
		return;
	}
	if (not M_VehicleAvoidance.Footprint.GetIsValid())
	{
		M_VehicleAvoidance.Footprint = AvoidanceSubsystem->GetFootprint(this);
	}
	AvoidanceSubsystem->FindNeighbours(
		this,
		ControlledPawn->GetActorLocation(),
		TrackFollowingAvoidance::NeighbourRange,
		M_VehicleAvoidance.Neighbours);
}

bool UTrackPathFollowingComponent::BuildVehicleAvoidanceFrame(
	const FVector& Destination,
	FVehicleAvoidanceFrame& OutFrame)
{
	using namespace TrackFollowingAvoidance;
	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const FVector2D ToDestination(Destination - PawnLocation);
	if (ToDestination.SizeSquared() <= FMath::Square(MinimumDirectionLength))
	{
		return false;
	}

	OutFrame.Forward = FVector2D(ControlledPawn->GetActorForwardVector()).GetSafeNormal();
	OutFrame.Location = M_VehicleAvoidance.Footprint.GetWorldCenter(FVector2D(PawnLocation), OutFrame.Forward);
	OutFrame.TravelDirection = ToDestination.GetSafeNormal();
	OutFrame.TravelRight = FVector2D(-OutFrame.TravelDirection.Y, OutFrame.TravelDirection.X);
	OutFrame.DriveDirection = bReversing ? -OutFrame.Forward : OutFrame.Forward;
	OutFrame.DriveRight = FVector2D(-OutFrame.DriveDirection.Y, OutFrame.DriveDirection.X);
	OutFrame.Speed = ControlledPawn->GetVelocity().Size2D();
	OutFrame.LookAhead = FMath::Clamp(OutFrame.Speed * LookAheadSeconds, MinimumLookAhead, MaximumLookAhead);
	OutFrame.DistanceToGoal = GetPathLengthRemaining();
	OutFrame.GoalLocation = Path.IsValid() && Path->GetPathPoints().Num() > 0
		                        ? FVector2D(Path->GetPathPoints().Last().Location)
		                        : FVector2D(Destination);
	return true;
}

bool UTrackPathFollowingComponent::GetIsNeighbourLeadingTheSameWay(
	const FRTSVehicleAvoidanceAgent& Neighbour,
	const FVehicleAvoidanceFrame& Frame) const
{
	using namespace TrackFollowingAvoidance;
	const APawn* Other = Neighbour.Pawn.Get();
	const UTrackPathFollowingComponent* OtherPathFollowing = Neighbour.PathFollowingComponent.Get();
	if (not IsValid(Other) || not IsValid(OtherPathFollowing) || not OtherPathFollowing->GetIsFollowingPath())
	{
		return false;
	}
	const FVector2D OtherVelocity(Other->GetVelocity());
	if (FVector2D::DotProduct(OtherVelocity, Frame.TravelDirection) < SameWayMinimumSpeed)
	{
		return false;
	}
	const FVector2D OtherTravelDirection(GetMovingOverlapTravelDirection(Other, OtherPathFollowing));
	return FVector2D::DotProduct(OtherTravelDirection, Frame.TravelDirection) >= SameWayDirectionDot;
}

void UTrackPathFollowingComponent::ApplyNeighbourToSpeedCap(
	const FRTSVehicleAvoidanceAgent& Neighbour,
	const FVector2D& Relative,
	const FVector2D& OtherForward,
	FVehicleAvoidanceFrame& Frame) const
{
	using namespace TrackFollowingAvoidance;
	const float Longitudinal = FVector2D::DotProduct(Relative, Frame.DriveDirection);
	const FRTSVehicleFootprint& MyFootprint = M_VehicleAvoidance.Footprint;
	const float FarthestReach = MyFootprint.GetCornerRadius() + Neighbour.Footprint.GetCornerRadius() +
		Frame.LookAhead;
	if (Longitudinal <= 0.f || Relative.SizeSquared() > FMath::Square(FarthestReach))
	{
		return;
	}
	const float Gap = RTSVehicleFootprint::GetFreeTravelDistance(
		Frame.Location,
		Frame.DriveDirection,
		MyFootprint,
		Frame.Location + Relative,
		OtherForward,
		Neighbour.Footprint,
		Frame.LookAhead);
	if (Gap >= Frame.LookAhead)
	{
		return;
	}
	const float Lateral = FVector2D::DotProduct(Relative, Frame.DriveRight);
	const float LateralReach = MyFootprint.GetReachAlong(Frame.Forward, Frame.DriveRight) +
		Neighbour.Footprint.GetReachAlong(OtherForward, Frame.DriveRight);

	const APawn* Other = Neighbour.Pawn.Get();
	const FVector2D OtherVelocity(Other->GetVelocity());
	const bool bIsFlocking = M_VehicleAvoidance.Mode == EVehicleAvoidanceMode::Flocking;
	const float StopGap = bIsFlocking ? FlockingStopGap : WaitingStopGap;
	// Follow at the speed of what is ahead, plus whatever the free gap still allows.
	float AllowedSpeed = FMath::Max(0.f, FVector2D::DotProduct(OtherVelocity, Frame.DriveDirection)) +
		FMath::Max(0.f, Gap - StopGap) * GapClosingRate;
	if (bIsFlocking)
	{
		AllowedSpeed = FMath::Max(AllowedSpeed, FlockingCreepSpeed);
	}
	Frame.SpeedCap = FMath::Min(Frame.SpeedCap, AllowedSpeed);
	if (Gap >= Frame.BlockerGap)
	{
		return;
	}
	Frame.BlockerGap = Gap;
	Frame.Blocker = Other;
	Frame.BlockerPathFollowing = Neighbour.PathFollowingComponent.Get();
	Frame.bIsBlockerStationary = OtherVelocity.SizeSquared() < FMath::Square(StationarySpeed);
	Frame.bIsBlockerStraightAhead = FMath::Abs(Lateral) < LateralReach * 0.5f;
}

float UTrackPathFollowingComponent::ChoosePassingSide(
	const APawn* Other,
	const float Lateral,
	const float Clearance,
	const float Gap,
	FVehicleAvoidanceFrame& Frame)
{
	using namespace TrackFollowingAvoidance;
	// Positive steers to the right of the travel direction. Away from the side the other vehicle is on.
	const float SideAwayFromOther = Lateral > 0.f ? -1.f : 1.f;
	const bool bIsCentered = FMath::Abs(Lateral) < CenteredTolerance;
	float Side = bIsCentered ? 1.f : SideAwayFromOther;

	const bool bHasKeptSide = M_VehicleAvoidance.PassSideVehicle.Get() == Other ||
		M_VehicleAvoidance.PassSideHoldSecondsLeft > 0.f;
	const bool bIsClearlyOffToOneSide = FMath::Abs(Lateral) > Clearance * PassSideSwitchFraction;
	if (bHasKeptSide && not bIsClearlyOffToOneSide)
	{
		Side = M_VehicleAvoidance.PassSide;
	}
	if (Gap < Frame.NearestPassingGap)
	{
		Frame.NearestPassingGap = Gap;
		if (M_VehicleAvoidance.PassSideVehicle.Get() != Other || M_VehicleAvoidance.PassSide != Side)
		{
			M_VehicleAvoidance.PassSideHoldSecondsLeft = PassSideHoldSeconds;
		}
		M_VehicleAvoidance.PassSideVehicle = Other;
		M_VehicleAvoidance.PassSide = Side;
	}
	return Side;
}

void UTrackPathFollowingComponent::ApplyNeighbourToSteering(
	const FRTSVehicleAvoidanceAgent& Neighbour,
	const FVector2D& Relative,
	const FVector2D& OtherForward,
	FVehicleAvoidanceFrame& Frame)
{
	using namespace TrackFollowingAvoidance;
	const FRTSVehicleFootprint& MyFootprint = M_VehicleAvoidance.Footprint;
	const float Longitudinal = FVector2D::DotProduct(Relative, Frame.TravelDirection);
	const float MyReach = MyFootprint.GetReachAlong(Frame.Forward, Frame.TravelDirection);
	const float Gap = Longitudinal - MyReach -
		Neighbour.Footprint.GetReachAlong(OtherForward, Frame.TravelDirection);
	// A vehicle that comes the other way closes the gap at both speeds; it is seen from that much further off,
	// so the two have moved aside before they meet instead of stopping nose to nose.
	const float OncomingSpeed = FMath::Max(
		0.f,
		-FVector2D::DotProduct(FVector2D(Neighbour.Pawn->GetVelocity()), Frame.TravelDirection));
	const float LookAhead = FMath::Min(
		Frame.LookAhead + OncomingSpeed * LookAheadSeconds,
		MaximumOncomingLookAhead);
	if (Longitudinal < -MyReach || Gap > LookAhead)
	{
		return;
	}

	const float Lateral = FVector2D::DotProduct(Relative, Frame.TravelRight);
	const float AbsoluteLateral = FMath::Abs(Lateral);
	const float LateralReach = MyFootprint.GetReachAlong(Frame.Forward, Frame.TravelRight) +
		Neighbour.Footprint.GetReachAlong(OtherForward, Frame.TravelRight);
	const float Clearance = LateralReach + PassingSideMargin;
	if (Longitudinal > 0.f && AbsoluteLateral < Clearance)
	{
		const float Side = ChoosePassingSide(Neighbour.Pawn.Get(), Lateral, Clearance, Gap, Frame);
		const float Urgency = 1.f - FMath::Clamp(Gap / LookAhead, 0.f, 1.f);
		const float Depth = 1.f - AbsoluteLateral / Clearance;
		float Weight = Urgency * FMath::Lerp(PassingMinimumDepthWeight, 1.f, Depth);
		const bool bShouldFollow = M_VehicleAvoidance.Mode != EVehicleAvoidanceMode::Flocking &&
			GetIsNeighbourLeadingTheSameWay(Neighbour, Frame);
		Weight *= bShouldFollow ? FollowingSteeringScale : 1.f;
		Frame.LateralPush += Side * Weight * PassingStrength;
	}

	const float HullGap = FMath::Max(Gap, AbsoluteLateral - LateralReach);
	if (HullGap < SeparationRange && AbsoluteLateral > CenteredTolerance)
	{
		const float Closeness = 1.f - FMath::Max(HullGap, 0.f) / SeparationRange;
		Frame.LateralPush += (Lateral > 0.f ? -1.f : 1.f) * Closeness * SeparationStrength;
	}
}

void UTrackPathFollowingComponent::UpdateVehicleAvoidanceMode(
	const FVehicleAvoidanceFrame& Frame,
	const float DeltaTime)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	const bool bIsWaiting = Frame.Speed < WaitingSpeed && Frame.SpeedCap < WaitingSpeedCap;
	if (State.Mode == EVehicleAvoidanceMode::Flocking)
	{
		State.FlockingSecondsLeft -= DeltaTime;
		const bool bIsFree = Frame.Blocker == nullptr;
		if (State.FlockingSecondsLeft <= 0.f || bIsFree)
		{
			State.Mode = EVehicleAvoidanceMode::Driving;
			State.WaitingSeconds = 0.f;
		}
		return;
	}

	State.WaitingSeconds = bIsWaiting
		                       ? State.WaitingSeconds + DeltaTime
		                       : FMath::Max(0.f, State.WaitingSeconds - DeltaTime * WaitingRecoveryRate);
	TryStartFlocking(Frame);
}

void UTrackPathFollowingComponent::TryStartFlocking(const FVehicleAvoidanceFrame& Frame)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	if (Frame.Blocker == nullptr)
	{
		return;
	}
	// Two vehicles that wait for each other would wait for nothing; the one with the lower id goes first.
	const bool bIsWaitedForByBlocker = IsValid(Frame.BlockerPathFollowing) &&
		Frame.BlockerPathFollowing->GetVehicleAvoidanceState().LastBlocker.Get() == ControlledPawn;
	const bool bGoesFirst = bIsWaitedForByBlocker && ControlledPawn->GetUniqueID() < Frame.Blocker->GetUniqueID();
	const float BasePatience = bIsWaitedForByBlocker
		                           ? (bGoesFirst ? MutualWaitPatienceSeconds : MutualWaitYieldSeconds)
		                           : WaitingPatienceSeconds;
	const float Patience = BasePatience +
		(ControlledPawn->GetUniqueID() % WaitingPatienceStaggerSteps) * WaitingPatienceStaggerSeconds;
	// Shuffling back and forth behind others without getting anywhere is waiting too, however fast the hull moves.
	if (FMath::Max(State.WaitingSeconds, State.BlockedSeconds) < Patience)
	{
		return;
	}
	State.Mode = EVehicleAvoidanceMode::Flocking;
	State.FlockingSecondsLeft = FlockingSeconds;
}

void UTrackPathFollowingComponent::TryMakeIdleBlockerMoveAside(
	const FVehicleAvoidanceFrame& Frame,
	const float DeltaTime)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	State.SecondsUntilDisplacementAllowed -= DeltaTime;
	const bool bIsHeldUpByStandingVehicle = Frame.Blocker != nullptr && Frame.bIsBlockerStationary &&
		Frame.bIsBlockerStraightAhead && Frame.BlockerGap < DisplacementGap;
	if (not bIsHeldUpByStandingVehicle || State.WaitingSeconds < SecondsBeforeDisplacingIdleAlly ||
		State.SecondsUntilDisplacementAllowed > 0.f)
	{
		return;
	}
	// Vehicles sent to one spot would keep chasing each other off it; those settle around it instead.
	const bool bWasSentToSameSpot = IsValid(Frame.BlockerPathFollowing) &&
		Frame.BlockerPathFollowing->GetHasStoppedForGoalNear(
			FVector(Frame.GoalLocation, 0.f),
			State.Footprint.GetCornerRadius());
	if (bWasSentToSameSpot)
	{
		return;
	}

	if (not M_RTSOverlapEvasionComponent.IsValid())
	{
		M_RTSOverlapEvasionComponent = ControlledPawn->FindComponentByClass<URTSOverlapEvasionComponent>();
	}
	if (not M_RTSOverlapEvasionComponent.IsValid())
	{
		return;
	}
	AActor* BlockingActor = const_cast<APawn*>(Frame.Blocker);
	State.SecondsUntilDisplacementAllowed = SecondsBetweenDisplacements;
	// Only an idle ally accepts this; it then drives aside and this vehicle waits for it as before.
	if (M_RTSOverlapEvasionComponent->TryDisplaceIdleOverlappingVehicle(BlockingActor))
	{
		RegisterIdleBlockingActor(BlockingActor);
		++State.DisplacementsAsked;
	}
}

void UTrackPathFollowingComponent::TryRequestRepathAroundParkedBlocker(
	const FVehicleAvoidanceFrame& Frame,
	const float DeltaTime)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	State.SecondsUntilRepathAllowed -= DeltaTime;
	const bool bIsHeldUpByParkedVehicle = Frame.Blocker != nullptr && Frame.bIsBlockerStationary &&
		IsValid(Frame.BlockerPathFollowing) && not Frame.BlockerPathFollowing->GetIsFollowingPath();
	if (not bIsHeldUpByParkedVehicle || State.BlockedSeconds < RepathAfterBlockedSeconds ||
		State.SecondsUntilRepathAllowed > 0.f)
	{
		return;
	}
	// On the last stretch a new path changes nothing; there the vehicle settles or has the other make room.
	if (Frame.DistanceToGoal < State.Footprint.GetCornerRadius() * NearGoalRadii)
	{
		return;
	}
	State.SecondsUntilRepathAllowed = SecondsBetweenRepaths;
	State.bWantsRepath = true;
}

void UTrackPathFollowingComponent::RepathAroundParkedVehicles()
{
	AAITankMaster* TankController = Cast<AAITankMaster>(GetOwner());
	if (not IsValid(TankController) || not GetIsFollowingPath() ||
		not FAISystem::IsValidLocation(OriginalMoveRequestGoalLocation))
	{
		return;
	}
	// The same move goes on with a new path; what the avoidance learned about this jam stays.
	M_VehicleAvoidance.bIsRepathing = true;
	TankController->TryRepathActiveMovementRequest(OriginalMoveRequestGoalLocation);
	M_VehicleAvoidance.bIsRepathing = false;
	// Replacing the path ended the old one as aborted; the move itself is still under way.
	M_VehicleAvoidance.LastMoveEnd = EVehicleMoveEnd::None;
}

float UTrackPathFollowingComponent::GetFreeDistanceBehind(const FVehicleAvoidanceFrame& Frame) const
{
	using namespace TrackFollowingAvoidance;
	float FreeDistance = BackOffLookBehind;
	for (const FRTSVehicleAvoidanceAgent& Neighbour : M_VehicleAvoidance.Neighbours)
	{
		const APawn* Other = Neighbour.Pawn.Get();
		if (not IsValid(Other))
		{
			continue;
		}
		const FVector2D OtherForward = FVector2D(Other->GetActorForwardVector()).GetSafeNormal();
		FreeDistance = FMath::Min(FreeDistance, RTSVehicleFootprint::GetFreeTravelDistance(
			Frame.Location,
			-Frame.DriveDirection,
			M_VehicleAvoidance.Footprint,
			Neighbour.Footprint.GetWorldCenter(FVector2D(Other->GetActorLocation()), OtherForward),
			OtherForward,
			Neighbour.Footprint,
			BackOffLookBehind));
	}
	return FreeDistance;
}

void UTrackPathFollowingComponent::UpdateBackOff(
	const FVehicleAvoidanceFrame& Frame,
	const float DeltaTime,
	FVehicleAvoidanceOutput& Output)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	State.SecondsUntilBackOffAllowed -= DeltaTime;
	if (State.BackOffSecondsLeft > 0.f)
	{
		State.BackOffSecondsLeft -= DeltaTime;
		if (GetFreeDistanceBehind(Frame) < BackOffMinimumRoom)
		{
			State.BackOffSecondsLeft = 0.f;
			return;
		}
		Output.bShouldBackOff = true;
		// The nose swings to the side the vehicle wants to pass on; reversing, the tail is the nose.
		Output.BackOffSteering = State.PassSide * BackOffSteering * (bReversing ? -1.f : 1.f);
		return;
	}

	const bool bIsWedged = State.Mode == EVehicleAvoidanceMode::Flocking && Frame.Speed < WedgedSpeed &&
		Frame.Blocker != nullptr && Frame.BlockerGap < PushingContactGap;
	State.WedgedSeconds = bIsWedged ? State.WedgedSeconds + DeltaTime : 0.f;
	if (State.WedgedSeconds < WedgedSecondsBeforeBackingOff || State.SecondsUntilBackOffAllowed > 0.f)
	{
		return;
	}
	State.WedgedSeconds = 0.f;
	State.BackOffSecondsLeft = BackOffSeconds;
	State.SecondsUntilBackOffAllowed = SecondsBetweenBackOffs;
}

void UTrackPathFollowingComponent::UpdateGoalBlockedByVehicles(
	FVehicleAvoidanceFrame& Frame,
	const bool bIsStandingStill,
	const float DeltaTime)
{
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	const float SettleSeconds = GetSecondsBeforeSettling(Frame, bIsStandingStill);
	if (SettleSeconds < 0.f)
	{
		State.GoalBlockedSeconds = FMath::Max(0.f, State.GoalBlockedSeconds - DeltaTime);
		return;
	}

	State.GoalBlockedSeconds += DeltaTime;
	if (State.GoalBlockedSeconds < SettleSeconds)
	{
		return;
	}
	State.bShouldSettleNearGoal = true;
	State.LastMoveEnd = Frame.PendingMoveEnd;
}

float UTrackPathFollowingComponent::GetSecondsBeforeSettling(
	FVehicleAvoidanceFrame& Frame,
	const bool bIsStandingStill) const
{
	using namespace TrackFollowingAvoidance;
	constexpr float KeepTrying = -1.f;
	if (Frame.Blocker == nullptr)
	{
		return KeepTrying;
	}

	const float CornerRadius = M_VehicleAvoidance.Footprint.GetCornerRadius();
	const bool bSharesGoal = IsValid(Frame.BlockerPathFollowing) &&
		Frame.BlockerPathFollowing->GetHasGoalNear(FVector(Frame.GoalLocation, 0.f), CornerRadius);
	const bool bIsHeldUpByStandingVehicle = bIsStandingStill && Frame.bIsBlockerStationary;
	if (bSharesGoal && bIsHeldUpByStandingVehicle && not Frame.BlockerPathFollowing->GetIsFollowingPath())
	{
		Frame.PendingMoveEnd = EVehicleMoveEnd::SettledAroundSharedGoal;
		return SettleSecondsWhenSentToSameSpot;
	}
	// Still driving, but to the same spot: around it nobody gets closer, they only keep each other out.
	if (bSharesGoal && Frame.DistanceToGoal < CornerRadius * ContestedGoalRadii)
	{
		Frame.PendingMoveEnd = EVehicleMoveEnd::SettledAroundSharedGoal;
		return SettleSecondsAroundContestedGoal;
	}
	if (not bIsHeldUpByStandingVehicle || Frame.DistanceToGoal >= CornerRadius * NearGoalRadii)
	{
		return KeepTrying;
	}
	if (Frame.bIsGoalCovered)
	{
		Frame.PendingMoveEnd = EVehicleMoveEnd::SettledBesideCoveredGoal;
		return SettleSecondsWhenGoalIsCovered;
	}
	Frame.PendingMoveEnd = EVehicleMoveEnd::SettledNearGoal;
	return SettleSecondsNearGoal;
}

FVehicleAvoidanceOutput UTrackPathFollowingComponent::UpdateVehicleAvoidance(
	const FVector& Destination,
	const float DeltaTime)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceOutput Output;
	Output.SteeringDestination = Destination;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	if (not State.bIsRegistered)
	{
		RegisterVehicleForAvoidance();
	}
	RefreshVehicleAvoidanceNeighbours(DeltaTime);
	State.PassSideHoldSecondsLeft -= DeltaTime;

	FVehicleAvoidanceFrame Frame;
	if (not State.Footprint.GetIsValid() || not BuildVehicleAvoidanceFrame(Destination, Frame))
	{
		return Output;
	}

	WeighVehicleAvoidanceNeighbours(Frame);
	UpdateVehicleAvoidanceMode(Frame, DeltaTime);
	// Counts while flocking too: a vehicle that leans on others without moving is still not getting there.
	const bool bIsStandingStill = Frame.Speed < WaitingSpeed && Frame.Blocker != nullptr;
	TryMakeIdleBlockerMoveAside(Frame, DeltaTime);
	UpdateGoalBlockedByVehicles(Frame, bIsStandingStill, DeltaTime);
	const bool bIsHeldBack = UpdateTimeHeldBackByVehicles(Frame, DeltaTime);

	Output = BuildVehicleAvoidanceOutput(Destination, Frame, bIsHeldBack);
	TryRequestRepathAroundParkedBlocker(Frame, DeltaTime);
	UpdateBackOff(Frame, DeltaTime, Output);
	if (Output.bShouldBackOff)
	{
		Output.DriveStrength = 1.f;
		Output.ThrottleLimit = 1.f;
	}

	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		DebugVehicleAvoidance(Frame, DeltaTime);
	}
	return Output;
}

void UTrackPathFollowingComponent::WeighVehicleAvoidanceNeighbours(FVehicleAvoidanceFrame& Frame)
{
	using namespace TrackFollowingAvoidance;
	const FVehicleAvoidanceState& State = M_VehicleAvoidance;
	for (const FRTSVehicleAvoidanceAgent& Neighbour : State.Neighbours)
	{
		const APawn* Other = Neighbour.Pawn.Get();
		if (not IsValid(Other))
		{
			continue;
		}
		const FVector2D OtherForward = FVector2D(Other->GetActorForwardVector()).GetSafeNormal();
		const FVector2D OtherLocation = Neighbour.Footprint.GetWorldCenter(
			FVector2D(Other->GetActorLocation()),
			OtherForward);
		const FVector2D Relative = OtherLocation - Frame.Location;
		ApplyNeighbourToSpeedCap(Neighbour, Relative, OtherForward, Frame);
		ApplyNeighbourToSteering(Neighbour, Relative, OtherForward, Frame);

		const FVector2D OtherToGoal = Frame.GoalLocation - OtherLocation;
		const float OtherReachToGoal = Neighbour.Footprint.GetReachAlong(OtherForward, OtherToGoal.GetSafeNormal());
		const bool bStandsOnGoal = Other->GetVelocity().SizeSquared2D() < FMath::Square(StationarySpeed) &&
			OtherToGoal.SizeSquared() < FMath::Square(OtherReachToGoal + State.Footprint.HalfWidth);
		Frame.bIsGoalCovered = Frame.bIsGoalCovered || bStandsOnGoal;
	}
}

bool UTrackPathFollowingComponent::UpdateTimeHeldBackByVehicles(
	const FVehicleAvoidanceFrame& Frame,
	const float DeltaTime)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	if (FVector::DistSquared2D(PawnLocation, State.BlockedAnchorLocation) > FMath::Square(BlockedProgressDistance))
	{
		State.BlockedAnchorLocation = PawnLocation;
		State.BlockedSeconds = 0.f;
	}
	const bool bIsHeldBack = Frame.SpeedCap < MAX_flt;
	if (bIsHeldBack)
	{
		// Standing still on purpose is not being stuck: the unstuck teleport must not go off.
		State.BlockedSeconds += DeltaTime;
		StuckLocationSamples.Reset();
		NextStuckSampleIdx = 0;
	}
	return bIsHeldBack;
}

FVehicleAvoidanceOutput UTrackPathFollowingComponent::BuildVehicleAvoidanceOutput(
	const FVector& Destination,
	const FVehicleAvoidanceFrame& Frame,
	const bool bIsHeldBack)
{
	using namespace TrackFollowingAvoidance;
	FVehicleAvoidanceOutput Output;
	FVehicleAvoidanceState& State = M_VehicleAvoidance;
	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	const bool bIsFlocking = State.Mode == EVehicleAvoidanceMode::Flocking;
	const float CornerRadius = State.Footprint.GetCornerRadius();
	const float GoalWeight = FMath::Clamp(
		(Frame.DistanceToGoal - CornerRadius * GoalFadeStartRadii) / (CornerRadius * GoalFadeRangeRadii),
		0.f,
		1.f);
	const float PushLimit = bIsFlocking ? MaximumFlockingPush : MaximumPush;
	float WantedPush = Frame.LateralPush;
	const bool bShouldTurnOff = bIsFlocking && Frame.Blocker != nullptr && Frame.bIsBlockerStraightAhead &&
		FMath::Abs(WantedPush) < FlockingMinimumPush;
	if (bShouldTurnOff)
	{
		WantedPush = State.PassSide * FlockingMinimumPush;
	}
	const float Push = FMath::Clamp(WantedPush, -PushLimit, PushLimit) * GoalWeight;
	const FVector2D SteeringDirection = (Frame.TravelDirection + Frame.TravelRight * Push).GetSafeNormal();
	const float DestinationDistance2D = FVector::Dist2D(Destination, PawnLocation);
	Output.SteeringDestination = FVector(
		FVector2D(PawnLocation) + SteeringDirection * DestinationDistance2D,
		Destination.Z);
	Output.SpeedCap = Frame.SpeedCap;
	Output.bIsLimitingSpeed = bIsHeldBack;
	State.LastBlocker = Frame.Blocker;
	State.LastBlockerGap = Frame.BlockerGap;
	State.LastSpeedCap = Frame.SpeedCap;
	State.LastPush = Push;
	// After long enough without getting anywhere the engine may end the move as blocked again.
	Output.bShouldSuppressBlockDetection = bIsHeldBack && State.BlockedSeconds < MaximumBlockedSeconds;
	const bool bIsPushing = bIsFlocking && Frame.Blocker != nullptr && Frame.BlockerGap < PushingContactGap;
	Output.ThrottleLimit = bIsPushing ? PushingThrottleLimit : 1.f;
	const bool bIsWaitingInPlace = not bIsFlocking && Frame.SpeedCap < WaitingSpeedCap;
	Output.DriveStrength = bIsWaitingInPlace ? WaitingHoldStrength : 1.f;
	Output.DriveStrength = bIsPushing ? PushingDriveStrength : Output.DriveStrength;
	return Output;
}

bool UTrackPathFollowingComponent::GetIsTeleportLocationFreeOfVehicles(const FVector& TeleportLocation) const
{
	const UWorld* World = GetWorld();
	const URTSVehicleAvoidanceSubsystem* AvoidanceSubsystem = IsValid(World)
		                                                         ? World->GetSubsystem<URTSVehicleAvoidanceSubsystem>()
		                                                         : nullptr;
	if (not IsValid(AvoidanceSubsystem))
	{
		return true;
	}
	return AvoidanceSubsystem->GetIsLocationFreeForVehicle(TeleportLocation, ControlledPawn);
}

void UTrackPathFollowingComponent::DebugVehicleAvoidance(
	const FVehicleAvoidanceFrame& Frame,
	const float DeltaTime) const
{
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		if (Frame.Blocker == nullptr && FMath::IsNearlyZero(Frame.LateralPush))
		{
			return;
		}
		const bool bIsFlocking = M_VehicleAvoidance.Mode == EVehicleAvoidanceMode::Flocking;
		const FString DebugMessage = FString::Printf(
			TEXT("Avoid %s\nCap %.0f | Push %.2f | Waited %.1f | Goal blocked %.1f"),
			bIsFlocking ? TEXT("FLOCK") : TEXT("DRIVE"),
			Frame.SpeedCap < MAX_flt ? Frame.SpeedCap : -1.f,
			Frame.LateralPush,
			M_VehicleAvoidance.WaitingSeconds,
			M_VehicleAvoidance.GoalBlockedSeconds);
		RTSFunctionLibrary::DrawDebugAtLocation(
			this,
			GetAgentLocation() + FVector(0.f, 0.f, TrackFollowingAvoidance::DebugZOffset),
			DebugMessage,
			bIsFlocking ? FColor::Cyan : FColor::White,
			DeltaTime);
	}
}
