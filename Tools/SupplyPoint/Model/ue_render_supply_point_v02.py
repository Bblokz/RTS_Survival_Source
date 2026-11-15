"""v02 check renders (throwaway Development editor, never the shared one; nothing is saved):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
The v02 FBX are imported in memory into /Game/_SupplyPointPreviewV02 (never saved), because the real assets are locked
by the user's editor. Renders each bay from a low angle at 100 % and 25 %, the three unit loads, and the metal on both
candidate shaders. Logs SUPPLYV02 lines with the bounds min Z of the live (v01) and new radixite fills.
"""
import traceback

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Model/v02"
LIVE = "/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Meshes/"
SUPPLY_HOME = "/Game/_SupplyPointPreviewV02"
SUPPLY_IMPORT_LIBRARY = True
ONLY = None
exec(open("D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Model/ue_import_supply_point.py").read())
NS = {}
exec(open("D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read(), NS)
NS["RENDER_DIR"] = OUT
NEW = [f"SM_SupplyPoint_{k}At{lv}" for k in ("Radixite", "Metal") for lv in (10, 25, 50, 75, 100)] + \
      ["SM_SupplyLoad_Metal", "SM_SupplyLoad_Radixite"]
# stock socket location, yaw, and the azimuth from the bay toward the tower (its open side)
STOCKS = {"Radixite": ((-554.4, -421.8, 12.0), -70.0, 20.0), "Metal": ((0.0, -810.0, 12.0), 0.0, 90.0),
          "VehicleParts": ((554.4, -421.8, 12.0), 70.0, 160.0)}
BURNISHED = "/Game/RTS_Survival/StarterContent/Materials/M_Metal_Burnished_Steel"
METAL_RESOURCE = "/Game/RTS_Survival/Blueprints/Resources/Metal/Meshes/Materials/M_RTS_MetalResource"
S = {"t": 0.0, "pass": 0}


def log(msg):
    unreal.log("SUPPLYV02 " + str(msg))


def preview_import():
    """Returns the imported meshes: the caller keeps them referenced, or GC drops the unsaved packages."""
    keep = []
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        for name in NEW:
            mesh = import_mesh(name)
            set_materials(mesh)
            settle(mesh)
            keep.append(mesh)
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")
    for kind in ("Radixite", "Metal", "VehicleParts"):
        for lv in (10, 25, 50, 75, 100):
            old = unreal.load_asset(LIVE + f"Fills/SM_SupplyPoint_{kind}At{lv}")
            new = unreal.load_asset(SUPPLY_HOME + f"/Fills/SM_SupplyPoint_{kind}At{lv}") if kind != "VehicleParts" else old
            log(f"{kind}At{lv} bounds min z: v01 {old.get_bounding_box().min.z:.1f} cm, v02 {new.get_bounding_box().min.z:.1f} cm "
                f"(stock socket = bay floor top)")
    return keep


def mesh_path(kind, level, new=True):
    if kind == "VehicleParts" or not new:
        return LIVE + f"Fills/SM_SupplyPoint_{kind}At{level}"
    return SUPPLY_HOME + f"/Fills/SM_SupplyPoint_{kind}At{level}"


def name_of(c):
    return c.static_mesh.get_name() if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh else ""


def shoot(out_name, parts, frame_names, azimuth, elevation, padding=1.25, res=1100, fov=24.0, exposure_bias=0.0,
          ambient=2.5):
    spawned = []
    try:
        actors, comps = NS["_stage_parts"](parts, spawned)
        if out_name.startswith("metal_candidates"):
            for c in comps:
                log(f"{out_name} {name_of(c)} at x {c.get_world_location().x - NS['STUDIO_LOCATION'].x:.0f}: "
                    f"{[m.get_name() if m else None for m in c.get_materials()]}")
        frame = [c for c in comps if name_of(c) in frame_names]
        NS["_capture_staged"](out_name, actors, frame, spawned, res=res, azimuth=azimuth, elevation=elevation,
                              fov=fov, padding=padding, light_intensity=8.0, exposure_bias=exposure_bias,
                              ambient_intensity=ambient)
    finally:
        NS["_destroy"](spawned)


def render(tag):
    world = NS["_world"]()
    unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 0")
    try:
        ground = dict(asset="/Engine/BasicShapes/Plane", loc=(0, 0, -1), scale=(45.0, 45.0, 1.0))
        floor = dict(asset="/Engine/BasicShapes/Plane", loc=(0, 0, 12.0), scale=(8.0, 8.0, 1.0))
        for kind, (loc, yaw, az) in STOCKS.items():
            for lv in (100, 25):
                # in its bay, from the side of the bay (the opening faces the tower, so az + 90 looks across it)
                stock = dict(asset=mesh_path(kind, lv), loc=loc, yaw=yaw)
                shoot(f"bay_{kind}_{lv:03d}{tag}", [dict(asset=LIVE + "SM_SupplyPoint_Base", yaw=0), stock, ground],
                      {f"SM_SupplyPoint_{kind}At{lv}"}, az + 90.0, 24.0, padding=1.5)
                # gap check: the fill alone on a plane at the bay-floor height, seen almost edge-on
                alone = dict(asset=mesh_path(kind, lv), loc=(0, 0, 12.0), yaw=0)
                shoot(f"gap_{kind}_{lv:03d}{tag}", [alone, floor], {f"SM_SupplyPoint_{kind}At{lv}"}, -60.0, 4.0,
                      padding=1.05)
        loads = [dict(asset=SUPPLY_HOME + "/Loads/SM_SupplyLoad_Radixite", loc=(-180, 0, 0), yaw=-20),
                 dict(asset=SUPPLY_HOME + "/Loads/SM_SupplyLoad_Metal", loc=(0, 0, 0), yaw=-20),
                 dict(asset=LIVE + "Loads/SM_SupplyLoad_VehicleParts", loc=(180, 0, 0), yaw=-20), ground]
        shoot(f"loads{tag}", loads, {"SM_SupplyLoad_Radixite", "SM_SupplyLoad_Metal", "SM_SupplyLoad_VehicleParts"},
              -70.0, 8.0, padding=1.1, res=1400, fov=20.0)
        # both metal candidates side by side: A = M_RTS_MetalResource (left), B = M_Metal_Burnished_Steel (right)
        ab = [dict(asset=SUPPLY_HOME + "/Fills/SM_SupplyPoint_MetalAt100", loc=(-170, 0, 0), yaw=90),
              dict(asset=SUPPLY_HOME + "/Loads/SM_SupplyLoad_Metal", loc=(-170, 260, 0), yaw=0),
              dict(asset=SUPPLY_HOME + "/Fills/SM_SupplyPoint_MetalAt100", loc=(170, 0, 0), yaw=90,
                   materials=[(0, BURNISHED)]),
              dict(asset=SUPPLY_HOME + "/Loads/SM_SupplyLoad_Metal", loc=(170, 260, 0), yaw=0,
                   materials=[(1, BURNISHED)]),
              # the in-game metal resource node for reference, scaled down, behind the two candidates
              dict(asset="/Game/RTS_Survival/Blueprints/Resources/Metal/Meshes/SM_MetalResource_broken_water_tank",
                   loc=(0, -330, 0), yaw=0, scale=0.45),
              ground]
        shoot(f"metal_candidates{tag}", ab, {"SM_SupplyPoint_MetalAt100", "SM_SupplyLoad_Metal",
                                              "SM_MetalResource_broken_water_tank"}, -90.0, 30.0,
              padding=1.0, res=1400, fov=22.0, exposure_bias=0.0, ambient=0.6)
    finally:
        unreal.SystemLibrary.execute_console_command(world, "r.TextureStreaming 1")


def tick(delta):
    # the FBX import pumps Slate, which re-enters this callback: ignore ticks while a step runs
    if S.get("busy"):
        return
    S["t"] += delta
    S["busy"] = True
    try:
        if S["pass"] == 0 and S["t"] > 8.0:
            S["pass"] = 1
            S["meshes"] = preview_import()
            render("_warmup")
            S["t"] = 0.0
        elif S["pass"] == 1 and S["t"] > 5.0:
            S["pass"] = 2
            render("")
            log("DONE")
            unreal.SystemLibrary.quit_editor()
    except Exception:
        log("FAILED " + traceback.format_exc())
        S["pass"] = 99
        unreal.SystemLibrary.quit_editor()
    finally:
        S["busy"] = False


S["handle"] = unreal.register_slate_post_tick_callback(tick)
