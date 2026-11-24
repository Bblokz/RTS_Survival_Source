"""Spawn the POI plan (poi_plan.py -> Saved/IntroFoliage/poi/poi_r<N>.json) in the throwaway editor.

    exec(open(FOL + 'poi_apply.py').read()); apply_pois(1)     # clears every POIW_/POIE_ actor first

Seating follows the project rule (actor-bounds trap): never by actor bounds. Props sit at the LOWEST landscape point
under their footprint minus 2 cm (traces that only accept the landscape); trees by pivot; pieces on the pit slope are
pitched/rolled to the ground normal. Folder IntroPOI/West or IntroPOI/East.
"""
import json
import math

import unreal

POI_ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroFoliage/poi/'


def _eas():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


_IGNORE = []


def _land_z(x, y):
    """Landscape height only (other actors never lift a prop). Multi traces stop at the first blocking hit, so every
    actor this pass spawned is ignored (a table placed first used to hide the ground from the life ring)."""
    hits = unreal.SystemLibrary.line_trace_multi(_world(), unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, _IGNORE,
                                                 unreal.DrawDebugTrace.NONE, True)
    for h in hits or []:
        t = h.to_tuple()
        actor = t[9]
        if actor and actor.get_class().get_name() in ('Landscape', 'LandscapeStreamingProxy'):
            return t[4].z
    return None


def _footprint_min_z(it, scale=1.0):
    hx, hy = it['half'][0] * scale, it['half'][1] * scale
    a = math.radians(it['yaw'])
    zs = []
    for du in (-hx, 0.0, hx):
        for dv in (-hy, 0.0, hy):
            z = _land_z(it['x'] + du * math.cos(a) - dv * math.sin(a), it['y'] + du * math.sin(a) + dv * math.cos(a))
            if z is not None:
                zs.append(z)
    return min(zs) if zs else 150.0


def _remember(a):
    if a is not None:
        _IGNORE.append(a)
    return a


def _folder(label):
    return 'IntroPOI/West' if label.startswith('POIW_') else 'IntroPOI/East'


def _label(a, label):
    _remember(a)
    a.set_actor_label(label)
    a.set_folder_path(_folder(label))
    return a


def _bp_class(path):
    return unreal.load_class(None, path + '.' + path.split('/')[-1] + '_C')


def _mesh_actor(path, label, loc, rot, scale3=(1.0, 1.0, 1.0)):
    a = _eas().spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
    a.static_mesh_component.set_static_mesh(unreal.load_asset(path))
    a.set_actor_scale3d(unreal.Vector(*scale3))
    return _label(a, label)


def _slope_rot(x, y, yaw, along=300.0, across=120.0):
    """Pitch/roll so the item's local X/Y follow the ground."""
    a = math.radians(yaw)
    ux, uy = math.cos(a), math.sin(a)
    vx, vy = -uy, ux
    zf = _land_z(x + ux * along, y + uy * along)
    zb = _land_z(x - ux * along, y - uy * along)
    zl = _land_z(x + vx * across, y + vy * across)
    zr = _land_z(x - vx * across, y - vy * across)
    if None in (zf, zb, zl, zr):
        return 0.0, 0.0
    pitch = math.degrees(math.atan2(zf - zb, 2 * along))
    roll = -math.degrees(math.atan2(zl - zr, 2 * across))
    return pitch, roll


def _ground_plane(x, y, yaw, hx, hy):
    """Least-squares plane over a 5x5 grid of the footprint: (z at centre, dz/du, dz/dv, mean |residual|)."""
    a = math.radians(yaw)
    rows = []
    for du in (-hx, -hx / 2, 0.0, hx / 2, hx):
        for dv in (-hy, -hy / 2, 0.0, hy / 2, hy):
            z = _land_z(x + du * math.cos(a) - dv * math.sin(a), y + du * math.sin(a) + dv * math.cos(a))
            if z is not None:
                rows.append((du, dv, z))
    n = len(rows)
    su = sum(r[0] for r in rows); sv = sum(r[1] for r in rows); sz = sum(r[2] for r in rows)
    suu = sum(r[0] ** 2 for r in rows); svv = sum(r[1] ** 2 for r in rows)
    suz = sum(r[0] * r[2] for r in rows); svz = sum(r[1] * r[2] for r in rows)
    # grid is symmetric: su = sv = 0 and suv = 0, so the normal equations decouple
    c = sz / n
    bu = (suz - su * c) / suu if suu else 0.0
    bv = (svz - sv * c) / svv if svv else 0.0
    res = sum(abs(r[2] - (c + bu * r[0] + bv * r[1])) for r in rows) / n
    return c, bu, bv, res


def _fit_to_ground(actor, x, y, yaw, hx, hy, z_offset, extra_roll=0.0):
    """Tilt + seat an actor on the ground plane under its footprint. The roll sign is verified on the placed actor
    (local +Y corner must land on the plane), so no rotator convention is assumed."""
    c, bu, bv, res = _ground_plane(x, y, yaw, hx, hy)
    pitch = math.degrees(math.atan(bu))
    best = None
    for sign in (1.0, -1.0):
        roll = sign * math.degrees(math.atan(bv))
        actor.set_actor_location_and_rotation(unreal.Vector(x, y, c + z_offset),
                                              unreal.Rotator(roll + extra_roll, pitch, yaw), False, False)
        t = actor.get_actor_transform()
        err = 0.0
        for du, dv in ((hx, hy), (-hx, hy), (hx, -hy), (-hx, -hy)):
            p = t.transform_location(unreal.Vector(du / max(t.scale3d.x, 1e-3), dv / max(t.scale3d.y, 1e-3), 0.0))
            err += abs((p.z - z_offset) - (c + bu * du + bv * dv))
        if best is None or err < best[0]:
            best = (err, roll)
    actor.set_actor_location_and_rotation(unreal.Vector(x, y, c + z_offset),
                                          unreal.Rotator(best[1] + extra_roll, pitch, yaw), False, False)
    return res


def _local(points, origin, yaw):
    c, s = math.cos(math.radians(-yaw)), math.sin(math.radians(-yaw))
    return [unreal.Vector((x - origin[0]) * c - (y - origin[1]) * s, (x - origin[0]) * s + (y - origin[1]) * c, 0.0)
            for x, y in points]


def _set_spline(spline, pts, closed):
    spline.clear_spline_points(False)
    for p in pts:
        spline.add_spline_point(p, unreal.SplineCoordinateSpace.LOCAL, False)
    for i in range(len(pts)):
        spline.set_spline_point_type(i, unreal.SplinePointType.LINEAR, False)
    spline.set_closed_loop(closed, False)
    spline.update_spline()
    spline.set_editor_property('spline_has_been_edited', True)


def _spawn(it):
    kind, label, x, y, yaw = it['kind'], it['label'], it['x'], it['y'], it['yaw']
    if kind == 'bp':
        tree = 'DeadTree' in it['path'] or 'OrchardTree' in it['path']
        z = _land_z(x, y) if tree else _footprint_min_z(it) - 2.0
        a = _eas().spawn_actor_from_class(_bp_class(it['path']), unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        if it.get('scale'):
            sc = it['scale']
            a.set_actor_scale3d(unreal.Vector(sc, sc, sc))
        if it.get('material'):
            mat = unreal.load_asset(it['material'])
            for c in a.get_components_by_class(unreal.StaticMeshComponent):
                for k in range(c.get_num_materials()):
                    c.set_material(k, mat)
        return _label(a, label)
    if kind == 'mesh':
        s = it.get('scale', 1.0)
        a = _mesh_actor(it['path'], label, unreal.Vector(x, y, 0.0), unreal.Rotator(0.0, 0.0, yaw), (s, s, s))
        _fit_to_ground(a, x, y, yaw, it['half'][0] * s, it['half'][1] * s, -3.0)
        return a
    if kind == 'oldcar':
        a = _eas().spawn_actor_from_class(_bp_class(it['path']), unreal.Vector(x, y, 0.0), unreal.Rotator(0.0, 0.0, yaw))
        _fit_to_ground(a, x, y, yaw, it['half'][0], it['half'][1], -3.0)
        for prop, val in (('CarMesh', unreal.load_asset(it['mesh'])), ('Wear', it['wear'])):
            try:
                a.set_editor_property(prop, val)
            except Exception:
                a.set_editor_property(prop[0].lower() + prop[1:], val)
        return _label(a, label)
    if kind == 'field':
        pts = it['outline']
        cx, cy = sum(p[0] for p in pts) / len(pts), sum(p[1] for p in pts) / len(pts)
        a = _eas().spawn_actor_from_class(_bp_class(it['path']), unreal.Vector(cx, cy, _land_z(cx, cy)),
                                          unreal.Rotator(0.0, 0.0, yaw))
        _set_spline(a.get_editor_property('field_outline'), _local(pts, (cx, cy), yaw), True)
        if it.get('material'):
            a.set_editor_property('material_override', unreal.load_asset(it['material']))
        lay = a.get_editor_property('layout')
        lay.set_editor_property('random_seed', it['seed'])
        if it.get('layout'):
            L = it['layout']
            for k_, v_ in (('row_spacing', L['spacing']), ('plant_spacing', L['spacing']), ('min_scale', L['smin']),
                           ('max_scale', L['smax']), ('position_jitter', L.get('jitter')),
                           ('missing_plant_chance', L.get('missing')), ('edge_inset', L.get('inset'))):
                if v_ is not None:
                    lay.set_editor_property(k_, float(v_))
        a.set_editor_property('layout', lay)
        return _label(a, label)
    if kind == 'fence':
        pts = it['pts']
        a = _eas().spawn_actor_from_class(_bp_class(it['path']), unreal.Vector(pts[0][0], pts[0][1], _land_z(*pts[0])),
                                          unreal.Rotator(0.0, 0.0, 0.0))
        _set_spline(a.get_editor_property('spline'), _local(pts, pts[0], 0.0), False)
        return _label(a, label)
    if kind == 'sign':
        z = _land_z(x, y) - 10.0
        pole = _mesh_actor('/Game/NordicHarbour/Props/TrafficSigns/SM_TrafficSign_Pole', label,
                           unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        _mesh_actor(it['path'], label + '_Plate1', unreal.Vector(x, y, z + 185.0), unreal.Rotator(0.0, 0.0, yaw))
        _mesh_actor(it['second'], label + '_Plate2', unreal.Vector(x, y, z + 130.0), unreal.Rotator(0.0, 0.0, yaw))
        return pole
    if kind == 'jetty':
        s3 = it['scale3']
        if 'deck_z' in it:      # review r2: explicit shared joints, validated pitch/roll; pivot 15 cm below the deck top
            return _mesh_actor(it['path'], label, unreal.Vector(x, y, it['deck_z'] - 19.0 * s3[2]),
                               unreal.Rotator(it['roll'], it['pitch'], yaw), s3)
        a = _mesh_actor(it['path'], label, unreal.Vector(x, y, 0.0), unreal.Rotator(0.0, 0.0, yaw), s3)
        _fit_to_ground(a, x, y, yaw, 300.0 * s3[0], 225.0 * s3[1], 15.0 - 19.0 * s3[2])
        return a
    if kind == 'pitmesh':
        s = it.get('scale', 1.0)
        upside_down = it.get('roll', 0) == 180
        mesh = unreal.load_asset(it['path'])
        b = mesh.get_bounding_box()
        # lie ACROSS the slope (review r1: Rowboat1 pointed straight down a 31 deg bank): long axis on the contour.
        # The boat mesh is long along local Y, so yaw puts local Y on the contour = local X downhill.
        zx1, zx0 = _land_z(x + 200.0, y), _land_z(x - 200.0, y)
        zy1, zy0 = _land_z(x, y + 200.0), _land_z(x, y - 200.0)
        if None not in (zx1, zx0, zy1, zy0) and it.get('across', True):
            yaw = math.degrees(math.atan2(-(zy1 - zy0), -(zx1 - zx0)))
        a = _mesh_actor(it['path'], label, unreal.Vector(x, y, 0.0), unreal.Rotator(0.0, 0.0, yaw), (s, s, s))
        lift = (b.max.z if upside_down else -b.min.z) * s - 8.0 + it.get('lift', 0.0)
        _fit_to_ground(a, x, y, yaw, (b.max.x - b.min.x) * s / 2, (b.max.y - b.min.y) * s / 2, lift,
                       180.0 if upside_down else 0.0)
        return a
    if kind == 'pitbp':
        # review r1: willows stood on 36-49 deg bank faces; take the gentlest spot within 8 m (or the fixed spot)
        best = None
        if it.get('fixed'):
            a = _eas().spawn_actor_from_class(_bp_class(it['path']), unreal.Vector(x, y, _land_z(x, y)),
                                              unreal.Rotator(0.0, 0.0, yaw))
            return _label(a, label)
        for r in (0.0, 200.0, 400.0, 600.0, 800.0):
            for k in range(1 if r == 0 else 12):
                t = 2 * math.pi * k / 12
                px, py = x + r * math.cos(t), y + r * math.sin(t)
                _, bu, bv, _ = _ground_plane(px, py, 0.0, 150.0, 150.0)
                slope = math.degrees(math.atan(math.hypot(bu, bv)))
                if best is None or slope < best[0] - 2.0:
                    best = (slope, px, py)
        _, px, py = best
        a = _eas().spawn_actor_from_class(_bp_class(it['path']), unreal.Vector(px, py, _land_z(px, py)),
                                          unreal.Rotator(0.0, 0.0, yaw))
        print(label, 'slope %.0f deg at (%d, %d)' % best)
        return _label(a, label)
    if kind == 'cards':         # flat no-collision card scatters (yard litter, scorch fringe)
        made = None
        for mesh_path, pts in it['cards'].items():
            if not pts:
                continue
            transforms = []
            for cx_, cy_, cyaw, sc in pts:
                z = _land_z(cx_, cy_)
                if z is None:
                    continue
                transforms.append(unreal.Transform(unreal.Vector(cx_, cy_, z - 1.0), unreal.Rotator(0.0, 0.0, cyaw),
                                                   unreal.Vector(sc, sc, sc)))
            mat = unreal.load_asset(it['materials'][mesh_path]) if it.get('materials', {}).get(mesh_path) else None
            made = _hism_cards('%s_%s' % (label, mesh_path.split('/')[-1].replace('SM_', '')), mesh_path, transforms, mat)
        return made
    raise RuntimeError('unknown kind ' + kind)


def _hism_cards(label, mesh_path, transforms, mat):
    actor = _eas().spawn_actor_from_class(unreal.Actor, unreal.Vector(0.0, 0.0, 0.0))
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(actor)[0]
    handle, _ = sds.add_new_subobject(unreal.AddNewSubobjectParams(
        parent_handle=root, new_class=unreal.HierarchicalInstancedStaticMeshComponent))
    comp = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
    comp.set_static_mesh(unreal.load_asset(mesh_path))
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('receives_decals', False)      # cliff stone decals must not paint the grass cards
    comp.set_editor_property('cast_shadow', False)
    comp.set_editor_property('instance_start_cull_distance', 11000)
    comp.set_editor_property('instance_end_cull_distance', 16000)
    if mat is not None:
        for k in range(comp.get_num_materials()):
            comp.set_material(k, mat)
    comp.add_instances(transforms, False, True)
    return _label(actor, label)


def clear_pois():
    n = 0
    for a in _eas().get_all_level_actors():
        if a.get_actor_label().startswith(('POIW_', 'POIE_')):
            _eas().destroy_actor(a)
            n += 1
    print('cleared POI actors', n)


def apply_pois(round_no):
    with open(POI_ROOT + 'poi_r%d.json' % round_no) as h:
        plan = json.load(h)
    if plan['violations']:
        raise RuntimeError('plan has keep-out violations: %s' % plan['violations'])
    clear_pois()
    done = 0
    for it in plan['items']:
        try:
            _spawn(it)
            done += 1
        except Exception as exc:
            print('FAILED', it['label'], exc)
    print('POI items spawned', done, 'of', len(plan['items']))
