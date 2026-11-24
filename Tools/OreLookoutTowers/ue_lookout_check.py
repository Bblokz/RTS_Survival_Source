"""Fresh-load check of BP_OreLookoutTower (separate process from the build), headless, saves nothing:

  sh run_ue_py.sh ue_lookout_check.py <log> DebugGame

Loads and compiles the Blueprint, reads the component templates (meshes, collision, navigation, tags, places),
checks the tower mesh's UCX hull boxes lie inside the mesh (scale), spawns one tower in an unsaved /Temp level, calls
OnLookoutCaptured(1) twice (held once, captured event once) and leaves without saving the level.
BeginPlay does not run here: the reveal, the warnings, the spin and the runtime collision are not exercised.
"""
import re

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers'
BP = HOME + '/BP_OreLookoutTower'
failures = []


def log(*a):
    unreal.log_warning('LOOKOUT_CHECK ' + ' '.join(str(x) for x in a))


def check(ok, what):
    if not ok:
        failures.append(what)
        log('FAIL', what)
    return ok


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def check_hulls(mesh):
    """Each hull's ElemBox (from its exported text: FKConvexElem has no python properties) must lie inside the mesh
    bounds; a 100x import would put them metres outside."""
    box = mesh.get_bounding_box()
    geom = mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
    hulls = geom.get_editor_property('convex_elems')
    check(len(hulls) == 13, 'tower hulls %d, expected 13' % len(hulls))
    num = r'(-?[0-9.]+(?:[eE][-+]?[0-9]+)?)'
    pat = re.compile(r'ElemBox=\(Min=\(X=%s,Y=%s,Z=%s\),Max=\(X=%s,Y=%s,Z=%s\)' % ((num,) * 6))
    worst, parsed, top = 0.0, 0, 0.0
    for h in hulls:
        m = pat.search(h.export_text())
        if not m:
            continue
        parsed += 1
        v = [float(x) for x in m.groups()]
        top = max(top, v[5])
        worst = max(worst, box.min.x - v[0], box.min.y - v[1], box.min.z - v[2], v[3] - box.max.x, v[4] - box.max.y,
                    v[5] - box.max.z)
    if parsed == 0:
        log('hull text', hulls[0].export_text()[:400])
    log('hulls', len(hulls), 'boxes parsed', parsed, 'worst cm outside mesh bounds', round(worst, 2), 'top cm', round(top))
    check(parsed == len(hulls), 'hull boxes not readable (%d of %d)' % (parsed, len(hulls)))
    check(worst < 5.0, 'a UCX hull sticks %.1f cm out of the mesh (scale wrong)' % worst)


def templates(blueprint):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    found = {}
    for handle in sds.k2_gather_subobject_data_for_blueprint(blueprint):
        obj = sdl.get_object_for_blueprint(sds.k2_find_subobject_data_from_handle(handle), blueprint)
        if obj:
            found[obj.get_name().replace('_GEN_VARIABLE', '')] = obj
    return found


def check_blueprint():
    bp = unreal.load_asset(BP)
    if not check(bp is not None, 'BP does not load'):
        return None
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = bp.generated_class()
    check(cls is not None, 'no generated class')
    check(unreal.MathLibrary.class_is_child_of(cls, unreal.LookoutTowerCaptureActor), 'parent is not ALookoutTowerCaptureActor')
    check(unreal.MathLibrary.class_is_child_of(cls, unreal.CaptureActor), 'not an ACaptureActor')
    cdo = unreal.get_default_object(cls)
    cs = cdo.get_editor_property('capture_settings')
    ls = cdo.get_editor_property('lookout_settings')
    log('capture units', cs.get_editor_property('amount_capture_units_needed'), 'seconds',
        cs.get_editor_property('capture_duration'), 'text', cdo.get_editor_property('capture_progress_text'),
        'health', cdo.get_editor_property('health'), 'can be damaged', cdo.get_editor_property('can_be_damaged'))
    log('lookout reveal', ls.get_editor_property('reveal_radius'), 'socket', ls.get_editor_property('reveal_socket_name'),
        'capture filter', ls.get_editor_property('capture_location_socket_filter'),
        'spin', ls.get_editor_property('spin_degrees_per_second'))
    check(abs(ls.get_editor_property('reveal_radius') - 9000.0) < 0.1, 'reveal radius not 9000')
    comps = templates(bp)
    log('components', sorted(comps))
    for name in ('TowerMesh', 'SheaveAxle', 'Sheave_N', 'Sheave_S', 'HeldPanel'):
        check(name in comps, 'missing component ' + name)
    tower = comps.get('TowerMesh')
    root = comps.get('DefaultSceneRoot')
    if root:
        log('root mobility', root.get_editor_property('mobility'))
        check(root.get_editor_property('mobility') == unreal.ComponentMobility.STATIC, 'root not static')
    if tower:
        check(tower.get_editor_property('mobility') == unreal.ComponentMobility.STATIC, 'tower mesh not static')
        mesh = tower.get_editor_property('static_mesh')
        check(mesh is not None and mesh.get_name() == 'SM_OreLookout_Schachtwarte', 'tower mesh not set')
        log('TowerMesh profile', tower.get_collision_profile_name(), 'enabled', tower.get_collision_enabled(),
            'nav', tower.get_editor_property('can_ever_affect_navigation'),
            'pawn', tower.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN),
            'visibility', tower.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY))
        check(str(tower.get_collision_profile_name()) == 'BlockAll', 'tower profile not BlockAll')
        check(tower.get_editor_property('can_ever_affect_navigation'), 'tower does not affect navigation')
        if mesh:
            check_hulls(mesh)
            check(len(mesh.get_sockets_by_tag('')) == 13, 'tower sockets not 13')
    check('ClickVolume' not in comps, 'old ClickVolume still there')
    for name in ('SheaveAxle', 'Sheave_N', 'Sheave_S', 'HeldPanel'):
        c = comps.get(name)
        if c:
            log(name, 'at', vec(c.get_editor_property('relative_location')),
                'rot', c.get_editor_property('relative_rotation'), 'tags', list(c.get_editor_property('component_tags')),
                'mobility', c.get_editor_property('mobility'))
    for name in ('Sheave_N', 'Sheave_S', 'HeldPanel'):
        c = comps.get(name)
        if c:
            check(str(c.get_collision_profile_name()) == 'NoCollision', name + ' has collision')
            check(not c.get_editor_property('can_ever_affect_navigation'), name + ' affects navigation')
    check('LookoutSpin' in [str(t) for t in comps['SheaveAxle'].get_editor_property('component_tags')], 'axle tag')
    check('LookoutHeldPanel' in [str(t) for t in comps['HeldPanel'].get_editor_property('component_tags')], 'panel tag')
    return cls


def spawn_test(cls):
    # The commandlet's own transient editor world: LevelEditorSubsystem.new_level('/Temp/..') wrote a .umap to Saved/.
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not check(actor is not None, 'spawn failed'):
        return
    comps = actor.get_components_by_class(unreal.StaticMeshComponent)
    log('spawned', actor.get_name(), 'static meshes', sorted(c.get_name() for c in comps))
    for c in comps:
        if c.get_name() == 'TowerMesh':
            log('spawned TowerMesh bounds', vec(c.bounds.box_extent) if hasattr(c, 'bounds') else '?',
                'sockets', len(c.get_all_socket_names()), 'entrance at', vec(c.get_socket_location('entrance_01')))
    # Line traces get no hits in this commandlet world (no physics scene), so runtime collision is checked in game.
    fired = []
    actor.on_lookout_tower_captured.add_callable(lambda tower, player: fired.append(player))
    check(not actor.get_is_held(), 'held before capture')
    actor.on_lookout_captured(1)
    actor.on_lookout_captured(1)
    log('after capture held', actor.get_is_held(), 'holder', actor.get_holding_player(), 'events', fired,
        'towers in world', len(unreal.LookoutTowerCaptureActor.get_all_lookout_towers(actor)),
        'any held', unreal.LookoutTowerCaptureActor.get_is_any_lookout_tower_held(actor, 1))
    check(actor.get_is_held() and actor.get_holding_player() == 1, 'not held by 1 after capture')
    check(fired == [1], 'captured event fired %s, expected once' % fired)
    actor.destroy_actor()


cls = check_blueprint()
if cls:
    spawn_test(cls)
log('RESULT', 'OK' if not failures else 'FAIL %s' % failures)
