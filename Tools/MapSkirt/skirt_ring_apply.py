"""Applies skirt_ring_plan.py's plan in a throwaway editor that has the map open (exec through a command server).

    SKIRT_NAME = 'Ger_FirstMission'; exec(open(<this file>).read()); apply_all()

Steps (all re-runnable; actors are labelled SKIRT_* in the outliner folder MapSkirt):
  add_ring()    adds the ring sections (one row run per AddLandscapeComponents call, so lake holes are never filled)
                and imports ring_height / ring_<layer> over the extended rectangle (existing sections get their own
                values back: unchanged)
  planes()      survival horizon skirt planes: 4 strips around the extended rectangle + fills for far visible void
  guards()      blocking volumes over ring sections on the playable side of the map's blockers
"""
import json

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/MapSkirt/%s/' % SKIRT_NAME
PLAN = json.load(open(ROOT + 'ring_plan.json'))
SKIRT_MATERIAL = '/Game/RTS_Survival/SurvivalMode/Maps/Materials/M_SM_HorizonSkirt'
REACH = 150000.0
TUCK = 400.0
COMP = 15


def _asub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def _landscape():
    lands = [a for a in _asub().get_all_level_actors() if isinstance(a, unreal.Landscape)]
    assert len(lands) == 1
    return lands[0]


def _clear(prefix):
    n = 0
    for a in _asub().get_all_level_actors():
        if a.get_actor_label().startswith(prefix):
            _asub().destroy_actor(a)
            n += 1
    return n


def _layer_infos(land):
    tl = land.get_editor_property('target_layers')
    by_name = {str(k): tl[k].get_editor_property('layer_info_obj') for k in tl.keys()}
    return [by_name[n] for n in PLAN['layers']]


def add_ring():
    land = _landscape()
    rows = {}
    for c in PLAN['ring']:
        rows.setdefault(c['cj'], []).append(c['ci'])
    added = 0
    for cj, cis in rows.items():
        cis = sorted(cis)
        start = prev = cis[0]
        for ci in cis[1:] + [None]:
            if ci is not None and ci == prev + 1:
                prev = ci
                continue
            added += unreal.RTSShortsLandscapeLibrary.add_landscape_components(land, start, cj, prev, cj)
            if ci is not None:
                start = prev = ci
    g = PLAN['ext_grid']
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(
        land, g[0], g[1], g[2], g[3], ROOT + 'ring_height.r16', _layer_infos(land),
        [ROOT + 'ring_%s.r8' % n for n in PLAN['layers']])
    print('ring: added %d sections, import %s, grid now %s' % (
        added, ok, unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land)))


def _plane(label, cx, cy, sx, sy, z):
    a = _asub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(cx, cy, z))
    comp = a.static_mesh_component
    comp.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    comp.set_material(0, unreal.load_asset(SKIRT_MATERIAL))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('cast_shadow', False)
    comp.set_editor_property('hidden_in_scene_capture', True)
    a.set_actor_scale3d(unreal.Vector(sx / 100.0, sy / 100.0, 1.0))
    a.set_actor_label(label)
    a.set_folder_path('MapSkirt')
    return a


def planes():
    print('cleared', _clear('SKIRT_Plane_'))
    z = PLAN['skirt_z']
    x0, y0, x1, y1 = PLAN['ext_rect']
    R = REACH
    _plane('SKIRT_Plane_PX', (x1 - TUCK + R) / 2.0, 0.0, R - (x1 - TUCK), 2.0 * R, z)
    _plane('SKIRT_Plane_NX', (x0 + TUCK - R) / 2.0, 0.0, (x0 + TUCK) + R, 2.0 * R, z)
    _plane('SKIRT_Plane_PY', (x0 + x1) / 2.0, (y1 - TUCK + R) / 2.0, x1 - x0, R - (y1 - TUCK), z)
    _plane('SKIRT_Plane_NY', (x0 + x1) / 2.0, (y0 + TUCK - R) / 2.0, x1 - x0, (y0 + TUCK) + R, z)
    # merge fills that share x0/x1 on consecutive rows
    fills = sorted(PLAN['fills'], key=lambda f: (f['x0'], f['x1'], f['y0']))
    merged = []
    for f in fills:
        m = merged[-1] if merged else None
        if m and m['x0'] == f['x0'] and m['x1'] == f['x1'] and abs(m['y1'] - f['y0']) < 1.0:
            m['y1'] = f['y1']
        else:
            merged.append(dict(f))
    for k, f in enumerate(merged):
        _plane('SKIRT_Plane_Fill_%03d' % k, (f['x0'] + f['x1']) / 2.0, (f['y0'] + f['y1']) / 2.0,
               f['x1'] - f['x0'] + 2 * TUCK, f['y1'] - f['y0'] + 2 * TUCK, z)
    print('planes: 4 strips + %d fills at z %.0f' % (len(merged), z))


def guards():
    print('cleared', _clear('SKIRT_Guard_'))
    src = None
    for a in _asub().get_all_level_actors():
        if a.get_class().get_name() == 'BlockingVolume' and not a.get_actor_label().startswith('SKIRT_'):
            src = a
            break
    rows = sorted(PLAN['guards'], key=lambda g: (g['y0'], g['x0']))
    merged = []
    for g in rows:
        m = merged[-1] if merged else None
        if m and abs(m['y0'] - g['y0']) < 1 and abs(m['x1'] - g['x0']) < 1:
            m['x1'] = g['x1']
            m['zmax'] = max(m['zmax'], g['zmax'])
        else:
            merged.append(dict(g))
    for k, g in enumerate(merged):
        a = _asub().duplicate_actor(src)
        a.set_actor_label('SKIRT_Guard_%02d' % k)
        a.set_folder_path('MapSkirt')
        z0, z1 = -2500.0, g['zmax'] + 600.0
        a.set_actor_location(unreal.Vector((g['x0'] + g['x1']) / 2, (g['y0'] + g['y1']) / 2, (z0 + z1) / 2), False, False)
        a.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
        a.set_actor_scale3d(unreal.Vector((g['x1'] - g['x0']) / 200.0, (g['y1'] - g['y0']) / 200.0, (z1 - z0) / 200.0))
    print('guards: %d blocking volumes over %d ring sections' % (len(merged), len(PLAN['guards'])))


def apply_all():
    add_ring()
    planes()
    guards()
