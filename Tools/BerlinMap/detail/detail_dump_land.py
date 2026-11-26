"""Read-only: export the whole Ger_Berlin landscape (heights + paint weights) from a -nullrhi commandlet."""
import os
import unreal
MAP = "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin"
OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/dump/"
os.makedirs(OUT, exist_ok=True)
world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
land = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LandscapeProxy)][0]
info = unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land)
print("DETAIL_LAND info", repr(info))
x0, y0, x1, y1, comp = [int(v) for v in info.split()]
tl = land.get_editor_property("target_layers")
names = [str(k) for k in tl.keys()]
infos = [tl[k].get_editor_property("layer_info_obj") for k in tl.keys()]
ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
    land, x0, y0, x1, y1, OUT + "land_height.r16", infos, [OUT + "land_%s.r8" % n for n in names])
loc, sc = land.get_actor_location(), land.get_actor_scale3d()
with open(OUT + "land_grid.txt", "w") as f:
    f.write("%d %d %d %d\n%f %f %f\n%f %f %f\n%s\n" % (x0, y0, x1, y1, loc.x, loc.y, loc.z, sc.x, sc.y, sc.z, " ".join(names)))
print("DETAIL_LAND export", ok, names)
