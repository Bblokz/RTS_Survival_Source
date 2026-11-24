"""Desert rock kit: the base and the 16 rock Blueprints, headless (run ue_import.py first).

  bash run_ue.sh ue_blueprints.py bps -nullrhi

Idempotent; never deletes a Blueprint. Report: Saved/DesertRocks/blueprints_report.json.

/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/
  BP_DesertRock_Base   a DUPLICATE of BP_OreCrushRock_Base (parent BP_NaturalCover -> C++ ADestructableActorHealth),
                       made once; it keeps that Blueprint's graph (crush + death) and its WallHoleDamage / NavBlock
                       components. ADestructableActorHealth::TakeDamage forwards every hit to WallHoleDamage.
  BP_DesertRock_<L|M><N>_<Kind>   graph-less children: mesh, health, holes (DamagedMaterials = MI_DesertRock_WallHoles,
                       NoGrid), nav box, aim points.
Medium rocks are crushable by medium and heavy tanks (MediumOrHeavyTank; nav box RTS_NavArea_BlockCharLight, so
infantry and light vehicles path round): a crush sinks the rock (VerticalDestruction).
Large rocks are not crushable (nav box NavArea_Null: nobody paths through).
Death by damage on both (WallHoleDamage.CollapseIntoPieces: the rock bursts into SM_DesertRock_Debris_* chunks).
"""
import json
import math
import os

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks'
BLUEPRINTS = HOME + '/Blueprints'
MESH = HOME + '/Meshes'
SRC_BASE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Blueprints/BP_OreCrushRock_Base'
ORE_DEBRIS = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Meshes/SM_OreRock_Debris_%d'
HOLES_MI = HOME + '/Materials/MI_DesertRock_WallHoles'
BASE_NAME = 'BP_DesertRock_Base'
ROCKS_JSON = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/DesertRockKit/Model/out/rocks.json'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/DesertRocks/blueprints_report.json'
NAV_AREAS = '/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/'
DEATH_CUE = '/Game/RTS_Survival/Sounds/DestructionAndCollapse/CollapseSmallStoneObj/Cue_SmallStoneObj_Collapse'
COLLAPSE_ATT = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
COLLAPSE_CONC = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'

# hole radius: cm per mm of calibre, capped so a hole never opens through the rock (medium rocks are 1-1.8 m thick)
RULES = {
    # medium rocks: crushed by medium and heavy tanks (user 2026-11-15); infantry and light vehicles path round
    'medium': dict(crush='MEDIUM_OR_HEAVY_TANK', area='RTS_NavArea_BlockCharLight', sink_time=2.6,
                   health=2500.0, reduction=20.0, radius=0.8, max_hole=70.0, min_hole=12.0, area_per_piece=3000.0,
                   max_hit=3, collapse_pieces=30, spacing=45.0, aims=1),
    'large': dict(crush=None, area=None, health=6000.0, reduction=25.0, radius=1.0, max_hole=110.0, min_hole=15.0, area_per_piece=4000.0,
                  max_hit=4, collapse_pieces=80, spacing=70.0, aims=3),
}
TIP_SPEED = 0.1          # a slow lean (about 11 degrees in 3 s) while the rock sinks, as the crushable pipes / cover
NAV_BOX_MIN_TOP = 80.0
NAV_BOX_PAD = 5.0        # cm around the hull footprint

EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
errors = []


def log(message):
    unreal.log_warning('DROCK_BP ' + message)


# ---------------------------------------------------------------- footprint (min-area rectangle of the hulls)
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
    _, cx, cy, hu, hv, ang = best
    while ang > math.pi / 4 + 1e-9:
        ang -= math.pi / 2
        hu, hv = hv, hu
    while ang <= -math.pi / 4 + 1e-9:
        ang += math.pi / 2
        hu, hv = hv, hu
    return cx, cy, hu, hv, ang


def nav_box(info):
    """One box over all hull footprints (cm, mesh space). The Blender pivot shift is already in hull_pts."""
    pts = [(p[0] * 100.0, p[1] * 100.0, p[2] * 100.0) for hull in info['hull_pts'] for p in hull]
    cx, cy, hu, hv, ang = min_area_rect(pts)
    top = max(p[2] for p in pts)
    return dict(cx=cx, cy=cy, hu=hu + NAV_BOX_PAD, hv=hv + NAV_BOX_PAD, yaw=math.degrees(ang), z0=0.0,
                z1=max(top, NAV_BOX_MIN_TOP)), top


# ---------------------------------------------------------------- subobjects
def named(bp):
    out = {}
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
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


def own(bp, comps, name):
    return SDL.get_object_for_blueprint(comps[name][1], bp)


def add_component(bp, parent_handle, cls, name):
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError(f'{bp.get_name()}: adding {name} failed: {fail}')
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle


def nav_area_class(name=None):
    """None: NavArea_Null (nobody paths through); else an RTS nav area that only some agents path round."""
    if name is None:
        return unreal.NavArea_Null.static_class()
    cls = unreal.load_class(None, f'{NAV_AREAS}{name}.{name}_C')
    if cls is None:
        raise RuntimeError('nav area missing: ' + name)
    return cls


# ---------------------------------------------------------------- templates
def setup_mesh_template(comp, mesh):
    if mesh is not None:
        comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('override_materials', [])
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('receives_decals', True)
    comp.set_editor_property('generate_overlap_events', True)
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_collision_object_type(unreal.CollisionChannel.ECC_DESTRUCTIBLE)
    comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    ch = unreal.CollisionChannel
    for overlapped in (ch.ECC_BUILDING_PLACEMENT, ch.ECC_PLAYER_OBJ, ch.ECC_ENEMY_OBJ):
        comp.set_collision_response_to_channel(overlapped, unreal.CollisionResponseType.ECR_OVERLAP)
    for blocked in (ch.ECC_VISIBILITY, ch.ECC_PROJECTILE, ch.ECC_ENEMY, ch.ECC_PLAYER):
        comp.set_collision_response_to_channel(blocked, unreal.CollisionResponseType.ECR_BLOCK)


def setup_nav_box(comp, box, area_class):
    comp.set_editor_property('relative_location', unreal.Vector(box['cx'], box['cy'], (box['z0'] + box['z1']) * 0.5))
    comp.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=box['yaw']))
    comp.set_editor_property('box_extent', unreal.Vector(box['hu'], box['hv'], (box['z1'] - box['z0']) * 0.5))
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property('generate_overlap_events', False)
    comp.set_editor_property('can_ever_affect_navigation', True)
    comp.set_editor_property('dynamic_obstacle', True)
    comp.set_editor_property('area_class_override', area_class)
    comp.set_editor_property('use_system_default_obstacle_area_class', False)
    comp.set_editor_property('visible', False)
    comp.set_editor_property('hidden_in_game', True)


def setup_health_template(comp, health, reduction):
    comp.set_editor_property('MaxHealth', float(health))
    comp.set_editor_property('CurrentHealth', float(health))
    red = comp.get_editor_property('DamageReductionSettings')
    red.set_editor_property('DamageReduction', float(reduction))
    comp.set_editor_property('DamageReductionSettings', red)


def setup_holes_template(comp, debris, rule):
    """Procedural shell holes on the rock (NoGrid, two-sided masked instance); a death by damage bursts the rock into
    debris (called by the base graph, so collapse_into_chunks_on_death off)."""
    comp.set_editor_property('WallMeshComponentName', 'MainMesh')
    comp.set_editor_property('DamagedMaterials', [unreal.load_asset(HOLES_MI)])
    comp.set_editor_property('DebrisMeshes', debris)
    hole = comp.get_editor_property('HoleSettings')
    hole.set_editor_property('hole_radius_per_calibre_mm', rule['radius'])
    hole.set_editor_property('max_hole_radius', rule['max_hole'])
    comp.set_editor_property('HoleSettings', hole)
    grid = comp.get_editor_property('GridSettings')
    grid.set_editor_property('grid_axis', unreal.WallHoleGridAxis.NO_GRID)
    grid.set_editor_property('hole_centre_outset_share', 0.0)
    grid.set_editor_property('min_hole_radius', rule['min_hole'])
    grid.set_editor_property('area_per_debris_piece', rule['area_per_piece'])
    grid.set_editor_property('max_pieces_per_hit', rule['max_hit'])
    grid.set_editor_property('max_collapse_pieces', rule['collapse_pieces'])
    grid.set_editor_property('collapse_piece_spacing', rule['spacing'])
    comp.set_editor_property('GridSettings', grid)
    collapse = comp.get_editor_property('CollapseSettings')
    collapse.set_editor_property('collapse_into_chunks_on_death', False)
    collapse.set_editor_property('destroy_owner_delay_seconds', 3.0)
    collapse.set_editor_property('death_speed_scale', 0.8)
    collapse.set_editor_property('collapse_sound', unreal.load_asset(DEATH_CUE))
    collapse.set_editor_property('collapse_attenuation', unreal.load_asset(COLLAPSE_ATT))
    collapse.set_editor_property('collapse_concurrency', unreal.load_asset(COLLAPSE_CONC))
    comp.set_editor_property('CollapseSettings', collapse)


def save(bp):
    if not EAL.save_loaded_asset(bp, False):
        errors.append('save failed ' + bp.get_path_name())
        return False
    return True


# ---------------------------------------------------------------- base and children
def ensure_base():
    path = f'{BLUEPRINTS}/{BASE_NAME}'
    created = False
    if not EAL.does_asset_exist(path):
        if not EAL.does_asset_exist(SRC_BASE):
            raise RuntimeError(SRC_BASE + ' missing: the crush graph to copy is gone (BerlinRubbleCover.md)')
        if not EAL.duplicate_asset(SRC_BASE, path):
            raise RuntimeError('duplicate failed ' + path)
        created = True
    base = unreal.load_asset(path)
    cdo = unreal.get_default_object(base.generated_class())
    for prop in ('Crushable', 'CrushDestructionType', 'CrushTipSpeedScale', 'CrushSinkDeltaZ', 'CrushSinkTime',
                 'Overlap Tanks'):
        cdo.get_editor_property(prop)       # raises if the copied graph lost its variables
    comps = named(base)
    for need in ('MainMesh', 'HealthComp', '__root__', '__actor__'):
        if need not in comps:
            raise RuntimeError(f'{BASE_NAME} lacks {need}: {list(comps)}')
    if 'WallHoleDamage' not in comps:
        add_component(base, comps['__actor__'][0], unreal.WallHoleDamageComponent, 'WallHoleDamage')
    if 'NavBlock' not in comps:
        add_component(base, comps['__root__'][0], unreal.BoxComponent, 'NavBlock')
    comps = named(base)
    setup_nav_box(SDL.get_object(comps['NavBlock'][1]),
                  dict(cx=0.0, cy=0.0, hu=50.0, hv=50.0, yaw=0.0, z0=0.0, z1=100.0),
                  nav_area_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(base)
    cdo = unreal.get_default_object(base.generated_class())
    cdo.set_editor_property('Crushable', False)          # the base itself is never placed
    unreal.BlueprintEditorLibrary.compile_blueprint(base)
    save(base)
    return base, created


def get_or_create(name, base):
    path = f'{BLUEPRINTS}/{name}'
    if EAL.does_asset_exist(path):
        bp = unreal.load_asset(path)
        if not unreal.MathLibrary.class_is_child_of(bp.generated_class(), base.generated_class()):
            raise RuntimeError(f'{name} exists but is not a child of {BASE_NAME}')
        return bp, False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', base.generated_class())
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, BLUEPRINTS, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError('could not create ' + path)
    return bp, True


def aim_points(mesh, count):
    b = mesh.get_bounding_box()
    z = round(max(b.max.z * 0.5, 30.0))
    pts = [unreal.Vector(0.0, 0.0, z)]
    if count > 1:
        dx = round((b.max.x - b.min.x) * 0.25)
        pts += [unreal.Vector(dx, 0.0, round(z * 0.8)), unreal.Vector(-dx, 0.0, round(z * 0.8))]
    return pts


def build_one(rock, info, base, debris):
    rule = RULES[info['class']]
    mesh = unreal.load_asset(f'{MESH}/SM_DesertRock_{rock}')
    if mesh is None:
        raise RuntimeError('mesh missing SM_DesertRock_' + rock)
    bp, created = get_or_create('BP_DesertRock_' + rock, base)
    comps = named(bp)
    box, top_cm = nav_box(info)
    setup_mesh_template(own(bp, comps, 'MainMesh'), mesh)
    setup_health_template(own(bp, comps, 'HealthComp'), rule['health'], rule['reduction'])
    setup_holes_template(own(bp, comps, 'WallHoleDamage'), debris, rule)
    setup_nav_box(own(bp, comps, 'NavBlock'), box, nav_area_class(rule['area']))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    crush = rule['crush'] is not None
    cdo.set_editor_property('Crushable', crush)
    cdo.set_editor_property('Overlap Tanks', crush)
    if crush:
        b = mesh.get_bounding_box()
        half = max(b.max.x - b.min.x, b.max.y - b.min.y) * 0.5
        cdo.set_editor_property('CrushDestructionType', getattr(unreal.CrushDestructionType, rule['crush']))
        cdo.set_editor_property('CrushTipSpeedScale', TIP_SPEED)
        # its height plus what the lean lifts, so nothing sticks out once sunk
        cdo.set_editor_property('CrushSinkDeltaZ', float(round(b.max.z + 0.2 * half + 20.0)))
        cdo.set_editor_property('CrushSinkTime', rule['sink_time'])
    cdo.set_editor_property('health', rule['health'])
    cdo.set_editor_property('aim_offset_points', aim_points(mesh, rule['aims']))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = save(bp)
    return dict(blueprint=bp.get_path_name(), created=created, saved=saved, size_class=info['class'],
                crush=rule['crush'], nav_area=rule['area'] or 'NavArea_Null', health=rule['health'], damage_reduction=rule['reduction'], hole=[rule['radius'], rule['max_hole']],
                top_cm=round(top_cm),
                nav_box=[round(box['cx']), round(box['cy']), round(box['hu']), round(box['hv']), round(box['yaw'], 1),
                         round(box['z1'])])


def main():
    rocks = json.load(open(ROCKS_JSON))
    report = {'rocks': {}, 'errors': errors}
    base, created = ensure_base()
    report['base'] = dict(path=base.get_path_name(), duplicated_now=created)
    debris = []
    for i in range(6):
        m = unreal.load_asset(f'{MESH}/SM_DesertRock_Debris_{i}')
        if m is None:
            # prototype runs (not every rock built yet): borrow the Ore chunks
            m = unreal.load_asset(ORE_DEBRIS % i)
            report.setdefault('borrowed_debris', []).append(i)
        debris.append(m)
    for rock, info in sorted(rocks.items(), key=lambda kv: kv[1]['index']):
        try:
            row = build_one(rock, info, base, debris)
            report['rocks'][rock] = row
            log(rock + ' ' + json.dumps(row))
        except Exception as error:  # noqa: BLE001
            errors.append(f'{rock}: {error!r}')
            log(f'{rock}: ERROR {error!r}')
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1)
    log(f'done: {len(report["rocks"])} Blueprints, errors {errors}')


main()
