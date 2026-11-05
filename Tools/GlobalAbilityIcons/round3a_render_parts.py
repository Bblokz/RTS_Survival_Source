"""Round 3a (Icons A agent, 2026-11-05): cut-out renders for the unit-drop ability icons.

Never run this in the shared editor (a SceneCapture there hung it on 2026-11-05). It runs in its own editor process on
the DebugGame build, under Saved/AgentLocks/debuggame_build.lockdir, from PowerShell:
    $env:ICONSA_JOBS = "E50_Ground,Hero_EliteScav"      # optional subset; empty = every job
    & "D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame-Cmd.exe" D:/UE5Projects/RTS_Survival/RTS_Survival.uproject `
        /Engine/Maps/Templates/Template_Default -NoLiveCoding -unattended -nosplash `
        -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/round3a_render_parts.py"
Before launching, check Get-Process UnrealEditor* shows the shared editor: a second RTS_Survival editor becomes the UECP
broker when the shared one goes away (2026-11-05 15:45: other sessions' calls and a QUIT_EDITOR landed in the render
editor). round3a_run_render.ps1 -Jobs <subset> -Dump <path to round3a_dump_da.py> also writes a read-only DA dump.
SK_RailTrooper (SK_PostApoSoldier_Skeleton) with the InfantryMaster hang anim crashed the editor (FSkeletonRemapping).
The editor waits for its shaders, renders every job twice with texture streaming off (the first capture after an asset
loads can be texture-starved), writes renders/round3a_done.txt and quits. Outputs go to
Saved/GlobalAbilityIcons/renders/<job>_{color,mask,normal}.png + <job>_points.json like the round-1 parts.
"""
import os
import time

import unreal

exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/render_ability_parts.py").read())

VEH = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
SQ = "/Game/RTS_Survival/Blueprints/Squads/"
SKEL = SQ + "SquadUnits/Skeletons/"
ANIMS = SQ + "SquadUnits/Anims/"
STAND_AIM = ANIMS + "AimPosMontages/BaseAnimations/Crouch_to_Stand_Rifle_Ironsights"
IDLE_HIP = ANIMS + "Idle/Idle_Rifle_Hip"
JOG = ANIMS + "Movement/Anim_SquadUnit_Jog"
HANG = PARA + "Anims/AN_Infantry_Parachute_Hang_Ger"
CHUTE = PARA + "Mesh/SM_Parachute_German"

# Super-heavies delivered by the Mammut: standing on the ground, seen from the front right and a little above.
HERO_TANK = dict(azimuth=40.0, elevation=16.0, padding=0.92, light_intensity=9.0, ambient_intensity=2.5)
# Soldiers and walkers: full figure from the front left, eye height a little above the hips.
HERO_SOLDIER = dict(azimuth=35.0, elevation=6.0, padding=1.0, res=1024, light_intensity=8.0, ambient_intensity=2.5)


def _hero_tank(path, hide=()):
    return dict(parts=[dict(asset=VEH + path, yaw=0.0, hide=("Crew",) + tuple(hide))], **HERO_TANK)


# Squad unit Blueprints face +X with the mesh turned inside; yaw -10 shows them three-quarter front, turned towards the
# screen right (the camera at azimuth 35 sees world yaw -55 as screen right).
HERO_YAW = -10.0


def _hero_soldier(bp, anim=IDLE_HIP, anim_time=0.5, yaw=HERO_YAW, azimuth=None):
    job = dict(parts=[dict(asset=SQ + bp, yaw=yaw, anim=anim, anim_time=anim_time, hide=("Selection",))],
               **HERO_SOLDIER)
    if azimuth is not None:
        job["azimuth"] = azimuth
    return job


def _para_raw(mesh, azimuth):
    """Raw soldier mesh in the hang pose under the German canopy (squad unit BPs ignore the hang pose)."""
    return dict(parts=[dict(asset=mesh, yaw=-90.0, anim=HANG, anim_time=0.3), dict(asset=CHUTE, yaw=-90.0)],
                azimuth=azimuth, elevation=12.0, padding=1.0, res=1024)


ROUND3A_JOBS = {
    # Ju 390 hanging vehicles: the LEVEL view of the round-1 drops (render_ability_parts._ger_tank).
    "PzIIIRadix": _ger_tank("Ger_Medium/PanzerIII/RadixiteCannon/Ger_Vehicle_PanzerIII_J_RadixiteCannon"),
    "WespeRadix": _ger_tank("Ger_Light/Wespe/Ger_Vehicle_Wespe_RadixiteHowitzer"),
    "PzJagerLaser": _ger_tank("Ger_Light/PanzerJager1/Ger_Vehicle_PanzerJager_Laser"),
    "PzIIIFlamm": _ger_tank("Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_Flamm"),
    "Puma": _ger_tank("Ger_ArmoredCar/Puma/Ger_ArmoredCar_Puma"),
    # Mammut deliveries: the tank big on the ground.
    "E50_Ground": _hero_tank("Ger_Heavy/E50/Ger_Vehicle_E50"),
    "E75_Ground": _hero_tank("Ger_Heavy/E75/Ger_Vehicle_E75"),
    "Maus_Ground": _hero_tank("Ger_Heavy/Maus/Ger_Vehicle_Maus"),
    "JagdTigerLaser_Ground": _hero_tank("Ger_Heavy/JagdTiger/Ger_Vehicle_JagdTiger"),
    "JagdTigerScrap_Ground": _hero_tank("Ger_Heavy/JagdTiger/Scrap128/Ger_Vehicle_JagdTiger_Scrap128"),
    "JagdPantherFlamer_Ground": _hero_tank("Ger_Heavy/JagdPanther/Flamer/Ger_Vehicle_JagdPanther_Flamer"),
    "SturmTiger_Ground": _hero_tank("Ger_Heavy/SturmTiger/Ger_Vehicle_SturmTiger"),
    "TigerII105_Ground": _hero_tank("Ger_Heavy/TigerII/Ger_Vehicle_TigerII_105"),
    "TigerRail_Ground": _hero_tank("Ger_Heavy/Tiger/Ger_Vehicle_TigerH1_Rail"),
    # Landed soldiers: the squad's own Blueprint (weapon and kit included), posed.
    "Hero_EliteScav": _hero_soldier("Ger/EliteInfantry/EliteScavengers/Ger_Inf_EliteScavenger_STG"),
    "Hero_EliteScavFaust": _hero_soldier("Ger/EliteInfantry/EliteScavengers/Ger_Inf_EliteScavenger_Faust"),
    "Hero_Scavenger": _hero_soldier("Ger/Barracks_Infantry/Scavengers/Ger_Inf_Scavenger"),
    "Hero_RailTrooper": _hero_soldier("Ger/EliteInfantry/RailTroopers/Ger_Inf_RailTrooper"),
    "Hero_LightBringer": _hero_soldier("Ger/EliteInfantry/LightBringers/Ger_Inf_LightBringer"),
    "Hero_FeuerSturm": _hero_soldier("Ger/Barracks_Infantry/FlameSquad/Ger_Inf_FlameTrooper", azimuth=55.0),
    "Hero_Vulture": _hero_soldier("Ger/ArmoryUnlocked/GerHazmat/Ger_Inf_Hazmat_Vulture"),
    "Hero_PzGren": _hero_soldier("Ger/T2/PanzerGrenadiers/Ger_Inf_PanzerGrenadierSTG"),
    "Hero_IronStorm": _hero_soldier("Ger/EliteInfantry/IronStorm/Ger_Inf_IronStorm_STG"),
    "Hero_SturmPioneer": _hero_soldier("Ger/T2/SturmPioneer/Ger_Inf_SturmPioneer_Ripper"),
    "Hero_RedSpine": _hero_soldier("Ger/Walkers/RedSpine/Ger_Inf_Walker_RedSpine", JOG, 0.3),
    "Hero_RedSpineB": _hero_soldier("Ger/Walkers/RedSpine/Ger_Inf_Walker_RedSpine", JOG, 1.0, yaw=-40.0),
    "Hero_FelsKrieger": _hero_soldier("Ger/Walkers/FelsKrieger/Ger_Inf_Walker_FelsKrieger", JOG, 0.3),
    # Paratroopers of each squad's mesh (raw mesh + canopy).
    "Para_PostApo_A": _para_raw(SKEL + "PostApocalyptic/Mesh/SK_PostApoSoldier", 25.0),
    "Para_PostApo_B": _para_raw(SKEL + "PostApocalyptic/Mesh/SK_PostApoSoldier", -30.0),
    # (no Para_Rail_*: SK_RailTrooper is on SK_PostApoSoldier_Skeleton and the hang anim's skeleton remap crashed
    # the editor on 2026-11-05; the rail paratroopers use the same PostApo body, Para_PostApo_*)
    "Para_Dystopia_A": _para_raw(SKEL + "Dystopian/SK_RTSDystopia_Soldier", 25.0),
    "Para_Dystopia_B": _para_raw(SKEL + "Dystopian/SK_RTSDystopia_Soldier", -30.0),
    "Para_CyborgFlame_A": _para_raw(SKEL + "CyborgInf(Flame)/Mesh/SK_cyborginfantry1", 25.0),
    "Para_CyborgFlame_B": _para_raw(SKEL + "CyborgInf(Flame)/Mesh/SK_cyborginfantry1", -30.0),
    "Para_CyborgArmored_A": _para_raw(SKEL + "CyborgInf_Flame_/MeshArmored/SK_Cyborg_Armored", 25.0),
    "Para_CyborgArmored_B": _para_raw(SKEL + "CyborgInf_Flame_/MeshArmored/SK_Cyborg_Armored", -30.0),
    # Cargo: the canopy alone (the composer hangs guns and pits under it, scaled up like the cargo chute).
    "Pak40_Gun": dict(parts=[dict(asset=SQ + "TeamWeapons/Pak40_75MM/Mesh/Sk_FullAnimPak40TW", yaw=0.0,
                                  anim=SQ + "TeamWeapons/Pak40_75MM/Animations/Deploy/Anim_Pak40_Packed", anim_time=0.0)],
                      azimuth=40.0, elevation=16.0, padding=0.95, res=1024),
    "Chute_Only": dict(parts=[dict(asset=CHUTE, yaw=-90.0)], azimuth=25.0, elevation=12.0, padding=1.0, res=1024),
    # The pit's own mesh plus the gun in its deployed pose (the BXP Blueprint renders black: build-state material).
    "Pit_Pak40": dict(parts=[dict(asset="/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/"
                                        "SM_GerPit_Pak", yaw=0.0),
                             dict(asset=SQ + "TeamWeapons/Pak40_75MM/Mesh/Sk_FullAnimPak40TW", yaw=0.0,
                                  anim=SQ + "TeamWeapons/Pak40_75MM/Animations/Deploy/Anim_TWPak40_Deployed", anim_time=0.0)],
                      azimuth=40.0, elevation=24.0, padding=0.95, res=1024),
    # Follow-up (lead, 2026-11-05 ~16:50): the four older drops that shared the Spotter Pz III icon.
    "PzIVHarvester": _ger_tank("Ger_Medium/PanzerIV/Harvester/Ger_Harvester_PanzerIV"),
    "PzIHarvester": _ger_tank("Ger_Light/Panzer1/Ger_Harvester_Panzer1"),
    "Pz38tRail": _ger_tank("Ger_Light/Panzer38_RAIL/Ger_Vehicle_Panzer38_RAIL"),  # Panzer38T_R is the rocket one
    "Mortar_GrW42": dict(parts=[dict(asset=SQ + "TeamWeapons/WheeledMortar/BP_TW_Ger80MM_Mortar", yaw=0.0)],
                         azimuth=40.0, elevation=16.0, padding=0.95, res=1024),
}
JOBS.update(ROUND3A_JOBS)

WANTED = [name.strip() for name in os.environ.get("ICONSA_JOBS", "").split(",") if name.strip()] or list(ROUND3A_JOBS)
# Seconds after startup for the first and second pass, and when to quit.
SCHEDULE = [(120.0, "first"), (210.0, "second"), (225.0, "quit")]
_state = {"start": time.time(), "step": 0, "handle": None, "log": []}


def _run_pass(label):
    unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
    try:
        for name in WANTED:
            try:
                run_jobs([name])
                _state["log"].append("%s ok %s" % (label, name))
            except Exception as error:
                _state["log"].append("%s FAIL %s: %s" % (label, name, error))
                clear_studio()
    finally:
        unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 1")


def _tick(delta_seconds):
    if _state["step"] >= len(SCHEDULE):
        return
    at, what = SCHEDULE[_state["step"]]
    if time.time() - _state["start"] < at:
        return
    _state["step"] += 1
    if what == "quit":
        with open(RENDER_DIR + "/round3a_done.txt", "w") as done:
            done.write("\n".join(_state["log"]))
        unreal.unregister_slate_post_tick_callback(_state["handle"])
        unreal.SystemLibrary.quit_editor()
        return
    if what == "first" and os.environ.get("ICONSA_DUMP"):
        try:
            exec(open(os.environ["ICONSA_DUMP"]).read(), {"unreal": unreal})
        except Exception as error:
            _state["log"].append("dump FAIL %s" % error)
    _run_pass(what)
    unreal.log("[IconsA] %s pass done" % what)


_state["handle"] = unreal.register_slate_post_tick_callback(_tick)
