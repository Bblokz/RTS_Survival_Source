// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalPOISettings.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"

namespace SurvivalPOIDefaults
{
	const FString Environment = TEXT("/Game/RTS_Survival/Blueprints/Environment/");
	const FString DestTanks = Environment + TEXT("DestructableEnvActor/Military/DestTanks/");
	const FString ScavTanks = Environment + TEXT("ScavengeObjects/DestroyedTanks/German/");
	const FString GerPits = Environment + TEXT("CaptureObjects/GerPits/");
	const FString Sandbags = Environment + TEXT("DestructableEnvActor/NaturalCover/SandBagWalls/");
	const FString Airport = Environment + TEXT("DestructableEnvActor/Airport/");
	const FString VehicleParts = Environment + TEXT("ScavengeObjects/VehicleParts/");
	const FString Craters = TEXT("/Game/RTS_Survival/Materials/Decals/CraterDirt/");
	const FString DirtDecals = TEXT("/Game/RTS_Survival/Materials/Decals/DirtTracks/");
	const FString AmmoMeshes = TEXT("/Game/RTS_Survival/Meshes/Props/Ammo/");
	const FString Animated = Environment + TEXT("Animated/");

	/** @return The asset name of a "Sub/Folder/Name" relative path. */
	FString AssetName(const FString& Name)
	{
		int32 Slash = INDEX_NONE;
		return Name.FindLastChar(TEXT('/'), Slash) ? Name.Mid(Slash + 1) : Name;
	}

	/** @return A class path "<Folder><Name>.<AssetName>_C"; Name may carry sub folders. */
	FString ClassPath(const FString& Folder, const FString& Name)
	{
		return Folder + Name + TEXT(".") + AssetName(Name) + TEXT("_C");
	}

	FString ObjectPath(const FString& Folder, const FString& Name)
	{
		return Folder + Name + TEXT(".") + AssetName(Name);
	}

	FSurvivalPOIAsset MakeClass(const FString& Folder, const FString& Name, const float Footprint,
	                            const ESurvivalPOISeatMode SeatMode, const float Weight = 1.f, const float MaxSlope = 15.f)
	{
		FSurvivalPOIAsset Asset;
		Asset.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath(Folder, Name)));
		Asset.FootprintRadius = Footprint;
		Asset.SeatMode = SeatMode;
		Asset.Weight = Weight;
		Asset.MaxSlopeDegrees = MaxSlope;
		return Asset;
	}

	FSurvivalPOIAsset MakeMesh(const FString& Folder, const FString& Name, const float Footprint, const bool bBlocksNavigation,
	                           const float Weight = 1.f)
	{
		FSurvivalPOIAsset Asset;
		Asset.Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(ObjectPath(Folder, Name)));
		Asset.FootprintRadius = Footprint;
		Asset.bBlocksNavigation = bBlocksNavigation;
		Asset.Weight = Weight;
		Asset.SeatMode = ESurvivalPOISeatMode::AlignToGround;
		return Asset;
	}

	FSurvivalPOIAsset MakeDecal(const FString& Folder, const FString& Name, const FVector& Size, const float Weight = 1.f)
	{
		FSurvivalPOIAsset Asset;
		Asset.DecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(ObjectPath(Folder, Name)));
		Asset.DecalSize = Size;
		Asset.FootprintRadius = FMath::Max(Size.Y, Size.Z);
		Asset.SeatMode = ESurvivalPOISeatMode::PivotOnGround;
		Asset.MaxSlopeDegrees = 30.f;
		Asset.bBlocksNavigation = false;
		Asset.Weight = Weight;
		return Asset;
	}

	/** @brief Truck wrecks placed outside a death carry no salvage until they know which vehicle they were. */
	FSurvivalPOIAsset WithTankReward(FSurvivalPOIAsset Asset, const TCHAR* TankSubtype)
	{
		Asset.RewardTankSubtype = TankSubtype;
		return Asset;
	}

	FSurvivalPOIAssetPool MakePool(const FName Name, TArray<FSurvivalPOIAsset>&& Assets)
	{
		FSurvivalPOIAssetPool Pool;
		Pool.PoolName = Name;
		Pool.Assets = MoveTemp(Assets);
		return Pool;
	}

	FSurvivalPOITypeRules MakeRules(const ESurvivalPOIType Type, const TCHAR* DisplayName, const float Radius,
	                                const float HeightRange, const float Slope, const FVector2D& OwnerDistance)
	{
		FSurvivalPOITypeRules Rules;
		Rules.Type = Type;
		Rules.DisplayName = FText::FromString(DisplayName);
		Rules.SiteRadius = Radius;
		Rules.MaxHeightRange = HeightRange;
		Rules.MaxSlopeDegrees = Slope;
		Rules.OwnerStartDistance = OwnerDistance;
		return Rules;
	}
}

void USurvivalPointsOfInterestSettings::PostLoad()
{
	Super::PostLoad();
	if (Pools.IsEmpty())
	{
		// An asset saved before the tables existed still works; ResetToDefaults plus a save stores them.
		FillDefaults();
		bM_UsingCodeDefaults = true;
	}
}

void USurvivalPointsOfInterestSettings::ResetToDefaults()
{
	FillDefaults();
	bM_UsingCodeDefaults = false;
	MarkPackageDirty();
}

void USurvivalPointsOfInterestSettings::FillDefaults()
{
	FillDefaults_Placement();
	FillDefaults_TypesAndSectors();
	Pools.Reset();
	FillDefaults_BattlefieldPools();
	FillDefaults_OtherPools();
	FillDefaults_LandmarkPools();
	FillDefaults_Wildlife();
}

void USurvivalPointsOfInterestSettings::FillDefaults_Placement()
{
	Placement = FSurvivalPOIPlacementRules();
	// SM_Desert_01: the oasis basin outline (layout JSON features.world.basins "oasis") has its centroid at (16000, -3525)
	// and its farthest point 6109 cm from it.
	FSurvivalPOIExclusionZone Oasis;
	Oasis.Name = TEXT("Oasis");
	Oasis.Center = FVector2D(16000.0, -3525.0);
	Oasis.Radius = 6150.f;
	Oasis.PropRim = 3000.f;
	Oasis.bIsWater = true;
	Placement.ExclusionZones = {Oasis};
	// SM_Desert_01 cropped to its playable square (coordinator, 2026-10-31 19:32); the landscape still reaches +-75 600.
	Placement.PlayableAreaMin = FVector2D(-69400.0, -64400.0);
	Placement.PlayableAreaMax = FVector2D(66200.0, 71200.0);
}

void USurvivalPointsOfInterestSettings::FillDefaults_TypesAndSectors()
{
	using namespace SurvivalPOIDefaults;
	TypeRules = {
		MakeRules(ESurvivalPOIType::SmallBattlefield, TEXT("Small battlefield"), 3400.f, 260.f, 8.f, FVector2D(19000.0, 27000.0)),
		MakeRules(ESurvivalPOIType::RuinedHangar, TEXT("Ruined hangar"), 2800.f, 120.f, 4.f, FVector2D(19000.0, 27000.0)),
		MakeRules(ESurvivalPOIType::CrashSite, TEXT("Crash site"), 4200.f, 300.f, 8.f, FVector2D(19000.0, 34000.0)),
		MakeRules(ESurvivalPOIType::DeadConvoy, TEXT("Dead convoy"), 4600.f, 300.f, 8.f, FVector2D(19000.0, 34000.0)),
		MakeRules(ESurvivalPOIType::TankGraveyard, TEXT("Tank graveyard"), 4000.f, 320.f, 9.f, FVector2D(19000.0, 34000.0)),
		MakeRules(ESurvivalPOIType::PumpStation, TEXT("Pump station"), 3000.f, 150.f, 5.f, FVector2D(19000.0, 34000.0)),
		MakeRules(ESurvivalPOIType::RelayStation, TEXT("Relay station"), 3800.f, 200.f, 6.f, FVector2D(19000.0, 34000.0)),
	};
	// 05_MapVision.md section 4, sectors named by the start display names of SM_Desert_01.
	auto MakeSector = [](const TCHAR* A, const TCHAR* B, const ESurvivalPOIType Type)
	{
		FSurvivalPOISectorLandmark Sector;
		Sector.StartA = A;
		Sector.StartB = B;
		Sector.Landmark = Type;
		return Sector;
	};
	SectorLandmarks = {
		MakeSector(TEXT("Dune Ridge"), TEXT("Salt Flats"), ESurvivalPOIType::CrashSite),
		MakeSector(TEXT("Salt Flats"), TEXT("Scorpion Basin"), ESurvivalPOIType::DeadConvoy),
		MakeSector(TEXT("Scorpion Basin"), TEXT("Red Mesa"), ESurvivalPOIType::PumpStation),
		MakeSector(TEXT("Red Mesa"), TEXT("Dry Wells"), ESurvivalPOIType::TankGraveyard),
		MakeSector(TEXT("Dry Wells"), TEXT("Dune Ridge"), ESurvivalPOIType::RelayStation),
	};
	BuiltLandmarkTypes = {ESurvivalPOIType::CrashSite, ESurvivalPOIType::DeadConvoy, ESurvivalPOIType::TankGraveyard,
	                      ESurvivalPOIType::PumpStation, ESurvivalPOIType::RelayStation};
	GenericPerSector = {ESurvivalPOIType::SmallBattlefield, ESurvivalPOIType::RuinedHangar};
}

void USurvivalPointsOfInterestSettings::FillDefaults_BattlefieldPools()
{
	using namespace SurvivalPOIDefaults;
	constexpr ESurvivalPOISeatMode Align = ESurvivalPOISeatMode::AlignToGround;
	constexpr ESurvivalPOISeatMode Upright = ESurvivalPOISeatMode::UprightOnGround;
	constexpr ESurvivalPOISeatMode Pivot = ESurvivalPOISeatMode::PivotOnGround;

	// The Soviet offensive that died in the basin: knocked-out Soviet armour, cover only.
	Pools.Add(MakePool(SurvivalPOIPools::SovietWreck, {
		MakeClass(DestTanks, TEXT("BP_Env_DestroyedT34"), 480.f, Align, 1.5f),
		MakeClass(DestTanks, TEXT("BP_Env_Destroyed_KV1E"), 520.f, Align, 1.f),
		MakeClass(DestTanks, TEXT("BP_Env_Destroyed_IS_3"), 540.f, Align, 0.6f),
	}));
	// Soviet hulls that still hold salvage (scavengeable, no burning effects: the variants made for placing in maps).
	const FString RusTracked = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/");
	Pools.Add(MakePool(SurvivalPOIPools::SovietScavengeWreck, {
		MakeClass(RusTracked + TEXT("Rus_Medium/T34/"), TEXT("Env_Dest_T34_85_NoFlame"), 450.f, Align, 1.5f),
		MakeClass(RusTracked + TEXT("Rus_Heavy/IS-1And-2/"), TEXT("Env_Dest_IS-2_NoFlame"), 500.f, Align, 0.8f),
		MakeClass(RusTracked + TEXT("Rus_Heavy/IS-1And-2/"), TEXT("Env_Dest_IS-1_NoFlame"), 480.f, Align, 0.8f),
		MakeClass(RusTracked + TEXT("Rus_Heavy/T28/"), TEXT("Env_Dest_T28_NoFlame"), 520.f, Align, 0.6f),
		MakeClass(RusTracked + TEXT("Rus_Medium/SU85/"), TEXT("Env_Dest_SU85_NoFlame"), 450.f, Align, 0.8f),
	}));
	// The German defenders' own losses: scavengeable hulks (metal, radixite, blueprints).
	Pools.Add(MakePool(SurvivalPOIPools::ScavengeTankWreck, {
		MakeClass(ScavTanks, TEXT("Light/PZ38T/ScavObj_Ger_Pz38T"), 450.f, Align, 1.f),
		MakeClass(ScavTanks, TEXT("Medium/Stug/ScavObj_Ger_Stug"), 500.f, Align, 1.f),
		MakeClass(ScavTanks, TEXT("Medium/PZIV/ScavObj_Ger_PZIV"), 500.f, Align, 1.f),
		MakeClass(ScavTanks, TEXT("Heavy/Tiger1/ScavObj_Ger_Tiger1"), 560.f, Align, 0.35f),
		MakeClass(RusTracked + TEXT("Ger_Medium/PanzerIII/"), TEXT("Env_Dest_PZIII_MNoFlame"), 480.f, Align, 0.7f),
	}));
	// Abandoned gun pits on the defenders' line; infantry capture them (ACaptureActor children).
	Pools.Add(MakePool(SurvivalPOIPools::GunPit, {
		MakeClass(GerPits, TEXT("BP_CaptureGerPit_Pak40"), 620.f, Pivot, 1.f, 12.f),
		MakeClass(GerPits, TEXT("BP_CaptureGerPit_Flak37"), 620.f, Pivot, 0.8f, 12.f),
		MakeClass(GerPits, TEXT("BP_CaptureGerPit_TwinMG42"), 560.f, Pivot, 1.f, 12.f),
		MakeClass(GerPits, TEXT("BP_CaptureGerPit_Mortar"), 560.f, Pivot, 0.8f, 12.f),
		MakeClass(GerPits, TEXT("BP_CaptureGerPit_Puppchen"), 560.f, Pivot, 0.6f, 12.f),
	}));
	// The pit Blueprints' guns point along their -X (round 1 views): turn them to face the composition's forward.
	for (FSurvivalPOIAsset& Pit : Pools.Last().Assets)
	{
		Pit.YawOffsetDegrees = 180.f;
	}
	Pools.Add(MakePool(SurvivalPOIPools::Sandbags, {
		MakeClass(Sandbags, TEXT("BP_Cover_RusSandbags_SingleStraight"), 260.f, Upright, 1.f, 12.f),
		MakeClass(Sandbags, TEXT("BP_Cover_RusSandbags_DoubleStraight"), 320.f, Upright, 1.f, 12.f),
		MakeClass(Sandbags, TEXT("BP_Cover_RusSandbags_LongArc"), 380.f, Upright, 0.7f, 12.f),
		MakeClass(Sandbags, TEXT("BP_Cover_RusSandbags_ZigZag"), 340.f, Upright, 0.6f, 12.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::ThrownTurret, {
		MakeMesh(TEXT("/Game/RTS_Survival/Blueprints/Turrets/Russian/Medium/T34Turrets/Meshes/"),
		         TEXT("SM_Destroyed_T3476Turret"), 230.f, true),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::Crater, {
		MakeClass(Craters, TEXT("BP_RTS_RandomDirtCraterDecal_Sandy"), 350.f, Pivot, 1.f, 30.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::SmallCrater, {
		MakeClass(Craters, TEXT("BP_RTS_RandomDirtCraterDecal_Small_Sandy"), 180.f, Pivot, 1.f, 30.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::AmmoDebris, {
		MakeMesh(AmmoMeshes, TEXT("SM_Ger_Ammobox"), 60.f, false),
		MakeMesh(AmmoMeshes, TEXT("SM_Ger_Ammobox_Long"), 80.f, false),
		MakeMesh(AmmoMeshes, TEXT("SM_AmmoCrateOpen"), 80.f, false),
		MakeMesh(AmmoMeshes, TEXT("AmmoCrate1"), 80.f, false, 0.7f),
	}));
	// Dirt and track decal materials read as dark or blue-grey slabs (round 1): the pool stays empty until a decal that
	// renders right is found; craters (Blueprint decals) carry the ground response.
	Pools.Add(MakePool(SurvivalPOIPools::Scorch, {}));
}

void USurvivalPointsOfInterestSettings::FillDefaults_OtherPools()
{
	using namespace SurvivalPOIDefaults;
	constexpr ESurvivalPOISeatMode Align = ESurvivalPOISeatMode::AlignToGround;
	constexpr ESurvivalPOISeatMode Upright = ESurvivalPOISeatMode::UprightOnGround;

	Pools.Add(MakePool(SurvivalPOIPools::VehicleParts, {
		MakeClass(VehicleParts, TEXT("ScavObj_VehicleParts1"), 330.f, Align, 1.f, 20.f),
		MakeClass(VehicleParts, TEXT("ScavObj_VehicleParts2"), 330.f, Align, 1.f, 20.f),
		MakeClass(VehicleParts, TEXT("ScavObj_VehicleParts3"), 330.f, Align, 1.f, 20.f),
		MakeClass(VehicleParts, TEXT("ScavObj_VehicleParts4"), 330.f, Align, 1.f, 20.f),
	}));
	// Visible meshes measured in round 1: +-860 cm square (the catalogue's 32 x 22 m is the unscaled mesh); door on +X.
	FSurvivalPOIAsset AirportHangar = MakeClass(Airport, TEXT("BP_Airport_Dest_Hangar"), 1250.f, Upright, 1.f, 5.f);
	AirportHangar.HalfExtent = FVector2D(900.0, 900.0);
	Pools.Add(MakePool(SurvivalPOIPools::Hangar, {AirportHangar}));
	// Abandoned on the apron: infantry capture the vehicle and drive it away (BP_CaptureActor.Player1ClassOnCapture).
	Pools.Add(MakePool(SurvivalPOIPools::CaptureVehicle, {
		MakeClass(Environment + TEXT("CaptureObjects/Vehicles/"), TEXT("BP_CaptureBrummbar"), 480.f, Align, 1.f, 10.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::HangarStores, {
		MakeClass(Airport, TEXT("BP_Airport_Dest_BarrelStack"), 260.f, Upright, 1.f, 8.f),
		MakeClass(Airport, TEXT("BP_Airport_Dest_BarrelStack2"), 260.f, Upright, 1.f, 8.f),
		MakeClass(Airport, TEXT("BP_Airport_Dest_BarrelStack3"), 260.f, Upright, 1.f, 8.f),
		MakeClass(Airport, TEXT("BP_Airport_Dest_CrateStack01"), 280.f, Upright, 1.f, 8.f),
		MakeClass(Airport, TEXT("BP_Airport_Dest_CrateStack02"), 280.f, Upright, 1.f, 8.f),
		MakeClass(Airport, TEXT("BP_Airport_Dest_CrateStack03"), 280.f, Upright, 0.7f, 8.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::CarWreck, {
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_1"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_2"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_3"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_4"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_5"), 380.f, Align, 1.f, 15.f),
	}));
	// Dirt and track decal materials read as dark or blue-grey slabs (round 1): the pool stays empty until a decal that
	// renders right is found; craters (Blueprint decals) carry the ground response.
	Pools.Add(MakePool(SurvivalPOIPools::ApronTracks, {}));
	FSurvivalPOIAsset DC3 = MakeMesh(Environment + TEXT("ScavengeObjects/CrashedPlanes/DC3/"), TEXT("SM_RTS_CrashedPlane_DC3"),
	                                 2300.f, true);
	DC3.MaxSlopeDegrees = 10.f;
	FSurvivalPOIAsset Avenger = MakeMesh(Environment + TEXT("ScavengeObjects/CrashedPlanes/Avenger/"),
	                                     TEXT("SM_RTS_CrashedPlane_Avenger"), 1500.f, true);
	Avenger.MaxSlopeDegrees = 10.f;
	Pools.Add(MakePool(SurvivalPOIPools::PlaneWreck, {DC3, Avenger}));
	const FString PlaneParts = TEXT("/Game/RTS_Survival/Meshes/Wrecks/PlaneParts/");
	// A torn-off wing section, left behind the fuselage on the scar.
	Pools.Add(MakePool(SurvivalPOIPools::PlaneLargeDebris, {MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_7"), 1200.f, true)}));
	Pools.Add(MakePool(SurvivalPOIPools::PlaneDebris, {
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_1"), 450.f, true),
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_2"), 350.f, true),
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_3"), 350.f, true),
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_4"), 350.f, true),
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_5"), 350.f, true),
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_6"), 350.f, true),
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_8"), 350.f, true),
		MakeMesh(PlaneParts, TEXT("SM_RTS_PlaneWreck_9"), 520.f, true),
	}));
	// The erosion-channel and tyre-track decal materials rendered as dark stretched strips under ADecalActor (round 1):
	// the scar is a chain of small crater Blueprints and churned dirt patches instead.
	Pools.Add(MakePool(SurvivalPOIPools::CrashScar, {
		MakeClass(Craters, TEXT("BP_RTS_RandomDirtCraterDecal_Small_Sandy"), 220.f, ESurvivalPOISeatMode::PivotOnGround, 1.f, 30.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::CrashScavenge, {
		MakeClass(VehicleParts, TEXT("ScavObj_VehicleParts1"), 330.f, Align, 1.f, 20.f),
		MakeClass(VehicleParts, TEXT("ScavObj_VehicleParts3"), 330.f, Align, 1.f, 20.f),
		MakeClass(Environment + TEXT("ScavengeObjects/Radixite/"), TEXT("BP_ScavObj_RadixiteDump"), 380.f, Align, 0.6f, 20.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::ConvoyEscort, {
		MakeClass(DestTanks, TEXT("BP_Env_DestroyedT34"), 480.f, Align, 1.f),
		MakeClass(DestTanks, TEXT("BP_Env_DestroyedPz38T"), 420.f, Align, 0.5f),
	}));
	const FString RusWheeled = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/");
	// The supply column: Soviet trucks (scavengeable) and a few requisitioned civilian cars.
	Pools.Add(MakePool(SurvivalPOIPools::ConvoyTruck, {
		WithTankReward(MakeClass(RusWheeled + TEXT("ZIS5V/"), TEXT("Env_Dest_ZIS5V"), 420.f, Align, 0.8f, 15.f), TEXT("Tank_ZilAmmo")),
		WithTankReward(MakeClass(RusWheeled + TEXT("ZilTroops/"), TEXT("Env_Dest_ZilTroops"), 480.f, Align, 1.f, 15.f), TEXT("Tank_ZilAmmo")),
		WithTankReward(MakeClass(RusWheeled + TEXT("ZilCargo/"), TEXT("Env_Dest_Zil_Fuel"), 450.f, Align, 1.f, 15.f), TEXT("Tank_ZilFuel")),
		WithTankReward(MakeClass(RusWheeled + TEXT("ZilCargo/"), TEXT("Env_Dest_Zil_Ammo"), 450.f, Align, 1.f, 15.f), TEXT("Tank_ZilAmmo")),
		WithTankReward(MakeClass(RusWheeled + TEXT("SovietTanker/"), TEXT("Env_Dest_SovietTanker"), 450.f, Align, 1.f, 15.f), TEXT("Tank_SovietTanker")),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::ConvoyVehicle, {
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_1"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_2"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_3"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_4"), 380.f, Align, 1.f, 15.f),
		MakeClass(VehicleParts + TEXT("Cars/"), TEXT("ScavObj_PostApo_Car_5"), 380.f, Align, 1.f, 15.f),
	}));
	// Dirt and track decal materials read as dark or blue-grey slabs (round 1): the pool stays empty until a decal that
	// renders right is found; craters (Blueprint decals) carry the ground response.
	Pools.Add(MakePool(SurvivalPOIPools::ConvoyTracks, {}));
}

void USurvivalPointsOfInterestSettings::FillDefaults_LandmarkPools()
{
	using namespace SurvivalPOIDefaults;
	constexpr ESurvivalPOISeatMode Upright = ESurvivalPOISeatMode::UprightOnGround;
	constexpr ESurvivalPOISeatMode Align = ESurvivalPOISeatMode::AlignToGround;
	const FString PostApo = Environment + TEXT("DestructableEnvActor/PostApo/");
	const FString Industrial = Environment + TEXT("DestructableEnvActor/Industrial/");
	// Pump station: a water tower over the dry well line, the pump engine, tanks, pipes and drums. Sizes: asset catalogue.
	Pools.Add(MakePool(SurvivalPOIPools::PumpTower, {MakeClass(PostApo, TEXT("BP_Dest_PostApo_WaterTower"), 260.f, Upright, 1.f, 5.f)}));
	Pools.Add(MakePool(SurvivalPOIPools::PumpMachinery, {
		MakeClass(PostApo, TEXT("BP_Dest_PostApo_Engine"), 380.f, Upright, 1.f, 6.f),
		MakeClass(PostApo, TEXT("BP_Dest_PostApo_WaterTank"), 300.f, Upright, 1.f, 6.f),
		MakeClass(Industrial, TEXT("BP_Industrial_Dest_OilTank1"), 300.f, Upright, 0.8f, 6.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::PumpPipes, {
		MakeClass(Industrial + TEXT("Pipes/"), TEXT("BP_Dest_Crushable_RusPipes1"), 380.f, Align, 1.f, 10.f),
		MakeClass(Industrial + TEXT("Pipes/"), TEXT("BP_Dest_Crushable_RusPipes2"), 380.f, Align, 1.f, 10.f),
	}));
	// The pipes run along their -Y from the pivot (measured): turn them to run along the composition's +X.
	for (FSurvivalPOIAsset& Pipe : Pools.Last().Assets)
	{
		Pipe.YawOffsetDegrees = 90.f;
	}
	Pools.Add(MakePool(SurvivalPOIPools::PumpBarrels, {
		MakeClass(Industrial, TEXT("BP_Industrial_Dest_OilTankBarrels1"), 420.f, Upright, 1.f, 6.f),
		MakeClass(Industrial, TEXT("BP_Industrial_Dest_OilTankBarrels2"), 420.f, Upright, 1.f, 6.f),
	}));
	// Relay station: where the power line turns, a wire-fenced yard round a steel mast; one mast already lies in the sand.
	Pools.Add(MakePool(SurvivalPOIPools::RelayMast, {
		MakeClass(Industrial + TEXT("SteelTowers/"), TEXT("BP_Dest_SteelTower_002"), 1000.f, Upright, 1.f, 5.f),
		MakeClass(Industrial + TEXT("SteelTowers/"), TEXT("BP_Dest_SteelTower_004"), 1050.f, Upright, 1.f, 5.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::RelayFence, {
		MakeClass(Environment + TEXT("DestructableEnvActor/Military/Barricades/"), TEXT("BP_Military_Dest_BarbedWireLong"), 120.f,
		          Align, 1.f, 15.f),
	}));
	Pools.Add(MakePool(SurvivalPOIPools::RelayPole, {
		MakeClass(Industrial + TEXT("Electricity/"), TEXT("BP_Electricity_SinglePole1"), 120.f, Upright, 1.f, 10.f),
	}));
	FSurvivalPOIAsset FallenMast = MakeClass(Environment + TEXT("ScavengeObjects/SteelTowers/"), TEXT("BP_ScavObj_SteelTower_001"),
	                                         1950.f, Align, 1.f, 8.f);
	FallenMast.HalfExtent = FVector2D(1935.0, 555.0);
	// The Blueprint carries no ScavengeResources: a 39 m steel mast is mostly metal.
	FallenMast.RewardOverride = {{TEXT("Resource_Metal"), 180}, {TEXT("Resource_VehicleParts"), 25}};
	Pools.Add(MakePool(SurvivalPOIPools::RelayFallenMast, {FallenMast}));
}

void USurvivalPointsOfInterestSettings::FillDefaults_Wildlife()
{
	using namespace SurvivalPOIDefaults;
	Wildlife.Reset();

	FSurvivalWildlifeEntry Goats;
	Goats.Name = TEXT("Goats");
	Goats.Kind = ESurvivalWildlifeKind::GroundHerd;
	Goats.Anchor = ESurvivalWildlifeAnchor::WaterBank;
	Goats.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath(Animated + TEXT("FarmAnimals/Goat/"), TEXT("BP_Wanderer_Goat"))));
	Goats.GroupCount = 3;
	Goats.GroupSize = FIntPoint(2, 5);
	// Groups drift over a wide patch of bank and bolt together (critic, round 1).
	Goats.WanderRadius = 1500.f;
	Goats.FleeDistance = 900.f;
	Goats.AlarmRadius = 1500.f;
	Goats.HerdSpread = 420.f;
	// Basin radius + 3 000: wander (1 500), herd spread (420) and a flight (900) still end outside the basin.
	Goats.WaterBankDistance = FVector2D(3000.0, 4600.0);
	Wildlife.Add(Goats);

	FSurvivalWildlifeEntry Ravens;
	Ravens.Name = TEXT("Ravens");
	Ravens.Kind = ESurvivalWildlifeKind::BirdFlock;
	Ravens.Anchor = ESurvivalWildlifeAnchor::OverPointOfInterest;
	Ravens.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath(Animated + TEXT("Birds/DesertFlocks/"), TEXT("BP_BirdFlock_InkRaven"))));
	Ravens.OverPointOfInterestTypes = {ESurvivalPOIType::SmallBattlefield, ESurvivalPOIType::TankGraveyard};
	Ravens.GroupCount = 4;
	Ravens.ChancePerPointOfInterest = 0.55f;
	Ravens.GroupSize = FIntPoint(4, 7);
	Ravens.FlightHeight = FVector2D(1300.0, 1900.0);
	Wildlife.Add(Ravens);

	FSurvivalWildlifeEntry Crows = Ravens;
	Crows.Name = TEXT("Crows");
	Crows.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath(Animated + TEXT("Birds/DesertFlocks/"), TEXT("BP_BirdFlock_Crow"))));
	Crows.OverPointOfInterestTypes = {ESurvivalPOIType::CrashSite, ESurvivalPOIType::DeadConvoy};
	Crows.GroupCount = 2;
	Crows.ChancePerPointOfInterest = 1.f;
	Crows.GroupSize = FIntPoint(5, 8);
	Wildlife.Add(Crows);

	FSurvivalWildlifeEntry Rollers;
	Rollers.Name = TEXT("CobaltRollers");
	Rollers.Kind = ESurvivalWildlifeKind::BirdFlock;
	Rollers.Anchor = ESurvivalWildlifeAnchor::WaterBank;
	Rollers.ActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath(Animated + TEXT("Birds/DesertFlocks/"), TEXT("BP_BirdFlock_CobaltRoller"))));
	Rollers.GroupCount = 1;
	Rollers.GroupSize = FIntPoint(3, 5);
	Rollers.FlightHeight = FVector2D(700.0, 1000.0);
	Rollers.SemiAxisX = FVector2D(2200.0, 2800.0);
	Rollers.SemiAxisY = FVector2D(1400.0, 1800.0);
	Rollers.WaterBankDistance = FVector2D(0.0, 600.0);
	Rollers.FlightSpeed = 800.f;
	Wildlife.Add(Rollers);

	FSurvivalWildlifeEntry Kites;
	Kites.Name = TEXT("Kites");
	Kites.Kind = ESurvivalWildlifeKind::NiagaraFlock;
	Kites.Anchor = ESurvivalWildlifeAnchor::OverSector;
	Kites.NiagaraSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(ObjectPath(Animated + TEXT("Birds/"), TEXT("NS_AzureDesertKite_Flock"))));
	Kites.GroupCount = 2;
	// Under the RTS camera (arm 5 200 is about 43 m up), so they are seen circling.
	Kites.FlightHeight = FVector2D(1500.0, 2500.0);
	Wildlife.Add(Kites);
}

const FSurvivalPOIAssetPool* USurvivalPointsOfInterestSettings::FindPool(const FName PoolName) const
{
	const int32 Index = FindPoolIndex(PoolName);
	return Pools.IsValidIndex(Index) ? &Pools[Index] : nullptr;
}

int32 USurvivalPointsOfInterestSettings::FindPoolIndex(const FName PoolName) const
{
	return Pools.IndexOfByPredicate([PoolName](const FSurvivalPOIAssetPool& Pool) { return Pool.PoolName == PoolName; });
}

const FSurvivalPOIAsset* USurvivalPointsOfInterestSettings::GetAssetByKey(const int32 AssetKey) const
{
	if (AssetKey < 0)
	{
		return nullptr;
	}
	const int32 PoolIndex = AssetKey / FSurvivalPOIPlacedObject::AssetKeyStride;
	const int32 AssetIndex = AssetKey % FSurvivalPOIPlacedObject::AssetKeyStride;
	if (not Pools.IsValidIndex(PoolIndex) or not Pools[PoolIndex].Assets.IsValidIndex(AssetIndex))
	{
		return nullptr;
	}
	return &Pools[PoolIndex].Assets[AssetIndex];
}

const FSurvivalPOITypeRules* USurvivalPointsOfInterestSettings::FindTypeRules(const ESurvivalPOIType Type) const
{
	return TypeRules.FindByPredicate([Type](const FSurvivalPOITypeRules& Rules) { return Rules.Type == Type; });
}

// ---------------------------------------------------------------------------

FSoftObjectPath FSurvivalPOIAsset::GetLoadPath() const
{
	if (not ActorClass.IsNull())
	{
		return ActorClass.ToSoftObjectPath();
	}
	if (not Mesh.IsNull())
	{
		return Mesh.ToSoftObjectPath();
	}
	return DecalMaterial.ToSoftObjectPath();
}

FString FSurvivalPOIAsset::GetDisplayPath() const
{
	return GetLoadPath().GetAssetName();
}

namespace SurvivalPOINames
{
	FString GetTypeName(const ESurvivalPOIType Type)
	{
		switch (Type)
		{
		case ESurvivalPOIType::SmallBattlefield: return TEXT("battlefield");
		case ESurvivalPOIType::RuinedHangar: return TEXT("hangar");
		case ESurvivalPOIType::CrashSite: return TEXT("crashsite");
		case ESurvivalPOIType::DeadConvoy: return TEXT("convoy");
		case ESurvivalPOIType::TankGraveyard: return TEXT("graveyard");
		case ESurvivalPOIType::PumpStation: return TEXT("pumpstation");
		case ESurvivalPOIType::RelayStation: return TEXT("relaystation");
		default: return TEXT("none");
		}
	}

	FString GetRoleName(const ESurvivalPOIRole Role)
	{
		switch (Role)
		{
		case ESurvivalPOIRole::Hero: return TEXT("hero");
		case ESurvivalPOIRole::Scavenge: return TEXT("scavenge");
		case ESurvivalPOIRole::Capture: return TEXT("capture");
		case ESurvivalPOIRole::Cover: return TEXT("cover");
		case ESurvivalPOIRole::Debris: return TEXT("debris");
		case ESurvivalPOIRole::Decal: return TEXT("decal");
		case ESurvivalPOIRole::Crater: return TEXT("crater");
		default: return TEXT("unknown");
		}
	}

	bool GetIsLandmarkType(const ESurvivalPOIType Type)
	{
		return Type == ESurvivalPOIType::CrashSite or Type == ESurvivalPOIType::DeadConvoy
			or Type == ESurvivalPOIType::TankGraveyard or Type == ESurvivalPOIType::PumpStation
			or Type == ESurvivalPOIType::RelayStation;
	}
}
