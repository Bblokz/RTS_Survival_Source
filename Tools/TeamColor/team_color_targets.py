"""Which German vehicle materials get the team colour CamoSwap, shared by the inspect and rollout scripts.

Base materials (roots) are spliced; instances of a spliced root get their own TC_KeyColor. Everything else a German
vehicle shows (wheels, tracks, glass, MGs, props, crew, nets, shared environment masters) is left alone.
"""

VEHICLES = "/Game/RTS_Survival/Blueprints/GroundVehicles/"
TRACKED = VEHICLES + "TrackBasedVehicle/"
NOMADIC = VEHICLES + "BaseAndNomadic/ChildBlueprints/"
TURRETS = "/Game/RTS_Survival/Blueprints/Turrets/German/"

# Hull masters: the team colour goes in before the unit stamps, so markings, flags and icons keep their colours.
MASTER_ROOTS = [
    TRACKED + "MasterMaterials/MM_Ger_PostApoTank_NoBlend",
    TRACKED + "MasterMaterials/MM_Ger_PostApoTank",
    TRACKED + "MasterMaterials/MM_Ger_PostApoTank_NoBlend_RailRed",
    TRACKED + "MasterMaterials/MM_Ger_PostApoTank_NoBlend_RailOrange",
    TRACKED + "Ger_Medium/JagdPanzerIV70/Materials/MM_Ger_PostApoTank_NoBlend_FullUV",
    TRACKED + "MasterMaterials/MM_TankHull",
]

# Vehicle specific hull, turret and body materials; the team colour goes in right before Base Color.
STANDALONE_ROOTS = [
    NOMADIC + "GerEco/OpelBlitz_Ref/Skeleton/Textures/M_OpelBlitz",
    NOMADIC + "GerT1/MechanizedDepot/TruckMeshes/M_RocketTruck",
    NOMADIC + "GerT2/CommunicationsCenter/Skeleton/Materials/M_Sdkfz9",
    TRACKED + "Ger_ArmoredCar/Keubel/Materials/M_Keubel",
    TRACKED + "Ger_ArmoredCar/Puma/Materials/M_Puma",
    TRACKED + "Ger_ArmoredCar/Sdkfz-251_75mm/Meshes/Turret/M_Sdkfz234_4",
    TRACKED + "Ger_ArmoredCar/Sdkfz231/Materials/M_sdkfz_231",
    TRACKED + "Ger_ArmoredCar/Sdkfz251_Transport/Variants/AmmoCarrier/Materials/M_Sdkfz251_AmmoCarrier",
    TRACKED + "Ger_ArmoredCar/Sdkfz7_Flak/Materials/M_Sdkfz7Flak",
    TRACKED + "Ger_ArmoredCar/Sdkfz_234_3/Materials/M_Sdkfz_234_3",
    TRACKED + "Ger_Heavy/E100/Meshes/Atlas/M_E100Atlas",
    TRACKED + "Ger_Heavy/E50/Materials/M_E50_Hull",
    TRACKED + "Ger_Heavy/E50/Materials/M_E50_Turret",
    TRACKED + "Ger_Heavy/E75/Materials/M_E75_Addons",
    TRACKED + "Ger_Heavy/E75/Materials/M_E75_Body",
    TRACKED + "Ger_Heavy/JagdPanther/Meshes/Materials/M_JagdPanther_Hull",
    TRACKED + "Ger_Heavy/PantherG/Materials/M_PantherG_Engine_Winter",
    TRACKED + "Ger_Heavy/PantherG/Materials/M_PantherG_Hull_Winter",
    TURRETS + "Heavy/PantherTurrets/PantherG/Materials/M_PantherG_Turret_Winter",
    TRACKED + "Ger_Heavy/PantherII/Textures/HullTextures/M_PantherII_Hull",
    TRACKED + "Ger_Heavy/PantherII/Textures/M_PantherII_Parts",
    TRACKED + "Ger_Heavy/PantherII/Textures/M_PantherII_Sheets",
    TURRETS + "Heavy/PantherIITurret/Materials/M_PantherII_Turret",
    TRACKED + "Ger_Heavy/Tiger/TracksMatTextures/DessertTiger/M_Tiger_Tracks_DessertCamo",
    TRACKED + "Ger_Heavy/TigerII/Materials/Hull/M_TigerII_Hull",
    TRACKED + "Ger_Light/PZ1_15cm/Materials/M_Hull_Panzer150",
    TRACKED + "Ger_Light/PanzerII/Meshes/Materials/Turret/M_PZII_Turret",
    TRACKED + "Ger_Light/SDKFZ-140/Materials/M_Sdkfz-140_Hull",
    TRACKED + "Ger_Light/SDKFZ-140/Materials/M_Sdkfz-140_Hull_SkeletalCopy",
    TRACKED + "Ger_Medium/Brumbar/Materials/M_Brumbar_Sides",
    TRACKED + "Ger_Medium/Brumbar/Materials/M_BrummbarGun",
    TRACKED + "Ger_Medium/PanzerIII/Materials/M_PanzerIII_DesertCamo",
]

TEAM_COLOR_FOLDER = TRACKED + "MasterMaterials/TeamColor"
TEAM_COLOR_FUNCTION = TEAM_COLOR_FOLDER + "/MF_TeamColor_CamoSwap"
