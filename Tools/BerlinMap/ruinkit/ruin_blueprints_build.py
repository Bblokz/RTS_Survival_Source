"""Berlin ruin kit: one destructible Blueprint per kit mesh, with procedural shell holes (2026-11-12).

Headless (never in the shared editor):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
      -DisablePlugins=Fab,BpGeneratorUltimate
or run() inside your own throwaway editor. Re-run it whenever kit meshes are added or changed: it picks up every
SM_BerlinRuin_* in RuinKit/Meshes and creates or updates BP_BerlinRuin_<suffix> in RuinKit/Blueprints. Blueprints are
never deleted, so actors already placed keep working.

Per Blueprint (parent: BP_BerlinRuin_Base, a graph-less child of the C++ ADestructableEnvActor that only holds the
Movable DefaultSceneRoot; no graphs anywhere, because headless Python cannot author Blueprint graphs):
- StaticMesh (Movable, as the PostApo ruins): the kit mesh with its own slot materials; the collision that
  ADestructableEnvActor::SetupComponentCollisions would give it at BeginPlay is set on the template instead (object type
  Destructible, blocks Visibility / Projectile / TraceEnemy / TracePlayer, overlaps BuildingPlacement, ignores pawns,
  no decals) and it does not affect navigation.
- WallHoleDamage on StaticMesh: NoGrid, DamagedMaterials[0] = MI_BerlinRuin_Wall_Holes (slot 1 rubble never), kit debris
  SM_BerlinRuin_Debris_*, per-class hole size and debris counts (CLASSES), bCollapseIntoChunksOnDeath = true: on death the
  surface flies off as pieces, the mesh hides and loses collision, and the actor is destroyed 4.5 s later.
- NavBlock_NN: hidden, collision-less UBoxComponents set as dynamic navigation obstacles with NavArea_Null, one per
  ground-touching wall run / rubble heap (the mesh's UCX hulls merged where they line up). They block every agent's
  navmesh exactly where the hulls stand, are runtime modifiers (DynamicModifiersOnly), and leave the navmesh when the
  actor is destroyed.
- Class defaults: Health, DamageReduction and AimOffsetPoints (points on the tallest wall hulls).
Also: the kit debris meshes get MI_BerlinRuin_Wall_Holes when the opaque master lacks the Niagara mesh-particle flag.
Report: Saved/Berlin/ruinkit_bp/build_report.json. Doc: Source/Docs/Environment/BerlinRuinKit.md.
"""
import json
import math
import re

import unreal

ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/'
MESHES = ROOT + 'Meshes'
DEBRIS = ROOT + 'Debris'
BLUEPRINTS = ROOT + 'Blueprints'
HOLES_MI = ROOT + 'Materials/MI_BerlinRuin_Wall_Holes'
BASE_NAME = 'BP_BerlinRuin_Base'
REPORT ='D:/UE5Projects/RTS_Survival/Saved/Berlin/ruinkit_bp/build_report.json'
COLLAPSE_CUE = '/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue'
COLLAPSE_ATT = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
COLLAPSE_CONC = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'

# Size classes. Lengths are mesh cm (kit meshes stand at scale 1; fillers may be scaled up to 1.15, never below 1).
#   radius/mm: hole radius per mm calibre (75 mm -> 75 x value cm); max: largest hole; min: smallest hole worth a slot.
#   area/max_hit: face area per flying piece (cm2) and pieces per hit; collapse: most pieces on death.
CLASSES = {
    # Death pieces: the C++ caps a piece at 1.2x its mesh (kit debris is 38-68 cm), so fewer pieces do not get bigger;
    # the collapse counts stay near PostApo's 120 so a dead hero does not just vanish. Per-hit debris is what was cut.
    'hero':   dict(radius=1.1, max=220, min=15, area=6000, max_hit=4, collapse=120, speed=0.9, aims=5),
    'core':   dict(radius=1.0, max=200, min=15, area=6000, max_hit=3, collapse=100, speed=0.8, aims=5),
    'slab':   dict(radius=1.0, max=180, min=15, area=5000, max_hit=3, collapse=70, speed=1.0, aims=4),
    'filler': dict(radius=0.9, max=150, min=12, area=4500, max_hit=3, collapse=45, speed=1.0, aims=3),
    'stub':   dict(radius=0.8, max=100, min=12, area=4000, max_hit=2, collapse=25, speed=1.0, aims=2),
    'rubble': dict(radius=0.8, max=120, min=12, area=4000, max_hit=2, collapse=40, speed=0.8, aims=2),
}
# Mesh suffix -> (class, health, damage reduction). Unknown suffixes fall back by their id (see _settings_for).
PIECES = {
    'E1_CornerTower': ('hero', 6000, 15),
    'E2_Courtyard': ('hero', 5500, 15),
    'E3_CoreRow': ('core', 8000, 20),
    'B7_WallCore': ('core', 7000, 20),
    'B1_Facade1St_A': ('slab', 1200, 10), 'B1_Facade1St_B': ('slab', 1200, 10),
    'B2_Facade2St_A': ('slab', 2200, 10), 'B2_Facade2St_B': ('slab', 2200, 10),
    'B3_Facade4St': ('slab', 4000, 10),
    'B4_Firewall': ('slab', 4000, 10),
    'B5_CornerL': ('slab', 3000, 10),
    'B6_Stub_A': ('stub', 700, 5), 'B6_Stub_B': ('stub', 700, 5),
    'B8_DebrisBed': ('rubble', 1500, 25),
    'E4_StubRing_A': ('stub', 700, 5), 'E4_StubRing_B': ('stub', 700, 5),
    'E5_Shell1St_A': ('filler', 1400, 8), 'E5_Shell1St_B': ('filler', 1400, 8),
    'E6_HeapFin_A': ('filler', 1100, 8), 'E6_HeapFin_B': ('filler', 1100, 8),
    'E7_HalfHouse_A': ('filler', 1800, 8), 'E7_HalfHouse_B': ('filler', 1800, 8),
}
FALLBACK_BY_ID = {'E1': 'hero', 'E2': 'hero', 'E3': 'core', 'B7': 'core', 'B6': 'stub', 'E4': 'stub', 'B8': 'rubble'}
FALLBACK_HEALTH = {'hero': (5500, 15), 'core': (7000, 20), 'slab': (2500, 10), 'filler': (1400, 8), 'stub': (700, 5),
                   'rubble': (1500, 25)}

# Navigation boxes: hulls starting below this height (mesh cm) touch the ground navmesh; upper hulls (balconies, slab
# remains) are skipped. Boxes reach at most NAV_BOX_TOP high (agents only need the ground band).
NAV_MAX_BOTTOM = 150.0
NAV_BOX_TOP = 400.0
# Two boxes merge when their union footprint wastes at most this share of their own area and this much in absolute terms.
NAV_MERGE_WASTE_SHARE = 0.12
NAV_MERGE_WASTE_ABS = 2500.0
WALL_MIN_HEIGHT = 200.0
WALL_MAX_THICKNESS = 150.0
# Wide hulls lower than this are rubble mounds / heaps; wide and taller are wall blocks (tower corners, chimneys).
HEAP_MAX_TOP = 450.0

EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary


def log(message):
    unreal.log_warning('BERLIN_RUIN_BP ' + message)


# ---------------------------------------------------------------- mesh data

def hull_boxes(mesh):
    """Mesh-space AABBs (x0, y0, z0, x1, y1, z1) of the UCX convex hulls. Python can't read FKConvexElem.VertexData, so
    the vertices come from the element's export text."""
    body = mesh.get_editor_property('body_setup')
    if body is None:
        return []
    boxes = []
    for elem in body.get_editor_property('agg_geom').get_editor_property('convex_elems'):
        text = elem.export_text().split('IndexData')[0]
        verts = [tuple(float(v) for v in m) for m in
                 re.findall(r'X=([-\d.eE+]+),Y=([-\d.eE+]+),Z=([-\d.eE+]+)', text)]
        if not verts:
            continue
        xs, ys, zs = zip(*verts)
        boxes.append((min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)))
    return boxes


def _area(b):
    return max(b[3] - b[0], 0.0) * max(b[4] - b[1], 0.0)


def _union(a, b):
    return (min(a[0], b[0]), min(a[1], b[1]), min(a[2], b[2]), max(a[3], b[3]), max(a[4], b[4]), max(a[5], b[5]))


def nav_boxes(hulls):
    """Ground-touching hulls, merged greedily where they line up (one box per straight wall run or heap)."""
    boxes = [(h[0], h[1], h[2], h[3], h[4], min(h[5], NAV_BOX_TOP)) for h in hulls if h[2] < NAV_MAX_BOTTOM]
    merged = True
    while merged and len(boxes) > 1:
        merged = False
        best = None
        for i in range(len(boxes)):
            for j in range(i + 1, len(boxes)):
                u = _union(boxes[i], boxes[j])
                own = _area(boxes[i]) + _area(boxes[j])
                waste = _area(u) - own
                if waste <= min(NAV_MERGE_WASTE_SHARE * own, NAV_MERGE_WASTE_ABS) and (best is None or waste < best[0]):
                    best = (waste, i, j, u)
        if best is not None:
            _, i, j, u = best
            boxes = [b for k, b in enumerate(boxes) if k not in (i, j)] + [u]
            merged = True
    return sorted(boxes)


def aim_points(hulls, count):
    """Points inside the tallest wall hulls (vertical face area first, then spread out): shells aimed there strike the
    wall on the shooter's side, at about a third of its height."""
    # A wall is thin in one direction; wide-in-both hulls are rubble mounds and heaps (slot 1, never holed), and a hit
    # there would spend a hole slot without showing a hole.
    def is_thin(h):
        return min(h[3] - h[0], h[4] - h[1]) <= WALL_MAX_THICKNESS

    heaps = [h for h in hulls if not is_thin(h) and h[5] < HEAP_MAX_TOP]
    walls = [h for h in hulls if h[5] - h[2] >= WALL_MIN_HEIGHT and (is_thin(h) or h[5] >= HEAP_MAX_TOP)]
    if not walls:
        walls = sorted([h for h in hulls if is_thin(h)] or hulls, key=lambda h: -(h[5] - h[2]))[:max(count, 1)]

    def face(h):
        return max(h[3] - h[0], h[4] - h[1]) * (h[5] - h[2])

    def heap_top(x, y):
        tops = [h[5] for h in heaps if h[0] - 100 <= x <= h[3] + 100 and h[1] - 100 <= y <= h[4] + 100]
        return max(tops) if tops else 0.0

    def centre(h):
        x, y = (h[0] + h[3]) * 0.5, (h[1] + h[4]) * 0.5
        bottom = max(h[2], 0.0)
        if h[5] - h[2] >= WALL_MIN_HEIGHT and (is_thin(h) or h[5] >= HEAP_MAX_TOP):
            z = min(bottom + (h[5] - bottom) * 0.4, 650.0)
            z = max(z, min(heap_top(x, y) + 120.0, h[5] - 60.0))
        else:
            # Low hulls (stubs, heaps): a third of the way up, so a side shot still meets a sloped heap's flank.
            z = bottom + (h[5] - bottom) * 0.35
        return (x, y, max(z, min(80.0, h[5] - 10.0)))

    ordered = sorted(walls, key=face, reverse=True)
    picked = [centre(ordered[0])]
    candidates = [centre(h) for h in ordered[1:]]
    while candidates and len(picked) < count:
        far = max(candidates, key=lambda p: min(math.dist(p, q) for q in picked))
        picked.append(far)
        candidates.remove(far)
    return [unreal.Vector(round(p[0]), round(p[1]), round(p[2])) for p in picked]


def _settings_for(suffix):
    if suffix in PIECES:
        kind, health, reduction = PIECES[suffix]
    else:
        kind = FALLBACK_BY_ID.get(suffix.split('_')[0], 'filler' if suffix.startswith('E') else 'slab')
        health, reduction = FALLBACK_HEALTH[kind]
        log(f'{suffix}: not in PIECES, using class {kind} health {health}')
    return kind, health, reduction


# ---------------------------------------------------------------- debris material

def ensure_debris_material():
    """Debris flies as Niagara mesh particles: the material needs bUsedWithNiagaraMeshParticles or it renders as the
    default material in a cooked game. The holes instance has it (Berlin material doc)."""
    opaque_master = unreal.load_asset(ROOT + 'Materials/M_BerlinRuin')
    flag = bool(opaque_master.get_editor_property('used_with_niagara_mesh_particles'))
    holes = unreal.load_asset(HOLES_MI)
    pieces = debris_meshes()
    changed = []
    if not flag:
        for piece in pieces:
            if piece.get_material(0) != holes:
                piece.set_material(0, holes)
                changed.append(piece)
        if changed:
            unreal.EditorLoadingAndSavingUtils.save_packages([p.get_outermost() for p in changed], False)
    log(f'debris: opaque master niagara flag {flag}; {len(pieces)} pieces; switched to holes MI: '
        f'{[p.get_name() for p in changed]}')
    return {'opaque_master_niagara_flag': flag, 'switched': [p.get_name() for p in changed]}


def debris_meshes():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    found = [a.get_asset() for a in reg.get_assets_by_path(DEBRIS, recursive=False)]
    return sorted([m for m in found if isinstance(m, unreal.StaticMesh) and m.get_name().startswith('SM_BerlinRuin_Debris_')],
                  key=lambda m: m.get_name())


def kit_meshes():
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    found = [a.get_asset() for a in reg.get_assets_by_path(MESHES, recursive=False)]
    return sorted([m for m in found if isinstance(m, unreal.StaticMesh) and m.get_name().startswith('SM_BerlinRuin_')],
                  key=lambda m: m.get_name())


# ---------------------------------------------------------------- Blueprint plumbing

def _handles(bp):
    return list(SDS.k2_gather_subobject_data_for_blueprint(bp))


def _named(bp):
    """{variable name: (handle, template)} for the Blueprint's own components, plus the root under '__root__'."""
    out = {}
    for handle in _handles(bp):
        data = SDL.get_data(handle)
        template = SDL.get_object(data)
        if template is None or isinstance(template, unreal.Actor):
            out.setdefault('__actor__', (handle, template))
            continue
        name = str(SDL.get_variable_name(data))
        out[name] = (handle, template)
        if isinstance(template, unreal.SceneComponent) and SDL.is_root_component(data):
            out['__root__'] = (handle, template)
    if '__root__' not in out and isinstance(out.get('DefaultSceneRoot', (None, None))[1], unreal.SceneComponent):
        out['__root__'] = out['DefaultSceneRoot']
    return out


def _add_component(bp, parent_handle, cls, name):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f'{bp.get_name()}: adding {name} failed: {fail}')
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle


def _delete_component(bp, actor_handle, handle):
    SDS.delete_subobject(actor_handle, handle, bp)


def ensure_base():
    """BP_BerlinRuin_Base: the shared parent (C++ ADestructableEnvActor, a Movable DefaultSceneRoot, no graphs). It
    exists so a death graph (e.g. CollapseIntoPieces(false) + VerticalDestruction with destroy) can be added ONCE for
    the whole kit; see BerlinRuinKit.md. The builder never touches its graphs."""
    path = f'{BLUEPRINTS}/{BASE_NAME}'
    if EAL.does_asset_exist(path):
        base = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.DestructableEnvActor)
        base = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BASE_NAME, BLUEPRINTS, unreal.Blueprint, factory)
        named = _named(base)
        if '__root__' not in named:
            _add_component(base, named['__actor__'][0], unreal.SceneComponent, 'DefaultSceneRoot')
            named = _named(base)
        named['__root__'][1].set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
        unreal.BlueprintEditorLibrary.compile_blueprint(base)
        EAL.save_loaded_asset(base, False)
        log('created ' + path)
    return base


def get_or_create_blueprint(name, base):
    path = f'{BLUEPRINTS}/{name}'
    base_class = base.generated_class()
    if EAL.does_asset_exist(path):
        bp = unreal.load_asset(path)
        if not unreal.MathLibrary.class_is_child_of(bp.generated_class(), base_class):
            raise RuntimeError(f'{name} exists but is not a child of {BASE_NAME}; reparent or delete it by hand')
        return bp, False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', base_class)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, BLUEPRINTS, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError('could not create ' + path)
    return bp, True


def ensure_components(bp, nav_count):
    named = _named(bp)
    actor_handle = named['__actor__'][0]
    if '__root__' not in named:
        raise RuntimeError(f'{bp.get_name()}: no inherited DefaultSceneRoot from {BASE_NAME}')
    # The child's handle of the inherited root: components attached through it stay in the child.
    root_handle = named['__root__'][0]
    if 'StaticMesh' not in named:
        _add_component(bp, root_handle, unreal.StaticMeshComponent, 'StaticMesh')
    if 'WallHoleDamage' not in named:
        _add_component(bp, actor_handle, unreal.WallHoleDamageComponent, 'WallHoleDamage')
    named = _named(bp)
    existing = sorted(n for n in named if n.startswith('NavBlock_'))
    for extra in existing[nav_count:]:
        _delete_component(bp, actor_handle, named[extra][0])
    for index in range(len(existing), nav_count):
        _add_component(bp, root_handle, unreal.BoxComponent, 'NavBlock_%02d' % index)
    named = _named(bp)
    log(f'{bp.get_name()} components: ' + ', '.join(
        f'{n}:{t.get_class().get_name()}' for n, (h, t) in named.items() if not n.startswith('__')))
    return named


# ---------------------------------------------------------------- template settings

def setup_mesh_template(comp, mesh):
    comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('override_materials', [])
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('receives_decals', False)
    comp.set_editor_property('cast_shadow', True)
    # What FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision does at BeginPlay, baked into the template.
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_collision_object_type(unreal.CollisionChannel.ECC_DESTRUCTIBLE)
    comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    channel = unreal.CollisionChannel
    block = unreal.CollisionResponseType.ECR_BLOCK
    # Python names the project channels by their DefaultEngine.ini names: BuildingPlacement = GameTraceChannel8
    # (COLLISION_OBJ_BUILDING_PLACEMENT), Projectile = 3, Enemy = 5 (COLLISION_TRACE_ENEMY), Player = 7 (COLLISION_TRACE_PLAYER).
    comp.set_collision_response_to_channel(channel.ECC_BUILDING_PLACEMENT, unreal.CollisionResponseType.ECR_OVERLAP)
    for blocked in (channel.ECC_VISIBILITY, channel.ECC_PROJECTILE, channel.ECC_ENEMY, channel.ECC_PLAYER):
        comp.set_collision_response_to_channel(blocked, block)


def setup_holes_template(comp, kind, is_rubble_only, pieces):
    c = CLASSES[kind]
    comp.set_editor_property('WallMeshComponentName', 'StaticMesh')
    comp.set_editor_property('DamagedMaterials', [] if is_rubble_only else [unreal.load_asset(HOLES_MI)])
    comp.set_editor_property('DebrisMeshes', pieces)
    hole = comp.get_editor_property('HoleSettings')
    hole.set_editor_property('hole_radius_per_calibre_mm', c['radius'])
    hole.set_editor_property('max_hole_radius', float(c['max']))
    comp.set_editor_property('HoleSettings', hole)
    grid = comp.get_editor_property('GridSettings')
    grid.set_editor_property('grid_axis', unreal.WallHoleGridAxis.NO_GRID)
    grid.set_editor_property('hole_centre_outset_share', 0.0)
    grid.set_editor_property('min_hole_radius', float(c['min']))
    grid.set_editor_property('area_per_debris_piece', float(c['area']))
    grid.set_editor_property('max_pieces_per_hit', c['max_hit'])
    grid.set_editor_property('max_collapse_pieces', c['collapse'])
    comp.set_editor_property('GridSettings', grid)
    collapse = comp.get_editor_property('CollapseSettings')
    collapse.set_editor_property('collapse_into_chunks_on_death', True)
    collapse.set_editor_property('destroy_owner_delay_seconds', 4.5)
    collapse.set_editor_property('death_speed_scale', c['speed'])
    collapse.set_editor_property('collapse_sound', unreal.load_asset(COLLAPSE_CUE))
    collapse.set_editor_property('collapse_attenuation', unreal.load_asset(COLLAPSE_ATT))
    collapse.set_editor_property('collapse_concurrency', unreal.load_asset(COLLAPSE_CONC))
    comp.set_editor_property('CollapseSettings', collapse)


def setup_nav_box(comp, box):
    centre = unreal.Vector((box[0] + box[3]) * 0.5, (box[1] + box[4]) * 0.5, (box[2] + box[5]) * 0.5)
    extent = unreal.Vector((box[3] - box[0]) * 0.5, (box[4] - box[1]) * 0.5, (box[5] - box[2]) * 0.5)
    comp.set_editor_property('relative_location', centre)
    comp.set_editor_property('box_extent', extent)
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_editor_property('can_ever_affect_navigation', True)
    comp.set_editor_property('dynamic_obstacle', True)
    comp.set_editor_property('area_class_override', unreal.NavArea_Null)
    comp.set_editor_property('use_system_default_obstacle_area_class', False)
    # Invisible in the editor too: a thousand ruins would otherwise draw thousands of wire boxes. Navigation relevance of
    # a dynamic-obstacle shape does not depend on visibility.
    comp.set_editor_property('visible', False)
    comp.set_editor_property('hidden_in_game', True)


def build_one(mesh, pieces, base):
    suffix = mesh.get_name()[len('SM_BerlinRuin_'):]
    name = 'BP_BerlinRuin_' + suffix
    kind, health, reduction = _settings_for(suffix)
    slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    is_rubble_only = slots == ['M_BerlinRuin_Rubble']
    hulls = hull_boxes(mesh)
    boxes = nav_boxes(hulls)
    aims = aim_points(hulls, CLASSES[kind]['aims']) if hulls else [unreal.Vector(0, 0, 100)]
    bp, created = get_or_create_blueprint(name, base)
    named = ensure_components(bp, len(boxes))
    setup_mesh_template(named['StaticMesh'][1], mesh)
    setup_holes_template(named['WallHoleDamage'][1], kind, is_rubble_only, pieces)
    for index, box in enumerate(boxes):
        setup_nav_box(named['NavBlock_%02d' % index][1], box)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('health', float(health))
    cdo.set_editor_property('damage_reduction', float(reduction))
    cdo.set_editor_property('aim_offset_points', aims)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = EAL.save_loaded_asset(bp, False)
    info = {'blueprint': bp.get_path_name(), 'created': created, 'saved': bool(saved), 'class': kind,
            'health': health, 'damage_reduction': reduction, 'slots': slots, 'rubble_only': is_rubble_only,
            'hulls': len(hulls), 'nav_boxes': [[round(v) for v in b] for b in boxes],
            # Footprint the boxes block vs the ground hulls' own AABB footprints (m2): how much merging overstates.
            'nav_box_m2': round(sum(_area(b) for b in boxes) / 1e4, 1),
            'ground_hull_m2': round(sum(_area(h) for h in hulls if h[2] < NAV_MAX_BOTTOM) / 1e4, 1),
            'aims': [[a.x, a.y, a.z] for a in aims], 'settings': CLASSES[kind]}
    log(json.dumps(info))
    return info


def run(only=None):
    """only: optional list of mesh suffixes (e.g. ['E1_CornerTower']) to rebuild just those."""
    report = {'debris': ensure_debris_material(), 'blueprints': {}}
    pieces = debris_meshes()
    if not pieces:
        raise RuntimeError('no SM_BerlinRuin_Debris_* meshes in ' + DEBRIS)
    base = ensure_base()
    for mesh in kit_meshes():
        suffix = mesh.get_name()[len('SM_BerlinRuin_'):]
        if only and suffix not in only:
            continue
        try:
            report['blueprints'][suffix] = build_one(mesh, pieces, base)
        except Exception as error:  # noqa: BLE001 - report every piece, keep going
            report['blueprints'][suffix] = {'error': repr(error)}
            log(f'{suffix}: ERROR {error!r}')
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    log(f'done: {len(report["blueprints"])} blueprints, report {REPORT}')
    return report


if __name__ == '__main__':
    run()
