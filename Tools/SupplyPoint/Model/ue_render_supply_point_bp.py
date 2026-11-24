"""Static RTS-camera picture of BP_SupplyPoint_TowerCrane at rest (DebugGame throwaway editor; see run_render_bp.sh).
In the editor world nothing has begun play: the crane sits in its Blueprint rest pose and the flag at its lowered
component position (ASupplyPoint raises it at BeginPlay). Nothing is saved.
"""
import traceback

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Model/v01"
BP = "/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/BP_SupplyPoint_TowerCrane"
TRUCK = "/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/BP_Ger_OpelSupplyTruck"
NS = {}
exec(open("D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read(), NS)
NS["RENDER_DIR"] = OUT
S = {"t": 0.0, "pass": 0}


def render(tag):
    world = NS["_world"]()
    unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 0")
    try:
        spawned = []
        try:
            actors, comps = NS["_stage_parts"]([dict(asset=BP, yaw=0), dict(asset=TRUCK, loc=(120, 330, 0), yaw=0),
                                                dict(asset="/Engine/BasicShapes/Plane", loc=(0, 0, -1), scale=(45.0, 45.0, 1.0))],
                                               spawned)
            frame = [c for c in comps if not (isinstance(c, unreal.StaticMeshComponent) and c.static_mesh
                                               and c.static_mesh.get_name() == "Plane")]
            NS["_capture_staged"](f"ue_bp_rest{tag}", actors, frame, spawned, res=1600, azimuth=54.2, elevation=32.4,
                                  fov=18.0, padding=0.9, light_intensity=8.0)
        finally:
            NS["_destroy"](spawned)
    finally:
        unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 1")


def tick(delta):
    S["t"] += delta
    try:
        if S["pass"] == 0 and S["t"] > 10.0:
            S["pass"] = 1
            render("_warmup")
            S["t"] = 0.0
        elif S["pass"] == 1 and S["t"] > 4.0:
            S["pass"] = 2
            render("")
            unreal.log("SUPPLYBPRENDER DONE")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        unreal.log("SUPPLYBPRENDER FAILED " + traceback.format_exc())
        S["pass"] = 99
        unreal.SystemLibrary.quit_editor()


S["handle"] = unreal.register_slate_post_tick_callback(tick)
