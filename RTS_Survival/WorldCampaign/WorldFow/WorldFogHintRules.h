// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapItemType.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowTypes.h"

/**
 * @brief Pure rules for what the campaign map shows under the fog of war: dim roads and unknown mission markers.
 * No world access, so they are covered by automation tests (RTS.WorldCampaign.FogHints).
 */
namespace WorldFogHintRules
{
	/**
	 * @brief Whether a map item is drawn as the plain unknown marker instead of its own piece. Only a mission the fog
	 * fully hides is; enemy and other items never are, so scouting keeps its value.
	 * @param ItemType What stands on the anchor.
	 * @param FowState Fog state of the item.
	 * @param bIsFogOpenAtItem The fog mask is open where the item stands, so its piece can be seen as before.
	 * @param bIsClearedByPlayer A mission the player won shows no marker.
	 * @param bShowUnknownMissionMarkers The setting.
	 * @return true when the marker replaces the piece.
	 */
	RTS_SURVIVAL_API bool GetShowsUnknownMarker(EMapItemType ItemType, EWorldMapFowState FowState,
	                                            bool bIsFogOpenAtItem, bool bIsClearedByPlayer,
	                                            bool bShowUnknownMissionMarkers);

	/**
	 * @brief How a map item is drawn for one fog pass. An unidentified mission shows its real piece through an
	 * opening in the fog, unless the look asks for the plain marker or the opening would show a hidden enemy piece.
	 * @param ItemType What stands on the anchor.
	 * @param FowState Fog state of the item.
	 * @param bIsFogOpenAtItem The fog is open where the item stands by the real reveal rules.
	 * @param bIsClearedByPlayer A mission the player won.
	 * @param bShowUnknownMissionMarkers The setting that switches the hint off altogether.
	 * @param UnknownMissionLook The setting that picks the plain marker or the real piece.
	 * @param bIsHiddenEnemyPieceInsideOpening A hidden enemy object or division stands where the opening would show it.
	 * @return The presentation; AsFogState for everything that is not an unidentified mission.
	 */
	RTS_SURVIVAL_API EWorldMissionFogPresentation GetMissionFogPresentation(
		EMapItemType ItemType, EWorldMapFowState FowState, bool bIsFogOpenAtItem, bool bIsClearedByPlayer,
		bool bShowUnknownMissionMarkers, EWorldUnknownMissionLook UnknownMissionLook,
		bool bIsHiddenEnemyPieceInsideOpening);

	/**
	 * @brief How far from a mission a hidden enemy piece must stand for the opening not to show any of it.
	 * @param OpeningRadius Fully open part of the opening.
	 * @param OpeningFalloff Soft edge outside the radius.
	 * @param MaskPixelSize Size of one fog mask pixel in map units; the opening is drawn in whole pixels.
	 * @param EnemyPieceHalfWidth Half the width of the widest enemy piece.
	 * @return Distance on the map plane, measured between the two anchors.
	 */
	RTS_SURVIVAL_API float GetMissionOpeningExposureRadius(float OpeningRadius, float OpeningFalloff,
	                                                       float MaskPixelSize, float EnemyPieceHalfWidth);

	/**
	 * @brief Whether any of a set of pieces stands within a distance of a place, on the map plane.
	 * @param Center The place, such as a mission.
	 * @param PieceLocations Locations of the pieces, such as the hidden enemy pieces.
	 * @param Radius Distance on the map plane.
	 * @return true when at least one piece is inside or on the radius.
	 */
	RTS_SURVIVAL_API bool GetIsAnyPieceInsideRadius(const FVector& Center, TConstArrayView<FVector> PieceLocations,
	                                                float Radius);

	/**
	 * @brief Whether one piece of a road gets its dim copy above the fog.
	 * @param ConnectionState Fog state of the connection the road belongs to.
	 * @param bIsFogOpenAtSegment The fog mask is open where this piece of the road lies.
	 * @param bShowRoadsUnderFog The setting.
	 * @return true when the dim piece is drawn.
	 */
	RTS_SURVIVAL_API bool GetShowsRoadUnderFog(EWorldMapFowState ConnectionState, bool bIsFogOpenAtSegment,
	                                           bool bShowRoadsUnderFog);

	/**
	 * @brief Colour of a road under fog, from the colour of a visible road.
	 * @param RoadColor Colour parameter of the road material.
	 * @param Brightness Share of the colour that is kept, 0 to 1.
	 * @param Opacity Share of the alpha that is kept, 0 to 1.
	 * @return The dimmed colour.
	 */
	RTS_SURVIVAL_API FLinearColor GetFogRoadColor(const FLinearColor& RoadColor, float Brightness, float Opacity);

	/**
	 * @brief Lays a solved road path flat at one height and cuts it in pieces of at most a given length, so each
	 * piece can be shown or hidden on its own.
	 * @param RoadPath Solved path of the visible road (at least two points).
	 * @param PathHeight World Z every point of the result gets.
	 * @param MaxSegmentLength Longest piece in map units, measured on the map plane.
	 * @param OutFogRoadPath Receives the path; empty when RoadPath has fewer than two points.
	 */
	RTS_SURVIVAL_API void BuildFogRoadPath(const TArray<FVector>& RoadPath, float PathHeight, float MaxSegmentLength,
	                                       TArray<FVector>& OutFogRoadPath);

	/**
	 * @brief Scale that turns a round mesh of MeshSize wide and high into the marker puck.
	 * @param Diameter Width of the marker in map units.
	 * @param Height Height of the marker in map units.
	 * @param MeshSize Width and height of the mesh at scale 1.
	 * @return World scale of the marker.
	 */
	RTS_SURVIVAL_API FVector GetUnknownMarkerScale(float Diameter, float Height, float MeshSize);

	/**
	 * @brief World Z of the middle of the marker, for a mesh with its pivot in its middle.
	 * @param FogTopZ World Z of the top of the fog cloud.
	 * @param HeightAboveFog Room between the fog and the underside of the marker.
	 * @param MarkerHeight Height of the marker.
	 * @return World Z of the marker's pivot.
	 */
	RTS_SURVIVAL_API float GetUnknownMarkerCenterZ(float FogTopZ, float HeightAboveFog, float MarkerHeight);
}
