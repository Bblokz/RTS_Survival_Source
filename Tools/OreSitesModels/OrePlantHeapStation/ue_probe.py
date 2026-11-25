"""Read-only probe: silo candidates, Base A stand-in classes, BP_OreHeapStation. Writes ue_oreplant/probe.json."""
import json

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/probe.json'
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
rep = {'meshes': {}, 'bps': {}}

MESHES = ['/Game/QuarryEnv/Meshes/SM_Silo_1', '/Game/QuarryEnv/Meshes/SM_Silo_2', '/Game/QuarryEnv/Meshes/SM_Silo_3',
          '/Game/QuarryEnv/Meshes/SM_Silo_4', '/Game/AtlassedQuarry/SM_Quarry_Silo3', '/Game/AtlassedQuarry/SM_Quarry_Silo4',
          '/Game/MillitaryBase/Meshes/SM_Silos', '/Game/MilitaryAirport/Meshes/Buildings/SM_FuelSilo_1',
          '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/Meshes/SM_Scorch_Silo',
          '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/Meshes/SM_Scorch_Silo2']
for d in ('/Game/QuarryEnv/Meshes', '/Game/AtlassedQuarry'):
    for p in EAL.list_assets(d, recursive=True):
        n = p.split('/')[-1].split('.')[0]
        if any(k in n.lower() for k in ('conveyor', 'crusher', 'hopper', 'belt', 'chute', 'bunker', 'crane', 'tower', 'shed', 'pipe')):
            MESHES.append(p.split('.')[0])
BPS = ['/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_Supplies',
       '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_AmmoStorage',
       '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_FuelStorage',
       '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/BP_Dest_PostApo_Silo',
       '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/BP_Dest_PostApo_Silo2',
       '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/BP_Dest_PostApo_FuelSilo',
       '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Zone_RefinerySilo',
       '/Game/RTS_Survival/Blueprints/Environment/OreMountains/HeapStations/BP_OreHeapStation']


def mesh_info(m):
    b = m.get_bounding_box()
    mats = []
    for i, s in enumerate(m.get_editor_property('static_materials')):
        mi = s.get_editor_property('material_interface')
        mats.append([str(s.get_editor_property('material_slot_name')), mi.get_path_name() if mi else None])
    body = m.get_editor_property('body_setup')
    g = body.get_editor_property('agg_geom') if body else None
    return dict(min=[round(b.min.x), round(b.min.y), round(b.min.z)], max=[round(b.max.x), round(b.max.y), round(b.max.z)],
                tris=m.get_num_triangles(0), lods=m.get_num_lods(), mats=mats,
                hulls=[len(g.get_editor_property(k)) for k in ('convex_elems', 'box_elems', 'sphyl_elems', 'sphere_elems')] if g else None,
                nanite=m.get_editor_property('nanite_settings').enabled)


for p in MESHES:
    m = unreal.load_asset(p)
    if isinstance(m, unreal.StaticMesh):
        rep['meshes'][p] = mesh_info(m)


def props(o, names):
    r = {}
    for n in names:
        try:
            v = o.get_editor_property(n)
            r[n] = v.get_path_name() if hasattr(v, 'get_path_name') else str(v)[:200]
        except Exception:
            pass
    return r


for p in BPS:
    bp = unreal.load_asset(p)
    if bp is None:
        rep['bps'][p] = None
        continue
    gc = bp.generated_class()
    par = unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp) if hasattr(unreal.BlueprintEditorLibrary, 'get_blueprint_parent_class') else None
    entry = dict(parent=par.get_path_name() if par else None, comps=[])
    # class chain
    chain = []
    try:
        c = gc
        for _ in range(8):
            c = unreal.SystemLibrary.get_super_class(c) if hasattr(unreal.SystemLibrary, 'get_super_class') else None
            if c is None:
                break
            chain.append(c.get_name())
    except Exception as e:
        chain.append('err %s' % e)
    entry['chain'] = chain
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDS.k2_find_subobject_data_from_handle(h)
        o = SDL.get_object_for_blueprint(data, bp)
        if o is None:
            continue
        ci = dict(name=o.get_name(), cls=o.get_class().get_name())
        if isinstance(o, unreal.SceneComponent):
            l = o.get_editor_property('relative_location')
            r = o.get_editor_property('relative_rotation')
            s = o.get_editor_property('relative_scale3d')
            ci['xf'] = [round(l.x), round(l.y), round(l.z), round(r.pitch, 1), round(r.yaw, 1), round(r.roll, 1),
                        round(s.x, 3), round(s.y, 3), round(s.z, 3)]
            ci['tags'] = [str(t) for t in o.get_editor_property('component_tags')]
        if isinstance(o, unreal.StaticMeshComponent):
            sm = o.get_editor_property('static_mesh')
            ci['mesh'] = sm.get_path_name() if sm else None
            if sm:
                ci['mesh_info'] = mesh_info(sm)
            ci['collision'] = str(o.get_collision_profile_name())
            ci['nav'] = o.get_editor_property('can_ever_affect_navigation')
            ci['visible'] = o.get_editor_property('visible')
        try:
            ci['inherited'] = SDL.is_inherited_component(data)
        except Exception:
            pass
        entry['comps'].append(ci)
    cdo = unreal.get_default_object(gc)
    entry['cdo'] = props(cdo, ['max_health', 'health', 'tags', 'b_destroy_on_death', 'unit_name'])
    entry['cdo']['tags'] = [str(t) for t in cdo.get_editor_property('tags')]
    rep['bps'][p] = entry

with open(OUT, 'w') as f:
    json.dump(rep, f, indent=1)
unreal.log_warning('OREPLANT_PROBE done %d meshes %d bps' % (len(rep['meshes']), len(rep['bps'])))
