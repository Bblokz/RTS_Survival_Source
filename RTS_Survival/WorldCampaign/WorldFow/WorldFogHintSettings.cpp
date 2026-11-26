// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintSettings.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace WorldFogHintSettingsDefaults
{
	// Clearly dimmer than a visible road, still readable on the fog.
	constexpr float FogRoadBrightness = 0.4f;
	constexpr float FogRoadOpacity = 0.85f;
	constexpr float FogRoadWidthScale = 0.6f;
	constexpr float FogRoadSegmentLength = 300.f;
	// The fog cloud draws at priority 0.
	constexpr int32 FogRoadSortPriority = 10;
	constexpr int32 RoadBelowFogSortPriority = -10;

	// A mission piece is about 200 units wide; the marker is a low puck of nearly that footprint.
	constexpr float UnknownMissionMarkerDiameter = 170.f;
	constexpr float UnknownMissionMarkerHeight = 22.f;
	const FLinearColor UnknownMissionMarkerColor(0.36f, 0.36f, 0.36f);
	const TCHAR* const UnknownMissionMarkerMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* const UnknownMissionMarkerMaterialPath = TEXT(
		"/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Story/Shared/M_WorldPiece_Flat.M_WorldPiece_Flat");

	constexpr float HeightAboveFog = 6.f;

	// A mission piece is about 200 units wide: fully open over the piece, with a short soft edge.
	constexpr float MissionOpeningRadius = 120.f;
	constexpr float MissionOpeningFalloff = 50.f;
	// Enemy pieces stand on a 180 unit slab; divisions are smaller.
	constexpr float EnemyPieceHalfWidth = 100.f;
}

UWorldFogHintSettings::UWorldFogHintSettings()
{
	namespace Defaults = WorldFogHintSettingsDefaults;
	bShowRoadsUnderFog = true;
	FogRoadBrightness = Defaults::FogRoadBrightness;
	FogRoadOpacity = Defaults::FogRoadOpacity;
	FogRoadWidthScale = Defaults::FogRoadWidthScale;
	RoadColorParameterName = TEXT("Color");
	FogRoadSegmentLength = Defaults::FogRoadSegmentLength;
	FogRoadSortPriority = Defaults::FogRoadSortPriority;
	RoadBelowFogSortPriority = Defaults::RoadBelowFogSortPriority;

	bShowUnknownMissionMarkers = true;
	UnknownMissionLook = EWorldUnknownMissionLook::RealPiece;
	MissionOpeningRadius = Defaults::MissionOpeningRadius;
	MissionOpeningFalloff = Defaults::MissionOpeningFalloff;
	EnemyPieceHalfWidth = Defaults::EnemyPieceHalfWidth;
	UnknownMissionMarkerColor = Defaults::UnknownMissionMarkerColor;
	UnknownMissionMarkerDiameter = Defaults::UnknownMissionMarkerDiameter;
	UnknownMissionMarkerHeight = Defaults::UnknownMissionMarkerHeight;
	UnknownMissionHoverText = NSLOCTEXT("WorldFogHints", "UnknownMissionHoverText", "Unidentified");
	UnknownMissionMarkerMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(Defaults::UnknownMissionMarkerMeshPath));
	UnknownMissionMarkerMaterial = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(Defaults::UnknownMissionMarkerMaterialPath));

	HeightAboveFog = Defaults::HeightAboveFog;
}
