"""Ger_Berlin start crater look fix (owner's request 2026-11-18, concept A "glassed pan in an old bomb crater").

The stock Resource_Radixite_Crater0 rim (MM_Crater, desert sand diffuse) read as a pale sand ring on the grey Berlin
ground. Fix, all inside the throwaway editor (nothing saved here):
- rim_material(): MainMesh material override MI_BerlinRuin_Rubble_Foot on the placed RES_Start_Radixite_Crater (the
  Blueprint stays stock). A child of MM_Crater with the glass textures still read yellow: MM_Crater tints its diffuse.
- hide_crystal_decals(): the stock crystals' own decals (dark splats + crystal-shard sprites) are hidden on the placed
  RES_Start_Radixite_* instances, so the field reads as a clean glazed pan (Berlin Radixite has no blight).
- export_region() / import_region(): the landscape patch round the crater, painted with the BerlinGlass layer by
  crater_paint.py (offline numpy) between the two calls. Heights are written back unchanged.
- decals(): RES_Start_CraterDecal_* ash-soot and glass-shard rubble decals (Berlin RubbleDecals kit) over the rim.

    sys.path.insert(0, ".../poi"); import start_crater_fix as cf
    cf.rim_material(); cf.hide_crystal_decals(); cf.export_region(); <python crater_paint.py>; cf.import_region(); cf.decals()
"""
import math
import os

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/start_resources/land/"
CENTRE = (17300.0, 47300.0)
HALF = 2600.0                       # cm: patch half size (crystals reach ~20 m from the centre)
DECALS = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/MI_BerlinRubbleDecal_"
PREFIX = "RES_Start_CraterDecal_"


def _sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _landscape():
    return [a for a in _sub().get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)][0]


def _actor(label):
    return [a for a in _sub().get_all_level_actors() if a.get_actor_label() == label][0]


RIM_MATERIAL = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/MI_BerlinRuin_Rubble_Foot"


def rim_material():
    crater = _actor("RES_Start_Radixite_Crater")
    crater.modify()
    mesh = [c for c in crater.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == "MainMesh"][0]
    mesh.set_material(0, unreal.load_asset(RIM_MATERIAL))
    print("rim material", [m.get_name() for m in mesh.get_materials()])


def hide_crystal_decals():
    n = 0
    for a in _sub().get_all_level_actors():
        if a.get_actor_label().startswith("RES_Start_Radixite"):
            a.modify()
            for d in a.get_components_by_class(unreal.DecalComponent):
                d.set_visibility(False)
                n += 1
    print("crystal decals hidden", n)


def _grid():
    land = _landscape()
    loc, sc = land.get_actor_location(), land.get_actor_scale3d()
    i0 = int(math.floor((CENTRE[0] - HALF - loc.x) / sc.x))
    i1 = int(math.ceil((CENTRE[0] + HALF - loc.x) / sc.x))
    j0 = int(math.floor((CENTRE[1] - HALF - loc.y) / sc.y))
    j1 = int(math.ceil((CENTRE[1] + HALF - loc.y) / sc.y))
    return land, (i0, j0, i1, j1), loc, sc


def _layers(land):
    tl = land.get_editor_property("target_layers")
    return [str(k) for k in tl.keys()], [tl[k].get_editor_property("layer_info_obj") for k in tl.keys()]


def export_region(tag="orig"):
    os.makedirs(OUT, exist_ok=True)
    land, (i0, j0, i1, j1), loc, sc = _grid()
    names, infos = _layers(land)
    ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
        land, i0, j0, i1, j1, OUT + "%s_height.r16" % tag, infos, [OUT + "%s_%s.r8" % (tag, n) for n in names])
    with open(OUT + "grid.txt", "w") as f:
        f.write("%d %d %d %d\n%f %f\n%f %f\n%s\n" % (i0, j0, i1, j1, loc.x, loc.y, sc.x, sc.y, " ".join(names)))
    print("export", tag, ok, (i0, j0, i1, j1), names)
    assert ok


def import_region(tag="new"):
    land, (i0, j0, i1, j1), loc, sc = _grid()
    names, infos = _layers(land)
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(
        land, i0, j0, i1, j1, "", infos, [OUT + "%s_%s.r8" % (tag, n) for n in names])
    print("import", tag, ok)
    assert ok


def _ground(x, y):
    ignore = []
    for _ in range(12):
        hit = unreal.SystemLibrary.line_trace_single(_world(), unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore,
                                                     unreal.DrawDebugTrace.NONE, True)
        if hit is None:
            return 0.0
        t = hit.to_tuple()
        if "Landscape" in t[9].get_class().get_name():
            return t[4].z
        ignore.append(t[9])
    return 0.0


# (material suffix, x, y, yaw, size along atlas x/y in cm): soot round the bowl, glass shards spilling out of it
DECAL_SPOTS = [
    ("09_AshSoot", 17300, 47300, 0, 520), ("09_AshSoot", 17650, 46850, 70, 380), ("09_AshSoot", 16900, 47750, 200, 360),
    ("10_GlassShards", 17000, 46950, 130, 300), ("10_GlassShards", 17700, 47700, 310, 300),
    ("10_GlassShards", 16700, 47350, 40, 260), ("10_GlassShards", 17950, 47250, 250, 260),
]


def decals():
    n = 0
    for a in _sub().get_all_level_actors():
        if a.get_actor_label().startswith(PREFIX):
            _sub().destroy_actor(a)
    for k, (mat, x, y, yaw, size) in enumerate(DECAL_SPOTS):
        a = _sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, _ground(x, y) + 30),
                                          unreal.Rotator(0, -90, yaw))
        a.decal.set_decal_material(unreal.load_asset(DECALS + mat))
        a.decal.set_editor_property("decal_size", unreal.Vector(180.0, size, size))
        a.set_actor_label("%s%02d" % (PREFIX, k))
        a.set_folder_path("Berlin/StartResources")
        n += 1
    print("decals", n)
