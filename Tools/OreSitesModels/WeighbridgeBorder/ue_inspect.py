"""Read-only inspection for the weighbridge / border models (headless). Writes Saved/OreMountains/sites/models/inspect.json
and exports a few plain textures as PNG for a look. Changes no asset."""
import json
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/models/'
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
rep = {}


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def bp_components(path):
    bp = unreal.load_asset(path)
    out = dict(path=path)
    if bp is None:
        out['missing'] = True
        return out
    gc = bp.generated_class()
    chain = []
    c = gc
    for _ in range(12):
        try:
            c = unreal.SystemLibrary.get_super_class(c) if hasattr(unreal.SystemLibrary, 'get_super_class') else None
        except Exception:
            c = None
        if c is None:
            break
        chain.append(c.get_name())
    out['parent_chain'] = chain
    try:
        out['parent_bp'] = str(bp.get_editor_property('parent_class').get_path_name())
    except Exception as e:
        out['parent_bp'] = 'err ' + str(e)
    comps = []
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDS.k2_find_subobject_data_from_handle(h)
        obj = SDL.get_object_for_blueprint(data, bp)
        if obj is None:
            continue
        row = dict(name=obj.get_name(), cls=obj.get_class().get_name(),
                   inherited=bool(SDL.is_inherited_component(data)) if hasattr(SDL, 'is_inherited_component') else None)
        try:
            row['parent'] = str(SDL.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(SDL.get_parent_handle(data)), bp).get_name()) if hasattr(SDL, 'get_parent_handle') else None
        except Exception:
            pass
        if isinstance(obj, unreal.SceneComponent):
            row['loc'] = vec(obj.get_editor_property('relative_location'))
            r = obj.get_editor_property('relative_rotation')
            row['rot'] = [round(r.roll, 1), round(r.pitch, 1), round(r.yaw, 1)]
            row['scale'] = vec(obj.get_editor_property('relative_scale3d'))
        if isinstance(obj, unreal.StaticMeshComponent):
            m = obj.get_editor_property('static_mesh')
            row['mesh'] = m.get_path_name() if m else None
            if m:
                b = m.get_bounding_box()
                row['mesh_bounds'] = [vec(b.min), vec(b.max)]
                row['mesh_tris'] = m.get_num_triangles(0)
                row['mesh_mats'] = [m.get_material(i).get_path_name() if m.get_material(i) else None
                                    for i in range(len(m.get_editor_property('static_materials')))]
            try:
                row['collision_profile'] = str(obj.get_collision_profile_name())
                row['affects_nav'] = bool(obj.get_editor_property('can_ever_affect_navigation'))
                row['visible'] = bool(obj.get_editor_property('visible'))
                row['override_mats'] = [x.get_path_name() if x else None for x in obj.get_editor_property('override_materials')]
            except Exception as e:
                row['err'] = str(e)
        if isinstance(obj, unreal.PrimitiveComponent) and not isinstance(obj, unreal.StaticMeshComponent):
            try:
                row['collision_profile'] = str(obj.get_collision_profile_name())
            except Exception:
                pass
            if isinstance(obj, unreal.BoxComponent):
                row['box_extent'] = vec(obj.get_editor_property('box_extent'))
        comps.append(row)
    out['components'] = comps
    return out


def mat_info(path):
    m = unreal.load_asset(path)
    if m is None:
        return dict(missing=True)
    d = dict(cls=m.get_class().get_name())
    if isinstance(m, unreal.MaterialInstance):
        p = m.get_editor_property('parent')
        d['parent'] = p.get_path_name() if p else None
        try:
            d['scalars'] = {str(x.parameter_info.name): round(x.parameter_value, 3) for x in m.get_editor_property('scalar_parameter_values')}
            d['vectors'] = {str(x.parameter_info.name): [round(x.parameter_value.r, 3), round(x.parameter_value.g, 3), round(x.parameter_value.b, 3)] for x in m.get_editor_property('vector_parameter_values')}
            d['textures'] = {str(x.parameter_info.name): (x.parameter_value.get_path_name() if x.parameter_value else None) for x in m.get_editor_property('texture_parameter_values')}
        except Exception as e:
            d['err'] = str(e)
    try:
        base = m.get_base_material()
        d['blend'] = str(base.get_editor_property('blend_mode'))
    except Exception:
        pass
    return d


def export_png(path, name):
    t = unreal.load_asset(path)
    if t is None or not isinstance(t, unreal.Texture2D):
        return 'missing'
    task = unreal.AssetExportTask()
    task.object = t
    task.filename = OUT + 'wbb_tex_' + name + '.png'
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = unreal.Exporter.run_asset_export_task(task)
    return dict(ok=bool(ok), size=[t.blueprint_get_size_x(), t.blueprint_get_size_y()])


RB = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/'
rep['hut'] = bp_components(RB + 'Rus_Bxp_ContainerHut')
rep['heap_station'] = bp_components('/Game/RTS_Survival/Blueprints/Environment/OreMountains/HeapStations/BP_OreHeapStation')
mats = ['/Game/RTS_Survival/Blueprints/Environment/OreMountains/A4Viaduct/MI_A4_' + k for k in ('Concrete', 'Granite', 'RebarRust', 'Rubble')]
mats += ['/Game/RTS_Survival/Blueprints/Environment/OreMountains/AditPortals/Materials/MI_OreAdit_' + k for k in ('Steel', 'Sheet', 'Sandbag', 'Timber', 'Paint', 'Tarp')]
mats += ['/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Clues/Materials/MI_OWClue_' + k for k in ('PaintRed', 'PaintWhite', 'Glass', 'RoofTar', 'WagonPaint', 'Canvas')]
mats += ['/Game/Greek_island/Materials/Temple/MI_roof', '/Game/MilitaryAirport/Materials/Tileable/MI_Plaster']
rep['materials'] = {p.split('/')[-1]: mat_info(p) for p in mats}
GI = '/Game/Greek_island/Textures/'
rep['png'] = {}
for p, n in ((GI + 'T_roof/T_rooftile_D', 'rooftile'), (GI + 'T_house_wall/T_greek_house_wall_D', 'housewall'),
             (GI + 'T_stone/T_stone_D', 'stone')):
    try:
        rep['png'][n] = export_png(p, n)
    except Exception as e:
        rep['png'][n] = 'err ' + str(e)
os.makedirs(OUT, exist_ok=True)
with open(OUT + 'wbb_inspect.json', 'w') as f:
    json.dump(rep, f, indent=1)
unreal.log_warning('ORE_SITES_INSPECT done')
