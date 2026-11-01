// Copyright (C) Bas Blokzijl - All rights reserved.

// Members of AConvoy that react to attack: threat detection, escorts engaging on a leash, re-forming, and what the
// escorts do once every truck is gone.

#include "Convoy.h"

#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace ConvoyCombatConstants
{
	constexpr uint8 PlayerOne = 1;
	constexpr uint8 PlayerTwo = 2;
	constexpr double EngageOrderRefreshSeconds = 3.0;
	constexpr double WithdrawTimeoutSeconds = 240.0;
}

uint8 AConvoy::GetHostilePlayer() const
{
	using namespace ConvoyCombatConstants;
	return M_Params.OwningPlayer == PlayerOne ? PlayerTwo : PlayerOne;
}

bool AConvoy::FindNearestThreat(FVector& OutThreatLocation) const
{
	const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(UnitManager))
	{
		return false;
	}
	const float RadiusSquared = FMath::Square(M_Definition->Combat.ResponseRadius);
	float BestDistanceSquared = TNumericLimits<float>::Max();
	auto ConsiderHostile = [&](const AActor* Hostile)
	{
		if (not IsValid(Hostile))
		{
			return;
		}
		const FVector HostileLocation = Hostile->GetActorLocation();
		for (const FConvoyMember& Member : M_Members)
		{
			if (not Member.GetIsAlive())
			{
				continue;
			}
			const float DistanceSquared = FVector::DistSquared2D(HostileLocation, Member.Location);
			if (DistanceSquared <= RadiusSquared and DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				OutThreatLocation = HostileLocation;
			}
		}
	};
	const uint8 Hostile = GetHostilePlayer();
	for (const ATankMaster* Tank : UnitManager->GetPlayerTanks(Hostile))
	{
		ConsiderHostile(Tank);
	}
	for (const ASquadUnit* Soldier : UnitManager->GetSquadUnitsOfPlayer(Hostile))
	{
		ConsiderHostile(Soldier);
	}
	return BestDistanceSquared < TNumericLimits<float>::Max();
}

bool AConvoy::GetIsAnyMemberInCombat() const
{
	for (const FConvoyMember& Member : M_Members)
	{
		if (not Member.GetIsAlive())
		{
			continue;
		}
		const URTSComponent* RTSComponent = Member.Actor->FindComponentByClass<URTSComponent>();
		if (IsValid(RTSComponent) and RTSComponent->GetIsUnitInCombat())
		{
			return true;
		}
	}
	return false;
}

void AConvoy::UpdateCombat(const double Now)
{
	if (M_State == EConvoyState::Forming)
	{
		return;
	}
	FVector ThreatLocation = FVector::ZeroVector;
	const bool bThreatInRange = FindNearestThreat(ThreatLocation);
	// Long-range fire (artillery) puts units in combat with nobody in range; that still counts as an attack.
	const bool bUnderFire = GetIsAnyMemberInCombat();
	if (bThreatInRange)
	{
		M_LastThreatLocation = ThreatLocation;
	}
	if (bThreatInRange or bUnderFire)
	{
		M_LastThreatTime = Now;
	}
	if (M_State != EConvoyState::UnderAttack)
	{
		if (bThreatInRange)
		{
			SetState(EConvoyState::UnderAttack, TEXT("hostile in range"));
			Broadcast(EConvoyEventType::UnderAttack, nullptr, TEXT("hostile in response radius"), ThreatLocation);
			EngageEscorts(Now);
		}
		return;
	}
	if (bThreatInRange)
	{
		EngageEscorts(Now);
	}
	UpdateEngagedEscorts(Now);
	if (Now - M_LastThreatTime >= M_Definition->Combat.CalmSecondsToResume)
	{
		EndCombat(Now);
	}
}

void AConvoy::EngageEscorts(const double Now)
{
	using namespace ConvoyCombatConstants;
	const float RadiusSquared = FMath::Square(M_Definition->Combat.ResponseRadius);
	for (FConvoyMember& Member : M_Members)
	{
		// Squads keep marching until the existing combat drop takes them out of the march; vehicles go for it.
		if (Member.Role != EConvoyUnitRole::EscortVehicle or not Member.GetIsAlive())
		{
			continue;
		}
		if (FVector::DistSquared2D(Member.Location, M_LastThreatLocation) > RadiusSquared)
		{
			continue;
		}
		const bool bNeedsOrder = Member.State != EConvoyMemberState::Engaging
			or Now - Member.LastEngageOrderTime >= EngageOrderRefreshSeconds;
		if (Member.State == EConvoyMemberState::Returning or not bNeedsOrder)
		{
			continue;
		}
		if (Member.State != EConvoyMemberState::Engaging)
		{
			Member.State = EConvoyMemberState::Engaging;
			Broadcast(EConvoyEventType::EscortEngage, &Member, TEXT("attack move to threat"), M_LastThreatLocation);
		}
		Member.WantedSpeed = Member.NominalSpeed;
		ApplyWantedSpeed(Member);
		IssueMove(Member, M_LastThreatLocation, true, Now);
		Member.LastEngageOrderTime = Now;
	}
}

void AConvoy::UpdateEngagedEscorts(const double Now)
{
	const FVector Centre = GetColumnCentre();
	const float Leash = M_Definition->Combat.LeashDistance;
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.State != EConvoyMemberState::Engaging or not Member.GetIsAlive())
		{
			continue;
		}
		if (FVector::Dist2D(Member.Location, Centre) <= Leash)
		{
			continue;
		}
		// The leash: never drawn further from the trucks than this, whatever the attacker does.
		Member.State = EConvoyMemberState::Returning;
		Member.bHasOrder = false;
		Broadcast(EConvoyEventType::EscortReturn, &Member, FString::Printf(TEXT("leash %.0f"), Leash));
		IssueMove(Member, GetSlotLocation(Member), false, Now);
	}
	// Squads that dropped their march for the fight but strayed past the leash walk back too.
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.Role != EConvoyUnitRole::EscortSquad or not Member.GetIsAlive()
			or Member.State != EConvoyMemberState::Station)
		{
			continue;
		}
		if (FVector::Dist2D(Member.Location, Centre) > Leash)
		{
			Member.State = EConvoyMemberState::Returning;
			Broadcast(EConvoyEventType::EscortReturn, &Member, FString::Printf(TEXT("leash %.0f"), Leash));
			IssueMove(Member, GetSlotLocation(Member), false, Now);
		}
	}
}

void AConvoy::EndCombat(const double Now)
{
	for (FConvoyMember& Member : M_Members)
	{
		if (Member.State != EConvoyMemberState::Engaging or not Member.GetIsAlive())
		{
			continue;
		}
		Member.State = EConvoyMemberState::Returning;
		Member.bHasOrder = false;
		Broadcast(EConvoyEventType::EscortReturn, &Member, TEXT("fight over"));
	}
	SetState(EConvoyState::Cruise, FString::Printf(TEXT("calm for %.0f s"), Now - M_LastThreatTime));
}

void AConvoy::UpdateTrucksLost(const double Now)
{
	const FConvoyCombatSettings& Combat = M_Definition->Combat;
	const bool bGuarding = Combat.EscortsAfterTrucksLost == EConvoyEscortsAfterTrucksLost::GuardCargo
		and Now - M_StateStartTime < Combat.GuardCargoSeconds;
	const FVector RouteEnd = M_Polyline.GetPointAtDistance(M_Polyline.GetLength());
	bool bAnyAlive = false;
	for (FConvoyMember& Member : M_Members)
	{
		if (not Member.GetIsAlive())
		{
			continue;
		}
		bAnyAlive = true;
		if (not bGuarding and Member.State != EConvoyMemberState::Withdrawing)
		{
			Member.State = EConvoyMemberState::Withdrawing;
			Member.bHasOrder = false;
		}
		const bool bArrived = FVector::Dist2D(Member.Location, RouteEnd) <= M_Definition->Speed.ExitRadius * 2.f;
		const bool bTimedOut = Now - M_StateStartTime > Combat.GuardCargoSeconds + ConvoyCombatConstants::WithdrawTimeoutSeconds;
		if (Member.State == EConvoyMemberState::Withdrawing and (bArrived or bTimedOut))
		{
			ReleaseMember(Member, true);
			continue;
		}
		if (Member.bHasOrder)
		{
			continue;
		}
		if (Member.State == EConvoyMemberState::Withdrawing)
		{
			IssueMove(Member, RouteEnd, false, Now);
			continue;
		}
		// Guard ring around the caches, each escort on its own bearing.
		const float Angle = 2.f * PI * Member.MemberId / FMath::Max(1, M_Members.Num());
		FVector GuardPoint = M_LastThreatLocation;
		if (not M_Caches.IsEmpty())
		{
			GuardPoint = M_Caches[Member.MemberId % M_Caches.Num()].Location;
		}
		GuardPoint += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Combat.GuardRingRadius;
		UConvoyRouteLibrary::ProjectToVehicleNavmesh(GetWorld(), GuardPoint, GuardPoint);
		Member.State = EConvoyMemberState::GuardingCargo;
		IssueMove(Member, GuardPoint, true, Now);
	}
	if (not bAnyAlive)
	{
		FinishConvoy(EConvoyOutcome::TrucksDestroyed, false);
	}
}
