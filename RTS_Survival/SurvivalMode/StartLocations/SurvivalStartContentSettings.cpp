// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalStartContentSettings.h"

#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Misc/Paths.h"

namespace SurvivalStartContentDefaults
{
	/**
	 * @brief Builds a default entry from a Blueprint package path; the generated class name is derived from it.
	 * @param BlueprintPackagePath e.g. /Game/Folder/BP_Thing (without the .BP_Thing_C suffix).
	 */
	FSurvivalWeightedActorClass MakeEntry(
		const FString& BlueprintPackagePath,
		const float Weight,
		const float FootprintRadius,
		const bool bAlignToGround = false,
		const bool bRandomYaw = true)
	{
		const FString AssetName = FPaths::GetBaseFilename(BlueprintPackagePath);
		FSurvivalWeightedActorClass Entry;
		Entry.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(
			FString::Printf(TEXT("%s.%s_C"), *BlueprintPackagePath, *AssetName)));
		Entry.Weight = Weight;
		Entry.FootprintRadius = FootprintRadius;
		Entry.bAlignToGround = bAlignToGround;
		Entry.bRandomYaw = bRandomYaw;
		return Entry;
	}

	FSurvivalWeightedMesh MakeMesh(const FString& MeshPackagePath, const float Weight)
	{
		const FString AssetName = FPaths::GetBaseFilename(MeshPackagePath);
		FSurvivalWeightedMesh Entry;
		Entry.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
			FString::Printf(TEXT("%s.%s"), *MeshPackagePath, *AssetName)));
		Entry.Weight = Weight;
		return Entry;
	}

	const FString RadixiteFolder = TEXT("/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/");
	const FString MetalFolder = TEXT("/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/");
	const FString TankWreckFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/DestroyedTanks/German/");
	const FString VehiclePartsFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/");
	const FString GerPitFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/CaptureObjects/GerPits/");
	const FString CaptureBunkerFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/CaptureObjects/Bunkers/");
	const FString CraterFolder = TEXT("/Game/RTS_Survival/Materials/Decals/CraterDirt/");
	// The map's own dead shrubs (FT_SM_DeadShrub_* paint these variants, so they cook with the map).
	const FString DeadShrubFolder = TEXT("/Game/Megascans/3D_Plants/Deadshrub-01_vdknafgha/");
	const FString AirportFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/");
	const FString SpecialsFolder = TEXT("/Game/RTS_Survival/SurvivalMode/Maps/Specials/");
	const FString GroundStoneFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/GroundStones/Meshes/");
	const FString IndustrialPropFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Industrial/");
	const FString DestroyedTankPropFolder = TEXT("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/DestTanks/");

	// Footprints measured from the Blueprints' mesh bounds (2026-10-29), rounded up.
	constexpr float GrowthCrystalFootprint = 600.f;
	constexpr float CrystalFootprint = 200.f;
	constexpr float MetalTowerFootprint = 1400.f;
	constexpr float MetalRuinFootprint = 650.f;
	constexpr float MetalBarrelsFootprint = 280.f;
	constexpr float MetalWaterTankFootprint = 970.f;
	constexpr float MetalRailsFootprint = 620.f;
	// Geometry-collection wrecks and pits have no static mesh to measure; conservative estimates.
	constexpr float TankWreckFootprint = 550.f;
	constexpr float HeavyTankWreckFootprint = 650.f;
	constexpr float VehiclePartsFootprint = 350.f;
	constexpr float PitFootprint = 600.f;
	constexpr float CaptureBunkerFootprint = 750.f;
	constexpr float CraterFootprint = 350.f;
	constexpr float SmallCraterFootprint = 180.f;
	// Round 10 specials (measured visible meshes: hangar +-860 cm, Luchs and Marder hulls ~ 3 m long).
	constexpr float HangarFootprint = 1250.f;
	constexpr float LightTankFootprint = 330.f;
	constexpr float HangarStoreFootprint = 270.f;
	constexpr float LargeCrystalFootprint = 350.f;
	constexpr float DetailDecalFootprint = 400.f;
	// Round 11 scrap yard props (measured actor bounds, rounded up).
	constexpr float YardContainerFootprint = 650.f;
	constexpr float YardOilTankFootprint = 400.f;
	constexpr float YardTankHulkFootprint = 350.f;
}

USurvivalStartContentSettings::USurvivalStartContentSettings()
{
	InitDefaultRadixiteFields();
	InitDefaultMetalFields();
	InitDefaultWreckGroups();
	InitDefaultPits();
	InitDefaultClutter();
	InitDefaultDifficultyScaling();
	InitDefaultCloseResources();
	InitDefaultSpecials();
	InitDefaultGroundDetail();
}

const FSurvivalStartSpecialSettings* USurvivalStartContentSettings::FindSpecial(const ESurvivalStartSpecial Special) const
{
	if (Special == ESurvivalStartSpecial::None)
	{
		return nullptr;
	}
	return Specials.FindByPredicate([Special](const FSurvivalStartSpecialSettings& Entry) { return Entry.Special == Special; });
}

void USurvivalStartContentSettings::GetCloseNodeCounts(const ESurvivalStartSpecial Special, int32& OutRadixite,
                                                       int32& OutMetal) const
{
	OutRadixite = CloseResources.RadixiteNodes;
	OutMetal = CloseResources.MetalNodes;
	if (const FSurvivalStartSpecialSettings* SpecialSettings = FindSpecial(Special))
	{
		OutRadixite += SpecialSettings->ExtraRadixiteNodes;
		OutMetal += SpecialSettings->ExtraMetalNodes;
	}
}

FSurvivalStartContentDifficultyScale USurvivalStartContentSettings::GetDifficultyScale(
	const ERTSGameDifficulty Difficulty) const
{
	if (const FSurvivalStartContentDifficultyScale* Scale = DifficultyScaling.Find(Difficulty))
	{
		return *Scale;
	}
	return FSurvivalStartContentDifficultyScale();
}

float USurvivalStartContentSettings::GetOuterContentDistanceFromBuildEdge() const
{
	const auto GetFieldOuterDistance = [](const FSurvivalResourceFieldSettings& Fields) -> float
	{
		return Fields.GapFromBuildEdge + Fields.MaxExtraOutwardOffset + 2.f * Fields.FieldRadius;
	};
	float OuterDistance = FMath::Max(GetFieldOuterDistance(RadixiteFields), GetFieldOuterDistance(MetalFields));
	for (const FSurvivalWreckGroup& Group : WreckGroups)
	{
		OuterDistance = FMath::Max(OuterDistance, Group.MaxDistanceFromBuildEdge);
	}
	return FMath::Max(OuterDistance, Clutter.MaxDistanceFromBuildEdge);
}

void USurvivalStartContentSettings::InitDefaultRadixiteFields()
{
	using namespace SurvivalStartContentDefaults;
	RadixiteFields.FieldCount = 2;
	RadixiteFields.GapFromBuildEdge = 600.f;
	RadixiteFields.MaxExtraOutwardOffset = 1200.f;
	RadixiteFields.FieldRadius = 1800.f;
	RadixiteFields.MinFieldActors = 5;
	RadixiteFields.MaxFieldActors = 8;
	// The growth crystal spawns new growth nodes over time (URadixiteGrowthComponent).
	RadixiteFields.CentreActors = {
		MakeEntry(RadixiteFolder + TEXT("Resource_Radixite_Crystal_LargeWithGrowth"), 1.f, GrowthCrystalFootprint)
	};
	RadixiteFields.FieldActors = {
		MakeEntry(RadixiteFolder + TEXT("Resource_Radixite_Crystal"), 1.f, CrystalFootprint),
		MakeEntry(RadixiteFolder + TEXT("Resource_Radixite_Crystal_SmallDecal"), 1.f, CrystalFootprint),
		MakeEntry(RadixiteFolder + TEXT("Resource_Radixite_Crystal_NoDecal"), 1.f, CrystalFootprint)
	};
}

void USurvivalStartContentSettings::InitDefaultMetalFields()
{
	using namespace SurvivalStartContentDefaults;
	MetalFields.FieldCount = 2;
	MetalFields.GapFromBuildEdge = 600.f;
	MetalFields.MaxExtraOutwardOffset = 800.f;
	MetalFields.FieldRadius = 2600.f;
	MetalFields.MinFieldActors = 2;
	MetalFields.MaxFieldActors = 4;
	MetalFields.ActorClearance = 150.f;
	MetalFields.MaxGroundSlopeDegrees = 25.f;
	MetalFields.CentreActors = {
		MakeEntry(MetalFolder + TEXT("Resource_MetalTower"), 1.f, MetalTowerFootprint),
		MakeEntry(MetalFolder + TEXT("Resource_Metal_Ruin1"), 0.6f, MetalRuinFootprint)
	};
	MetalFields.FieldActors = {
		MakeEntry(MetalFolder + TEXT("Resource_Metal_Barrels"), 1.f, MetalBarrelsFootprint),
		MakeEntry(MetalFolder + TEXT("Resource_Metal_BrokenWaterTank"), 0.5f, MetalWaterTankFootprint),
		MakeEntry(MetalFolder + TEXT("Resource_Metal_Rails"), 0.7f, MetalRailsFootprint)
	};
}

void USurvivalStartContentSettings::InitDefaultWreckGroups()
{
	using namespace SurvivalStartContentDefaults;
	// Same tank hulks the campaign's BP_Prx_ScavengeObject_Medium picks from (metal, radixite, blueprints),
	// spawned directly: the proxy and BP_Async_* loaders need ARTSLandscapeDivider / add a 2 s delay.
	FSurvivalWreckGroup TankHulks;
	TankHulks.GroupName = TEXT("TankHulks");
	TankHulks.Count = 3;
	TankHulks.MinDistanceFromBuildEdge = 5500.f;
	TankHulks.MaxDistanceFromBuildEdge = 9500.f;
	TankHulks.MinSpacing = 2000.f;
	TankHulks.Wrecks = {
		MakeEntry(TankWreckFolder + TEXT("Light/PZ38T/ScavObj_Ger_Pz38T"), 1.f, TankWreckFootprint, true),
		MakeEntry(TankWreckFolder + TEXT("Medium/Stug/ScavObj_Ger_Stug"), 1.f, TankWreckFootprint, true),
		MakeEntry(TankWreckFolder + TEXT("Medium/PZIV/ScavObj_Ger_PZIV"), 1.f, TankWreckFootprint, true),
		// 300-600 metal plus blueprints: rare.
		MakeEntry(TankWreckFolder + TEXT("Heavy/Tiger1/ScavObj_Ger_Tiger1"), 0.25f, HeavyTankWreckFootprint, true)
	};
	WreckGroups.Add(TankHulks);

	// Vehicle-part piles and car wrecks: RESOURCE_VEHICLE_PARTS + a little metal each.
	FSurvivalWreckGroup VehicleParts;
	VehicleParts.GroupName = TEXT("VehicleParts");
	VehicleParts.Count = 4;
	VehicleParts.MinDistanceFromBuildEdge = 5000.f;
	VehicleParts.MaxDistanceFromBuildEdge = 9000.f;
	VehicleParts.MinSpacing = 1200.f;
	VehicleParts.Wrecks = {
		MakeEntry(VehiclePartsFolder + TEXT("ScavObj_VehicleParts1"), 1.f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("ScavObj_VehicleParts2"), 1.f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("ScavObj_VehicleParts3"), 1.f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("ScavObj_VehicleParts4"), 1.f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("Cars/ScavObj_PostApo_Car_1"), 0.5f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("Cars/ScavObj_PostApo_Car_2"), 0.5f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("Cars/ScavObj_PostApo_Car_3"), 0.5f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("Cars/ScavObj_PostApo_Car_4"), 0.5f, VehiclePartsFootprint, true),
		MakeEntry(VehiclePartsFolder + TEXT("Cars/ScavObj_PostApo_Car_5"), 0.5f, VehiclePartsFootprint, true)
	};
	WreckGroups.Add(VehicleParts);
}

void USurvivalStartContentSettings::InitDefaultPits()
{
	using namespace SurvivalStartContentDefaults;
	Pits.PitCount = 3;
	constexpr float LightPitWeight = 1.f;
	constexpr float HeavyPitWeight = 0.35f;
	constexpr float BunkerWeight = 0.2f;
	constexpr bool bAlignToGround = false;
	constexpr bool bRandomYaw = false;
	Pits.Pits = {
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_Flak37"), LightPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_Pak40"), LightPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_TwinMG42"), LightPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_Mortar"), LightPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_Puppchen"), LightPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_Panzerschreck"), LightPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_Flak88"), HeavyPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_LeFH18"), HeavyPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(GerPitFolder + TEXT("BP_CaptureGerPit_SFH18"), HeavyPitWeight, PitFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(CaptureBunkerFolder + TEXT("BP_CaptureBunker_37MM"), BunkerWeight, CaptureBunkerFootprint, bAlignToGround, bRandomYaw),
		MakeEntry(CaptureBunkerFolder + TEXT("BP_CaptureBunker_Turret"), BunkerWeight, CaptureBunkerFootprint, bAlignToGround, bRandomYaw)
	};
}

void USurvivalStartContentSettings::InitDefaultClutter()
{
	using namespace SurvivalStartContentDefaults;
	Clutter.CraterActors = {
		MakeEntry(CraterFolder + TEXT("BP_RTS_RandomDirtCraterDecal_Sandy"), 1.f, CraterFootprint),
		MakeEntry(CraterFolder + TEXT("BP_RTS_RandomDirtCraterDecal_Small_Sandy"), 1.5f, SmallCraterFootprint)
	};
	// Round 11: the grey-green SM_GF dry tufts read as placeholder spots from the RTS camera on the desert discs; the
	// map's brown dead shrubs read as dry growth.
	Clutter.FoliageMeshes = {
		MakeMesh(DeadShrubFolder + TEXT("S_Deadshrub-01_vdknafgha_Var1_lod0"), 1.f),
		MakeMesh(DeadShrubFolder + TEXT("S_Deadshrub-01_vdknafgha_Var3_lod0"), 1.f),
		MakeMesh(DeadShrubFolder + TEXT("S_Deadshrub-01_vdknafgha_Var5_lod0"), 1.f),
		MakeMesh(DeadShrubFolder + TEXT("S_Deadshrub-01_vdknafgha_Var7_lod0"), 1.f),
		MakeMesh(DeadShrubFolder + TEXT("S_Deadshrub-01_vdknafgha_Var9_lod0"), 0.7f),
		MakeMesh(DeadShrubFolder + TEXT("S_Deadshrub-01_vdknafgha_Var11_lod0"), 0.7f)
	};
}

void USurvivalStartContentSettings::InitDefaultDifficultyScaling()
{
	const auto MakeScale = [](const float FieldCount, const float FieldActors, const float Wrecks, const float Pits)
	{
		FSurvivalStartContentDifficultyScale Scale;
		Scale.ResourceFieldCountMultiplier = FieldCount;
		Scale.ResourceFieldActorMultiplier = FieldActors;
		Scale.WreckCountMultiplier = Wrecks;
		Scale.PitCountMultiplier = Pits;
		return Scale;
	};
	DifficultyScaling.Add(ERTSGameDifficulty::NewToRTS, MakeScale(1.5f, 1.25f, 1.5f, 1.5f));
	DifficultyScaling.Add(ERTSGameDifficulty::Normal, MakeScale(1.f, 1.f, 1.f, 1.f));
	DifficultyScaling.Add(ERTSGameDifficulty::Hard, MakeScale(1.f, 0.85f, 0.75f, 0.75f));
	DifficultyScaling.Add(ERTSGameDifficulty::Brutal, MakeScale(0.75f, 0.75f, 0.6f, 0.5f));
	DifficultyScaling.Add(ERTSGameDifficulty::Ironman, MakeScale(0.75f, 0.7f, 0.5f, 0.35f));
}

void USurvivalStartContentSettings::InitDefaultCloseResources()
{
	using namespace SurvivalStartContentDefaults;
	CloseResources.RadixiteActors = {
		MakeEntry(RadixiteFolder + TEXT("Resource_Radixite_Crystal_Large"), 1.f, LargeCrystalFootprint),
		MakeEntry(RadixiteFolder + TEXT("Resource_Radixite_Crystal"), 1.f, CrystalFootprint)
	};
	// Round 11: the flat rails (22 cm high) do not read as metal from the RTS camera; barrels and the water tank do.
	CloseResources.MetalActors = {
		MakeEntry(MetalFolder + TEXT("Resource_Metal_Barrels"), 1.f, MetalBarrelsFootprint),
		MakeEntry(MetalFolder + TEXT("Resource_Metal_BrokenWaterTank"), 0.8f, MetalWaterTankFootprint)
	};
}

void USurvivalStartContentSettings::InitDefaultSpecials()
{
	using namespace SurvivalStartContentDefaults;
	constexpr bool bUpright = false;
	constexpr bool bFixedYaw = false;
	FSurvivalStartSpecialSettings Radixite;
	Radixite.Special = ESurvivalStartSpecial::MostRadixite;
	Radixite.Title = NSLOCTEXT("SurvivalStartSpecial", "RadixiteTitle", "Radixite seam");
	Radixite.Description = NSLOCTEXT("SurvivalStartSpecial", "RadixiteLine", "The most radixite next to the drop-off");
	Radixite.ExtraRadixiteNodes = 4;
	Specials.Add(Radixite);

	FSurvivalStartSpecialSettings Metal;
	Metal.Special = ESurvivalStartSpecial::MostMetal;
	Metal.Title = NSLOCTEXT("SurvivalStartSpecial", "MetalTitle", "Scrap yard");
	Metal.Description = NSLOCTEXT("SurvivalStartSpecial", "MetalLine", "The most metal next to the drop-off");
	Metal.ExtraMetalNodes = 4;
	// Round 11: the extra nodes stand together in a scrap yard with containers, oil tanks and burnt-out hulks round it.
	Metal.MinDistance = 3300.f;
	Metal.MaxDistance = 5000.f;
	Metal.MaxGroundSlopeDegrees = 14.f;
	Metal.YardRadius = 1500.f;
	Metal.YardMetalActors = {
		MakeEntry(MetalFolder + TEXT("Resource_Metal_BrokenWaterTank"), 1.f, MetalWaterTankFootprint),
		MakeEntry(MetalFolder + TEXT("Resource_Metal_Barrels"), 1.f, MetalBarrelsFootprint)
	};
	Metal.YardPropCount = 5;
	Metal.YardProps = {
		MakeEntry(IndustrialPropFolder + TEXT("BP_Industrial_Dest_Container"), 1.f, YardContainerFootprint, bUpright),
		MakeEntry(IndustrialPropFolder + TEXT("BP_Industrial_Dest_OilTankBarrels1"), 1.f, YardOilTankFootprint, bUpright),
		MakeEntry(IndustrialPropFolder + TEXT("BP_Industrial_Dest_OilTankBarrels2"), 1.f, YardOilTankFootprint, bUpright),
		MakeEntry(DestroyedTankPropFolder + TEXT("BP_Env_DestroyedPz38T"), 0.7f, YardTankHulkFootprint),
		MakeEntry(DestroyedTankPropFolder + TEXT("BP_Env_DestroyedPzII"), 0.7f, YardTankHulkFootprint)
	};
	Metal.YardDecalCount = 5;
	Metal.YardDecals = {
		MakeEntry(SpecialsFolder + TEXT("BP_SMD_TrackDecal_Churn"), 1.f, DetailDecalFootprint),
		MakeEntry(SpecialsFolder + TEXT("BP_SMD_TrackDecal_Ruts"), 1.f, DetailDecalFootprint),
		MakeEntry(CraterFolder + TEXT("BP_RTS_RandomDirtCraterDecal_Small_Sandy"), 0.8f, DetailDecalFootprint)
	};
	Specials.Add(Metal);

	FSurvivalStartSpecialSettings Wrecks;
	Wrecks.Special = ESurvivalStartSpecial::Wrecks;
	Wrecks.Title = NSLOCTEXT("SurvivalStartSpecial", "WrecksTitle", "Wreck field");
	Wrecks.Description = NSLOCTEXT("SurvivalStartSpecial", "WrecksLine", "4 scavengeable tank wrecks by the drop-off");
	Wrecks.WreckCount = 4;
	Wrecks.MinDistance = 2600.f;
	Wrecks.MaxDistance = 5200.f;
	Wrecks.MaxGroundSlopeDegrees = 25.f;
	Wrecks.Wrecks = {
		MakeEntry(TankWreckFolder + TEXT("Light/PZ38T/ScavObj_Ger_Pz38T"), 1.f, TankWreckFootprint, true),
		MakeEntry(TankWreckFolder + TEXT("Medium/Stug/ScavObj_Ger_Stug"), 1.f, TankWreckFootprint, true),
		MakeEntry(TankWreckFolder + TEXT("Medium/PZIV/ScavObj_Ger_PZIV"), 1.f, TankWreckFootprint, true),
		MakeEntry(TankWreckFolder + TEXT("Heavy/Tiger1/ScavObj_Ger_Tiger1"), 0.3f, HeavyTankWreckFootprint, true)
	};
	Specials.Add(Wrecks);

	const auto MakeHangar = [&](const ESurvivalStartSpecial Special, const FText& Description, const FString& CaptureBlueprint)
	{
		FSurvivalStartSpecialSettings Hangar;
		Hangar.Special = Special;
		Hangar.Title = NSLOCTEXT("SurvivalStartSpecial", "HangarTitle", "Hangar");
		Hangar.Description = Description;
		// BP_SMD_Hangar is the Airport hangar with matte shell MIs (the Airport MIs' Specular_Int 3.0 reads as chrome).
		Hangar.Hangar = MakeEntry(SpecialsFolder + TEXT("BP_SMD_Hangar"), 1.f, HangarFootprint, bUpright, bFixedYaw);
		Hangar.CaptureVehicleCount = 3;
		Hangar.CaptureVehicle = MakeEntry(SpecialsFolder + CaptureBlueprint, 1.f, LightTankFootprint, bUpright, bFixedYaw);
		Hangar.HangarStores = {
			MakeEntry(AirportFolder + TEXT("BP_Airport_Dest_BarrelStack"), 1.f, HangarStoreFootprint, bUpright),
			MakeEntry(AirportFolder + TEXT("BP_Airport_Dest_CrateStack01"), 1.f, HangarStoreFootprint, bUpright)
		};
		return Hangar;
	};
	Specials.Add(MakeHangar(ESurvivalStartSpecial::LuchsHangar,
	                        NSLOCTEXT("SurvivalStartSpecial", "LuchsLine", "3 capturable Panzer II Luchs"),
	                        TEXT("BP_SMD_CaptureLuchs")));
	Specials.Add(MakeHangar(ESurvivalStartSpecial::MarderHangar,
	                        NSLOCTEXT("SurvivalStartSpecial", "MarderLine", "3 capturable twin-barrel Marders"),
	                        TEXT("BP_SMD_CaptureMarderDual")));
}

void USurvivalStartContentSettings::InitDefaultGroundDetail()
{
	using namespace SurvivalStartContentDefaults;
	GroundDetail.DecalActors = {
		MakeEntry(SpecialsFolder + TEXT("BP_SMD_TrackDecal_Ruts"), 1.2f, DetailDecalFootprint),
		MakeEntry(SpecialsFolder + TEXT("BP_SMD_TrackDecal_Churn"), 0.8f, DetailDecalFootprint),
		// Round 11: the Crater_Decals dirt clusters carry green radixite debris and read as grey-green placeholder
		// spots on the sand; sandy scorch craters replace them (the sandy track and tyre-print decals read as red claw
		// marks at this size, and the concrete crack decals read as grey squares on sand, so neither is used).
		MakeEntry(CraterFolder + TEXT("BP_RTS_RandomDirtCraterDecal_Small_Sandy"), 1.f, DetailDecalFootprint),
		MakeEntry(CraterFolder + TEXT("BP_RTS_RandomDirtCraterDecal_Sandy"), 0.5f, DetailDecalFootprint)
	};
	GroundDetail.StoneMeshes = {
		MakeMesh(GroundStoneFolder + TEXT("SM_GS_Chunk_A"), 1.f),
		MakeMesh(GroundStoneFolder + TEXT("SM_GS_Chunk_B"), 1.f),
		MakeMesh(GroundStoneFolder + TEXT("SM_GS_Chunk_C"), 1.f),
		MakeMesh(GroundStoneFolder + TEXT("SM_GS_Cluster_A"), 0.6f),
		MakeMesh(GroundStoneFolder + TEXT("SM_GS_Cluster_B"), 0.6f),
		MakeMesh(GroundStoneFolder + TEXT("SM_GS_Slab_A"), 0.4f)
	};
}
