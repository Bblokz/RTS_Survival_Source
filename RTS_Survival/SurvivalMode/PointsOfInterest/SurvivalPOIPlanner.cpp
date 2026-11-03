// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPOIPlanner.h"

#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOINav.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOISettings.h"
#include "RTS_Survival/SurvivalMode/SovietBases/PCG/SurvivalSovietBasePCGNodes.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartLocation.h"
#include "RTS_Survival/SurvivalMode/Waves/SurvivalEdgeSpawnZone.h"

namespace SurvivalPOIPlannerConstants
{
	constexpr float GroundTraceUp = 20000.f;
	constexpr float GroundTraceDown = 50000.f;
	constexpr int32 SiteRingDirections = 8;
	constexpr int32 RimDirections = 12;
	constexpr float BasinFloorPercentile = 0.25f;
	// Fraction windows inside a sector: landmarks in the middle, generic sites on the side of the start that owns them.
	const FVector2D LandmarkWindow(0.32, 0.68);
	const FVector2D StartASideWindow(0.08, 0.44);
	const FVector2D StartBSideWindow(0.56, 0.92);
	constexpr float RelaxedBandFactor = 1.2f;
	constexpr float SecondRelaxedBandFactor = 1.45f;
	constexpr float SecondRelaxedTerrainFactor = 1.4f;
	constexpr float LandmarkShrinkFactor = 0.8f;
	// A landmark's last resort (it may not move sector): smaller still, a wider band and rougher ground, more path tests.
	constexpr float LandmarkLastShrinkFactor = 0.65f;
	constexpr float LandmarkLastBandFactor = 1.7f;
	constexpr float LandmarkLastBandMinFactor = 0.8f;
	constexpr float LandmarkLastTerrainFactor = 1.7f;
	constexpr int32 LandmarkLastNavChecks = 16;
	constexpr int32 LandmarkLastRelaxLevel = 3;
	// Site yaw offsets tried before a site is rejected because its main object does not fit.
	constexpr float HeroYawOffsets[] = {0.f, 20.f, -20.f};
	// Sites whose story allows either way round (crash, pump, relay, graveyard, convoy) also try the flipped layout, which
	// moves a large main object to the other side of the centre (away from a route or a rim).
	constexpr float FlippableYawOffsets[] = {0.f, 20.f, -20.f, 180.f, 160.f, -160.f};
	// A site along a route keeps the route's direction: only its two ways round are tried.
	constexpr float RouteYawOffsets[] = {0.f, 180.f};
	// A convoy beside a route: its column runs this far outside the route's free width (wrecks keep their footprint clear).
	constexpr float ConvoyColumnVergeOffset = 500.f;
	// Anchor trials (each a light composition) per request and relax level; keeps one request's frame short.
	constexpr int32 MaxAnchorTriesPerRequest = 16;
	// The fairness reach (17 300 on the shrunk map) less a margin.
	constexpr double LandmarkBothStartsReach = 16700.0;
	constexpr int32 WaterTracePoints = 4;
	// A trace that hits anything but the landscape this far above the landscape height is water, a rock or a building.
	constexpr float OpenGroundTolerance = 60.f;
	constexpr int32 SiteSeedStride = 7919;
	constexpr float SeparationScoreWeight = 0.15f;
	constexpr float FlatnessScoreWeight = 0.5f;
	constexpr int32 AnimalSpotAttempts = 40;
	constexpr float AnimalClearanceFromSites = 1200.f;
	constexpr float FlockOffsetFromSite = 600.f;
	constexpr float SectorFlockDistanceFactor = 0.55f;
	// Path queries one site may spend before the request gives up (each is up to five chained queries).
	constexpr int32 MaxNavChecksPerSite = 8;
	constexpr int32 WildlifeSeedStride = 104729;
	// Weight every valid candidate keeps on top of its score, so lower-scored ground still gets picked by some seeds.
	constexpr double CandidateBaseWeight = 0.35;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

namespace SurvivalPOIGeometry
{
	double DistanceToPolyline(const FVector2D& Point, const TArray<FVector2D>& Line)
	{
		double Best = TNumericLimits<double>::Max();
		for (int32 Index = 1; Index < Line.Num(); ++Index)
		{
			const FVector2D Closest = FMath::ClosestPointOnSegment2D(Point, Line[Index - 1], Line[Index]);
			Best = FMath::Min(Best, FVector2D::Distance(Closest, Point));
		}
		return Best;
	}

	bool IsInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Outline)
	{
		bool bInside = false;
		for (int32 Index = 0, Previous = Outline.Num() - 1; Index < Outline.Num(); Previous = Index++)
		{
			const FVector2D& A = Outline[Index];
			const FVector2D& B = Outline[Previous];
			const bool bCrosses = (A.Y > Point.Y) != (B.Y > Point.Y);
			if (bCrosses and Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
			{
				bInside = not bInside;
			}
		}
		return bInside;
	}

	double DistanceToPolygon(const FVector2D& Point, const TArray<FVector2D>& Outline)
	{
		if (Outline.Num() < 3 or IsInsidePolygon(Point, Outline))
		{
			return Outline.Num() < 3 ? TNumericLimits<double>::Max() : 0.0;
		}
		TArray<FVector2D> Closed = Outline;
		Closed.Add(Outline[0]);
		return DistanceToPolyline(Point, Closed);
	}
}

// ---------------------------------------------------------------------------
// World context
// ---------------------------------------------------------------------------

void FSurvivalPOIWorldContext::Gather(UWorld& World, const TArray<ASurvivalStartLocation*>& StartLocations)
{
	LandscapeBounds = FBox2D(ForceInit);
	for (TActorIterator<ALandscapeProxy> It(&World); It; ++It)
	{
		const FBox Box = It->GetComponentsBoundingBox();
		if (Box.IsValid)
		{
			LandscapeBounds += FBox2D(FVector2D(Box.Min), FVector2D(Box.Max));
		}
	}
	Starts.Reset();
	ReservedSpots.Reset();
	FVector2D StartSum = FVector2D::ZeroVector;
	for (int32 Index = 0; Index < StartLocations.Num(); ++Index)
	{
		const ASurvivalStartLocation* Location = StartLocations[Index];
		if (not IsValid(Location))
		{
			continue;
		}
		FSurvivalPOIStart& Start = Starts.AddDefaulted_GetRef();
		const FVector Center = Location->GetDeployCenter();
		Start.Position = FVector2D(Center);
		Start.Height = Center.Z;
		Start.Name = Location->GetDisplayName().ToString();
		Start.DirectorIndex = Index;
		// One rule everywhere (critic, round 1): paths start at the deploy centre.
		Start.NavOrigin = Center;
		StartSum += Start.Position;
		// Protected on the start; read through reflection (actor-relative XY from the deploy centre, Z = radius).
		const FArrayProperty* SpotsProperty = FindFProperty<FArrayProperty>(Location->GetClass(), TEXT("ReservedContentSpots"));
		const TArray<FVector>* Spots = SpotsProperty != nullptr and CastField<FStructProperty>(SpotsProperty->Inner) != nullptr
			                               ? SpotsProperty->ContainerPtrToValuePtr<TArray<FVector>>(Location)
			                               : nullptr;
		if (Spots == nullptr)
		{
			continue;
		}
		for (const FVector& Spot : *Spots)
		{
			ReservedSpots.Add(FVector(Start.Position + FVector2D(Spot), Spot.Z));
		}
	}
	BasinCenter = Starts.IsEmpty() ? LandscapeBounds.GetCenter() : StartSum / Starts.Num();
	for (FSurvivalPOIStart& Start : Starts)
	{
		const FVector2D Offset = Start.Position - BasinCenter;
		Start.AngleRadians = FMath::Atan2(Offset.Y, Offset.X);
	}
	Starts.Sort([](const FSurvivalPOIStart& A, const FSurvivalPOIStart& B) { return A.AngleRadians < B.AngleRadians; });

	SpawnZoneSegments.Reset();
	for (TActorIterator<ASurvivalEdgeSpawnZone> It(&World); It; ++It)
	{
		const FVector2D Center(It->GetSpawnCenter());
		const FVector2D Inward(It->GetInwardDirection());
		const FVector2D Along(-Inward.Y, Inward.X);
		const double HalfLength = It->GetEdgeLength() * 0.5;
		SpawnZoneSegments.Emplace(Center - Along * HalfLength, Center + Along * HalfLength);
	}
	SovietBaseBoxes.Reset();
	for (TActorIterator<ASurvivalSovietBasePCGVolume> It(&World); It; ++It)
	{
		const FBox Box = It->GetComponentsBoundingBox(true);
		if (Box.IsValid)
		{
			SovietBaseBoxes.Add(FBox2D(FVector2D(Box.Min), FVector2D(Box.Max)));
		}
	}
}

// ---------------------------------------------------------------------------
// Height field
// ---------------------------------------------------------------------------

void FSurvivalPOIHeightField::Init(const FBox2D& Bounds, const float Spacing)
{
	M_Bounds = Bounds;
	M_Spacing = FMath::Max(100.f, Spacing);
	const FVector2D Size = Bounds.GetSize();
	M_CountX = FMath::Max(2, FMath::CeilToInt(Size.X / M_Spacing) + 1);
	M_CountY = FMath::Max(2, FMath::CeilToInt(Size.Y / M_Spacing) + 1);
	M_Heights.Init(TNumericLimits<float>::Lowest(), M_CountX * M_CountY);
	M_NextSample = 0;
}

bool FSurvivalPOIHeightField::Fill(const FSurvivalGroundProjector& Projector, const int32 MaxSamples, const double BudgetSeconds)
{
	constexpr int32 TimeCheckInterval = 64;
	const double Deadline = FPlatformTime::Seconds() + BudgetSeconds;
	const int32 End = FMath::Min(M_Heights.Num(), M_NextSample + FMath::Max(1, MaxSamples));
	for (; M_NextSample < End; ++M_NextSample)
	{
		if (M_NextSample % TimeCheckInterval == 0 and FPlatformTime::Seconds() > Deadline)
		{
			break;
		}
		const int32 X = M_NextSample % M_CountX;
		const int32 Y = M_NextSample / M_CountX;
		const FVector2D Position = M_Bounds.Min + FVector2D(X, Y) * M_Spacing;
		float Height = 0.f;
		M_Heights[M_NextSample] = Projector.TryGetHeight(Position, Height) ? Height : TNumericLimits<float>::Lowest();
	}
	return GetIsFilled();
}

float FSurvivalPOIHeightField::GetSample(const int32 X, const int32 Y) const
{
	const int32 ClampedX = FMath::Clamp(X, 0, M_CountX - 1);
	const int32 ClampedY = FMath::Clamp(Y, 0, M_CountY - 1);
	return M_Heights[ClampedY * M_CountX + ClampedX];
}

bool FSurvivalPOIHeightField::GetHeight(const FVector2D& Position, float& OutHeight) const
{
	if (M_Heights.IsEmpty() or not M_Bounds.IsInside(Position))
	{
		return false;
	}
	const FVector2D Grid = (Position - M_Bounds.Min) / M_Spacing;
	const int32 X = FMath::FloorToInt(Grid.X);
	const int32 Y = FMath::FloorToInt(Grid.Y);
	const float FractionX = Grid.X - X;
	const float FractionY = Grid.Y - Y;
	const float H00 = GetSample(X, Y);
	const float H10 = GetSample(X + 1, Y);
	const float H01 = GetSample(X, Y + 1);
	const float H11 = GetSample(X + 1, Y + 1);
	constexpr float Missing = TNumericLimits<float>::Lowest();
	if (H00 == Missing or H10 == Missing or H01 == Missing or H11 == Missing)
	{
		return false;
	}
	OutHeight = FMath::Lerp(FMath::Lerp(H00, H10, FractionX), FMath::Lerp(H01, H11, FractionX), FractionY);
	return true;
}

float FSurvivalPOIHeightField::GetSlopeDegrees(const FVector2D& Position) const
{
	float Center = 0.f;
	float East = 0.f;
	float North = 0.f;
	float West = 0.f;
	float South = 0.f;
	if (not GetHeight(Position, Center) or not GetHeight(Position + FVector2D(M_Spacing, 0.0), East)
		or not GetHeight(Position + FVector2D(0.0, M_Spacing), North) or not GetHeight(Position - FVector2D(M_Spacing, 0.0), West)
		or not GetHeight(Position - FVector2D(0.0, M_Spacing), South))
	{
		return 0.f;
	}
	const float GradientX = (East - West) / (2.f * M_Spacing);
	const float GradientY = (North - South) / (2.f * M_Spacing);
	return FMath::RadiansToDegrees(FMath::Atan(FMath::Sqrt(GradientX * GradientX + GradientY * GradientY)));
}

// ---------------------------------------------------------------------------
// Planner: sectors and candidates
// ---------------------------------------------------------------------------

FSurvivalPOIPlanner::FSurvivalPOIPlanner(UWorld& World, const USurvivalPointsOfInterestSettings& Settings, const int32 MatchSeed,
                                         const TArray<ASurvivalStartLocation*>& StartLocations)
	: M_World(&World)
	, M_Settings(&Settings)
	, M_Projector(World, SurvivalPOIPlannerConstants::GroundTraceUp, SurvivalPOIPlannerConstants::GroundTraceDown)
	, M_MatchSeed(MatchSeed)
{
	M_Context.Gather(World, StartLocations);
	const FSurvivalPOIPlacementRules& Rules = Settings.Placement;
	if (Rules.PlayableAreaMax.X > Rules.PlayableAreaMin.X and Rules.PlayableAreaMax.Y > Rules.PlayableAreaMin.Y)
	{
		// The landscape can be larger than the play area (a cropped map keeps its old landscape outside the edge).
		const FBox2D Playable(Rules.PlayableAreaMin, Rules.PlayableAreaMax);
		const FBox2D& Landscape = M_Context.LandscapeBounds;
		M_Context.LandscapeBounds = Landscape.bIsValid
			                            ? FBox2D(FVector2D(FMath::Max(Landscape.Min.X, Playable.Min.X), FMath::Max(Landscape.Min.Y, Playable.Min.Y)),
			                                     FVector2D(FMath::Min(Landscape.Max.X, Playable.Max.X), FMath::Min(Landscape.Max.Y, Playable.Max.Y)))
			                            : Playable;
	}
	FBox2D FieldBounds = M_Context.LandscapeBounds;
	FieldBounds.Min += FVector2D(Settings.Placement.MinDistanceFromLandscapeEdge);
	FieldBounds.Max -= FVector2D(Settings.Placement.MinDistanceFromLandscapeEdge);
	if (FieldBounds.Min.X >= FieldBounds.Max.X or FieldBounds.Min.Y >= FieldBounds.Max.Y)
	{
		FieldBounds = M_Context.LandscapeBounds;
	}
	M_HeightField.Init(FieldBounds, Settings.Placement.HeightGridSpacing);
	for (FSurvivalPOIStart& Start : M_Context.Starts)
	{
		for (const FSurvivalPOILayoutLine& Ramp : Settings.Placement.Layout.Ramps)
		{
			if (Ramp.Name.StartsWith(Start.Name + TEXT("/")) and Ramp.Points.Num() >= 2)
			{
				Start.GateFeet.Add(Ramp.FootPoint);
				// The drive from the deploy centre to this foot: to the ramp's top, then down the whole ramp line.
				double RampLength = 0.0;
				for (int32 Index = 0; Index + 1 < Ramp.Points.Num(); ++Index)
				{
					RampLength += FVector2D::Distance(Ramp.Points[Index], Ramp.Points[Index + 1]);
				}
				const double ToTop = FMath::Min(FVector2D::Distance(Start.Position, Ramp.Points[0]),
				                                FVector2D::Distance(Start.Position, Ramp.Points.Last()));
				Start.GateFootDrives.Add(ToTop + RampLength);
			}
		}
	}
	BuildSectors();
}

void FSurvivalPOIPlanner::BuildSectors()
{
	M_Sectors.Reset();
	const int32 StartCount = M_Context.Starts.Num();
	if (StartCount < 2)
	{
		M_PlanNotes.Add(TEXT("fewer than two starts: no sectors"));
		return;
	}
	for (int32 Index = 0; Index < StartCount; ++Index)
	{
		FSurvivalPOISector& Sector = M_Sectors.AddDefaulted_GetRef();
		Sector.SectorIndex = Index;
		Sector.StartA = Index;
		Sector.StartB = (Index + 1) % StartCount;
		Sector.AngleA = M_Context.Starts[Sector.StartA].AngleRadians;
		Sector.AngleB = M_Context.Starts[Sector.StartB].AngleRadians;
		if (Sector.AngleB <= Sector.AngleA)
		{
			Sector.AngleB += UE_TWO_PI;
		}
		const FString& NameA = M_Context.Starts[Sector.StartA].Name;
		const FString& NameB = M_Context.Starts[Sector.StartB].Name;
		for (const FSurvivalPOISectorLandmark& Entry : M_Settings->SectorLandmarks)
		{
			const bool bMatches = (Entry.StartA == NameA and Entry.StartB == NameB) or (Entry.StartA == NameB and Entry.StartB == NameA);
			if (bMatches)
			{
				Sector.Landmark = Entry.Landmark;
				Sector.bLandmarkBuilt = M_Settings->BuiltLandmarkTypes.Contains(Entry.Landmark);
				break;
			}
		}
		if (Sector.Landmark == ESurvivalPOIType::None)
		{
			M_PlanNotes.Add(FString::Printf(TEXT("sector %d (%s - %s) has no landmark entry"), Index, *NameA, *NameB));
		}
		else if (not Sector.bLandmarkBuilt)
		{
			M_PlanNotes.Add(FString::Printf(TEXT("sector %d (%s - %s): landmark %s has no composition yet"), Index, *NameA,
			                                *NameB, *SurvivalPOINames::GetTypeName(Sector.Landmark)));
		}
	}
}

float FSurvivalPOIPlanner::GetFractionInSector(const FVector2D& Position, const FSurvivalPOISector& Sector) const
{
	const FVector2D Offset = Position - M_Context.BasinCenter;
	double Angle = FMath::Atan2(Offset.Y, Offset.X);
	while (Angle < Sector.AngleA)
	{
		Angle += UE_TWO_PI;
	}
	return static_cast<float>((Angle - Sector.AngleA) / (Sector.AngleB - Sector.AngleA));
}

int32 FSurvivalPOIPlanner::GetSectorOf(const FVector2D& Position) const
{
	for (const FSurvivalPOISector& Sector : M_Sectors)
	{
		const float Fraction = GetFractionInSector(Position, Sector);
		if (Fraction >= 0.f and Fraction <= 1.f)
		{
			return Sector.SectorIndex;
		}
	}
	return INDEX_NONE;
}

float FSurvivalPOIPlanner::GetDistanceToNearestStart(const FVector2D& Position) const
{
	float Best = TNumericLimits<float>::Max();
	for (const FSurvivalPOIStart& Start : M_Context.Starts)
	{
		Best = FMath::Min(Best, static_cast<float>(FVector2D::Distance(Start.Position, Position)));
	}
	return Best;
}

bool FSurvivalPOIPlanner::StepSiteSearch(const int32 MaxSamples, const double BudgetSeconds)
{
	if (bM_SitesChosen)
	{
		return true;
	}
	if (not M_HeightField.GetIsFilled())
	{
		M_HeightField.Fill(M_Projector, MaxSamples, BudgetSeconds);
		return false;
	}
	if (not bM_CandidatesBuilt)
	{
		BuildCandidates();
		bM_CandidatesBuilt = true;
		M_Sites.Reset();
		BuildRequestQueue();
		return false;
	}
	// One site request per step: each may run several path queries.
	if (RunNextQueuedRequest())
	{
		return false;
	}
	for (int32 Index = 0; Index < M_Sites.Num(); ++Index)
	{
		M_Sites[Index].SiteIndex = Index;
	}
	bM_SitesChosen = true;
	return true;
}

void FSurvivalPOIPlanner::ReplanSites(const int32 NewMatchSeed)
{
	M_MatchSeed = NewMatchSeed;
	M_Sites.Reset();
	M_PlanNotes.Reset();
	// Sectors (with their basin floor heights from the candidates) do not depend on the seed; keep them.
	if (not bM_CandidatesBuilt)
	{
		BuildCandidates();
		bM_CandidatesBuilt = true;
	}
	ChooseSites();
	bM_SitesChosen = true;
}

void FSurvivalPOIPlanner::BuildCandidates()
{
	M_Candidates.Reset();
	const FSurvivalPOIPlacementRules& Rules = M_Settings->Placement;
	FBox2D Area = M_Context.LandscapeBounds;
	Area.Min += FVector2D(Rules.MinDistanceFromLandscapeEdge);
	Area.Max -= FVector2D(Rules.MinDistanceFromLandscapeEdge);
	const float Spacing = Rules.CandidateSpacing;
	TArray<TArray<float>> HeightsPerSector;
	HeightsPerSector.SetNum(M_Sectors.Num());
	for (double X = Area.Min.X; X <= Area.Max.X; X += Spacing)
	{
		for (double Y = Area.Min.Y; Y <= Area.Max.Y; Y += Spacing)
		{
			const FVector2D Position(X, Y);
			// The cheap tests first: most of the map is near a start or at the edge.
			if (GetDistanceToNearestStart(Position) < Rules.MinDistanceFromStart or not GetIsClearOfExclusions(Position, 0.f, false))
			{
				continue;
			}
			FCandidate Candidate;
			Candidate.Position = Position;
			Candidate.Sector = GetSectorOf(Position);
			if (Candidate.Sector == INDEX_NONE or not M_HeightField.GetHeight(Position, Candidate.Height))
			{
				continue;
			}
			Candidate.SectorFraction = GetFractionInSector(Position, M_Sectors[Candidate.Sector]);
			HeightsPerSector[Candidate.Sector].Add(Candidate.Height);
			M_Candidates.Add(Candidate);
		}
	}
	for (FSurvivalPOISector& Sector : M_Sectors)
	{
		TArray<float>& Heights = HeightsPerSector[Sector.SectorIndex];
		if (Heights.IsEmpty())
		{
			continue;
		}
		Heights.Sort();
		const int32 PercentileIndex = FMath::Clamp(
			FMath::FloorToInt(Heights.Num() * SurvivalPOIPlannerConstants::BasinFloorPercentile), 0, Heights.Num() - 1);
		Sector.BasinFloorHeight = Heights[PercentileIndex];
	}
}

bool FSurvivalPOIPlanner::GetIsClearOfExclusions(const FVector2D& Position, const float Radius, const bool bIncludeLayout) const
{
	const FSurvivalPOIPlacementRules& Rules = M_Settings->Placement;
	if (GetDistanceToNearestStart(Position) < Rules.MinDistanceFromStart + Radius)
	{
		return false;
	}
	FBox2D Inner = M_Context.LandscapeBounds;
	Inner.Min += FVector2D(Rules.MinDistanceFromLandscapeEdge + Radius);
	Inner.Max -= FVector2D(Rules.MinDistanceFromLandscapeEdge + Radius);
	if (not Inner.IsInside(Position))
	{
		return false;
	}
	for (const TPair<FVector2D, FVector2D>& Segment : M_Context.SpawnZoneSegments)
	{
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(Position, Segment.Key, Segment.Value);
		if (FVector2D::Distance(Closest, Position) < Rules.MinDistanceFromEdgeSpawnZone + Radius)
		{
			return false;
		}
	}
	for (const FBox2D& Box : M_Context.SovietBaseBoxes)
	{
		const FBox2D Expanded = Box.ExpandBy(Rules.SovietBaseClearance + Radius);
		if (Expanded.IsInside(Position))
		{
			return false;
		}
	}
	for (const FSurvivalPOIExclusionZone& Zone : Rules.ExclusionZones)
	{
		if (FVector2D::Distance(Zone.Center, Position) < Zone.Radius + Zone.PropRim + Radius)
		{
			return false;
		}
	}
	for (const FVector& Spot : M_Context.ReservedSpots)
	{
		if (FVector2D::Distance(FVector2D(Spot), Position) < Spot.Z + Rules.ReservedSpotClearance + Radius)
		{
			return false;
		}
	}
	return not bIncludeLayout or GetIsClearOfLayout(Position, Radius);
}

bool FSurvivalPOIPlanner::GetIsClearOfLayout(const FVector2D& Position, const float Radius, const bool bAllowRoutes) const
{
	const FSurvivalPOILayoutExclusions& Layout = M_Settings->Placement.Layout;
	if (not bAllowRoutes)
	{
		for (const FSurvivalPOILayoutLine& Route : Layout.Routes)
		{
			if (SurvivalPOIGeometry::DistanceToPolyline(Position, Route.Points) < Radius + Layout.RouteSiteClearance)
			{
				return false;
			}
		}
	}
	for (const FSurvivalPOILayoutLine& Ramp : Layout.Ramps)
	{
		if (SurvivalPOIGeometry::DistanceToPolyline(Position, Ramp.Points) < Layout.RampHalfWidth + Radius
			or FVector2D::Distance(Ramp.FootPoint, Position) < Layout.GateFootClearance + Radius)
		{
			return false;
		}
	}
	for (const FSurvivalPOILayoutArea& Area : Layout.Areas)
	{
		if (SurvivalPOIGeometry::DistanceToPolygon(Position, Area.Outline) < Area.Clearance + Radius)
		{
			return false;
		}
	}
	for (const FVector& Obstacle : Layout.Obstacles)
	{
		if (FVector2D::Distance(FVector2D(Obstacle), Position) < Obstacle.Z + Radius)
		{
			return false;
		}
	}
	return true;
}

bool FSurvivalPOIPlanner::GetIsNearWater(const FVector2D& Position, const float Margin) const
{
	for (const FSurvivalPOIExclusionZone& Zone : M_Settings->Placement.ExclusionZones)
	{
		if (Zone.bIsWater and FVector2D::Distance(Zone.Center, Position) < Zone.Radius + Margin)
		{
			return true;
		}
	}
	return false;
}

bool FSurvivalPOIPlanner::GetIsTerrainFit(const FVector2D& Position, const FSurvivalPOITypeRules& Rules,
                                          const FSurvivalPOISector& Sector, float& OutHeightRange) const
{
	using namespace SurvivalPOIPlannerConstants;
	const FSurvivalPOIPlacementRules& Placement = M_Settings->Placement;
	float CenterHeight = 0.f;
	if (not M_HeightField.GetHeight(Position, CenterHeight)
		or CenterHeight > Sector.BasinFloorHeight + Placement.MaxHeightAboveBasinFloor)
	{
		return false;
	}
	float MinHeight = CenterHeight;
	float MaxHeight = CenterHeight;
	float MaxSlope = M_HeightField.GetSlopeDegrees(Position);
	for (int32 Ring = 1; Ring <= 2; ++Ring)
	{
		const float RingRadius = Rules.SiteRadius * Ring * 0.5f;
		for (int32 Direction = 0; Direction < SiteRingDirections; ++Direction)
		{
			const float Angle = UE_TWO_PI * (Direction + 0.5f * (Ring - 1)) / SiteRingDirections;
			const FVector2D Sample = Position + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius;
			float Height = 0.f;
			if (not M_HeightField.GetHeight(Sample, Height))
			{
				return false;
			}
			MinHeight = FMath::Min(MinHeight, Height);
			MaxHeight = FMath::Max(MaxHeight, Height);
			MaxSlope = FMath::Max(MaxSlope, M_HeightField.GetSlopeDegrees(Sample));
		}
	}
	OutHeightRange = MaxHeight - MinHeight;
	if (OutHeightRange > Rules.MaxHeightRange or MaxSlope > Rules.MaxSlopeDegrees)
	{
		return false;
	}
	// No cliff, pad rim or ramp right next to the site.
	const float RimRadius = Rules.SiteRadius + Placement.RimCheckDistance;
	for (int32 Direction = 0; Direction < RimDirections; ++Direction)
	{
		const float Angle = UE_TWO_PI * Direction / RimDirections;
		const FVector2D Sample = Position + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RimRadius;
		float Height = 0.f;
		if (not M_HeightField.GetHeight(Sample, Height))
		{
			return false;
		}
		if (Height - CenterHeight > Placement.MaxRimRise or CenterHeight - Height > Placement.MaxRimDrop)
		{
			return false;
		}
	}
	return true;
}

bool FSurvivalPOIPlanner::GetIsOpenGroundByTrace(const FVector2D& Position, const float Radius) const
{
	using namespace SurvivalPOIPlannerConstants;
	UWorld* World = M_World.Get();
	if (not IsValid(World))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SurvivalPOIOpenGround), false);
	for (int32 Point = 0; Point <= WaterTracePoints; ++Point)
	{
		FVector2D Sample = Position;
		if (Point > 0)
		{
			const float Angle = UE_TWO_PI * Point / WaterTracePoints;
			Sample += FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius * 0.6f;
		}
		float LandscapeHeight = 0.f;
		if (not M_Projector.TryGetHeight(Sample, LandscapeHeight))
		{
			return false;
		}
		FHitResult Hit;
		const FVector Top(Sample, LandscapeHeight + GroundTraceUp);
		const FVector Bottom(Sample, LandscapeHeight - OpenGroundTolerance);
		if (not World->LineTraceSingleByChannel(Hit, Top, Bottom, ECC_Visibility, Params))
		{
			continue;
		}
		const bool bHitLandscape = IsValid(Hit.GetActor()) and Hit.GetActor()->IsA<ALandscapeProxy>();
		if (not bHitLandscape and Hit.ImpactPoint.Z > LandscapeHeight + OpenGroundTolerance)
		{
			return false;
		}
		if (not bHitLandscape and Hit.ImpactPoint.Z > LandscapeHeight - OpenGroundTolerance)
		{
			// Water planes sit at the ground there; anything not landscape is no open ground.
			return false;
		}
	}
	return true;
}

// ---------------------------------------------------------------------------
// Planner: sites
// ---------------------------------------------------------------------------

void FSurvivalPOIPlanner::ChooseSites()
{
	M_Sites.Reset();
	BuildRequestQueue();
	while (RunNextQueuedRequest())
	{
	}
	for (int32 Index = 0; Index < M_Sites.Num(); ++Index)
	{
		M_Sites[Index].SiteIndex = Index;
	}
}

TArray<FSurvivalPOIPlanner::FSiteRequest> FSurvivalPOIPlanner::GetSiteRequests(const FSurvivalPOISector& Sector) const
{
	using namespace SurvivalPOIPlannerConstants;
	TArray<FSiteRequest> Requests;
	if (Sector.bLandmarkBuilt)
	{
		FSiteRequest& Landmark = Requests.AddDefaulted_GetRef();
		Landmark.Type = Sector.Landmark;
		Landmark.bIsLandmark = true;
		Landmark.FractionWindow = LandmarkWindow;
	}
	if (M_Settings->GenericSitesPerSector == 1 and not M_Settings->GenericPerSector.IsEmpty())
	{
		// One per sector, owned by its first start: every start owns exactly one; the type alternates by sector and seed.
		FSiteRequest& Generic = Requests.AddDefaulted_GetRef();
		const int32 TypeIndex = FMath::Abs(Sector.SectorIndex + M_MatchSeed) % M_Settings->GenericPerSector.Num();
		Generic.Type = M_Settings->GenericPerSector[TypeIndex];
		Generic.bOwnedByStartA = true;
		Generic.FractionWindow = StartASideWindow;
		return Requests;
	}
	for (int32 Index = 0; Index < M_Settings->GenericPerSector.Num() and Index < M_Settings->GenericSitesPerSector; ++Index)
	{
		FSiteRequest& Generic = Requests.AddDefaulted_GetRef();
		Generic.Type = M_Settings->GenericPerSector[Index];
		Generic.bOwnedByStartA = Index % 2 == 0;
		Generic.FractionWindow = Generic.bOwnedByStartA ? StartASideWindow : StartBSideWindow;
	}
	return Requests;
}

void FSurvivalPOIPlanner::BuildRequestQueue()
{
	M_RequestQueue.Reset();
	M_SectorRandoms.Reset();
	M_NextRequest = 0;
	TArray<FQueuedRequest> Generics;
	for (const FSurvivalPOISector& Sector : M_Sectors)
	{
		M_SectorRandoms.Emplace(HashCombine(GetTypeHash(M_MatchSeed), GetTypeHash(Sector.SectorIndex)));
		for (const FSiteRequest& Request : GetSiteRequests(Sector))
		{
			(Request.bIsLandmark ? M_RequestQueue : Generics).Add({Sector.SectorIndex, Request});
		}
	}
	M_RequestQueue.Append(Generics);
}

bool FSurvivalPOIPlanner::RunNextQueuedRequest()
{
	if (not M_RequestQueue.IsValidIndex(M_NextRequest))
	{
		return false;
	}
	const FQueuedRequest Queued = M_RequestQueue[M_NextRequest++];
	if (M_Sectors.IsValidIndex(Queued.SectorIndex) and M_SectorRandoms.IsValidIndex(Queued.SectorIndex))
	{
		RunSiteRequest(M_Sectors[Queued.SectorIndex], Queued.Request, M_SectorRandoms[Queued.SectorIndex]);
	}
	return true;
}

void FSurvivalPOIPlanner::RunSiteRequest(const FSurvivalPOISector& Sector, const FSiteRequest& Request, FRandomStream& Random)
{
	if (TryAddSite(Sector, Request, 0, Random) or TryAddSite(Sector, Request, 1, Random) or TryAddSite(Sector, Request, 2, Random))
	{
		return;
	}
	using namespace SurvivalPOIPlannerConstants;
	if (Request.bIsLandmark)
	{
		if (TryAddSite(Sector, Request, LandmarkLastRelaxLevel, Random))
		{
			return;
		}
		M_PlanNotes.Add(FString::Printf(TEXT("sector %d: no ground for %s"), Sector.SectorIndex,
		                                *SurvivalPOINames::GetTypeName(Request.Type)));
		return;
	}
	// A generic site keeps its owner (every start owns one): the neighbouring sector on the owner's side, strict first, then
	// the last resort in the home sector and in that neighbour.
	const int32 OwnerStart = Request.bOwnedByStartA ? Sector.StartA : Sector.StartB;
	const FSurvivalPOISector* Other = M_Sectors.Num() < 2 ? nullptr : M_Sectors.FindByPredicate(
		[&Sector, OwnerStart](const FSurvivalPOISector& Candidate)
		{
			return Candidate.SectorIndex != Sector.SectorIndex and (Candidate.StartA == OwnerStart or Candidate.StartB == OwnerStart);
		});
	FSiteRequest Moved = Request;
	if (Other != nullptr)
	{
		Moved.bOwnedByStartA = Other->StartA == OwnerStart;
		Moved.FractionWindow = Moved.bOwnedByStartA ? StartASideWindow : StartBSideWindow;
		if (TryAddSite(*Other, Moved, 0, Random) or TryAddSite(*Other, Moved, 1, Random) or TryAddSite(*Other, Moved, 2, Random))
		{
			M_PlanNotes.Add(FString::Printf(TEXT("sector %d: %s moved to sector %d (same owner start)"), Sector.SectorIndex,
			                                *SurvivalPOINames::GetTypeName(Request.Type), Other->SectorIndex));
			return;
		}
	}
	if (TryAddSite(Sector, Request, LandmarkLastRelaxLevel, Random))
	{
		return;
	}
	if (Other != nullptr and TryAddSite(*Other, Moved, LandmarkLastRelaxLevel, Random))
	{
		M_PlanNotes.Add(FString::Printf(TEXT("sector %d: %s moved to sector %d (same owner start, last resort)"), Sector.SectorIndex,
		                                *SurvivalPOINames::GetTypeName(Request.Type), Other->SectorIndex));
		return;
	}
	// A hangar needs a large flat footprint; rather than leave its start without a site, the start gets a battlefield
	// (its own capture targets, the gun pits) through the same chain.
	if (Request.Type == ESurvivalPOIType::RuinedHangar)
	{
		M_PlanNotes.Add(FString::Printf(TEXT("sector %d: no ground for hangar, trying a battlefield"), Sector.SectorIndex));
		FSiteRequest Swapped = Request;
		Swapped.Type = ESurvivalPOIType::SmallBattlefield;
		RunSiteRequest(Sector, Swapped, Random);
		return;
	}
	M_PlanNotes.Add(FString::Printf(TEXT("sector %d: no ground for %s"), Sector.SectorIndex,
	                                *SurvivalPOINames::GetTypeName(Request.Type)));
}

float FSurvivalPOIPlanner::ScoreSite(const FCandidate& Candidate, const FSurvivalPOITypeRules& Rules,
                                     const FSurvivalPOISector& Sector, const FVector2D& OwnerPosition,
                                     const FVector2D& DistanceBand, int32* OutRejectCounts) const
{
	using namespace SurvivalPOIPlannerConstants;
	const float OwnerDistance = FVector2D::Distance(OwnerPosition, Candidate.Position);
	if (OwnerDistance < DistanceBand.X or OwnerDistance > DistanceBand.Y)
	{
		++OutRejectCounts[0];
		return -1.f;
	}
	if (not GetIsClearOfExclusions(Candidate.Position, Rules.SiteRadius, false))
	{
		++OutRejectCounts[1];
		return -1.f;
	}
	const float MinGap = M_Settings->Placement.MinGapBetweenSites;
	float NearestOther = TNumericLimits<float>::Max();
	for (const FSurvivalPOISite& Other : M_Sites)
	{
		const float Distance = FVector2D::Distance(FVector2D(Other.Center), Candidate.Position);
		if (Distance < Other.Radius + Rules.SiteRadius + MinGap)
		{
			++OutRejectCounts[2];
			return -1.f;
		}
		NearestOther = FMath::Min(NearestOther, Distance);
	}
	float HeightRange = 0.f;
	if (not GetIsTerrainFit(Candidate.Position, Rules, Sector, HeightRange))
	{
		++OutRejectCounts[3];
		return -1.f;
	}
	if (not GetIsClearOfLayout(Candidate.Position, Rules.SiteRadius, GetMayLieAlongRoute(Rules.Type)))
	{
		++OutRejectCounts[4];
		return -1.f;
	}
	const float BandMiddle = 0.5f * (DistanceBand.X + DistanceBand.Y);
	const float BandHalf = FMath::Max(1.f, 0.5f * (DistanceBand.Y - DistanceBand.X));
	const float BandScore = 1.f - FMath::Abs(OwnerDistance - BandMiddle) / BandHalf;
	const float FlatScore = 1.f - HeightRange / FMath::Max(1.f, Rules.MaxHeightRange);
	const float SeparationScore = FMath::Min(1.f, NearestOther / (4.f * (Rules.SiteRadius + MinGap)));
	return BandScore + FlatnessScoreWeight * FlatScore + SeparationScoreWeight * SeparationScore;
}

bool FSurvivalPOIPlanner::TryAddSite(const FSurvivalPOISector& Sector, const FSiteRequest& Request, const int32 RelaxLevel,
                                     FRandomStream& Random)
{
	using namespace SurvivalPOIPlannerConstants;
	const FSurvivalPOITypeRules* Rules = M_Settings->FindTypeRules(Request.Type);
	if (Rules == nullptr)
	{
		return false;
	}
	const bool bRelaxed = RelaxLevel > 0;
	const FVector2D Window = bRelaxed ? FVector2D(0.03, 0.97) : Request.FractionWindow;
	FVector2D Band = Rules->OwnerStartDistance;
	const bool bLastResort = RelaxLevel >= LandmarkLastRelaxLevel;
	if (bRelaxed)
	{
		Band = bLastResort
			       ? FVector2D(Band.X * LandmarkLastBandMinFactor, Band.Y * LandmarkLastBandFactor)
			       : FVector2D(Band.X, Band.Y * (RelaxLevel == 1 ? RelaxedBandFactor : SecondRelaxedBandFactor));
	}
	// The last level also accepts rougher ground (higher range and slope) for this request only.
	FSurvivalPOITypeRules RelaxedRules = *Rules;
	if (RelaxLevel >= 2)
	{
		const float TerrainFactor = bLastResort ? LandmarkLastTerrainFactor : SecondRelaxedTerrainFactor;
		RelaxedRules.MaxHeightRange *= TerrainFactor;
		RelaxedRules.MaxSlopeDegrees *= TerrainFactor;
		// A landmark cannot move to another sector: it shrinks a little instead; so does a battlefield (its pits and wrecks
		// are spread loosely). Hangars keep their size: shrunk, they lost their capture vehicles and scavengeables.
		if (Request.bIsLandmark or Request.Type == ESurvivalPOIType::SmallBattlefield)
		{
			RelaxedRules.SiteRadius *= bLastResort ? LandmarkLastShrinkFactor : LandmarkShrinkFactor;
		}
		Rules = &RelaxedRules;
	}
	const FSurvivalPOIStart& StartA = M_Context.Starts[Sector.StartA];
	const FSurvivalPOIStart& StartB = M_Context.Starts[Sector.StartB];
	TArray<TPair<float, int32>> Scored;
	// Why candidates in the window failed: distance band, exclusions, other sites, terrain, layout.
	int32 RejectCounts[5] = {0, 0, 0, 0, 0};
	int32 InWindow = 0;
	for (int32 Index = 0; Index < M_Candidates.Num(); ++Index)
	{
		const FCandidate& Candidate = M_Candidates[Index];
		if (Candidate.Sector != Sector.SectorIndex or Candidate.SectorFraction < Window.X or Candidate.SectorFraction > Window.Y)
		{
			continue;
		}
		// Landmarks belong to the nearer start; generic sites to the start whose side they are on.
		const bool bOwnedByA = Request.bIsLandmark
			                       ? FVector2D::Distance(StartA.Position, Candidate.Position)
			                       <= FVector2D::Distance(StartB.Position, Candidate.Position)
			                       : Request.bOwnedByStartA;
		const FVector2D Owner = bOwnedByA ? StartA.Position : StartB.Position;
		++InWindow;
		// A landmark sits between its two starts: within reach of both, so it counts for both when POIs near a start are compared.
		const double FartherStart = FMath::Max(FVector2D::Distance(StartA.Position, Candidate.Position),
		                                       FVector2D::Distance(StartB.Position, Candidate.Position));
		if (Request.bIsLandmark and RelaxLevel < 2 and FartherStart > LandmarkBothStartsReach)
		{
			++RejectCounts[0];
			continue;
		}
		const float Score = ScoreSite(Candidate, *Rules, Sector, Owner, Band, RejectCounts);
		if (Score >= 0.f)
		{
			Scored.Emplace(Score, Index);
		}
	}
	if (Scored.IsEmpty())
	{
		M_PlanNotes.Add(FString::Printf(TEXT("sector %d %s relax %d: %d candidates in window, rejected band %d, exclusions %d, other sites %d, terrain %d, layout %d"),
		                                Sector.SectorIndex, *SurvivalPOINames::GetTypeName(Request.Type), RelaxLevel, InWindow,
		                                RejectCounts[0], RejectCounts[1], RejectCounts[2], RejectCounts[3], RejectCounts[4]));
		return false;
	}
	// The seed chooses among all valid candidates, weighted by score (Efraimidis-Spirakis keys), not best-first: two seeds
	// then rarely share a site.
	TArray<TPair<double, int32>> Keyed;
	for (const TPair<float, int32>& Entry : Scored)
	{
		const double Weight = CandidateBaseWeight + FMath::Max(0.f, Entry.Key);
		Keyed.Emplace(FMath::Pow(FMath::Max(Random.FRand(), UE_KINDA_SMALL_NUMBER), 1.0 / Weight), Entry.Value);
	}
	Keyed.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key > B.Key; });
	TArray<int32> Order;
	for (const TPair<double, int32>& Entry : Keyed)
	{
		Order.Add(Entry.Value);
	}
	int32 NavChecks = 0;
	int32 AnchorTries = 0;
	// Landmarks and hangars are composed in full and must keep half of the scavengeables and capture targets they try; the
	// last resort asks for one (round 2: stricter rules there cost a landmark).
	const bool bNeedsComposition = GetHasHero(Request.Type) or Request.bIsLandmark or Request.Type == ESurvivalPOIType::SmallBattlefield;
	for (const int32 CandidateIndex : Order)
	{
		const FCandidate& Candidate = M_Candidates[CandidateIndex];
		if (not GetIsOpenGroundByTrace(Candidate.Position, Rules->SiteRadius))
		{
			continue;
		}
		// Every site must be drivable from the start it belongs to (heavy tanks; infantry reach whatever tanks reach).
		const bool bCandidateOwnedByA = Request.bIsLandmark
			                                ? FVector2D::Distance(StartA.Position, Candidate.Position)
			                                <= FVector2D::Distance(StartB.Position, Candidate.Position)
			                                : Request.bOwnedByStartA;
		const FSurvivalPOIStart& CandidateOwner = bCandidateOwnedByA ? StartA : StartB;
		// Anchor or reject, before the (dearer) drive test: the site stands only if its main object fits (the hangar, the
		// plane, the tower, the mast, enough convoy vehicles, hulls or pits); otherwise the next candidate is tried.
		if (bNeedsComposition and AnchorTries++ >= (bLastResort ? LandmarkLastNavChecks : MaxAnchorTriesPerRequest))
		{
			break;
		}
		FSurvivalPOISite Trial;
		Trial.Type = Request.Type;
		Trial.bIsLandmark = Request.bIsLandmark;
		Trial.Sector = Sector.SectorIndex;
		Trial.SiteIndex = M_Sites.Num();
		Trial.OwnerStart = CandidateOwner.DirectorIndex;
		Trial.Center = FVector(Candidate.Position, Candidate.Height);
		Trial.Radius = Rules->SiteRadius;
		Trial.Seed = HashCombine(GetTypeHash(M_MatchSeed), GetTypeHash(M_Sites.Num() * SiteSeedStride + Sector.SectorIndex));
		FRandomStream SiteRandom(Trial.Seed);
		Trial.SubLayout = SiteRandom.RandRange(0, 2);
		Trial.YawDegrees = FRotator::NormalizeAxis(GetSiteYaw(Trial, SiteRandom));
		const bool bAlongRoute = GetMayLieAlongRoute(Request.Type) and TryAlignToRoute(Trial, *Rules, Sector);
		if (bNeedsComposition and not TryFitSiteAnchor(Trial, GetNeedsKeyObjects(Trial), bLastResort, bAlongRoute))
		{
			continue;
		}
		if (NavChecks++ >= (bLastResort ? LandmarkLastNavChecks : MaxNavChecksPerSite))
		{
			break;
		}
		// From the gate foot nearest the candidate (outside any base), plus the straight run from the deploy centre to it.
		FVector Origin = CandidateOwner.NavOrigin;
		double OriginOffset = 0.0;
		double BestFoot = TNumericLimits<double>::Max();
		for (int32 FootIndex = 0; FootIndex < CandidateOwner.GateFeet.Num(); ++FootIndex)
		{
			// The gate foot nearest the candidate, plus the straight run from the deploy centre to it. (Round 2 tried the full
			// ramp-line drive instead; it chose worse sites by measured nav, so the straight run stays. GateFootDrives is logged.)
			const FVector2D& Foot = CandidateOwner.GateFeet[FootIndex];
			const double FootDrive = FVector2D::Distance(Foot, CandidateOwner.Position);
			const double Distance = FVector2D::Distance(Foot, Candidate.Position);
			float FootHeight = 0.f;
			if (Distance < BestFoot and M_HeightField.GetHeight(Foot, FootHeight))
			{
				BestFoot = Distance;
				Origin = FVector(Foot, FootHeight);
				OriginOffset = FootDrive;
			}
		}
		SurvivalPOINav::FChainedPathResult Path = SurvivalPOINav::FindChainedPath(
			M_World.Get(), Origin, Trial.Center, SurvivalPOINav::HeavyTankRadius, SurvivalPOINav::HeavyTankHeight);
		Path.Length += OriginOffset;
		if (not Path.bReached)
		{
			++M_UnreachableCandidates;
			continue;
		}
		const float NavFactor = RelaxLevel == 0 ? 1.f
			                        : (RelaxLevel == 1 ? RelaxedBandFactor : (bLastResort ? LandmarkLastBandFactor : SecondRelaxedBandFactor));
		if (Path.Length < Rules->OwnerNavDistance.X or Path.Length > Rules->OwnerNavDistance.Y * NavFactor)
		{
			++M_NavBandRejects;
			continue;
		}
		M_Sites.Add(Trial);
		if (bRelaxed)
		{
			M_PlanNotes.Add(FString::Printf(TEXT("sector %d: %s placed at relax level %d"), Sector.SectorIndex,
			                                *SurvivalPOINames::GetTypeName(Request.Type), RelaxLevel));
		}
		return true;
	}
	return false;
}

bool FSurvivalPOIPlanner::GetHoldsAnchor(const FSurvivalPOISite& Site, const TArray<FSurvivalPOIPlacedObject>& Objects)
{
	using namespace SurvivalPOIPlannerConstants;
	int32 Heroes = 0;
	for (const FSurvivalPOIPlacedObject& Object : Objects)
	{
		Heroes += Object.Role == ESurvivalPOIRole::Hero ? 1 : 0;
	}
	switch (Site.Type)
	{
	case ESurvivalPOIType::RuinedHangar:
	case ESurvivalPOIType::CrashSite:
	case ESurvivalPOIType::PumpStation:
	case ESurvivalPOIType::RelayStation:
		return Heroes > 0;
	default:
		// Battlefield, convoy and graveyard have no single centrepiece; the round 2 count rules (pits, vehicles, hulls)
		// rejected too many sites and cost a landmark, so they are not used.
		return true;
	}
}

bool FSurvivalPOIPlanner::GetHasHero(const ESurvivalPOIType Type)
{
	return Type == ESurvivalPOIType::RuinedHangar or Type == ESurvivalPOIType::CrashSite or Type == ESurvivalPOIType::PumpStation
		or Type == ESurvivalPOIType::RelayStation;
}

bool FSurvivalPOIPlanner::GetMayLieAlongRoute(const ESurvivalPOIType Type)
{
	return Type == ESurvivalPOIType::DeadConvoy;
}

bool FSurvivalPOIPlanner::GetNeedsKeyObjects(const FSurvivalPOISite& Site)
{
	return Site.bIsLandmark or Site.Type == ESurvivalPOIType::RuinedHangar or Site.Type == ESurvivalPOIType::SmallBattlefield;
}

bool FSurvivalPOIPlanner::GetKeepsRewards(const FSurvivalPOISite& Site, const FSurvivalPOIKeyObjectCount& KeyObjects, const bool bLastResort)
{
	if (Site.Type == ESurvivalPOIType::SmallBattlefield)
	{
		// The battlefield is its start's capture target when the hangar found no ground: it must keep a gun pit.
		return KeyObjects.CapturePlaced > 0;
	}
	return bLastResort ? KeyObjects.GetAnyPlaced() : KeyObjects.GetEnoughPlaced();
}

bool FSurvivalPOIPlanner::GetNearestRoutePoint(const FVector2D& Position, FVector2D& OutPoint, FVector2D& OutTangent,
                                               double& OutDistance) const
{
	OutDistance = TNumericLimits<double>::Max();
	for (const FSurvivalPOILayoutLine& Route : M_Settings->Placement.Layout.Routes)
	{
		for (int32 Index = 1; Index < Route.Points.Num(); ++Index)
		{
			const FVector2D& From = Route.Points[Index - 1];
			const FVector2D& To = Route.Points[Index];
			const FVector2D Closest = FMath::ClosestPointOnSegment2D(Position, From, To);
			const double Distance = FVector2D::Distance(Closest, Position);
			if (Distance < OutDistance and not (To - From).IsNearlyZero())
			{
				OutDistance = Distance;
				OutPoint = Closest;
				OutTangent = (To - From).GetSafeNormal();
			}
		}
	}
	return OutDistance < TNumericLimits<double>::Max();
}

bool FSurvivalPOIPlanner::TryAlignToRoute(FSurvivalPOISite& Site, const FSurvivalPOITypeRules& Rules,
                                          const FSurvivalPOISector& Sector) const
{
	using namespace SurvivalPOIPlannerConstants;
	const FVector2D Center(Site.Center);
	FVector2D RoutePoint = FVector2D::ZeroVector;
	FVector2D Tangent = FVector2D::ZeroVector;
	double Distance = 0.0;
	if (not GetNearestRoutePoint(Center, RoutePoint, Tangent, Distance) or Distance > Site.Radius)
	{
		return false;
	}
	// The column stays on the candidate's side of the route, parallel to it, outside the route's free width.
	FVector2D Normal(-Tangent.Y, Tangent.X);
	if (FVector2D::DotProduct(Center - RoutePoint, Normal) < 0.0)
	{
		Normal = -Normal;
	}
	const FSurvivalPOILayoutExclusions& Layout = M_Settings->Placement.Layout;
	const FVector2D Aligned = RoutePoint + Normal * (Layout.RouteHalfWidth + ConvoyColumnVergeOffset);
	float Height = 0.f;
	float HeightRange = 0.f;
	if (not M_HeightField.GetHeight(Aligned, Height) or not GetIsClearOfExclusions(Aligned, Site.Radius, false)
		or not GetIsClearOfLayout(Aligned, Site.Radius, true) or not GetIsTerrainFit(Aligned, Rules, Sector, HeightRange)
		or not GetIsOpenGroundByTrace(Aligned, Site.Radius))
	{
		return false;
	}
	const float MinGap = M_Settings->Placement.MinGapBetweenSites;
	for (const FSurvivalPOISite& Other : M_Sites)
	{
		if (FVector2D::Distance(FVector2D(Other.Center), Aligned) < Other.Radius + Site.Radius + MinGap)
		{
			return false;
		}
	}
	// Of the route's two directions, the one nearer the planned yaw.
	const float RouteYaw = FMath::RadiansToDegrees(FMath::Atan2(Tangent.Y, Tangent.X));
	const bool bForward = FMath::Abs(FRotator::NormalizeAxis(Site.YawDegrees - RouteYaw)) <= 90.f;
	Site.YawDegrees = FRotator::NormalizeAxis(bForward ? RouteYaw : RouteYaw + 180.f);
	Site.Center = FVector(Aligned, Height);
	return true;
}

bool FSurvivalPOIPlanner::TryFitSiteAnchor(FSurvivalPOISite& Site, const bool bRequireKeyObjects, const bool bLastResort,
                                           const bool bAlongRoute)
{
	using namespace SurvivalPOIPlannerConstants;
	const float PlannedYaw = Site.YawDegrees;
	TArray<FSurvivalPOIPlacedObject> Trial;
	bool bHeldAnchor = false;
	// The hangar's door faces its owner: it never turns round.
	const bool bFlippable = Site.Type != ESurvivalPOIType::RuinedHangar;
	const TArrayView<const float> Offsets = bAlongRoute ? MakeArrayView(RouteYawOffsets)
		                                        : (bFlippable ? MakeArrayView(FlippableYawOffsets) : MakeArrayView(HeroYawOffsets));
	for (const float Offset : Offsets)
	{
		Site.YawDegrees = FRotator::NormalizeAxis(PlannedYaw + Offset);
		if (GetHasHero(Site.Type))
		{
			Trial.Reset();
			ComposeSite(Site, Trial, true);
			if (not GetHoldsAnchor(Site, Trial))
			{
				continue;
			}
		}
		bHeldAnchor = true;
		if (not bRequireKeyObjects)
		{
			return true;
		}
		// The whole composition, exactly as the match lays it out (same seed, same order of draws and occupancy).
		Trial.Reset();
		FSurvivalPOIKeyObjectCount KeyObjects;
		ComposeSite(Site, Trial, false, &KeyObjects);
		if (GetKeepsRewards(Site, KeyObjects, bLastResort))
		{
			return true;
		}
	}
	Site.YawDegrees = PlannedYaw;
	if (bHeldAnchor)
	{
		++M_KeyObjectRejects;
	}
	else
	{
		++M_AnchorRejects;
	}
	return false;
}

float FSurvivalPOIPlanner::GetSiteYaw(const FSurvivalPOISite& Site, FRandomStream& Random) const
{
	const FVector2D Outward = (FVector2D(Site.Center) - M_Context.BasinCenter).GetSafeNormal();
	const float OutwardYaw = FMath::RadiansToDegrees(FMath::Atan2(Outward.Y, Outward.X));
	FVector2D ToOwner = FVector2D::ZeroVector;
	for (const FSurvivalPOIStart& Start : M_Context.Starts)
	{
		if (Start.DirectorIndex == Site.OwnerStart)
		{
			ToOwner = (Start.Position - FVector2D(Site.Center)).GetSafeNormal();
		}
	}
	const float OwnerYaw = FMath::RadiansToDegrees(FMath::Atan2(ToOwner.Y, ToOwner.X));
	switch (Site.Type)
	{
	case ESurvivalPOIType::SmallBattlefield:
		// The attack came in from the frontier: +X points inward, toward the defenders' line.
		return OutwardYaw + 180.f + Random.FRandRange(-40.f, 40.f);
	case ESurvivalPOIType::RuinedHangar:
		// The door (and the vehicles' way out) faces the start that owns the hangar.
		return OwnerYaw + Random.FRandRange(-35.f, 35.f);
	case ESurvivalPOIType::CrashSite:
		return OutwardYaw + (Random.FRand() < 0.5f ? 90.f : -90.f) + Random.FRandRange(-50.f, 50.f);
	case ESurvivalPOIType::DeadConvoy:
		// Along the basin, the way a supply column runs between two benches.
		return OutwardYaw + (Random.FRand() < 0.5f ? 90.f : -90.f) + Random.FRandRange(-25.f, 25.f);
	default:
		return OutwardYaw + 180.f + Random.FRandRange(-60.f, 60.f);
	}
}

// ---------------------------------------------------------------------------
// Wildlife
// ---------------------------------------------------------------------------

void FSurvivalPOIPlanner::PlanWildlife(const TArray<FSurvivalPOISite>& Sites, TArray<FSurvivalWildlifeGroup>& OutGroups) const
{
	for (int32 EntryIndex = 0; EntryIndex < M_Settings->Wildlife.Num(); ++EntryIndex)
	{
		const FSurvivalWildlifeEntry& Entry = M_Settings->Wildlife[EntryIndex];
		// Seeded by the entry index: FName hashes differ between processes.
		FRandomStream Random(HashCombine(GetTypeHash(M_MatchSeed), GetTypeHash(EntryIndex * SurvivalPOIPlannerConstants::WildlifeSeedStride + 1)));
		switch (Entry.Anchor)
		{
		case ESurvivalWildlifeAnchor::WaterBank:
			PlanWaterBankGroups(Entry, EntryIndex, Sites, Random, OutGroups);
			break;
		case ESurvivalWildlifeAnchor::OverPointOfInterest:
			PlanFlocksOverSites(Entry, EntryIndex, Sites, Random, OutGroups);
			break;
		case ESurvivalWildlifeAnchor::OverSector:
			PlanFlocksOverSectors(Entry, EntryIndex, Random, OutGroups);
			break;
		default:
			break;
		}
	}
}

bool FSurvivalPOIPlanner::GetIsAnimalSpotValid(const FSurvivalWildlifeEntry& Entry, const FVector2D& Position,
                                               const TArray<FSurvivalPOISite>& Sites) const
{
	using namespace SurvivalPOIPlannerConstants;
	const float Reach = Entry.WanderRadius + Entry.FleeDistance + Entry.HerdSpread;
	if (GetDistanceToNearestStart(Position) < Entry.MinDistanceFromStart + Reach)
	{
		return false;
	}
	for (const FBox2D& Box : M_Context.SovietBaseBoxes)
	{
		if (Box.ExpandBy(M_Settings->Placement.SovietBaseClearance + Reach).IsInside(Position))
		{
			return false;
		}
	}
	for (const FSurvivalPOISite& Site : Sites)
	{
		if (FVector2D::Distance(FVector2D(Site.Center), Position) < Site.Radius + AnimalClearanceFromSites)
		{
			return false;
		}
	}
	FSurvivalGroundHit Hit;
	if (not M_Projector.TryProject(Position, Hit)
		or FSurvivalGroundProjector::GetSlopeDegrees(Hit.Normal) > Entry.MaxGroundSlopeDegrees)
	{
		return false;
	}
	// The whole patch the herd wanders over stays on gentle ground (no scarp foot inside it).
	constexpr int32 WanderRingDirections = 12;
	for (int32 Ring = 0; Ring <= 2; ++Ring)
	{
		const float RingRadius = (Entry.WanderRadius + Entry.HerdSpread) * Ring * 0.5f;
		for (int32 Direction = 0; Direction < (Ring == 0 ? 1 : WanderRingDirections); ++Direction)
		{
			const float Angle = UE_TWO_PI * Direction / WanderRingDirections;
			const FVector2D Sample = Position + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius;
			if (M_HeightField.GetSlopeDegrees(Sample) > Entry.MaxGroundSlopeDegrees or GetIsNearWater(Sample, 0.f))
			{
				return false;
			}
		}
	}
	return GetIsOpenGroundByTrace(Position, Entry.HerdSpread);
}

void FSurvivalPOIPlanner::PlanWaterBankGroups(const FSurvivalWildlifeEntry& Entry, const int32 EntryIndex,
                                              const TArray<FSurvivalPOISite>& Sites, FRandomStream& Random,
                                              TArray<FSurvivalWildlifeGroup>& OutGroups) const
{
	using namespace SurvivalPOIPlannerConstants;
	for (const FSurvivalPOIExclusionZone& Zone : M_Settings->Placement.ExclusionZones)
	{
		if (not Zone.bIsWater)
		{
			continue;
		}
		const float StartAngle = Random.FRandRange(0.f, UE_TWO_PI);
		for (int32 GroupIndex = 0; GroupIndex < Entry.GroupCount; ++GroupIndex)
		{
			const float BaseAngle = StartAngle + UE_TWO_PI * GroupIndex / FMath::Max(1, Entry.GroupCount);
			for (int32 Attempt = 0; Attempt < AnimalSpotAttempts; ++Attempt)
			{
				const float Angle = BaseAngle + Random.FRandRange(-0.45f, 0.45f);
				const float Distance = Zone.Radius + Random.FRandRange(Entry.WaterBankDistance.X, Entry.WaterBankDistance.Y);
				const FVector2D Position = Zone.Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Distance;
				const bool bIsFlock = Entry.Kind != ESurvivalWildlifeKind::GroundHerd;
				if (not bIsFlock and not GetIsAnimalSpotValid(Entry, Position, Sites))
				{
					continue;
				}
				float Height = 0.f;
				M_Projector.TryGetHeight(Position, Height);
				FSurvivalWildlifeGroup& Group = OutGroups.AddDefaulted_GetRef();
				Group.EntryIndex = EntryIndex;
				Group.Center = FVector(Position, Height);
				Group.Count = Random.RandRange(Entry.GroupSize.X, Entry.GroupSize.Y);
				Group.YawDegrees = Random.FRandRange(0.f, 360.f);
				Group.Seed = Random.RandHelper(TNumericLimits<int32>::Max());
				if (bIsFlock)
				{
					FillFlockShape(Entry, Random, Group);
				}
				break;
			}
		}
	}
}

void FSurvivalPOIPlanner::FillFlockShape(const FSurvivalWildlifeEntry& Entry, FRandomStream& Random,
                                         FSurvivalWildlifeGroup& Group) const
{
	Group.FlightHeight = Random.FRandRange(Entry.FlightHeight.X, Entry.FlightHeight.Y);
	Group.SemiAxes = FVector2D(Random.FRandRange(Entry.SemiAxisX.X, Entry.SemiAxisX.Y),
	                           Random.FRandRange(Entry.SemiAxisY.X, Entry.SemiAxisY.Y));
}

void FSurvivalPOIPlanner::PlanFlocksOverSites(const FSurvivalWildlifeEntry& Entry, const int32 EntryIndex,
                                              const TArray<FSurvivalPOISite>& Sites, FRandomStream& Random,
                                              TArray<FSurvivalWildlifeGroup>& OutGroups) const
{
	using namespace SurvivalPOIPlannerConstants;
	int32 Added = 0;
	for (const FSurvivalPOISite& Site : Sites)
	{
		if (Added >= Entry.GroupCount or not Entry.OverPointOfInterestTypes.Contains(Site.Type))
		{
			continue;
		}
		if (Random.FRand() > Entry.ChancePerPointOfInterest)
		{
			continue;
		}
		FSurvivalWildlifeGroup& Group = OutGroups.AddDefaulted_GetRef();
		Group.EntryIndex = EntryIndex;
		Group.SiteIndex = Site.SiteIndex;
		const FVector2D Offset = FVector2D(Random.FRandRange(-1.f, 1.f), Random.FRandRange(-1.f, 1.f)) * FlockOffsetFromSite;
		Group.Center = Site.Center + FVector(Offset, 0.0);
		Group.Count = Random.RandRange(Entry.GroupSize.X, Entry.GroupSize.Y);
		Group.YawDegrees = Random.FRandRange(0.f, 360.f);
		Group.Seed = Random.RandHelper(TNumericLimits<int32>::Max());
		FillFlockShape(Entry, Random, Group);
		++Added;
	}
}

void FSurvivalPOIPlanner::PlanFlocksOverSectors(const FSurvivalWildlifeEntry& Entry, const int32 EntryIndex,
                                                FRandomStream& Random, TArray<FSurvivalWildlifeGroup>& OutGroups) const
{
	using namespace SurvivalPOIPlannerConstants;
	TArray<int32> SectorOrder;
	for (const FSurvivalPOISector& Sector : M_Sectors)
	{
		SectorOrder.Add(Sector.SectorIndex);
	}
	for (int32 Index = SectorOrder.Num() - 1; Index > 0; --Index)
	{
		SectorOrder.Swap(Index, Random.RandRange(0, Index));
	}
	float MeanStartDistance = 0.f;
	for (const FSurvivalPOIStart& Start : M_Context.Starts)
	{
		MeanStartDistance += FVector2D::Distance(Start.Position, M_Context.BasinCenter) / FMath::Max(1, M_Context.Starts.Num());
	}
	for (int32 Index = 0; Index < FMath::Min(Entry.GroupCount, SectorOrder.Num()); ++Index)
	{
		const FSurvivalPOISector& Sector = M_Sectors[SectorOrder[Index]];
		const double Angle = 0.5 * (Sector.AngleA + Sector.AngleB);
		const FVector2D Position = M_Context.BasinCenter
			+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * MeanStartDistance * SectorFlockDistanceFactor;
		float Height = 0.f;
		M_Projector.TryGetHeight(Position, Height);
		FSurvivalWildlifeGroup& Group = OutGroups.AddDefaulted_GetRef();
		Group.EntryIndex = EntryIndex;
		Group.Center = FVector(Position, Height);
		Group.Count = 1;
		Group.YawDegrees = Random.FRandRange(0.f, 360.f);
		Group.Seed = Random.RandHelper(TNumericLimits<int32>::Max());
		FillFlockShape(Entry, Random, Group);
	}
}
