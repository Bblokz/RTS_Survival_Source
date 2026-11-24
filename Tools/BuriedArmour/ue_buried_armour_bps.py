"""Buried 1945 armour: builds BP_BuriedArmour_Base (parent ABuriedVehicleSite) and the five BP_BuriedArmour_<Vehicle>
children, gives each sand mound complex collision and Scav_ sockets at its foot, then (BA_STEP=check) fresh-loads
every child, spawns it in an unsaved temp level and checks the parts sit where the poses say. Never saves a map.

Needs a DLL with ABuriedVehicleSite (DebugGame since 2026-11-23):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
    -unattended -nosplash -NoLiveCoding -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
  BA_STEP=build (default) or BA_STEP=check. Report: Saved/BuriedArmour/bps_<step>.json.
"""
import json
import math
import os

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/BuriedArmour/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/BuriedArmour/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/BuriedArmour'
MESHES = ROOT + '/Meshes'
BASE = 'BP_BuriedArmour_Base'
SCAV_TEMPLATE = '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/BP_ScavengeableObject'
REWARD_WIDGET = '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/BP_Comp_ScavReward_Widget'
SKIP = ('SK_PostApoSoldier', 'SM_GerPit_Shell')
# Starter scavenge seconds per vehicle (owner tunes after play-testing).
SCAV_TIME = {'StuGIII': 30.0, 'Wespe': 25.0, 'FlakLuchs': 25.0, 'PzIV_F1_Assault': 30.0, 'Sdkfz251_MortarTW': 20.0}
HIGH_SAND_CM = 110.0   # sand above this is under the nav blocker (with the hull footprint)
BLOCK_PAD_CM = 40.0
SOCKET_OUT_CM = 110.0  # Scav_ sockets this far outside the nav blocker
SOCKET_SPACING_CM = 120.0
STEP = os.environ.get('BA_STEP', 'build')

EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
REPORT = {'step': STEP, 'log': [], 'errors': []}
os.makedirs(OUT, exist_ok=True)


def log(m):
    REPORT['log'].append(m)
    unreal.log('BURIEDBPS ' + m)


def err(m):
    REPORT['errors'].append(m)
    unreal.log_error('BURIEDBPS ' + m)


def save_report():
    json.dump(REPORT, open(OUT + 'bps_%s.json' % STEP, 'w'), indent=1, default=str)


def save(path):
    ok = EAL.save_asset(path, only_if_is_dirty=False)
    disk = 'D:/UE5Projects/RTS_Survival/Content/' + path[len('/Game/'):] + '.uasset'
    if not ok or not os.path.exists(disk):
        err('save failed ' + path)
    return ok


def quat_rot(q):
    return unreal.Quat(q[0], q[1], q[2], q[3]).rotator()


def xf(d):
    return unreal.Transform(unreal.Vector(*d['loc']), quat_rot(d['quat']), unreal.Vector(*d['scale']))


def pose_xf(pose):
    return unreal.Transform(unreal.Vector(*pose['location_cm']), quat_rot(pose['quat_xyzw']), unreal.Vector(1, 1, 1))


def load_data():
    dump = json.load(open(HERE + 'BuriedArmour_vehicle_dump.json'))['vehicles']
    poses = json.load(open(HERE + 'BuriedArmour_poses.json'))
    return dump, poses


def kept_parts(rows):
    """Hull/turret static meshes + weapon skeletal meshes; no tracks, wheels, crew or loose rounds."""
    out, names = [], {}
    for r in rows:
        mesh = r.get('mesh', '')
        name = mesh.split('/')[-1]
        if not name or not r.get('visible', True) or any(s in name for s in SKIP) or 'Tracks' in name \
                or 'Wheels' in name:
            continue
        names[name] = names.get(name, 0) + 1
        comp_name = name if names[name] == 1 else '%s_%d' % (name, names[name])
        out.append(dict(r, comp_name=comp_name))
    return out


# ---------------------------------------------------------------- sand mesh: collision, nav box, sockets
def box_corners(bb_min, bb_max):
    return [unreal.Vector(x, y, z) for x in (bb_min.x, bb_max.x) for y in (bb_min.y, bb_max.y)
            for z in (bb_min.z, bb_max.z)]


def hull_footprint(parts, pose_t):
    """Actor-space XY/Z bounds of the static parts (hull, casemate) in their buried pose."""
    lo = [1e9, 1e9, 1e9]
    hi = [-1e9, -1e9, -1e9]
    for p in parts:
        asset = unreal.load_asset(p['mesh'])
        if not isinstance(asset, unreal.StaticMesh):
            continue
        bb = asset.get_bounding_box()
        world = xf(p['xf']) * pose_t
        for c in box_corners(bb.min, bb.max):
            w = unreal.MathLibrary.transform_location(world, c)
            for i, v in enumerate((w.x, w.y, w.z)):
                lo[i] = min(lo[i], v)
                hi[i] = max(hi[i], v)
    return lo, hi


def sand_verts(mesh):
    sec = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, 0)
    return sec[0]


def sand_height(verts, x, y, radius=60.0):
    best = -400.0
    r2 = radius * radius
    for v in verts:
        if (v.x - x) ** 2 + (v.y - y) ** 2 <= r2 and v.z > best:
            best = v.z
    return best


def nav_box(verts, foot_lo, foot_hi):
    lo = [foot_lo[0] - BLOCK_PAD_CM, foot_lo[1] - BLOCK_PAD_CM]
    hi = [foot_hi[0] + BLOCK_PAD_CM, foot_hi[1] + BLOCK_PAD_CM]
    high = [v for v in verts if v.z > HIGH_SAND_CM]
    for v in high:
        lo = [min(lo[0], v.x), min(lo[1], v.y)]
        hi = [max(hi[0], v.x), max(hi[1], v.y)]
    top = max([foot_hi[2]] + [v.z for v in high]) + 50.0
    bottom = -100.0
    centre = unreal.Vector((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (top + bottom) / 2)
    extent = unreal.Vector((hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2, (top - bottom) / 2)
    return centre, extent, len(high)


def socket_ring(verts, centre, extent):
    """8 spots side by side around the shallow front-left corner of the blocker (nose +X, left -Y): squad unit i
    stands at socket i, so they kneel together on the side the dune does not cover."""
    front = centre.x + extent.x + SOCKET_OUT_CM
    left = centre.y - extent.y - SOCKET_OUT_CM
    spots = [(front, left)]
    for k in range(1, 5):  # along the nose, toward the right side
        spots.append((front, left + SOCKET_OUT_CM + k * SOCKET_SPACING_CM))
    for k in range(1, 4):  # along the left flank, toward the rear
        spots.append((front - SOCKET_OUT_CM - k * SOCKET_SPACING_CM, left))
    spots = [(max(sand_height(verts, x, y), 0.0), x, y) for x, y in spots]
    corner = spots[0]
    spots.sort(key=lambda sp: (sp[1] - corner[1]) ** 2 + (sp[2] - corner[2]) ** 2)
    return spots


def prepare_sand(veh, parts, pose_t):
    path = MESHES + '/SM_BuriedSand_' + veh
    mesh = unreal.load_asset(path)
    if not mesh:
        err('missing ' + path)
        return None, None, None, None
    mesh.modify()
    body = mesh.get_editor_property('body_setup')
    # The scavenge mesh must take clicks/shells: query on the render triangles (the skirt stays under ground).
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    verts = sand_verts(mesh)
    foot_lo, foot_hi = hull_footprint(parts, pose_t)
    centre, extent, n_high = nav_box(verts, foot_lo, foot_hi)
    for s in list(mesh.get_sockets_by_tag('')):
        if 'scav' in str(s.socket_name).lower():
            mesh.remove_socket(s)
    spots = socket_ring(verts, centre, extent)
    for i, (h, x, y) in enumerate(spots):
        sock = unreal.StaticMeshSocket(mesh)
        sock.set_editor_property('socket_name', 'Scav_%02d' % (i + 1))
        sock.set_editor_property('relative_location', unreal.Vector(x, y, 0.0))
        mesh.add_socket(sock)
    save(path)
    info = {'foot_lo': foot_lo, 'foot_hi': foot_hi, 'nav_centre': [centre.x, centre.y, centre.z],
            'nav_extent': [extent.x, extent.y, extent.z], 'high_sand_verts': n_high,
            'sockets': [[round(x), round(y), round(h)] for h, x, y in spots]}
    log('%s sand ready %s' % (veh, json.dumps(info)))
    return mesh, centre, extent, info


# ---------------------------------------------------------------- blueprints
def handles(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        d = SDL.get_data(h)
        obj = SDL.get_object_for_blueprint(d, bp)
        if obj is None or isinstance(obj, unreal.Actor):
            out.setdefault('__actor__', (h, obj))
            continue
        out[str(SDL.get_variable_name(d))] = (h, obj)
    return out


def add_comp(bp, parent, cls, name):
    params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp)
    h, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError('%s: adding %s failed: %s' % (bp.get_name(), name, fail))
    SDS.rename_subobject(h, unreal.Text(name))
    return h


def make_bp(name, parent_class):
    path = ROOT + '/' + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)  # not placed anywhere yet: rebuild from scratch
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent_class)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Blueprint, factory)


def copy_progress_widget(dst):
    src_bp = unreal.load_asset(SCAV_TEMPLATE)
    src = handles(src_bp).get('ProgressBarWidget', (None, None))[1]
    if src is None:
        err('no ProgressBarWidget template on ' + SCAV_TEMPLATE)
        return
    for prop in ('widget_class', 'widget_space', 'draw_size', 'pivot', 'draw_at_desired_size', 'manually_redraw',
                 'tick_when_offscreen'):
        try:
            dst.set_editor_property(prop, src.get_editor_property(prop))
        except Exception as e:  # noqa: BLE001
            log('progress widget prop %s skipped: %s' % (prop, e))


def build_base():
    cls = getattr(unreal, 'BuriedVehicleSite', None)
    if cls is None:
        raise RuntimeError('ABuriedVehicleSite missing: run with the DebugGame DLL that has it')
    bp = make_bp(BASE, cls)
    named = handles(bp)
    copy_progress_widget(named['ProgressBarWidget'][1])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('reward_component_widget_class', EAL.load_blueprint_class(REWARD_WIDGET))
    cdo.set_editor_property('consume_squad_when_finished', True)
    cdo.set_editor_property('give_player_resource_rewards', False)
    cdo.set_editor_property('scavenge_resources', {})
    save(ROOT + '/' + BASE)  # no compile after the CDO edits
    log('base built')
    return bp


def set_part(comp, r):
    t = xf(r['xf'])
    comp.set_editor_property('relative_location', t.translation)
    comp.set_editor_property('relative_rotation', t.rotation.rotator())
    comp.set_editor_property('relative_scale3d', t.scale3d)
    asset = unreal.load_asset(r['mesh'])
    mats = [unreal.load_asset(m) if m else None for m in (r.get('mats') or [])]
    if isinstance(comp, unreal.StaticMeshComponent):
        comp.set_editor_property('static_mesh', asset)
    else:
        try:
            comp.set_editor_property('skeletal_mesh_asset', asset)
        except Exception:  # noqa: BLE001
            comp.set_skeletal_mesh_asset(asset)
    comp.set_editor_property('override_materials', mats)
    # Runtime C++ gives the parts query collision (clicks + shells); in the editor they stay out of nav.
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)


def build_child(veh, base_bp, dump, poses):
    bp_name = 'BP_BuriedArmour_' + veh
    data = dump[veh]
    parts = kept_parts(data['components'])
    pose_t = pose_xf(poses[veh]['pose_component'])
    sand, centre, extent, sand_info = prepare_sand(veh, parts, pose_t)
    if sand is None:
        return None
    bp = make_bp(bp_name, base_bp.generated_class())
    named = handles(bp)
    pose_h = named['Pose'][0]
    for p in parts:
        cls = unreal.StaticMeshComponent if p['class'] == 'StaticMeshComponent' else unreal.SkeletalMeshComponent
        add_comp(bp, pose_h, cls, p['comp_name'])
    named = handles(bp)
    named['Sand'][1].set_editor_property('static_mesh', sand)
    named['Pose'][1].set_editor_property('relative_location', pose_t.translation)
    named['Pose'][1].set_editor_property('relative_rotation', pose_t.rotation.rotator())
    nav = named['NavBlocker'][1]
    nav.set_editor_property('relative_location', centre)
    nav.set_editor_property('box_extent', extent)
    for p in parts:
        set_part(named[p['comp_name']][1], p)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    vehicle_cls = EAL.load_blueprint_class(data['bp'])
    if vehicle_cls is None:
        err('%s: vehicle class missing %s' % (veh, data['bp']))
    cdo.set_editor_property('recovered_vehicle_class', vehicle_cls)
    cdo.set_editor_property('scavenge_time', SCAV_TIME[veh])
    save(ROOT + '/' + bp_name)  # no compile after the CDO edits
    REPORT.setdefault('children', {})[veh] = {'bp': ROOT + '/' + bp_name, 'parts': [p['comp_name'] for p in parts],
                                              'sand': sand_info, 'scavenge_time': SCAV_TIME[veh]}
    log('%s built with %d parts' % (bp_name, len(parts)))
    return bp


def run_build():
    dump, poses = load_data()
    for veh in SCAV_TIME:  # children first, so the base is never deleted under them
        if EAL.does_asset_exist(ROOT + '/BP_BuriedArmour_' + veh):
            EAL.delete_asset(ROOT + '/BP_BuriedArmour_' + veh)
    base = build_base()
    for veh in SCAV_TIME:
        try:
            build_child(veh, base, dump, poses)
        except Exception as e:  # noqa: BLE001
            import traceback
            err('%s: %s' % (veh, traceback.format_exc()))


# ---------------------------------------------------------------- fresh-load check
def close(a, b, tol=0.5):
    return abs(a.x - b.x) < tol and abs(a.y - b.y) < tol and abs(a.z - b.z) < tol


def check_child(veh, dump, poses, eas, origin):
    path = ROOT + '/BP_BuriedArmour_' + veh
    cls = EAL.load_blueprint_class(path)
    res = {'loaded': cls is not None}
    if cls is None:
        err(veh + ': class did not load')
        return res
    yaw = 35.0
    actor = eas.spawn_actor_from_class(cls, origin, unreal.Rotator(0, 0, yaw))
    res['spawned'] = actor is not None
    if actor is None:
        err(veh + ': spawn failed')
        return res
    actor_t = actor.get_actor_transform()
    pose_t = pose_xf(poses[veh]['pose_component'])
    comps = {c.get_name(): c for c in actor.get_components_by_class(unreal.SceneComponent)}
    res['components'] = sorted(comps)
    parts = kept_parts(dump[veh]['components'])
    worst = 0.0
    for p in parts:
        c = comps.get(p['comp_name'])
        if c is None:
            err('%s: part %s missing' % (veh, p['comp_name']))
            continue
        want = xf(p['xf']) * pose_t * actor_t
        got = c.get_world_transform()
        d = (want.translation - got.translation).length()
        dr = abs(unreal.MathLibrary.normalize_axis(want.rotation.rotator().yaw - got.rotation.rotator().yaw))
        worst = max(worst, d)
        mesh = c.static_mesh if isinstance(c, unreal.StaticMeshComponent) else c.get_skeletal_mesh_asset()
        if mesh is None or mesh.get_path_name().split('.')[0] != p['mesh']:
            err('%s: part %s wrong mesh %s' % (veh, p['comp_name'], mesh))
        if d > 0.5 or dr > 0.1:
            err('%s: part %s off by %.2f cm / %.2f deg' % (veh, p['comp_name'], d, dr))
        if c.get_attach_parent() is None or c.get_attach_parent().get_name() != 'Pose':
            err('%s: part %s not under Pose' % (veh, p['comp_name']))
        want_mats = [m for m in (p.get('mats') or [])]
        got_mats = [m.get_path_name().split('.')[0] if m else None for m in c.get_materials()]
        if want_mats and got_mats[:len(want_mats)] != want_mats:
            err('%s: part %s materials %s != %s' % (veh, p['comp_name'], got_mats, want_mats))
    res['worst_part_offset_cm'] = worst
    sand_c = comps.get('Sand')
    if sand_c:
        res['sand_collision'] = [str(sand_c.get_collision_enabled()), str(sand_c.get_collision_object_type())]
    # Editor worlds never run PostInitializeComponents (no actor init for play) and do not refresh the physics query
    # structure without a tick, so runtime collision and these traces are info only (2026-11-23: all None).
    world = actor.get_world()
    res['traces'] = {}
    loc = actor.get_actor_location()
    for label, dx, dy in (('hull_centre', 0.0, 0.0), ('sand_rear_right', -330.0, 230.0)):
        start = loc + unreal.MathLibrary.greater_greater_vector_rotator(unreal.Vector(dx, dy, 0.0),
                                                                        actor.get_actor_rotation())
        hit = unreal.SystemLibrary.line_trace_single(world, start + unreal.Vector(0, 0, 1500),
                                                     start - unreal.Vector(0, 0, 500),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        info = hit.to_tuple() if hit else None
        hit_actor = info[9] if info else None
        hit_comp = info[10] if info else None
        res['traces'][label] = [hit_actor.get_name() if hit_actor else None, hit_comp.get_name() if hit_comp else None]

    pose = comps.get('Pose')
    rel = pose.get_editor_property('relative_location') if pose else None
    res['pose_rel'] = [rel.x, rel.y, rel.z] if rel else None
    if rel is None or not close(rel, pose_t.translation, 0.01):
        err('%s: Pose relative location wrong %s' % (veh, rel))
    sand = comps.get('Sand')
    sm = sand.static_mesh if sand else None
    res['sand'] = sm.get_name() if sm else None
    res['sand_is_root'] = actor.get_editor_property('root_component') == sand
    socks = [str(s.socket_name) for s in sm.get_sockets_by_tag('')] if sm else []
    res['scav_sockets'] = [s for s in socks if 'scav' in s.lower()]
    if len(res['scav_sockets']) != 8:
        err('%s: %d scav sockets' % (veh, len(res['scav_sockets'])))
    trace = sm.get_editor_property('body_setup').get_editor_property('collision_trace_flag') if sm else None
    res['sand_trace_flag'] = str(trace)
    nav = comps.get('NavBlocker')
    res['nav_extent'] = str(nav.get_editor_property('box_extent')) if nav else None
    res['nav_dynamic_obstacle'] = bool(nav.get_editor_property('dynamic_obstacle')) if nav else None
    res['nav_area'] = str(nav.get_editor_property('area_class_override')) if nav else None
    pw = comps.get('ProgressBarWidget')
    res['progress_widget_class'] = str(pw.get_editor_property('widget_class')) if pw else None
    res['vehicle_class'] = str(actor.get_editor_property('recovered_vehicle_class'))
    res['scavenge_time'] = actor.get_editor_property('scavenge_time')
    res['consume_squad'] = actor.get_editor_property('consume_squad_when_finished')
    res['give_resources'] = actor.get_editor_property('give_player_resource_rewards')
    res['reward_widget'] = str(actor.get_editor_property('reward_component_widget_class'))
    if 'None' in res['vehicle_class'] or 'None' in str(res['progress_widget_class']):
        err('%s: vehicle class or progress widget class unset' % veh)
    return res


def run_check():
    dump, poses = load_data()
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level('/Temp/BuriedArmourCheck')  # unsaved temp level, never saved
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    base = EAL.load_blueprint_class(ROOT + '/' + BASE)
    REPORT['base_loaded'] = base is not None
    for i, veh in enumerate(SCAV_TIME):
        try:
            REPORT.setdefault('children', {})[veh] = check_child(veh, dump, poses, eas,
                                                                 unreal.Vector(3000.0 * i, 0.0, 0.0))
        except Exception:  # noqa: BLE001
            import traceback
            err('%s: %s' % (veh, traceback.format_exc()))


try:
    if STEP == 'check':
        run_check()
    else:
        run_build()
except Exception:  # noqa: BLE001
    import traceback
    err(traceback.format_exc())
save_report()
log('DONE errors=%d' % len(REPORT['errors']))
