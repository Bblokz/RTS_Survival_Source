"""Read-only look at the Blueprints / materials the mineral-flora trees are copied from (headless commandlet).
Writes Saved/OreMountains/flora/trees/inspect/ue_inspect.json and T3D text of the source Blueprints and their CDOs."""
import json
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/inspect/'
os.makedirs(OUT, exist_ok=True)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SBL = unreal.SubobjectDataBlueprintFunctionLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
DF = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/'
BPS = [DF + 'BP_OreDeadSpruce_Survivor_9m', DF + 'BP_OreDeadSpruce_Skeleton_11m',
       '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/Logs/BP_Cover_Log1',
       '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/NaturalCover/Logs/BP_Cover_DeadTreeLog_01',
       '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Blueprints/BP_OreCrushRock_Base',
       '/Game/RTS_Survival/Blueprints/Environment/FrozenLake/WinterLarch/BP_FrozenLake_WinterLarch_A']
CDO_NAMES = ['TankDestructionType', 'CrushDestructionType', 'Health', 'MaxHealth', 'TreeMesh', 'bDisableRandomScale', 'VerticalCollapseSpeedScaler',
             'CrushSinkDeltaZ', 'CrushSinkTime', 'Crushable', 'Overlap Tanks', 'OverlapTanks', 'CrushTipSpeedScale', 'DataTableRow', 'HealthRow',
             'MinScale', 'MaxScale', 'StumpMesh', 'AimOffsetPoints', 'bCanBeCrushed']
out = {}


def props(obj, names):
    d = {}
    for n in names:
        try:
            d[n] = str(obj.get_editor_property(n))[:300]
        except Exception:
            pass
    return d


def t3d(obj, name):
    try:
        task = unreal.AssetExportTask()
        task.object = obj
        task.filename = OUT + name + '.t3d'
        task.automated = True
        task.prompt = False
        task.replace_identical = True
        task.exporter = unreal.ObjectExporterT3D()
        return bool(unreal.Exporter.run_asset_export_task(task))
    except Exception as e:
        return 'ERR ' + str(e)[:100]


def bp_info(path):
    bp = unreal.load_asset(path)
    if bp is None:
        return 'missing'
    gc = bp.generated_class()
    cdo = unreal.get_default_object(gc)
    info = dict(cls=gc.get_name(), 
                native_chain=[], cdo=props(cdo, CDO_NAMES))
    c = gc
    for _ in range(8):
        try:
            c = unreal.SystemLibrary.get_super_class(c) if hasattr(unreal.SystemLibrary, 'get_super_class') else None
        except Exception:
            c = None
        if c is None:
            break
        info['native_chain'].append(c.get_name())
    comps, seen = [], set()
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = sds.k2_find_subobject_data_from_handle(h)
        o = SBL.get_object_for_blueprint(data, bp)
        if o is None or not isinstance(o, unreal.ActorComponent) or o.get_path_name() in seen:
            continue
        seen.add(o.get_path_name())
        e = dict(name=o.get_name(), cls=o.get_class().get_name(), inherited=bool(SBL.is_inherited_component(data)))
        if isinstance(o, unreal.StaticMeshComponent):
            e.update(props(o, ['static_mesh', 'relative_location', 'relative_scale3d', 'cast_shadow', 'mobility', 'world_position_offset_disable_distance',
                               'can_ever_affect_navigation', 'generate_overlap_events', 'override_materials']))
            e['collision_profile'] = str(o.get_collision_profile_name())
            e['collision_enabled'] = str(o.get_collision_enabled())
        if isinstance(o, unreal.ShapeComponent):
            e.update(props(o, ['box_extent', 'relative_location', 'area_class', 'area_class_override', 'dynamic_obstacle', 'can_ever_affect_navigation',
                               'hidden_in_game']))
        if isinstance(o, unreal.NavModifierComponent):
            e.update(props(o, ['area_class', 'failsafe_extent', 'include_agent_height']))
        if 'Nav' in o.get_class().get_name() or 'Health' in o.get_class().get_name():
            for n in ('area_class', 'AreaClass', 'nav_collision_settings', 'NavCollisionSettings', 'max_health', 'MaxHealth', 'health', 'damage_reduction'):
                try:
                    e[n] = str(o.get_editor_property(n))[:300]
                except Exception:
                    pass
        comps.append(e)
    info['components'] = comps
    nm = path.split('/')[-1]
    info['t3d_bp'] = t3d(bp, nm)
    info['t3d_cdo'] = t3d(cdo, nm + '_CDO')
    return info


def mat_info(path):
    m = unreal.load_asset(path)
    d = dict(cls=m.get_class().get_name(), parent=m.get_editor_property('parent').get_path_name())
    d['scalars'] = {str(p.parameter_info.name): p.parameter_value for p in m.get_editor_property('scalar_parameter_values')}
    d['vectors'] = {str(p.parameter_info.name): str(p.parameter_value) for p in m.get_editor_property('vector_parameter_values')}
    d['textures'] = {str(p.parameter_info.name): (p.parameter_value.get_path_name() if p.parameter_value else None)
                     for p in m.get_editor_property('texture_parameter_values')}
    return d


def mesh_info(path):
    m = unreal.load_asset(path)
    box = m.get_bounding_box()
    g = m.get_editor_property('body_setup').get_editor_property('agg_geom')
    return dict(lods=[m.get_num_triangles(i) for i in range(m.get_num_lods())], screen=list(sub.get_lod_screen_sizes(m)),
                bounds=[[round(box.min.x), round(box.min.y), round(box.min.z)], [round(box.max.x), round(box.max.y), round(box.max.z)]],
                collision={k: len(g.get_editor_property(k)) for k in ('box_elems', 'convex_elems', 'sphere_elems', 'sphyl_elems')},
                trace=str(m.get_editor_property('body_setup').get_editor_property('collision_trace_flag')),
                sockets=[str(n) for n in sub.get_sockets_names(m)] if hasattr(sub, 'get_sockets_names') else None)


out['bps'] = {}
for p in BPS:
    try:
        out['bps'][p] = bp_info(p)
    except Exception as e:
        out['bps'][p] = 'ERR ' + str(e)
out['mats'] = {}
for p in (DF + 'Materials/MI_OreDeadForest_Cards', DF + 'Materials/MI_OreDeadForest_Wood'):
    try:
        out['mats'][p] = mat_info(p)
    except Exception as e:
        out['mats'][p] = 'ERR ' + str(e)
tex = unreal.load_asset(DF + 'Textures/T_OreDeadForest_BC')
out['tex_bc'] = props(tex, ['srgb', 'compression_settings', 'lod_group', 'alpha_coverage_thresholds', 'do_scale_mips_for_alpha_coverage', 'mip_gen_settings'])
out['meshes'] = {}
for p in (DF + 'Meshes/SM_OreDead_Survivor_9m', '/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/SM_RTS_DeadTree_Log1'):
    try:
        out['meshes'][p] = mesh_info(p)
    except Exception as e:
        out['meshes'][p] = 'ERR ' + str(e)
for bpp, info in out['bps'].items():
    if isinstance(info, dict):
        for c in info['components']:
            sm = c.get('static_mesh')
            if sm and 'StaticMesh' in sm and "'" in sm:
                mp = sm.split("'")[1].split('.')[0]
                try:
                    out['meshes'][mp] = mesh_info(mp)
                except Exception as e:
                    out['meshes'][mp] = 'ERR ' + str(e)
with open(OUT + 'ue_inspect.json', 'w') as f:
    json.dump(out, f, indent=1, default=str)
unreal.log('FLORA_INSPECT done')
