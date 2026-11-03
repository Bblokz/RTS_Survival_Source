// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalConvoyRoutePlanner.h"

#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/SurvivalMode/Events/SurvivalEventSettings.h"

namespace SurvivalConvoyRoutePlannerConstants
{
	constexpr float EvaluationSpacing = 400.f;
	constexpr float StraightLineSpacing = 2300.f;  // round 8: x0.577 with the map scale-down
	// Via points are tried at these bearings around the direction from the base to the entry-exit line.
	const float ViaBearingsDegrees[] = {0.f, -20.f, 20.f, -40.f, 40.f};
	const float ViaDistanceFactors[] = {1.f, 0.9f, 1.15f};
	constexpr float ScoreApproachWeightPerCm = 1.f / 1000.f;
	constexpr float ScoreLengthOverBudgetWeightPerCm = 1.f / 3000.f;
	constexpr float ScoreRandomJitter = 1.5f;
	// The first and last stretch of the route are at the entry and exit; the spawn-zone rule skips them.
	constexpr float EndpointStretchSkipped = 3460.f;  // round 8: x0.577
}

TArray<FVector> FSurvivalConvoyRoutePlanner::Densify(const TArray<FVector>& ControlPoints, const float Spacing)
{
	FConvoyRoutePolyline Polyline;
	Polyline.Build(ControlPoints);
	TArray<FVector> Points;
	if (not Polyline.GetIsValid())
	{
		return ControlPoints;
	}
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Polyline.GetLength() / Spacing));
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		Points.Add(Polyline.GetPointAtDistance(Polyline.GetLength() * Step / Steps));
	}
	return Points;
}

FSurvivalConvoyRouteMetrics FSurvivalConvoyRoutePlanner::Evaluate(const TArray<FVector>& Points,
                                                                  const FSurvivalConvoyRouteRequest& Request)
{
	using namespace SurvivalConvoyRoutePlannerConstants;
	FSurvivalConvoyRouteMetrics Metrics;
	const FSurvivalConvoyRouteSettings& Settings = *Request.Settings;
	FConvoyRoutePolyline Polyline;
	Polyline.Build(Points);
	Metrics.Length = Polyline.GetLength();
	Metrics.ClosestApproach = TNumericLimits<float>::Max();
	Metrics.MinSovietBaseDistance = TNumericLimits<float>::Max();
	Metrics.MinExclusionDistance = TNumericLimits<float>::Max();
	Metrics.MinActiveSpawnDistance = TNumericLimits<float>::Max();
	const int32 Samples = FMath::Max(1, FMath::CeilToInt(Metrics.Length / EvaluationSpacing));
	for (int32 Sample = 0; Sample <= Samples; ++Sample)
	{
		const float Distance = Metrics.Length * Sample / Samples;
		const FVector Point = Polyline.GetPointAtDistance(Distance);
		Metrics.ClosestApproach = FMath::Min(Metrics.ClosestApproach, static_cast<float>(FVector::Dist2D(Point, Request.BaseCentre)));
		for (const FVector& Soviet : Request.SovietBaseCentres)
		{
			Metrics.MinSovietBaseDistance = FMath::Min(Metrics.MinSovietBaseDistance, static_cast<float>(FVector::Dist2D(Point, Soviet)));
		}
		for (const FSurvivalEventExclusionZone& Zone : Settings.ExclusionZones)
		{
			const float Clearance = FVector2D::Distance(FVector2D(Point.X, Point.Y), Zone.Centre) - Zone.Radius;
			Metrics.MinExclusionDistance = FMath::Min(Metrics.MinExclusionDistance, Clearance);
		}
		const bool bAtEndpoint = Distance < EndpointStretchSkipped or Distance > Metrics.Length - EndpointStretchSkipped;
		for (const FVector& Active : Request.ActiveSpawnLocations)
		{
			if (not bAtEndpoint)
			{
				Metrics.MinActiveSpawnDistance = FMath::Min(Metrics.MinActiveSpawnDistance, static_cast<float>(FVector::Dist2D(Point, Active)));
			}
		}
	}
	if (Metrics.ClosestApproach < Settings.MinClosestApproach)
	{
		Metrics.Violation = FString::Printf(TEXT("too close to the base (%.0f)"), Metrics.ClosestApproach);
	}
	else if (Metrics.ClosestApproach > Settings.MaxClosestApproach)
	{
		Metrics.Violation = FString::Printf(TEXT("too far from the base (%.0f)"), Metrics.ClosestApproach);
	}
	else if (Metrics.MinSovietBaseDistance < Settings.SovietBaseClearance)
	{
		Metrics.Violation = FString::Printf(TEXT("through a Soviet base (%.0f)"), Metrics.MinSovietBaseDistance);
	}
	else if (Metrics.MinExclusionDistance < 0.f)
	{
		Metrics.Violation = FString::Printf(TEXT("through an exclusion zone (%.0f)"), Metrics.MinExclusionDistance);
	}
	else if (Metrics.MinActiveSpawnDistance < Settings.ActiveSpawnZoneClearance)
	{
		Metrics.Violation = FString::Printf(TEXT("past an active spawn zone (%.0f)"), Metrics.MinActiveSpawnDistance);
	}
	else if (Metrics.Length < Settings.MinRouteLength or Metrics.Length > Settings.MaxRouteLength)
	{
		Metrics.Violation = FString::Printf(TEXT("length %.0f outside %.0f-%.0f"), Metrics.Length,
		                                    Settings.MinRouteLength, Settings.MaxRouteLength);
	}
	return Metrics;
}

bool FSurvivalConvoyRoutePlanner::GetIsEndpointUsable(const FSurvivalConvoyRouteRequest& Request,
                                                      const FSurvivalConvoyEndpoint& Endpoint, const bool bIsEntry)
{
	const FSurvivalConvoyRouteSettings& Settings = *Request.Settings;
	if (FVector::Dist2D(Endpoint.Location, Request.BaseCentre) < Settings.MinEndpointDistanceFromBase)
	{
		return false;
	}
	for (const FVector& Active : Request.ActiveSpawnLocations)
	{
		if (FVector::Dist2D(Endpoint.Location, Active) < Settings.ActiveSpawnZoneClearance)
		{
			return false;
		}
	}
	if (not bIsEntry)
	{
		return true;
	}
	// Spawned out of sight: no player unit near the entry.
	for (const FVector& PlayerUnit : Request.PlayerUnitLocations)
	{
		if (FVector::Dist2D(Endpoint.Location, PlayerUnit) < Settings.MinSpawnDistanceFromPlayerUnits)
		{
			return false;
		}
	}
	return true;
}

void FSurvivalConvoyRoutePlanner::BuildCandidates(const FSurvivalConvoyRouteRequest& Request, FRandomStream& Random,
                                                  TArray<FCandidate>& OutCandidates)
{
	using namespace SurvivalConvoyRoutePlannerConstants;
	const FSurvivalConvoyRouteSettings& Settings = *Request.Settings;
	const TArray<FSurvivalConvoyEndpoint>& Endpoints = Request.Endpoints;
	for (int32 Entry = 0; Entry < Endpoints.Num(); ++Entry)
	{
		const bool bEntryAllowed = Request.ForcedEntry.IsEmpty() or Endpoints[Entry].Name == Request.ForcedEntry;
		if (not bEntryAllowed or not GetIsEndpointUsable(Request, Endpoints[Entry], true))
		{
			continue;
		}
		for (int32 Exit = 0; Exit < Endpoints.Num(); ++Exit)
		{
			const bool bExitAllowed = Request.ForcedExit.IsEmpty() or Endpoints[Exit].Name == Request.ForcedExit;
			if (Exit == Entry or not bExitAllowed or not GetIsEndpointUsable(Request, Endpoints[Exit], false))
			{
				continue;
			}
			const FVector EntryLocation = Endpoints[Entry].Location;
			const FVector ExitLocation = Endpoints[Exit].Location;
			const FVector OnLine = FMath::ClosestPointOnSegment(Request.BaseCentre, EntryLocation, ExitLocation);
			FVector Outward = (OnLine - Request.BaseCentre).GetSafeNormal2D();
			if (Outward.IsNearlyZero())
			{
				// The straight line runs through the base: bend it towards the map centre side.
				Outward = FVector(-Request.BaseCentre.Y, Request.BaseCentre.X, 0.f).GetSafeNormal2D();
			}
			for (const float Bearing : ViaBearingsDegrees)
			{
				for (const float DistanceFactor : ViaDistanceFactors)
				{
					const FVector Direction = Outward.RotateAngleAxis(Bearing, FVector::UpVector);
					const FVector Via = Request.BaseCentre + Direction * Settings.TargetClosestApproach * DistanceFactor;
					FCandidate Candidate;
					Candidate.EntryIndex = Entry;
					Candidate.ExitIndex = Exit;
					Candidate.ControlPoints = {EntryLocation, Via, ExitLocation};
					const FSurvivalConvoyRouteMetrics Straight = Evaluate(Densify(Candidate.ControlPoints, StraightLineSpacing),
					                                                      Request);
					if (not Straight.GetIsValid())
					{
						continue;
					}
					const float OverBudget = Request.LengthBudget > 0.f ? FMath::Max(0.f, Straight.Length - Request.LengthBudget) : 0.f;
					Candidate.Score = FMath::Abs(Straight.ClosestApproach - Settings.TargetClosestApproach) * ScoreApproachWeightPerCm
						+ OverBudget * ScoreLengthOverBudgetWeightPerCm + Random.FRandRange(0.f, ScoreRandomJitter);
					OutCandidates.Add(Candidate);
				}
			}
		}
	}
	OutCandidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		return A.Score < B.Score;
	});
}

FSurvivalConvoyRoutePlan FSurvivalConvoyRoutePlanner::Plan(UWorld* World, const FSurvivalConvoyRouteRequest& Request)
{
	FSurvivalConvoyRoutePlan Plan;
	if (World == nullptr or Request.Settings == nullptr or Request.Endpoints.Num() < 2)
	{
		Plan.Notes = TEXT("no world, settings or endpoints");
		return Plan;
	}
	FRandomStream Random(Request.Seed);
	TArray<FCandidate> Candidates;
	BuildCandidates(Request, Random, Candidates);
	Plan.CandidatesConsidered = Candidates.Num();
	// One candidate per entry-exit pair first, so a bad pair cannot use up every nav attempt.
	TSet<int64> TriedPairs;
	for (const FCandidate& Candidate : Candidates)
	{
		if (Plan.NavCandidatesTried >= Request.Settings->MaxNavCandidates)
		{
			break;
		}
		const int64 PairKey = static_cast<int64>(Candidate.EntryIndex) * 1000 + Candidate.ExitIndex;
		if (TriedPairs.Contains(PairKey))
		{
			continue;
		}
		TriedPairs.Add(PairKey);
		++Plan.NavCandidatesTried;
		bool bReachedEnd = false;
		const FName RouteId(*FString::Printf(TEXT("%s_to_%s"), *Request.Endpoints[Candidate.EntryIndex].Name,
		                                     *Request.Endpoints[Candidate.ExitIndex].Name));
		FConvoyRoute Route = UConvoyRouteLibrary::BuildNavigableRoute(World, Candidate.ControlPoints,
		                                                              Request.Settings->RouteResampleSpacing, RouteId,
		                                                              bReachedEnd);
		const FSurvivalConvoyRouteMetrics Metrics = Evaluate(Route.Points, Request);
		if (not bReachedEnd or not Metrics.GetIsValid())
		{
			Plan.Notes += FString::Printf(TEXT("[%s rejected: %s] "), *RouteId.ToString(),
			                              bReachedEnd ? *Metrics.Violation : TEXT("navmesh path did not reach the end"));
			continue;
		}
		Plan.bIsValid = true;
		Plan.Route = MoveTemp(Route);
		Plan.EntryName = Request.Endpoints[Candidate.EntryIndex].Name;
		Plan.ExitName = Request.Endpoints[Candidate.ExitIndex].Name;
		Plan.Metrics = Metrics;
		return Plan;
	}
	Plan.Notes += FString::Printf(TEXT("no route passed (%d candidates)"), Candidates.Num());
	return Plan;
}
