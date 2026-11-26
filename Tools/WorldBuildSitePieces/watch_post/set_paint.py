"""Writes picked paint values into the watch post instances (headless, no GPU): run_headless.sh set_paint.

WATCHPOST_PAINT: semicolon separated '<instance>:<parameter>=<value>' (scalar) or '<instance>:<parameter>=r,g,b'
(vector); '*' as instance means all four. Example: '*:EmissiveStrength=0.3;MI_WatchPost_Roof:EmissiveStrength=0.5'
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                "WorldMapPieces"))
import piece_lib as lib  # noqa: E402

FOLDER = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/WatchPost"
NAMES = ["MI_WatchPost_Body", "MI_WatchPost_Roof", "MI_WatchPost_Lattice", "MI_WatchPost_Net"]
library = unreal.MaterialEditingLibrary
lib.OUT_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                           "WorldBuildSitePieces", "watch_post")
touched = set()
for entry in [item.strip() for item in os.environ.get("WATCHPOST_PAINT", "").split(";") if item.strip()]:
    target, assignment = entry.split(":", 1)
    parameter, value = assignment.split("=", 1)
    for name in NAMES if target == "*" else [target]:
        instance = unreal.load_asset(FOLDER + "/" + name)
        if "," in value:
            r, g, b = [float(part) for part in value.split(",")]
            library.set_material_instance_vector_parameter_value(instance, parameter, unreal.LinearColor(r, g, b, 1.0))
        else:
            library.set_material_instance_scalar_parameter_value(instance, parameter, float(value))
        touched.add(name)
        lib.log("%s %s = %s" % (name, parameter, value))
for name in sorted(touched):
    instance = unreal.load_asset(FOLDER + "/" + name)
    library.update_material_instance(instance)
    lib.save(instance)
lib.write_log("set_paint.txt")
