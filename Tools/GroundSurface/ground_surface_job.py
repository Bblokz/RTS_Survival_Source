"""Headless editor job for the ground surface maps (run by bake_surface_maps.sh).

  ground_surface_job.py bake <map> [<map>...]  opens each map and runs RTS.GroundSurface.BakeSurfaceMap
"""
import sys

import unreal

args = sys.argv[1:]
if len(args) < 2 or args[0] != "bake":
    raise SystemExit("usage: ground_surface_job.py bake <map>...")
for map_path in args[1:]:
    unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    unreal.SystemLibrary.execute_console_command(None, "RTS.GroundSurface.BakeSurfaceMap")
