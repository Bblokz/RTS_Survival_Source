// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * @brief Draws one group of units on the minimap the way survival waves are shown: a chevron along the group's heading
 * plus an upright badge, kept inside the minimap edge and moved to the centroid of the group's living units.
 * Used by USurvivalWaveMarkerTracker and by UMissionBase::TrackUnitGroupOnMiniMap; the caller owns the marker state
 * (icon ID, location, heading) and calls UpdateFromUnits on its own timer.
 */
namespace RTSUnitGroupMiniMapMarker
{
	// How often the owners refresh their markers.
	constexpr float UpdateIntervalSeconds = 0.25f;

	/**
	 * @brief Adds the chevron (IconId) and the badge (GetBadgeIconId) at the location, the chevron turned to the heading.
	 * @param WorldContext Any object in the world whose fog manager draws the minimap.
	 * @param IconId Base ID of the two icons.
	 * @param Location World location of the group.
	 * @param HeadingYaw World yaw the chevron points along.
	 */
	void AddIcons(const UObject* WorldContext, const FName IconId, const FVector& Location, const float HeadingYaw);

	/**
	 * @brief Moves both icons of a marker added with AddIcons.
	 * @param WorldContext Any object in the world whose fog manager draws the minimap.
	 * @param IconId Base ID of the two icons.
	 * @param Location New world location of the group.
	 * @param HeadingYaw New world yaw of the chevron.
	 */
	void MoveIcons(const UObject* WorldContext, const FName IconId, const FVector& Location, const float HeadingYaw);

	void RemoveIcons(const UObject* WorldContext, const FName IconId);

	FName GetBadgeIconId(const FName IconId);

	/**
	 * @brief Drops units that are gone, moves the location to the centroid of the rest and turns the heading towards
	 * the group's progress (measured only once it moved, so units standing in combat do not spin the chevron).
	 * @param Units The group's units; dead or destroyed ones are removed.
	 * @param Location Set to the centroid of the living units.
	 * @param HeadingAnchor Where the heading was last measured from; updated with it.
	 * @param HeadingYaw Smoothed world yaw of the group's movement.
	 * @return False when no unit is left (the marker should go).
	 */
	bool UpdateFromUnits(TArray<TWeakObjectPtr<AActor>>& Units, FVector& Location, FVector& HeadingAnchor,
	                     float& HeadingYaw);
}
