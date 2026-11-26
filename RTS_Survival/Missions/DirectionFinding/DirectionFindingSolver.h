// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/** Bearing of one station towards the command post; opened the first time the player holds the station. */
struct FDirectionFindingWedge
{
	bool bOpen = false;
	bool bHeld = false;
	// Bearing (degrees, world XY, atan2 of Y over X) from the station to the command post.
	float TrueBearingDegrees = 0.f;
	float HalfAngleDegrees = 0.f;
	// Half-angle before the last narrowing; the map animates from it.
	float PreviousHalfAngleDegrees = 0.f;
	// Signed share of the half-angle the wedge centre is off the true bearing.
	float ErrorFraction = 0.f;
	// World seconds of the last narrowing; negative before the first.
	double NarrowedAtSeconds = -1.0;

	float GetCentreBearingDegrees(const float HalfAngle) const { return TrueBearingDegrees + ErrorFraction * HalfAngle; }
};

/**
 * @brief The direction-finding rules without any world: ADirectionFindingManager feeds it captures, bursts and
 * eliminated groups, and the map test runs it for every command post and station set. Positions are world XY (cm).
 */
class RTS_SURVIVAL_API FDirectionFindingSolver
{
public:
	/**
	 * @brief Starts a hunt; every wedge closed.
	 * @param InStationLocations The stations, in DF order.
	 * @param InWerkCentres Centre of each bunker group.
	 * @param InWerkRadii Radius of each group; a group touching a wedge counts as inside it.
	 * @param InCommandPostIndex The group that transmits.
	 */
	void Init(const TArray<FVector2D>& InStationLocations, const TArray<FVector2D>& InWerkCentres,
	          const TArray<float>& InWerkRadii, const int32 InCommandPostIndex);

	/**
	 * @brief The player took or lost a station. Taking it the first time opens its wedge; a lost wedge stays open,
	 * stops narrowing, and continues from the same width when retaken.
	 * @param StationIndex DF order index.
	 * @param bHeld True when the player holds it now.
	 * @param StartHalfAngle Half-angle of a newly opened wedge.
	 * @param ErrorFraction Signed centre error of a newly opened wedge (share of the half-angle).
	 * @return True when this opened the wedge.
	 */
	bool SetStationHeld(const int32 StationIndex, const bool bHeld, const float StartHalfAngle,
	                    const float ErrorFraction);

	/**
	 * @brief Every held wedge hears a burst and narrows.
	 * @param Factor Half-angle multiplier.
	 * @param MinHalfAngle Floor of the half-angle.
	 * @param NowSeconds World time, for the map animation.
	 * @return Number of wedges that heard it (held wedges).
	 */
	int32 ApplyBurst(const float Factor, const float MinHalfAngle, const double NowSeconds);

	/** A destroyed wrong group drops out of the search. The command post cannot be eliminated this way. */
	bool SetWerkEliminated(const int32 WerkIndex);

	/** @return The standing groups inside every held wedge; empty while no wedge is held. */
	TArray<int32> GetCandidateWerke() const;

	int32 GetHeldWedgeCount() const;
	bool GetIsWerkEliminated(const int32 WerkIndex) const
	{
		return M_WerkEliminated.IsValidIndex(WerkIndex) && M_WerkEliminated[WerkIndex];
	}
	int32 GetCommandPostIndex() const { return M_CommandPostIndex; }
	int32 GetStationCount() const { return M_StationLocations.Num(); }
	const FDirectionFindingWedge* GetWedge(const int32 StationIndex) const;

	/** @return True when at least StationsNeeded wedges are held and their overlap holds one standing group only. */
	bool GetIsFix(const int32 StationsNeeded) const;

	/** @return True when the player holds every station. */
	bool GetAreAllStationsHeld() const;

	/**
	 * @brief Half-angle as the map shows it: easing from the previous width after a narrowing.
	 * @param Wedge The wedge.
	 * @param NowSeconds World time.
	 * @param AnimationSeconds Length of the narrowing animation.
	 */
	static float GetShownHalfAngle(const FDirectionFindingWedge& Wedge, const double NowSeconds,
	                               const float AnimationSeconds);

	/**
	 * @brief The wedge as a convex polygon clipped to the map square, counter-clockwise in world XY.
	 * @param Apex The station.
	 * @param CentreBearingDegrees Wedge centre line.
	 * @param HalfAngleDegrees Wedge half-angle.
	 * @param MapBounds The map square.
	 * @param OutPolygon Empty when the wedge misses the map.
	 */
	static void BuildWedgePolygon(const FVector2D& Apex, const float CentreBearingDegrees,
	                              const float HalfAngleDegrees, const FBox2D& MapBounds,
	                              TArray<FVector2D>& OutPolygon);

	/**
	 * @brief Sutherland-Hodgman: cuts Subject down to its part inside the convex Clip polygon. Both counter-clockwise.
	 * @param Subject Polygon to cut; replaced by the result (empty when they do not meet).
	 * @param Clip Convex clip polygon.
	 */
	static void ClipConvexPolygon(TArray<FVector2D>& Subject, const TArray<FVector2D>& Clip);

	/** @return Bearing in degrees from From to To (atan2 of Y over X). */
	static float GetBearingDegrees(const FVector2D& From, const FVector2D& To);

	/** @return True when a circle around Point touches the wedge of the given centre line and half-angle. */
	static bool GetIsCircleInWedge(const FVector2D& Apex, const float CentreBearingDegrees,
	                               const float HalfAngleDegrees, const FVector2D& Point, const float Radius);

	FVector2D GetStationLocation(const int32 StationIndex) const;

private:
	TArray<FVector2D> M_StationLocations;
	TArray<FVector2D> M_WerkCentres;
	TArray<float> M_WerkRadii;
	TArray<bool> M_WerkEliminated;
	TArray<FDirectionFindingWedge> M_Wedges;
	int32 M_CommandPostIndex = INDEX_NONE;

	bool GetIsWerkInsideEveryHeldWedge(const int32 WerkIndex) const;
};
