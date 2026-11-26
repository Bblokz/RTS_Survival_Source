// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignGeneration/Enums/Enum_MapMission.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowTypes.h"
#include "WorldFogHintSettings.generated.h"

class UMaterialInterface;
class UStaticMesh;

/**
 * @brief Where the tall landmark of one mission stands. The map camera looks along +X, so "behind" is +X and
 * "right" is +Y from the mission piece's centre.
 */
USTRUCT(BlueprintType)
struct FWorldMissionLandmark
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landmarks")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landmarks")
	float OffsetBehind = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landmarks")
	float OffsetRight = 0.f;

	// The imported models have their front on +Y; 90 turns it to the map camera.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landmarks")
	float Yaw = 90.f;
};

/**
 * @brief Project settings page (Config/DefaultGame.ini) for what the campaign map still shows under the fog of war:
 * the road network, drawn dim above the fog, and a plain marker where a mission stands. Read by the connection
 * spline renderer, the mission objects and the map tags.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="World Campaign Fog Hints"))
class RTS_SURVIVAL_API UWorldFogHintSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UWorldFogHintSettings();

	static const UWorldFogHintSettings* Get()
	{
		return GetDefault<UWorldFogHintSettings>();
	}

	// --- Roads ---

	/** Draws every road the fog hides as a dim copy above the fog, so routes can be planned. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads")
	bool bShowRoadsUnderFog;

	/** Colour of a road under fog as a share of the colour of a visible road. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads", meta = (ClampMin = "0", ClampMax = "1"))
	float FogRoadBrightness;

	/** Alpha of a road under fog as a share of the alpha of a visible road; only works if the road material uses it. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads", meta = (ClampMin = "0", ClampMax = "1"))
	float FogRoadOpacity;

	/** Width of a road under fog as a share of the width of a visible road. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads", meta = (ClampMin = "0.05", ClampMax = "2"))
	float FogRoadWidthScale;

	/** Vector parameter of the road material that holds its colour. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads")
	FName RoadColorParameterName;

	/**
	 * A road under fog is cut in pieces of at most this length, in map units; each piece is hidden on its own where
	 * the fog is open, so the dim copy never lies over a road the player can see.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads", meta = (ClampMin = "50"))
	float FogRoadSegmentLength;

	/** Translucent sort priority of the dim roads; must be above the fog cloud's (0) so they draw over it. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads")
	int32 FogRoadSortPriority;

	/**
	 * Translucent sort priority of the real roads while roads are shown under fog; must be below the fog cloud's (0),
	 * so a real road never shines through the fog next to its dim copy.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Roads")
	int32 RoadBelowFogSortPriority;

	// --- Unknown mission markers ---

	/** Hints at a mission the fog hides; off leaves such a mission fully hidden. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions")
	bool bShowUnknownMissionMarkers;

	/**
	 * Real Piece: a small opening in the fog cloud shows the mission's own piece; the mission stays unidentified
	 * (no name, no click). Plain Marker: a grey puck above the fog. A mission whose opening would show a hidden
	 * enemy piece gets the plain marker either way.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions")
	EWorldUnknownMissionLook UnknownMissionLook;

	/** Fully open part of the opening around an unidentified mission, in map units; a piece is about 200 wide. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions", meta = (ClampMin = "0"))
	float MissionOpeningRadius;

	/** Soft edge of the opening, outside its radius. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions", meta = (ClampMin = "0"))
	float MissionOpeningFalloff;

	// How far the fog thins over an unidentified mission: 1 removes it, 0 leaves it closed. The owner wants such
	// a piece vaguely visible, not fully revealed, so the default is well under 1.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions",
		meta = (ClampMin = "0", ClampMax = "1"))
	float MissionOpeningStrength;

	/**
	 * Half the width of the widest enemy piece. A hidden enemy piece closer to a mission than the opening plus this
	 * would show in the opening, so that mission gets the plain marker.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions", meta = (ClampMin = "0"))
	float EnemyPieceHalfWidth;

	/** The same for every mission: the marker must not tell which mission it is. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions")
	FLinearColor UnknownMissionMarkerColor;

	/** Width of the marker in map units; a mission piece is about 200 wide. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions", meta = (ClampMin = "1"))
	float UnknownMissionMarkerDiameter;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions", meta = (ClampMin = "1"))
	float UnknownMissionMarkerHeight;

	/** The only text an unknown mission shows, on a small plate while the cursor is on its marker. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions")
	FText UnknownMissionHoverText;

	/** A round shape of 100 units wide and high around its pivot, such as the engine cylinder. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions")
	TSoftObjectPtr<UStaticMesh> UnknownMissionMarkerMesh;

	/** Needs a vector parameter Color; unset or missing leaves the mesh its own material. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Unknown Missions")
	TSoftObjectPtr<UMaterialInterface> UnknownMissionMarkerMaterial;

	// --- Landmarks ---

	/** Tall landmarks stand beside scripted missions and show above the fog while the mission is still unknown. */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Landmarks")
	bool bShowMissionLandmarks;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Landmarks")
	TMap<EMapMission, FWorldMissionLandmark> MissionLandmarks;

	// --- Placement ---

	/**
	 * The dim roads and the markers lie this far above the top of the fog cloud, in map units. They are drawn above
	 * the fog and not on the ground, so a hidden enemy piece can never cut its shape out of them.
	 */
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0"))
	float HeightAboveFog;
};
