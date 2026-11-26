"""Imports the enemy map object prototypes and builds one Blueprint each (headless, -nullrhi).

Models: Saved/WorldMapUIDesigns/EnemyObjects/<Item>/model (see BRIEF.md there); plinths: .../_Base (make_enemy_base.py).
Assets go to /Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Prototypes. Nothing is registered in the config:
the eight live enemy Blueprints stay as they are until the owner picks.
Items come from the environment variable WORLDPIECES (comma separated, default: all eight).
Log: Saved/WorldMapPieces/build_enemy.txt
"""
import os
import sys
import traceback

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import piece_lib as lib  # noqa: E402
import concrete_lib  # noqa: E402
os.environ["WORLDPIECES_IMPORT_ONLY"] = "1"
import build_pieces  # noqa: E402  (helpers only: the line above stops its own build)

ROOT = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Prototypes"
ENEMY_PARENT = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/BP_EnemyWorldObject.BP_EnemyWorldObject_C"
DESIGNS = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                       "WorldMapUIDesigns", "EnemyObjects")
ITEMS = ["HQ", "Wall", "Factory", "Airfield", "Barracks", "SupplyDepot", "ResearchFacility", "FortifiedCheckpoint"]
PLINTH_HEIGHT = 12.0
SLAB_HEIGHT = 3.0
DEFAULT_BASE = "A"


def build_bases(masters):
    folder = ROOT + "/Base"
    bases = {}
    for letter in ("A", "B", "C"):
        name = "SM_WorldPiece_EnemyBase_" + letter
        glb_path = os.path.join(DESIGNS, "_Base", name + ".glb")
        slot_materials, keep_paths = build_pieces.glb_slot_materials(
            masters, glb_path, os.path.join(DESIGNS, "_Base"), folder, "M_WP_EN_Base_", "T_WP_EN_Base_")
        mesh, created = lib.import_mesh(glb_path, folder, name)
        lib.finish_mesh(mesh, slot_materials, created, keep_paths)
        bases[letter] = mesh
    return bases


def build_item(masters, bases, item):
    folder = ROOT + "/" + item
    model_dir = os.path.join(DESIGNS, item, "model")
    name = "SM_WorldPiece_Enemy" + item
    glb_path = os.path.join(model_dir, name + ".glb")
    if not os.path.exists(glb_path):
        lib.log("NO MODEL " + glb_path)
        return
    prefix = "M_WP_EN_%s_" % item
    slot_materials, keep_paths = build_pieces.glb_slot_materials(
        masters, glb_path, os.path.join(model_dir, "textures"), folder, prefix, "T_WP_EN_%s_" % item)
    concrete = None
    for slot_name in list(slot_materials):
        if "Concrete" not in slot_name:
            continue
        if concrete is None:
            concrete = concrete_lib.concrete_instance(
                masters["concrete"], folder, "MI_WP_EN_%s_CastConcrete" % item, GrimeBaseCm=SLAB_HEIGHT,
                **build_pieces.CONCRETE_LOOK)
            keep_paths.add(folder + "/MI_WP_EN_%s_CastConcrete" % item)
        slot_materials[slot_name] = concrete
    mesh, created = lib.import_mesh(glb_path, folder, name)
    if mesh is None:
        lib.log("IMPORT FAILED " + glb_path)
        return
    lib.finish_mesh(mesh, slot_materials, created, keep_paths)
    lib.build_mission_blueprint(folder, "BP_EnemyProto_" + item, [
        {"name": "Base", "mesh": bases[DEFAULT_BASE]},
        {"name": "Mesh", "mesh": mesh, "location": (0.0, 0.0, PLINTH_HEIGHT), "collision": False},
    ], parent_class_path=ENEMY_PARENT)


def main():
    wanted = [name.strip() for name in os.environ.get("WORLDPIECES", "").split(",") if name.strip()] or ITEMS
    masters = lib.ensure_master_materials(lib.OUT_DIR)
    masters["concrete"] = concrete_lib.ensure_concrete_material()
    bases = build_bases(masters)
    for item in wanted:
        lib.log("=== " + item)
        try:
            build_item(masters, bases, item)
        except Exception:
            lib.log(traceback.format_exc())


lib.LINES.clear()
try:
    main()
except Exception:
    lib.log(traceback.format_exc())
lib.write_log("build_enemy.txt")
