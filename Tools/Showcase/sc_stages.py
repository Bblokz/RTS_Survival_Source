"""Stage definitions of the showcase map: where each stage sits, its ground, its units and its camera presets.

Stage frame: +u points from the cameras to the backdrop, v sideways (+v to the right seen from the cameras).
Vehicles face the cameras (yaw 180 in the stage frame) unless a formation says otherwise. Units are named by asset
name (sc_common.find_asset resolves the folder), so variant sub folders cannot break a roster.

Hero stages (WoT-ad style, at most 12 vehicles on a detailed landscape): HeroSteppe (Soviet), HeroAlpine
(Breakthrough), HeroMesa (Strike Division). Overviews: SovietArmy, SovietIndustrial, ForgeArmy, StrikeDivision,
Breakthrough.
"""
import math
import random

import unreal

import sc_common
from sc_common import smoothstep

ENV = sc_common.ENV
MESHES = sc_common.MESHES
BRUSHIFY_MOUNTAINS = ["/Game/Brushify/DistanceMeshes/Desert_Mountain_0%d/S_Desert_Mountain_0%d" % (i, i) for i in range(1, 5)]
BRUSHIFY_MOUNTAIN_MI = "/Game/Brushify/DistanceMeshes/Desert_Mountain_0{index}/MI_Desert_Mountain_0{index}_{kind}"
WPO = ENV + "WpoTreeFoliage/"
SPRUCES = [WPO + "Spruces/BP_WpoSpruce_1", WPO + "Spruces/BP_WpoSpruce_Tall", WPO + "Spruces/BP_WpoSpruce_Small",
           WPO + "Firs/BP_WpoFir", WPO + "Firs/BP_WpoFir_Small"]
DARK_SPRUCES = [WPO + "Spruces/BP_WpoSpruce_TallDark", WPO + "Spruces/BP_WpoSpruce_SmallDark", WPO + "Spruces/BP_WpoSpruce_1"]
LEAF_TREES = [WPO + "LeafTrees/BP_WpoLeafTree_Large", WPO + "SmallTrees/BP_WpoLeafTreeSmall", WPO + "LeafTrees/BP_WpoBush_1",
              WPO + "LeafTrees/BP_WpoBush_2"]
ORCHARD = [ENV + "OrchardTrees/BP_OrchardTree_" + kind for kind in ("Apple_A", "Apple_B", "Pear_A", "Pear_B", "Cherry_A")]
DEAD_TREES = [MESHES + "Nature/Trees/DeadTrees/SM_RTS_DeadTree_" + kind for kind in ("1", "2", "04", "05", "06")]
STONES = [ENV + "GroundStones/Meshes/SM_GS_" + kind for kind in ("Chunk_A", "Chunk_B", "Chunk_C", "Chunk_D", "Cluster_A")]
DESERT_ROCKS = [MESHES + "Desert/Rocks/SM_Rock_0%d" % index for index in range(1, 8)]
CACTUS_BUSHES = [MESHES + "Desert/Cacti/CactusBush/SM_RTS_CactusBush_%s_%s_0%d" % (size, colour, index)
                 for size in ("Large", "Medium", "Small") for colour in ("Green",) for index in (1, 2, 3)]
DEAD_SHRUBS = ["/Game/Megascans/3D_Plants/Deadshrub-01_vdknafgha/S_Deadshrub-01_vdknafgha_Var%d_lod0" % index
               for index in (1, 2, 4, 5, 6, 8, 9, 10, 11, 12)]
MEADOW_SHRUBS = (["/Game/Megascans/3D_Plants/Elderberry_wfzobb2ia/S_Elderberry_wfzobb2ia_Var%d_lod3" % index
                  for index in (1, 2, 3, 10, 11)] +
                 ["/Game/Megascans/3D_Plants/Dwarf_Birch_xblnaabqx/S_Dwarf_Birch_xblnaabqx_Var%d_lod0" % index
                  for index in (10, 11, 12, 13)])
LOGS = [MESHES + "Nature/Trees/DeadTrees/Logs/SM_RTS_DeadTree_Log_0%d" % index for index in range(1, 10)]
FARM = ENV + "DestructableEnvActor/Farm/BP_Farm_Dest_"
SCORCH_POWERLINE = ENV + "DestructableEnvActor/BP_Scorch_Dest_Powerline_1"
CRATERS = ["/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d" % index for index in range(1, 7)]
SANDY_CRATERS = ["/Game/RTS_Survival/Materials/Decals/CraterDirt/MI_Decal_Crater_Dirt_0%d_Sandy" % index for index in range(1, 7)]
SCORCH_DECALS = ["/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/MI_RTS_Decal_Scorch_01",
                 "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/MI_RTS_Decal_Scorch_02",
                 "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteDecals/MI_RTS_Decal_Crater_Scorched_Scatter_01"]
TRACK_DECALS = ["/Game/RTS_Survival/Materials/Decals/DirtTracks/MI_Decal_Dirt_TracksSandy_0%d" % index for index in (1, 2, 3)]
PATH_DECALS = ["/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Decals/MI_PathDecal_MudChurn",
               "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Decals/MI_PathDecal_RutFade"]
SAND_GROUND_MATERIAL = "/Game/RTS_Survival/Meshes/PresetRegion_PlayerStart/AirfieldGround/Materials/M_RTSGround_WithSand"
YARD_PROPS = [ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_Container",
              ENV + "DestructableEnvActor/PostApo/BP_Dest_PostApo_ContainerBox1",
              ENV + "DestructableEnvActor/PostApo/BP_Dest_PostApo_ContainerBox2",
              ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_OilTankBarrels1",
              ENV + "DestructableEnvActor/Industrial/BP_Industrial_Dest_OilTankBarrels2",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_BarrelStack2",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_BarrelStack3",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_CrateStack01",
              ENV + "DestructableEnvActor/Airport/BP_Airport_Dest_CrateStack03"]
YARD_DECALS = ["/Game/RTS_Survival/Meshes/Industrial/Zone/Materials/Decals/MI_Decals_0%d" % index for index in (1, 2, 3, 4, 5, 6, 7, 8)]
YARD_SLAB = "/Game/RTS_Survival/Blueprints/Environment/GroundEnvActors/Airport/BP_Airport_Ground1"
BRUSHIFY_CLIFFS = ["/Game/Brushify/Meshes/Cliffs/Meshes/Cliff_Large_01/S_Cliff_Large_01",
                   "/Game/Brushify/Meshes/Cliffs/Meshes/Cliff_Large_02/S_Cliff_Large_02"]
CROPS = "/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/FarmCrops/"
INDUSTRIAL_GROUNDS = ["/Game/IndustrialZone_Buildings/Blueprints/BP_Ground_A", "/Game/IndustrialZone_Buildings/Blueprints/BP_Ground_B",
                      "/Game/IndustrialZone_Buildings/Blueprints/BP_Ground_C"]
RUS_BUILDINGS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/"


# -- rosters (asset names) --------------------------------------------------------------------------------------------
SD_ROWS = [
    ("Experimental", ["Ger_Vehicle_E100", "Ger_Vehicle_SturmTiger", "Ger_Vehicle_SturmTiger_Scatterer", "Ger_Vehicle_PantherII",
                      "Ger_Vehicle_PantherD", "Ger_Vehicle_JagdPanther", "Ger_Vehicle_JagdPanther_Bollwerk",
                      "Ger_Vehicle_JagdPanther_Flamer", "Ger_Vehicle_JagdPanther_Pfeil", "Ger_Vehicle_E25",
                      "Ger_Vehicle_E25_Assault", "Ger_Vehicle_E25_Hunter"]),
    ("Medium", ["Ger_Vehicle_StugIII", "Ger_Vehicle_Jaguar", "Ger_Vehicle_Jaguar_Flanker", "Ger_Vehicle_Hetzer",
                "Ger_Vehicle_Hetzer_Flame", "Ger_Vehicle_Hetzer_Mortar", "Ger_Vehicle_Wespe", "Ger_Vehicle_Wespe_CamouflagedNest",
                "Ger_Vehicle_Wespe_RadixiteHowitzer", "Ger_Vehicle_PanzerIII_M", "Ger_Vehicle_PanzerIII_M_Overcharged",
                "Ger_Vehicle_PanzerIII_M_SpacedArmor", "Ger_Vehicle_PanzerIII_Flamm"]),
    ("Light", ["Ger_Vehicle_PanzerII_F", "Ger_Vehicle_PanzerII_F_RadixiteAutocannon", "Ger_Vehicle_PanzerII_F_SpacedArmor",
               "Ger_Vehicle_PanzerII_F_Strike", "Ger_Vehicle_PanzerII_HFire", "Ger_ArmoredCar_Puma", "Ger_ArmoredCar_Puma_DualBarrel",
               "Ger_ArmoredCar_Puma_OverchargedBeam", "Ger_ArmoredCar_Puma_RadixiteSniper", "Ger_ArmoredCar_FlamePuma",
               "Ger_Vehicle_PanzerJager_Laser", "Ger_ArmoredCar_Sdkfz_234_3", "Ger_ArmoredCar_Sdkfz_234_3_IncendiarySupport",
               "Ger_ArmoredCar_Sdkfz_234_3_StummelAssault", "Ger_Vehicle_PanzerIII_J", "Ger_Vehicle_PanzerIII_J_RadixiteCannon",
               "Ger_Vehicle_PanzerIII_J_RocketLauncher", "Ger_Vehicle_PanzerIII_AA", "Ger_Vehicle_PanzerIII_AA_Drilling",
               "Ger_Vehicle_PanzerIII_AA_Flak41", "Ger_Vehicle_Panzer38T_R", "Ger_Vehicle_Panzer38T_R_IncendiarySalvo",
               "Ger_Vehicle_Panzer38T_R_PrecisionMissile"]),
    ("Mechanized", ["Ger_ArmoredCar_251_Transport", "Ger_ArmoredCar_251_AmmoCarrier", "Ger_ArmoredCar_251_GuidedMissile",
                    "Ger_ArmoredCar_251_Incendiary", "Ger_ArmoredCar_251_Medic", "Ger_ArmoredCar_251_MortarTW",
                    "Ger_ArmoredCar_Sdkfz-251_75mm", "Ger_ArmoredCar_Sdkfz-251_75mm_HeavyAA",
                    "Ger_ArmoredCar_Sdkfz-251_75mm_RadixiteCharged", "Ger_Vehicle_SDKFZ-140", "Ger_Vehicle_SDKFZ-140_Rockets",
                    "Ger_Vehicle_SDKFZ-140_AA", "Ger_Vehicle_SDKFZ-140_Bunker", "Ger_ArmoredCar_Sdkfz_231",
                    "Ger_ArmoredCar_Sdkfz_231_MG231", "Ger_Harvester_Panzer1"]),
]
SD_SQUADS = ["Squad_Ger_Scavengers", "Squad_Ger_JagerTrupp", "Squad_Ger_SturmPioneers", "Squad_Ger_HazmatVultures",
             "Squad_Ger_IronStorm", "Squad_Ger_SturmCommando", "Squad_Ger_FemaleSniper", "Squad_Ger_Faust",
             "Squad_Ger_PanzerGrenadiers", "Squad_Ger_HazmatSniperTeam", "Squad_Ger_LightBringers"]
SD_TEAM_WEAPONS = ["Ger_SquadTW_Flak37MM", "Ger_SquadTW_Pak38", "Ger_SquadTW_80mmMortar", "Ger_SquadTW_Sfh18",
                   "Ger_SquadTW_Flak88", "Ger_SquadTW_Morser210"]
GER_PITS = ["Ger_BXP_GerPit_Pak40", "Ger_BXP_GerPit_Flak37", "Ger_BXP_GerPit_Mortar", "Ger_BXP_GerPit_Puppchen",
            "Ger_BXP_GerPit_LeFH18", "Ger_BXP_GerPit_SFH18", "Ger_BXP_GerPit_Panzerschreck", "Ger_BXP_GerPit_Flak88",
            "Ger_BXP_GerPit_TwinMG42"]
SD_BUILDINGS = ["Ger_HQ_Truck", "Ger_Barracks_Truck", "Ger_MechanizedDepot_Truck", "Ger_Forge_Truck", "Ger_MedFactory_Truck",
                "Ger_Experimental_Truck", "Ger_Airbase_Truck", "Ger_Armory_Truck", "Ger_CommunicationCenter_Truck",
                "Ger_Refinery_Truck", "Ger_MetalVault_Truck", "Ger_GammaFacility_Truck"]
GER_AIRCRAFT = ["Ger_Aircraft_BF110Airborne", "Ger_Aircraft_StukaJu87_Airborne", "Ger_Aircraft_ME5_Airbrone",
                "Ger_Aircraft_Horten229_Airborne"]

BT_ROWS = [
    ("Experimental", ["Ger_Vehicle_Maus", "Ger_Vehicle_Maus_AA", "Ger_Vehicle_TigerII", "Ger_Vehicle_TigerII_105",
                      "Ger_Vehicle_TigerII_Rocketeer", "Ger_Vehicle_E75", "Ger_Vehicle_E50", "Ger_Vehicle_JagdTiger",
                      "Ger_Vehicle_JagdTiger_Scrap128", "Ger_Vehicle_TigerH1", "Ger_Vehicle_TigerH1_ASPS", "Ger_Vehicle_TigerH1_TwinGun",
                      "Ger_Vehicle_TigerE", "Ger_Vehicle_TigerH1_Rail", "Ger_Vehicle_TigerH1_Rail_Overcharged",
                      "Ger_Vehicle_TigerH1_Rail_RailBunker"]),
    ("Medium", ["Ger_Vehicle_PanzerIV_H", "Ger_Vehicle_PanzerIV_H_StandardPlus", "Ger_Vehicle_PanzerIV_H_Ostwind",
                "Ger_Vehicle_PanzerIV_G", "Ger_Vehicle_PanzerIV_G_DoubleTurret", "Ger_Vehicle_JagdPanzerIV70",
                "Ger_Vehicle_JagdPanzerIV70_Ambush", "Ger_Vehicle_JagdPanzerIV70_Siege", "Ger_Vehicle_Brummbar",
                "Ger_Vehicle_Brummbar_DirectSiege", "Ger_Vehicle_Brummbar_LateWar", "Ger_Vehicle_PanzerIV_Rocket",
                "Ger_Vehicle_PanzerIV_Rocket_IncendiarySalvo", "Ger_Vehicle_PanzerIV_Rocket_RadixiteHail", "Ger_Vehicle_Marder",
                "Ger_Vehicle_Marder_DualBarrel"]),
    ("Light", ["Ger_Vehicle_PanzerIV_F1", "Ger_Vehicle_PanzerIV_F1_Assault", "Ger_Vehicle_PanzerIV_F1_Spotter",
               "Ger_Vehicle_PanzerII_Luchs", "Ger_Vehicle_PanzerII_Luchs_FlakLuchs", "Ger_Vehicle_PanzerII_Luchs_SpacedArmor",
               "Ger_Vehicle_PanzerJager1", "Ger_Vehicle_PanzerJager1_AmbushShield", "Ger_Vehicle_PanzerJager1_MobileBattery",
               "Ger_Vehicle_Pz1_15CM", "Ger_Vehicle_Pz1_15CM_DirectFire", "Ger_Vehicle_Pz1_15CM_IndirectFire",
               "Ger_Vehicle_Panzer38_RAIL", "Ger_Vehicle_Panzer38_RAIL_Overcharge", "Ger_Vehicle_Panzer38_RAIL_Sniper",
               "Ger_Vehicle_Sdkfz7Flak", "Ger_Vehicle_Sdkfz7Flak_Flak36", "Ger_Vehicle_Sdkfz7Flak_SearchlightTruck"]),
    ("Mechanized", ["Ger_ArmoredCar_Sdkfz250", "Ger_ArmoredCar_Sdkfz250_AmmoCarrier", "Ger_ArmoredCar_Sdkfz250_ArmouredAmbulance",
                    "Ger_ArmoredCar_Sdkfz250_37", "Ger_ArmoredCar_Sdkfz250_37_Howitzer250", "Ger_ArmoredCar_Sdkfz250_37_LongBarrel",
                    "Ger_Vehicle_Panzer38T", "Ger_Vehicle_Panzer38T_ATAmmoStock", "Ger_Vehicle_Panzer38T_Rocketeer",
                    "Ger_Vehicle_SDKFZ-140", "Ger_ArmoredCar_Sdkfz_231", "Ger_Harvester_Panzer1"]),
]
BT_SQUADS = ["Squad_Ger_Scavengers", "Squad_Ger_JagerTrupp", "Squad_Ger_SturmPioneers", "Squad_Ger_HazmatVultures",
             "Squad_Ger_FlameTroopers", "Squad_Ger_SturmCommando", "Squad_Ger_FemaleSniper", "Squad_Ger_Faust",
             "Squad_Ger_PanzerGrenadiers", "Squad_Ger_HazmatSniperTeam", "Squad_Ger_LightBringers", "Squad_Ger_LMG"]
BT_TEAM_WEAPONS = ["Ger_SquadTW_Flak37MM", "Ger_SquadTW_Pak38", "Ger_SquadTW_Pak40_75MM", "Ger_SquadTW_80mmMortar",
                   "Ger_SquadTW_Lefh", "Ger_SquadTW_Flak88", "Ger_SquadTW_Morser210"]
BT_BUNKERS = ["Ger_BXP_Flak88Bunker", "Ger_BXP_FlakvierlingBunker", "Ger_BXP_RailPakBunker"]
BT_BUILDING_TRUCKS = ["Ger_HQ_Truck_Breakthrough", "BF_Armory_Truck", "BF_MechanizedDepot_Truck", "BF_ExperimentalFactory_Truck"]
BT_BUILDING_MESHES = ["SM_BF_LightFactory", "SM_BF_MediumFactory", "SM_BF_OxideWorks"]

RUS_TANKS = ["Rus_Vehicle_IS-3", "Rus_Vehicle_IS-2", "Rus_Vehicle_IS-1", "Rus_Vehicle_IS6", "Rus_Vehicle_IS-3_Flame",
             "Rus_Vehicle_KV-5", "Rus_Vehicle_KV3", "Rus_Vehicle_KV_IS", "Rus_Vehicle_KV-2", "Rus_Vehicle_KV-1E",
             "Rus_Vehicle_KV-1_L11", "Rus_Vehicle_KV-1Arc", "Rus_Vehicle_SMK", "Rus_Vehicle_T-35", "Rus_Vehicle_T-28",
             "Rus_Vehicle_T-28Flame", "Rus_Vehicle_ISU152", "Rus_Vehicle_ISU130", "Rus_Vehicle_Su122", "Rus_Vehicle_Su100",
             "Rus_Vehicle_Su85", "Rus_Vehicle_T44_100L", "Rus_Vehicle_T44_155", "Rus_Vehicle_T3485", "Rus_Vehicle_T3476",
             "Rus_Vehicle_T34E", "Rus_Vehicle_T34L", "Rus_Vehicle_T34100", "Rus_Vehicle_T34AA", "Rus_Vehicle_T50",
             "Rus_Vehicle_T-70", "Rus_Vehicle_T-70Flame", "Rus_Vehicle_T26", "Rus_Vehicle_BT-7", "Rus_Vehicle_BT-7-4",
             "Rus_Vehicle_MS1", "Rus_Vehicle_SU76", "Rus_Harvester_T34Breaker"]
RUS_TRUCKS = ["Rus_ArmoredCar_BM13", "Rus_ArmoredCar_GAZMM72K", "Rus_ArmoredCar_ZIS5V", "Rus_ArmoredCar_ZilAmmo",
              "Rus_ArmoredCar_ZilFuel", "Rus_ArmoredCar_ZilTroops", "Rus_ArmoredCar_SovietTanker", "Rus_ArmoredCar_BTR152A",
              "Rus_ArmoredCar_BA11", "Rus_ArmoredCar_BA14"]
RUS_SQUADS = ["Rus_Squad_Mosin", "Rus_Squad_PTRS", "Rus_Squad_SVT", "Rus_Squad_Fedrov", "Rus_Squad_KorovinAssault",
              "Rus_Squad_FlameROKS", "Rus_Squad_DynakonovGrenade", "Rus_Squad_HazmatBazooka", "Rus_Squad_HazmatEngineers",
              "Rus_T2Squad_RedHammer", "Rus_T2Squad_ToxicGuard", "Rus_Squad_Elite_CortexOfficer", "Rus_Squad_Elite_Kvarc77_LaserAT"]
RUS_TEAM_WEAPONS = ["Rus_TWSquad_53K_45mm", "Rus_TWSquad_Zis3_76", "Rus_TWSquad_BS3_100mm", "Rus_TWSquad_Maxim",
                    "Rus_SquadTW_80mmMortar", "Rus_SquadTW_120mmMortar", "Rus_SquadTW_M1938", "Rus_SquadTW_ML20", "Rus_SquadTW_B4"]
RUS_AIRCRAFT = ["Showcase_Rus_PE8_Airborne", "Showcase_Rus_PE2_Airborne", "Showcase_Rus_Sturmovic_Airborne"]
RUS_BACKDROP = ["Rus_Building_Factory", "Rus_Building_Barracks", "Rus_Building_ResearchCenter", "Rus_Building_PlatformFactory",
                "Rus_Building_AmmoStorage", "Rus_Building_CoolingTowers", "Rus_Building_Supplies", "Rus_Building_AdminBunker"]
RUS_FORTS = ["Rus_BXP_GuardTowerMG", "Rus_BXP_MGBunker", "Rus_BXP_DomeBunker", "Rus_BXP_LargeGuardTower", "Rus_BXP_Bunker05"]

FORGE_VEHICLES = ["Rus_Vehicle_IS-3_Soviet", "Rus_Vehicle_IS-2_Soviet", "Rus_Vehicle_IS-1_Soviet", "Rus_Vehicle_KV-1Arc_Soviet",
                  "Rus_Vehicle_KV-1_L11_Soviet", "Rus_Vehicle_KV-1E_Soviet", "Rus_Vehicle_T-35_Soviet", "Rus_Vehicle_Su100_Soviet",
                  "Rus_Vehicle_Su85_Soviet", "Rus_Vehicle_T34100_Soviet", "Rus_Vehicle_T3485_Soviet", "Rus_Vehicle_T34E_Soviet",
                  "Rus_Vehicle_T3476_Soviet", "Rus_Vehicle_T3476_Soviet_FieldKit", "Rus_Vehicle_T34AA_Soviet",
                  "Rus_Vehicle_T50_Soviet", "Rus_Vehicle_T-70_Soviet", "Rus_Vehicle_T26_Soviet", "Rus_Vehicle_BT-7_Soviet",
                  "Rus_Vehicle_BT-7-4_Soviet", "Rus_Vehicle_MS1_Soviet", "Rus_ArmoredCar_BM13_Forge",
                  "Rus_ArmoredCar_GAZMM72K_Forge", "Rus_ArmoredCar_SovietTanker_Forge"]
FORGE_BACKDROP = ["Rus_Building_CoolingTowers_Soviet", "Rus_Building_LargeTent_Soviet", "Rus_BXP_RusBunker03_Soviet",
                  "Rus_BXP_GuardTowerMG_Soviet"]

HERO_SOVIET = ["Rus_Vehicle_IS-3", "Rus_Vehicle_KV-2", "Rus_Vehicle_IS6", "Rus_Vehicle_T3485", "Rus_Vehicle_ISU152",
               "Rus_Vehicle_KV-5", "Rus_Vehicle_Su100", "Rus_Vehicle_T44_100L", "Rus_Vehicle_KV-1E", "Rus_Vehicle_IS-2"]
HERO_BREAKTHROUGH = ["Ger_Vehicle_TigerII", "Ger_Vehicle_Maus", "Ger_Vehicle_E75", "Ger_Vehicle_TigerH1", "Ger_Vehicle_JagdTiger",
                     "Ger_Vehicle_E50", "Ger_Vehicle_PanzerIV_H", "Ger_Vehicle_Brummbar", "Ger_Vehicle_JagdPanzerIV70",
                     "Ger_Vehicle_TigerII_105"]
HERO_STRIKE = ["Ger_Vehicle_E100", "Ger_Vehicle_PantherII", "Ger_Vehicle_JagdPanther", "Ger_Vehicle_SturmTiger",
               "Ger_Vehicle_PantherD", "Ger_Vehicle_E25", "Ger_Vehicle_Jaguar", "Ger_Vehicle_PanzerIII_M", "Ger_Vehicle_StugIII",
               "Ger_Vehicle_Hetzer"]


# -- unit helpers ------------------------------------------------------------------------------------------------------
def place_vehicle(stage, name, label, u, v, yaw=180.0, folder="Vehicles"):
    return stage.blueprint(name, label, u, v, yaw, folder, z=10.0)


def place_squad(stage, name, label, u, v, yaw=180.0, folder="Infantry"):
    return stage.blueprint(name, label, u, v, yaw, folder, z=20.0)


def place_building_truck(stage, name, label, u, v, yaw=180.0, folder="Buildings"):
    actor = stage.blueprint(name, label, u, v, yaw, folder, z=10.0)
    if actor is not None:
        actor.set_editor_property("start_as_converted_to_building", True)
    return actor


def place_aircraft(stage, name, label, u, v, height, yaw=180.0, folder="Aircraft"):
    # Only *_Airborne Blueprints start in the air: bM_StartedAsLanded is not editable on instances.
    return stage.blueprint(name, label, u, v, yaw, folder, absolute_z=stage.ground(u, v) + height)


def grid(stage, names, label, front_u, v_spacing, u_spacing, per_row, yaw=180.0, folder="Vehicles", place=place_vehicle,
         stagger=0.0, centre_v=0.0, jitter=0.0):
    """Rows starting at front_u going back (+u), each row centred on centre_v. Returns the u after the last row."""
    rng = random.Random(len(names) * 13 + int(front_u))
    for index, name in enumerate(names):
        row, column = divmod(index, per_row)
        in_row = min(per_row, len(names) - row * per_row)
        v = centre_v + (column - (in_row - 1) / 2.0) * v_spacing + (stagger if row % 2 else 0.0)
        u = front_u + row * u_spacing
        turn = rng.uniform(-2.0, 2.0) if jitter else 0.0
        place(stage, name, "%s_%02d" % (label, index), u + rng.uniform(-jitter, jitter), v + rng.uniform(-jitter, jitter),
              yaw + turn, folder)
    rows = (len(names) + per_row - 1) // per_row
    return front_u + rows * u_spacing


def cluster(stage, names, label, centre, facing=0.0, per_row=4, spacing=1350.0, depth=1500.0, jitter=180.0,
            place=None, folder="Vehicles", seed=1):
    """A loose company block: rows of per_row units around centre (u, v), the whole block turned by `facing` degrees,
    each unit jittered in place and heading so the army reads as parked groups, not a parade grid."""
    place = place or place_vehicle
    frame = hero_frame(centre, facing)
    rng = random.Random(seed)
    rows = (len(names) + per_row - 1) // per_row
    for index, name in enumerate(names):
        row, column = divmod(index, per_row)
        in_row = min(per_row, len(names) - row * per_row)
        a = (row - (rows - 1) / 2.0) * depth + rng.uniform(-jitter, jitter)
        b = (column - (in_row - 1) / 2.0) * spacing + (spacing * 0.5 if row % 2 else 0.0) + rng.uniform(-jitter, jitter)
        u, v = frame(a, b)
        place(stage, name, "%s_%02d" % (label, index), u, v, 180.0 + facing + rng.uniform(-9.0, 9.0), folder)


def hero_frame(origin=(0.0, 0.0), facing=0.0):
    """Maps hero coordinates (a: depth away from the hero cameras, b: sideways) into the stage frame. facing 0 keeps
    the stage axes (cameras at -u looking +u); -90 makes the cameras look towards -v."""
    theta = math.radians(facing)
    return lambda a, b: (origin[0] + a * math.cos(theta) - b * math.sin(theta),
                         origin[1] + a * math.sin(theta) + b * math.cos(theta))


def wedge(stage, names, label, frame=None, spread=720.0, depth=1000.0, facing=0.0, folder="Vehicles", seed=3):
    """WoT-key-art arrowhead (reference 3): the first vehicle is the tip, nearest the cameras; the others alternate
    right and left, each pair one rank (about 10 m) further back, turned a little outwards and jittered so hulls
    never line up nose to tail."""
    frame = frame or hero_frame(facing=facing)
    rng = random.Random(seed)
    placed = []
    for index, name in enumerate(names):
        rank = (index + 1) // 2
        side = 0 if index == 0 else (1 if index % 2 else -1)
        u, v = frame(rank * depth + rng.uniform(-120.0, 120.0), side * rank * spread + side * rng.uniform(-80.0, 80.0))
        yaw = 180.0 + facing - side * min(18.0, rank * 4.0) + rng.uniform(-6.0, 6.0)
        place_vehicle(stage, name, "%s_%02d" % (label, index), u, v, yaw, folder)
        placed.append((u, v))
    return placed


def mountains(stage, kind, count, distance=(260000.0, 380000.0), height=(14000.0, 30000.0), spread_deg=130.0,
              centre_deg=0.0, sink=0.3, seed=5, skirt=True):
    """Brushify distance mountains on an arc behind the stage (+u), 2.5-4 km out, sized from their mesh bounds so the
    visible part rises `height` cm over the stage; kind picks the material (Snow, Desert, DesertMountains, Grasslands,
    Rocky). A plane in the same material fills the ground up to them."""
    rng = random.Random(seed)
    base = stage.terrain.base
    for index in range(count):
        mesh_index = index % 4 + 1
        mesh = unreal.load_asset(BRUSHIFY_MOUNTAINS[mesh_index - 1])
        bounds = mesh.get_bounds()
        target_height = rng.uniform(*height)
        scale = target_height / (2.0 * bounds.box_extent.z * (1.0 - sink))
        half_width = max(bounds.box_extent.x, bounds.box_extent.y) * scale
        angle = math.radians(centre_deg + (index / max(1, count - 1) - 0.5) * spread_deg + rng.uniform(-5, 5))
        radius = max(rng.uniform(*distance), stage.terrain.half + half_width * 0.9)
        u, v = radius * math.cos(angle), radius * math.sin(angle)
        bottom = (bounds.origin.z - bounds.box_extent.z) * scale
        z = base - bottom - 2.0 * bounds.box_extent.z * scale * sink
        stage.mesh(BRUSHIFY_MOUNTAINS[mesh_index - 1], "Mountain_%02d" % index, u, v, yaw=rng.uniform(0, 360),
                   folder="Backdrop", scale3d=(scale, scale, scale * rng.uniform(0.85, 1.15)), z=z,
                   material=BRUSHIFY_MOUNTAIN_MI.format(index=mesh_index, kind=kind))
    if skirt:
        stage.skirt(BRUSHIFY_MOUNTAIN_MI.format(index=1, kind=kind))


def tree_ring(stage, paths, label, count, radius_u, radius_v, band=(0.95, 1.15), blueprint=True, scale=(0.9, 1.35),
              back_only=False, seed=3, avoid=None):
    rng = random.Random(seed)
    placed = 0
    for index in range(count):
        angle = (index + rng.uniform(-0.4, 0.4)) / count * math.tau
        if back_only and math.cos(angle) < -0.2:
            continue
        radius = rng.uniform(*band)
        u, v = math.cos(angle) * radius_u * radius, math.sin(angle) * radius_v * radius
        if avoid is not None and avoid(u, v):
            continue
        path = paths[index % len(paths)]
        if blueprint:
            stage.blueprint(path, "%s_%03d" % (label, index), u, v, rng.uniform(0, 360), "Foliage", z=-10.0,
                            scale=rng.uniform(*scale))
        else:
            stage.mesh(path, "%s_%03d" % (label, index), u, v, rng.uniform(0, 360), "Foliage", scale=rng.uniform(*scale))
        placed += 1
    return placed


def decals(stage, paths, label, count, u_range, v_range, size=(500.0, 1000.0), seed=9, avoid=None):
    rng = random.Random(seed)
    for index in range(count):
        u, v = rng.uniform(*u_range), rng.uniform(*v_range)
        if avoid is not None and avoid(u, v):
            continue
        stage.decal(paths[index % len(paths)], "%s_%02d" % (label, index), u, v, rng.uniform(*size), rng.uniform(0, 360))


def track_lanes(stage, u_from, u_to, v_positions, width=420.0, seed=4, paths=None):
    """Tread marks running toward the cameras, as if the column just drove in."""
    paths = paths or TRACK_DECALS
    rng = random.Random(seed)
    for lane, v in enumerate(v_positions):
        u = u_from
        index = 0
        while u > u_to:
            length = rng.uniform(900, 1300)
            stage.decal(paths[(lane + index) % len(paths)], "Tracks_%d_%02d" % (lane, index), u - length / 2.0,
                        v + rng.uniform(-60, 60), (length, width * 2.2), rng.uniform(-4, 4))
            u -= length * 0.92
            index += 1


def industrial_ground(stage, label, centre_u, centre_v, columns, rows, tile=2400.0, height=None):
    """Concrete yard: BP_Airport_Ground1 slabs (24 m, top at the pivot) with M_RTSGround_WithSand, the concrete
    shader with panning sand. (BP_Ground_A/B/C are 10-30 m tall industrial plots, not flat ground.)"""
    height = stage.ground(centre_u, centre_v) + 3.0 if height is None else height
    for row in range(rows):
        for column in range(columns):
            u = centre_u + (row - (rows - 1) / 2.0) * tile
            v = centre_v + (column - (columns - 1) / 2.0) * tile
            # Unrotated: the panning sand is laid out per slab, rotated slabs read as a checkerboard.
            stage.blueprint(YARD_SLAB, "%s_%d_%d" % (label, row, column), u, v, 0.0, "Ground", absolute_z=height)


# -- ground weights ----------------------------------------------------------------------------------------------------
def weights_steppe(terrain, u, v, x, y):
    rim = terrain.rim_amount(u, v)
    patch = 0.5 + 0.5 * terrain.noise(u, v, 1500.0, 1)
    patch2 = 0.5 + 0.5 * terrain.noise(u, v, 700.0, 5)
    worn = math.exp(-((u - 2000.0) / 7000.0) ** 2 - (v / 9000.0) ** 2)
    slope = terrain.slope(x, y)
    return {"GrassCastle": 0.35 + 0.25 * patch * (1 - rim * 0.5), "DryGrass": 0.45 + 0.2 * patch2 * (1 - 0.6 * worn),
            "Desert": 0.05 * patch2 + 0.2 * worn * patch, "GraySand": 0.18 * worn * (1 - patch2),
            "RockDesert": 1.4 * smoothstep(0.35, 0.7, slope) + 0.1 * rim}


def weights_lush(terrain, u, v, x, y):
    weights = weights_steppe(terrain, u, v, x, y)
    weights["GrassCastle"] = weights["GrassCastle"] * 1.8 + 0.2
    weights["Desert"] *= 0.5
    return weights


def weights_snow(terrain, u, v, x, y):
    rim = terrain.rim_amount(u, v)
    patch = 0.5 + 0.5 * terrain.noise(u, v, 1200.0, 1)
    patch2 = 0.5 + 0.5 * terrain.noise(u, v, 600.0, 5)
    slope = terrain.slope(x, y)
    worn = math.exp(-((u - 1500.0) / 5000.0) ** 2 - (v / 7000.0) ** 2)
    rock = smoothstep(0.3, 0.65, slope)
    return {"Snow": 1.0 - rock, "GrassDry": 0.0, "ForestDry": 0.04 * worn * patch, "Desert": 0.0,
            "Forest": 0.9 * rock, "Grass": 0.0}


def weights_meadow(terrain, u, v, x, y):
    rim = terrain.rim_amount(u, v)
    patch = 0.5 + 0.5 * terrain.noise(u, v, 1400.0, 1)
    patch2 = 0.5 + 0.5 * terrain.noise(u, v, 650.0, 5)
    worn = math.exp(-((u - 2000.0) / 7000.0) ** 2 - (v / 9000.0) ** 2)
    slope = terrain.slope(x, y)
    return {"Grass": 0.65 * (1 - worn * 0.6) + 0.2 * patch, "GrassDry": 0.3 * patch2, "Forest": 0.8 * rim * patch,
            "ForestDry": 0.35 * rim * patch2 + 0.6 * smoothstep(0.35, 0.7, slope), "Desert": 0.45 * worn * patch2, "Snow": 0.0}


def weights_desert(terrain, u, v, x, y):
    rim = terrain.rim_amount(u, v)
    patch = 0.5 + 0.5 * terrain.noise(u, v, 1600.0, 1)
    patch2 = 0.5 + 0.5 * terrain.noise(u, v, 650.0, 5)
    slope = terrain.slope(x, y)
    worn = math.exp(-((u - 2000.0) / 7000.0) ** 2 - (v / 9000.0) ** 2)
    rock = smoothstep(0.3, 0.6, slope)
    gravel = smoothstep(0.5, 0.75, patch2)
    return {"Sand": 0.55 * (1 - rock) + 0.15 * patch, "DesertGravel": 0.6 * worn * patch2 + 0.9 * gravel,
            "Weedy": 0.18 * (1 - worn) * patch2 * (1 - rim), "DesertRockDark": 1.2 * rock + 0.25 * rim * patch,
            "ScorchSandyStone": 0.2 * rim * patch2, "ScorchConcrete": 0.0}


def weights_scorched(terrain, u, v, x, y):
    rim = terrain.rim_amount(u, v)
    patch = 0.5 + 0.5 * terrain.noise(u, v, 1300.0, 1)
    patch2 = 0.5 + 0.5 * terrain.noise(u, v, 600.0, 5)
    slope = terrain.slope(x, y)
    rock = smoothstep(0.3, 0.6, slope)
    return {"Sand": 0.05 * patch2, "DesertGravel": 0.45 * patch, "Weedy": 0.0, "DesertRockDark": 1.2 * rock + 0.5 * rim + 0.25 * patch2,
            "ScorchSandyStone": 0.35 * (1 - rock) * patch, "ScorchConcrete": 0.6 * (1 - rim) * (1 - patch)}


def plateau_height(terrain, u, v):
    """HeroAlpine: the tanks stand on a snowy shelf; in front of them the ground drops towards the cameras, behind
    them the slope climbs into ridges."""
    field = terrain.bumps * (terrain.noise(u, v, 1800.0, 3) + 0.35 * terrain.noise(u, v, 600.0, 6))
    shelf = smoothstep(-5200.0, -2600.0, u)
    drop = 1300.0 * (1.0 - shelf)
    rim = terrain.rim_amount(u, v)
    crest = max(0.55, 0.75 + 0.3 * terrain.noise(u, v, 6000.0, 0)) + 0.1 * terrain.noise(u, v, 1200.0, 7)
    ridge = rim * terrain.rim_height(u, v) * crest
    lump = 220.0 * terrain.noise(u, v, 2600.0, 9) * (1.0 - shelf)
    return terrain.base + 1300.0 + field - drop + lump + ridge


# -- cameras -----------------------------------------------------------------------------------------------------------
def hero_ground_dressing(stage, vehicles, frame, stones, shrubs, seed=13, stone_scale=(0.25, 0.7), count=26):
    """Reference 3 has rubble and scrub right in front of the tanks: small stones and shrubs within 3-25 m of the
    lead, never under a hull, never in front of a camera lens."""
    rng = random.Random(seed)
    cameras = [frame(a, b) for a, b in ((-2600.0, -500.0), (-2300.0, 0.0), (-3000.0, 0.0), (-1900.0, 1100.0), (-1200.0, -1500.0))]

    def blocked(u, v):
        near_hull = any((u - hu) ** 2 + (v - hv) ** 2 < 650.0 ** 2 for hu, hv in vehicles)
        near_lens = any((u - cu) ** 2 + (v - cv) ** 2 < 260.0 ** 2 for cu, cv in cameras)
        return near_hull or near_lens

    def scatter(paths, label, number, scale, sink):
        placed = 0
        attempts = 0
        while placed < number and attempts < number * 30:
            attempts += 1
            a, b = rng.uniform(-2600.0, 5200.0), rng.uniform(-4500.0, 4500.0)
            u, v = frame(a, b)
            if blocked(u, v):
                continue
            stage.mesh(rng.choice(paths), "%s_%02d" % (label, placed), u, v, rng.uniform(0, 360), "HeroGround",
                       scale=rng.uniform(*scale), sink=sink)
            placed += 1

    scatter(stones, "HeroStone", count, stone_scale, 12.0)
    if shrubs:
        scatter(shrubs, "HeroShrub", count, (0.6, 1.1), 4.0)


def hero_cameras(stage, origin=(0.0, 0.0), facing=0.0, tip_height=150.0):
    """WoT-key-art framings about 20 m from the lead vehicle's nose at about 1.5 m: the lead fills about a quarter of
    the frame height, the wings fan out behind it, the horizon sits in the upper third."""
    frame = hero_frame(origin, facing)

    def cam(shot, eye, target, fmt, focal, notes):
        eu, ev = frame(eye[0], eye[1])
        tu, tv = frame(target[0], target[1])
        stage.camera(shot, (eu, ev, eye[2]), (tu, tv, target[2]), fmt, focal, notes)

    cam("Hero_H", (-2600.0, -500.0, 170.0), (900.0, 0.0, 110.0), "Horizontal16x9", 35.0,
        "WoT-style hero: 1.7 m high, about 20 m from the lead vehicle's nose, wings fanning back.")
    cam("Hero_V", (-1900.0, 0.0, 260.0), (1000.0, 0.0, -40.0), "Vertical1x2", 24.0,
        "Vertical 1:2 poster: the arrowhead in the middle band, mountains and sky above.")
    cam("Hero_Pano", (-3000.0, 0.0, 190.0), (1600.0, 0.0, 100.0), "Panorama2x1", 28.0,
        "Wide 2:1 key art straight on the arrowhead.")
    cam("Hero_Low_H", (-1900.0, 1100.0, 60.0), (900.0, -200.0, 180.0), "Horizontal16x9", 22.0,
        "Ground-level wide angle from the right of the lead vehicle.")
    cam("Hero_Side_V", (-1200.0, -1500.0, 160.0), (0.0, 0.0, 120.0), "Vertical1x2", 24.0,
        "Vertical three-quarter view of the lead vehicle's left flank, the wing behind it.")


def standard_cameras(stage, target, radius, aerial=True, low_az=-28.0):
    """Overviews: at least one vertical 1:2 and one horizontal 16:9 still, plus aerials."""
    tu, tv, th = target
    stage.orbit_camera("Front_H", (tu, tv, th), radius, low_az, 9.0, focal=30.0,
                       notes="Low front three-quarter of the whole formation, buildings behind.")
    stage.camera("Front_V", (tu - radius * 1.1, tv - radius * 0.1, 700.0), (tu - radius * 0.2, tv, 200.0), fmt="Vertical1x2",
                 focal=19.0, notes="Vertical poster: front ranks in the lower third, army and backdrop above.")
    if aerial:
        stage.orbit_camera("Aerial_H", (tu, tv, 0.0), radius, -32.0, 36.0, focal=28.0, margin=1.2,
                           notes="Oblique aerial overview of every unit (like reference 2).")
        stage.orbit_camera("Aerial_Pano", (tu, tv, 0.0), radius, 25.0, 26.0, fmt="Panorama2x1", focal=28.0, margin=1.15,
                           notes="Wide 2:1 aerial from the left flank.")
        stage.orbit_camera("Aerial_V", (tu, tv, 0.0), radius * 0.8, -10.0, 48.0, fmt="Vertical1x2", focal=20.0,
                           notes="Vertical high overview.")


# -- shared dressing ---------------------------------------------------------------------------------------------------
def keep_clear(u_min, u_max, half_v):
    return lambda u, v: abs(v) < half_v and u_min < u < u_max


def dress_steppe(stage, back_u=18000.0):
    clear = keep_clear(-10000, 17000, 10500)
    tree_ring(stage, LEAF_TREES, "Tree", 50, 19000.0, 19000.0, band=(0.95, 1.12), seed=11, avoid=lambda u, v: u < -12000.0)
    stage.scatter(STONES, "Stone", 30, (-14000, 18000), (-18000, 18000), "Ground", scale=(0.7, 1.5), avoid=clear)
    stage.scatter(DEAD_SHRUBS, "Shrub", 80, (-16000, 19000), (-19000, 19000), "Foliage", scale=(0.8, 1.4), sink=5.0, avoid=clear)
    decals(stage, CRATERS, "Crater", 10, (-12000, 16000), (-17000, 17000), avoid=clear)
    for index in range(9):
        stage.blueprint(SCORCH_POWERLINE, "Powerline_%d" % index, back_u + 1500.0, -16000.0 + index * 4000.0, 90.0, "Backdrop")
    mountains(stage, "Grasslands", 9, height=(12740.0, 27300.0))


def dress_meadow(stage):
    clear = keep_clear(-10000, 19000, 10500)
    tree_ring(stage, SPRUCES, "Forest", 90, 19500.0, 19500.0, band=(0.92, 1.15), seed=23, avoid=lambda u, v: u < -12000.0)
    stage.scatter(MEADOW_SHRUBS, "Shrub", 60, (-15000, 19000), (-19000, 19000), "Foliage", scale=(0.8, 1.3), sink=5.0, avoid=clear)
    stage.scatter(STONES, "Stone", 24, (-14000, 18000), (-18000, 18000), "Ground", scale=(0.7, 1.5), avoid=clear)
    decals(stage, CRATERS, "Crater", 8, (-12000, 16000), (-17000, 17000), avoid=clear)
    mountains(stage, "Rocky", 9, height=(16380.0, 35100.0))


def dress_desert(stage):
    clear = keep_clear(-10000, 18000, 10500)
    stage.scatter(DESERT_ROCKS, "Rock", 34, (-14000, 19000), (-19000, 19000), "Ground", scale=(0.7, 1.8), sink=40.0, avoid=clear)
    stage.scatter(CACTUS_BUSHES, "Bush", 40, (-15000, 19000), (-19000, 19000), "Foliage", scale=(0.7, 1.2), avoid=clear)
    stage.scatter(DEAD_SHRUBS, "Shrub", 50, (-15000, 19000), (-19000, 19000), "Foliage", scale=(0.8, 1.3), sink=5.0, avoid=clear)
    decals(stage, SANDY_CRATERS, "Crater", 8, (-12000, 16000), (-17000, 17000), avoid=clear)
    mountains(stage, "Desert", 10, height=(14560.0, 31200.0))


def dress_scorched(stage):
    clear = keep_clear(-6000, 14000, 9500)
    stage.scatter(DESERT_ROCKS, "Rock", 30, (-12000, 18000), (-18000, 18000), "Ground", scale=(0.8, 1.8), sink=40.0, avoid=clear)
    stage.scatter(DEAD_TREES, "DeadTree", 14, (-10000, 18000), (-18000, 18000), "Foliage", avoid=clear)
    decals(stage, SCORCH_DECALS, "Scorch", 14, (-9000, 14000), (-15000, 15000), size=(600.0, 1300.0),
           avoid=keep_clear(-4000, 9000, 8000))
    mountains(stage, "DesertMountains", 9, height=(18200.0, 39000.0))


# -- stage builders ----------------------------------------------------------------------------------------------------
def build_army_overview(stage, rows, squads, team_weapons, pits):
    """Heavies centre-front, mediums on the left, lights on the right, mechanized behind; infantry in front of the
    flank blocks, team weapons beside the pits."""
    layout = {"Experimental": ((-1200.0, 0.0), 0.0, 4), "Medium": ((1800.0, -7600.0), 14.0, 4),
              "Light": ((2200.0, 7800.0), -14.0, 5), "Mechanized": ((6800.0, -500.0), 0.0, 6)}
    for index, (row_name, names) in enumerate(rows):
        centre, facing, per_row = layout[row_name]
        cluster(stage, names, row_name, centre, facing, per_row, seed=31 + index)
    cluster(stage, squads[:len(squads) // 2], "SquadL", (-4200.0, -6800.0), 10.0, 3, spacing=1300.0, depth=1200.0,
            place=place_squad, folder="Infantry", seed=41)
    cluster(stage, squads[len(squads) // 2:], "SquadR", (-4200.0, 6800.0), -10.0, 3, spacing=1300.0, depth=1200.0,
            place=place_squad, folder="Infantry", seed=42)
    cluster(stage, team_weapons, "TW", (-5600.0, 0.0), 0.0, 4, spacing=1700.0, depth=1400.0, place=place_squad,
            folder="TeamWeapons", seed=43)
    for index, name in enumerate(pits):
        side = -1 if index % 2 else 1
        stage.blueprint(name, "Pit_%02d" % index, -3500.0 + (index // 2) * 2600.0, side * 13500.0, 180.0 - side * 75.0, "Pits")


def aircraft_formation(stage, names, kind):
    """V (StrikeDivision), echelon (Breakthrough) or loose pairs (Soviet) high over the parade."""
    for index, name in enumerate(names):
        if kind == "V":
            rank = (index + 1) // 2
            side = 0 if index == 0 else (1 if index % 2 else -1)
            u, v, h = 1500.0 + rank * 1800.0, side * rank * 2200.0, 4200.0 + rank * 250.0
        elif kind == "Echelon":
            u, v, h = 500.0 + index * 1700.0, -5000.0 + index * 2600.0, 3800.0 + index * 350.0
        else:
            u, v, h = 1500.0 + (index % 3) * 2600.0, (index - 2.5) * 2800.0, 3600.0 + (index % 2) * 900.0
        place_aircraft(stage, name, "Air_%02d" % index, u, v, h)


def no_aircraft_shadows(stage):
    """Aircraft over the parade cast big detached shadows that read as oil stains on the ground."""
    for actor in sc_common.actor_subsystem().get_all_level_actors():
        if actor.get_actor_label().startswith("%s%s_Air_" % (sc_common.PREFIX, stage.name)):
            for component in actor.get_components_by_class(unreal.PrimitiveComponent):
                component.set_editor_property("cast_shadow", False)


def place_crop_field(stage, crop, row_yaw, corners, seed, label):
    """Crushable crop field (as on Ger_Trains) over a polygon given in the stage frame."""
    centre_u = sum(c[0] for c in corners) / len(corners)
    centre_v = sum(c[1] for c in corners) / len(corners)
    field = stage.blueprint(CROPS + "BP_CropField_" + crop, label, centre_u, centre_v, row_yaw, "Farm")
    if field is None:
        return
    yaw = math.radians(-row_yaw)
    spline = field.get_editor_property("field_outline")
    spline.clear_spline_points(False)
    for u, v in corners:
        du, dv = u - centre_u, v - centre_v
        spline.add_spline_point(unreal.Vector(du * math.cos(yaw) - dv * math.sin(yaw), du * math.sin(yaw) + dv * math.cos(yaw), 0.0),
                                unreal.SplineCoordinateSpace.LOCAL, False)
    for index in range(len(corners)):
        spline.set_spline_point_type(index, unreal.SplinePointType.LINEAR, False)
    spline.set_closed_loop(True, False)
    spline.update_spline()
    spline.set_editor_property("spline_has_been_edited", True)
    layout = field.get_editor_property("layout")
    layout.set_editor_property("random_seed", seed)
    field.set_editor_property("layout", layout)


def build_strike_division(stage):
    # Clear late-morning summer light with fair-weather clouds.
    stage.lighting(sun_pitch=-30.0, sun_yaw=-40.0, sun_lux=10.0, sun_temperature=5600.0, sky_intensity=1.0,
                   fog_density=0.008, fog_falloff=0.15, exposure_bias=0.1, saturation=1.05, contrast=1.06,
                   temperature=6400.0, vignette=0.3, clouds=True)
    build_army_overview(stage, SD_ROWS, SD_SQUADS, SD_TEAM_WEAPONS, GER_PITS)
    aircraft_formation(stage, GER_AIRCRAFT, "V")
    # Deployed buildings in two staggered rows behind the parade (footprints are the deployed buildings, not trucks).
    for index, name in enumerate(SD_BUILDINGS):
        row, column = divmod(index, 6)
        place_building_truck(stage, name, "Building_%02d" % index, 12500.0 + row * 4500.0,
                             (column - 2.5) * 3900.0 + row * 1900.0)
    no_aircraft_shadows(stage)
    dress_steppe(stage)
    standard_cameras(stage, (2500.0, 0.0, 250.0), 9800.0)


def build_breakthrough(stage):
    # Soft, slightly overcast northern light over a green meadow.
    # Cold, overcast early morning.
    stage.lighting(sun_pitch=-16.0, sun_yaw=35.0, sun_lux=6.5, sun_temperature=6600.0, sky_intensity=1.4,
                   fog_density=0.005, fog_falloff=0.14, fog_color=(0.5, 0.56, 0.64), exposure_bias=0.4, saturation=1.0,
                   contrast=1.08, temperature=7200.0, vignette=0.35, clouds=True)
    build_army_overview(stage, BT_ROWS, BT_SQUADS, BT_TEAM_WEAPONS, GER_PITS + BT_BUNKERS)
    aircraft_formation(stage, GER_AIRCRAFT, "Echelon")
    for index, name in enumerate(BT_BUILDING_TRUCKS):
        place_building_truck(stage, name, "Building_%02d" % index, 12500.0, (index - 1.5) * 4600.0)
    stage.row_by_bounds(BT_BUILDING_MESHES, "BuildingMesh", 16500.0, 1500.0, mesh=True)
    no_aircraft_shadows(stage)
    dress_meadow(stage)
    standard_cameras(stage, (2500.0, 0.0, 250.0), 9800.0)


def build_soviet_army(stage):
    # Warm low desert sun like reference 1.
    stage.lighting(sun_pitch=-13.0, sun_yaw=-55.0, sun_lux=8.5, sun_temperature=4200.0, sky_intensity=0.9,
                   fog_density=0.012, fog_falloff=0.12, fog_color=(0.6, 0.45, 0.3), exposure_bias=0.2, saturation=1.06,
                   contrast=1.1, temperature=5600.0, vignette=0.35, mie_scale=2.5, rayleigh_scale=0.75, clouds=True)
    # Heavies front centre, mediums and lights behind, trucks and rocket trucks on the left flank, guns on the right.
    cluster(stage, RUS_TANKS[:17], "Heavy", (-1500.0, 0.0), 0.0, 5, seed=51)
    cluster(stage, RUS_TANKS[17:29], "Medium", (2600.0, -6200.0), 12.0, 4, seed=52)
    cluster(stage, RUS_TANKS[29:], "Light", (2600.0, 6200.0), -12.0, 3, seed=53)
    cluster(stage, RUS_TRUCKS, "Truck", (-1800.0, -10800.0), 25.0, 2, spacing=1400.0, folder="Trucks", seed=54)
    cluster(stage, RUS_TEAM_WEAPONS, "TW", (-2200.0, 10800.0), -25.0, 3, spacing=1600.0, place=place_squad,
            folder="TeamWeapons", seed=55)
    cluster(stage, RUS_SQUADS, "Squad", (-5200.0, 0.0), 0.0, 5, spacing=1300.0, depth=1200.0, place=place_squad,
            folder="Infantry", seed=56)
    stage.row_by_bounds(RUS_BACKDROP, "Building", 7500.0, 900.0)
    for index, name in enumerate(RUS_FORTS):
        side = -1 if index % 2 else 1
        stage.blueprint(name, "Fort_%02d" % index, 1000.0 + index * 1400.0, side * 11500.0, 180.0 - side * 90.0, "Buildings")
    aircraft_formation(stage, RUS_AIRCRAFT * 2, "Pairs")
    no_aircraft_shadows(stage)
    stage.scatter(DESERT_ROCKS, "PadRock", 30, (-7000, 12000), (-12000, 12000), "Ground", scale=(0.25, 0.6), sink=25.0)
    dress_desert(stage)
    standard_cameras(stage, (2500.0, 0.0, 250.0), 9000.0)


def build_soviet_industrial(stage):
    # Hazy industrial afternoon.
    # Low dusk sun through industrial haze.
    stage.lighting(sun_pitch=-7.0, sun_yaw=50.0, sun_lux=7.5, sun_temperature=3400.0, sky_intensity=0.9,
                   fog_density=0.005, fog_falloff=0.14, fog_color=(0.5, 0.4, 0.32), exposure_bias=0.4, saturation=1.02,
                   contrast=1.12, temperature=5600.0, vignette=0.4, mie_scale=1.2, clouds=True)
    industrial_ground(stage, "Yard", 0.0, 0.0, 7, 9)
    selection = ["Rus_Vehicle_IS-3", "Rus_Vehicle_KV-2", "Rus_Vehicle_T3485", "Rus_Vehicle_IS6", "Rus_Vehicle_KV-1E",
                 "Rus_Vehicle_T34AA", "Rus_Vehicle_Su100", "Rus_Vehicle_ISU152", "Rus_Vehicle_T-70", "Rus_Vehicle_T44_100L",
                 "Rus_Harvester_T34Breaker", "Rus_ArmoredCar_BM13", "Rus_ArmoredCar_ZilFuel", "Rus_ArmoredCar_SovietTanker",
                 "Rus_ArmoredCar_ZilAmmo", "Rus_ArmoredCar_BTR152A"]
    grid(stage, selection, "Unit", -3000.0, 1300.0, 1500.0, 6, jitter=120.0, stagger=300.0)
    grid(stage, ["Rus_Squad_Mosin", "Rus_Squad_HazmatEngineers", "Rus_T2Squad_ToxicGuard", "Rus_Squad_PTRS"], "Squad",
         -5600.0, 1500.0, 1200.0, 2, place=place_squad, folder="Infantry", centre_v=-6500.0)
    refineries = ["Rus_Zone_Refinery1", "Rus_Zone_Refinery2", "Rus_Zone_Refinery3", "Rus_Zone_Refinery4", "Rus_Zone_RefinerySilo",
                  "IndusZone_Ref", "Rus_Building_CoolingTowers", "Rus_Building_FuelStorageLarge", "Rus_Building_FuelStorage2"]
    stage.row_by_bounds(refineries[:5], "Refinery", 6500.0, 800.0, folder="Refinery")
    stage.row_by_bounds(refineries[5:], "RefineryBack", 12500.0, 1000.0, folder="Refinery", centre_v=1500.0)
    stage.row_by_bounds(["Rus_Zone_RefineryPipeSystem", "Rus_Zone_RefineryPipeSystem2"], "Pipes", 4500.0, 6000.0,
                        yaw=90.0, folder="Refinery")
    # Yard clutter along the edges and between the groups: containers, barrel and crate stacks, painted markings.
    rng = random.Random(61)
    for index in range(16):
        side = -1 if index % 2 else 1
        u = -7500.0 + (index // 2) * 1800.0 + rng.uniform(-300, 300)
        v = side * rng.uniform(8200.0, 10200.0)
        stage.blueprint(YARD_PROPS[index % len(YARD_PROPS)], "YardProp_%02d" % index, u, v,
                        rng.choice((0.0, 90.0)) + rng.uniform(-6, 6), "Yard")
    decals(stage, YARD_DECALS, "YardMark", 24, (-8000, 9000), (-9500, 9500), size=(300.0, 900.0), seed=62)
    for index, name in enumerate(["Rus_Building_FuelStorageTank", "Rus_Building_FuelStorageTankSmall", "Rus_Building_FuelStorageSMall"]):
        stage.blueprint(name, "Tank_%02d" % index, 1000.0 + index * 3000.0, -12500.0 + (index % 2) * 25000.0, 0.0, "Refinery")
    stage.scatter(DEAD_SHRUBS, "Shrub", 40, (-16000, 22000), (-22000, 22000), "Foliage", scale=(0.8, 1.3), sink=5.0,
                  avoid=keep_clear(-12000, 24000, 12500))
    mountains(stage, "Desert", 9, height=(14000.0, 30000.0))
    standard_cameras(stage, (2000.0, 0.0, 250.0), 7000.0)
    stage.camera("Refinery_H", (-7500.0, 4500.0, 500.0), (6000.0, -800.0, 700.0), focal=26.0,
                 notes="The column on the concrete yard with the refinery towers behind, from the right.")
    stage.camera("Refinery_V", (-5200.0, -2500.0, 200.0), (6000.0, 0.0, 1300.0), fmt="Vertical1x2", focal=18.0,
                 notes="Vertical: column low, refinery towers and sky above.")


def build_forge(stage):
    # Smouldering dusk: low red sun, light haze (no heavy fog).
    stage.lighting(sun_pitch=-6.0, sun_yaw=60.0, sun_lux=7.0, sun_temperature=3300.0, sky_intensity=1.0,
                   fog_density=0.008, fog_falloff=0.12, fog_color=(0.35, 0.36, 0.42), exposure_bias=0.5, saturation=0.92,
                   contrast=1.14, temperature=7200.0, vignette=0.45, mie_scale=1.5, rayleigh_scale=1.0, clouds=True)
    # The spearhead stands on the left flank facing +v; its cameras look towards -v, away from the parade.
    spearhead = (-1500.0, -9500.0)
    hull_positions = wedge(stage, FORGE_VEHICLES[:10], "Forge_Hero", frame=hero_frame(spearhead, -90.0), facing=-90.0, seed=9)
    hero_ground_dressing(stage, hull_positions, hero_frame(spearhead, -90.0), DESERT_ROCKS, DEAD_TREES[:2] + DEAD_SHRUBS,
                         seed=24, stone_scale=(0.15, 0.45), count=20)
    grid(stage, FORGE_VEHICLES[10:], "Forge", -2500.0, 1150.0, 1350.0, 7, stagger=300.0, centre_v=2500.0)
    for index, name in enumerate(FORGE_BACKDROP + ["Rus_Building_Factory", "Rus_Building_AmmoStorage", "Rus_BXP_MGBunker"]):
        stage.blueprint(name, "Building_%02d" % index, 5500.0 + (index % 2) * 2200.0, (index - 3.0) * 3300.0 + 2500.0,
                        180.0, "Buildings")
    dress_scorched(stage)
    standard_cameras(stage, (500.0, 2500.0, 250.0), 4600.0)
    hero_cameras(stage, origin=spearhead, facing=-90.0)
    mountains(stage, "DesertMountains", 6, height=(15000.0, 30000.0), spread_deg=80.0, centre_deg=-90.0, seed=11,
              skirt=False)


def build_hero_steppe(stage):
    # Golden hour over farmland: long shadows, warm light.
    stage.lighting(sun_pitch=-8.0, sun_yaw=-75.0, sun_lux=8.0, sun_temperature=3700.0, sky_intensity=1.0,
                   fog_density=0.012, fog_falloff=0.12, fog_color=(0.6, 0.48, 0.36), exposure_bias=0.4, saturation=1.1,
                   contrast=1.1, temperature=5900.0, vignette=0.4, clouds=True, mie_scale=1.6)
    hull_positions = wedge(stage, HERO_SOVIET, "Hero")
    hero_ground_dressing(stage, hull_positions, hero_frame(), STONES, DEAD_SHRUBS + MEADOW_SHRUBS[:3], seed=21)
    clear = keep_clear(-1500, 4500, 4500)
    # Wheat in a golden band right behind the arrowhead, a burnt field on the right foreground flank.
    place_crop_field(stage, "Wheat", 0.0, [(4300, -13000), (10500, -13000), (10500, -1200), (4300, -1200)], 2041, "Field_0")
    place_crop_field(stage, "Wheat", 0.0, [(4300, 1200), (10500, 1200), (10500, 13000), (4300, 13000)], 2082, "Field_1")
    place_crop_field(stage, "WheatBurned", 90.0, [(-7500, 3800), (-3200, 3800), (-3200, 12000), (-7500, 12000)], 2123, "Field_2")
    for index, (kind, u, v, yaw) in enumerate([("FarmHouse", 14000.0, -6500.0, 200.0), ("Barn", 15000.0, 4500.0, 170.0),
                                                ("HayShed", 13000.0, 9500.0, 160.0), ("Stable", 16000.0, -1500.0, 185.0)]):
        stage.blueprint(FARM + kind, "Farm_%d" % index, u, v, yaw, "Backdrop")
    for row in range(2):
        for column in range(9):
            stage.blueprint(ORCHARD[(row + column) % len(ORCHARD)], "Orchard_%d_%d" % (row, column), 11500.0 + row * 1500.0,
                            -16500.0 + column * 1300.0, row * 40.0 + column * 17.0, "Foliage", z=-10.0)
    tree_ring(stage, LEAF_TREES, "Tree", 46, 16500.0, 16500.0, back_only=True, seed=21)
    stage.scatter(STONES, "Stone", 26, (-7000, 9000), (-12000, 12000), "Ground", scale=(0.6, 1.3), avoid=clear)
    for index in range(7):
        stage.blueprint(SCORCH_POWERLINE, "Powerline_%d" % index, 17500.0 + index * 300.0, -15000.0 + index * 5000.0, 90.0, "Backdrop")
    mountains(stage, "Grasslands", 8, height=(10000.0, 22000.0))
    hero_cameras(stage)


def build_hero_alpine(stage):
    # Cold clear morning: blue shadows, bright snow.
    stage.lighting(sun_pitch=-18.0, sun_yaw=-35.0, sun_lux=11.0, sun_temperature=6500.0, sky_intensity=1.3,
                   fog_density=0.01, fog_falloff=0.1, fog_color=(0.55, 0.64, 0.8), exposure_bias=-0.3, saturation=0.95,
                   contrast=1.12, temperature=7400.0, vignette=0.35, clouds=True)
    hull_positions = wedge(stage, HERO_BREAKTHROUGH, "Hero", seed=5)
    hero_ground_dressing(stage, hull_positions, hero_frame(), STONES, LOGS[:4], seed=22, stone_scale=(0.35, 0.9))
    clear = keep_clear(-2000, 4500, 4500)
    decals(stage, CRATERS[:3], "Mud", 6, (-2500, 5000), (-6000, 6000), size=(350.0, 600.0), seed=41,
           avoid=keep_clear(-600, 4500, 3600))
    tree_ring(stage, DARK_SPRUCES, "Spruce", 80, 14000.0, 14000.0, band=(0.75, 1.2), seed=31,
              avoid=lambda u, v: u < -2500.0 and abs(v) < 9000.0)
    stage.scatter(STONES, "Stone", 30, (-9000, 9000), (-13000, 13000), "Ground", scale=(0.9, 2.2), avoid=clear)
    stage.scatter(DEAD_TREES, "DeadTree", 8, (-6000, 9000), (-14000, 14000), "Foliage", avoid=lambda u, v: abs(v) < 8000)
    stage.scatter(LOGS, "Log", 10, (-4000, 9000), (-12000, 12000), "Foliage", avoid=lambda u, v: abs(v) < 6500)
    mountains(stage, "Snow", 10, height=(26000.0, 52000.0), distance=(240000.0, 340000.0))
    hero_cameras(stage)


def build_hero_mesa(stage):
    # Late afternoon desert: warm sun, blue-gold sky with cloud, mesa cliffs close behind.
    stage.lighting(sun_pitch=-14.0, sun_yaw=55.0, sun_lux=9.5, sun_temperature=4600.0, sky_intensity=1.0,
                   fog_density=0.01, fog_falloff=0.12, fog_color=(0.65, 0.52, 0.38), exposure_bias=0.2, saturation=1.08,
                   contrast=1.1, temperature=5900.0, vignette=0.4, mie_scale=1.5, rayleigh_scale=1.0, clouds=True)
    hull_positions = wedge(stage, HERO_STRIKE, "Hero", seed=7)
    hero_ground_dressing(stage, hull_positions, hero_frame(), DESERT_ROCKS, DEAD_SHRUBS + CACTUS_BUSHES[6:], seed=23,
                         stone_scale=(0.15, 0.4))
    clear = keep_clear(-2000, 4500, 4500)
    track_lanes(stage, 5500.0, -1200.0, (-720.0, 720.0), width=300.0)
    rng = random.Random(88)
    for index in range(7):
        u = rng.uniform(14000.0, 21000.0)
        v = (index - 3) * 5200.0 + rng.uniform(-900, 900)
        stage.mesh(BRUSHIFY_CLIFFS[index % 2], "Mesa_%02d" % index, u, v, rng.uniform(0, 360), "Backdrop",
                   scale=rng.uniform(2.2, 3.4), sink=700.0, material=BRUSHIFY_MOUNTAIN_MI.format(index=2, kind="DesertMountains"))
    stage.scatter(DESERT_ROCKS, "Rock", 34, (-8000, 12000), (-15000, 15000), "Ground", scale=(0.7, 1.8), sink=90.0, avoid=clear)
    stage.scatter(DESERT_ROCKS, "NearRock", 10, (-5000, -1600), (-6000, 6000), "Ground", scale=(0.3, 0.6), sink=30.0,
                  avoid=lambda u, v: abs(v) < 1600)
    stage.scatter(CACTUS_BUSHES, "Bush", 40, (-9000, 12000), (-15000, 15000), "Foliage", scale=(0.7, 1.2), avoid=clear)
    stage.scatter(DEAD_SHRUBS, "Shrub", 50, (-9000, 12000), (-15000, 15000), "Foliage", scale=(0.8, 1.3), sink=5.0, avoid=clear)
    decals(stage, SANDY_CRATERS, "Crater", 9, (-6000, 8000), (-12000, 12000), avoid=keep_clear(-1500, 5000, 5000))
    mountains(stage, "DesertMountains", 9, height=(16000.0, 34000.0))
    hero_cameras(stage)


STAGES = {
    "StrikeDivision": dict(name="StrikeDivision", centre=(-2000000.0, 0.0), ground="DryCastle", seed=11,
                           pad=(24000.0, 18000.0), rim=4000.0, back_rim=7500.0, bumps=50.0,
                           flats=[(17000.0, 0.0, 5500.0, 16000.0, 400.0, 3000.0)],
                           weights=weights_steppe, build=build_strike_division),
    "Breakthrough": dict(name="Breakthrough", centre=(-2000000.0, 2000000.0), ground="Brushify", seed=12, 
                         pad=(26000.0, 18000.0), rim=4500.0, back_rim=8000.0, bumps=50.0,
                         flats=[(17500.0, 0.0, 5500.0, 16000.0, 400.0, 3000.0)],
                         weights=weights_meadow, build=build_breakthrough),
    "SovietArmy": dict(name="SovietArmy", centre=(2000000.0, 0.0), ground="Desert", seed=13, 
                       pad=(24000.0, 18000.0), rim=4000.0, back_rim=7000.0, bumps=50.0,
                       flats=[(16000.0, 0.0, 4500.0, 17000.0, 400.0, 3000.0)],
                       weights=weights_desert, build=build_soviet_army),
    "SovietIndustrial": dict(name="SovietIndustrial", centre=(2000000.0, 2000000.0), ground="Desert", seed=14, 
                             pad=(30000.0, 20000.0), rim=3500.0, back_rim=5500.0, bumps=30.0,
                             flats=[(8000.0, 0.0, 17000.0, 16000.0, 400.0, 3000.0)],
                             weights=weights_desert, build=build_soviet_industrial),
    "ForgeArmy": dict(name="ForgeArmy", centre=(2000000.0, -2000000.0), ground="Desert", seed=15, 
                      pad=(20000.0, 16000.0), rim=4500.0, back_rim=8500.0, bumps=60.0,
                      flats=[(13000.0, 0.0, 3000.0, 14000.0, 400.0, 3000.0)],
                      weights=weights_scorched, build=build_forge),
    "HeroSteppe": dict(name="HeroSteppe", centre=(0.0, 2000000.0), ground="DryCastle", seed=16, 
                       pad=(16000.0, 14000.0), rim=3500.0, back_rim=6000.0, bumps=70.0,
                       weights=weights_lush, build=build_hero_steppe),
    "HeroAlpine": dict(name="HeroAlpine", centre=(0.0, -2000000.0), ground="BrushifySnow", seed=17, 
                       pad=(14000.0, 13000.0), rim=5000.0, back_rim=9500.0, bumps=60.0, height_fn=plateau_height,
                       weights=weights_snow, build=build_hero_alpine),
    "HeroMesa": dict(name="HeroMesa", centre=(-2000000.0, -2000000.0), ground="Desert", seed=18, 
                     pad=(16000.0, 14000.0), rim=4500.0, back_rim=8000.0, bumps=60.0,
                     weights=weights_desert, build=build_hero_mesa),
}
