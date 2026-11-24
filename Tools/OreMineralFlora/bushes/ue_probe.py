"""Read-only probe for the Ore mineral flora bushes (commandlet). Writes Saved/OreMountains/flora/bushes/probe.json."""
import json
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/bushes/probe.json'
R = {}
mel = unreal.MaterialEditingLibrary


def mat_info(path):
    m = unreal.load_asset(path)
    if m is None:
        return 'missing'
    d = dict(cls=m.get_class().get_name())
    base = m
    if isinstance(m, unreal.MaterialInstance):
        d['parent'] = m.get_editor_property('parent').get_path_name()
        base = m.get_base_material()
        d['vectors'] = {str(p.parameter_info.name): str(p.parameter_value) for p in m.get_editor_property('vector_parameter_values')}
        d['scalars'] = {str(p.parameter_info.name): p.parameter_value for p in m.get_editor_property('scalar_parameter_values')}
    for k in ('used_with_instanced_static_meshes', 'used_with_static_lighting', 'two_sided', 'blend_mode', 'opacity_mask_clip_value',
              'shading_model', 'used_with_niagara_mesh_particles'):
        try:
            d[k] = str(base.get_editor_property(k))
        except Exception as e:
            d[k] = 'ERR'
    try:
        d['scalar_names'] = [str(n) for n in mel.get_scalar_parameter_names(base)]
        d['vector_names'] = [str(n) for n in mel.get_vector_parameter_names(base)]
        d['texture_names'] = [str(n) for n in mel.get_texture_parameter_names(base)]
        d['switch_names'] = [str(n) for n in mel.get_static_switch_parameter_names(base)]
    except Exception as e:
        d['names_err'] = str(e)
    return d


def cdo(path):
    gc = unreal.load_object(None, '%s.%s_C' % (path, path.split('/')[-1]))
    return gc, unreal.get_default_object(gc)


def crop_info(path):
    gc, c = cdo(path)
    d = dict(cls=gc.get_name())
    for k in ('crush_destruction_type', 'standing_plants_nav_area', 'material_override', 'b_standing_plants_cast_shadow',
              'b_crushed_plants_cast_shadow', 'crush_footprint_padding', 'crush_sound_cooldown', 'max_crush_vfx_per_sample'):
        try:
            v = c.get_editor_property(k)
            d[k] = v.get_path_name() if hasattr(v, 'get_path_name') else str(v)
        except Exception as e:
            d[k] = 'ERR ' + str(e)[:80]
    try:
        fx = c.get_editor_property('crush_fx')
        d['crush_fx'] = str(fx)
        for k in ('collapse_vfx', 'collapse_sfx', 'fx_location_offset', 'vfx_scale'):
            try:
                v = fx.get_editor_property(k)
                d['crush_fx.' + k] = v.get_path_name() if hasattr(v, 'get_path_name') else str(v)
            except Exception as e:
                d['crush_fx.' + k] = 'ERR'
    except Exception as e:
        d['crush_fx'] = 'ERR ' + str(e)[:100]
    try:
        d['variants'] = [(v.intact_mesh.get_path_name() if v.intact_mesh else None, v.crushed_mesh.get_path_name() if v.crushed_mesh else None, v.weight)
                         for v in c.get_editor_property('variants')]
        lay = c.get_editor_property('layout')
        d['layout'] = {k: str(lay.get_editor_property(k)) for k in ('row_spacing', 'plant_spacing', 'position_jitter', 'edge_inset', 'missing_plant_chance', 'min_scale', 'max_scale')}
        sp = c.get_editor_property('field_outline')
        d['outline_points'] = [str(sp.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.LOCAL)) for i in range(sp.get_number_of_spline_points())]
    except Exception as e:
        d['variants_err'] = str(e)[:200]
    return d


C = '/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/'
for p in ('/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/MasterMats/M_Foilage_00',
          '/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/MasterMats/M_Trunks_01',
          '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/Materials/MI_OreDeadForest_Cards',
          '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/Materials/MI_OreDeadForest_Wood',
          C + 'DryScrub/M_DryScrub_Simple', C + 'DryScrub/M_DeadThistle',
          C + 'FarmCrops/Materials/MI_FarmCrops'):
    R[p.split('/')[-1]] = mat_info(p)
for p in (C + 'DryScrub/BP_CrushableScrub_DeadBrush', C + 'DryScrub/BP_CrushableScrub_Thistle', C + 'FarmCrops/BP_CropField_Vineyard',
          C + 'FarmCrops/BP_CropField_Wheat'):
    try:
        R[p.split('/')[-1]] = crop_info(p)
    except Exception as e:
        R[p.split('/')[-1]] = 'ERR ' + str(e)
try:
    fc = unreal.load_asset(C + 'FarmCrops/BP_CropField_Vineyard').get_editor_property('parent_class').get_path_name()
    R['vineyard_parent'] = fc
except Exception as e:
    R['vineyard_parent'] = str(e)
sm = unreal.load_asset('/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/Meshes/SM_OreDead_Survivor_9m')
R['survivor_mats'] = [m.material_interface.get_path_name() for m in sm.static_materials]
json.dump(R, open(OUT, 'w'), indent=1, default=str)
unreal.log('BUSHPROBE done')
