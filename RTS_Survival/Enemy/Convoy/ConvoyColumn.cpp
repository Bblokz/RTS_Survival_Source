// Copyright (C) Bas Blokzijl - All rights reserved.

// Members of AConvoy that move the column: the column reference, speeds, move orders, stuck units and the exit.

#include "Convoy.h"

#include "AIController.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

namespace ConvoyColumnConstants
{
	constexpr double SpawningTimeoutSeconds = 45.0;
	// Window around the last known route distance searched when projecting a unit onto the route.
	constexpr float ProjectionSearchWindow = 6000.f;
	constexpr float MeasuredSpeedSmoothing = 0.35f;
	constexpr float MinSampleSeconds = 0.05f;
	constexpr float SpeedFractionDeadband = 0.01f;
	constexpr float MarchSpeedDeadband = 8.f;
	// Squads never march slower than this; below it they mark time in place.
	constexpr float MinSquadMarchSpeed = 60.f;
	constexpr float ReturnedSlotError = 1000.f;
	constexpr float RegroupEndFraction = 0.5f;
	constexpr float MinWantedSpeedToCheckStuck = 80.f;
	constexpr int32 MaxStuckNudges = 4;
}

void AConvoy::TickConvoy()
{
	using namespace ConvoyColumnConstants;
	if (M_Outcome != EConvoyOutcome::None or not GetIsValidDefinition())
	{
		return;
	}
	const double Now = GetNow();
	UpdateWaitingSquads(Now);
	UpdateMemberSamples(Now);
	UpdateDeaths();
	if (M_Outcome != EConvoyOutcome::None)
	{
		return;
	}
	if (M_State == EConvoyState::Spawning)
	{
		if (Now - M_StateStartTime > SpawningTimeoutSeconds)
		{
			for (FConvoyMember& Member : M_Members)
			{
				Member.State = Member.State == EConvoyMemberState::Spawning ? EConvoyMemberState::Dead : Member.State;
			}
			M_PendingSpawns = 0;
			OnSpawningComplete();
		}
		return;
	}
	if (M_State == EConvoyState::TrucksLost)
	{
		UpdateTrucksLost(Now);
		return;
	}
	if (M_State == EConvoyState::Forming and Now - M_StateStartTime >= M_Definition->Speed.FormingSeconds)
	{
		SetState(EConvoyState::Cruise, TEXT("formed"));
	}
	UpdateCombat(Now);
	UpdateColumnReference();
	UpdateTruckSpeeds();
	UpdateEscortSpeeds();
	UpdateOrders(Now);
	UpdateStuck(Now);
	UpdateMinimapIcon();
	CheckForExit();
}

void AConvoy::UpdateMemberSamples(const double Now)
{
	using namespace ConvoyColumnConstants;
	for (FConvoyMember& Member : M_Members)
	{
		if (not Member.GetIsAlive())
		{
			continue;
		}
		Member.Location = Member.Squad.IsValid() ? Member.Squad->GetSquadLocation() : Member.Actor->GetActorLocation();
		const double Elapsed = Now - Member.PreviousSampleTime;
		if (Member.PreviousSampleTime >= 0.0 and Elapsed > MinSampleSeconds)
		{
			const float Speed = FVector::Dist2D(Member.Location, Member.PreviousLocation) / Elapsed;
			Member.MeasuredSpeed = FMath::Lerp(Member.MeasuredSpeed, Speed, MeasuredSpeedSmoothing);
		}
		Member.PreviousLocation = Member.Location;
		Member.PreviousSampleTime = Now;
		Member.RouteDistance = M_Polyline.ProjectLocation(Member.Location, Member.RouteDistance,
		                                                  ProjectionSearchWindow, Member.LateralOffset);
		UpdatePathState(Member);
	}
}

bool AConvoy::GetIsTruckHoldingColumn(const FConvoyMember& Member, const double Now) const
{
	if (Member.Role != EConvoyUnitRole::Truck or not Member.GetIsAlive())
	{
		return false;
	}
	// A truck stuck for too long is left behind as a target instead of stopping the column forever.
	const bool bStuckTooLong = Member.bIsStuck and Member.StuckSince >= 0.0
		and Now - Member.StuckSince > M_Definition->Robustness.MaxWaitForStuckTruckSeconds;
	return not bStuckTooLong;
}

void AConvoy::UpdateColumnReference()
{
	const double Now = GetNow();
	float Reference = TNumericLimits<float>::Max();
	float MostAhead = -TNumericLimits<float>::Max();
	float SpeedSum = 0.f;
	int32 HoldingCount = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		if (not GetIsTruckHoldingColumn(Member, Now))
		{
			continue;
		}
		// Where this truck says the column is: its own distance minus its slot offset.
		const float Implied = Member.RouteDistance - Member.SlotAlongOffset;
		Reference = FMath::Min(Reference, Implied);
		MostAhead = FMath::Max(MostAhead, Implied);
		SpeedSum += Member.MeasuredSpeed;
		++HoldingCount;
	}
	if (HoldingCount == 0)
	{
		return;
	}
	if (M_State != EConvoyState::Forming)
	{
		M_ColumnReference = Reference;
	}
	M_MeasuredTruckSpeed = SpeedSum / HoldingCount;
	const float Stretch = MostAhead - Reference;
	const float RegroupLag = M_Definition->Speed.RegroupLagDistance;
	if (M_State == EConvoyState::Cruise and Stretch > RegroupLag)
	{
		SetState(EConvoyState::Regroup, FString::Printf(TEXT("stretch=%.0f"), Stretch));
		Broadcast(EConvoyEventType::RegroupStart, nullptr, FString::Printf(TEXT("stretch=%.0f"), Stretch));
	}
	else if (M_State == EConvoyState::Regroup and Stretch < RegroupLag * ConvoyColumnConstants::RegroupEndFraction)
	{
		Broadcast(EConvoyEventType::RegroupEnd, nullptr, FString::Printf(TEXT("stretch=%.0f"), Stretch));
		SetState(EConvoyState::Cruise, TEXT("closed up"));
	}
}

void AConvoy::UpdateTruckSpeeds()
{
	const FConvoySpeedSettings& Speed = M_Definition->Speed;
	const bool bHalt = M_State == EConvoyState::Forming
		or (M_State == EConvoyState::UnderAttack
			and M_Definition->Combat.TruckBehaviour == EConvoyTruckCombatBehaviour::HaltBehindEscorts);
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.Role != EConvoyUnitRole::Truck or not Member.GetIsAlive())
		{
			continue;
		}
		const float Cap = Member.CalibratedSpeed * Speed.TruckSpeedFraction;
		const float Max = Member.CalibratedSpeed * (Speed.TruckSpeedFraction + Speed.TruckCatchUpFraction);
		// Zero for the truck that sets the column reference, negative for trucks ahead of their slot.
		const float SlotError = M_ColumnReference + Member.SlotAlongOffset - Member.RouteDistance;
		Member.WantedSpeed = bHalt ? 0.f : FMath::Clamp(Cap + Speed.SlotSpeedGain * SlotError, 0.f, Max);
		ApplyWantedSpeed(Member);
	}
}

void AConvoy::UpdateEscortSpeeds()
{
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.Role == EConvoyUnitRole::Truck or not Member.GetIsAlive())
		{
			continue;
		}
		Member.WantedSpeed = GetEscortWantedSpeed(Member);
		ApplyWantedSpeed(Member);
	}
}

float AConvoy::GetEscortWantedSpeed(const FConvoyMember& Member) const
{
	const FConvoySpeedSettings& Speed = M_Definition->Speed;
	const bool bIsSquad = Member.Role == EConvoyUnitRole::EscortSquad;
	const float OwnMax = bIsSquad ? Speed.MaxSquadMarchSpeedCms : Member.NominalSpeed;
	if (Member.State == EConvoyMemberState::Engaging)
	{
		// Fighting escorts are free: no convoy limit while the fight lasts.
		return OwnMax;
	}
	// The trucks' real speed is the base, so a slow or climbing truck slows the escorts as well.
	const float Base = M_State == EConvoyState::Forming ? 0.f : M_MeasuredTruckSpeed;
	const float CatchUpLimit = FMath::Max(Base, M_PaceSpeed) * Speed.EscortCatchUpSpeedFactor;
	const float SlotError = M_ColumnReference + Member.SlotAlongOffset - Member.RouteDistance;
	const float Wanted = Base + Speed.SlotSpeedGain * SlotError;
	return FMath::Clamp(Wanted, 0.f, FMath::Min(OwnMax, CatchUpLimit));
}

void AConvoy::ApplyWantedSpeed(FConvoyMember& Member)
{
	using namespace ConvoyColumnConstants;
	if (Member.Tank.IsValid())
	{
		UpdateSpeedTrim(Member);
		const float Fraction = Member.NominalSpeed > KINDA_SMALL_NUMBER
			                       ? FMath::Clamp(Member.WantedSpeed * Member.SpeedTrim / Member.NominalSpeed, 0.f, 1.f)
			                       : 1.f;
		if (FMath::Abs(Fraction - Member.AppliedSpeedFraction) < SpeedFractionDeadband)
		{
			return;
		}
		Member.AppliedSpeedFraction = Fraction;
		Member.Tank->SetMobilityRestriction(this, Fraction, 1.f, 1.f);
		return;
	}
	if (not Member.Squad.IsValid())
	{
		return;
	}
	const float MarchSpeed = FMath::Clamp(Member.WantedSpeed, MinSquadMarchSpeed,
	                                      M_Definition->Speed.MaxSquadMarchSpeedCms);
	if (FMath::Abs(MarchSpeed - Member.AppliedMarchSpeed) < MarchSpeedDeadband)
	{
		return;
	}
	Member.AppliedMarchSpeed = MarchSpeed;
	Member.Squad->SetMarchSpeedOverride(MarchSpeed);
}

void AConvoy::UpdateSpeedTrim(FConvoyMember& Member) const
{
	// Learn only while the unit cruises: a stuck, accelerating or fighting unit says nothing about the drive train.
	constexpr float MinWantedForTrim = 150.f;
	constexpr float MinMeasuredFraction = 0.5f;
	constexpr float MinTrim = 0.7f;
	constexpr float MaxTrim = 2.f;
	constexpr float TickSeconds = 0.25f;
	const bool bCruising = (M_State == EConvoyState::Cruise or M_State == EConvoyState::Regroup)
		and Member.State == EConvoyMemberState::Station and not Member.bIsStuck
		and Member.WantedSpeed > MinWantedForTrim and Member.MeasuredSpeed > Member.WantedSpeed * MinMeasuredFraction;
	if (not bCruising)
	{
		return;
	}
	const float RelativeError = (Member.WantedSpeed - Member.MeasuredSpeed) / Member.WantedSpeed;
	Member.SpeedTrim = FMath::Clamp(
		Member.SpeedTrim + RelativeError * M_Definition->Speed.SpeedTrimGainPerSecond * TickSeconds, MinTrim, MaxTrim);
}

FVector AConvoy::GetSlotLocation(const FConvoyMember& Member) const
{
	return M_Polyline.GetOffsetPoint(M_ColumnReference + Member.SlotAlongOffset, Member.SlotLateralOffset);
}

FVector AConvoy::GetOrderTarget(const FConvoyMember& Member) const
{
	const float Distance = FMath::Min(M_ColumnReference + Member.SlotAlongOffset + M_Definition->Speed.LookAheadDistance,
	                                  M_Polyline.GetLength());
	return M_Polyline.GetOffsetPoint(Distance, Member.SlotLateralOffset);
}

void AConvoy::UpdateOrders(const double Now)
{
	if (M_State == EConvoyState::Forming)
	{
		return;
	}
	for (FConvoyMember& Member : M_Members)
	{
		if (not Member.GetIsAlive())
		{
			continue;
		}
		if (Member.State == EConvoyMemberState::Station or Member.State == EConvoyMemberState::Returning)
		{
			UpdateStationOrder(Member, Now);
		}
	}
}

void AConvoy::UpdateStationOrder(FConvoyMember& Member, const double Now)
{
	ICommands* Commands = Member.GetCommands();
	if (Commands == nullptr)
	{
		return;
	}
	if (Member.State == EConvoyMemberState::Returning
		and FVector::Dist2D(Member.Location, GetSlotLocation(Member)) < ConvoyColumnConstants::ReturnedSlotError)
	{
		Member.State = EConvoyMemberState::Station;
		Broadcast(EConvoyEventType::EscortReturn, &Member, TEXT("back on station"));
	}
	const URTSComponent* RTSComponent = Member.Actor->FindComponentByClass<URTSComponent>();
	const bool bSquadFighting = Member.Squad.IsValid() and IsValid(RTSComponent) and RTSComponent->GetIsUnitInCombat()
		and Commands->GetActiveCommandID() != EAbilityID::IdMarch;
	if (bSquadFighting and Member.State == EConvoyMemberState::Station)
	{
		// The march dropped itself for the fight (existing squad behaviour); it is picked up once the fight is over.
		return;
	}
	FVector Target = GetOrderTarget(Member);
	UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Target, Target);
	const EAbilityID ActiveCommand = Commands->GetActiveCommandID();
	const bool bIsMarching = ActiveCommand == EAbilityID::IdMarch;
	const bool bIsMoving = ActiveCommand == EAbilityID::IdMove;
	const bool bLostOrder = Member.Squad.IsValid() ? not bIsMarching : not bIsMoving;
	const bool bTargetMoved = FVector::Dist2D(Target, Member.LastOrderTarget) > M_Definition->Speed.ReissueDistance;
	if (Member.bHasOrder and not bLostOrder and not bTargetMoved)
	{
		return;
	}
	if (Member.Squad.IsValid() and Member.State == EConvoyMemberState::Station)
	{
		IssueSquadMarch(Member, Target, Now);
		return;
	}
	IssueMove(Member, Target, false, Now);
}

void AConvoy::IssueMove(FConvoyMember& Member, const FVector& Target, const bool bAttackMove, const double Now)
{
	ICommands* Commands = Member.GetCommands();
	if (Commands == nullptr)
	{
		return;
	}
	const FRotator Facing = M_Polyline.GetDirectionAtDistance(Member.RouteDistance).Rotation();
	const ECommandQueueError Error = bAttackMove
		                                 ? Commands->AttackMoveToLocation(Target, true, Facing)
		                                 : Commands->MoveToLocation(Target, true, Facing);
	Member.bHasOrder = Error == ECommandQueueError::NoError;
	Member.LastOrderTarget = Target;
	Member.LastOrderTime = Now;
	if (not Member.bHasOrder)
	{
		UE_LOG(LogConvoy, Verbose, TEXT("CONVOY %d member %d move refused (%d)"), M_ConvoyId, Member.MemberId,
		       static_cast<int32>(Error));
	}
}

void AConvoy::IssueSquadMarch(FConvoyMember& Member, const FVector& Target, const double Now)
{
	ICommands* Commands = Member.GetCommands();
	if (Commands == nullptr or not Member.Squad.IsValid())
	{
		return;
	}
	// A running march only moves its far end (no stop, no turn back); otherwise a fresh march order.
	bool bOrdered = Member.Squad->RedirectMarch(Target);
	if (not bOrdered)
	{
		bOrdered = Commands->MarchToLocation(Target, true) == ECommandQueueError::NoError;
		// A fresh march applies the march speed from scratch; make the convoy pace stick again.
		Member.AppliedMarchSpeed = 0.f;
		ApplyWantedSpeed(Member);
	}
	Member.bHasOrder = bOrdered;
	Member.LastOrderTarget = Target;
	Member.LastOrderTime = Now;
}

void AConvoy::UpdatePathState(FConvoyMember& Member) const
{
	const APawn* Pawn = Member.Tank.IsValid() ? Cast<APawn>(Member.Tank.Get()) : nullptr;
	if (Member.Squad.IsValid())
	{
		const TArray<ASquadUnit*> Soldiers = Member.Squad->GetSquadUnitsChecked();
		Pawn = Soldiers.IsEmpty() ? nullptr : Soldiers[0];
	}
	const AAIController* Controller = Pawn != nullptr ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	const UPathFollowingComponent* PathFollowing = Controller != nullptr ? Controller->GetPathFollowingComponent() : nullptr;
	Member.bHasPartialPath = PathFollowing != nullptr and PathFollowing->GetPath().IsValid()
		and PathFollowing->GetPath()->IsPartial();
}

void AConvoy::UpdateStuck(const double Now)
{
	using namespace ConvoyColumnConstants;
	const FConvoyRobustnessSettings& Robustness = M_Definition->Robustness;
	for (FConvoyMember& Member : M_Members)
	{
		const bool bShouldMove = Member.GetIsAlive() and Member.WantedSpeed > MinWantedSpeedToCheckStuck
			and (Member.State == EConvoyMemberState::Station or Member.State == EConvoyMemberState::Returning)
			and M_State != EConvoyState::UnderAttack;
		if (not bShouldMove)
		{
			Member.StuckWindowStart = -1.0;
			continue;
		}
		if (Member.StuckWindowStart < 0.0)
		{
			Member.StuckWindowStart = Now;
			Member.StuckWindowLocation = Member.Location;
			continue;
		}
		const float Progress = FVector::Dist2D(Member.Location, Member.StuckWindowLocation);
		if (Member.bIsStuck and Progress >= Robustness.StuckProgressCm)
		{
			Member.bIsStuck = false;
			Member.StuckAttempts = 0;
			Member.StuckSince = -1.0;
			Broadcast(EConvoyEventType::Unstuck, &Member, FString::Printf(TEXT("progress=%.0f"), Progress));
		}
		if (Now - Member.StuckWindowStart < Robustness.StuckSeconds)
		{
			continue;
		}
		if (Progress < Robustness.StuckProgressCm)
		{
			HandleStuckMember(Member, Now);
		}
		Member.StuckWindowStart = Now;
		Member.StuckWindowLocation = Member.Location;
	}
}

void AConvoy::HandleStuckMember(FConvoyMember& Member, const double Now)
{
	const FConvoyRobustnessSettings& Robustness = M_Definition->Robustness;
	if (not Member.bIsStuck)
	{
		Member.bIsStuck = true;
		Member.StuckSince = Now;
		Broadcast(EConvoyEventType::Stuck, &Member, FString::Printf(TEXT("partial=%d"), Member.bHasPartialPath ? 1 : 0));
	}
	// Re-path to a point nudged sideways, alternating sides and growing; never a teleport.
	Member.StuckAttempts = FMath::Min(Member.StuckAttempts + 1, ConvoyColumnConstants::MaxStuckNudges);
	const float Side = Member.StuckAttempts % 2 == 0 ? 1.f : -1.f;
	const float Along = FMath::Min(Member.RouteDistance + M_Definition->Speed.LookAheadDistance * 0.5f,
	                               M_Polyline.GetLength());
	FVector Target = M_Polyline.GetOffsetPoint(
		Along, Member.SlotLateralOffset + Side * Robustness.StuckNudgeDistance * Member.StuckAttempts);
	UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), Target, Target);
	Broadcast(EConvoyEventType::Repath, &Member, FString::Printf(TEXT("attempt=%d"), Member.StuckAttempts), Target);
	IssueMove(Member, Target, false, Now);
}

void AConvoy::UpdateMinimapIcon()
{
	if (not M_Params.bShowOnMinimap)
	{
		return;
	}
	// The mark follows the front truck; it moves to the next one when that truck dies.
	AActor* Front = nullptr;
	float FrontDistance = -TNumericLimits<float>::Max();
	for (const FConvoyMember& Member : M_Members)
	{
		if (Member.Role == EConvoyUnitRole::Truck and Member.GetIsAlive() and Member.RouteDistance > FrontDistance)
		{
			FrontDistance = Member.RouteDistance;
			Front = Member.Actor.Get();
		}
	}
	if (Front == nullptr or Front == M_IconActor.Get())
	{
		return;
	}
	if (M_ConvoyIconId != NAME_None)
	{
		URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, M_ConvoyIconId);
	}
	FMinimapIconTextPayload Text;
	Text.bAlsoWriteText = true;
	Text.Text = M_Definition->DisplayName;
	M_ConvoyIconId = URTSBlueprintFunctionLibrary::BP_AddCustomMiniMapIconAttachedToActor(
		this, FName(*FString::Printf(TEXT("Convoy_%d"), M_ConvoyId)), M_Definition->MinimapIcon,
		Front->GetActorLocation(), Front, FRotator::ZeroRotator, false, Text);
	M_IconActor = Front;
}

void AConvoy::CheckForExit()
{
	const FVector RouteEnd = M_Polyline.GetPointAtDistance(M_Polyline.GetLength());
	const double Now = GetNow();
	int32 HoldingTrucks = 0;
	for (const FConvoyMember& Member : M_Members)
	{
		if (not GetIsTruckHoldingColumn(Member, Now))
		{
			continue;
		}
		++HoldingTrucks;
		const bool bAtEnd = FVector::Dist2D(Member.Location, RouteEnd) <= M_Definition->Speed.ExitRadius
			or Member.RouteDistance >= M_Polyline.GetLength() - M_Definition->Speed.ExitRadius;
		if (not bAtEnd)
		{
			return;
		}
	}
	if (HoldingTrucks == 0)
	{
		return;
	}
	Broadcast(EConvoyEventType::ReachedExit, nullptr, FString::Printf(TEXT("trucks=%d"), HoldingTrucks), RouteEnd);
	FinishConvoy(EConvoyOutcome::Escaped, true);
}
