// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "RTS_Survival/Environment/GroundSurface/GroundSurfaceTypes.h"
#include "RTSGroundSurfaceSettings.generated.h"

class UStaticMesh;

/** @brief One kind of ground: which landscape paint layers bake into it and how track smoke looks on it. */
USTRUCT()
struct FGroundSurfaceDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Surface")
	EGroundSurfaceType SurfaceType = EGroundSurfaceType::Dirt;

	// Landscape paint layers that count as this surface when a surface map is baked.
	UPROPERTY(EditAnywhere, Category="Surface")
	TArray<FName> LayerNames;

	UPROPERTY(EditAnywhere, Category="Surface")
	FGroundSurfaceSmokeLook SmokeLook;
};

/**
 * @brief Meshes that lie on the landscape and decide the surface where they are, such as roads and paths.
 * A component matches when it belongs to an actor of ActorClass (if set) and shows Mesh (if set).
 */
USTRUCT()
struct FGroundSurfaceMeshRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Surface")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category="Surface")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, Category="Surface")
	EGroundSurfaceType SurfaceType = EGroundSurfaceType::Concrete;
};

/**
 * @brief Which ground is where on each map (baked into SurfaceMap_<MapName> with RTS.GroundSurface.BakeSurfaceMap)
 * and how track smoke looks on each kind of ground. Edit in Project Settings -> Game -> RTS Ground Surfaces.
 * Actors tagged GroundSurface_<Type> (for example GroundSurface_Concrete) bake as that surface without a rule.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Ground Surfaces"))
class RTS_SURVIVAL_API URTSGroundSurfaceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTSGroundSurfaceSettings();

	static const URTSGroundSurfaceSettings* Get();

	// The baked surface map of a map: <SurfaceMapFolder>/SurfaceMap_<MapName>.
	FSoftObjectPath GetSurfaceMapPath(const FString& MapName) const;

	const FGroundSurfaceDefinition* FindSurfaceDefinition(EGroundSurfaceType SurfaceType) const;

	// Folder of the baked SurfaceMap_<MapName> data assets; bake again after repainting a map or moving roads.
	UPROPERTY(EditAnywhere, Config, Category="Surface Map", meta=(ContentDir))
	FDirectoryPath SurfaceMapFolder;

	// Size of the baked grid cells in cm; a road of 8 m is about two cells wide at 400.
	UPROPERTY(EditAnywhere, Config, Category="Surface Map", meta=(ClampMin="100"))
	float CellSize = 400.f;

	UPROPERTY(EditAnywhere, Config, Category="Surface Map")
	TArray<FGroundSurfaceDefinition> Surfaces;

	// Surface of landscape paint layers that no definition lists.
	UPROPERTY(EditAnywhere, Config, Category="Surface Map")
	EGroundSurfaceType UnlistedLayerSurface = EGroundSurfaceType::Dirt;

	// Checked in order; the first rule that matches a mesh component decides its surface (None leaves the mesh out).
	UPROPERTY(EditAnywhere, Config, Category="Surface Map")
	TArray<FGroundSurfaceMeshRule> MeshRules;

	// Surface mesh hits higher than this above the landscape are overhead (wires, gantries) and ignored, in cm.
	UPROPERTY(EditAnywhere, Config, Category="Surface Map", meta=(ClampMin="0"))
	float MaxSurfaceMeshHeightAboveLandscape = 300.f;

	// Track smoke where the map has no baked surface (no surface map, or off the landscape).
	UPROPERTY(EditAnywhere, Config, Category="Track Smoke")
	FGroundSurfaceSmokeLook NoSurfaceSmokeLook;

	// Share of the way the smoke moves to the new ground's look per vehicle effect update (0.33 s), so borders blend.
	UPROPERTY(EditAnywhere, Config, Category="Track Smoke", meta=(ClampMin="0.05", ClampMax="1"))
	float SmokeLookBlendPerUpdate = 0.5f;
};
