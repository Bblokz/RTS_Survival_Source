"""Berlin rubble natural cover: import the 10 cover meshes and build one Blueprint per piece (2026-11-13).

Headless (never in the shared editor):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
      -DisablePlugins=Fab,BpGeneratorUltimate
or run() in your own throwaway editor. Idempotent: re-imports the FBX over the meshes, re-applies sockets and every
Blueprint setting. Never deletes a Blueprint. Report: Saved/Berlin/cover_bp/build_report.json.
Doc: Source/Docs/Environment/BerlinRubbleCover.md.

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/Cover
  (generate_cover.py -> FBX/SM_BerlinCover_<NN>_<Name>.FBX with UCX hulls, cover_sockets.json in Unreal axes).

Meshes:     /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/Meshes/SM_BerlinCover_<NN>_<Name>
Blueprints: /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Cover/BP_BerlinCover_<NN>_<Name>
            parent BP_BerlinCover_Base (graph-less) -> BP_NaturalCover (natural cover master, C++ ADestructableActorHealth)

Why this works on one actor:
- Cover: the cover subsystem registers every actor derived from BP_NaturalCover (M_SocketCoverActorClasses) and turns
  the sockets of its first mesh with sockets (MainMesh) into cover points; destroyed actors drop them.
- Holes: ADestructableActorHealth::TakeDamage applies the hit to HealthComp and forwards it to the hole components
  (ForwardDamageToHoleComponents) before the death check; its BeginPlay hands HealthComp's max health to the hole
  component. The death (OnUnitDies -> OnDestructableEnvActorDied) triggers the hole component's
  bCollapseIntoChunksOnDeath: pieces fly, the mesh hides and loses collision, the actor is destroyed 4.5 s later, which
  frees the cover sockets and the nav boxes.
- BP_NaturalCover's BeginPlay (SetupComponentCollisions(MainMesh), Overlap Tanks) gives the mesh its weapon
  collision; the template gets the same values here so the editor nav build and traces agree before BeginPlay.
- Crushing (2026-11-13, CRUSH table below): BP_BerlinCover_Base carries the only graph (authored once through a
  UECP editor, never by this script): BeginPlay -> parent -> SetupCrush (if Crushable: ConfigureVerticalCollapse(MainMesh)
  + SetupCrushDestructionOverlap(MainMesh, CrushDestructionType, VerticalCollapse)); BP_OnUnitDies -> HandleCrushableDeath
  (if Crushable: Scavenging = crushed -> VerticalDestruction sink + destroy, any other death -> the hole component's
  CollapseIntoPieces). This script only sets the per-piece values: Crushable, CrushDestructionType, Overlap Tanks,
  the sink depth, the nav area of the NavBlock boxes and collapse_into_chunks_on_death off (the graph calls it).
  Pieces not in CRUSH (and the heaps of heap_cover_build.py) keep Crushable off and behave as before.
Inherited components (MainMesh, HealthComp) are edited through get_object_for_blueprint, which creates the child's
inherited-component override; BP_NaturalCover itself is checked to be unchanged and is never saved.
"""
import json
import math
import re

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinRuinKit/Model/Cover/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/'
COVER = ROOT + 'Cover'
MESHES = COVER + '/Meshes'
MATS = ROOT + 'Materials/'
DEBRIS = ROOT + 'Debris'
HOLES_MI = MATS + 'MI_BerlinRuin_Wall_Holes'
NATURAL_COVER = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/BP_NaturalCover'
BASE_NAME = 'BP_BerlinCover_Base'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/cover_bp/build_report.json'
COLLAPSE_CUE = '/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue'
COLLAPSE_ATT = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
COLLAPSE_CONC = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'
SLOT_TO_INSTANCE = {'M_BerlinRuin_Wall': 'MI_BerlinRuin_Wall', 'M_BerlinRuin_Rubble': 'MI_BerlinRuin_Rubble'}
EXPECTED_SLOTS = ['M_BerlinRuin_Wall', 'M_BerlinRuin_Rubble']

# Mesh suffix -> (health, damage reduction). Low brick cover: below the ruin fillers (1100-1800), about a tractor.
PIECES = {
    '01_WallRun': (1200, 8), '02_CornerL': (1400, 8), '03_HeapChunk': (1800, 8), '04_ToppledSlab': (1100, 8),
    '05_FallenChimney': (1300, 8), '06_Breastwork': (1000, 8), '07_Doorway': (1400, 8), '08_FacadePanels': (1700, 8),
    '09_CellarLip': (1600, 8), '10_MasonryBlock': (1500, 10),
}
# Holes on 0.5-1.4 m walls: smaller than the ruin "stub" class (0.8 / 100) so a 75 mm hit bites ~45 cm, not the whole wall.
HOLES = dict(radius=0.6, max=90.0, min=10.0, area=3000.0, max_hit=3, collapse=45, speed=1.0)

# Crush rule per piece: which tank classes drive through and crush it (ECrushDestructionType). Pieces left out are not
# crushable (NavArea_Null boxes, everything paths around). The NavBlock boxes get the nav area that only the crushing
# classes may cross: the per-agent query filters (/Game/RTS_Survival/Blueprints/Nav/Filters) exclude
#   RTS_NavArea_BlockCharLight        for Character + LightTank           -> MediumOrHeavyTank pieces
#   RTS_NavArea_BlockCharLightMedium  for Character + LightTank + Medium  -> HeavyTank pieces
# the same areas BP_Cover_Log1 (MediumOrHeavy) and BP_Cover_DeadTreeLog_01 (HeavyTank) use.
CRUSH = {
    '06_Breastwork': 'MEDIUM_OR_HEAVY_TANK', '04_ToppledSlab': 'MEDIUM_OR_HEAVY_TANK',
    '08_FacadePanels': 'MEDIUM_OR_HEAVY_TANK', '01_WallRun': 'MEDIUM_OR_HEAVY_TANK', '02_CornerL': 'MEDIUM_OR_HEAVY_TANK',
    '03_HeapChunk': 'HEAVY_TANK', '09_CellarLip': 'HEAVY_TANK', '10_MasonryBlock': 'HEAVY_TANK',
    '05_FallenChimney': 'HEAVY_TANK', '07_Doorway': 'HEAVY_TANK',
}
NAV_AREAS = '/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/'
CRUSH_NAV_AREA = {'MEDIUM_OR_HEAVY_TANK': 'RTS_NavArea_BlockCharLight', 'HEAVY_TANK': 'RTS_NavArea_BlockCharLightMedium',
                  'ANY_TANK': 'RTS_NavArea_BlockChar'}
# Crushed look, as the crushable pipes (BP_Dest_Crushable_RusPipes1): a slow lean (tip speed 0.1 = about 11 deg in 3 s)
# while the piece sinks its own height into the ground, then the actor is destroyed.
CRUSH_TIP_SPEED = 0.1
CRUSH_SINK_TIME = 3.0

# Navigation boxes: one per UCX hull that is taller than NAV_MIN_TOP (walls, real heaps; flat rubble beds stay walkable)
# and starts below NAV_MAX_BOTTOM, as the min-area rectangle of its footprint (yawed for diagonal walls), NavArea_Null.
NAV_MIN_TOP = 40.0
NAV_MAX_BOTTOM = 150.0
NAV_BOX_TOP = 400.0
WALL_MAX_THICKNESS = 170.0

EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary


def log(message):
    unreal.log_warning('BERLIN_COVER ' + message)


# ---------------------------------------------------------------- geometry helpers
def hull_points(mesh):
    """Mesh-space vertices of each UCX hull (from the convex element's export text; VertexData is not exposed)."""
    body = mesh.get_editor_property('body_setup')
    if body is None:
        return []
    out = []
    for elem in body.get_editor_property('agg_geom').get_editor_property('convex_elems'):
        text = elem.export_text().split('IndexData')[0]
        verts = [tuple(float(v) for v in m) for m in re.findall(r'X=([-\d.eE+]+),Y=([-\d.eE+]+),Z=([-\d.eE+]+)', text)]
        if verts:
            out.append(verts)
    return out


def convex_hull_2d(points):
    pts = sorted(set((round(p[0], 2), round(p[1], 2)) for p in points))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def min_area_rect(points):
    """(cx, cy, half_u, half_v, angle_rad); same rule as the Blender validator (cover_lib.min_area_rect)."""
    hull = convex_hull_2d(points)
    best = None
    for i in range(len(hull)):
        x0, y0 = hull[i]
        x1, y1 = hull[(i + 1) % len(hull)]
        ang = math.atan2(y1 - y0, x1 - x0)
        c, s = math.cos(ang), math.sin(ang)
        us = [x * c + y * s for x, y in hull]
        vs = [-x * s + y * c for x, y in hull]
        area = (max(us) - min(us)) * (max(vs) - min(vs))
        if best is None or area < best[0] - 1e-9:
            uc, vc = (max(us) + min(us)) / 2, (max(vs) + min(vs)) / 2
            best = (area, uc * c - vc * s, uc * s + vc * c, (max(us) - min(us)) / 2, (max(vs) - min(vs)) / 2, ang)
    area, cx, cy, hu, hv, ang = best
    xs, ys = [p[0] for p in hull], [p[1] for p in hull]
    if area > 0.85 * (max(xs) - min(xs)) * (max(ys) - min(ys)):
        return (max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, (max(xs) - min(xs)) / 2, (max(ys) - min(ys)) / 2, 0.0
    while ang > math.pi / 4 + 1e-9:
        ang -= math.pi / 2
        hu, hv = hv, hu
    while ang <= -math.pi / 4 + 1e-9:
        ang += math.pi / 2
        hu, hv = hv, hu
    return cx, cy, hu, hv, ang


def rect_distance(rect, x, y):
    cx, cy, hu, hv, ang = rect
    c, s = math.cos(ang), math.sin(ang)
    u, v = (x - cx) * c + (y - cy) * s, -(x - cx) * s + (y - cy) * c
    return math.hypot(max(abs(u) - hu, 0.0), max(abs(v) - hv, 0.0))


def nav_boxes(hulls):
    boxes = []
    for pts in hulls:
        zs = [p[2] for p in pts]
        if max(zs) <= NAV_MIN_TOP or min(zs) >= NAV_MAX_BOTTOM:
            continue
        cx, cy, hu, hv, ang = min_area_rect(pts)
        boxes.append(dict(cx=cx, cy=cy, hu=hu, hv=hv, yaw=math.degrees(ang), z0=min(zs), z1=min(max(zs), NAV_BOX_TOP),
                          rect=(cx, cy, hu, hv, ang)))
    return boxes


def aim_points(hulls):
    """Inside the masonry hulls (thin in one direction): the hull's vertex mean, at least 40 cm up; up to 3, spread."""
    walls = []
    for pts in hulls:
        _, _, hu, hv, _ = min_area_rect(pts)
        if 2 * min(hu, hv) <= WALL_MAX_THICKNESS and max(p[2] for p in pts) >= 90:
            n = len(pts)
            mean = [sum(p[i] for p in pts) / n for i in range(3)]
            walls.append(((2 * max(hu, hv)) * max(p[2] for p in pts), (mean[0], mean[1], max(mean[2], 40.0))))
    walls.sort(reverse=True)
    picked = [walls[0][1]] if walls else [(0.0, 0.0, 60.0)]
    cands = [w[1] for w in walls[1:]]
    while cands and len(picked) < 3:
        far = max(cands, key=lambda p: min(math.dist(p, q) for q in picked))
        picked.append(far)
        cands.remove(far)
    return [unreal.Vector(round(p[0]), round(p[1]), round(p[2])) for p in picked]


def ray_hits_hull(hulls, x, y, z, yaw_deg, length=150.0):
    """Does a ray from the socket along its facing meet any hull's AABB within length (mirror / facing check)?"""
    dx, dy = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    best = None
    for pts in hulls:
        lo = [min(p[i] for p in pts) for i in range(3)]
        hi = [max(p[i] for p in pts) for i in range(3)]
        if not lo[2] <= z <= hi[2]:
            continue
        t0, t1 = 0.0, length
        ok = True
        for o, d, a, b in ((x, dx, lo[0], hi[0]), (y, dy, lo[1], hi[1])):
            if abs(d) < 1e-9:
                if not a <= o <= b:
                    ok = False
                continue
            ta, tb = (a - o) / d, (b - o) / d
            t0, t1 = max(t0, min(ta, tb)), min(t1, max(ta, tb))
        if ok and t0 <= t1:
            best = t0 if best is None else min(best, t0)
    return best


# ---------------------------------------------------------------- meshes
def import_mesh(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'FBX/' + name + '.FBX'
    task.destination_path = MESHES
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def finish_mesh(name, sockets):
    mesh = unreal.load_asset(MESHES + '/' + name)
    if mesh is None:
        raise RuntimeError('import made nothing for ' + name)
    slot_names = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    if slot_names != EXPECTED_SLOTS:
        raise RuntimeError(f'{name}: slot order {slot_names}, expected {EXPECTED_SLOTS}')
    for index, slot_name in enumerate(slot_names):
        mesh.set_material(index, unreal.load_asset(MATS + SLOT_TO_INSTANCE[slot_name]))
    mesh.set_editor_property('light_map_coordinate_index', 1)
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
    return mesh


def check_sockets(mesh, hulls, boxes):
    """Read the sockets back from the mesh: forward vector, pitch 0, facing ray meets a hull, clear of every nav box."""
    out, bad = [], []
    for s in mesh.get_sockets_by_tag(''):
        loc = s.get_editor_property('relative_location')
        rot = s.get_editor_property('relative_rotation')
        fwd = rot.get_forward_vector()
        ray_z = 20.0 if 'prone' in str(s.get_editor_property('socket_name')) else 60.0
        hit = ray_hits_hull(hulls, loc.x, loc.y, ray_z, math.degrees(math.atan2(fwd.y, fwd.x)))
        clear = min((rect_distance(b['rect'], loc.x, loc.y) for b in boxes), default=999.0)
        row = dict(name=str(s.get_editor_property('socket_name')), x=round(loc.x), y=round(loc.y), z=round(loc.z),
                   yaw=round(rot.yaw, 1), pitch=round(rot.pitch, 2), fwd=[round(fwd.x, 3), round(fwd.y, 3), round(fwd.z, 3)],
                   hull_hit_cm=None if hit is None else round(hit), nav_box_clear_cm=round(clear))
        if hit is None or hit > 150 or abs(rot.pitch) > 0.1 or abs(fwd.z) > 0.01 or clear < 30:
            bad.append(row['name'])
        out.append(row)
    return out, bad


# ---------------------------------------------------------------- Blueprints
def _handles(bp):
    return list(SDS.k2_gather_subobject_data_for_blueprint(bp))


def _named(bp):
    """{variable name: (handle, data)} plus '__actor__' and '__root__' (read only; no overrides are created here)."""
    out = {}
    for handle in _handles(bp):
        data = SDL.get_data(handle)
        template = SDL.get_object(data)
        if template is None or isinstance(template, unreal.Actor):
            out.setdefault('__actor__', (handle, data))
            continue
        name = str(SDL.get_variable_name(data))
        out[name] = (handle, data)
        if SDL.is_root_component(data):
            out['__root__'] = (handle, data)
    if '__root__' not in out and 'DefaultSceneRoot' in out:
        out['__root__'] = out['DefaultSceneRoot']
    return out


def _own(bp, named, name):
    """The child's own template of a component: its override for inherited components (created if needed)."""
    return SDL.get_object_for_blueprint(named[name][1], bp)


def _add_component(bp, parent_handle, cls, name):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f'{bp.get_name()}: adding {name} failed: {fail}')
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle


def parent_snapshot():
    """BP_NaturalCover's own templates; compared after the build to prove no child edit leaked into the master."""
    bp = unreal.load_asset(NATURAL_COVER)
    snap = {}
    for name, (handle, data) in _named(bp).items():
        if name.startswith('__'):
            continue
        t = SDL.get_object(data)
        if isinstance(t, unreal.StaticMeshComponent):
            snap[name] = (str(t.get_editor_property('static_mesh')), t.get_editor_property('can_ever_affect_navigation'))
        elif t.get_class().get_name() == 'HealthComponent':
            snap[name] = (t.get_editor_property('MaxHealth'), t.get_editor_property('CurrentHealth'),
                          t.get_editor_property('DamageReductionSettings').export_text())
    return snap


def ensure_base():
    path = f'{COVER}/{BASE_NAME}'
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.load_asset(NATURAL_COVER).generated_class())
    base = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BASE_NAME, COVER, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(base)
    EAL.save_loaded_asset(base, False)
    log('created ' + path)
    return base


def get_or_create_blueprint(name, base):
    path = f'{COVER}/{name}'
    if EAL.does_asset_exist(path):
        bp = unreal.load_asset(path)
        if not unreal.MathLibrary.class_is_child_of(bp.generated_class(), base.generated_class()):
            raise RuntimeError(f'{name} exists but is not a child of {BASE_NAME}')
        return bp, False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', base.generated_class())
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, COVER, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError('could not create ' + path)
    return bp, True


def setup_mesh_template(comp, mesh):
    comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('override_materials', [])
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('receives_decals', False)
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_collision_object_type(unreal.CollisionChannel.ECC_DESTRUCTIBLE)
    comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    ch = unreal.CollisionChannel
    comp.set_collision_response_to_channel(ch.ECC_BUILDING_PLACEMENT, unreal.CollisionResponseType.ECR_OVERLAP)
    for blocked in (ch.ECC_VISIBILITY, ch.ECC_PROJECTILE, ch.ECC_ENEMY, ch.ECC_PLAYER):
        comp.set_collision_response_to_channel(blocked, unreal.CollisionResponseType.ECR_BLOCK)


def setup_health_template(comp, health, reduction):
    comp.set_editor_property('MaxHealth', float(health))
    comp.set_editor_property('CurrentHealth', float(health))
    red = comp.get_editor_property('DamageReductionSettings')
    red.set_editor_property('DamageReduction', float(reduction))
    comp.set_editor_property('DamageReductionSettings', red)


def setup_holes_template(comp, pieces, collapse_on_death=True):
    """collapse_on_death False: the Blueprint graph decides the death (crushable pieces: sink when crushed, else
    CollapseIntoPieces); True: the component collapses on every death by itself."""
    c = HOLES
    comp.set_editor_property('WallMeshComponentName', 'MainMesh')
    comp.set_editor_property('DamagedMaterials', [unreal.load_asset(HOLES_MI)])
    comp.set_editor_property('DebrisMeshes', pieces)
    hole = comp.get_editor_property('HoleSettings')
    hole.set_editor_property('hole_radius_per_calibre_mm', c['radius'])
    hole.set_editor_property('max_hole_radius', c['max'])
    comp.set_editor_property('HoleSettings', hole)
    grid = comp.get_editor_property('GridSettings')
    grid.set_editor_property('grid_axis', unreal.WallHoleGridAxis.NO_GRID)
    grid.set_editor_property('hole_centre_outset_share', 0.0)
    grid.set_editor_property('min_hole_radius', c['min'])
    grid.set_editor_property('area_per_debris_piece', c['area'])
    grid.set_editor_property('max_pieces_per_hit', c['max_hit'])
    grid.set_editor_property('max_collapse_pieces', c['collapse'])
    comp.set_editor_property('GridSettings', grid)
    collapse = comp.get_editor_property('CollapseSettings')
    collapse.set_editor_property('collapse_into_chunks_on_death', bool(collapse_on_death))
    collapse.set_editor_property('destroy_owner_delay_seconds', 4.5)
    collapse.set_editor_property('death_speed_scale', c['speed'])
    collapse.set_editor_property('collapse_sound', unreal.load_asset(COLLAPSE_CUE))
    collapse.set_editor_property('collapse_attenuation', unreal.load_asset(COLLAPSE_ATT))
    collapse.set_editor_property('collapse_concurrency', unreal.load_asset(COLLAPSE_CONC))
    comp.set_editor_property('CollapseSettings', collapse)


def nav_area_class(name):
    """Generated class of a project nav area Blueprint (RTS_NavArea_*)."""
    cls = unreal.load_class(None, f'{NAV_AREAS}{name}.{name}_C')
    if cls is None:
        raise RuntimeError('nav area missing: ' + name)
    return cls


def setup_nav_box(comp, box, area_class=None):
    comp.set_editor_property('relative_location', unreal.Vector(box['cx'], box['cy'], (box['z0'] + box['z1']) * 0.5))
    comp.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=box['yaw']))
    comp.set_editor_property('box_extent', unreal.Vector(box['hu'], box['hv'], (box['z1'] - box['z0']) * 0.5))
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_editor_property('can_ever_affect_navigation', True)
    comp.set_editor_property('dynamic_obstacle', True)
    comp.set_editor_property('area_class_override', area_class or unreal.NavArea_Null)
    comp.set_editor_property('use_system_default_obstacle_area_class', False)
    comp.set_editor_property('visible', False)
    comp.set_editor_property('hidden_in_game', True)


def base_has_crush_graph(base):
    """True when BP_BerlinCover_Base carries the crush variables (and so the graph that reads them)."""
    cdo = unreal.get_default_object(base.generated_class())
    try:
        cdo.get_editor_property('Crushable')
        cdo.get_editor_property('CrushDestructionType')
        return True
    except Exception:  # noqa: BLE001
        return False


def setup_crush_defaults(cdo, rule, mesh):
    """Class defaults read by BP_BerlinCover_Base's graph. rule None switches crushing off. Returns the sink depth."""
    cdo.set_editor_property('Crushable', rule is not None)
    # BP_NaturalCover's BeginPlay passes this to SetupComponentCollisions: MainMesh overlaps the vehicle channels.
    cdo.set_editor_property('Overlap Tanks', rule is not None)
    if rule is None:
        return None
    cdo.set_editor_property('CrushDestructionType', getattr(unreal.CrushDestructionType, rule))
    box = mesh.get_bounding_box()
    half = max(box.max.x - box.min.x, box.max.y - box.min.y) * 0.5
    # Its own height plus what the lean lifts the near edge (sin 11 deg = 0.2), so nothing sticks out at the end.
    sink = round(box.max.z + 0.2 * half + 30.0)
    cdo.set_editor_property('CrushTipSpeedScale', CRUSH_TIP_SPEED)
    cdo.set_editor_property('CrushSinkDeltaZ', float(sink))
    cdo.set_editor_property('CrushSinkTime', CRUSH_SINK_TIME)
    return sink


def build_one(suffix, mesh, hulls, boxes, pieces, base):
    name = 'BP_BerlinCover_' + suffix
    health, reduction = PIECES[suffix]
    bp, created = get_or_create_blueprint(name, base)
    named = _named(bp)
    if 'MainMesh' not in named or 'HealthComp' not in named or '__root__' not in named:
        raise RuntimeError(f'{name}: inherited MainMesh / HealthComp / root missing: {list(named)}')
    if 'WallHoleDamage' not in named:
        _add_component(bp, named['__actor__'][0], unreal.WallHoleDamageComponent, 'WallHoleDamage')
    existing = sorted(n for n in named if n.startswith('NavBlock_'))
    for extra in existing[len(boxes):]:
        SDS.delete_subobject(named['__actor__'][0], named[extra][0], bp)
    for index in range(len(existing), len(boxes)):
        _add_component(bp, named['__root__'][0], unreal.BoxComponent, 'NavBlock_%02d' % index)
    named = _named(bp)
    setup_mesh_template(_own(bp, named, 'MainMesh'), mesh)
    setup_health_template(_own(bp, named, 'HealthComp'), health, reduction)
    has_graph = base_has_crush_graph(base)
    rule = CRUSH.get(suffix) if has_graph else None
    area = nav_area_class(CRUSH_NAV_AREA[rule]) if rule else None
    setup_holes_template(_own(bp, named, 'WallHoleDamage'), pieces, collapse_on_death=rule is None)
    for index, box in enumerate(boxes):
        setup_nav_box(_own(bp, named, 'NavBlock_%02d' % index), box, area)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    aims = aim_points(hulls)
    cdo.set_editor_property('aim_offset_points', aims)
    # ADestructableEnvActor::Health is unused by this class (HealthComp holds the health); kept equal for readers.
    cdo.set_editor_property('health', float(health))
    sink = setup_crush_defaults(cdo, rule, mesh) if has_graph else None
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = EAL.save_loaded_asset(bp, False)
    return dict(blueprint=bp.get_path_name(), created=created, saved=bool(saved), health=health,
                crush=rule, crush_nav_area=CRUSH_NAV_AREA[rule] if rule else 'NavArea_Null', crush_sink_cm=sink,
                damage_reduction=reduction, nav_boxes=[[round(b['cx']), round(b['cy']), round(b['hu']), round(b['hv']),
                                                        round(b['yaw'], 1), round(b['z0']), round(b['z1'])] for b in boxes],
                aims=[[a.x, a.y, a.z] for a in aims])


def debris_meshes():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    found = [a.get_asset() for a in reg.get_assets_by_path(DEBRIS, recursive=False)]
    return sorted([m for m in found if isinstance(m, unreal.StaticMesh) and m.get_name().startswith('SM_BerlinRuin_Debris_')],
                  key=lambda m: m.get_name())


def run(only=None, import_meshes=True):
    sockets_by_mesh = json.load(open(MODEL + 'cover_sockets.json'))
    report = {'pieces': {}, 'errors': []}
    before = parent_snapshot()
    pieces = debris_meshes()
    if not pieces:
        raise RuntimeError('no kit debris in ' + DEBRIS)
    names = [n for n in sockets_by_mesh if not only or n[len('SM_BerlinCover_'):] in only]
    if import_meshes:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
        try:
            for name in names:
                import_mesh(name)
        finally:
            unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    base = ensure_base()
    report['base_has_crush_graph'] = base_has_crush_graph(base)
    if not report['base_has_crush_graph']:
        report['errors'].append('BP_BerlinCover_Base has no Crushable / CrushDestructionType variables: crush settings '
                                'skipped, every piece built not crushable (BerlinRubbleCover.md, "Crushing")')
    for name in names:
        suffix = name[len('SM_BerlinCover_'):]
        try:
            mesh = finish_mesh(name, sockets_by_mesh[name]) if import_meshes else unreal.load_asset(MESHES + '/' + name)
            hulls = hull_points(mesh)
            boxes = nav_boxes(hulls)
            socks, bad = check_sockets(mesh, hulls, boxes)
            box = mesh.get_bounding_box()
            info = dict(mesh=mesh.get_path_name(), hulls=len(hulls), sockets=socks, bad_sockets=bad,
                        bounds=[round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
                        materials=[mesh.get_material(i).get_name() for i in range(2)])
            if bad:
                report['errors'].append(f'{suffix}: bad sockets {bad}')
            info.update(build_one(suffix, mesh, hulls, boxes, pieces, base))
            report['pieces'][suffix] = info
            log(f'{suffix}: ' + json.dumps({k: info[k] for k in ('hulls', 'bad_sockets', 'health', 'crush', 'crush_nav_area', 'saved')}))
        except Exception as error:  # noqa: BLE001
            report['errors'].append(f'{suffix}: {error!r}')
            log(f'{suffix}: ERROR {error!r}')
    after = parent_snapshot()
    report['natural_cover_master_unchanged'] = before == after
    if before != after:
        report['errors'].append(f'BP_NaturalCover templates changed in memory: {before} -> {after} (not saved)')
    import os
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    log(f'done: {len(report["pieces"])} pieces, errors {report["errors"]}, report {REPORT}')
    return report


if __name__ == '__main__':
    import os as _os
    # COVER_IMPORT=0: keep the meshes as they are and only re-apply the Blueprint settings.
    run(import_meshes=_os.environ.get('COVER_IMPORT', '1') != '0')
