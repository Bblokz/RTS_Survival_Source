// Copyright (C) Bas Blokzijl - All rights reserved.

// Members of AConvoy that run the optional mission behaviours handed over with SetBehaviourPlan: a timed stop that
// is skipped or cut short (and the trucks floor it) when attacked, a branch for the trucks of one cargo type, ambush
// zones with a radio speed-up, the hunter dismount point, and the truck fates the cargo outcome sums up.
// All timing reads world seconds inside the convoy tick, so there are no timers to clear.

#include "Convoy.h"

#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace ConvoyBehaviourConstants
{
	// "Attacked" holds this long after the last threat or hit the combat update saw (one convoy tick and a bit).
	constexpr double AttackedMemorySeconds = 0.5;
	// Speed allowed per cm a truck still has to roll to its slot behind the stop point (cm/s per cm).
	constexpr float StopBrakeGain = 0.35f;
	// A truck short of its stop slot always rolls at least this fast (above the stuck check's 80 cm/s), so the
	// braking never stalls it before the stop starts.
	constexpr float StopCreepSpeed = 150.f;
	// Once the front truck is this close to the stop point the stop starts after MaxBrakeSeconds at the latest.
	constexpr float StopBrakeZoneDistance = 2500.f;
	constexpr double MaxBrakeSeconds = 20.0;
	// A truck of the branch cargo turns off once it is this close (along the route) to the fork.
	constexpr float ForkTriggerTolerance = 300.f;
	// Window around the last branch distance searched when projecting a branch truck onto the branch path.
	constexpr float BranchProjectionWindow = 6000.f;
	constexpr int32 MaxBranchStuckNudges = 4;
}

void FConvoyCargoTypeOutcome::AddFate(const EConvoyTruckFate Fate)
{
	switch (Fate)
	{
	case EConvoyTruckFate::Escaped:
		++EscapedTrucks;
		break;
	case EConvoyTruckFate::Destroyed:
		++DestroyedTrucks;
		break;
	case EConvoyTruckFate::Captured:
		++CapturedTrucks;
		break;
	case EConvoyTruckFate::BranchDelivered:
		++BranchDeliveredTrucks;
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Plan and queries
// ---------------------------------------------------------------------------------------------------------------------

void AConvoy::SetBehaviourPlan(const FConvoyBehaviourPlan& Plan)
{
	M_BehaviourPlan = Plan;
	M_BehaviourRuntime = FConvoyBehaviourRuntime();
	M_BranchPolyline.Build(Plan.Branch.BranchPoints);
	if (M_BehaviourPlan.Branch.bEnabled and not M_BranchPolyline.GetIsValid())
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Convoy %d got a branch without a drivable branch path; its trucks stay in the column."), M_ConvoyId));
		M_BehaviourPlan.Branch.bEnabled = false;
	}
	M_BehaviourRuntime.StopPhase = M_BehaviourPlan.Stop.bEnabled ? EConvoyStopPhase::Approaching : EConvoyStopPhase::None;
	M_BehaviourRuntime.bInsideZone.Init(false, M_BehaviourPlan.Ambush.Zones.Num());
	UE_LOG(LogConvoy, Log, TEXT("CONVOY %d behaviours: stop=%d (at %.0f, %.0f s) branch=%d (%s at %.0f, %d points) "
		       "ambush_zones=%d radio=%d hunters=%d (at %.0f)"), M_ConvoyId,
	       M_BehaviourPlan.Stop.bEnabled ? 1 : 0, M_BehaviourPlan.Stop.StopDistance, M_BehaviourPlan.Stop.StopSeconds,
	       M_BehaviourPlan.Branch.bEnabled ? 1 : 0, *ConvoyNames::GetCargoName(M_BehaviourPlan.Branch.CargoType),
	       M_BehaviourPlan.Branch.ForkDistance, M_BranchPolyline.GetPoints().Num(), M_BehaviourPlan.Ambush.Zones.Num(),
	       GetIsRadioPostAlive() ? 1 : 0, M_BehaviourPlan.Hunters.bEnabled ? 1 : 0,
	       M_BehaviourPlan.Hunters.DismountDistance);
}

bool AConvoy::GetIsFloorItActive() const
{
	return M_BehaviourRuntime.FloorItUntilTime > GetNow();
}

float AConvoy::GetTruckSpeedMultiplier() const
{
	float Multiplier = 1.f;
	if (GetIsFloorItActive())
	{
		Multiplier = FMath::Max(Multiplier, M_BehaviourPlan.Stop.FloorItSpeedMultiplier);
	}
	if (GetIsInsideBoostedAmbushZone())
	{
		Multiplier = FMath::Max(Multiplier, M_BehaviourPlan.Ambush.SpeedMultiplier);
	}
	return Multiplier;
}

int32 AConvoy::GetActiveBranchTruckCount() const
{
	int32 Count = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		Count += Member.Role == EConvoyUnitRole::Truck and Member.GetIsOnBranch() and Member.GetIsAlive() ? 1 : 0;
	}
	return Count;
}

int32 AConvoy::GetAliveColumnTruckCount() const
{
	int32 Count = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		Count += Member.GetIsColumnTruck() ? 1 : 0;
	}
	return Count;
}

int32 AConvoy::CountTrucksWithFate(const EConvoyTruckFate Fate) const
{
	int32 Count = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		Count += Member.Role == EConvoyUnitRole::Truck and Member.Fate == Fate ? 1 : 0;
	}
	return Count;
}

FConvoyCargoOutcome AConvoy::GetCargoOutcome() const
{
	FConvoyCargoOutcome Outcome;
	Outcome.ConvoyId = M_ConvoyId;
	Outcome.CargoType = IsValid(M_Definition) ? M_Definition->Cargo.CargoType : EConvoyCargoType::Fuel;
	Outcome.Outcome = M_Outcome;
	FConvoyCargoTypeOutcome Totals;
	for (const FConvoyMember& Member : M_Members)
	{
		if (Member.Role != EConvoyUnitRole::Truck or Member.Fate == EConvoyTruckFate::None)
		{
			continue;
		}
		Totals.AddFate(Member.Fate);
		const EConvoyCargoType CargoType = Member.CargoType;
		FConvoyCargoTypeOutcome* Entry = Outcome.ByCargoType.FindByPredicate(
			[CargoType](const FConvoyCargoTypeOutcome& Candidate)
			{
				return Candidate.CargoType == CargoType;
			});
		if (Entry == nullptr)
		{
			Entry = &Outcome.ByCargoType.AddDefaulted_GetRef();
			Entry->CargoType = CargoType;
		}
		Entry->AddFate(Member.Fate);
	}
	Outcome.EscapedTrucks = Totals.EscapedTrucks;
	Outcome.DestroyedTrucks = Totals.DestroyedTrucks;
	Outcome.CapturedTrucks = Totals.CapturedTrucks;
	Outcome.BranchDeliveredTrucks = Totals.BranchDeliveredTrucks;
	Outcome.TruckCount = Totals.EscapedTrucks + Totals.DestroyedTrucks + Totals.CapturedTrucks
		+ Totals.BranchDeliveredTrucks;
	return Outcome;
}

bool AConvoy::GetTruckCargoType(const AActor* Truck, EConvoyCargoType& OutCargoType) const
{
	if (Truck == nullptr)
	{
		return false;
	}
	const FConvoyMember* Member = M_Members.FindByPredicate([Truck](const FConvoyMember& Candidate)
	{
		return Candidate.Role == EConvoyUnitRole::Truck and Candidate.Actor.Get() == Truck;
	});
	if (Member == nullptr)
	{
		return false;
	}
	OutCargoType = Member->CargoType;
	return true;
}

void AConvoy::MarkTruckFate(const int32 MemberId, const EConvoyTruckFate Fate)
{
	FConvoyMember* Member = FindMember(MemberId);
	// The first fate sticks: a delivered or escaped truck that is destroyed afterwards stays delivered.
	if (Member == nullptr or Member->Role != EConvoyUnitRole::Truck or Member->Fate != EConvoyTruckFate::None)
	{
		return;
	}
	Member->Fate = Fate;
}

void AConvoy::MarkAliveTrucksEscaped()
{
	for (const FConvoyMember& Member : M_Members)
	{
		if (Member.Role == EConvoyUnitRole::Truck and Member.GetIsAlive())
		{
			MarkTruckFate(Member.MemberId, EConvoyTruckFate::Escaped);
		}
	}
}

EConvoyOutcome AConvoy::GetFinalOutcomeWithoutTrucks() const
{
	if (CountTrucksWithFate(EConvoyTruckFate::Escaped) > 0)
	{
		return EConvoyOutcome::Escaped;
	}
	// Every truck that spawned took the branch and delivered: the convoy got through, nothing was stopped.
	const bool bNothingStopped = CountTrucksWithFate(EConvoyTruckFate::Destroyed) == 0 and M_CapturedTruckCount == 0;
	if (bNothingStopped and CountTrucksWithFate(EConvoyTruckFate::BranchDelivered) > 0)
	{
		return EConvoyOutcome::Escaped;
	}
	return GetOutcomeWithoutTrucks();
}

void AConvoy::BroadcastBehaviour(FConvoyBehaviourEvent& Event, const EConvoyEventType EventLogType,
                                 const FConvoyMember* Member, const FString& Detail)
{
	Event.ConvoyId = M_ConvoyId;
	Broadcast(EventLogType, Member, Detail, Event.Location);
	OnBehaviourEventNative.Broadcast(Event);
	OnBehaviourEvent.Broadcast(Event);
}

// ---------------------------------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------------------------------

void AConvoy::UpdateBehavioursAnyState(const double Now)
{
	UpdateBranchTrucks(Now);
	if (M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	const bool bStopRunning = M_BehaviourRuntime.StopPhase == EConvoyStopPhase::Approaching
		or M_BehaviourRuntime.StopPhase == EConvoyStopPhase::Halted;
	if (bStopRunning and GetAliveColumnTruckCount() == 0)
	{
		EndStop(EConvoyStopEndReason::Cancelled, Now);
	}
}

void AConvoy::UpdateBehaviours(const double Now)
{
	if (M_State == EConvoyState::Forming)
	{
		return;
	}
	UpdateStop(Now);
	if (M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	UpdateAmbushZones();
	if (M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	UpdateHunters(Now);
}

bool AConvoy::GetIsAttackedForBehaviours(const double Now) const
{
	if (M_State == EConvoyState::UnderAttack)
	{
		return true;
	}
	// Long-range fire puts members in combat with nobody in the response radius; UpdateCombat stamps that too.
	return M_LastThreatTime >= 0.0 and Now - M_LastThreatTime <= ConvoyBehaviourConstants::AttackedMemorySeconds;
}

float AConvoy::GetFrontColumnReference() const
{
	const double Now = GetNow();
	float Front = -TNumericLimits<float>::Max();
	for (const FConvoyMember& Member : M_Members)
	{
		if (GetIsTruckHoldingColumn(Member, Now))
		{
			Front = FMath::Max(Front, Member.RouteDistance - Member.SlotAlongOffset);
		}
	}
	return Front;
}

// ---------------------------------------------------------------------------------------------------------------------
// Timed stop
// ---------------------------------------------------------------------------------------------------------------------

void AConvoy::UpdateStop(const double Now)
{
	const FConvoyStopPlan& Stop = M_BehaviourPlan.Stop;
	const EConvoyStopPhase Phase = M_BehaviourRuntime.StopPhase;
	if (Phase != EConvoyStopPhase::Approaching and Phase != EConvoyStopPhase::Halted)
	{
		return;
	}
	const bool bAttacked = GetIsAttackedForBehaviours(Now);
	if (Phase == EConvoyStopPhase::Approaching)
	{
		if (bAttacked)
		{
			EndStop(EConvoyStopEndReason::SkippedAttacked, Now);
			return;
		}
		const float FrontReference = GetFrontColumnReference();
		if (FrontReference >= Stop.StopDistance - Stop.ArriveTolerance)
		{
			StartStop(Now);
			return;
		}
		UpdateStopBrakeTimeout(FrontReference, Now);
		return;
	}
	if (bAttacked)
	{
		EndStop(EConvoyStopEndReason::Attacked, Now);
		return;
	}
	if (Now - M_BehaviourRuntime.StopStartTime >= Stop.StopSeconds)
	{
		EndStop(EConvoyStopEndReason::TimedOut, Now);
	}
}

void AConvoy::UpdateStopBrakeTimeout(const float FrontReference, const double Now)
{
	using namespace ConvoyBehaviourConstants;
	if (FrontReference < M_BehaviourPlan.Stop.StopDistance - StopBrakeZoneDistance)
	{
		return;
	}
	if (M_BehaviourRuntime.StopBrakeZoneEnterTime < 0.0)
	{
		M_BehaviourRuntime.StopBrakeZoneEnterTime = Now;
		return;
	}
	// A truck that cannot crawl the last metres (slope, blocked road) must not keep the convoy short of its stop.
	if (Now - M_BehaviourRuntime.StopBrakeZoneEnterTime >= MaxBrakeSeconds)
	{
		StartStop(Now);
	}
}

void AConvoy::StartStop(const double Now)
{
	M_BehaviourRuntime.StopPhase = EConvoyStopPhase::Halted;
	M_BehaviourRuntime.StopStartTime = Now;
	FConvoyBehaviourEvent Event;
	Event.Type = EConvoyBehaviourEventType::StopStarted;
	Event.Location = M_BehaviourPlan.Stop.StopLocation;
	BroadcastBehaviour(Event, EConvoyEventType::StopStarted, nullptr,
	                   FString::Printf(TEXT("stop for %.0f s at %.0f"), M_BehaviourPlan.Stop.StopSeconds,
	                                   M_BehaviourPlan.Stop.StopDistance));
}

void AConvoy::EndStop(const EConvoyStopEndReason Reason, const double Now)
{
	const EConvoyStopPhase Phase = M_BehaviourRuntime.StopPhase;
	if (Phase != EConvoyStopPhase::Approaching and Phase != EConvoyStopPhase::Halted)
	{
		return;
	}
	M_BehaviourRuntime.StopPhase = EConvoyStopPhase::Done;
	if (Reason == EConvoyStopEndReason::Attacked or Reason == EConvoyStopEndReason::SkippedAttacked)
	{
		StartFloorIt(Now);
	}
	FConvoyBehaviourEvent Event;
	Event.Type = EConvoyBehaviourEventType::StopEnded;
	Event.StopEndReason = Reason;
	Event.Location = M_BehaviourPlan.Stop.StopLocation;
	const double StoodSeconds = Phase == EConvoyStopPhase::Halted ? Now - M_BehaviourRuntime.StopStartTime : 0.0;
	BroadcastBehaviour(Event, EConvoyEventType::StopEnded, nullptr,
	                   FString::Printf(TEXT("reason=%s stood=%.1f s floor_it=%d"),
	                                   *StaticEnum<EConvoyStopEndReason>()->GetNameStringByValue(
		                                   static_cast<int64>(Reason)), StoodSeconds, GetIsFloorItActive() ? 1 : 0));
}

void AConvoy::StartFloorIt(const double Now)
{
	M_BehaviourRuntime.FloorItUntilTime = Now + FMath::Max(0.f, M_BehaviourPlan.Stop.FloorItSeconds);
}

void AConvoy::CancelStop()
{
	EndStop(EConvoyStopEndReason::Cancelled, GetNow());
}

bool AConvoy::GetIsStopLimitingSpeed() const
{
	return M_BehaviourRuntime.StopPhase == EConvoyStopPhase::Approaching
		or M_BehaviourRuntime.StopPhase == EConvoyStopPhase::Halted;
}

float AConvoy::GetStopLimitedSpeed(const FConvoyMember& Member, const float WantedSpeed) const
{
	if (not GetIsStopLimitingSpeed())
	{
		return WantedSpeed;
	}
	// Each truck brakes towards the slot it has when the front truck stands at the stop point; far away this
	// limit is far above the pace, so it only bites on the last metres.
	const float SlotAtStop = M_BehaviourPlan.Stop.StopDistance + Member.SlotAlongOffset;
	const float DistanceToSlot = SlotAtStop - Member.RouteDistance;
	if (DistanceToSlot <= 0.f)
	{
		return 0.f;
	}
	const float Limit = FMath::Max(ConvoyBehaviourConstants::StopCreepSpeed,
	                               ConvoyBehaviourConstants::StopBrakeGain * DistanceToSlot);
	return FMath::Min(WantedSpeed, Limit);
}

bool AConvoy::GetIsDismountHaltActive(const double Now) const
{
	return M_BehaviourRuntime.HaltUntilTime > Now;
}

// ---------------------------------------------------------------------------------------------------------------------
// Ambush zones and the radio post
// ---------------------------------------------------------------------------------------------------------------------

bool AConvoy::GetIsRadioPostAlive() const
{
	// Optional reference: an unset or destroyed radio post simply gives no speed-up (no error).
	return RTSFunctionLibrary::RTSIsValid(M_BehaviourPlan.Ambush.RadioPost.Get());
}

bool AConvoy::GetIsInsideBoostedAmbushZone() const
{
	if (not M_BehaviourRuntime.bInsideZone.Contains(true))
	{
		return false;
	}
	return GetIsRadioPostAlive();
}

void AConvoy::UpdateAmbushZones()
{
	const TArray<FConvoyAmbushZonePlan>& Zones = M_BehaviourPlan.Ambush.Zones;
	if (Zones.IsEmpty() or GetAliveColumnTruckCount() == 0)
	{
		return;
	}
	if (M_BehaviourRuntime.bInsideZone.Num() != Zones.Num())
	{
		M_BehaviourRuntime.bInsideZone.Init(false, Zones.Num());
	}
	const FVector Centre = GetColumnCentre();
	for (int32 ZoneIndex = 0; ZoneIndex < Zones.Num() and ZoneIndex < M_BehaviourRuntime.bInsideZone.Num(); ++ZoneIndex)
	{
		const FConvoyAmbushZonePlan& Zone = Zones[ZoneIndex];
		const bool bInside = FVector::Dist2D(Centre, Zone.Centre) <= Zone.Radius;
		const bool bEntered = bInside and not M_BehaviourRuntime.bInsideZone[ZoneIndex];
		M_BehaviourRuntime.bInsideZone[ZoneIndex] = bInside;
		if (not bEntered)
		{
			continue;
		}
		FConvoyBehaviourEvent Event;
		Event.Type = EConvoyBehaviourEventType::AmbushZoneEntered;
		Event.ZoneIndex = ZoneIndex;
		Event.bRadioPostAlive = GetIsRadioPostAlive();
		Event.Location = Zone.Centre;
		BroadcastBehaviour(Event, EConvoyEventType::AmbushZoneEntered, nullptr,
		                   FString::Printf(TEXT("zone=%d radio=%d"), ZoneIndex, Event.bRadioPostAlive ? 1 : 0));
		if (M_Outcome != EConvoyOutcome::None)
		{
			return;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Hunters
// ---------------------------------------------------------------------------------------------------------------------

void AConvoy::UpdateHunters(const double Now)
{
	const FConvoyHunterPlan& Hunters = M_BehaviourPlan.Hunters;
	if (not Hunters.bEnabled or M_BehaviourRuntime.bHuntersDismounted)
	{
		return;
	}
	if (GetFrontColumnReference() >= Hunters.DismountDistance - Hunters.DismountRadius)
	{
		TriggerHuntersDismount(TEXT("reached the dismount point"), Now);
		return;
	}
	if (Hunters.bDismountWhenAttacked and GetIsAttackedForBehaviours(Now))
	{
		TriggerHuntersDismount(TEXT("attacked on the way"), Now);
	}
}

void AConvoy::TriggerHuntersDismount(const FString& Reason, const double Now)
{
	M_BehaviourRuntime.bHuntersDismounted = true;
	M_BehaviourRuntime.HaltUntilTime = Now + FMath::Max(0.f, M_BehaviourPlan.Hunters.DismountHaltSeconds);
	FConvoyBehaviourEvent Event;
	Event.Type = EConvoyBehaviourEventType::HuntersDismount;
	for (const FConvoyMember& Member : M_Members)
	{
		// Trucks are the first members, in column order, so the member id is the truck's column index.
		if (Member.Role == EConvoyUnitRole::Truck and Member.GetIsAlive())
		{
			Event.TruckLocations.Add(Member.Location);
			Event.TruckColumnIndices.Add(Member.MemberId);
		}
	}
	Event.Location = GetColumnCentre();
	BroadcastBehaviour(Event, EConvoyEventType::HuntersDismount, nullptr,
	                   FString::Printf(TEXT("%s trucks=%d halt=%.1f s"), *Reason, Event.TruckLocations.Num(),
	                                   M_BehaviourPlan.Hunters.DismountHaltSeconds));
}

// ---------------------------------------------------------------------------------------------------------------------
// Branch
// ---------------------------------------------------------------------------------------------------------------------

void AConvoy::UpdateBranchTrucks(const double Now)
{
	if (not M_BehaviourPlan.Branch.bEnabled)
	{
		return;
	}
	// Index loop: the member array never changes size, but listeners of the branch events run in between.
	for (int32 MemberIndex = 0; MemberIndex < M_Members.Num(); ++MemberIndex)
	{
		FConvoyMember& Member = M_Members[MemberIndex];
		if (Member.Role != EConvoyUnitRole::Truck or not Member.GetIsAlive())
		{
			continue;
		}
		if (Member.GetIsOnBranch())
		{
			DriveBranchTruck(Member, Now);
		}
		else if (GetCanTruckTakeBranch(Member))
		{
			StartBranch(Member, Now);
		}
		if (M_Outcome != EConvoyOutcome::None)
		{
			return;
		}
	}
}

bool AConvoy::GetCanTruckTakeBranch(const FConvoyMember& Member) const
{
	const bool bColumnDriving = M_State == EConvoyState::Cruise or M_State == EConvoyState::Regroup
		or M_State == EConvoyState::UnderAttack;
	const bool bKeepsStation = Member.State == EConvoyMemberState::Station
		or Member.State == EConvoyMemberState::Returning;
	if (not bColumnDriving or not bKeepsStation or M_BehaviourRuntime.ExitReachedTime >= 0.0)
	{
		return false;
	}
	return Member.CargoType == M_BehaviourPlan.Branch.CargoType
		and Member.RouteDistance >= M_BehaviourPlan.Branch.ForkDistance - ConvoyBehaviourConstants::ForkTriggerTolerance;
}

void AConvoy::StartBranch(FConvoyMember& Member, const double Now)
{
	Member.State = EConvoyMemberState::Branching;
	Member.Branch = FConvoyMemberBranchState();
	Member.Branch.LeftColumnTime = Now;
	float LateralOffset = 0.f;
	Member.Branch.BranchDistance = M_BranchPolyline.ProjectLocation(Member.Location, -1.f, 0.f, LateralOffset);
	// The column's stuck bookkeeping no longer applies; the branch keeps its own.
	Member.bIsStuck = false;
	Member.StuckSince = -1.0;
	Member.StuckAttempts = 0;
	Member.StuckWindowStart = -1.0;
	Member.bHasOrder = false;
	RemoveConvoyIconIfAttachedTo(Member.Actor.Get());
	AssignSlots();
	Broadcast(EConvoyEventType::SlotReassigned, nullptr,
	          FString::Printf(TEXT("column_trucks=%d"), GetAliveColumnTruckCount()));
	FConvoyBehaviourEvent Event;
	Event.Type = EConvoyBehaviourEventType::BranchTruckLeftColumn;
	Event.Truck = Member.Actor.Get();
	Event.CargoType = Member.CargoType;
	Event.Location = Member.Location;
	BroadcastBehaviour(Event, EConvoyEventType::BranchTruckLeft, &Member,
	                   FString::Printf(TEXT("%s cargo=%s branch_length=%.0f"), *Member.UnitName,
	                                   *ConvoyNames::GetCargoName(Member.CargoType), M_BranchPolyline.GetLength()));
	if (M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	DriveBranchTruck(Member, Now);
}

void AConvoy::DriveBranchTruck(FConvoyMember& Member, const double Now)
{
	if (not Member.GetIsOnBranch() or not Member.GetIsAlive())
	{
		return;
	}
	const FConvoyBranchPlan& Branch = M_BehaviourPlan.Branch;
	float LateralOffset = 0.f;
	Member.Branch.BranchDistance = M_BranchPolyline.ProjectLocation(
		Member.Location, Member.Branch.BranchDistance, ConvoyBehaviourConstants::BranchProjectionWindow, LateralOffset);
	const float BranchLength = M_BranchPolyline.GetLength();
	const FVector BranchEnd = M_BranchPolyline.GetPointAtDistance(BranchLength);
	if (FVector::Dist2D(Member.Location, BranchEnd) <= Branch.ArrivalRadius)
	{
		OnBranchTruckArrived(Member);
		return;
	}
	// Branch trucks keep the convoy pace (times the branch multiplier); a column halt does not stop them.
	Member.WantedSpeed = Member.CalibratedSpeed * M_Definition->Speed.TruckSpeedFraction * Branch.SpeedMultiplier;
	ApplyWantedSpeed(Member);
	if (GetIsMemberHeldInPlace(Member))
	{
		// The scuttle countdown holds the truck; no orders and no stuck re-paths until it drives again.
		Member.Branch.StuckWindowStart = -1.0;
		Member.Branch.bHasOrder = false;
		return;
	}
	if (UpdateBranchTruckStuck(Member, Now))
	{
		return;
	}
	const float TargetDistance = FMath::Min(Member.Branch.BranchDistance + M_Definition->Speed.LookAheadDistance,
	                                        BranchLength);
	FVector Target = M_BranchPolyline.GetPointAtDistance(TargetDistance);
	UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Target, Target);
	ICommands* Commands = Member.GetCommands();
	if (Commands == nullptr)
	{
		return;
	}
	const bool bLostOrder = Commands->GetActiveCommandID() != EAbilityID::IdMove;
	const bool bTargetMoved = FVector::Dist2D(Target, Member.Branch.LastOrderTarget) > M_Definition->Speed.ReissueDistance;
	if (Member.Branch.bHasOrder and not bLostOrder and not bTargetMoved)
	{
		return;
	}
	IssueBranchMove(Member, Target, Now);
}

bool AConvoy::UpdateBranchTruckStuck(FConvoyMember& Member, const double Now)
{
	const FConvoyRobustnessSettings& Robustness = M_Definition->Robustness;
	FConvoyMemberBranchState& BranchState = Member.Branch;
	if (BranchState.StuckWindowStart < 0.0)
	{
		BranchState.StuckWindowStart = Now;
		BranchState.StuckWindowLocation = Member.Location;
		return false;
	}
	if (Now - BranchState.StuckWindowStart < Robustness.StuckSeconds)
	{
		return false;
	}
	const float Progress = FVector::Dist2D(Member.Location, BranchState.StuckWindowLocation);
	BranchState.StuckWindowStart = Now;
	BranchState.StuckWindowLocation = Member.Location;
	if (Progress >= Robustness.StuckProgressCm)
	{
		BranchState.StuckAttempts = 0;
		return false;
	}
	// Re-path to a point nudged sideways, alternating sides and growing; never a teleport.
	BranchState.StuckAttempts = FMath::Min(BranchState.StuckAttempts + 1, ConvoyBehaviourConstants::MaxBranchStuckNudges);
	const float Side = BranchState.StuckAttempts % 2 == 0 ? 1.f : -1.f;
	const float Along = FMath::Min(BranchState.BranchDistance + M_Definition->Speed.LookAheadDistance * 0.5f,
	                               M_BranchPolyline.GetLength());
	FVector Target = M_BranchPolyline.GetOffsetPoint(Along, Side * Robustness.StuckNudgeDistance * BranchState.StuckAttempts);
	UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Target, Target);
	Broadcast(EConvoyEventType::Repath, &Member, FString::Printf(TEXT("branch attempt=%d"), BranchState.StuckAttempts),
	          Target);
	IssueBranchMove(Member, Target, Now);
	return true;
}

void AConvoy::IssueBranchMove(FConvoyMember& Member, const FVector& Target, const double Now)
{
	ICommands* Commands = Member.GetCommands();
	if (Commands == nullptr)
	{
		return;
	}
	const FRotator Facing = M_BranchPolyline.GetDirectionAtDistance(Member.Branch.BranchDistance).Rotation();
	const ECommandQueueError Error = Commands->MoveToLocation(Target, true, Facing);
	Member.Branch.bHasOrder = Error == ECommandQueueError::NoError;
	Member.Branch.LastOrderTarget = Target;
	Member.LastOrderTime = Now;
	if (not Member.Branch.bHasOrder)
	{
		UE_LOG(LogConvoy, Verbose, TEXT("CONVOY %d branch truck %d move refused (%d)"), M_ConvoyId, Member.MemberId,
		       static_cast<int32>(Error));
	}
}

void AConvoy::OnBranchTruckArrived(FConvoyMember& Member)
{
	// Delivered before the event, so a listener that ends the convoy or destroys the truck cannot turn it into
	// an escaped or destroyed truck.
	MarkTruckFate(Member.MemberId, EConvoyTruckFate::BranchDelivered);
	AActor* Truck = Member.Actor.Get();
	FConvoyBehaviourEvent Event;
	Event.Type = EConvoyBehaviourEventType::BranchTruckArrived;
	Event.Truck = Truck;
	Event.CargoType = Member.CargoType;
	Event.Location = Member.Location;
	BroadcastBehaviour(Event, EConvoyEventType::BranchTruckArrived, &Member,
	                   FString::Printf(TEXT("%s cargo=%s delivered=%d"), *Member.UnitName,
	                                   *ConvoyNames::GetCargoName(Member.CargoType), GetBranchDeliveredTruckCount()));
	if (M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	RemoveConvoyIconIfAttachedTo(Truck);
	ReleaseMember(Member, M_BehaviourPlan.Branch.bDespawnOnArrival);
	if (GetAliveTruckCount() > 0 or M_State == EConvoyState::TrucksLost)
	{
		return;
	}
	if (M_LastThreatTime < 0.0)
	{
		// Escorts left without trucks guard the last threat; without a fight that is where the column stood.
		M_LastThreatLocation = GetColumnCentre();
	}
	// The column was already gone (destroyed, captured or all branched) and this was the last truck.
	OnLastTruckGone(TEXT("last branch truck delivered, no truck left in the column"));
}
