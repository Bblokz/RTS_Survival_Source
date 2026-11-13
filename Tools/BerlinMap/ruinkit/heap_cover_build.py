"""Berlin rubble heaps as natural cover (2026-11-13): the loose rubble heaps of Ger_Berlin become destructible cover.

Headless (never in the shared editor):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
      -DisablePlugins=Fab,BpGeneratorUltimate
Idempotent. Report: Saved/Berlin/cover_bp/heap_build_report.json. Doc: Source/Docs/Environment/BerlinRubbleCover.md.

Each source heap mesh (five heaps of the Destroyed Building Kit, the two modelled collapse heaps of the footprint
skirts) is duplicated to Cover/Meshes/SM_BerlinCoverHeap_<Name>, so the shared kit meshes stay untouched. The copy
gets the kit's rubble material, simple collision (convex hulls) where the source had none, and prone cover sockets
all round its foot: a heap is low cover from every side, so soldiers lie behind it.

Blueprints: Cover/BP_BerlinCoverHeap_<Name>, children of BP_BerlinCover_Base (-> BP_NaturalCover), built with the
helpers of cover_build.py: health on HealthComp, hidden NavArea_Null boxes per hull, and a WallHoleDamage component
that is used for the death only (collapse into kit debris, actor destroyed 4.5 s later, sockets and nav boxes freed).
A heap has no wall to punch through, so it takes no shell holes: DamagedMaterials stays empty.

Socket rule (same as the other natural cover): feet near the ground, +X of the socket points at the cover, name
"prone_<n>", about 90 cm outside the heap's body, at least 30 cm clear of every nav box, 2.4 m or more apart.
"""
import json
import math
import os
import sys

import unreal

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit')
import cover_build as cb  # noqa: E402

DBK = '/Game/DestroyedBuildingKit/'
FOOT = cb.ROOT + 'Footprints/'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/cover_bp/heap_build_report.json'
# name -> (source mesh name, material instance, health, damage reduction, socket z)
# The kit heaps are placed sunk 20-40 cm into the ground, so their sockets sit 30 cm above the mesh origin.
HEAPS = {
    'Brick': ('DBK_Brick_Rubble_A', 'MI_BerlinRuin_Rubble', 900, 8, 30.0),
    'Block': ('DBK_Cinderblock_Rubble_A', 'MI_BerlinRuin_Rubble', 900, 8, 30.0),
    'Concrete_A': ('DBK_Concrete_Rubble_Mid_A', 'MI_BerlinRuin_Rubble', 1000, 8, 30.0),
    'Concrete_B': ('DBK_Concrete_Rubble_Mid_B', 'MI_BerlinRuin_Rubble', 1000, 8, 30.0),
    'Concrete_C': ('DBK_Concrete_Rubble_Mid_C', 'MI_BerlinRuin_Rubble', 1000, 8, 30.0),
    'Collapse_S': ('SM_BerlinFoot_CollapseHeap_S', 'MI_BerlinRuin_Rubble_Foot', 1600, 10, 15.0),
    'Collapse_L': ('SM_BerlinFoot_CollapseHeap_L', 'MI_BerlinRuin_Rubble_Foot', 2200, 10, 15.0),
}
BODY_MIN_Z = 30.0          # hull vertices above this make the body a soldier lies behind
SOCKET_OUT = 50.0
SOCKET_SPACING = 260.0
MAX_SOCKETS = 8
EAL = unreal.EditorAssetLibrary
ESL = unreal.EditorStaticMeshLibrary


def find_source(name):
    if name.startswith('SM_BerlinFoot_'):
        return FOOT + name
    probe = json.load(open('D:/UE5Projects/RTS_Survival/Saved/Berlin/logs/probe_kit.json'))['meshes']
    for path in probe:
        if path.rsplit('/', 1)[1] == name:
            return path
    raise RuntimeError('source mesh not found: ' + name)


def ensure_mesh(key):
    source_name, material, _, _, _ = HEAPS[key]
    path = cb.MESHES + '/SM_BerlinCoverHeap_' + key
    if not EAL.does_asset_exist(path):
        if EAL.duplicate_asset(find_source(source_name), path) is None:
            raise RuntimeError('duplicate failed for ' + path)
    mesh = unreal.load_asset(path)
    mi = unreal.load_asset(cb.MATS + material)
    if mi is None:
        raise RuntimeError('missing material ' + material)
    for index in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(index, mi)
    if not cb.hull_points(mesh):
        ESL.set_convex_decomposition_collisions(mesh, 6, 16, 100000)
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    return mesh


def polygon_offset_points(hull, spacing):
    """Points every `spacing` along a convex polygon (counter-clockwise), with the outward normal of their edge."""
    n = len(hull)
    lengths = [math.dist(hull[i], hull[(i + 1) % n]) for i in range(n)]
    total = sum(lengths)
    count = int(max(3, min(MAX_SOCKETS, round(total / spacing))))
    out = []
    for k in range(count):
        target = (k + 0.5) * total / count
        for i in range(n):
            if target <= lengths[i] or i == n - 1:
                a, b = hull[i], hull[(i + 1) % n]
                t = min(1.0, target / max(lengths[i], 1e-6))
                ex, ey = (b[0] - a[0]) / max(lengths[i], 1e-6), (b[1] - a[1]) / max(lengths[i], 1e-6)
                out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, ey, -ex))
                break
            target -= lengths[i]
    return out


def make_sockets(hulls, boxes, socket_z):
    """Prone points round the heap. The outline is the heap body plus every nav box (a soldier cannot lie inside a
    blocked box), so the points end up 0.5-1.5 m from the rubble that covers them."""
    body = [p for pts in hulls for p in pts if p[2] >= BODY_MIN_Z]
    for b in boxes:
        cx, cy, hu, hv, ang = b['rect']
        c, s_ = math.cos(ang), math.sin(ang)
        for u, v in ((-hu, -hv), (hu, -hv), (hu, hv), (-hu, hv)):
            body.append((cx + u * c - v * s_, cy + u * s_ + v * c, BODY_MIN_Z))
    hull = cb.convex_hull_2d(body)
    sockets = []
    for px, py, nx, ny in polygon_offset_points(hull, SOCKET_SPACING):
        yaw = math.degrees(math.atan2(-ny, -nx))
        x, y = px + nx * SOCKET_OUT, py + ny * SOCKET_OUT
        clear = min((cb.rect_distance(b['rect'], x, y) for b in boxes), default=999.0)
        hit = cb.ray_hits_hull(hulls, x, y, socket_z + 20.0, yaw, length=260.0)
        if clear < 30.0 or hit is None:
            continue
        if any(math.hypot(x - s['x'], y - s['y']) < 150.0 for s in sockets):
            continue
        sockets.append(dict(name='prone_%d' % (len(sockets) + 1), x=round(x, 1), y=round(y, 1), z=socket_z,
                            yaw=round(yaw, 1), cover_cm=round(hit)))
    return sockets


def apply_sockets(mesh, sockets):
    for s in list(mesh.get_sockets_by_tag('')):
        mesh.remove_socket(s)
    for s in sockets:
        sock = unreal.StaticMeshSocket(mesh)
        sock.set_editor_property('socket_name', s['name'])
        sock.set_editor_property('relative_location', unreal.Vector(s['x'], s['y'], s['z']))
        sock.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=s['yaw']))
        mesh.add_socket(sock)
    mesh.modify()
    unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)


def build_blueprint(key, mesh, hulls, boxes, pieces, base):
    _, _, health, reduction, _ = HEAPS[key]
    name = 'BP_BerlinCoverHeap_' + key
    bp, created = cb.get_or_create_blueprint(name, base)
    named = cb._named(bp)
    if 'WallHoleDamage' not in named:
        cb._add_component(bp, named['__actor__'][0], unreal.WallHoleDamageComponent, 'WallHoleDamage')
    existing = sorted(n for n in named if n.startswith('NavBlock_'))
    for extra in existing[len(boxes):]:
        cb.SDS.delete_subobject(named['__actor__'][0], named[extra][0], bp)
    for index in range(len(existing), len(boxes)):
        cb._add_component(bp, named['__root__'][0], unreal.BoxComponent, 'NavBlock_%02d' % index)
    named = cb._named(bp)
    cb.setup_mesh_template(cb._own(bp, named, 'MainMesh'), mesh)
    cb.setup_health_template(cb._own(bp, named, 'HealthComp'), health, reduction)
    holes = cb._own(bp, named, 'WallHoleDamage')
    cb.setup_holes_template(holes, pieces)
    holes.set_editor_property('DamagedMaterials', [])        # no wall to punch through: the component only runs the death
    for index, box in enumerate(boxes):
        cb.setup_nav_box(cb._own(bp, named, 'NavBlock_%02d' % index), box)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    top = max(p[2] for pts in hulls for p in pts)
    cdo.set_editor_property('aim_offset_points', [unreal.Vector(0.0, 0.0, round(min(max(top * 0.5, 40.0), 120.0)))])
    cdo.set_editor_property('health', float(health))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return dict(blueprint=bp.get_path_name(), created=created, saved=bool(EAL.save_loaded_asset(bp, False)),
                health=health, damage_reduction=reduction)


def run():
    report = {'heaps': {}, 'errors': []}
    before = cb.parent_snapshot()
    pieces = cb.debris_meshes()
    base = cb.ensure_base()
    for key in HEAPS:
        try:
            mesh = ensure_mesh(key)
            hulls = cb.hull_points(mesh)
            if not hulls:
                raise RuntimeError('no collision hulls')
            boxes = cb.nav_boxes(hulls)
            sockets = make_sockets(hulls, boxes, HEAPS[key][4])
            apply_sockets(mesh, sockets)
            box = mesh.get_bounding_box()
            info = dict(mesh=mesh.get_path_name(), hulls=len(hulls), nav_boxes=len(boxes), sockets=sockets,
                        bounds=[round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)])
            if len(sockets) < 3:
                report['errors'].append('%s: only %d sockets' % (key, len(sockets)))
            info.update(build_blueprint(key, mesh, hulls, boxes, pieces, base))
            report['heaps'][key] = info
            cb.log('heap %s: hulls %d, boxes %d, sockets %d, bounds %s' % (key, len(hulls), len(boxes), len(sockets), info['bounds']))
        except Exception as error:  # noqa: BLE001
            report['errors'].append('%s: %r' % (key, error))
            cb.log('heap %s: ERROR %r' % (key, error))
    report['natural_cover_master_unchanged'] = before == cb.parent_snapshot()
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    cb.log('heaps done: %d, errors %s' % (len(report['heaps']), report['errors']))
    return report


if __name__ == '__main__':
    run()
