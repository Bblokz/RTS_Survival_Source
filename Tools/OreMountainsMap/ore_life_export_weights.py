"""Headless read-only: export the live Ore landscape weightmaps (whole landscape) for the life pass blending.
Writes Saved/OreMountains/life/weights/<Layer>.raw (uint8, verts_x * verts_y, row = Y) + region.json."""
import json
import os
import sys

import unreal

sys.path.append("D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap")
import ore_map_build as omb  # noqa: E402  (LAYER_INFOS)

# ORE_LIFE_WEIGHTS_OUT: another folder, e.g. to check the saved map after the blend without touching the baseline.
OUT = os.environ.get("ORE_LIFE_WEIGHTS_OUT", "D:/UE5Projects/RTS_Survival/Saved/OreMountains/life/weights/")
unreal.EditorLoadingAndSavingUtils.load_map("/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains")
os.makedirs(OUT, exist_ok=True)
landscape = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
             if isinstance(a, unreal.Landscape)][0]
info = unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(landscape)
names = [str(n) for n in landscape.get_target_layer_names()]
layers = [unreal.load_asset(omb.LAYER_INFOS[n]) for n in names if n in omb.LAYER_INFOS]
used = [n for n in names if n in omb.LAYER_INFOS]
files = [OUT + n + ".raw" for n in used]
ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(landscape, 0, 0, 756, 756, OUT + "height.raw", layers, files)
json.dump({"grid_info": str(info), "layers": used, "files": files, "region": [0, 0, 756, 756], "ok": bool(ok)},
          open(OUT + "region.json", "w"), indent=1)
unreal.log_warning("ORE_LIFE_EXPORT ok=%s layers=%s info=%s" % (ok, used, info))
