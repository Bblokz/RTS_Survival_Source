"""Imports the Mammut game model and builds its materials (DebugGame-Cmd -run=pythonscript).

Meshes and textures come from D:/Blender/BlenderProjects/BlenderTanks/Planes/SuperHeavyTransport/Export
(model_mammut.py, make_textures.py). Mesh slots are filled by name: Camo, Underside, Interior, Glass, Dark, Marking.
"""
import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/Planes/SuperHeavyTransport/Export'
ROOT = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut'
MESHES = ['SM_Mammut_Body', 'SM_Mammut_Nose', 'SM_He111Z_Tug', 'SM_He111Z_Prop']
WHITE = '/Engine/EngineResources/WhiteSquareTexture'
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

# Linear tints; sRGB references RLM 65 (130,160,180), dark steel and interior grey.
SLOT_SETTINGS = {
    'Camo': dict(texture='T_Mammut_Splinter', tint=(1, 1, 1), roughness=0.72),
    'Underside': dict(texture=None, tint=(0.22, 0.35, 0.46), roughness=0.7),
    'Interior': dict(texture=None, tint=(0.07, 0.075, 0.07), roughness=0.85),
    'Glass': dict(texture=None, tint=(0.02, 0.035, 0.05), roughness=0.08),
    'Dark': dict(texture=None, tint=(0.025, 0.025, 0.025), roughness=0.55),
}


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
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat


def make_instance(name, parent, texture, tint, roughness):
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
    MEL.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    return mi


def main():
    textures = {}
    for name in ('T_Mammut_Splinter', 'T_Balkenkreuz'):
        textures[name] = import_file(SRC + '/Textures/' + name + '.png', ROOT + '/Textures', name)
    airframe = make_material('M_Mammut_Airframe', masked=False)
    marking = make_material('M_Mammut_Marking', masked=True)
    slot_materials = {}
    for slot, cfg in SLOT_SETTINGS.items():
        texture = textures[cfg['texture']] if cfg['texture'] else None
        slot_materials[slot] = make_instance('MI_Mammut_' + slot, airframe, texture, cfg['tint'], cfg['roughness'])
    slot_materials['Marking'] = make_instance('MI_Mammut_Marking', marking, textures['T_Balkenkreuz'],
                                              (1, 1, 1), 0.6)

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
