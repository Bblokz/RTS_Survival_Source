"""Assets for the Ger_IntroMission foliage pass (exec'd in the throwaway editor via fol_cmd.py). Re-runnable.

/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/DryScrub/
  M_DryScrub_Simple                           small masked two-sided material (dead-shrub 4K albedo/alpha + normal,
                                              Tint) with bUsedWithInstancedStaticMeshes SAVED: the crop field builds
                                              its ISMs at runtime, so the cook never sees that usage. The Megascans
                                              foliage master drew only one of five variants from those runtime ISMs
                                              in game (all five in the editor), so it is not used.
  Meshes/SM_DryScrub_DeadShrub_A..E           copies of five Deadshrub variants, LOD0 only, on M_DryScrub_Simple
  BP_CrushableScrub_DeadBrush                 child of BP_CropField_Wheat: sparse dead brush, any tank crushes it
  BP_CrushableScrub_DryGrass                  child of BP_CropField_Wheat: sparse feral dry-grass (wheat) clumps
  MI_DryScrub_FeralGrass                      straw tint of MI_FarmCrops, MaterialOverride of the dry-grass BP
"""
import unreal

D = '/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/DryScrub'
MS = '/Game/Megascans/3D_Plants/Deadshrub-01_vdknafgha/'
GF = '/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/'
WHEAT_BP = '/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/FarmCrops/BP_CropField_Wheat'
SHRUB_VARS = {'A': 3, 'B': 5, 'C': 9, 'D': 11, 'E': 13}
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary


def _dup(src, dst):
    if EAL.does_asset_exist(dst):
        return unreal.load_asset(dst)
    obj = EAL.duplicate_asset(src, dst)
    if obj is None:
        raise RuntimeError('duplicate failed %s -> %s' % (src, dst))
    return obj


def build_materials():
    """M_DryScrub_Simple: albedo RGB x Tint -> base colour, albedo A -> opacity mask, normal, roughness 0.85."""
    path = D + '/M_DryScrub_Simple'
    m = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_DryScrub_Simple', D, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('opacity_mask_clip_value', 0.33)
    MEL.delete_all_material_expressions(m)
    alb = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 0)
    alb.set_editor_property('parameter_name', 'Albedo')
    alb.set_editor_property('texture', unreal.load_asset(MS + 'T_Deadshrub_01_vdknafgha_4K_D'))
    nrm = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -500, 300)
    nrm.set_editor_property('parameter_name', 'Normal')
    nrm.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    nrm.set_editor_property('texture', unreal.load_asset(MS + 'T_Deadshrub_01_vdknafgha_4K_N'))
    tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, -250)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(*SHRUB_TINT))
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, -100)
    MEL.connect_material_expressions(alb, 'RGB', mul, 'A')
    MEL.connect_material_expressions(tint, '', mul, 'B')
    rough = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -200, 200)
    rough.set_editor_property('r', 0.85)
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(alb, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.connect_material_property(nrm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    EAL.save_asset(path, only_if_is_dirty=False)
    return m


SHRUB_TINT = (1.0, 0.88, 0.72, 1.0)     # drier, browner than the source albedo


def build_meshes(mat):
    out = {}
    for key, var in SHRUB_VARS.items():
        dst = D + '/Meshes/SM_DryScrub_DeadShrub_' + key
        m = _dup(MS + 'S_Deadshrub-01_vdknafgha_Var%d_lod0' % var, dst)
        for slot in range(len(m.get_editor_property('static_materials'))):
            m.set_material(slot, mat)
        # LOD0 only. The crop field draws plain ISMs whose LOD follows the whole field's bounds, so in game the brush
        # fell to the far LODs: the Megascans billboard drew nothing, auto-reduced LODs lost the twigs. LOD0 is
        # 0.5-3.4k triangles; a map holds a few hundred of these plants.
        # (set_num_source_models + set_lod_screen_sizes crashed the editor; remove_lods is safe.)
        if m.get_num_lods() > 1:
            unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_lods(m)
        EAL.save_asset(dst, only_if_is_dirty=False)
        out[key] = m
    print('meshes', sorted(out))
    return out


# review r1: the first tint read as golden pom-poms / mini hay bales; darker, greyer, more spread between instances
GRASS_TINT = {'InstanceTintLow': (0.62, 0.50, 0.36), 'InstanceTintHigh': (1.0, 0.82, 0.58),
              'FieldPatchTint': (0.72, 0.60, 0.46)}


def build_grass_mi():
    """Sun-bleached straw tint of the farm crop atlas for the feral dry-grass clumps."""
    dst = D + '/MI_DryScrub_FeralGrass'
    if EAL.does_asset_exist(dst):
        mi = unreal.load_asset(dst)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'MI_DryScrub_FeralGrass', D, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(
            mi, unreal.load_asset('/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/FarmCrops/Materials/MI_FarmCrops'))
    for k, v in GRASS_TINT.items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    MEL.set_material_instance_scalar_parameter_value(mi, 'TransmissionStrength', 0.25)
    MEL.update_material_instance(mi)
    EAL.save_asset(dst, only_if_is_dirty=False)
    return mi


def _variant(intact, crushed, weight):
    v = unreal.CrushableCropVariant()
    v.set_editor_property('intact_mesh', intact)
    v.set_editor_property('crushed_mesh', crushed)
    v.set_editor_property('weight', weight)
    return v


def _child_bp(name):
    path = D + '/' + name
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    parent = unreal.load_asset(WHEAT_BP)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent.generated_class())
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, D, unreal.Blueprint, factory)
    if bp is None:
        raise RuntimeError('could not create ' + path)
    return bp


def _layout(spacing, jitter, missing, inset, smin, smax, seed):
    lay = unreal.CrushableCropLayout()
    for k, v in (('row_spacing', spacing), ('plant_spacing', spacing), ('position_jitter', jitter),
                 ('edge_inset', inset), ('missing_plant_chance', missing), ('min_scale', smin), ('max_scale', smax),
                 ('random_yaw', True), ('yaw_jitter_degrees', 45.0), ('random_seed', seed)):
        lay.set_editor_property(k, v)
    return lay


def build_bps(meshes):
    litter = unreal.load_asset(GF + 'Meshes/SM_GF_DryLitter')
    tuft_a = unreal.load_asset(GF + 'Meshes/SM_GF_DryTuft_A')
    tuft_b = unreal.load_asset(GF + 'Meshes/SM_GF_DryTuft_B')
    brush = _child_bp('BP_CrushableScrub_DeadBrush')
    cdo = unreal.get_default_object(brush.generated_class())
    weights = {'A': 0.2, 'B': 1.0, 'C': 1.0, 'D': 0.6, 'E': 0.9}
    cdo.set_editor_property('variants', [_variant(meshes[k], litter, weights[k]) for k in sorted(meshes)])
    cdo.set_editor_property('layout', _layout(330.0, 0.5, 0.35, 90.0, 2.4, 3.8, 4242))
    unreal.BlueprintEditorLibrary.compile_blueprint(brush)
    EAL.save_asset(brush.get_path_name().split('.')[0], only_if_is_dirty=False)
    grass = _child_bp('BP_CrushableScrub_DryGrass')
    cdo = unreal.get_default_object(grass.generated_class())
    fc = '/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/FarmCrops/Meshes/'
    crushed = unreal.load_asset(fc + 'SM_Wheat_Crushed')
    cdo.set_editor_property('variants', [_variant(unreal.load_asset(fc + 'SM_Wheat_%s' % k), crushed, 1.0) for k in 'ABC'])
    cdo.set_editor_property('layout', _layout(135.0, 0.5, 0.5, 40.0, 0.38, 0.95, 5151))
    cdo.set_editor_property('material_override', build_grass_mi())
    unreal.BlueprintEditorLibrary.compile_blueprint(grass)
    EAL.save_asset(grass.get_path_name().split('.')[0], only_if_is_dirty=False)
    print('bps', brush.get_path_name(), grass.get_path_name())


def build_all():
    meshes = build_meshes(build_materials())
    build_bps(meshes)


# Round 3 card tints (user: the yellow straw/tuft cards "do not fit"; concept sheet A Aschgras + C Rostquecke).
# TintA/TintB multiply the atlas: the stars are olive, the straw/litter cards yellow.
CARD_MIS = {
    'MI_DryScrub_AshGrassTop': ('/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Materials/MI_GroundFoliageTop',
                                (1.25, 1.05, 1.45), (1.45, 1.2, 1.6)),
    'MI_DryScrub_RustMatTop': ('/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Materials/MI_GroundFoliageTop',
                               (0.72, 0.44, 0.40), (0.82, 0.52, 0.46)),
    'MI_DryScrub_RustMat': ('/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Materials/MI_GroundFoliage',
                            (0.72, 0.44, 0.40), (0.82, 0.52, 0.46)),
    # review r3: grey tussocks for the dried pit's grey mud (the ash-tan ones read golden there)
    'MI_DryScrub_AshGrassPit': ('/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Materials/MI_GroundFoliageTop',
                                (1.0, 0.98, 1.45), (1.12, 1.1, 1.62)),
}
ORCHARD_DROUGHT = ('MI_DryScrub_OrchardDrought', '/Game/RTS_Survival/Blueprints/Environment/OrchardTrees/Materials/MI_OrchardTrees',
                   # review r3: (1.05, 0.78, 0.38) left the canopies green; cut green harder for sere olive-brown
                   {'TreeTintLow': (1.3, 0.58, 0.28), 'TreeTintHigh': (1.45, 0.65, 0.32)})   # r4: leaf G/R ~1.8; 1.65/0.9 lime, 1.0/0.62 green


def _child_mi(name, parent):
    dst = D + '/' + name
    if EAL.does_asset_exist(dst):
        return unreal.load_asset(dst)
    mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, D, unreal.MaterialInstanceConstant,
                                                                 unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(parent))
    return mi


def build_card_mis(card_mis=None, orchard=None):
    for name, (parent, a, b) in (card_mis or CARD_MIS).items():
        mi = _child_mi(name, parent)
        MEL.set_material_instance_vector_parameter_value(mi, 'TintA', unreal.LinearColor(a[0], a[1], a[2], 1.0))
        MEL.set_material_instance_vector_parameter_value(mi, 'TintB', unreal.LinearColor(b[0], b[1], b[2], 1.0))
        MEL.update_material_instance(mi)
        EAL.save_asset(D + '/' + name, only_if_is_dirty=False)
    name, parent, params = orchard or ORCHARD_DROUGHT
    mi = _child_mi(name, parent)
    for k, v in params.items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    MEL.update_material_instance(mi)
    EAL.save_asset(D + '/' + name, only_if_is_dirty=False)
    print('card MIs', list((card_mis or CARD_MIS).keys()), name)


# ---------------------------------------------------------------------------------------------------- concept D thistle
# User picked grass concepts A, C and D (2026-11-20). D = Distelhorst dead thistle clumps: thistle_blender.py builds the
# meshes (vertex colour, opaque), here: M_DeadThistle + six meshes + BP_CrushableScrub_Thistle (crushable, any tank).
THISTLE_SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/DeadThistle/'
THISTLE_TINT = (1.45, 1.35, 1.3, 1.0)   # PIE tests 2026-11-20: 1.0 read dark olive, warm 1.75/1.4/1.15 turned the heads yellow


def build_thistle_material():
    """M_DeadThistle: opaque two-sided, VertexColor x Tint, roughness 0.9, ISM usage saved (runtime crop ISMs)."""
    path = D + '/M_DeadThistle'
    m = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_DeadThistle', D, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    MEL.delete_all_material_expressions(m)
    vc = MEL.create_material_expression(m, unreal.MaterialExpressionVertexColor, -500, 0)
    tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, -200)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(*THISTLE_TINT))
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -250, -100)
    MEL.connect_material_expressions(vc, '', mul, 'A')
    MEL.connect_material_expressions(tint, '', mul, 'B')
    rough = MEL.create_material_expression(m, unreal.MaterialExpressionConstant, -250, 150)
    rough.set_editor_property('r', 0.9)
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    EAL.save_asset(path, only_if_is_dirty=False)
    return m


def import_thistle_meshes(mat):
    out = {}
    for name in ['SM_DeadThistle_' + k for k in 'ABC'] + ['SM_DeadThistle_Crushed_' + k for k in 'ABC']:
        task = unreal.AssetImportTask()
        task.filename = THISTLE_SRC + name + '.fbx'
        task.destination_path = D + '/Meshes'
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        ui = unreal.FbxImportUI()
        ui.set_editor_property('import_mesh', True)
        ui.set_editor_property('import_materials', False)
        ui.set_editor_property('import_textures', False)
        ui.set_editor_property('import_as_skeletal', False)
        sd = ui.static_mesh_import_data
        sd.set_editor_property('combine_meshes', True)
        sd.set_editor_property('auto_generate_collision', False)
        sd.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
        sd.set_editor_property('generate_lightmap_u_vs', False)
        task.options = ui
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        m = unreal.load_asset(D + '/Meshes/' + name)
        if m is None:
            raise RuntimeError('import failed ' + name)
        for slot in range(len(m.get_editor_property('static_materials'))):
            m.set_material(slot, mat)
        EAL.save_asset(D + '/Meshes/' + name, only_if_is_dirty=False)
        out[name] = m
    print('thistle meshes', sorted(out))
    return out


def build_thistle_bp(meshes):
    bp = _child_bp('BP_CrushableScrub_Thistle')
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('variants', [
        _variant(meshes['SM_DeadThistle_' + k], meshes['SM_DeadThistle_Crushed_' + k], w)
        for k, w in (('A', 1.0), ('B', 0.8), ('C', 0.7))])
    # clumps 0.9-1.2 m; scale 1.3-1.9 so they read at RTS height (calibration: >= 1.4 for 1 m props)
    cdo.set_editor_property('layout', _layout(210.0, 0.55, 0.3, 60.0, 1.3, 1.9, 6161))
    cdo.set_editor_property('material_override', None)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_asset(bp.get_path_name().split('.')[0], only_if_is_dirty=False)
    print('bp', bp.get_path_name())
    return bp


def build_thistle():
    return build_thistle_bp(import_thistle_meshes(build_thistle_material()))
