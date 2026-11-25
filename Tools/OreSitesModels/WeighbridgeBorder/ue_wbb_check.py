"""Fresh-load check of the weighbridge / border assets (separate process from the import), headless, saves nothing.
  sh run_ue.sh ue_wbb_check.py check

- BP_OreWB_Booth: a child of Rus_Bxp_ContainerHut; its BuildingMeshComponent shows SM_OreWB_Booth with the mesh's own
  materials (no container-blue override), FlagPole without collision; the parent hut still shows SM_RusHut_Container.
- Spawns the child and the parent in the commandlet's transient world and reads the live components (BeginPlay does
  not run here: death / health are the parent's, unchanged).
- UCX hull boxes lie inside each mesh's bounds (a 100x import would put them metres out); socket positions.
- BP_OreWB_BoomBarrier components, tags, collision.
"""
import re

import unreal

SITES = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/'
WB = SITES + 'Weighbridge/'
HUT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_ContainerHut'
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
failures = []


def log(*a):
    unreal.log_warning('ORE_SITES_WBBCHECK ' + ' '.join(str(x) for x in a))


def check(ok, what):
    if not ok:
        failures.append(what)
        log('FAIL', what)
    return ok


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def templates(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        o = SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(h), bp)
        if o is not None:
            out[o.get_name().replace('_GEN_VARIABLE', '')] = o
    return out


def hull_check(path, want):
    mesh = unreal.load_asset(path)
    box = mesh.get_bounding_box()
    geom = mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
    hulls = geom.get_editor_property('convex_elems')
    num = r'(-?[0-9.]+(?:[eE][-+]?[0-9]+)?)'
    pat = re.compile(r'ElemBox=\(Min=\(X=%s,Y=%s,Z=%s\),Max=\(X=%s,Y=%s,Z=%s\)' % ((num,) * 6))
    worst, parsed, boxes = 0.0, 0, []
    for h in hulls:
        m = pat.search(h.export_text())
        if not m:
            continue
        parsed += 1
        v = [float(x) for x in m.groups()]
        boxes.append([round(x) for x in v])
        worst = max(worst, box.min.x - v[0], box.min.y - v[1], box.min.z - v[2], v[3] - box.max.x, v[4] - box.max.y, v[5] - box.max.z)
    log(path.split('/')[-1], 'hulls', len(hulls), 'boxes', boxes, 'worst cm outside', round(worst, 1),
        'bounds', vec(box.min), vec(box.max), 'sockets', {str(s.get_editor_property('socket_name')): vec(s.get_editor_property('relative_location'))
                                                         for s in mesh.get_sockets_by_tag('')})
    check(len(hulls) == want, '%s hulls %d, expected %d' % (path, len(hulls), want))
    check(parsed == len(hulls), '%s hull boxes unreadable' % path)
    check(worst < 5.0, '%s hull sticks %.1f cm out (scale)' % (path, worst))
    return mesh


def booth():
    bp = unreal.load_asset(WB + 'BP_OreWB_Booth')
    if not check(bp is not None, 'booth BP missing'):
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = bp.generated_class()
    parent = unreal.load_asset(HUT)
    check(unreal.MathLibrary.class_is_child_of(cls, parent.generated_class()), 'booth is not a child of the hut')
    check(unreal.MathLibrary.class_is_child_of(cls, unreal.BuildingExpansion), 'booth is not a BuildingExpansion')
    t = templates(bp)
    m = t['BuildingMeshComponent']
    sm = m.get_editor_property('static_mesh')
    log('child template mesh', sm.get_name() if sm else None, 'overrides', [x.get_name() if x else None for x in m.get_editor_property('override_materials')],
        'profile', m.get_collision_profile_name(), 'nav', m.get_editor_property('can_ever_affect_navigation'), 'mobility', m.get_editor_property('mobility'))
    check(sm is not None and sm.get_name() == 'SM_OreWB_Booth', 'child template mesh not the booth')
    check(len(m.get_editor_property('override_materials')) == 0, 'child keeps material overrides')
    check('FlagPole' in t, 'no FlagPole')
    if 'FlagPole' in t:
        f = t['FlagPole']
        log('FlagPole at', vec(f.get_editor_property('relative_location')), 'profile', f.get_collision_profile_name(),
            'parent', f.get_attach_parent().get_name() if f.get_attach_parent() else None)
        check(str(f.get_collision_profile_name()) == 'NoCollision', 'flag pole has collision')
    for n in ('ProgressBarWidget', 'SelectedDecal', 'bpSelectionArea'):
        if n in t:
            log(n, 'loc', vec(t[n].get_editor_property('relative_location')), 'scale', vec(t[n].get_editor_property('relative_scale3d')))
    pt = templates(parent)
    psm = pt['BuildingMeshComponent'].get_editor_property('static_mesh')
    log('parent template mesh', psm.get_name() if psm else None, 'parent widget z', pt['ProgressBarWidget'].get_editor_property('relative_location').z)
    check(psm is not None and psm.get_name() == 'SM_RusHut_Container', 'parent hut mesh changed')
    cdo = unreal.get_default_object(cls)
    for prop in ('death_vfx_lifetime',):
        try:
            log('cdo', prop, cdo.get_editor_property(prop))
        except Exception as e:
            log('cdo', prop, 'n/a', e)
    # spawn both
    for c, want in ((cls, 'SM_OreWB_Booth'), (parent.generated_class(), 'SM_RusHut_Container')):
        a = EAS.spawn_actor_from_class(c, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        if not check(a is not None, 'spawn failed %s' % c.get_name()):
            continue
        comp = a.get_editor_property('building_mesh_component') if hasattr(a, 'building_mesh_component') else None
        if comp is None:
            comp = next((x for x in a.get_components_by_class(unreal.StaticMeshComponent) if x.get_name() == 'BuildingMeshComponent'), None)
        got = comp.get_editor_property('static_mesh').get_name() if comp and comp.get_editor_property('static_mesh') else None
        mats = [comp.get_material(i).get_name() if comp.get_material(i) else None for i in range(comp.get_num_materials())] if comp else []
        log('spawned', c.get_name(), 'mesh', got, 'mats', mats, 'meshes', sorted(x.get_name() for x in a.get_components_by_class(unreal.StaticMeshComponent)),
            'health comps', len(a.get_components_by_class(unreal.HealthComponent)), 'rts comps', len(a.get_components_by_class(unreal.RTSComponent)))
        check(got == want, 'spawned %s shows %s, expected %s' % (c.get_name(), got, want))
        if want == 'SM_OreWB_Booth':
            check(mats and mats[0] == 'MI_OreWB_Plaster', 'spawned booth slot 0 is %s' % (mats[0] if mats else None))
        a.destroy_actor()


def boom():
    bp = unreal.load_asset(WB + 'BP_OreWB_BoomBarrier')
    if not check(bp is not None, 'boom BP missing'):
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    t = templates(bp)
    for n in ('Post', 'Rest', 'Arm'):
        if check(n in t, 'boom missing ' + n):
            c = t[n]
            log('boom', n, c.get_editor_property('static_mesh').get_name(), 'at', vec(c.get_editor_property('relative_location')),
                'mobility', c.get_editor_property('mobility'), 'profile', c.get_collision_profile_name(),
                'tags', [str(x) for x in c.get_editor_property('component_tags')])
    check('BoomArm' in [str(x) for x in t['Arm'].get_editor_property('component_tags')], 'arm tag missing')
    check(t['Arm'].get_editor_property('mobility') == unreal.ComponentMobility.MOVABLE, 'arm not movable')


booth()
hull_check(WB + 'Meshes/SM_OreWB_Booth', 3)
hull_check(WB + 'Meshes/SM_OreWB_Deck', 1)
hull_check(WB + 'Meshes/SM_OreWB_BoomPost', 1)
for n in ('Stone_A', 'Stone_B', 'Stone_C', 'PostSnapped'):
    hull_check(SITES + 'Border/Meshes/SM_OreBorder_' + n, 0)
boom()
log('RESULT', 'OK' if not failures else 'FAIL %s' % failures)
