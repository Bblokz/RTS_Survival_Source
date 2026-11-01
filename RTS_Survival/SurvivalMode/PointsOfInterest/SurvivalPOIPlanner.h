// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/SurvivalMode/PointsOfInterest/SurvivalPOITypes.h"
#include "RTS_Survival/SurvivalMode/StartLocations/SurvivalStartPlacementHelpers.h"

class ASurvivalStartLocation;
class USurvivalPointsOfInterestSettings;
class UWorld;

namespace SurvivalPOIGeometry
{
	/** @return Distance from Point to the nearest segment of the polyline (huge when it has fewer than two points). */
	double DistanceToPolyline(const FVector2D& Point, const TArray<FVector2D>& Line);

	bool IsInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Outline);

	/** @return 0 inside the outline, else the distance to its nearest edge. */
	double DistanceToPolygon(const FVector2D& Point, const TArray<FVector2D>& Outline);
}

/** @brief A start as the planner sees it. */
struct FSurvivalPOIStart
{
	FVector2D Position = FVector2D::ZeroVector;
	float Height = 0.f;
	FString Name;
	// Index in the director's start list.
	int32 DirectorIndex = INDEX_NONE;
	double AngleRadians = 0.0;
	// Where paths from this start begin: the deploy centre (the same origin the fairness numbers use).
	FVector NavOrigin = FVector::ZeroVector;
	// The feet of this start's ramps (layout data): where its gates open onto the basin. Site selection measures drives
	// from here (plus the straight run from the deploy centre), because the Soviet base being built around an unpicked
	// start's deploy centre changes its paths while the sites are chosen.
	TArray<FVector2D> GateFeet;
	// Per gate foot: the drive from the deploy centre to it (to the ramp's top, then the ramp line's length).
	TArray<double> GateFootDrives;
};

/** @brief The wedge between two neighbouring starts (counter-clockwise around the basin centre). */
struct FSurvivalPOISector
{
	int32 SectorIndex = INDEX_NONE;
	// Indices into the planner's sorted start list.
	int32 StartA = INDEX_NONE;
	int32 StartB = INDEX_NONE;
	double AngleA = 0.0;
	// Always greater than AngleA (unwrapped).
	double AngleB = 0.0;
	ESurvivalPOIType Landmark = ESurvivalPOIType::None;
	bool bLandmarkBuilt = false;
	float BasinFloorHeight = 0.f;
};

/** @brief The scavengeables and capture targets a composition tried to place, and how many found ground. */
struct FSurvivalPOIKeyObjectCount
{
	int32 Attempted = 0;
	int32 Placed = 0;

	bool GetAllPlaced() const { return Placed >= Attempted; }
};

/** @brief The level actors the placement must keep away from, gathered once per match. */
struct FSurvivalPOIWorldContext
{
	FBox2D LandscapeBounds = FBox2D(ForceInit);
	FVector2D BasinCenter = FVector2D::ZeroVector;
	TArray<FSurvivalPOIStart> Starts;
	// Edge spawn zones as segments along the map edge.
	TArray<TPair<FVector2D, FVector2D>> SpawnZoneSegments;
	TArray<FBox2D> SovietBaseBoxes;
	// Each start's ReservedContentSpots (its ramps and gate aprons) in world XY; Z is the radius.
	TArray<FVector> ReservedSpots;

	/** @brief Reads landscapes, spawn zones and Soviet base volumes from the world; starts come from the caller. */
	void Gather(UWorld& World, const TArray<ASurvivalStartLocation*>& StartLocations);
};

/**
 * @brief Landscape heights on a regular grid, filled over several frames, so site tests read heights without touching
 * the landscape again.
 */
class FSurvivalPOIHeightField
{
public:
	void Init(const FBox2D& Bounds, const float Spacing);

	/**
	 * @brief Fills samples until MaxSamples are done or BudgetSeconds have passed.
	 * @return True when every sample is filled.
	 */
	bool Fill(const FSurvivalGroundProjector& Projector, const int32 MaxSamples, const double BudgetSeconds);

	bool GetIsFilled() const { return M_NextSample >= M_Heights.Num() and not M_Heights.IsEmpty(); }

	/** @return Bilinear height; false outside the grid or where the landscape has no height. */
	bool GetHeight(const FVector2D& Position, float& OutHeight) const;

	/** @return Steepest slope in degrees of the grid cell around Position (0 outside the grid). */
	float GetSlopeDegrees(const FVector2D& Position) const;

private:
	FBox2D M_Bounds = FBox2D(ForceInit);
	float M_Spacing = 750.f;
	int32 M_CountX = 0;
	int32 M_CountY = 0;
	int32 M_NextSample = 0;
	// Heights in cm; NaN where the landscape has none.
	TArray<float> M_Heights;

	float GetSample(const int32 X, const int32 Y) const;
};

/**
 * @brief Plans the points of interest and wildlife of one match from a seed: sectors from the starts, sites from the
 * real terrain, compositions per site and animal groups. Pure planning; the subsystem spawns what it returns.
 * The same seed on the same map gives the same plan, whichever start the player picked.
 */
class FSurvivalPOIPlanner
{
public:
	FSurvivalPOIPlanner(UWorld& World, const USurvivalPointsOfInterestSettings& Settings, const int32 MatchSeed,
	                    const TArray<ASurvivalStartLocation*>& StartLocations);

	/**
	 * @brief Advances the site search: height field slices, then the candidates, then one sector per step.
	 * @param MaxSamples Height samples a step may take.
	 * @param BudgetSeconds Time a height field slice may take.
	 * @return True when the sites are chosen.
	 */
	bool StepSiteSearch(const int32 MaxSamples, const double BudgetSeconds);

	/** @brief Restarts the site picks with another seed on the same height field (seed sweeps). */
	void ReplanSites(const int32 NewMatchSeed);

	const TArray<FSurvivalPOISite>& GetSites() const { return M_Sites; }
	const TArray<FSurvivalPOISector>& GetSectors() const { return M_Sectors; }
	const FSurvivalPOIWorldContext& GetContext() const { return M_Context; }
	const TArray<FString>& GetPlanNotes() const { return M_PlanNotes; }
	int32 GetMatchSeed() const { return M_MatchSeed; }
	// Candidates dropped because heavy tanks cannot drive there from their start.
	int32 GetUnreachableCandidates() const { return M_UnreachableCandidates; }
	// Reachable candidates dropped because their drive from the start was outside the type's nav band.
	int32 GetNavBandRejects() const { return M_NavBandRejects; }
	int32 GetAnchorRejects() const { return M_AnchorRejects; }
	// Candidates whose main object fit but whose composition lost a scavengeable or capture target.
	int32 GetKeyObjectRejects() const { return M_KeyObjectRejects; }

	/** @return Whether a composed site holds its main object (hero, enough vehicles, hulls or pits). */
	static bool GetHoldsAnchor(const FSurvivalPOISite& Site, const TArray<FSurvivalPOIPlacedObject>& Objects);
	/** @return Whether the type has one main object (hangar, plane, water tower, mast) that the site must hold. */
	static bool GetHasHero(ESurvivalPOIType Type);
	/** @return Whether a site of this type may lie along a route (the dead convoy) instead of keeping off routes. */
	static bool GetMayLieAlongRoute(ESurvivalPOIType Type);
	/** @return Whether a site must keep every scavengeable and capture target its composition tries (landmarks and hangars). */
	static bool GetNeedsKeyObjects(const FSurvivalPOISite& Site);

	/**
	 * @brief The point on the layout's routes nearest to Position.
	 * @param OutTangent Unit direction of the route there.
	 * @return False when the layout has no routes.
	 */
	bool GetNearestRoutePoint(const FVector2D& Position, FVector2D& OutPoint, FVector2D& OutTangent, double& OutDistance) const;
	bool GetIsHeightFieldFilled() const { return M_HeightField.GetIsFilled(); }

	/** @brief Lays out one site's composition in world space. */
	/** @param bAnchorOnly Place only the hero (a planning trial): the hero is each hero type's first object, so the trial is exact and cheap. */
	/** @param OutKeyObjects When set, counts the scavengeables and capture targets tried and placed. */
	void ComposeSite(const FSurvivalPOISite& Site, TArray<FSurvivalPOIPlacedObject>& OutObjects, bool bAnchorOnly = false,
	                 FSurvivalPOIKeyObjectCount* OutKeyObjects = nullptr) const;

	/** @brief Places the herds and flocks; flocks over POIs read the sites given. */
	void PlanWildlife(const TArray<FSurvivalPOISite>& Sites, TArray<FSurvivalWildlifeGroup>& OutGroups) const;

	/** @return True when an XY lies in a water zone's basin or within Margin of it. */
	bool GetIsNearWater(const FVector2D& Position, const float Margin) const;

private:
	struct FCandidate
	{
		FVector2D Position = FVector2D::ZeroVector;
		float Height = 0.f;
		int32 Sector = INDEX_NONE;
		// Angular position inside the sector, 0 at StartA, 1 at StartB.
		float SectorFraction = 0.f;
	};

	struct FSiteRequest
	{
		ESurvivalPOIType Type = ESurvivalPOIType::None;
		bool bIsLandmark = false;
		// Fraction window inside the sector and which of its two starts owns the site.
		FVector2D FractionWindow = FVector2D(0.0, 1.0);
		bool bOwnedByStartA = true;
	};

	TWeakObjectPtr<UWorld> M_World;
	const USurvivalPointsOfInterestSettings* M_Settings = nullptr;
	FSurvivalGroundProjector M_Projector;
	FSurvivalPOIWorldContext M_Context;
	FSurvivalPOIHeightField M_HeightField;
	int32 M_MatchSeed = 0;
	bool bM_SitesChosen = false;
	bool bM_CandidatesBuilt = false;
	// Sectors whose sites are chosen; the search does one sector per step.
	int32 M_SectorsDone = 0;
	// The sector being searched one request per step, with its own seeded stream (as ChooseSites uses).
	int32 M_CurrentSector = INDEX_NONE;
	FRandomStream M_SectorRandom;
	TArray<FSiteRequest> M_SectorRequests;
	int32 M_NextRequest = 0;
	int32 M_UnreachableCandidates = 0;
	int32 M_NavBandRejects = 0;
	// Candidates that passed every test but could not hold their main object at any tried yaw.
	int32 M_AnchorRejects = 0;
	int32 M_KeyObjectRejects = 0;

	/**
	 * @brief Composes the site at its planned yaw and a few offsets; keeps the first yaw whose composition holds its anchor
	 * and, when asked, places every scavengeable and capture target it tries (the full composition, as the match lays it out).
	 * @param bAlongRoute The site lies along a route: only the route's two directions are tried.
	 */
	bool TryFitSiteAnchor(FSurvivalPOISite& Site, bool bRequireKeyObjects, bool bAlongRoute);

	/**
	 * @brief Moves a convoy site beside the route nearest to it (its column parallel to the route, the drivable width free)
	 * when that route passes inside the site and the moved site still keeps every site rule.
	 * @return Whether the site was moved.
	 */
	bool TryAlignToRoute(FSurvivalPOISite& Site, const FSurvivalPOITypeRules& Rules, const FSurvivalPOISector& Sector) const;

	TArray<FSurvivalPOISector> M_Sectors;
	TArray<FCandidate> M_Candidates;
	TArray<FSurvivalPOISite> M_Sites;
	TArray<FString> M_PlanNotes;

	void BuildSectors();
	void BuildCandidates();
	void ChooseSites();
	void ChooseSitesForSector(const FSurvivalPOISector& Sector, FRandomStream& Random);
	void RunSiteRequest(const FSurvivalPOISector& Sector, const FSiteRequest& Request, FRandomStream& Random);
	TArray<FSiteRequest> GetSiteRequests(const FSurvivalPOISector& Sector) const;

	/**
	 * @brief Picks the site for one request among the candidates of its sector.
	 * @param RelaxLevel 0 strict; 1 ignores the fraction window and widens the distance band; 2 also accepts rougher ground and
	 * shrinks the site; 3 (last resort) widens the band and the ground rule further and asks only for the main object.
	 * @return Whether a site was added.
	 */
	bool TryAddSite(const FSurvivalPOISector& Sector, const FSiteRequest& Request, const int32 RelaxLevel, FRandomStream& Random);

	/** @return A score (higher is better) or a negative value when the site breaks a rule. */
	float ScoreSite(const FCandidate& Candidate, const FSurvivalPOITypeRules& Rules, const FSurvivalPOISector& Sector,
	                const FVector2D& OwnerPosition, const FVector2D& DistanceBand, int32* OutRejectCounts) const;
	/** @param bIncludeLayout Also test the layout's ramps, pads and basins (the expensive part). */
	bool GetIsClearOfExclusions(const FVector2D& Position, const float Radius, const bool bIncludeLayout = true) const;
	/** @param bAllowRoutes Skip the routes (a site that may lie along one). */
	bool GetIsClearOfLayout(const FVector2D& Position, const float Radius, const bool bAllowRoutes = false) const;
	bool GetIsTerrainFit(const FVector2D& Position, const FSurvivalPOITypeRules& Rules, const FSurvivalPOISector& Sector,
	                     float& OutHeightRange) const;
	bool GetIsOpenGroundByTrace(const FVector2D& Position, const float Radius) const;
	float GetSiteYaw(const FSurvivalPOISite& Site, FRandomStream& Random) const;
	float GetFractionInSector(const FVector2D& Position, const FSurvivalPOISector& Sector) const;
	int32 GetSectorOf(const FVector2D& Position) const;
	float GetDistanceToNearestStart(const FVector2D& Position) const;

	// --- Wildlife ---
	void PlanWaterBankGroups(const FSurvivalWildlifeEntry& Entry, const int32 EntryIndex, const TArray<FSurvivalPOISite>& Sites,
	                         FRandomStream& Random, TArray<FSurvivalWildlifeGroup>& OutGroups) const;
	void PlanFlocksOverSites(const FSurvivalWildlifeEntry& Entry, const int32 EntryIndex, const TArray<FSurvivalPOISite>& Sites,
	                         FRandomStream& Random, TArray<FSurvivalWildlifeGroup>& OutGroups) const;
	void PlanFlocksOverSectors(const FSurvivalWildlifeEntry& Entry, const int32 EntryIndex, FRandomStream& Random,
	                           TArray<FSurvivalWildlifeGroup>& OutGroups) const;
	bool GetIsAnimalSpotValid(const FSurvivalWildlifeEntry& Entry, const FVector2D& Position,
	                          const TArray<FSurvivalPOISite>& Sites) const;
	void FillFlockShape(const FSurvivalWildlifeEntry& Entry, FRandomStream& Random, FSurvivalWildlifeGroup& Group) const;

	friend class FSurvivalPOIComposer;
};
