"""Imports the Träger C game model and its skin textures (run in the editor, or DebugGame-Cmd -run=pythonscript).

Meshes come from D:/Blender/BlenderProjects/BlenderTanks/Planes/SuperHeavyTransport/Export/TraegerC
(Model/model_traeger_c.py), textures from its Textures folder (Model/make_traeger_textures.py). Mesh slots carry the
Mammut names (Camo, Underside, Interior, Glass, Dark, Marking). The painted slots get MI_TraegerC_* instances: children
of the matching MI_Mammut_* that swap in the Träger C detail normal and mask. Camo and Underside also turn on
LargeCamo: the 32 m T_TraegerC_Camo on UV channel 1 (white for the underside) times the 6 m T_TraegerC_PaintDetail,
so the splinter doesn't repeat across the wing. Glass and Marking use the Mammut instances as they are. TraegerC_sockets.json beside the FBX files holds every pivot in Unreal centimetres
relative to the airframe pivot (nose tips, ground).
"""
import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/Planes/SuperHeavyTransport/Export/TraegerC'
ROOT = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/TraegerC'
MAMMUT_MATERIALS = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut/Materials/MI_Mammut_'
MESHES = ['SM_TraegerC_Airframe', 'SM_TraegerC_Prop', 'SM_TraegerC_BowFairing', 'SM_TraegerC_TailFairing',
          'SM_TraegerC_Spreader', 'SM_TraegerC_Cable']
SLOTS = ['Camo', 'Underside', 'Interior', 'Glass', 'Dark', 'Marking']
OWN_SLOTS = ['Camo', 'Underside', 'Interior', 'Dark']
LARGE_CAMO_SLOTS = ['Camo', 'Underside']
# Texture -> (sRGB, compression).
TEXTURES = {
    'T_TraegerC_Camo': (True, unreal.TextureCompressionSettings.TC_DEFAULT),
    'T_TraegerC_PaintDetail': (False, unreal.TextureCompressionSettings.TC_DEFAULT),
    'T_TraegerC_Detail_N': (False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    'T_TraegerC_Detail_M': (False, unreal.TextureCompressionSettings.TC_MASKS),
}
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning('TRAEGER_IMPORT ' + msg)


def save(asset):
    """save_loaded_asset can report success for new assets without writing them (revision control is on), so save
    by path with only_if_is_dirty off; check the .uasset files on disk afterwards."""
    path = asset.get_path_name().split('.')[0]
    if not unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False):
        log('FAILED to save ' + path)


def import_file(path, folder, name):
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', folder)
    task.set_editor_property('destination_name', name)
    task.set_editor_property('automated', True)
    task.set_editor_property('replace_existing', True)
    task.set_editor_property('save', True)
    TOOLS.import_asset_tasks([task])
    return unreal.load_asset(folder + '/' + name)


def import_textures():
    textures = {}
    for name, (srgb, compression) in TEXTURES.items():
        texture = import_file(SRC + '/Textures/' + name + '.png', ROOT + '/Textures', name)
        texture.set_editor_property('srgb', srgb)
        texture.set_editor_property('compression_settings', compression)
        save(texture)
        textures[name] = texture
    return textures


def make_instance(slot, textures):
    name = 'MI_TraegerC_' + slot
    path = ROOT + '/Materials/' + name
    mi = unreal.load_asset(path)  # also finds an instance made earlier that never reached the disk
    if mi is None:
        mi = TOOLS.create_asset(name, ROOT + '/Materials', unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', unreal.load_asset(MAMMUT_MATERIALS + slot))
    MEL.set_material_instance_texture_parameter_value(mi, 'DetailNormal', textures['T_TraegerC_Detail_N'])
    MEL.set_material_instance_texture_parameter_value(mi, 'DetailMask', textures['T_TraegerC_Detail_M'])
    if slot in LARGE_CAMO_SLOTS:
        MEL.set_material_instance_static_switch_parameter_value(mi, 'LargeCamo', True)
        MEL.set_material_instance_texture_parameter_value(mi, 'PaintDetail', textures['T_TraegerC_PaintDetail'])
    if slot == 'Camo':
        MEL.set_material_instance_texture_parameter_value(mi, 'BaseTexture', textures['T_TraegerC_Camo'])
    MEL.update_material_instance(mi)
    save(mi)
    return mi


def main():
    textures = import_textures()
    slot_materials = {slot: unreal.load_asset(MAMMUT_MATERIALS + slot) for slot in SLOTS}
    for slot in OWN_SLOTS:
        slot_materials[slot] = make_instance(slot, textures)
    for mesh_name in MESHES:
        mesh = import_file(SRC + '/' + mesh_name + '.fbx', ROOT + '/Meshes', mesh_name)
        for index, static_material in enumerate(mesh.get_editor_property('static_materials')):
            slot = str(static_material.get_editor_property('material_slot_name')).split('.')[0]
            if slot in slot_materials:
                mesh.set_material(index, slot_materials[slot])
            else:
                log('unknown slot %s on %s' % (slot, mesh_name))
        save(mesh)
        box = mesh.get_bounding_box()
        log('mesh %s bounds min=%s max=%s' % (mesh_name, box.min, box.max))

    # The FBX import also made plain materials named after the slots; nothing uses them now.
    for asset_path in unreal.EditorAssetLibrary.list_assets(ROOT + '/Meshes', recursive=False):
        if isinstance(unreal.load_asset(asset_path), unreal.Material):
            unreal.EditorAssetLibrary.delete_asset(asset_path)
            log('removed import material ' + asset_path)
    log('DONE')


main()
