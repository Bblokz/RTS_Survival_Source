// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalSovietBaseSettings.h"

#include "RTS_Survival/SurvivalMode/SovietBases/Archetypes/SovietBasePresetLibrary.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBaseLantern.h"
#include "RTS_Survival/SurvivalMode/SovietBases/SurvivalSovietBasePad.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

#define LOCTEXT_NAMESPACE "SurvivalSovietBaseSettings"

namespace
{
	FSurvivalSovietWallPiece SovietWall_Piece(const FEnemyBlueprintEntry& Entry, const float Length, const float PivotAlong,
	                                          const float PivotAcross)
	{
		FSurvivalSovietWallPiece Piece;
		Piece.Entry = Entry;
		Piece.Length = Length;
		Piece.PivotAlong = PivotAlong;
		Piece.PivotAcross = PivotAcross;
		return Piece;
	}

	/**
	 * @return The heavy concrete family. Lengths and pivots are the visible meshes, measured from the spawned actors
	 * (manifest piece_list, wall_joints.py), not the colliders: the colliders run 30 to 60 cm past the meshes, which left
	 * a slot of sand at every joint. All pieces share the outer face at local X 260 (the star on the flag pieces looks along +X).
	 */
	FSurvivalSovietWallFamily SovietWall_MakeSovietConcrete()
	{
		using namespace SurvivalSovietBaseDefaults;
		using EFront = EEnemySandbagFrontAxis;
		constexpr float CentreLine = -130.f;
		// The arch's outer face stands 6.6 cm behind the wall's.
		constexpr float GateCentreLine = -123.4f;
		const auto Wall = [](const TCHAR* const Name)
		{
			return MakeSegment(RusWallBuildings, Name, EFront::PositiveX);
		};
		FSurvivalSovietWallFamily Family;
		Family.Family = ESurvivalSovietWallFamily::SovietConcrete;
		Family.Straight = SovietWall_Piece(Wall(TEXT("Rus_Building_Wall_Soviet")), 1220.4f, 1181.2f, CentreLine);
		Family.FlagStraight = SovietWall_Piece(Wall(TEXT("Rus_Building_WallFlag_Soviet")), 1220.4f, 1181.2f, CentreLine);
		// The piece named after the right hand has its rounded end at the start of the walk.
		Family.StartCap = SovietWall_Piece(Wall(TEXT("Rus_Building_WallRightConnection_Soviet")), 1256.2f, 1248.3f, CentreLine);
		Family.FlagStartCap = SovietWall_Piece(Wall(TEXT("Rus_Building_WallRightConnection_Flag_Soviet")), 1256.2f, 1248.3f, CentreLine);
		Family.EndCap = SovietWall_Piece(Wall(TEXT("Rus_Building_WallLeftConnection_Soviet")), 1249.9f, 4.3f, CentreLine);
		Family.FlagEndCap = SovietWall_Piece(Wall(TEXT("Rus_Building_WallLeftConnection_Flag_Soviet")), 1249.9f, 4.3f, CentreLine);
		Family.Gate = SovietWall_Piece(Wall(TEXT("Rus_Building_GateWired_Soviet")), 1564.7f, 820.1f, GateCentreLine);
		// About one piece in fifteen, never closer than four pieces apart: a flag is a mark, not a pattern.
		Family.FlagRate = 0.07f;
		Family.FlagMinGap = 4;
		Family.Thickness = 260.f;
		return Family;
	}

	FSurvivalSovietWallFamily SovietWall_MakePostApoConcrete()
	{
		using namespace SurvivalSovietBaseDefaults;
		using EFront = EEnemySandbagFrontAxis;
		FSurvivalSovietWallFamily Family;
		Family.Family = ESurvivalSovietWallFamily::PostApoConcrete;
		Family.Straight = SovietWall_Piece(MakeSegment(ConcreteWalls, TEXT("BP_Dest_PostApo_ConcreteFenceLong"), EFront::PositiveX),
		                                   1020.f, 510.f, 0.f);
		Family.Corner = SovietWall_Piece(MakeSegment(ConcreteWalls, TEXT("BP_Dest_PostApo_ConcreteFenceCorner"), EFront::PositiveX),
		                                 176.f, 0.f, 0.f);
		Family.GateLeaf = SovietWall_Piece(MakeSegment(ConcreteWalls, TEXT("BP_Dest_PostApo_ConcreteFenceGate"), EFront::PositiveX),
		                                   334.f, 0.f, 0.f);
		Family.Thickness = 20.f;
		return Family;
	}

	FSurvivalSovietWallFamily SovietWall_MakeSandbag()
	{
		using namespace SurvivalSovietBaseDefaults;
		FSurvivalSovietWallFamily Family;
		Family.Family = ESurvivalSovietWallFamily::Sandbag;
		Family.Straight = SovietWall_Piece(
			MakeSegment(Sandbags, TEXT("BP_Cover_RusSandbags_DoubleStraight"), EEnemySandbagFrontAxis::PositiveX), 858.f, 429.f, 0.f);
		Family.Thickness = 114.f;
		return Family;
	}

	FSurvivalSovietWallFamily SovietWall_MakeWireFence()
	{
		using namespace SurvivalSovietBaseDefaults;
		FSurvivalSovietWallFamily Family;
		Family.Family = ESurvivalSovietWallFamily::WireFence;
		Family.Straight = SovietWall_Piece(
			MakeSegment(Airport, TEXT("BP_Airport_Dest_FenceWire"), EEnemySandbagFrontAxis::PositiveY), 376.f, 188.f, 0.f);
		Family.Thickness = 74.f;
		return Family;
	}

	/** @return Chain-link panels on posts, 364 cm, for fuel yards; the panel stands 43 cm behind its pivot. */
	FSurvivalSovietWallFamily SovietWall_MakeChainLink()
	{
		using namespace SurvivalSovietBaseDefaults;
		FSurvivalSovietWallFamily Family;
		Family.Family = ESurvivalSovietWallFamily::ChainLink;
		Family.Straight = SovietWall_Piece(
			MakeSegment(Environment, TEXT("Industrial/BP_Industrial_Dest_FenceWire"), EEnemySandbagFrontAxis::PositiveX), 364.f, 182.f, 43.f);
		Family.Thickness = 30.f;
		return Family;
	}

	TArray<FSurvivalSovietWallFamily> MakeWallFamilies()
	{
		return {
			SovietWall_MakeSovietConcrete(), SovietWall_MakePostApoConcrete(), SovietWall_MakeSandbag(), SovietWall_MakeWireFence(),
			SovietWall_MakeChainLink()
		};
	}
}

USurvivalSovietBaseSettings::USurvivalSovietBaseSettings()
{
	BaseDestroyedNotification = LOCTEXT("BaseDestroyed", "Soviet base destroyed: {0}");
}

void USurvivalSovietBaseSettings::PostLoad()
{
	Super::PostLoad();
	// The tables live in the asset. An asset saved before they were filled gets the code defaults so a match still
	// works; ResetToDefaults plus a save moves them into the asset for good.
	bool bUseCodeDefaults = Presets.IsEmpty();
#if RTS_WITH_MAP_TESTS
	// One base type's work needs no asset save: -SovietBaseCodeDefaults runs the code tables as they are now.
	bUseCodeDefaults = bUseCodeDefaults or FParse::Param(FCommandLine::Get(), TEXT("SovietBaseCodeDefaults"));
#endif
	if (bUseCodeDefaults)
	{
		FillAllDefaults();
		bM_UsingCodeDefaults = true;
	}
}

void USurvivalSovietBaseSettings::FillAllDefaults()
{
	FillDefaultPresets();
	FillDefaultKit();
	FillDefaultDifficultyScaling();
	FillDefaultStrategicAITypes();
}

FSurvivalSovietBaseDifficultyScaling USurvivalSovietBaseSettings::GetScalingForDifficulty(
	const ERTSGameDifficulty Difficulty) const
{
	const FSurvivalSovietBaseDifficultyScaling* NormalRow = nullptr;
	for (const FSurvivalSovietBaseDifficultyScaling& Row : DifficultyScaling)
	{
		if (Row.Difficulty == Difficulty)
		{
			return Row;
		}
		if (Row.Difficulty == ERTSGameDifficulty::Normal)
		{
			NormalRow = &Row;
		}
	}
	return NormalRow != nullptr ? *NormalRow : FSurvivalSovietBaseDifficultyScaling();
}

#if WITH_EDITOR
void USurvivalSovietBaseSettings::ResetToDefaults()
{
	Modify();
	FillAllDefaults();
	bM_UsingCodeDefaults = false;
}
#endif

void USurvivalSovietBaseSettings::FillDefaultKit()
{
	using namespace SurvivalSovietBaseDefaults;
	using EFront = EEnemySandbagFrontAxis;
	Kit.MainRoadClass = TSoftClassPtr<AActor>(MakeClassPath(GroundPaths, TEXT("BP_GroundPathSpline_Gravel")));
	// Gravel for lanes too: the two-track surface fades in and out and reads as dashes on this ground.
	Kit.LaneRoadClass = TSoftClassPtr<AActor>(MakeClassPath(GroundPaths, TEXT("BP_GroundPathSpline_Gravel")));
	Kit.StripRoadClass = TSoftClassPtr<AActor>(MakeClassPath(GroundPaths, TEXT("BP_GroundPathSpline_Gravel")));

	const TCHAR* const DecalFolder = TEXT("/Game/RTS_Survival/SurvivalMode/Data/SovietBases/Pads/");
	const auto DecalMaterial = [DecalFolder](const TCHAR* const Name)
	{
		return TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), DecalFolder, Name, Name)));
	};
	Kit.MainRoadSurface = DecalMaterial(TEXT("MI_SovietRoad_Main"));
	Kit.LaneRoadSurface = DecalMaterial(TEXT("MI_SovietRoad_Lane"));
	Kit.TrackRoadSurface = DecalMaterial(TEXT("MI_SovietRoad_Track"));
	Kit.StripRoadSurface = DecalMaterial(TEXT("MI_SovietRoad_Main"));
	Kit.GateApronSurface = DecalMaterial(TEXT("MI_SovietRoad_Apron"));
	Kit.MainRoadPaintShare = 1.05f;
	Kit.LaneRoadPaintShare = 0.78f;
	Kit.TrackRoadPaintShare = 0.55f;
	Kit.YardNets = {
		MakeEntry(RusBuildings, TEXT("CamoNets/BP_Dest_RusCamoNetRegular"), 2.f),
		MakeEntry(RusBuildings, TEXT("CamoNets/BP_Dest_RusCamoNetUpright"), 1.f),
		MakeEntry(Military, TEXT("BP_Military_Dest_Camonet_2"), 1.f)
	};
	Kit.AmmoYardFence = ESurvivalSovietWallFamily::PostApoConcrete;
	Kit.FuelYardFence = ESurvivalSovietWallFamily::ChainLink;
	Kit.Lantern.BlueprintClass = ASurvivalSovietBaseLantern::StaticClass();
	Kit.Lantern.bUseFootprintOverride = true;
	Kit.Lantern.FootprintOverride = FVector2D(60.0, 60.0);
	Kit.LanternMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MilitaryBaseCamp/Meshes/SM_LampPost.SM_LampPost")));

	Kit.WallFamilies = MakeWallFamilies();
	Kit.SandbagRuns = {
		MakeSegment(Sandbags, TEXT("BP_Cover_RusSandbags_SingleStraight"), EFront::PositiveX, 2.f),
		MakeSegment(Sandbags, TEXT("BP_Cover_RusSandbags_DoubleStraight"), EFront::PositiveX, 1.f)
	};
	Kit.BarbedWire = {MakeSegment(Barricades, TEXT("BP_Military_Dest_BarbedWire"), EFront::PositiveY)};
	Kit.GateBlocks = {MakeEntry(Barricades, TEXT("BP_Military_Dest_ConcreteBarrier"))};
	Kit.Revetments = {MakeEntry(Sandbags, TEXT("BP_Cover_RusSandbags_Cornered"))};
	Kit.CamoNets = {MakeEntry(Military, TEXT("BP_Military_Dest_Camonet"))};
	Kit.Wrecks = {MakeEntry(DestroyedTanks, TEXT("BP_Env_DestroyedT34"))};
	Kit.Huts = {MakeEntry(Airport, TEXT("BP_Airport_Dest_Building01"))};
	Kit.Hangars = {MakeEntry(Airport, TEXT("BP_Airport_Dest_Hangar"))};
	Kit.Flags = {MakeEntry(Environment, TEXT("Dest_SovietFlag"))};
	Kit.Crates = {
		MakeEntry(Airport, TEXT("BP_Airport_Dest_CrateStack01")),
		MakeEntry(Airport, TEXT("BP_Airport_Dest_CrateStack02")),
		MakeEntry(Airport, TEXT("BP_Airport_Dest_CrateStack03"))
	};
	Kit.Barrels = {
		MakeEntry(Airport, TEXT("BP_Airport_Dest_BarrelStack")),
		MakeEntry(Airport, TEXT("BP_Airport_Dest_BarrelStack2")),
		MakeEntry(Airport, TEXT("BP_Airport_Dest_BarrelStack3"))
	};
	Kit.Munitions = {
		MakeEntry(Missiles, TEXT("BP_Military_Dest_MissileStack_1")),
		MakeEntry(Missiles, TEXT("BP_Military_Dest_MissileStack_2")),
		MakeEntry(Missiles, TEXT("BP_Military_Dest_MissileStack_3")),
		MakeEntry(Missiles, TEXT("BP_Military_Dest_MissileStack_4")),
		MakeEntry(Missiles, TEXT("BP_Military_Dest_MissileStack_5"))
	};
	Kit.Materials = {
		MakeEntry(Airport, TEXT("BP_Airport_Dest_CrateStack01")),
		MakeEntry(Airport, TEXT("BP_Airport_Dest_BarrelStack2"))
	};

	Kit.PadClass = ASurvivalSovietBasePad::StaticClass();
	const TCHAR* const PadFolder = TEXT("/Game/RTS_Survival/SurvivalMode/Data/SovietBases/Pads/");
	const auto PadMaterial = [PadFolder](const TCHAR* const Name)
	{
		return TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(FString::Printf(TEXT("%s%s.%s"), PadFolder, Name, Name)));
	};
	Kit.ConcretePad = PadMaterial(TEXT("MI_SovietPad_Concrete"));
	Kit.GravelPad = PadMaterial(TEXT("MI_SovietPad_Gravel"));
	Kit.PackedEarthPad = PadMaterial(TEXT("MI_SovietPad_PackedEarth"));
	Kit.VirtualTextureEarth = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Game/RTS_Survival/Materials/RVT/M_RTS_RVT_GlobalColor.M_RTS_RVT_GlobalColor")));
	Kit.EarthStandIn = PadMaterial(TEXT("MI_SovietPit_Earth"));
}

void USurvivalSovietBaseSettings::FillDefaultPresets()
{
	// Every type writes its own preset in Archetypes/SovietBaseArchetype_<Type>.cpp.
	Presets.Reset();
	const FSovietPresetLibrary Library;
	Library.AddForwardFirebase(Presets);
	Library.AddRifleCantonment(Presets);
	Library.AddFieldAirstrip(Presets);
	Library.AddTankPark(Presets);
	Library.AddSupplyDepot(Presets);
	Library.AddHillStronghold(Presets);
	Library.AddCommandFortress(Presets);
}

void USurvivalSovietBaseSettings::FillDefaultDifficultyScaling()
{
	using EDiff = ERTSGameDifficulty;
	DifficultyScaling.Reset();
	auto AddRow = [this](const EDiff Difficulty, const FString& Character, const TArray<int32>& Spread,
	                     const TArray<int32>& Budget, const TArray<int32>& Reserve, const int32 MaxUnitPoints,
	                     const int32 MinTopUp, const int32 MaxProductionTier) -> FSurvivalSovietBaseDifficultyScaling&
	{
		FSurvivalSovietBaseDifficultyScaling& Row = DifficultyScaling.AddDefaulted_GetRef();
		Row.Difficulty = Difficulty;
		Row.Character = Character;
		Row.StrengthByDistanceRank = Spread;
		Row.ThreatBudgetByDistanceRank = Budget;
		Row.ReservePointsByDistanceRank = Reserve;
		Row.MaxUnitPoints = MaxUnitPoints;
		Row.MinTopUpUnitPoints = MinTopUp;
		Row.MaxProductionTier = MaxProductionTier;
		return Row;
	};
	FSurvivalSovietBaseDifficultyScaling& NewRow = AddRow(
		EDiff::NewToRTS, TEXT("Rear guard: infantry, MG towers and 45 mm guns, no wire, no tanks"),
		{0, 0, 1, 1}, {50, 70, 125, 190}, {0, 0, 4, 5}, 10, 3, 0);
	NewRow.WireRowsByStrength = {0, 0, 0};
	NewRow.BeltMinStrength = 3;
	NewRow.RewardPerThreatPoint = 3.5f;

	FSurvivalSovietBaseDifficultyScaling& NormalRow = AddRow(
		EDiff::Normal, TEXT("Line regiment: wire, mortars, light tanks, the first tank factories"),
		{0, 0, 1, 2}, {65, 110, 170, 310}, {0, 0, 7, 20}, 15, 3, 1);
	NormalRow.WireRowsByStrength = {0, 1, 1};
	NormalRow.BeltMinStrength = 2;
	NormalRow.RewardPerThreatPoint = 3.f;

	FSurvivalSovietBaseDifficultyScaling& HardRow = AddRow(
		EDiff::Hard, TEXT("Guards division: T-34, BS-3 guns, ZiS bunkers, Bofors, hedgehog belts, research; forward posts"),
		{0, 1, 1, 2}, {85, 165, 250, 450}, {0, 5, 10, 20}, 20, 10, 2);
	HardRow.WireRowsByStrength = {1, 1, 2};
	HardRow.BeltMinStrength = 1;
	HardRow.ForwardPostsByStrength = {0, 2, 2};
	HardRow.RewardPerThreatPoint = 3.f;

	FSurvivalSovietBaseDifficultyScaling& BrutalRow = AddRow(
		EDiff::Brutal, TEXT("Shock army: T-34/85, KV-1, ML-20 batteries, turret bunkers; forward posts and an inner line"),
		{0, 1, 2, 2}, {120, 250, 400, 640}, {0, 7, 12, 20}, 28, 12, 2);
	BrutalRow.WireRowsByStrength = {1, 2, 2};
	BrutalRow.BeltMinStrength = 1;
	BrutalRow.ForwardPostsByStrength = {0, 2, 3};
	BrutalRow.SecondRowBunkersByStrength = {0, 3, 4};
	BrutalRow.RewardPerThreatPoint = 2.75f;
	BrutalRow.FarthestBasePresetWeights = {{TEXT("CommandFortress"), 2.f}, {TEXT("FieldAirstrip"), 0.f}};

	FSurvivalSovietBaseDifficultyScaling& IronmanRow = AddRow(
		EDiff::Ironman, TEXT("Stavka reserve: KV-2, IS-1, B-4 siege guns, mega bunkers; a second belt and crest outposts"),
		{1, 1, 2, 2}, {170, 370, 640, 940}, {0, 10, 20, 26}, 50, 20, 2);
	IronmanRow.WireRowsByStrength = {2, 2, 3};
	IronmanRow.BeltMinStrength = 0;
	IronmanRow.ForwardPostsByStrength = {1, 3, 4};
	IronmanRow.SecondRowBunkersByStrength = {0, 3, 4};
	IronmanRow.bSecondBelt = true;
	IronmanRow.RewardPerThreatPoint = 2.5f;
	IronmanRow.FarthestBasePresetWeights = {{TEXT("CommandFortress"), 3.f}, {TEXT("FieldAirstrip"), 0.f}};
}

void USurvivalSovietBaseSettings::FillDefaultStrategicAITypes()
{
	StrategicAICoreBuildingTypes = {
		EBuildingExpansionType::BTX_RusBarracks,
		EBuildingExpansionType::BTX_RusFactory,
		EBuildingExpansionType::BTX_RusCoolingTowers,
		EBuildingExpansionType::BTX_RusResearchCenter,
		EBuildingExpansionType::BTX_RusPlatformFactory,
		EBuildingExpansionType::BTX_RusAssemblyYard
	};
	StrategicAISatelliteBuildingTypes = {
		EBuildingExpansionType::BTX_RusAmmoStorage,
		EBuildingExpansionType::BTX_RusFuelStorage1,
		EBuildingExpansionType::BTX_Rus_SuppliesStorage,
		EBuildingExpansionType::BTX_RusGuardTower,
		EBuildingExpansionType::BTX_RusBunkerMG,
		EBuildingExpansionType::BTX_RusDomeBunker,
		EBuildingExpansionType::BTX_RusLongCamoBunker,
		EBuildingExpansionType::BTX_RusMegaBunker,
		EBuildingExpansionType::BTX_Bunker02_ZIS,
		EBuildingExpansionType::BTX_Bunker05_WithTurrets,
		EBuildingExpansionType::BTX_Bunker05,
		EBuildingExpansionType::BTX_RusBoforsPosition
	};
}

#undef LOCTEXT_NAMESPACE
