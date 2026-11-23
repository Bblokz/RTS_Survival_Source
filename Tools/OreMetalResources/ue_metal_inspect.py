"""Read-only look at the stock metal resource Blueprints (headless commandlet, -nullrhi).
Writes Saved/OreMountains/metal/inspect/inspect.json and T3D of each Blueprint, its CDO and its materials."""
import json
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/metal/inspect/'
os.makedirs(OUT, exist_ok=True)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SBL = unreal.SubobjectDataBlueprintFunctionLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
R = '/Game/RTS_Survival/Blueprints/Resources/'
BPS = [R + 'RTS_BP_Resource', R + 'ChildBps/Metal/BP_Prx_MetalResource', R + 'ChildBps/Metal/Resource_Metal_Barrels',
       R + 'ChildBps/Metal/Resource_Metal_BrokenWaterTank', R + 'ChildBps/Metal/Resource_Metal_Rails',
       R + 'ChildBps/Metal/Resource_Metal_Ruin1', R + 'ChildBps/Metal/Resource_MetalTower']
out = {'bps': {}, 'mats': {}, 'meshes': {}, 'gcs': {}}


def t3d(obj, name):
    try:
        task = unreal.AssetExportTask()
        task.object, task.filename, task.automated, task.prompt, task.replace_identical = obj, OUT + name + '.t3d', True, False, True
        task.exporter = unreal.ObjectExporterT3D()
        return bool(unreal.Exporter.run_asset_export_task(task))
    except Exception as e:
        return 'ERR ' + str(e)[:100]


def props(obj, names):
    d = {}
    for n in names:
        try:
            d[n] = str(obj.get_editor_property(n))[:400]
        except Exception:
            pass
    return d


for path in BPS:
    try:
        bp = unreal.load_asset(path)
        gc = bp.generated_class()
        cdo = unreal.get_default_object(gc)
        info = {'cls': gc.get_name(), 'parent': str(bp.get_editor_property('parent_class')) if hasattr(bp, 'parent_class') else None}
        comps, seen = [], set()
        for h in sds.k2_gather_subobject_data_for_blueprint(bp):
            data = sds.k2_find_subobject_data_from_handle(h)
            o = SBL.get_object_for_blueprint(data, bp)
            if o is None or not isinstance(o, unreal.ActorComponent) or o.get_path_name() in seen:
                continue
            seen.add(o.get_path_name())
            e = dict(name=o.get_name(), cls=o.get_class().get_name(), inherited=bool(SBL.is_inherited_component(data)))
            e.update(props(o, ['static_mesh', 'rest_collection', 'relative_location', 'relative_rotation', 'relative_scale3d', 'override_materials',
                               'box_extent', 'area_class', 'visible', 'hidden_in_game', 'attach_parent']))
            if isinstance(o, unreal.PrimitiveComponent):
                e['collision_profile'] = str(o.get_collision_profile_name())
            comps.append(e)
            nm = path.split('/')[-1] + '__' + o.get_name()
            if not isinstance(o, (unreal.StaticMeshComponent, unreal.SceneComponent)) or 'Resource' in o.get_class().get_name():
                t3d(o, nm)
        info['components'] = comps
        nm = path.split('/')[-1]
        info['t3d'] = [t3d(bp, nm), t3d(cdo, nm + '_CDO')]
        out['bps'][path] = info
    except Exception as e:
        out['bps'][path] = 'ERR ' + str(e)

M = R + 'Metal/Meshes/'
for p in (M + 'Materials/M_RTS_MetalResource', M + 'Materials/M_RTS_WaterTank_MetalResource', M + 'Materials/MI_MetalResource_Barrels',
          M + 'Materials/MI_MetalResource_Rails'):
    try:
        m = unreal.load_asset(p)
        d = {'cls': m.get_class().get_name(), 't3d': t3d(m, p.split('/')[-1])}
        if isinstance(m, unreal.MaterialInstanceConstant):
            d['parent'] = m.get_editor_property('parent').get_path_name()
            d['scalars'] = {str(q.parameter_info.name): q.parameter_value for q in m.get_editor_property('scalar_parameter_values')}
            d['vectors'] = {str(q.parameter_info.name): str(q.parameter_value) for q in m.get_editor_property('vector_parameter_values')}
            d['textures'] = {str(q.parameter_info.name): (q.parameter_value.get_path_name() if q.parameter_value else None)
                             for q in m.get_editor_property('texture_parameter_values')}
        out['mats'][p] = d
    except Exception as e:
        out['mats'][p] = 'ERR ' + str(e)
for a in unreal.EditorAssetLibrary.list_assets(R + 'Metal', recursive=True):
    a = a.split('.')[0]
    try:
        o = unreal.load_asset(a)
        if isinstance(o, unreal.StaticMesh):
            box = o.get_bounding_box()
            g = o.get_editor_property('body_setup').get_editor_property('agg_geom')
            out['meshes'][a] = dict(tris=[o.get_num_triangles(i) for i in range(o.get_num_lods())],
                                    bounds=[[round(box.min.x), round(box.min.y), round(box.min.z)], [round(box.max.x), round(box.max.y), round(box.max.z)]],
                                    mats=[str(s.material_interface.get_path_name()) if s.material_interface else None for s in o.static_materials],
                                    collision={k: len(g.get_editor_property(k)) for k in ('box_elems', 'convex_elems', 'sphere_elems', 'sphyl_elems')},
                                    sockets=[str(s.socket_name) for s in o.get_sockets_by_tag('')] if hasattr(o, 'get_sockets_by_tag') else None,
                                    nanite=str(o.get_editor_property('nanite_settings').enabled))
        elif o.get_class().get_name() == 'GeometryCollection':
            out['gcs'][a] = props(o, ['materials', 'geometry_source', 'enable_nanite', 'size_specific_data', 'damage_threshold', 'root_proxy_data'])
        else:
            out.setdefault('other', {})[a] = o.get_class().get_name()
    except Exception as e:
        out['meshes'][a] = 'ERR ' + str(e)
json.dump(out, open(OUT + 'inspect.json', 'w'), indent=1)
print('FLORA_INSPECT_DONE')
