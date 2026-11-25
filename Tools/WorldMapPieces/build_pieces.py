"""Imports the story mission world map pieces and builds their mission Blueprints.

Run headless (no GPU):
  UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=<this file> -nullrhi
Pieces to build come from the environment variable WORLDPIECES (comma separated, default: all that are ready).
Log: Saved/WorldMapPieces/build.txt
"""
import math
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402
import piece_texts  # noqa: E402
import concrete_lib  # noqa: E402

DESIGNS = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                       "WorldMapUIDesigns", "MissionObjects")


def texture_set(texture_dir, folder, stem, flip_green):
    """@return {BaseColor, Normal, ORM: texture} for T_<stem>_D (or _BC), _N, _ORM; None without a colour file."""
    color_path = os.path.join(texture_dir, stem + "_D.png")
    if not os.path.exists(color_path):
        color_path = os.path.join(texture_dir, stem + "_BC.png")
    if not os.path.exists(color_path):
        return None
    textures = {"BaseColor": lib.import_texture(color_path, folder, stem + "_D", "color")}
    normal_path = os.path.join(texture_dir, stem + "_N.png")
    if os.path.exists(normal_path):
        textures["Normal"] = lib.import_texture(normal_path, folder, stem + "_N", "normal", flip_green)
    masks_path = os.path.join(texture_dir, stem + "_ORM.png")
    if os.path.exists(masks_path):
        textures["ORM"] = lib.import_texture(masks_path, folder, stem + "_ORM", "masks")
    return textures


def glb_slot_materials(masters, glb_path, texture_dir, folder, material_prefix, texture_prefix):
    """One instance per glTF material: textured when T_<prefix><name>_D.png exists, else the file's flat factors."""
    slot_materials = {}
    keep_paths = set()
    for material_name, factors in lib.read_glb_materials(glb_path).items():
        short_name = material_name[len(material_prefix):] if material_name.startswith(material_prefix) else material_name
        instance_name = "MI_" + material_name[2:] if material_name.startswith("M_") else "MI_" + material_name
        textures = texture_set(texture_dir, folder, texture_prefix + short_name, flip_green=True)
        if textures and "ORM" in textures:
            instance = lib.make_instance(folder, instance_name, masters["pbr"], textures=textures,
                                         two_sided=factors["two_sided"])
        elif textures:
            instance = lib.make_instance(folder, instance_name, masters["albedo"], textures=textures,
                                         scalars={"Roughness": factors["roughness"], "Metallic": factors["metallic"]},
                                         two_sided=factors["two_sided"])
        else:
            instance = lib.make_instance(folder, instance_name, masters["flat"], vectors={"Color": factors["color"]},
                                         scalars={"Roughness": factors["roughness"], "Metallic": factors["metallic"]},
                                         two_sided=factors["two_sided"])
        slot_materials[material_name] = instance
        keep_paths.add(folder + "/" + instance_name)
        for texture in (textures or {}).values():
            keep_paths.add(texture.get_path_name().split(".")[0])
    return slot_materials, keep_paths


def build_frozen_lake(masters):
    folder = lib.STORY_ROOT + "/FrozenLake"
    model_dir = os.path.join(DESIGNS, "FrozenLake", "model")
    glb_path = os.path.join(model_dir, "SM_WorldPiece_FrozenLake.glb")
    slot_materials, keep_paths = glb_slot_materials(
        masters, glb_path, os.path.join(model_dir, "textures"), folder, "M_WP_FL_", "T_WP_FL_")
    mesh, created = lib.import_mesh(glb_path, folder, "SM_WorldPiece_FrozenLake")
    lib.finish_mesh(mesh, slot_materials, created, keep_paths)
    lib.build_mission_blueprint(folder, "BP_Mission_FrozenLake",
                                [{"name": "Mesh", "mesh": mesh, "scale": 0.96}],
                                piece_texts.TEXTS.get("FrozenLake"))


def build_pripyat(masters):
    folder = lib.STORY_ROOT + "/Pripyat"
    model_dir = os.path.join(DESIGNS, "Pripyat", "Model")
    atlas = lib.import_texture(os.path.join(model_dir, "T_PripyatPiece_D.png"), folder, "T_PripyatPiece_D", "color")
    land = lib.make_instance(folder, "MI_PripyatPiece", masters["albedo"], textures={"BaseColor": atlas},
                             scalars={"Roughness": 0.85, "Metallic": 0.0})
    water = lib.make_instance(folder, "MI_PripyatPiece_Water", masters["albedo"], textures={"BaseColor": atlas},
                              scalars={"Roughness": 0.1, "Metallic": 0.0})
    keep_paths = {folder + "/T_PripyatPiece_D", folder + "/MI_PripyatPiece", folder + "/MI_PripyatPiece_Water"}
    mesh, created = lib.import_mesh(os.path.join(model_dir, "SM_WorldPiece_Pripyat.fbx"), folder,
                                    "SM_WorldPiece_Pripyat")
    # Exact names first: 'M_PripyatPiece' is a prefix of the water slot's name.
    lib.finish_mesh(mesh, {"M_PripyatPiece": land, "M_PripyatPiece_Water": water}, created, keep_paths)
    lib.build_mission_blueprint(folder, "BP_Mission_Pripyat",
                                [{"name": "Mesh", "mesh": mesh, "scale": 1.05}],
                                piece_texts.TEXTS.get("Pripyat"))


GAME = "/Game/RTS_Survival/Blueprints/"
# The setting picked from four in one side-by-side picture (concrete_variants.py, variant B), in centimetres of a
# map-size piece: the Sooted Fortress texture once per 45 cm, pours of 12 cm.
CONCRETE_LOOK = dict(TileCm=45.0, PourHeightCm=12.0, PanelWidthCm=26.0, JointWidthCm=0.6, ReliefStrength=1.5)
FLAK_TOWER_MESH = GAME + "Environment/Berlin/FlakTower/SM_BF_FlakTower_Damaged"
REICHSTAG_BLUEPRINT = GAME + "Environment/Berlin/ReichstagRuin/BP_ReichstagRuin_B2"
PANZER_IV_MESH = GAME + "GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/SM_PzIV_H_TrainingPreview"
HEIGHT_BOOST = 1.25


def build_oder_warthe(masters):
    folder = lib.STORY_ROOT + "/OderWarthe"
    model_dir = os.path.join(DESIGNS, "OderWarthe", "model")
    texture_dir = os.path.join(model_dir, "textures")
    glb_path = os.path.join(model_dir, "SM_WorldPiece_OderWarthe.glb")
    aerial_path = os.path.join(model_dir, "SM_WorldPiece_OderWarthe_Aerial.glb")
    slot_materials, keep_paths = glb_slot_materials(masters, glb_path, texture_dir, folder, "M_WP_OW_", "T_WP_OW_")
    # The plinth top is 12.2 cm over the pivot: ground grime starts there.
    concrete = concrete_lib.concrete_instance(masters["concrete"], folder, "MI_WP_OW_CastConcrete",
                                              GrimeBaseCm=12.2, **CONCRETE_LOOK)
    slot_materials["M_WP_OW_Concrete"] = concrete
    slot_materials["M_WP_OW_TeethConcrete"] = concrete
    keep_paths.add(folder + "/MI_WP_OW_CastConcrete")
    mesh, created = lib.import_mesh(glb_path, folder, "SM_WorldPiece_OderWarthe")
    lib.finish_mesh(mesh, slot_materials, created, keep_paths)
    aerial, created = lib.import_mesh(aerial_path, folder, "SM_WorldPiece_OderWarthe_Aerial")
    lib.finish_mesh(aerial, slot_materials, created, keep_paths)

    # The post top is the highest point of the main mesh and the mount of the aerial (model frame 4.08, 12.24).
    mount = lib.highest_point(mesh)
    to_unreal = lib.infer_axis_map((4.08, 12.24), (mount[0], mount[1]))
    aerial_yaw = lib.map_yaw(to_unreal, 28.0)
    lib.log("aerial mount %s yaw %.1f" % (mount, aerial_yaw))
    lib.build_mission_blueprint(folder, "BP_Mission_OderWarthe", [
        {"name": "Mesh", "mesh": mesh},
        {"name": "Aerial", "mesh": aerial, "location": mount, "rotation": (0.0, aerial_yaw, 0.0),
         "collision": False},
    ], piece_texts.TEXTS.get("OderWarthe"))


def reichstag_components(seat_xy, seat_z, yaw, footprint_x):
    """The instanced meshes of the ruin under one scaled group, without the forecourt and the outer rubble."""
    layout = lib.read_instanced_layout(REICHSTAG_BLUEPRINT)
    wall_points = [transform.translation for entry in layout
                   if "Wall" in entry["mesh"].get_name() or "Tower" in entry["mesh"].get_name()
                   for transform in entry["transforms"]]
    min_x, max_x = min(p.x for p in wall_points), max(p.x for p in wall_points)
    min_y, max_y = min(p.y for p in wall_points), max(p.y for p in wall_points)
    lib.log("reichstag walls x %.0f..%.0f y %.0f..%.0f" % (min_x, max_x, min_y, max_y))
    centre = ((min_x + max_x) * 0.5, (min_y + max_y) * 0.5)
    long_side = max(max_x - min_x, max_y - min_y)
    scale = footprint_x / long_side
    margin = 900.0
    radians = math.radians(yaw)
    offset = (seat_xy[0] - scale * (centre[0] * math.cos(radians) - centre[1] * math.sin(radians)),
              seat_xy[1] - scale * (centre[0] * math.sin(radians) + centre[1] * math.cos(radians)))
    components = [{"name": "Reichstag", "kind": "scene", "location": (offset[0], offset[1], seat_z),
                   "rotation": (0.0, yaw, 0.0), "scale": (scale, scale, scale * HEIGHT_BOOST)}]
    kept = 0
    for entry in layout:
        transforms = [transform for transform in entry["transforms"]
                      if min_x - margin <= transform.translation.x <= max_x + margin
                      and min_y - margin <= transform.translation.y <= max_y + margin
                      and "Ramp" not in entry["mesh"].get_name()]
        lib.log("  %s %s: %d of %d instances" % (entry["name"], entry["mesh"].get_name(), len(transforms),
                                               len(entry["transforms"])))
        if not transforms:
            continue
        kept += len(transforms)
        components.append({"name": "RT_" + entry["name"], "kind": "instanced", "parent": "Reichstag",
                           "mesh": entry["mesh"], "transforms": transforms, "materials": entry["materials"],
                           "collision": False})
    lib.log("reichstag scale %.5f, %d instances kept" % (scale, kept))
    return components


def build_berlin(masters):
    folder = lib.STORY_ROOT + "/Berlin"
    model_dir = os.path.join(DESIGNS, "Berlin", "model")
    glb_path = os.path.join(model_dir, "SM_WorldPiece_Berlin_Base.glb")
    slot_materials, keep_paths = glb_slot_materials(masters, glb_path, model_dir, folder, "M_WP_", "T_WP_")
    base, created = lib.import_mesh(glb_path, folder, "SM_WorldPiece_Berlin_Base")
    lib.finish_mesh(base, slot_materials, created, keep_paths)

    # The largest rubble heap (source 63.8, -43.0) is the highest point of the base: it tells the axis mapping.
    heap = lib.highest_point(base, tolerance=0.5)
    to_unreal = lib.infer_axis_map((63.8, -43.0), (heap[0], heap[1]))
    seat_z = 11.65
    tower_xy = to_unreal((52.7, 6.9))
    tower_yaw = lib.map_yaw(to_unreal, -3.0)
    reichstag_xy = to_unreal((-45.8, -8.3))
    reichstag_yaw = lib.map_yaw(to_unreal, 4.0)
    tower_scale = 61.0 / 4000.0
    # The tower is the real mesh, so its object centimetres are 1 / tower_scale larger than the piece's.
    tower_concrete = concrete_lib.concrete_instance(masters["concrete"], folder, "MI_WP_Berlin_TowerConcrete",
                                                    length_scale=1.0 / tower_scale, **CONCRETE_LOOK)
    components = [
        {"name": "Base", "mesh": base},
        {"name": "FlakTower", "mesh": unreal.load_asset(FLAK_TOWER_MESH),
         "location": (tower_xy[0], tower_xy[1], seat_z), "rotation": (0.0, tower_yaw, 0.0),
         "scale": (tower_scale, tower_scale, tower_scale * HEIGHT_BOOST), "collision": False,
         "materials": {0: tower_concrete}},
    ]
    components += reichstag_components(reichstag_xy, seat_z, reichstag_yaw, 80.4)
    lib.build_mission_blueprint(folder, "BP_Mission_Berlin", components, piece_texts.TEXTS.get("Berlin"))


PANZER_IV_PAINT = (0.30, 0.24, 0.12)
# The size of the block tank in the picked drawing; the true-scale-times-1.3 figure (0.043) read too small.
PANZER_IV_SCALE = 0.057


def build_ore_mountains(masters):
    folder = lib.STORY_ROOT + "/OreMountains"
    model_dir = os.path.join(DESIGNS, "OreMountains", "model")
    glb_path = os.path.join(model_dir, "SM_WorldPiece_OreMountains.glb")
    slot_materials, keep_paths = glb_slot_materials(
        masters, glb_path, os.path.join(model_dir, "textures"), folder, "M_WP_Ore_", "T_WP_Ore_")
    # Headwall, cap slab and wing walls. The base top is 13.2 cm over the pivot: ground grime starts there.
    slot_materials["M_WP_Ore_Concrete"] = concrete_lib.concrete_instance(
        masters["concrete"], folder, "MI_WP_Ore_CastConcrete", GrimeBaseCm=13.2, **CONCRETE_LOOK)
    keep_paths.add(folder + "/MI_WP_Ore_CastConcrete")
    mesh, created = lib.import_mesh(glb_path, folder, "SM_WorldPiece_OreMountains")
    lib.finish_mesh(mesh, slot_materials, created, keep_paths)

    # The importer puts glTF +Z (the adit front) on Unreal +Y: measured on the Oder-Warthe and Berlin files.
    panzer = unreal.load_asset(PANZER_IV_MESH)
    # The preview mesh is one slot with the hologram material and its UVs do not fit the camo texture of the tank
    # (tried: it renders near-black), so it gets a plain dark-yellow paint.
    panzer_material = lib.make_instance(folder, "MI_WP_Ore_Panzer", masters["flat"],
                                        vectors={"Color": PANZER_IV_PAINT},
                                        scalars={"Roughness": 0.55, "Metallic": 0.0})
    lib.build_mission_blueprint(folder, "BP_Mission_OreMountains", [
        {"name": "Mesh", "mesh": mesh},
        {"name": "Panzer", "mesh": panzer, "location": (0.0, 58.5, 13.2), "rotation": (0.0, 90.0, 0.0),
         "scale": PANZER_IV_SCALE, "collision": False, "materials": {0: panzer_material}},
    ], piece_texts.TEXTS.get("OreMountains"))


BUILDERS = {
    "OreMountains": build_ore_mountains,
    "OderWarthe": build_oder_warthe,
    "Berlin": build_berlin,
    "FrozenLake": build_frozen_lake,
    "Pripyat": build_pripyat,
}


def main():
    wanted = [name.strip() for name in os.environ.get("WORLDPIECES", ",".join(BUILDERS)).split(",") if name.strip()]
    masters = lib.ensure_master_materials(lib.OUT_DIR)
    masters["concrete"] = concrete_lib.ensure_concrete_material()
    if unreal.EditorAssetLibrary.does_directory_exist(concrete_lib.TEST_FOLDER):
        unreal.EditorAssetLibrary.delete_directory(concrete_lib.TEST_FOLDER)
    for name in wanted:
        lib.log("=== " + name)
        try:
            BUILDERS[name](masters)
        except Exception:
            lib.log(traceback.format_exc())


os.makedirs(lib.OUT_DIR, exist_ok=True)
try:
    main()
except Exception:
    lib.log(traceback.format_exc())
lib.write_log("build.txt")
