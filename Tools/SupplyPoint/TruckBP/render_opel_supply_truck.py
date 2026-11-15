"""Look-review renders of BP_Ger_OpelSupplyTruck and its wreck (throwaway DebugGame editor, never the shared one).
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Pictures land in Source/Tools/SupplyPoint/TruckBP/. Uses Source/Tools/GlobalAbilityIcons/ue_render_unit.py.
"""
import traceback
import unreal

OUT = "D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/TruckBP"
DEST = "/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/"
TRUCK = DEST + "BP_Ger_OpelSupplyTruck"
WRECK = DEST + "Env_Dest_OpelSupplyTruck"
OTHER_WRECK_MESH = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT1/DestroyedOpel/SM_OpelDestroyed"
PLANE = "/Engine/BasicShapes/Plane"
SPHERE = "/Engine/BasicShapes/Sphere"
NS = {}
exec(open("D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read(), NS)
NS["RENDER_DIR"] = OUT
S = {"t": 0.0, "pass": 0, "handle": None}
GROUND = dict(asset=PLANE, loc=(0, 0, 0), yaw=0, scale=(9.0, 6.0, 1.0))
SLOTS = [dict(asset=SPHERE, loc=(x, 0, 118 + 6), yaw=0, scale=0.12) for x in (-28, -128, -229)]


def render_all(tag):
    world = NS["_world"]()
    unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 0")
    scene = NS["render_scene"]
    try:
        scene(f"truck_rts_camera{tag}", [dict(asset=TRUCK, yaw=-60), GROUND], res=1400, elevation=55.0, azimuth=-30.0)
        scene(f"truck_three_quarter{tag}", [dict(asset=TRUCK, yaw=-60), GROUND], res=1400, elevation=18.0, azimuth=-30.0)
        scene(f"truck_rear_three_quarter{tag}", [dict(asset=TRUCK, yaw=120), GROUND], res=1400, elevation=30.0, azimuth=-30.0)
        scene(f"truck_side_slots{tag}", [dict(asset=TRUCK, yaw=0)] + SLOTS, res=1400, elevation=4.0, azimuth=-90.0)
        scene(f"truck_top_slots{tag}", [dict(asset=TRUCK, yaw=0)] + SLOTS, res=1400, elevation=80.0, azimuth=-90.0)
        scene(f"wreck_blueprint{tag}", [dict(asset=WRECK, yaw=-60), GROUND], res=1000, elevation=35.0, azimuth=-30.0)
        scene(f"wreck_other_mesh{tag}", [dict(asset=OTHER_WRECK_MESH, yaw=-60), GROUND], res=1000, elevation=35.0, azimuth=-30.0)
    finally:
        unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 1")


def tick(delta):
    S["t"] += delta
    try:
        if S["pass"] == 0 and S["t"] > 10.0:
            S["pass"] = 1
            render_all("_warmup")
            S["t"] = 0.0
        elif S["pass"] == 1 and S["t"] > 4.0:
            S["pass"] = 2
            render_all("")
            unreal.log("OPELRENDER DONE")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        unreal.log_error("OPELRENDER FAILED " + traceback.format_exc())
        S["pass"] = 99
        unreal.SystemLibrary.quit_editor()


S["handle"] = unreal.register_slate_post_tick_callback(tick)
