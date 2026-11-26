"""Imports Saved/Berlin/detail/paint/new_<layer>.r8 (detail_paint.py) over the whole Ger_Berlin landscape (weights only,
heights untouched) in the throwaway editor, then reads the weights back to prove the import. Nothing is saved here."""
import unreal

PAINT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/paint/"


def run(tag="new"):
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    land = [a for a in sub.get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)][0]
    x0, y0, x1, y1, comp = [int(v) for v in unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land).split()]
    tl = land.get_editor_property("target_layers")
    names = [str(k) for k in tl.keys()]
    infos = [tl[k].get_editor_property("layer_info_obj") for k in tl.keys()]
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(
        land, x0, y0, x1, y1, "", infos, [PAINT + "%s_%s.r8" % (tag, n) for n in names])
    print("import", ok, names)
    ok2 = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
        land, x0, y0, x1, y1, PAINT + "check_height.r16", infos, [PAINT + "check_%s.r8" % n for n in names])
    print("readback", ok2)
