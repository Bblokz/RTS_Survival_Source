"""Bog foliage kit -> /Game/RTS_Survival/Blueprints/Environment/Prypyat/BogFoliage (static meshes for HISM scatter).

Builds T_Prypyat_BogFoliage (alpha coverage preserved in the mips), M_Prypyat_BogFoliage (masked, two-sided, base
colour x Tint, opacity = alpha x OpacityBoost so thin blades survive small mips: the Ore Opacity_Instensity lesson),
MI_Prypyat_BogFoliage, and imports the nine meshes with no collision. exec in the server namespace.
"""
import unreal

BOG_SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PripyatBogFoliage/'
BOG_HOME = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/BogFoliage'
_MEL = unreal.MaterialEditingLibrary
_EAL = unreal.EditorAssetLibrary
_AT = unreal.AssetToolsHelpers.get_asset_tools()


def _bog_texture():
    task = unreal.AssetImportTask()
    task.filename = BOG_SRC + 'T_Prypyat_BogFoliage.png'
    task.destination_path = BOG_HOME + '/Textures'
    task.destination_name = 'T_Prypyat_BogFoliage'
    task.automated, task.replace_existing, task.save = True, True, False
    _AT.import_asset_tasks([task])
    tex = unreal.load_asset(BOG_HOME + '/Textures/T_Prypyat_BogFoliage')
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    try:
        tex.set_editor_property('do_scale_mips_for_alpha_coverage', True)
        tex.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0.0, 0.0, 0.0, 0.4))
        print('alpha coverage mips on')
    except Exception as exc:  # noqa: BLE001
        print('alpha coverage not available:', exc)
    _EAL.save_loaded_asset(tex, False)
    return tex


def _bog_material(tex):
    path = BOG_HOME + '/Materials/M_Prypyat_BogFoliage'
    if _EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        for _ in range(30):
            if _MEL.get_num_material_expressions(mat) == 0:
                break
            _MEL.delete_all_material_expressions(mat)
    else:
        mat = _AT.create_asset('M_Prypyat_BogFoliage', BOG_HOME + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('opacity_mask_clip_value', 0.4)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    sample = _MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -700, 0)
    sample.set_editor_property('texture', tex)
    tint = _MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -250)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mul = _MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, -150)
    boost = _MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 300)
    boost.set_editor_property('parameter_name', 'OpacityBoost')
    boost.set_editor_property('default_value', 2.0)
    amul = _MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 250)
    sat = _MEL.create_material_expression(mat, unreal.MaterialExpressionSaturate, -200, 250)
    rough = _MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, 100)
    rough.set_editor_property('parameter_name', 'Roughness')
    rough.set_editor_property('default_value', 0.85)
    ok = [_MEL.connect_material_expressions(sample, 'RGB', mul, 'A'),
          _MEL.connect_material_expressions(tint, '', mul, 'B'),
          _MEL.connect_material_expressions(sample, 'A', amul, 'A'),
          _MEL.connect_material_expressions(boost, '', amul, 'B'),
          _MEL.connect_material_expressions(amul, '', sat, ''),
          _MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR),
          _MEL.connect_material_property(sat, '', unreal.MaterialProperty.MP_OPACITY_MASK),
          _MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)]
    if not all(ok):
        raise RuntimeError('bog material links failed %s' % ok)
    _MEL.recompile_material(mat)
    _EAL.save_loaded_asset(mat, False)
    mi_path = BOG_HOME + '/Materials/MI_Prypyat_BogFoliage'
    mi = unreal.load_asset(mi_path) if _EAL.does_asset_exist(mi_path) else _AT.create_asset(
        'MI_Prypyat_BogFoliage', BOG_HOME + '/Materials', unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew())
    _MEL.set_material_instance_parent(mi, mat)
    _MEL.update_material_instance(mi)
    _EAL.save_loaded_asset(mi, False)
    return mi


_bog_mi = _bog_material(_bog_texture())
_ALG = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/MI_Phytophage_VioletShroud'
_SNAG = '/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/Materials/MI_Env_DeadTree_1'
PRY_STATIC = []
for _n in ('Reed_A', 'Reed_B', 'Cattail_A', 'Sedge_A', 'Sedge_B', 'Snag_A', 'Snag_B', 'AlgaeMat_A', 'AlgaeMat_B'):
    _mat = _ALG if 'Algae' in _n else (_SNAG if 'Snag' in _n else _bog_mi.get_path_name().split('.')[0])
    PRY_STATIC.append(dict(fbx=BOG_SRC + 'FBX/SM_PrypyatBog_' + _n + '.FBX', dest_folder=BOG_HOME,
                           name='SM_PrypyatBog_' + _n, materials=[_mat], collision='none', lightmap_index=1))
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/static_import.py').read())
print('stats', _MEL.get_statistics(unreal.load_asset(BOG_HOME + '/Materials/M_Prypyat_BogFoliage')).num_pixel_shader_instructions)
