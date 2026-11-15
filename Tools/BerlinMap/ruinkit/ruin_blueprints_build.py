"""Berlin ruin kit: one destructible Blueprint per kit mesh, with procedural shell holes (2026-11-12).

Headless (never in the shared editor):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
      -DisablePlugins=Fab,BpGeneratorUltimate
or run() inside your own throwaway editor. Re-run it whenever kit meshes are added or changed: it picks up every
SM_BerlinRuin_* in RuinKit/Meshes and creates or updates BP_BerlinRuin_<suffix> in RuinKit/Blueprints. Blueprints are
never deleted, so actors already placed keep working.

Per Blueprint (parent: BP_BerlinRuin_Base, a child of the C++ ADestructableEnvActor that holds the Movable
DefaultSceneRoot and the kit's one death graph; the children have no graphs, headless Python cannot author them):
- StaticMesh (Movable, as the PostApo ruins): the kit mesh with its own slot materials; the collision that
  ADestructableEnvActor::SetupComponentCollisions would give it at BeginPlay is set on the template instead (object type
  Destructible, blocks Visibility / Projectile / TraceEnemy / TracePlayer, overlaps BuildingPlacement, ignores pawns,
  no decals) and it does not affect navigation.
- WallHoleDamage on StaticMesh: NoGrid, DamagedMaterials[0] = MI_BerlinRuin_Wall_Holes (slot 1 rubble never), kit debris
  SM_BerlinRuin_Debris_*, per-class hole size and debris counts (CLASSES). bCollapseIntoChunksOnDeath is false when the
  base has its death graph (the graph throws the pieces itself, then sinks the ruin); true otherwise (old chunk death).
- Death (2026-11-13): the graph lives in BP_BerlinRuin_Base only (authored through UECP, calls in
  ruin_death_graph_uecp.json; this builder never touches graphs). Per child this builder sets the class defaults the
  graph reads: sink depth / time / tilt, the dust effect with its scale, spread and pulse rate for the piece's size,
  the collapse cue, and the collapse heaps of footprint_table.json ("dead") as actor-relative transforms
  (DeathHeapsSmall / DeathHeapsLarge) with the moment they appear.
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
FOOTPRINT_TABLE = 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/footprint_table.json'
# ---------------------------------------------------------------- the death, one table
# Owner, 2026-11-14: no ruin vanishes. Every piece ends as a dead remnant that stays: lowered and turned. Small and
# medium pieces go down "a bit" (25-40 % of their height) with a clear yaw; large ones keep the deep sink (55-65 %).
# Dust and thrown debris are sized to the piece: a stub gets one low puff and a handful of stones, a hero the big event.
#
# The two dust effects (both one-shot; scale = component scale):
#   PUFF  NS_Artillery_StoneImpact: a brown dust plume without stones, about 6 m across at scale 1, gone after 1.5 s.
#   CLOUD NS_RTSDebrisExplosion_Huge: a ring of rolling smoke, about 7 m across at scale 1, 2.5 s, that also throws
#         about 25 large stones 10-15 m. The stones do NOT shrink with the scale, so it is for the large classes only.
# Both are tinted by the graph (DeathDustColor) to grey-brown masonry dust; untinted the cloud reads white on Ger_Berlin.
PUFF = '/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Stone/NS_Artillery_StoneImpact'
CLOUD = '/Game/RTS_Survival/ImpactSfxVfx/4SuperHeavyCalibre/Stone/NS_RTSDebrisExplosion_Huge'
DUST_COLOR = (0.30, 0.27, 0.23, 1.0)
# Death class per piece id (the part of the mesh suffix before the first underscore).
DEATH_CLASS_OF = {'E1': 'hero', 'E2': 'hero', 'E3': 'core', 'B7': 'core', 'B3': 'tall', 'B4': 'tall', 'B5': 'tall',
                  'B2': 'two_storey', 'E7': 'two_storey', 'B1': 'one_storey', 'E5': 'one_storey', 'E6': 'one_storey',
                  'B6': 'stub', 'E4': 'stub', 'B8': 'bed'}
# Per death class:
#   sink      share of the mesh height that goes into the ground
#   time      seconds the sink takes
#   tilt      (min, max) pitch/roll degrees     yaw  (min, max) degrees; the C++ picks size, direction and sign at random
#   fx        dust effect, fx_scale its scale, bursts how many (the first at the death, the rest spread over the sink
#             at random spots of the footprint), fx_speed the effect's "Velocity Speed Mlt" (PUFF: lower = lower, rounder puff)
#   heap_fx   scale of the dust on a heap spot just before the heap appears
#   debris    most kit debris pieces thrown off the wall (38-68 cm each), debris_speed how hard
#   heap      False: no collapse heap even if the footprint table has one (it would bury what still stands)
DEATH = {
    'hero':       dict(sink=0.62, time=6.0, tilt=(2.0, 4.0), yaw=(1.0, 3.0), fx=CLOUD, fx_scale=1.4, bursts=6, fx_speed=1.0,
                       heap_fx=1.2, debris=70, debris_speed=0.8, heap=True),
    'core':       dict(sink=0.58, time=5.0, tilt=(2.0, 4.0), yaw=(1.0, 3.0), fx=CLOUD, fx_scale=1.2, bursts=4, fx_speed=1.0,
                       heap_fx=1.1, debris=50, debris_speed=0.7, heap=True),
    'tall':       dict(sink=0.62, time=4.0, tilt=(2.0, 4.0), yaw=(2.0, 5.0), fx=PUFF, fx_scale=1.6, bursts=4, fx_speed=0.65,
                       heap_fx=1.3, debris=28, debris_speed=0.7, heap=True),
    'two_storey': dict(sink=0.35, time=2.2, tilt=(1.0, 3.0), yaw=(3.0, 7.0), fx=PUFF, fx_scale=1.3, bursts=3, fx_speed=0.6,
                       heap_fx=1.2, debris=16, debris_speed=0.6, heap=True),
    'one_storey': dict(sink=0.30, time=1.8, tilt=(1.0, 3.0), yaw=(3.0, 7.0), fx=PUFF, fx_scale=1.0, bursts=2, fx_speed=0.6,
                       heap_fx=1.1, debris=10, debris_speed=0.6, heap=True),
    'stub':       dict(sink=0.35, time=1.2, tilt=(1.0, 3.0), yaw=(3.0, 8.0), fx=PUFF, fx_scale=0.7, bursts=2, fx_speed=0.6,
                       heap_fx=0.8, debris=5, debris_speed=0.5, heap=False),
    'bed':        dict(sink=0.30, time=1.5, tilt=(0.0, 1.5), yaw=(2.0, 5.0), fx=PUFF, fx_scale=0.9, bursts=2, fx_speed=0.55,
                       heap_fx=0.8, debris=6, debris_speed=0.5, heap=False),
}
# Per piece exceptions to the class row (suffix -> changed keys).
#   E6 heap fins stand on their own modelled heap: a second heap would bury the fin.
#   E3 and B7 are a little shallower / deeper than their class so 7.3 m and 5.2 m stay standing.
DEATH_PIECE = {
    'E6_HeapFin_A': dict(heap=False), 'E6_HeapFin_B': dict(heap=False),
    'E2_Courtyard': dict(sink=0.60), 'E3_CoreRow': dict(sink=0.60), 'B7_WallCore': dict(sink=0.55),
    'B3_Facade4St': dict(sink=0.65), 'B4_Firewall': dict(sink=0.65), 'B5_CornerL': dict(sink=0.60),
}
# Heap spots that differ from footprint_table.json (suffix -> {index of the "dead" entry: (x, y)}).
#   B7 is one wall line; the table's heap at the centre lay astride the wall stump. It now lies in front of it.
HEAP_SPOT = {'B7_WallCore': {0: (400.0, 0.0)}}
HEAP_MESH_TO_ARRAY = {'SM_BerlinFoot_CollapseHeap_S': 'DeathHeapsSmall', 'SM_BerlinFoot_CollapseHeap_L': 'DeathHeapsLarge'}
# The variables the death graph of BP_BerlinRuin_Base reads. When one is missing the base has no death graph (or an
# older one): the children then keep the hole component's own chunk death and the report says so.
DEATH_VARIABLES = ('SinkOnDeath', 'LeaveRemnant', 'SinkDeltaZ', 'SinkTime', 'SinkMinTilt', 'SinkMaxTilt', 'SinkMinYaw',
                   'SinkMaxYaw', 'HeapDustDelay', 'HeapDustLead', 'DustPulseInterval', 'DustSpreadX', 'DustSpreadY',
                   'DeathVfxOffset', 'DeathDustScale', 'DeathHeapDustScale', 'DeathDustColor', 'DeathDustSpeed',
                   'DeathDustVfx', 'DeathSound', 'DeathHeapsSmall', 'DeathHeapsLarge')
# A NavBlock box stays (tag REMNANT_NAV_TAG; the death graph detaches it so it keeps standing at ground level while the
# actor sinks) when the hulls it was made from still stand at least this high after the sink; lower ones are destroyed.
REMNANT_NAV_TAG = 'RemnantNav'
REMNANT_NAV_MIN_STAND = 50.0
HEAP_AT = 0.30               # the heaps appear after this share of the sink time ...
HEAP_DUST_LEAD = 0.35        # ... and this many seconds after the dust on their spot
DUST_SPREAD_SHARE = 0.30     # later bursts are scattered this share of each ground side around the footprint centre


def death_row(suffix):
    """The death values of one piece: its class row with the piece's exceptions."""
    kind = DEATH_CLASS_OF.get(suffix.split('_')[0], 'one_storey')
    row = dict(DEATH[kind], kind=kind)
    row.update(DEATH_PIECE.get(suffix, {}))
    return row


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
    """BP_BerlinRuin_Base: the shared parent (C++ ADestructableEnvActor, a Movable DefaultSceneRoot). It holds the one
    death graph of the kit (BP_OnUnitDies -> StartRuinDeath -> DustAtHeaps -> SpawnDeathHeaps; see BerlinRuinKit.md and
    ruin_death_graph_uecp.json). The builder never touches its graphs; a base created here has none, and the children
    then keep the chunk death until the graph calls are replayed."""
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


def base_death_ready(base):
    """True when BP_BerlinRuin_Base carries every variable its death graph reads (see ruin_death_graph_uecp.json)."""
    cdo = unreal.get_default_object(base.generated_class())
    missing = []
    for name in DEATH_VARIABLES:
        try:
            cdo.get_editor_property(name)
        except Exception:  # noqa: BLE001 - a missing Blueprint variable raises
            missing.append(name)
    if missing:
        log(f'{BASE_NAME} has no death graph variables {missing}: children keep the chunk death')
    return not missing


def death_heaps(suffix, row):
    """({'DeathHeapsSmall': [Transform], 'DeathHeapsLarge': [Transform]}, list for the report) from the "dead" entries
    of the footprint table: actor-relative (x, y, 0), yaw added to the piece yaw, uniform scale. Pieces without an
    entry, and classes with heap False, leave no heap. HEAP_SPOT moves single heaps."""
    with open(FOOTPRINT_TABLE, 'r') as handle:
        table = json.load(handle)['pieces']
    out = {'DeathHeapsSmall': [], 'DeathHeapsLarge': []}
    listed = []
    if not row['heap']:
        return out, listed
    for index, entry in enumerate(table.get(suffix, {}).get('dead', [])):
        scale = float(entry.get('scale', 1.0))
        x, y = HEAP_SPOT.get(suffix, {}).get(index, (entry['x'], entry['y']))
        transform = unreal.Transform(unreal.Vector(x, y, 0.0), unreal.Rotator(roll=0.0, pitch=0.0, yaw=entry['yaw']),
                                     unreal.Vector(scale, scale, scale))
        out[HEAP_MESH_TO_ARRAY[entry['mesh']]].append(transform)
        listed.append([entry['mesh'].rsplit('_', 1)[1], x, y, entry['yaw'], scale])
    return out, listed


def death_defaults(suffix, mesh):
    """Class defaults the death graph reads, from the mesh size, the DEATH table and the footprint table."""
    d = death_row(suffix)
    box = mesh.get_bounding_box()
    size_x, size_y = box.max.x - box.min.x, box.max.y - box.min.y
    heaps, listed = death_heaps(suffix, d)
    sink = round(box.max.z * d['sink'])
    # Bursts: the first at the death, the others on a looping timer that the end of the sink stops.
    pulse = round(d['time'] / (d['bursts'] - 0.5), 2) if d['bursts'] > 1 else round(d['time'] * 3.0, 2)
    heap_dust = round(d['heap_fx'] * max([h[4] for h in listed] or [1.0]), 2)
    heap_time = d['time'] * HEAP_AT
    values = {
        'SinkOnDeath': True,
        'LeaveRemnant': True,
        'SinkDeltaZ': float(sink),
        'SinkTime': d['time'],
        'SinkMinTilt': d['tilt'][0], 'SinkMaxTilt': d['tilt'][1], 'SinkMinYaw': d['yaw'][0], 'SinkMaxYaw': d['yaw'][1],
        'HeapDustDelay': round(max(heap_time - HEAP_DUST_LEAD, 0.1), 2),
        'HeapDustLead': HEAP_DUST_LEAD,
        'DustPulseInterval': pulse,
        'DustSpreadX': round(size_x * DUST_SPREAD_SHARE), 'DustSpreadY': round(size_y * DUST_SPREAD_SHARE),
        # The dust is centred on the middle of the mesh footprint, 60 cm up (local space; the graph turns it with the actor).
        'DeathVfxOffset': unreal.Vector(round((box.min.x + box.max.x) * 0.5), round((box.min.y + box.max.y) * 0.5), 60.0),
        'DeathDustScale': unreal.Vector(d['fx_scale'], d['fx_scale'], d['fx_scale']),
        'DeathHeapDustScale': unreal.Vector(heap_dust, heap_dust, heap_dust),
        'DeathDustColor': unreal.LinearColor(*DUST_COLOR),
        'DeathDustSpeed': d['fx_speed'],
        'DeathDustVfx': unreal.load_asset(d['fx']),
        'DeathSound': unreal.load_asset(COLLAPSE_CUE),
        'DeathHeapsSmall': heaps['DeathHeapsSmall'],
        'DeathHeapsLarge': heaps['DeathHeapsLarge'],
    }
    info = {'class': d['kind'], 'top_cm': round(box.max.z), 'sink_share': d['sink'], 'sink_cm': sink,
            'stands_after_cm': round(box.max.z - sink), 'sink_s': d['time'], 'tilt_deg': list(d['tilt']), 'yaw_deg': list(d['yaw']),
            'fx': d['fx'].rsplit('/', 1)[1], 'fx_scale': d['fx_scale'], 'bursts': d['bursts'], 'pulse_s': pulse,
            'heap_dust_scale': heap_dust, 'heap_spawn_s': round(values['HeapDustDelay'] + HEAP_DUST_LEAD, 2), 'heaps': listed,
            'debris': d['debris'], 'footprint_cm': [round(size_x), round(size_y)],
            'dust_spread': [values['DustSpreadX'], values['DustSpreadY']]}
    return values, info


def setup_holes_template(comp, kind, is_rubble_only, pieces, graph_death=False, suffix=''):
    c = CLASSES[kind]
    # With the death graph the debris of the death comes from the DEATH table (sized to the piece), not from CLASSES.
    row = death_row(suffix) if graph_death else None
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
    grid.set_editor_property('max_collapse_pieces', row['debris'] if row else c['collapse'])
    comp.set_editor_property('GridSettings', grid)
    collapse = comp.get_editor_property('CollapseSettings')
    # With the death graph in the base, the graph calls CollapseIntoPieces(false) and sinks the ruin; the component's
    # own death would hide the mesh at once and the sink would never show.
    collapse.set_editor_property('collapse_into_chunks_on_death', not graph_death)
    collapse.set_editor_property('destroy_owner_delay_seconds', 4.5)
    collapse.set_editor_property('death_speed_scale', row['debris_speed'] if row else c['speed'])
    collapse.set_editor_property('collapse_sound', unreal.load_asset(COLLAPSE_CUE))
    collapse.set_editor_property('collapse_attenuation', unreal.load_asset(COLLAPSE_ATT))
    collapse.set_editor_property('collapse_concurrency', unreal.load_asset(COLLAPSE_CONC))
    comp.set_editor_property('CollapseSettings', collapse)


def remnant_nav_flags(suffix, mesh, hulls, boxes):
    """Per nav box: True when it must stay under the remnant (some hull inside it still stands REMNANT_NAV_MIN_STAND
    above the ground after the sink)."""
    sink = mesh.get_bounding_box().max.z * death_row(suffix)['sink']
    flags = []
    for b in boxes:
        tops = [h[5] for h in hulls if h[2] < NAV_MAX_BOTTOM and h[0] >= b[0] - 1 and h[1] >= b[1] - 1
                and h[3] <= b[3] + 1 and h[4] <= b[4] + 1]
        flags.append(bool(tops) and max(tops) - sink >= REMNANT_NAV_MIN_STAND)
    return flags


def setup_nav_box(comp, box, keep_under_remnant=False):
    comp.set_editor_property('component_tags', [REMNANT_NAV_TAG] if keep_under_remnant else [])
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


def build_one(mesh, pieces, base, graph_death=False):
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
    setup_holes_template(named['WallHoleDamage'][1], kind, is_rubble_only, pieces, graph_death, suffix)
    keep = remnant_nav_flags(suffix, mesh, hulls, boxes)
    for index, box in enumerate(boxes):
        setup_nav_box(named['NavBlock_%02d' % index][1], box, keep[index] and graph_death)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('health', float(health))
    cdo.set_editor_property('damage_reduction', float(reduction))
    cdo.set_editor_property('aim_offset_points', aims)
    death_info = {'graph': False}
    if graph_death:
        values, death_info = death_defaults(suffix, mesh)
        for key, value in values.items():
            cdo.set_editor_property(key, value)
        death_info['graph'] = True
        death_info['nav_boxes_kept_under_remnant'] = '%d/%d' % (sum(keep), len(keep))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = EAL.save_loaded_asset(bp, False)
    info = {'blueprint': bp.get_path_name(), 'created': created, 'saved': bool(saved), 'class': kind,
            'health': health, 'damage_reduction': reduction, 'slots': slots, 'rubble_only': is_rubble_only,
            'hulls': len(hulls), 'nav_boxes': [[round(v) for v in b] for b in boxes],
            # Footprint the boxes block vs the ground hulls' own AABB footprints (m2): how much merging overstates.
            'nav_box_m2': round(sum(_area(b) for b in boxes) / 1e4, 1),
            'ground_hull_m2': round(sum(_area(h) for h in hulls if h[2] < NAV_MAX_BOTTOM) / 1e4, 1),
            'aims': [[a.x, a.y, a.z] for a in aims], 'settings': CLASSES[kind], 'death': death_info}
    log(json.dumps(info))
    return info


def run(only=None):
    """only: optional list of mesh suffixes (e.g. ['E1_CornerTower']) to rebuild just those."""
    report = {'debris': ensure_debris_material(), 'blueprints': {}}
    pieces = debris_meshes()
    if not pieces:
        raise RuntimeError('no SM_BerlinRuin_Debris_* meshes in ' + DEBRIS)
    base = ensure_base()
    graph_death = base_death_ready(base)
    report['death_graph_in_base'] = graph_death
    if not graph_death:
        report['errors'] = [f'{BASE_NAME} lacks the death graph variables: every piece keeps the chunk death '
                            f'(replay ruin_death_graph_uecp.json, then re-run)']
    for mesh in kit_meshes():
        suffix = mesh.get_name()[len('SM_BerlinRuin_'):]
        if only and suffix not in only:
            continue
        try:
            report['blueprints'][suffix] = build_one(mesh, pieces, base, graph_death)
        except Exception as error:  # noqa: BLE001 - report every piece, keep going
            report['blueprints'][suffix] = {'error': repr(error)}
            log(f'{suffix}: ERROR {error!r}')
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    log(f'done: {len(report["blueprints"])} blueprints, report {REPORT}')
    return report


if __name__ == '__main__':
    run()
