"""Assembled look check of the imported supply point meshes in Unreal (throwaway editor, never the shared one):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Stages base + upper works + trolley + rope + hook at the rest pose, stocks at 100 %, the flag raised, and
BP_Ger_OpelSupplyTruck on the park point with a unit load in each CargoSlot_* component. Renders from the RTS camera,
plus flag close-ups with MI_Emblem_DirectorOfShield and MI_StrikeDivisionPole. Nothing is saved.
Prints SUPPLYRENDER lines (cargo slot world positions vs the Blender bed gaps).
"""
import math
import traceback

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Model/v01"
M = "/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Meshes/"
TRUCK = "/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/BP_Ger_OpelSupplyTruck"
EMBLEMS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/Emblems/"
STRIKE_FLAG = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/FactionFlags/MI_StrikeDivisionPole"
PLANE = "/Engine/BasicShapes/Plane"
NS = {}
exec(open("D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read(), NS)
NS["RENDER_DIR"] = OUT
S = {"t": 0.0, "pass": 0}

# Rest pose (sp_layout): slew 200 deg in Blender = UE yaw -200; trolley 3.4 m; hook pivot 6.2 m.
SLEW = (0.0, -220.0, 675.0)
SLEW_YAW = -200.0
TROLLEY_R = 340.0
HOOK_Z = 620.0
FLAG_LOWERED = (-694.4, -790.2, 220.0)
FLAG_YAW = -215.8
FLAG_RAISE = 600.0
STOCKS = (("Radixite", (-554.4, -421.8, 12.0), -70.0), ("Metal", (0.0, -810.0, 12.0), 0.0),
          ("VehicleParts", (554.4, -421.8, 12.0), 70.0))
# Park point = bottom centre of the Opel's bounding box; the Opel mesh origin is 5.25 cm behind it.
PARK = (125.0 - 5.25, 330.0, 0.0)
BLENDER_GAPS_FROM_BOX_CENTRE = {"Radixite": -28.0, "Metal": -128.0, "VehicleParts": -229.0}


def log(msg):
    unreal.log("SUPPLYRENDER " + str(msg))


def flag_rotation():
    """Banner local +X (hoist -> fly) along the fly yaw, local +Y (bottom -> top edge) up."""
    a = math.radians(FLAG_YAW)
    return unreal.MathLibrary.make_rot_from_xy(unreal.Vector(math.cos(a), math.sin(a), 0.0), unreal.Vector(0, 0, 1))


def parts(flag_material=None):
    a = math.radians(SLEW_YAW)
    tx, ty = SLEW[0] + TROLLEY_R * math.cos(a), SLEW[1] + TROLLEY_R * math.sin(a)
    fr = flag_rotation()
    out = [
        dict(asset=M + "SM_SupplyPoint_Base", loc=(0, 0, 0), yaw=0),
        dict(asset=M + "SM_SupplyPoint_UpperWorks", loc=SLEW, yaw=SLEW_YAW),
        dict(asset=M + "SM_SupplyPoint_Trolley", loc=(tx, ty, SLEW[2]), yaw=SLEW_YAW),
        dict(asset=M + "SM_SupplyPoint_Rope", loc=(tx, ty, SLEW[2]), yaw=SLEW_YAW, scale=(1.0, 1.0, (SLEW[2] - HOOK_Z) / 100.0)),
        dict(asset=M + "SM_SupplyPoint_HookBlock", loc=(tx, ty, HOOK_Z), yaw=SLEW_YAW),
        dict(asset=M + "SM_SupplyPoint_Flag", loc=(FLAG_LOWERED[0], FLAG_LOWERED[1], FLAG_LOWERED[2] + FLAG_RAISE),
             yaw=fr.yaw, pitch=fr.pitch, roll=fr.roll,
             materials=[(0, flag_material)] if flag_material else ()),
        dict(asset=PLANE, loc=(0, 0, -1), yaw=0, scale=(45.0, 45.0, 1.0)),
    ]
    for kind, loc, yaw in STOCKS:
        out.append(dict(asset=M + f"Fills/SM_SupplyPoint_{kind}At100", loc=loc, yaw=yaw))
    out.append(dict(asset=TRUCK, loc=PARK, yaw=0))
    return out


def stage(flag_material=None):
    spawned = []
    actors, comps = NS["_stage_parts"](parts(flag_material), spawned)
    truck = spawned[-1]
    slots = {c.get_name(): c for c in truck.get_components_by_class(unreal.SceneComponent)
             if c.get_name().startswith("CargoSlot_")}
    studio = NS["STUDIO_LOCATION"]
    for kind in ("Radixite", "Metal", "VehicleParts"):
        slot = slots.get("CargoSlot_" + kind)
        if slot is None:
            log(f"missing CargoSlot_{kind}; components: {sorted(slots)}")
            continue
        w = slot.get_world_location() - studio
        rel = slot.get_editor_property("relative_location")
        gap_world_x = 125.0 + BLENDER_GAPS_FROM_BOX_CENTRE[kind]
        log(f"CargoSlot_{kind} relative ({rel.x:.1f}, {rel.y:.1f}, {rel.z:.1f}) world ({w.x:.1f}, {w.y:.1f}, {w.z:.1f}) "
            f"blender gap world x {gap_world_x:.1f} -> offset {w.x - gap_world_x:+.1f} cm")
        load = NS["_spawn_asset"](M + f"Loads/SM_SupplyLoad_{kind}", -90.0, (w.x, w.y, w.z))
        spawned.append(load)
        actors.append(load)
        comps.extend(NS["_visible_mesh_components"]([load], ()))
    return spawned, actors, comps


def mesh_name(c):
    if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
        return c.static_mesh.get_name()
    return ""


def render(tag):
    world = NS["_world"]()
    unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 0")
    try:
        spawned, actors, comps = stage()
        frame = [c for c in comps if mesh_name(c) != "Plane"]
        try:
            NS["_capture_staged"](f"ue_assembled{tag}", actors, frame, spawned, res=1600, azimuth=54.2,
                                  elevation=32.4, fov=18.0, padding=0.9, light_intensity=8.0)
        finally:
            NS["_destroy"](spawned)
        for name, material in (("shield", EMBLEMS + "MI_Emblem_DirectorOfShield"), ("strike", STRIKE_FLAG)):
            spawned, actors, comps = stage(material)
            flag = [c for c in comps if mesh_name(c) == "SM_SupplyPoint_Flag"]
            try:
                NS["_capture_staged"](f"ue_flag_{name}{tag}", actors, flag, spawned, res=900, azimuth=54.2,
                                      elevation=32.4, fov=18.0, padding=2.6, light_intensity=8.0)
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
            log("DONE")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        log("FAILED " + traceback.format_exc())
        S["pass"] = 99
        unreal.SystemLibrary.quit_editor()


S["handle"] = unreal.register_slate_post_tick_callback(tick)
