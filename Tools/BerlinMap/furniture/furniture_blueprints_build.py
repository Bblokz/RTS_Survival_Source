"""Berlin apartment furniture: the 32 crushable Blueprints (run furniture_import.py first).

Headless (never in the shared editor):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
      -DisablePlugins=Fab,BpGeneratorUltimate
Add -FurnDryRun to do everything in memory and save nothing (works while another editor holds the Blueprints).
Incremental and idempotent (since 2026-11-14): a Blueprint whose settings already equal what this script would set
is not compiled or saved, so a second run changes nothing and pieces 17-32 can be added while an editor holds the
base and the first 16. Add -FurnForce to re-apply and save every Blueprint, the base included (needs them free).
Pieces 17-32 are built only when the modeller's READY2.txt exists.
Never deletes a Blueprint. Report: Saved/Berlin/furniture/build_report.json.
Doc: Source/Docs/Environment/BerlinFurniture.md.

/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Blueprints/
  BP_BerlinFurniture_Base            parent C++ ADestructableEnvActor. Components: MainMesh, NavBlock (box),
                                     WallHoleDamage. It carries the only graph, authored once through a UECP editor
                                     (headless Python cannot write graphs; this script never touches it):
      BeginPlay     -> SetupCrush: SetupComponentCollisions([MainMesh], [], OverlapTanks true)
                       -> ConfigureVerticalCollapse(MainMesh, CrushTipSpeedScale, CrushSound, Att_Collapse, SC_Collapse)
                       -> SetupCrushDestructionOverlap(MainMesh, CrushDestructionType, VerticalCollapse)
      BP_OnUnitDies -> HandleDeath: Scavenging (= crushed) -> VerticalDestruction(CrushSinkDeltaZ, CrushSinkTime,
                       destroy); every other death -> WallHoleDamage.CollapseIntoPieces(hide and destroy)
  BP_BerlinFurn_<NN>_<Name>          graph-less children: mesh, health, nav box, sink depth, crush sound.
If the base is missing or lacks the crush variables the script stops: re-author the graph first (doc, "Rebuild").

What this script sets:
- MainMesh: the piece's mesh, Movable, the collision SetupComponentCollisions gives it at BeginPlay anyway
  (Destructible; blocks Visibility / Projectile / weapon traces; overlaps the vehicle object channels; no nav).
- NavBlock: hidden dynamic-obstacle box over the body footprint (the UCX hulls) with RTS_NavArea_BlockChar: only the
  Characters query filter excludes that area and it is not built into the tank navmeshes, so infantry walk around a
  piece and every tank class paths straight through it and crushes it (CrushDestructionType AnyTank).
- WallHoleDamage (on the base): NoGrid, DamagedMaterials[0] = MI_BerlinFurniture_Holes, the furniture debris, small
  holes; collapse_into_chunks_on_death off (the graph decides the death).
- Class defaults: Health / DamageReduction (ADestructableEnvActor fields), aim point, tip speed and sink depth.
- Sound: the variable CrushSound (what a crush plays) and WallHoleDamage.CollapseSettings.CollapseSound (what a death
  by damage plays) both get the piece's cue from sounds/furniture_sound_table.py; the base gets the small-wood cue.
  The cues are made by sounds/furniture_sounds_import.py (run that first).
"""
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', 'ruinkit')))
import cover_build as cb  # noqa: E402  (hull_points, subobject helpers, nav box and nav area helpers)
sys.path.insert(0, os.path.join(HERE, 'sounds'))
import furniture_sound_table as sounds  # noqa: E402  (which piece plays which crush cue)

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinFurniture/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture'
BLUEPRINTS = ROOT + '/Blueprints'
MESHES = ROOT + '/Meshes'
HOLES_MI = ROOT + '/Materials/MI_BerlinFurniture_Holes'
BASE_NAME = 'BP_BerlinFurniture_Base'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/build_report.json'
COLLAPSE_ATT = sounds.ATTENUATION
COLLAPSE_CONC = sounds.CONCURRENCY
DRY_RUN = '-furndryrun' in unreal.SystemLibrary.get_command_line().lower()
FORCE = '-furnforce' in unreal.SystemLibrary.get_command_line().lower()
SECOND_BATCH_READY = 'READY2.txt'             # written last by the modeller for pieces 17-32
SECOND_BATCH_FIRST = 17

CRUSH_RULE = 'ANY_TANK'                      # light tanks and above
NAV_AREA = cb.CRUSH_NAV_AREA[CRUSH_RULE]     # RTS_NavArea_BlockChar
# Health: the table's suggested_health is a relative guess (chair 30 .. tiled stove 350); scaled into 150-600.
HEALTH_SCALE = 1.7
HEALTH_MIN = 150
DAMAGE_REDUCTION = 4.0
# Holes on furniture: a 75 mm hit bites about 22 cm, never more than 40.
# spacing: one death piece per 35 x 35 cm of face (the component default of 110 throws 1-2 from a piece this small).
HOLES = dict(radius=0.3, max=40.0, min=6.0, area=900.0, max_hit=3, collapse=14, spacing=35.0, speed=0.8,
             destroy_delay=3.0)
# Crushed look. Upright pieces (taller than 1.2 x their shorter footprint side) fall over in 1.7 s; low or already
# fallen ones only lean (as the rubble cover does) while they sink.
TIP_SPEED_UPRIGHT = 0.5
TIP_SPEED_LOW = 0.12
SINK_TIME = 2.4
NAV_BOX_MIN_TOP = 80.0

EAL = unreal.EditorAssetLibrary
SDS = cb.SDS
SDL = cb.SDL


def log(message):
    unreal.log_warning('BERLIN_FURN_BP ' + message)


def health_for(piece):
    return float(max(HEALTH_MIN, int(round(piece['suggested_health'] * HEALTH_SCALE / 10.0)) * 10))


def load_base():
    path = f'{BLUEPRINTS}/{BASE_NAME}'
    if not EAL.does_asset_exist(path):
        raise RuntimeError(path + ' missing: author it first (BerlinFurniture.md, "Rebuild")')
    base = unreal.load_asset(path)
    cdo = unreal.get_default_object(base.generated_class())
    for prop in ('CrushDestructionType', 'CrushTipSpeedScale', 'CrushSinkDeltaZ', 'CrushSinkTime', 'CrushSound'):
        try:
            cdo.get_editor_property(prop)
        except Exception:  # noqa: BLE001
            raise RuntimeError(f'{BASE_NAME} lacks the variable {prop}: its graph is not authored')
    named = cb._named(base)
    for comp in ('MainMesh', 'NavBlock', 'WallHoleDamage'):
        if comp not in named:
            raise RuntimeError(f'{BASE_NAME} lacks the component {comp}')
    return base


def setup_mesh_template(comp, mesh):
    if mesh is not None:
        comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('override_materials', [])
    comp.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('receives_decals', False)
    comp.set_editor_property('generate_overlap_events', True)
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_collision_object_type(unreal.CollisionChannel.ECC_DESTRUCTIBLE)
    comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    ch = unreal.CollisionChannel
    for overlapped in (ch.ECC_BUILDING_PLACEMENT, ch.ECC_PLAYER_OBJ, ch.ECC_ENEMY_OBJ):
        comp.set_collision_response_to_channel(overlapped, unreal.CollisionResponseType.ECR_OVERLAP)
    for blocked in (ch.ECC_VISIBILITY, ch.ECC_PROJECTILE, ch.ECC_ENEMY, ch.ECC_PLAYER):
        comp.set_collision_response_to_channel(blocked, unreal.CollisionResponseType.ECR_BLOCK)


def load_cue(key):
    cue = unreal.load_asset(sounds.cue_path(key))
    if cue is None:
        raise RuntimeError(sounds.cue_path(key) + ' missing: run sounds/furniture_sounds_import.py')
    return cue


def save(blueprint):
    return False if DRY_RUN else bool(EAL.save_loaded_asset(blueprint, False))


def set_death_sound(comp, cue):
    """What WallHoleDamage.CollapseIntoPieces plays (a death by damage): the piece's crush cue."""
    collapse = comp.get_editor_property('CollapseSettings')
    collapse.set_editor_property('collapse_sound', cue)
    comp.set_editor_property('CollapseSettings', collapse)


def setup_holes_template(comp, debris):
    c = HOLES
    comp.set_editor_property('WallMeshComponentName', 'MainMesh')
    comp.set_editor_property('DamagedMaterials', [unreal.load_asset(HOLES_MI)])
    comp.set_editor_property('DebrisMeshes', debris)
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
    grid.set_editor_property('collapse_piece_spacing', c['spacing'])
    comp.set_editor_property('GridSettings', grid)
    collapse = comp.get_editor_property('CollapseSettings')
    # Off: BP_BerlinFurniture_Base.HandleDeath calls CollapseIntoPieces for every death except a crush.
    collapse.set_editor_property('collapse_into_chunks_on_death', False)
    collapse.set_editor_property('destroy_owner_delay_seconds', c['destroy_delay'])
    collapse.set_editor_property('death_speed_scale', c['speed'])
    collapse.set_editor_property('collapse_sound', load_cue(sounds.DEFAULT_CUE))
    collapse.set_editor_property('collapse_attenuation', unreal.load_asset(COLLAPSE_ATT))
    collapse.set_editor_property('collapse_concurrency', unreal.load_asset(COLLAPSE_CONC))
    comp.set_editor_property('CollapseSettings', collapse)


def footprint_box(hulls):
    """One axis-aligned box over all UCX hulls (the body; loose debris beside a piece has no hull)."""
    pts = [p for hull in hulls for p in hull]
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    return dict(cx=(lo[0] + hi[0]) / 2, cy=(lo[1] + hi[1]) / 2, hu=(hi[0] - lo[0]) / 2, hv=(hi[1] - lo[1]) / 2,
                yaw=0.0, z0=0.0, z1=max(hi[2], NAV_BOX_MIN_TOP)), lo, hi


def debris_meshes():
    out = []
    for index in range(6):
        mesh = unreal.load_asset(f'{MESHES}/SM_BerlinFurn_Debris_{index}')
        if mesh is None:
            raise RuntimeError('furniture debris missing: run furniture_import.py')
        out.append(mesh)
    return out


def base_is_set_up(base):
    """The base already carries what setup_base writes (read-only check of the values children depend on)."""
    named = cb._named(base)
    holes = SDL.get_object(named['WallHoleDamage'][1])
    damaged = list(holes.get_editor_property('DamagedMaterials'))
    box = SDL.get_object(named['NavBlock'][1])
    cdo = unreal.get_default_object(base.generated_class())
    sound = cdo.get_editor_property('CrushSound')
    area = box.get_editor_property('area_class_override')
    return bool(len(damaged) == 1 and damaged[0] is not None and damaged[0].get_name() == HOLES_MI.split('/')[-1]
                and len(list(holes.get_editor_property('DebrisMeshes'))) == 6
                and str(holes.get_editor_property('WallMeshComponentName')) == 'MainMesh'
                and sound is not None and sound.get_name() == sounds.cue_name(sounds.DEFAULT_CUE)
                and area is not None and area.get_name() == NAV_AREA + '_C')


def setup_base(base, debris, area):
    named = cb._named(base)
    setup_mesh_template(SDL.get_object(named['MainMesh'][1]), None)
    setup_holes_template(SDL.get_object(named['WallHoleDamage'][1]), debris)
    cb.setup_nav_box(SDL.get_object(named['NavBlock'][1]),
                     dict(cx=0.0, cy=0.0, hu=50.0, hv=50.0, yaw=0.0, z0=0.0, z1=100.0), area)
    unreal.BlueprintEditorLibrary.compile_blueprint(base)
    cdo = unreal.get_default_object(base.generated_class())
    cdo.set_editor_property('CrushDestructionType', getattr(unreal.CrushDestructionType, CRUSH_RULE))
    cdo.set_editor_property('damage_reduction', DAMAGE_REDUCTION)
    cdo.set_editor_property('CrushSound', load_cue(sounds.DEFAULT_CUE))
    unreal.BlueprintEditorLibrary.compile_blueprint(base)
    return save(base)


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


def desired_state(piece, mesh, hulls):
    """Everything this script sets on one child, as plain values (also what current_state reads back)."""
    box, lo, hi = footprint_box(hulls)
    body = [hi[i] - lo[i] for i in range(3)]
    upright = body[2] > 1.2 * min(body[0], body[1])
    cue = sounds.cue_name(sounds.PIECE_CUE[piece['name'][len('SM_BerlinFurn_'):]])
    state = dict(mesh=mesh.get_name(),
                 nav_box=[round(box['cx'], 1), round(box['cy'], 1), round((box['z0'] + box['z1']) * 0.5, 1),
                          round(box['hu'], 1), round(box['hv'], 1), round((box['z1'] - box['z0']) * 0.5, 1)],
                 nav_area=NAV_AREA + '_C', health=health_for(piece), damage_reduction=DAMAGE_REDUCTION,
                 aim=[0.0, 0.0, float(round(max(hi[2] * 0.5, 20.0)))], crush_rule=CRUSH_RULE,
                 tip=TIP_SPEED_UPRIGHT if upright else TIP_SPEED_LOW, sink=float(round(max(body) * 0.6 + 30.0)),
                 sink_time=SINK_TIME, crush_sound=cue, death_sound=cue)
    return state, box, body, upright


def current_state(bp, named):
    """The same values read from an existing child (class defaults and its component templates)."""
    cdo = unreal.get_default_object(bp.generated_class())
    mesh_comp = cb._own(bp, named, 'MainMesh')
    box = cb._own(bp, named, 'NavBlock')
    holes = cb._own(bp, named, 'WallHoleDamage')
    mesh = mesh_comp.get_editor_property('static_mesh')
    loc, ext = box.get_editor_property('relative_location'), box.get_editor_property('box_extent')
    area = box.get_editor_property('area_class_override')
    aims = list(cdo.get_editor_property('aim_offset_points'))
    crush = cdo.get_editor_property('CrushSound')
    death = holes.get_editor_property('CollapseSettings').get_editor_property('collapse_sound')
    rule = str(cdo.get_editor_property('CrushDestructionType')).split('.')[-1].split(':')[0]
    return dict(mesh=mesh.get_name() if mesh else None,
                nav_box=[round(loc.x, 1), round(loc.y, 1), round(loc.z, 1), round(ext.x, 1), round(ext.y, 1),
                         round(ext.z, 1)],
                nav_area=area.get_name() if area else None, health=float(cdo.get_editor_property('health')),
                damage_reduction=float(cdo.get_editor_property('damage_reduction')),
                aim=[aims[0].x, aims[0].y, aims[0].z] if len(aims) == 1 else None, crush_rule=rule,
                tip=round(float(cdo.get_editor_property('CrushTipSpeedScale')), 4),
                sink=float(cdo.get_editor_property('CrushSinkDeltaZ')),
                sink_time=round(float(cdo.get_editor_property('CrushSinkTime')), 4),
                crush_sound=crush.get_name() if crush else None, death_sound=death.get_name() if death else None)


def build_one(piece, base, area):
    mesh = unreal.load_asset(MESHES + '/' + piece['name'])
    if mesh is None:
        raise RuntimeError('mesh missing: ' + piece['name'])
    hulls = cb.hull_points(mesh)
    if not hulls:
        raise RuntimeError(piece['name'] + ': no UCX hulls')
    name = 'BP_BerlinFurn_' + piece['name'][len('SM_BerlinFurn_'):]
    want, box, body, upright = desired_state(piece, mesh, hulls)
    info = dict(health=want['health'], upright=upright, table_tall=piece.get('tall'), crush_sound=want['crush_sound'],
                death_sound=want['death_sound'], sink_cm=want['sink'], body_cm=[round(v) for v in body],
                nav_box=[round(box['cx']), round(box['cy']), round(box['hu']), round(box['hv']), round(box['z1'])])
    exists = EAL.does_asset_exist(f'{BLUEPRINTS}/{name}')
    bp, created = get_or_create(name, base)
    info.update(blueprint=bp.get_path_name(), created=created)
    named = cb._named(bp)
    if exists and not FORCE:
        have = current_state(bp, named)
        differing = sorted(k for k in want if have.get(k) != want[k])
        if not differing:
            info.update(changed=False, saved=False)
            return info
        info['differed'] = {k: [have.get(k), want[k]] for k in differing}
    setup_mesh_template(cb._own(bp, named, 'MainMesh'), mesh)
    cb.setup_nav_box(cb._own(bp, named, 'NavBlock'), box, area)
    cue = load_cue(sounds.PIECE_CUE[piece['name'][len('SM_BerlinFurn_'):]])
    set_death_sound(cb._own(bp, named, 'WallHoleDamage'), cue)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('health', want['health'])
    cdo.set_editor_property('damage_reduction', DAMAGE_REDUCTION)
    cdo.set_editor_property('aim_offset_points', [unreal.Vector(*want['aim'])])
    cdo.set_editor_property('CrushDestructionType', getattr(unreal.CrushDestructionType, CRUSH_RULE))
    cdo.set_editor_property('CrushTipSpeedScale', want['tip'])
    cdo.set_editor_property('CrushSinkDeltaZ', want['sink'])
    cdo.set_editor_property('CrushSinkTime', SINK_TIME)
    cdo.set_editor_property('CrushSound', cue)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    info.update(changed=True, saved=save(bp))
    after = current_state(bp, cb._named(bp))
    left = sorted(k for k in want if after.get(k) != want[k])
    if left:
        info['not_applied'] = {k: [after.get(k), want[k]] for k in left}
    return info


def piece_number(name):
    return int(name[len('SM_BerlinFurn_'):].split('_')[0])


def run():
    if not os.path.isfile(SOURCE + 'READY.txt'):
        raise RuntimeError('READY.txt missing in ' + SOURCE)
    pieces = json.load(open(SOURCE + 'furniture_table.json'))['pieces']
    if not os.path.isfile(SOURCE + SECOND_BATCH_READY):
        pieces = [p for p in pieces if piece_number(p['name']) < SECOND_BATCH_FIRST]
    report = {'pieces': {}, 'errors': [], 'dry_run': DRY_RUN, 'force': FORCE}
    base = load_base()
    area = cb.nav_area_class(NAV_AREA)
    if FORCE or not base_is_set_up(base):
        report['base'] = 'set up'
        report['base_saved'] = setup_base(base, debris_meshes(), area)
        if not report['base_saved'] and not DRY_RUN:
            report['errors'].append(BASE_NAME + ': save failed')
    else:
        report['base'] = 'kept (already set up; -FurnForce re-applies it)'
    for piece in pieces:
        try:
            info = build_one(piece, base, area)
            report['pieces'][piece['name']] = info
            if info['changed'] and not info['saved'] and not DRY_RUN:
                report['errors'].append(piece['name'] + ': save failed')
            if info.get('not_applied'):
                report['errors'].append(f'{piece["name"]}: values did not stick {info["not_applied"]}')
            log(piece['name'] + ': ' + json.dumps(info))
        except Exception as error:  # noqa: BLE001
            report['errors'].append(f'{piece["name"]}: {error!r}')
            log(f'{piece["name"]}: ERROR {error!r}')
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT.replace('.json', '_dry_run.json') if DRY_RUN else REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    changed = [n for n, i in report['pieces'].items() if i['changed']]
    report['changed'] = changed
    log(f'done: {len(report["pieces"])} Blueprints, {len(changed)} changed '
        f'({sum(1 for n in changed if report["pieces"][n]["created"])} new), base {report["base"][:6]}, '
        f'nav area {NAV_AREA}, errors {report["errors"]}, report {REPORT}')
    return report


if __name__ == '__main__':
    run()
