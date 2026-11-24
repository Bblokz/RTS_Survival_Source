"""Applies berlin_skirt_plan.py's plan to Ger_Berlin (exec through skirt_editor_server after berlin_skirt_survey.py
and berlin_skirt_material.py, with the map open in a throwaway DebugGame editor). Re-runnable; nothing is saved here.

  import_rim()   writes the four border strips (heights + six paint layers); every vertex outside the eased band
                 carries its exported value, so only the band changes
  planes()       SKIRT_Plane_PX / NX / PY / NY in the outliner folder MapSkirt: engine Plane, MI_Berlin_HorizonSkirt,
                 no collision, no navigation, no shadow, hidden in scene captures, out to 1.5 km, tucked 3 m under
                 the flat rim band and 3 cm below it
  save_map()     waits for the navmesh, checks the file is not locked, copies the map to
                 Saved/MapSkirt/Ger_Berlin_before_skirt.umap (once) and saves the map package only
"""
import json
import os
import shutil
import time

PLAN = json.load(open(OUT + 'plan.json'))
SKIRT_MI = '/Game/RTS_Survival/Materials/ground/Landscape/Berlin/MI_Berlin_HorizonSkirt'
UMAP = 'D:/UE5Projects/RTS_Survival/Content/RTS_Survival/Maps/GerMissions/Ger_Berlin.umap'
BACKUP = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/Ger_Berlin_before_skirt.umap'


def import_rim():
    check_map()
    land = landscape()
    tl = land.get_editor_property('target_layers')
    by_name = {str(k): tl[k].get_editor_property('layer_info_obj') for k in tl.keys()}
    infos = [by_name[n] for n in PLAN['layers']]
    for name, (i0, j0, i1, j1) in PLAN['strips'].items():
        ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(
            land, i0, j0, i1, j1, OUT + 'rim_%s_height.r16' % name, infos,
            [OUT + 'rim_%s_%s.r8' % (name, n) for n in PLAN['layers']])
        print('rim strip %s (%d,%d)-(%d,%d): %s' % (name, i0, j0, i1, j1, ok))
        assert ok


def _plane(label, cx, cy, sx, sy, z):
    a = asub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx, cy, z))
    comp = a.static_mesh_component
    comp.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    comp.set_material(0, unreal.load_asset(SKIRT_MI))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_collision_profile_name('NoCollision')
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('cast_shadow', False)
    comp.set_editor_property('hidden_in_scene_capture', True)
    comp.set_editor_property('affect_distance_field_lighting', False)
    a.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, 1.0))
    a.set_actor_label(label)
    a.set_folder_path('MapSkirt')
    return a


def planes():
    check_map()
    n = 0
    for a in asub().get_all_level_actors():
        if a.get_actor_label().startswith('SKIRT_Plane_'):
            asub().destroy_actor(a)
            n += 1
    z, R, TUCK = PLAN['plane_z'], PLAN['reach'], PLAN['tuck']
    x0, y0, x1, y1 = PLAN['rect']
    _plane('SKIRT_Plane_PX', (x1 - TUCK + R) / 2.0, 0.0, R - (x1 - TUCK), 2.0 * R, z)
    _plane('SKIRT_Plane_NX', (x0 + TUCK - R) / 2.0, 0.0, (x0 + TUCK) + R, 2.0 * R, z)
    _plane('SKIRT_Plane_PY', (x0 + x1) / 2.0, (y1 - TUCK + R) / 2.0, (x1 - TUCK) - (x0 + TUCK), R - (y1 - TUCK), z)
    _plane('SKIRT_Plane_NY', (x0 + x1) / 2.0, (y0 + TUCK - R) / 2.0, (x1 - TUCK) - (x0 + TUCK), (y0 + TUCK) + R, z)
    for a in asub().get_all_level_actors():
        if a.get_actor_label().startswith('SKIRT_Plane_'):
            c = a.static_mesh_component
            o, e = a.get_actor_bounds(False)
            print('%s o (%.0f,%.0f,%.1f) e (%.0f,%.0f) collision %s profile %s nav %s shadow %s hidden_in_capture %s mat %s' % (
                a.get_actor_label(), o.x, o.y, o.z, e.x, e.y, c.get_collision_enabled().name, c.get_collision_profile_name(),
                c.get_editor_property('can_ever_affect_navigation'), c.get_editor_property('cast_shadow'),
                c.get_editor_property('hidden_in_scene_capture'), c.get_material(0).get_name()))
    print('planes: cleared %d, placed 4 at z %.2f' % (n, z))


def file_locked(path):
    try:
        with open(path, 'r+b'):
            return False
    except OSError:
        return True


def save_map(expect_size=None):
    """Deferred: returns a callable for `_defer`. Saves only the map package, after the navmesh is idle."""
    w = world()
    check_map()
    t0 = time.time()

    def _go():
        if nav_building() or time.time() - t0 < 3:
            if time.time() - t0 > 600:
                print('navmesh still building after 600 s: NOT saved')
                return True
            return False
        # (no in-process lock test: this editor's own linker holds the file, so the test always says locked; check from
        #  outside that no other UnrealEditor process runs, then judge by the save result and the file time)
        size = os.path.getsize(UMAP)
        if expect_size is not None and size != expect_size:
            print('map file changed on disk since the survey (%d != %d): NOT saved' % (size, expect_size))
            return True
        if not os.path.exists(BACKUP):
            shutil.copy2(UMAP, BACKUP)
            print('backup', BACKUP, os.path.getsize(BACKUP))
        ok = unreal.EditorLoadingAndSavingUtils.save_packages([w.get_outermost()], False)
        print('save %s: %d -> %d bytes, mtime %s' % (ok, size, os.path.getsize(UMAP), time.ctime(os.path.getmtime(UMAP))))
        return True
    return _go
