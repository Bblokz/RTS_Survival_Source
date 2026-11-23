// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "RTSShortsDirector.h"

#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSShortsFlanks, Log, All);

namespace RTSShortsFlankPrivate
{
	// A flanking vehicle closer than this to its last waypoint has arrived: tanks do not stop dead on a point.
	constexpr float ArrivedCm = 700.f;
	// Rounding in the health component must not read as a hit.
	constexpr float MinimumHitFraction = 0.0005f;

	bool GetIsAlive(const TWeakObjectPtr<AActor>& Unit)
	{
		return Unit.IsValid() && Unit->IsUnitAlive();
	}

	int32 CountAlive(const TArray<TWeakObjectPtr<AActor>>& Line)
	{
		int32 Alive = 0;
		for (const TWeakObjectPtr<AActor>& Unit : Line)
		{
			Alive += GetIsAlive(Unit) ? 1 : 0;
		}
		return Alive;
	}
}

void ARTSShortsDirector::BeginBattleFlanks(const FRTSShortsSegment& Segment)
{
	for (int32 RouteIndex = 0; RouteIndex < Segment.BattleFlankRoutes.Num(); ++RouteIndex)
	{
		const FRTSShortsBattleFlankRoute& Route = Segment.BattleFlankRoutes[RouteIndex];
		if (not M_Battle.EnemyUnits.IsValidIndex(Route.EnemyIndex) || Route.Waypoints.Num() == 0)
		{
			UE_LOG(LogRTSShortsFlanks, Warning, TEXT("RTS_SHORTS_FLANK_ROUTE_UNUSED route=%d enemy=%d waypoints=%d"), RouteIndex,
			       Route.EnemyIndex, Route.Waypoints.Num());
			continue;
		}
		FRTSShortsBattleFlankState Flank;
		Flank.Unit = M_Battle.EnemyUnits[Route.EnemyIndex];
		Flank.RouteIndex = RouteIndex;
		M_Battle.Flanks.Add(Flank);
	}
}

void ARTSShortsDirector::TickBattleFlanks(const int32 Frame)
{
	using namespace RTSShortsFlankPrivate;
	const FRTSShortsSegment& Segment = M_Job.Segments[M_CurrentSegment];
	const float SegmentSeconds = static_cast<float>(Frame - M_SegmentStartFrames[M_CurrentSegment]) / M_Job.Fps;
	const FTransform StageTransform = M_Stage->GetActorTransform();
	for (FRTSShortsBattleFlankState& Flank : M_Battle.Flanks)
	{
		if (Flank.bFinished)
		{
			continue;
		}
		if (not GetIsAlive(Flank.Unit))
		{
			Flank.bFinished = true;
			continue;
		}
		const FRTSShortsBattleFlankRoute& Route = Segment.BattleFlankRoutes[Flank.RouteIndex];
		if (not Flank.bOrdered)
		{
			if (SegmentSeconds >= Route.OrderAtSeconds)
			{
				OrderBattleFlankRoute(Flank, Route, Frame, SegmentSeconds);
			}
			continue;
		}
		const FVector LastWaypoint = StageTransform.TransformPosition(Route.Waypoints.Last());
		const bool bArrived = FVector::Dist2D(Flank.Unit->GetActorLocation(), LastWaypoint) < ArrivedCm;
		if (not bArrived && SegmentSeconds < Flank.OrderedAtSeconds + Route.TimeoutSeconds)
		{
			continue;
		}
		Flank.bFinished = true;
		AddBattleEvent(TEXT("FlankArrived"), false, 1.f, Frame);
		UE_LOG(LogRTSShortsFlanks, Display, TEXT("RTS_SHORTS_FLANK_ARRIVED frame=%d unit=%s arrived=%d at=%s"), Frame,
		       *Flank.Unit->GetName(), bArrived ? 1 : 0, *Flank.Unit->GetActorLocation().ToCompactString());
	}
}

void ARTSShortsDirector::OrderBattleFlankRoute(FRTSShortsBattleFlankState& Flank, const FRTSShortsBattleFlankRoute& Route,
                                               const int32 Frame, const float SegmentSeconds)
{
	Flank.bOrdered = true;
	Flank.OrderedAtSeconds = SegmentSeconds;
	AActor* Unit = Flank.Unit.Get();
	ICommands* Commands = Cast<ICommands>(Unit);
	if (Commands == nullptr)
	{
		Flank.bFinished = true;
		return;
	}
	const FTransform StageTransform = M_Stage->GetActorTransform();
	const FVector StageLocation = StageTransform.GetLocation();
	FVector From = Unit->GetActorLocation();
	for (int32 Index = 0; Index < Route.Waypoints.Num(); ++Index)
	{
		const FVector Destination = GetBattleGroundPoint(StageTransform.TransformPosition(Route.Waypoints[Index]));
		// Every leg ends looking along its drive; the last one faces the stage's unit, which is what it came for.
		const bool bLastLeg = Index == Route.Waypoints.Num() - 1;
		const FRotator Facing = (bLastLeg ? StageLocation - Destination : Destination - From).GetSafeNormal2D().Rotation();
		// The first leg replaces whatever the unit was doing, the rest queue behind it.
		const ECommandQueueError Result = Commands->MoveToLocation(Destination, Index == 0, Facing, false);
		const UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(), From, Destination, Unit);
		UE_LOG(LogRTSShortsFlanks, Display, TEXT("RTS_SHORTS_FLANK_ORDER frame=%d unit=%s leg=%d result=%d pathValid=%d length=%.0f to=%s"),
		       Frame, *Unit->GetName(), Index, static_cast<int32>(Result), Path != nullptr && Path->IsValid() ? 1 : 0,
		       Path != nullptr ? Path->GetPathLength() : 0.f, *Destination.ToCompactString());
		From = Destination;
	}
}

bool ARTSShortsDirector::GetIsBattleUnitSelfDirected(const AActor* Unit) const
{
	if (GetIsEvader(Unit))
	{
		return true;
	}
	// Segment.BattleSquadTactics squads choose their own spots and attacks for the whole battle.
	if (M_Battle.SquadTactics.ContainsByPredicate([Unit](const FRTSShortsSquadTacticState& Tactic) { return Tactic.Squad.Get() == Unit; }))
	{
		return true;
	}
	for (const TPair<TWeakObjectPtr<AActor>, FRTSShortsReflankState>& Reflank : M_Battle.Reflanks)
	{
		if (Reflank.Value.bDriving && Reflank.Key.Get() == Unit)
		{
			return true;
		}
	}
	return M_Battle.Flanks.ContainsByPredicate([Unit](const FRTSShortsBattleFlankState& Flank)
	{
		return not Flank.bFinished && Flank.Unit.Get() == Unit;
	});
}

void ARTSShortsDirector::TrackBattleEvents(const int32 Frame)
{
	TrackBattleLineEvents(M_Battle.PlayerUnits, true, Frame);
	TrackBattleLineEvents(M_Battle.EnemyUnits, false, Frame);
}

void ARTSShortsDirector::TrackBattleLineEvents(const TArray<TWeakObjectPtr<AActor>>& Line, const bool bPlayerSide,
                                               const int32 Frame)
{
	using namespace RTSShortsFlankPrivate;
	for (const TWeakObjectPtr<AActor>& Unit : Line)
	{
		const bool bAlive = GetIsAlive(Unit);
		const UHealthComponent* Health = bAlive ? Unit->FindComponentByClass<UHealthComponent>() : nullptr;
		const float Fraction = Health != nullptr ? Health->GetHealthPercentage() : 0.f;
		float* LastFraction = M_Battle.LastHealth.Find(Unit);
		if (LastFraction == nullptr)
		{
			if (bAlive)
			{
				M_Battle.LastHealth.Add(Unit, Fraction);
			}
			continue;
		}
		if (*LastFraction <= 0.f)
		{
			continue;
		}
		if (not bAlive)
		{
			*LastFraction = 0.f;
			AddBattleEvent(TEXT("Kill"), bPlayerSide, 0.f, Frame);
			continue;
		}
		if (Fraction < *LastFraction - MinimumHitFraction)
		{
			AddBattleEvent(TEXT("Hit"), bPlayerSide, Fraction, Frame);
		}
		*LastFraction = Fraction;
	}
}

void ARTSShortsDirector::AddBattleEvent(const TCHAR* Kind, const bool bPlayerSide, const float HealthFraction, const int32 Frame)
{
	using namespace RTSShortsFlankPrivate;
	FRTSShortsBattleEvent Event;
	Event.Frame = Frame;
	Event.Kind = Kind;
	Event.bPlayerSide = bPlayerSide;
	Event.HealthFraction = HealthFraction;
	Event.PlayersAlive = CountAlive(M_Battle.PlayerUnits);
	Event.EnemiesAlive = CountAlive(M_Battle.EnemyUnits);
	M_Manifest.Segments[M_CurrentSegment].BattleEvents.Add(Event);
	UE_LOG(LogRTSShortsFlanks, Display, TEXT("RTS_SHORTS_BATTLE_EVENT frame=%d kind=%s player=%d health=%.3f alive=%d/%d"), Frame, Kind,
	       bPlayerSide ? 1 : 0, HealthFraction, Event.PlayersAlive, Event.EnemiesAlive);
}
