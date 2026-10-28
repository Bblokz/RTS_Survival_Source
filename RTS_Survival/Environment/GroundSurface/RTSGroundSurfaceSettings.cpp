// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSGroundSurfaceSettings.h"

#include "Engine/StaticMesh.h"

namespace GroundSurfaceSettingsDefaults
{
	FGroundSurfaceDefinition MakeDefinition(const EGroundSurfaceType SurfaceType, TArray<FName> LayerNames,
	                                        const FLinearColor& SmokeTint, const float SmokeDensity)
	{
		FGroundSurfaceDefinition Definition;
		Definition.SurfaceType = SurfaceType;
		Definition.LayerNames = MoveTemp(LayerNames);
		Definition.SmokeLook.SmokeTint = SmokeTint;
		Definition.SmokeLook.SmokeDensity = SmokeDensity;
		return Definition;
	}

	FGroundSurfaceMeshRule MakeActorRule(const TCHAR* ActorClassPath, const EGroundSurfaceType SurfaceType)
	{
		FGroundSurfaceMeshRule Rule;
		Rule.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(ActorClassPath));
		Rule.SurfaceType = SurfaceType;
		return Rule;
	}

	FGroundSurfaceMeshRule MakeMeshRule(const TCHAR* MeshPath, const EGroundSurfaceType SurfaceType)
	{
		FGroundSurfaceMeshRule Rule;
		Rule.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
		Rule.SurfaceType = SurfaceType;
		return Rule;
	}
}

URTSGroundSurfaceSettings::URTSGroundSurfaceSettings()
{
	using namespace GroundSurfaceSettingsDefaults;
	CategoryName = TEXT("Game");
	SectionName = TEXT("RTS Ground Surfaces");
	SurfaceMapFolder.Path = TEXT("/Game/RTS_Survival/Blueprints/Environment/GroundSurface/SurfaceMaps");

	// Dirt keeps the tan the smoke had before it was tinted per surface.
	Surfaces = {
		MakeDefinition(EGroundSurfaceType::Dirt,
		               {TEXT("Dirt"), TEXT("BrownMud"), TEXT("Mud"), TEXT("Craters"), TEXT("ForrestPath"), TEXT("Wet_puddle"),
		                TEXT("Puddles")},
		               FLinearColor(0.88f, 0.682f, 0.55f), 1.f),
		MakeDefinition(EGroundSurfaceType::Grass,
		               {TEXT("Grass"), TEXT("Grass_a"), TEXT("Grass_b"), TEXT("GrassCastle"), TEXT("GrassDry"), TEXT("DryGrass"),
		                TEXT("Weedy"), TEXT("Forest"), TEXT("ForestDry"), TEXT("ForestGround"), TEXT("ForrestGround"),
		                TEXT("GreenForrest")},
		               FLinearColor(0.6f, 0.5f, 0.38f), 0.8f),
		MakeDefinition(EGroundSurfaceType::Sand,
		               {TEXT("Sand"), TEXT("AridSoil"), TEXT("Desert"), TEXT("DesertGravel"), TEXT("GraySand"), TEXT("StandardSand"),
		                TEXT("Standard Sand"), TEXT("WhiteSand"), TEXT("White Sand"), TEXT("RTSSand"), TEXT("Dunes"), TEXT("Beach")},
		               FLinearColor(1.f, 0.66f, 0.38f), 1.3f),
		MakeDefinition(EGroundSurfaceType::Rock,
		               {TEXT("Rock"), TEXT("RockDesert"), TEXT("RockSlope"), TEXT("RockyForrest"), TEXT("RockyPath"),
		                TEXT("DesertRockDark"), TEXT("Mountain"), TEXT("MountainTop"), TEXT("Slope"), TEXT("RiverStone"),
		                TEXT("StoneGround"), TEXT("Layer_StoneGround"), TEXT("ScorchSandyStone")},
		               FLinearColor(0.7f, 0.64f, 0.58f), 0.65f),
		MakeDefinition(EGroundSurfaceType::Concrete,
		               {TEXT("IndustrialConcrete"), TEXT("IndustrialConcrete2"), TEXT("Asphalt"), TEXT("ScorchConcrete")},
		               FLinearColor(0.66f, 0.66f, 0.66f), 0.4f),
	};

	// Specific meshes and the specific ground path kinds first: BP_RoadSpline draws rails and train wires too, and the
	// path kinds are children of BP_GroundPathSpline.
	MeshRules = {
		MakeMeshRule(TEXT("/Game/RTS_Survival/Meshes/Civilian/Poles/TrainWire/SM_RTS_TrainWire_Spline.SM_RTS_TrainWire_Spline"),
		             EGroundSurfaceType::None),
		MakeMeshRule(TEXT("/Game/RTS_Survival/Meshes/Buildings/Industrial/RailRoad/SM_RustedRefinery_RailRoad.SM_RustedRefinery_RailRoad"),
		             EGroundSurfaceType::Rock),
		MakeMeshRule(TEXT("/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01.SM_RTS_Railway_Track_01"),
		             EGroundSurfaceType::Rock),
		MakeActorRule(TEXT("/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Road/BP_GroundPathSpline_Road.BP_GroundPathSpline_Road_C"),
		              EGroundSurfaceType::Concrete),
		MakeActorRule(TEXT("/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/BP_GroundPathSpline_Gravel.BP_GroundPathSpline_Gravel_C"),
		              EGroundSurfaceType::Rock),
		MakeActorRule(TEXT("/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/BP_GroundPathSpline.BP_GroundPathSpline_C"),
		              EGroundSurfaceType::Dirt),
		MakeActorRule(TEXT("/Script/RTS_Survival.RoadSplineActor"), EGroundSurfaceType::Concrete),
	};
}

const URTSGroundSurfaceSettings* URTSGroundSurfaceSettings::Get()
{
	return GetDefault<URTSGroundSurfaceSettings>();
}

FSoftObjectPath URTSGroundSurfaceSettings::GetSurfaceMapPath(const FString& MapName) const
{
	const FString AssetName = TEXT("SurfaceMap_") + MapName;
	return FSoftObjectPath(SurfaceMapFolder.Path / AssetName + TEXT(".") + AssetName);
}

const FGroundSurfaceDefinition* URTSGroundSurfaceSettings::FindSurfaceDefinition(const EGroundSurfaceType SurfaceType) const
{
	return Surfaces.FindByPredicate([SurfaceType](const FGroundSurfaceDefinition& Definition)
	{
		return Definition.SurfaceType == SurfaceType;
	});
}
