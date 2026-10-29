"""Headless editor jobs for the ambient dust devils (run by patch_dust_devils.sh / bake_sand_masks.sh).

  ambient_fx_job.py patch                 RTS.AmbientFx.PatchNiagaraSystem
  ambient_fx_job.py bake <map> [<map>...]  opens each map and runs RTS.AmbientFx.BakeSandMask
"""
import sys

import unreal

args = sys.argv[1:]
if not args:
    raise SystemExit("usage: ambient_fx_job.py patch | bake <map>...")
if args[0] == "patch":
    unreal.SystemLibrary.execute_console_command(None, "RTS.AmbientFx.PatchNiagaraSystem")
elif args[0] == "bake":
    for map_path in args[1:]:
        unreal.EditorLoadingAndSavingUtils.load_map(map_path)
        unreal.SystemLibrary.execute_console_command(None, "RTS.AmbientFx.BakeSandMask")
