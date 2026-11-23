"""DF station minimap icon parts + station previews: cut-out renders of the game's own meshes and materials.

Headless only (run_render.ps1), the ability-icon capture pipeline (Source/Tools/GlobalAbilityIcons/ue_render_unit.py),
as Source/Tools/SupplyPoint/MinimapIcons/render_supply_parts.py does for the supply icons. Nothing is saved but PNGs in
Saved/OderWarthe/stations/icons/renders. Jobs from Saved/OderWarthe/stations/icons/run_config.json.
The holder flags are not rendered again: compose_df_icons.py uses the supply icon Flag_<holder> renders.
"""
import json
import time
import traceback

import unreal

WORK = "D:/UE5Projects/RTS_Survival/Saved/OderWarthe/stations/icons/"
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read())
RENDER_DIR = WORK + "renders"

ST = "/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Stations/"
BP = ST + "BP_OW_DFStation"
LOOP = ST + "Meshes/SM_OW_DFStation_LoopAerial"
FF = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/FactionFlags/"

SIDE = dict(azimuth=90.0, elevation=5.0, padding=1.0)
HERO = dict(azimuth=35.0, elevation=22.0, padding=1.0)
RTS = dict(azimuth=-35.0, elevation=48.0, padding=0.92, res=1600)
LOOK = dict(light_intensity=5.0, ambient_intensity=2.0, fov=18.0, res=1024)

JOBS = {
    "Loop_Side": dict(parts=[dict(asset=LOOP, yaw=0.0)], **SIDE),
    "Loop_Side20": dict(parts=[dict(asset=LOOP, yaw=20.0)], **SIDE),
    "Loop_Hero": dict(parts=[dict(asset=LOOP, yaw=0.0)], **HERO),
    "Station_Enemy_RTS": dict(parts=[dict(asset=BP, yaw=0.0, hide=("SignPlayer", "LampGlow"))], **RTS),
    "Station_Player_RTS": dict(parts=[dict(asset=BP, yaw=0.0, hide=("SignEnemy",))],
                               comp_mats={"Flag": FF + "MI_StrikeDivisionPole"}, **RTS),
    "Station_Enemy_Rear": dict(parts=[dict(asset=BP, yaw=180.0, hide=("SignPlayer", "LampGlow"))], **RTS),
}


class AllSockets:
    def __contains__(self, name):
        return True


def log(message):
    unreal.log("[DFIcons] " + message)
    with open(WORK + "render_log.txt", "a", encoding="utf-8") as fh:
        fh.write(time.strftime("%H:%M:%S ") + message + "\n")


def render_job(name):
    job = dict(JOBS[name])
    parts = job.pop("parts")
    comp_mats = job.pop("comp_mats", {})
    spawned = []
    try:
        actors, comps = _stage_parts(parts, spawned)
        for comp in comps:
            for pattern, path in comp_mats.items():
                if pattern.lower() in comp.get_name().lower() and comp.get_name().lower() != "flagpole":
                    comp.set_material(0, unreal.load_asset(path))
        unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncAssetCompilationFinishAll")
        _capture_staged(name, actors, comps, spawned, sockets=AllSockets(), **dict(LOOK, **job))
        log("RENDERED " + name)
    finally:
        _destroy(spawned)


def main():
    with open(WORK + "run_config.json", encoding="utf-8") as fh:
        config = json.load(fh)
    names = config["jobs"]
    if names == "all":
        names = list(JOBS)
    log("start %s" % names)
    unreal.EditorLoadingAndSavingUtils.load_map("/Engine/Maps/Templates/Template_Default")
    unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
    try:
        for attempt in range(int(config.get("passes", 2))):
            for name in names:
                try:
                    render_job(name)
                except Exception:
                    log("JOB %s FAILED\n%s" % (name, traceback.format_exc()))
            log("pass %d done" % (attempt + 1))
    finally:
        unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 1")
    log("done")


main()
