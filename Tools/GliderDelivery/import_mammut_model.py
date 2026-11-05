"""Imports the Mammut game model and builds its materials (DebugGame-Cmd -run=pythonscript).

Meshes and textures come from D:/Blender/BlenderProjects/BlenderTanks/Planes/SuperHeavyTransport/Export
(model_mammut.py, make_textures.py). Mesh slots are filled by name: Camo, Underside, Interior, Glass, Dark, Marking.
M_Mammut_Airframe is shared with the Träger C (Source/Tools/TraegerC), whose instances turn on LargeCamo.
"""
import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/Planes/SuperHeavyTransport/Export'
ROOT = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut'
MESHES = ['SM_Mammut_Body', 'SM_Mammut_Nose', 'SM_He111Z_Tug', 'SM_He111Z_Prop']
WHITE = '/Engine/EngineResources/WhiteSquareTexture'
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# Linear tints; sRGB references RLM 65 (130,160,180), dark steel and interior grey. The detail keys scale the shared
# panel normal map, roughness streaks, seam occlusion and paint wear of M_Mammut_Airframe (0 turns one off).
SLOT_SETTINGS = {
    'Camo': dict(texture='T_Mammut_Splinter', tint=(1, 1, 1), roughness=0.5,
                 detail=dict(NormalStrength=1.0, RoughnessVariation=0.5, CavityStrength=0.5, WearAmount=1.0)),
    'Underside': dict(texture=None, tint=(0.22, 0.35, 0.46), roughness=0.5,
                      detail=dict(NormalStrength=1.0, RoughnessVariation=0.5, CavityStrength=0.6, WearAmount=0.8)),
    'Interior': dict(texture=None, tint=(0.07, 0.075, 0.07), roughness=0.8,
                     detail=dict(NormalStrength=0.6, RoughnessVariation=0.3, CavityStrength=1.0, WearAmount=0.4)),
    'Glass': dict(texture=None, tint=(0.02, 0.035, 0.05), roughness=0.08,
                  detail=dict(NormalStrength=0, RoughnessVariation=0, CavityStrength=0, WearAmount=0)),
    'Dark': dict(texture=None, tint=(0.025, 0.025, 0.025), roughness=0.45,
                 detail=dict(NormalStrength=0.4, RoughnessVariation=0.3, CavityStrength=0.6, WearAmount=0,
                             Metallic=0.6)),
}
DETAIL_TEXTURES = {
    'T_Mammut_Detail_N': unreal.TextureCompressionSettings.TC_NORMALMAP,
    'T_Mammut_Detail_M': unreal.TextureCompressionSettings.TC_MASKS,
}
# The macro sample repeats every 26 m instead of 6 m, so the camo tile doesn't show as a grid across the wing.
MACRO_TILING = 0.23
# Bare aluminium where the paint is chipped through along the seams: a light linear colour, mostly metallic and a
# little smoother than the paint, so the chips catch the light instead of reading as grey dirt.
WEAR_COLOR = (0.26, 0.26, 0.24)
WEAR_METALLIC = 0.5
WEAR_ROUGHNESS_OFFSET = -0.12
# LargeCamo only: stencil paint laid over the camo where PaintDetail's alpha says so (light grey, linear).
OVERLAY_COLOR = (0.55, 0.54, 0.48)


def log(msg):
    unreal.log_warning('MAMMUT_IMPORT ' + msg)


def import_file(path, folder, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', folder)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(folder + '/' + name)
    log('imported %s -> %s' % (path, asset))
    return asset


def make_material(name, masked):
    path = ROOT + '/Materials/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = TOOLS.create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    tex = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, -100)
    tex.set_editor_property('parameter_name', 'BaseTexture')
    tex.set_editor_property('texture', unreal.load_asset(WHITE))
    tint = MEL.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, 200)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 0)
    MEL.connect_material_expressions(tex, 'RGB', mul, 'A')
    MEL.connect_material_expressions(tint, '', mul, 'B')
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -350, 250)
    rough.set_editor_property('parameter_name', 'Roughness')
    rough.set_editor_property('default_value', 0.7)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if masked:
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property('two_sided', True)
        MEL.connect_material_property(tex, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    mat.set_editor_property('used_with_nanite', True)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat


def expression(mat, cls, x, y, **props):
    node = MEL.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        node.set_editor_property(key, value)
    return node


def scalar(mat, name, value, x, y):
    return expression(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def link(src, src_pin, dst, dst_pin):
    if not MEL.connect_material_expressions(src, src_pin, dst, dst_pin):
        log('FAILED link %s.%s -> %s.%s' % (src.get_name(), src_pin, dst.get_name(), dst_pin))


def lerp(mat, a, a_pin, b, b_pin, alpha, alpha_pin, x, y):
    node = expression(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    for src, pin, dst in ((a, a_pin, 'A'), (b, b_pin, 'B'), (alpha, alpha_pin, 'Alpha')):
        link(src, pin, node, dst)
    return node


def multiply(mat, a, a_pin, b, b_pin, x, y):
    node = expression(mat, unreal.MaterialExpressionMultiply, x, y)
    link(a, a_pin, node, 'A')
    link(b, b_pin, node, 'B')
    return node


def make_airframe_material(name, textures):
    """Painted skin: splinter or tint base, panel seams and rivets in the normal map, seam occlusion, roughness
    streaks, paint chipped to bare metal along the seams, and a large-scale brightness drift so the 6 m tile doesn't
    read as a grid."""
    path = ROOT + '/Materials/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = TOOLS.create_asset(name, ROOT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    sampler_normal = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    sampler_masks = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    tex_class = unreal.MaterialExpressionTextureSampleParameter2D

    base = expression(mat, tex_class, -1400, -400, parameter_name='BaseTexture', texture=unreal.load_asset(WHITE))
    # LargeCamo (static switch, off for the Mammut): BaseTexture comes from UV channel 1, a much larger tile, and the
    # PaintDetail multiplier on channel 0 carries the panel-scale paint work, so big airframes don't show the camo
    # repeating (make_traeger_textures.py).
    uv0 = expression(mat, unreal.MaterialExpressionTextureCoordinate, -1900, -450, coordinate_index=0)
    uv1 = expression(mat, unreal.MaterialExpressionTextureCoordinate, -1900, -350, coordinate_index=1)
    base_uv = expression(mat, unreal.MaterialExpressionStaticSwitchParameter, -1650, -400, parameter_name='LargeCamo',
                         default_value=False)
    link(uv1, '', base_uv, 'True')
    link(uv0, '', base_uv, 'False')
    link(base_uv, '', base, 'UVs')
    tint = expression(mat, unreal.MaterialExpressionVectorParameter, -1400, -150, parameter_name='Tint',
                      default_value=unreal.LinearColor(1, 1, 1, 1))
    mask = expression(mat, tex_class, -1400, 100, parameter_name='DetailMask', texture=textures['T_Mammut_Detail_M'],
                      sampler_type=sampler_masks)
    normal_tex = expression(mat, tex_class, -1400, 450, parameter_name='DetailNormal',
                            texture=textures['T_Mammut_Detail_N'], sampler_type=sampler_normal)
    macro_uv = expression(mat, unreal.MaterialExpressionTextureCoordinate, -1700, 750, u_tiling=MACRO_TILING,
                          v_tiling=MACRO_TILING)
    macro = expression(mat, unreal.MaterialExpressionTextureSample, -1400, 750,
                       texture=textures['T_Mammut_Detail_M'], sampler_type=sampler_masks)
    link(macro_uv, '', macro, 'UVs')

    normal_strength = scalar(mat, 'NormalStrength', 1.0, -1100, 600)
    rough_variation = scalar(mat, 'RoughnessVariation', 0.45, -1100, 300)
    cavity_strength = scalar(mat, 'CavityStrength', 1.0, -1100, 0)
    wear_amount = scalar(mat, 'WearAmount', 1.0, -1100, -100)
    roughness = scalar(mat, 'Roughness', 0.62, -800, 350)
    metallic = scalar(mat, 'Metallic', 0.0, -300, 200)
    one = expression(mat, unreal.MaterialExpressionConstant, -1100, 100, r=1.0)

    # Base colour: paint, chipped to bare metal, darkened in the seams, drifting in brightness over 26 m.
    plain_paint = multiply(mat, base, 'RGB', tint, '', -1000, -300)
    detail = expression(mat, tex_class, -1400, -750, parameter_name='PaintDetail',
                        texture=textures['T_Neutral_PaintDetail'],
                        sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    shade = expression(mat, unreal.MaterialExpressionMultiply, -1100, -700, const_b=2.0)
    link(detail, 'RGB', shade, 'A')
    shaded_paint = multiply(mat, plain_paint, '', shade, '', -900, -650)
    overlay = expression(mat, unreal.MaterialExpressionVectorParameter, -1100, -850, parameter_name='OverlayColor',
                         default_value=unreal.LinearColor(*OVERLAY_COLOR, 1))
    overlaid = lerp(mat, shaded_paint, '', overlay, '', detail, 'A', -750, -650)
    paint = expression(mat, unreal.MaterialExpressionStaticSwitchParameter, -600, -450, parameter_name='LargeCamo',
                       default_value=False)
    link(overlaid, '', paint, 'True')
    link(plain_paint, '', paint, 'False')
    wear_color = expression(mat, unreal.MaterialExpressionConstant3Vector, -1000, -200,
                            constant=unreal.LinearColor(*WEAR_COLOR, 1))
    wear_alpha = multiply(mat, mask, 'B', wear_amount, '', -900, -100)
    worn = lerp(mat, paint, '', wear_color, '', wear_alpha, '', -700, -300)
    cavity = lerp(mat, one, '', mask, 'R', cavity_strength, '', -800, 0)
    macro_tone = expression(mat, unreal.MaterialExpressionLinearInterpolate, -1100, 850, const_a=0.86, const_b=1.1)
    link(macro, 'G', macro_tone, 'Alpha')
    shaded = multiply(mat, worn, '', cavity, '', -500, -250)
    toned = multiply(mat, shaded, '', macro_tone, '', -300, -250)
    MEL.connect_material_property(toned, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(cavity, '', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Roughness: the slot's value, pushed around by the streak channel; chipped bare metal is a little smoother.
    centred = expression(mat, unreal.MaterialExpressionSubtract, -900, 250, const_b=0.5)
    link(mask, 'G', centred, 'A')
    offset = multiply(mat, centred, '', rough_variation, '', -700, 250)
    rough_sum = expression(mat, unreal.MaterialExpressionAdd, -550, 300)
    link(roughness, '', rough_sum, 'A')
    link(offset, '', rough_sum, 'B')
    wear_rough = expression(mat, unreal.MaterialExpressionMultiply, -700, 400, const_b=WEAR_ROUGHNESS_OFFSET)
    link(wear_alpha, '', wear_rough, 'A')
    rough_total = expression(mat, unreal.MaterialExpressionAdd, -400, 350)
    link(rough_sum, '', rough_total, 'A')
    link(wear_rough, '', rough_total, 'B')
    rough_clamped = expression(mat, unreal.MaterialExpressionSaturate, -250, 350)
    link(rough_total, '', rough_clamped, '')
    MEL.connect_material_property(rough_clamped, '', unreal.MaterialProperty.MP_ROUGHNESS)
    worn_metallic = expression(mat, unreal.MaterialExpressionLinearInterpolate, -150, 200, const_b=WEAR_METALLIC)
    link(metallic, '', worn_metallic, 'A')
    link(wear_alpha, '', worn_metallic, 'Alpha')
    MEL.connect_material_property(worn_metallic, '', unreal.MaterialProperty.MP_METALLIC)

    # Normal: flatten towards +Z by NormalStrength so glass and metal parts can switch the panels off.
    flat = expression(mat, unreal.MaterialExpressionConstant3Vector, -1100, 500,
                      constant=unreal.LinearColor(0, 0, 1, 1))
    blended = lerp(mat, flat, '', normal_tex, 'RGB', normal_strength, '', -800, 550)
    normalised = expression(mat, unreal.MaterialExpressionNormalize, -550, 550)
    link(blended, '', normalised, 'VectorInput')
    MEL.connect_material_property(normalised, '', unreal.MaterialProperty.MP_NORMAL)

    # The imported meshes are Nanite; without this flag the game falls back to the default checker material.
    mat.set_editor_property('used_with_nanite', True)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat


def import_detail_textures():
    textures = {}
    for name, compression in DETAIL_TEXTURES.items():
        texture = import_file(SRC + '/Textures/' + name + '.png', ROOT + '/Textures', name)
        texture.set_editor_property('srgb', False)
        texture.set_editor_property('compression_settings', compression)
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
        textures[name] = texture
    return textures


def make_instance(name, parent, texture, tint, roughness, detail=None):
    path = ROOT + '/Materials/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = TOOLS.create_asset(name, ROOT + '/Materials', unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', parent)
    if texture is not None:
        MEL.set_material_instance_texture_parameter_value(mi, 'BaseTexture', texture)
    MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(*tint, 1))
    MEL.set_material_instance_scalar_parameter_value(mi, 'Roughness', roughness)
    for parameter, value in (detail or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, parameter, value)
    MEL.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


def build_materials():
    textures = {}
    for name in ('T_Mammut_Splinter', 'T_Balkenkreuz'):
        textures[name] = import_file(SRC + '/Textures/' + name + '.png', ROOT + '/Textures', name)
    neutral = import_file(SRC + '/Textures/T_Neutral_PaintDetail.png', ROOT + '/Textures', 'T_Neutral_PaintDetail')
    neutral.set_editor_property('srgb', False)
    unreal.EditorAssetLibrary.save_asset(neutral.get_path_name().split('.')[0], only_if_is_dirty=False)
    textures['T_Neutral_PaintDetail'] = neutral
    textures.update(import_detail_textures())
    airframe = make_airframe_material('M_Mammut_Airframe', textures)
    marking = make_material('M_Mammut_Marking', masked=True)
    slot_materials = {}
    for slot, cfg in SLOT_SETTINGS.items():
        texture = textures[cfg['texture']] if cfg['texture'] else None
        slot_materials[slot] = make_instance('MI_Mammut_' + slot, airframe, texture, cfg['tint'], cfg['roughness'],
                                             cfg['detail'])
    slot_materials['Marking'] = make_instance('MI_Mammut_Marking', marking, textures['T_Balkenkreuz'],
                                              (1, 1, 1), 0.6)
    return slot_materials


def main():
    slot_materials = build_materials()
    # rebuild_mammut_materials.py only refreshes the materials; the meshes keep their slots.
    if globals().get('MAMMUT_MATERIALS_ONLY', False):
        log('DONE materials only')
        return

    for mesh_name in MESHES:
        mesh = import_file(SRC + '/Model/' + mesh_name + '.fbx', ROOT + '/Meshes', mesh_name)
        static_materials = mesh.get_editor_property('static_materials')
        for index, static_material in enumerate(static_materials):
            slot = str(static_material.get_editor_property('material_slot_name')).split('.')[0]
            if slot in slot_materials:
                mesh.set_material(index, slot_materials[slot])
            else:
                log('unknown slot %s on %s' % (slot, mesh_name))
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        log('mesh %s slots=%s' % (mesh_name, [str(m.get_editor_property('material_slot_name'))
                                             for m in mesh.get_editor_property('static_materials')]))

    # The FBX import also made plain materials named after the slots; nothing uses them now.
    for asset_path in unreal.EditorAssetLibrary.list_assets(ROOT + '/Meshes', recursive=False):
        asset = unreal.load_asset(asset_path)
        if isinstance(asset, unreal.Material):
            unreal.EditorAssetLibrary.delete_asset(asset_path)
            log('removed import material ' + asset_path)
    log('DONE')


main()
