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
	constexpr float MissionOpeningStrength = 0.45f;
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

	bShowMissionLandmarks = true;
	// Offsets come from each model's hand-over (Saved/WorldMapUIDesigns/MissionLandmarks/<Mission>/model).
	const auto AddLandmark = [this](const EMapMission Mission, const TCHAR* FolderAndMesh, const float Behind,
	                                const float Right)
	{
		FWorldMissionLandmark Landmark;
		Landmark.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FString::Printf(
			TEXT("/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Landmarks/%s"), FolderAndMesh)));
		Landmark.OffsetBehind = Behind;
		Landmark.OffsetRight = Right;
		MissionLandmarks.Add(Mission, Landmark);
	};
	AddLandmark(EMapMission::OreMountains, TEXT("OreMountains/SM_WorldLandmark_OreMountains.SM_WorldLandmark_OreMountains"), 60.f, 195.f);
	AddLandmark(EMapMission::Berlin, TEXT("Berlin/SM_WorldLandmark_Berlin.SM_WorldLandmark_Berlin"), 100.f, 150.f);
	AddLandmark(EMapMission::OderWarthe, TEXT("OderWarthe/SM_WorldLandmark_OderWarthe.SM_WorldLandmark_OderWarthe"), 46.f, 178.f);
	AddLandmark(EMapMission::ME410UnlockMission, TEXT("Pripyat/SM_WorldLandmark_Pripyat.SM_WorldLandmark_Pripyat"), 48.f, 212.f);
	AddLandmark(EMapMission::FrozenLake, TEXT("FrozenLake/SM_WorldLandmark_FrozenLake.SM_WorldLandmark_FrozenLake"), 96.f, 172.f);
	AddLandmark(EMapMission::Maus_E100_AssaultMission_Moscow, TEXT("Moscow/SM_WorldLandmark_Moscow.SM_WorldLandmark_Moscow"), 60.f, 150.f);
	AddLandmark(EMapMission::KingTigerMission_SovietWall, TEXT("SvarogLine/SM_WorldLandmark_SvarogLine.SM_WorldLandmark_SvarogLine"), 70.f, 215.f);
	AddLandmark(EMapMission::UralShieldFreezeEngine, TEXT("FreezeEngine/SM_WorldLandmark_FreezeEngine.SM_WorldLandmark_FreezeEngine"), 60.f, 170.f);

	bShowUnknownMissionMarkers = true;
	UnknownMissionLook = EWorldUnknownMissionLook::RealPiece;
	MissionOpeningRadius = Defaults::MissionOpeningRadius;
	MissionOpeningFalloff = Defaults::MissionOpeningFalloff;
	MissionOpeningStrength = Defaults::MissionOpeningStrength;
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
