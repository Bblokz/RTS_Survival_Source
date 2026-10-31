"""Render jobs for the global ability icons: every aircraft, bomb, tank and paratrooper as a cut-out, in the pose the
icon uses. Run in the open editor, twice per batch (the first capture can catch unstreamed textures):
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/render_ability_parts.py").read())
    run_jobs(["Ju87_Dive"])          # subset, or run_jobs() for all
compose_ability_icons.py reads <name>_color.png, <name>_mask.png and <name>_points.json from Saved/GlobalAbilityIcons/renders.
"""
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read())

A = "/Game/RTS_Survival/Blueprints/Aircraft/"
BOMBS = A + "GerAircraft/BombMeshes/"
T34 = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/"
PARA = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Parachute/"
SHELLS = "/Game/RTS_Survival/Meshes/Props/Ammo/Shells/"
GER = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
SDKFZ7_TRACKS = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/SKDFZ-7/Skeleton/Animations/Anim_Sdkfz-7_FWD"
JU87 = A + "GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO"
JU87_FLY = A + "GerAircraft/JU_87VTO/Meshes/Animations/Anim_StukaJU87_Dive"
HO229 = A + "GerAircraft/HO229/Mesh/SK_Ho229"
HO229_FLY = A + "GerAircraft/HO229/Mesh/Anims/Anim_Ho229_Wing_Flying"
JU390 = A + "GerAircraft/JU390B/Mesh/SK_JU309B_Animated"
JU390_FLY = A + "GerAircraft/JU390B/PropAnim/JU_Prop_Fast"
PE8 = A + "RusAircraft/PE8Bomber/Rus_PE8_Aircraft_Transport"
PE8_FLY = A + "RusAircraft/PE8Bomber/PropAnims/SK_PE8_Bomber_Anim_Fly_Straight_Fast"
# The soldier meshes themselves: squad unit BPs ignore the hang pose in the editor and hide the canopy from the capture.
SOV_SOLDIER = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier"
GER_SOLDIER = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/GerHazmat/SK_GerHazmat"
# Everything the composer anchors on; names a mesh does not have are skipped.
POINTS = ["W1", "W2", "W3", "W4", "B_Gun_L", "B_Gun_R", "B_Rotor1", "B_Rotor2", "B_Rotor2_001", "B_Rotor2_002",
          "B_Rotor2_003", "B_Rotor2_004", "B_1", "B_2", "B_3", "B_4", "B_5", "B_6", "B_7", "B_8", "B_Large"]
# Key light intensity and ambient that keep the camo readable on the bright icon sky.
LOOK = dict(light_intensity=5.0, ambient_intensity=2.0, fov=18.0, res=2048)
# Level flight seen from the front right and a little above: the key light reaches the camo, and from this low
# angle whatever hangs under the aircraft stays in view.
LEVEL = dict(azimuth=60.0, elevation=14.0, padding=0.9)
SIDE_PROP = dict(azimuth=90.0, elevation=8.0, padding=1.0, res=1024)


def _tank(tank):
    """Same view as the aircraft; the composer hangs it lower than TankAttachZOffset so the turret clears the belly."""
    return dict(parts=[dict(asset=T34 + tank, yaw=0.0, pitch=2.0, roll=-5.0)], **LEVEL)


def _ger_tank(path, anim=None):
    """Crews stand in the reference pose in the editor, so open-topped vehicles are shown without them.
    anim poses the track mesh: the Sd.Kfz. 7 tracks are a stretched slab in the reference pose."""
    return dict(parts=[dict(asset=GER + path, yaw=0.0, pitch=2.0, roll=-5.0, hide=("Crew",), anim=anim, anim_time=0.1)],
                light_intensity=9.0, ambient_intensity=2.5, **LEVEL)


def _para(soldier, chute, hang, azimuth):
    return dict(parts=[dict(asset=soldier, yaw=-90.0, anim=PARA + "Anims/" + hang, anim_time=0.3),
                       dict(asset=PARA + "Mesh/" + chute, yaw=-90.0)],
                azimuth=azimuth, elevation=12.0, padding=1.0, res=1024)


JOBS = {
    "Ju87_Dive": dict(parts=[dict(asset=JU87, yaw=0.0, pitch=-22.0, roll=-12.0, anim=JU87_FLY)],
                      azimuth=35.0, elevation=22.0, padding=0.62),
    "Ho229_Dive": dict(parts=[dict(asset=HO229, yaw=0.0, pitch=-22.0, roll=-12.0, anim=HO229_FLY, anim_time=5.0,
                                   hide_bones=("B_WheelsMaster",))],
                       azimuth=35.0, elevation=22.0, padding=0.85),
    "Ju390_Level": dict(parts=[dict(asset=JU390, yaw=0.0, pitch=2.0, roll=-5.0, anim=JU390_FLY)], **LEVEL),
    "PE8_Level": dict(parts=[dict(asset=PE8, yaw=0.0, pitch=2.0, roll=-5.0, anim=PE8_FLY)], **LEVEL),
    "T3476": _tank("Rus_Vehicle_T3476"),
    "T3485": _tank("Rus_Vehicle_T3485"),
    "T34AA": _tank("Rus_Vehicle_T34AA"),
    "T34100": _tank("Rus_Vehicle_T34100"),
    "T34E": _tank("Rus_Vehicle_T34E"),
    "Flak41": _ger_tank("Ger_Medium/PanzerIII/Flak41/Ger_Vehicle_PanzerIII_AA_Flak41"),
    "Sdkfz7Flak": _ger_tank("Ger_ArmoredCar/Sdkfz7_Flak/Ger_Vehicle_Sdkfz7Flak", SDKFZ7_TRACKS),
    "Sdkfz7Flak36": _ger_tank("Ger_ArmoredCar/Sdkfz7_Flak/Ger_Vehicle_Sdkfz7Flak_Flak36", SDKFZ7_TRACKS),
    "PzIIRadix": _ger_tank("Ger_Light/PanzerII/Ger_Vehicle_PanzerII_F_RadixiteAutocannon"),
    "MarderDual": _ger_tank("Ger_Light/Marder1/Ger_Vehicle_Marder_DualBarrel"),
    "FlakLuchs": _ger_tank("Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs_FlakLuchs"),
    "StugIII": _ger_tank("Ger_Medium/Stug/Ger_Vehicle_StugIII"),
    "PzIVSpotter": _ger_tank("Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1_Spotter"),
    "PzIIIJ": _ger_tank("Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J"),
    "Para_Sov_A": _para(SOV_SOLDIER, "SM_Parachute_Soviet", "AN_Infantry_Parachute_Hang", 25.0),
    "Para_Sov_B": _para(SOV_SOLDIER, "SM_Parachute_Soviet", "AN_Infantry_Parachute_Hang", -30.0),
    "Para_Ger_A": _para(GER_SOLDIER, "SM_Parachute_German", "AN_Infantry_Parachute_Hang_Ger", 25.0),
    "Para_Ger_B": _para(GER_SOLDIER, "SM_Parachute_German", "AN_Infantry_Parachute_Hang_Ger", -30.0),
    "Bomb_Ger500": dict(parts=[dict(asset=BOMBS + "SM_Ger__500gr_Bomb", yaw=0.0)], **SIDE_PROP),
    "Bomb_250": dict(parts=[dict(asset=BOMBS + "SM_Bomb_250", yaw=0.0)], **SIDE_PROP),
    "Bomb_500": dict(parts=[dict(asset=BOMBS + "SM_Bomb_500", yaw=0.0)], **SIDE_PROP),
    "Bomb_2000": dict(parts=[dict(asset=BOMBS + "SM_BombRocket_2000", yaw=0.0)], **SIDE_PROP),
    "Bomb_3000": dict(parts=[dict(asset=BOMBS + "SM_BombRocket_Rus_3000", yaw=0.0)], **SIDE_PROP),
    "Shell_AP": dict(parts=[dict(asset=SHELLS + "SM_AP_CasingShell", yaw=0.0)], **SIDE_PROP),
    "Shell_HE": dict(parts=[dict(asset=SHELLS + "SM_HE_CasingShell", yaw=0.0)], **SIDE_PROP),
}



# Target-area abilities: recon flights, flare curtain, minefields, radixite nuke.
FW189 = A + "GerAircraft/FW_Recon/SK_Ger_Aircraft_FW_Recon"
FW189_FLY = A + "GerAircraft/FW_Recon/ManeuverAnims/Anim_FWRecon_PropFast"
PE2_RECON = A + "RusAircraft/PE2/Rus_PE2_Aircraft_Recon"
ME410 = A + "GerAircraft/ME410/Meshes/SK_Ger_Aircraft_Me410"
MINES = "/Game/RTS_Survival/Meshes/Weapons/Mines/"
FLARE = "/Game/RTS_Survival/SpecialEffects/Flare/"
JOBS.update({
    "Fw189_Level": dict(parts=[dict(asset=FW189, yaw=0.0, pitch=2.0, roll=-5.0, anim=FW189_FLY)], **LEVEL),
    "PE2_Level": dict(parts=[dict(asset=PE2_RECON, yaw=0.0, pitch=2.0, roll=-5.0)], **LEVEL),
    "Me410_Level": dict(parts=[dict(asset=ME410, yaw=0.0, pitch=2.0, roll=-5.0)], **LEVEL),
    "Mine_Teller": dict(parts=[dict(asset=MINES + "SM_TellerMine43", yaw=20.0, pitch=8.0)],
                        azimuth=30.0, elevation=32.0, padding=1.0, res=1024),
    "Mine_TM41": dict(parts=[dict(asset=MINES + "SM_TM41AT_Mine", yaw=20.0, pitch=8.0)],
                      azimuth=30.0, elevation=32.0, padding=1.0, res=1024),
    "Flare_Hanging": dict(parts=[dict(asset=FLARE + "SM_Flare_Parachute", yaw=0.0),
                                 dict(asset=FLARE + "SM_Flare_Cartridge", yaw=0.0)],
                          azimuth=25.0, elevation=10.0, padding=1.0, res=1024),
    "V2_Rocket": dict(parts=[dict(asset="/Game/Ballistic_Missiles/Mesh/v2", yaw=0.0)],
                      azimuth=90.0, elevation=4.0, padding=1.0, res=1024),
})


def run_jobs(names=None):
    for name in names or JOBS:
        job = dict(JOBS[name])
        parts = job.pop("parts")
        render_scene(name, parts, sockets=POINTS, **dict(LOOK, **job))
