"""The dark plate under an enemy tank division on the world map, with a cream rim.

Two modes in one file:
  python -I make_division_plate.py        writes Saved/WorldMapPieces/SM_DivisionPlate.glb (plain Python)
  run_build.sh make_division_plate.py     imports it into /Game/RTS_Survival/WorldCampaign/Divisions/TankPieces
The mesh is a 100 x 100 x 100 unit block with its base at height 0 and cut corners. The formation code
(UWorldDivisionTankFormation::CreatePlate) stretches it to the footprint of the tanks, so the rim is a share of the
size, not a fixed width.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

GLB_PATH = "D:/UE5Projects/RTS_Survival/Saved/WorldMapPieces/SM_DivisionPlate.glb"
NAME = "SM_DivisionPlate"
RIM = "M_WP_DivisionPlate_Rim"


def write_glb():
    import make_enemy_base as base

    # Cream rim (owner's pick, 2026-11-25): a red rim was too dark to stand out on the red Soviet ground.
    base.COLOURS[RIM] = (0.62, 0.57, 0.42, 0.55, 0.0)
    model = base.Model()
    outer = base.octagon(1.0, 0.12)
    inner = base.octagon(0.86, 0.09)
    model.ring_wall(base.STEEL, outer, 0.0, 1.0)
    model.flat_ring(RIM, outer, inner, 1.0)
    model.cap(base.STEEL, inner, 1.0)
    os.makedirs(os.path.dirname(GLB_PATH), exist_ok=True)
    model.write(GLB_PATH, NAME)


def import_into_unreal():
    import unreal

    os.environ["WORLDPIECES_IMPORT_ONLY"] = "1"
    import piece_lib as lib
    import build_pieces

    folder = "/Game/RTS_Survival/WorldCampaign/Divisions/TankPieces"
    masters = lib.ensure_master_materials(lib.OUT_DIR)
    slot_materials, keep_paths = build_pieces.glb_slot_materials(
        masters, GLB_PATH, os.path.dirname(GLB_PATH), folder, "M_WP_EN_Base_", "T_WP_EN_Base_")
    mesh, created = lib.import_mesh(GLB_PATH, folder, NAME)
    if mesh is None:
        lib.log("IMPORT FAILED")
    else:
        lib.finish_mesh(mesh, slot_materials, created, keep_paths)
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
        lib.save(mesh)
    lib.write_log("division_plate.txt")


try:
    import unreal  # noqa: F401
    IN_UNREAL = True
except ImportError:
    IN_UNREAL = False

if IN_UNREAL:
    import_into_unreal()
else:
    write_glb()
