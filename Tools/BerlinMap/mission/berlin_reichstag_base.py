"""Places the B2 "Kuppelwerk" Director of Shield base in the Reichstag ruin on Ger_Berlin (editor Python, my own editor).

Layout: Source/Tools/ReichstagRuin/RT_Layout.json ("blueprints": the static mesh bounds centre goes on bounds_centre_cm, in
the ruin's local frame: x south, y west, cm) plus the propaganda megaphone from the B2a sheet. Positions are mapped through
BP_ReichstagRuin_B2's actor transform on the map, so the base follows the ruin.

Steps (each re-runnable; nothing is saved here):
  place()        spawn or update RT_Base_* actors (folder Mission/ReichstagBase); the factory gets the tag
                 BerlinReichstagFactory (Mission_Berlin_RaidReichstag tracks it)
  clip_report()  every LOD0 vertex of every ruin piece instance tested against each base building's oriented box (and
                 the building's vertices against the ruin pieces' boxes), with a margin
  ground_report() landscape height under each building's footprint vs the mesh bottom
  flatten(names) levels the landscape under those footprints (+ a blended skirt) via RTSShortsLandscapeLibrary
  seat()         sets each actor's Z so the mesh bottom sits on the lowest landscape point under its footprint, minus 2 cm
  set_factory_exit() moves the factory instance's AITrainingExit into the hall's open west end (owner pick A)

Run 2026-11-24: place() + set_factory_exit(). The landscape under the base is a flat pad at z 0, so the actors stay at
their pivots (z 0; mesh bottoms 0 to -32 cm) and neither seat() nor flatten() was needed. clip_report(): only box hits,
and the surface checks were clean (no dome vertex inside the factory, 5 m clearance under the dome; the DShK pit is
outside the rubble heap's surface).
"""
import json
import math
import os
import struct

import unreal

LAYOUT = 'D:/UE5Projects/RTS_Survival/Source/Tools/ReichstagRuin/RT_Layout.json'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/BerlinReichstagBase/'
B = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
BLUEPRINTS = {
    'Rus_Building_Factory': B + 'RusBuildings/Rus_Building_Factory',
    'Rus_Building_Barracks': B + 'RusBuildings/Rus_Building_Barracks',
    'Rus_BXP_120MortarPit': B + 'RusMortarPit/Rus_BXP_120MortarPit',
    'Rus_BXP_DShKPit': B + 'RusDShKPit/Rus_BXP_DShKPit',
    'Rus_BXP_BS3Pit': B + 'RusBS3Pit/Rus_BXP_BS3Pit',
    'Rus_BXP_53KPit': B + 'Rus53KPit/Rus_BXP_53KPit',
    'Rus_Bxp_Megaphone_Propaganda1': B + 'RusBuildings/Rus_Bxp_Megaphone_Propaganda1',
}
# B2a sheet: sk.megaphone(k, (-12, -50)) in the concept frame (x south, y EAST in metres); the layout frame has y west.
EXTRA = [{'blueprint': 'Rus_Bxp_Megaphone_Propaganda1', 'bounds_centre_cm': [-1200, 5000], 'yaw': 0}]
FOLDER = 'Mission/ReichstagBase'
FACTORY_TAG = 'BerlinReichstagFactory'
BASE_TAG = 'BerlinReichstagBase'
CLIP_MARGIN = 30.0        # cm: a ruin vertex this close inside a building box counts as clipping
SEAT_BELOW = 2.0          # cm under the lowest ground point
SEARCH_RADIUS = 9000.0    # cm round the ruin: actors in here (except the landscape) are ignored by ground traces


def log(text):
    unreal.log_warning('RT_BASE ' + str(text))


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def ruin_actor(w):
    found = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
             if a.get_class().get_name() == 'BP_ReichstagRuin_B2_C']
    if len(found) != 1:
        raise RuntimeError(f'expected one BP_ReichstagRuin_B2, found {len(found)}')
    return found[0]


def layout_entries():
    rows = json.load(open(LAYOUT))['blueprints'] + EXTRA
    counts, entries = {}, []
    for row in rows:
        n = counts.get(row['blueprint'], 0) + 1
        counts[row['blueprint']] = n
        entries.append(dict(row, label=f"RT_Base_{row['blueprint']}_{n}"))
    return entries


def base_actors(w):
    return {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
            if a.get_actor_label().startswith('RT_Base_')}


# ----- geometry helpers -----

def visible_mesh_components(actor):
    comps = []
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if isinstance(c, unreal.InstancedStaticMeshComponent) or c.static_mesh is None:
            continue
        if not c.is_visible() or c.get_editor_property('hidden_in_game'):
            continue
        comps.append(c)
    return comps


def main_mesh(actor):
    best, size = None, -1
    for c in visible_mesh_components(actor):
        ext = c.static_mesh.get_bounds().box_extent
        if ext.x * ext.y > size:
            best, size = c, ext.x * ext.y
    return best


_VERT_CACHE = {}


def mesh_vertices(mesh):
    """LOD0 vertices in mesh space (cached)."""
    key = mesh.get_path_name()
    if key in _VERT_CACHE:
        return _VERT_CACHE[key]
    verts = []
    for section in range(mesh.get_num_sections(0)):
        data = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
        verts.extend(data[0])
    # thin out huge meshes a little; keep every vertex of small ones
    step = max(1, len(verts) // 6000)
    _VERT_CACHE[key] = [(v.x, v.y, v.z) for v in verts[::step]]
    return _VERT_CACHE[key]


class Box:
    """Oriented box: world transform of a mesh's local bounds."""

    def __init__(self, transform, mesh, name):
        b = mesh.get_bounds()
        self.name = name
        self.tf = transform
        self.inv = transform.inverse()
        self.lo = (b.origin.x - b.box_extent.x, b.origin.y - b.box_extent.y, b.origin.z - b.box_extent.z)
        self.hi = (b.origin.x + b.box_extent.x, b.origin.y + b.box_extent.y, b.origin.z + b.box_extent.z)
        s = transform.scale3d
        self.scale = (abs(s.x), abs(s.y), abs(s.z))

    def depth_inside(self, p, margin):
        """How deep (cm, in local units scaled) world point p is inside the box shrunk by margin; <= 0 = outside."""
        lp = self.inv.transform_location(unreal.Vector(*p))
        depth = 1e9
        for axis, value in enumerate((lp.x, lp.y, lp.z)):
            d = min(value - self.lo[axis], self.hi[axis] - value) * self.scale[axis] - margin
            depth = min(depth, d)
        return depth

    def world_points(self, verts):
        return [self.tf.transform_location(unreal.Vector(*v)) for v in verts]


def ruin_boxes(ruin):
    boxes = []
    for ism in ruin.get_components_by_class(unreal.InstancedStaticMeshComponent):
        mesh = ism.static_mesh
        if mesh is None:
            continue
        for i in range(ism.get_instance_count()):
            tf = ism.get_instance_transform(i, True)
            boxes.append(Box(tf, mesh, f'{mesh.get_name()}#{i}'))
    return boxes


def building_boxes(actor):
    return [Box(c.get_world_transform(), c.static_mesh, f'{actor.get_actor_label()}:{c.get_name()}')
            for c in visible_mesh_components(actor)]


# ----- placement -----

def place():
    w = world()
    ruin = ruin_actor(w)
    rtf = ruin.get_actor_transform()
    ryaw = ruin.get_actor_rotation().yaw
    log(f'ruin at {ruin.get_actor_location()} yaw {ryaw:.1f}')
    existing = base_actors(w)
    placed = []
    for e in layout_entries():
        target = rtf.transform_location(unreal.Vector(e['bounds_centre_cm'][0], e['bounds_centre_cm'][1], 0))
        actor = existing.get(e['label'])
        rot = unreal.Rotator(roll=0, pitch=0, yaw=ryaw + e['yaw'])
        if actor is None:
            cls = unreal.EditorAssetLibrary.load_blueprint_class(BLUEPRINTS[e['blueprint']])
            actor = actors_sub().spawn_actor_from_class(cls, unreal.Vector(target.x, target.y, target.z), rot)
            actor.set_actor_label(e['label'])
        actor.set_actor_rotation(rot, False)
        actor.set_folder_path(FOLDER)
        tags = [unreal.Name(BASE_TAG)]
        if e['blueprint'] == 'Rus_Building_Factory':
            tags.append(unreal.Name(FACTORY_TAG))
        actor.tags = tags
        comp = main_mesh(actor)
        centre = comp.get_world_transform().transform_location(comp.static_mesh.get_bounds().origin)
        loc = actor.get_actor_location()
        actor.set_actor_location(unreal.Vector(loc.x + target.x - centre.x, loc.y + target.y - centre.y, loc.z),
                                 False, False)
        placed.append(actor)
        log(f"{e['label']}: target ({target.x:.0f}, {target.y:.0f}) yaw {rot.yaw:.0f} mesh {comp.static_mesh.get_name()}")
    return placed


# ----- clipping -----

def clip_report(margin=CLIP_MARGIN):
    w = world()
    ruin = ruin_actor(w)
    rboxes = ruin_boxes(ruin)
    results = {}
    actors = base_actors(w)
    for label, actor in sorted(actors.items()):
        worst = []
        bboxes = building_boxes(actor)
        # ruin vertices inside the building's boxes
        for rb in rboxes:
            pts = rb.world_points(mesh_vertices_by_name(rb))
            for bb in bboxes:
                depth = max((bb.depth_inside((p.x, p.y, p.z), margin) for p in pts), default=-1)
                if depth > 0:
                    worst.append((round(depth), 'ruin ' + rb.name + ' inside ' + bb.name))
        # building vertices inside ruin boxes
        for bb in bboxes:
            comp_mesh = [c for c in visible_mesh_components(actor)
                         if f'{actor.get_actor_label()}:{c.get_name()}' == bb.name][0].static_mesh
            pts = bb.world_points(mesh_vertices(comp_mesh))
            for rb in rboxes:
                depth = max((rb.depth_inside((p.x, p.y, p.z), margin) for p in pts), default=-1)
                if depth > 0:
                    worst.append((round(depth), bb.name + ' inside ruin ' + rb.name))
        # other base buildings
        for other_label, other in actors.items():
            if other_label <= label:
                continue
            for ob in building_boxes(other):
                for bb in bboxes:
                    comp_mesh = [c for c in visible_mesh_components(actor)
                                 if f'{actor.get_actor_label()}:{c.get_name()}' == bb.name][0].static_mesh
                    pts = bb.world_points(mesh_vertices(comp_mesh))
                    depth = max((ob.depth_inside((p.x, p.y, p.z), margin) for p in pts), default=-1)
                    if depth > 0:
                        worst.append((round(depth), bb.name + ' inside ' + ob.name))
        worst.sort(reverse=True)
        results[label] = worst[:6]
        log(f'clip {label}: ' + ('CLEAR' if not worst else f'{len(worst)} hits, worst ' + '; '.join(
            f'{d} cm {t}' for d, t in worst[:4])))
    return results


_RUIN_MESH_BY_BOX = {}


def mesh_vertices_by_name(box):
    mesh_name = box.name.split('#')[0]
    if mesh_name not in _RUIN_MESH_BY_BOX:
        ruin = ruin_actor(world())
        for ism in ruin.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if ism.static_mesh is not None:
                _RUIN_MESH_BY_BOX[ism.static_mesh.get_name()] = ism.static_mesh
    return mesh_vertices(_RUIN_MESH_BY_BOX[mesh_name])


# ----- ground -----

def ground_ignore(w, centre):
    ignore = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        name = a.get_class().get_name()
        if 'Landscape' in name:
            continue
        loc = a.get_actor_location()
        if abs(loc.x - centre.x) < SEARCH_RADIUS and abs(loc.y - centre.y) < SEARCH_RADIUS:
            ignore.append(a)
    return ignore


def landscape_z(w, x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    if not t[0]:
        return None
    actor = t[9]
    if actor is None or 'Landscape' not in actor.get_class().get_name():
        return None
    return t[4].z


def footprint_points(actor, step=150.0):
    """World XY points over the main mesh's footprint (its bottom 60 cm of vertices), on a grid plus the hull."""
    comp = main_mesh(actor)
    tf = comp.get_world_transform()
    verts = mesh_vertices(comp.static_mesh)
    zmin = min(v[2] for v in verts)
    low = [v for v in verts if v[2] <= zmin + 60]
    xs = [v[0] for v in low]
    ys = [v[1] for v in low]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    nx = max(2, int((x1 - x0) / step) + 1)
    ny = max(2, int((y1 - y0) / step) + 1)
    pts = []
    for i in range(nx):
        for j in range(ny):
            lx = x0 + (x1 - x0) * i / (nx - 1)
            ly = y0 + (y1 - y0) * j / (ny - 1)
            pts.append(tf.transform_location(unreal.Vector(lx, ly, zmin)))
    bottom = tf.transform_location(unreal.Vector(0, 0, zmin)).z
    return pts, bottom, (x0, x1, y0, y1, zmin)


def ground_report():
    w = world()
    ruin = ruin_actor(w)
    ignore = ground_ignore(w, ruin.get_actor_location())
    report = {}
    for label, actor in sorted(base_actors(w).items()):
        pts, bottom, _ = footprint_points(actor)
        zs = [landscape_z(w, p.x, p.y, ignore + [actor]) for p in pts]
        missing = sum(1 for z in zs if z is None)
        zs = [z for z in zs if z is not None]
        if not zs:
            log(f'ground {label}: no landscape under it')
            continue
        report[label] = {'min': min(zs), 'max': max(zs), 'bottom': bottom, 'missing': missing, 'samples': len(pts)}
        log(f'ground {label}: landscape {min(zs):.0f}..{max(zs):.0f} (spread {max(zs) - min(zs):.0f} cm), '
            f'mesh bottom {bottom:.0f}, float {bottom - min(zs):.0f} cm, {missing}/{len(pts)} samples not landscape')
    return report


def seat(labels=None):
    w = world()
    ruin = ruin_actor(w)
    ignore = ground_ignore(w, ruin.get_actor_location())
    for label, actor in sorted(base_actors(w).items()):
        if labels and label not in labels:
            continue
        pts, bottom, _ = footprint_points(actor)
        zs = [z for z in (landscape_z(w, p.x, p.y, ignore + [actor]) for p in pts) if z is not None]
        if not zs:
            continue
        loc = actor.get_actor_location()
        dz = (min(zs) - SEAT_BELOW) - bottom
        actor.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + dz), False, False)
        log(f'seat {label}: moved {dz:+.0f} cm (ground {min(zs):.0f}..{max(zs):.0f})')


# ----- landscape flattening -----

def landscape_actor(w):
    lands = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
             if a.get_class().get_name() in ('Landscape', 'LandscapeProxy')]
    if not lands:
        raise RuntimeError('no landscape')
    return lands[0]


def landscape_grid(land):
    info = unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land).split()
    tf = land.get_actor_transform()
    return [int(v) for v in info], tf


def flatten(labels, pad=200.0, blend=400.0, level_to='median'):
    """Levels the landscape under each listed actor's footprint (+pad cm) to one height, blending over blend cm."""
    w = world()
    land = landscape_actor(w)
    (gx0, gy0, gx1, gy1, _), ltf = landscape_grid(land)
    inv = ltf.inverse()
    scale = ltf.scale3d
    actors = base_actors(w)
    ruin = ruin_actor(w)
    ignore = ground_ignore(w, ruin.get_actor_location())
    os.makedirs(OUT, exist_ok=True)
    for label in labels:
        actor = actors[label]
        comp = main_mesh(actor)
        ctf = comp.get_world_transform()
        _, _, (x0, x1, y0, y1, zmin) = footprint_points(actor)
        corners = [ctf.transform_location(unreal.Vector(x, y, zmin)) for x in (x0 - pad, x1 + pad)
                   for y in (y0 - pad, y1 + pad)]
        zs = [z for z in (landscape_z(w, p.x, p.y, ignore + [actor]) for p in footprint_points(actor)[0])
              if z is not None]
        zs.sort()
        target_z = zs[len(zs) // 2] if level_to == 'median' else min(zs)
        # landscape vertex rectangle covering footprint + pad + blend
        lcs = [inv.transform_location(c) for c in corners]
        margin_q = blend / abs(scale.x) + 2
        i0 = max(gx0, int(math.floor(min(c.x for c in lcs) - margin_q)))
        i1 = min(gx1, int(math.ceil(max(c.x for c in lcs) + margin_q)))
        j0 = max(gy0, int(math.floor(min(c.y for c in lcs) - margin_q)))
        j1 = min(gy1, int(math.ceil(max(c.y for c in lcs) + margin_q)))
        hfile = OUT + f'{label}_height.r16'
        ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(land, i0, j0, i1, j1, hfile, [], [])
        if not ok:
            log(f'flatten {label}: export failed')
            continue
        nx, ny = i1 - i0 + 1, j1 - j0 + 1
        raw = open(hfile, 'rb').read()
        heights = list(struct.unpack('<%dH' % (nx * ny), raw))
        # landscape uint16 -> world z: z = ltf.z + (h - 32768) * scale.z / 128
        target_h = int(round((target_z - ltf.translation.z) * 128.0 / scale.z + 32768))
        changed = 0
        inner_lo_x, inner_hi_x = x0 - pad, x1 + pad
        inner_lo_y, inner_hi_y = y0 - pad, y1 + pad
        cinv = ctf.inverse()
        for j in range(ny):
            for i in range(nx):
                wp = ltf.transform_location(unreal.Vector(i0 + i, j0 + j, 0))
                lp = cinv.transform_location(wp)
                dx = max(inner_lo_x - lp.x, 0, lp.x - inner_hi_x) * abs(ctf.scale3d.x)
                dy = max(inner_lo_y - lp.y, 0, lp.y - inner_hi_y) * abs(ctf.scale3d.y)
                dist = math.hypot(dx, dy)
                if dist >= blend:
                    continue
                t = 1.0 if dist <= 0 else 0.5 * (1 + math.cos(math.pi * dist / blend))
                idx = j * nx + i
                new = int(round(heights[idx] + (target_h - heights[idx]) * t))
                if new != heights[idx]:
                    heights[idx] = new
                    changed += 1
        open(hfile, 'wb').write(struct.pack('<%dH' % (nx * ny), *heights))
        ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(land, i0, j0, i1, j1, hfile, [], [])
        log(f'flatten {label}: region ({i0},{j0})-({i1},{j1}) to z {target_z:.0f} (ground was {zs[0]:.0f}..{zs[-1]:.0f}), '
            f'{changed} vertices changed, import {ok}')


def set_factory_exit():
    """The factory's gate faces a hall wall 2.3 m away (B2), so its AITrainingExit sat off the tank navmesh. Owner pick
    2026-11-24 (option A): put this instance's exit in the open west end of the hall (ruin local y +17 m, between the
    factory and the portico), facing the portico. Checked on all four navmeshes, 64 m path to the forecourt."""
    w = world()
    ruin = ruin_actor(w)
    fac = base_actors(w)['RT_Base_Rus_Building_Factory_1']
    rtf = ruin.get_actor_transform()
    target = rtf.transform_location(unreal.Vector(0, 1700, 0))
    exit_comp = [c for c in fac.get_components_by_class(unreal.SceneComponent) if c.get_name() == 'AITrainingExit'][0]
    fac.modify()
    exit_comp.modify()
    exit_comp.set_world_location(unreal.Vector(target.x, target.y, 50), False, False)
    yaw = unreal.MathLibrary.make_rot_from_x(rtf.transform_direction(unreal.Vector(0, 1, 0))).yaw
    exit_comp.set_world_rotation(unreal.Rotator(roll=0, pitch=0, yaw=yaw), False, False)
    log(f'factory exit at {exit_comp.get_world_location()} yaw {yaw:.0f}')


def describe():
    w = world()
    lines = []
    for label, actor in sorted(base_actors(w).items()):
        loc = actor.get_actor_location()
        rtsc = actor.get_components_by_class(unreal.RTSComponent) if hasattr(unreal, 'RTSComponent') else []
        owner = rtsc[0].get_editor_property('owning_player') if rtsc else '?'
        lines.append(f'{label} at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}) yaw {actor.get_actor_rotation().yaw:.0f} '
                     f'owner {owner} tags {[str(t) for t in actor.tags]}')
    return lines
