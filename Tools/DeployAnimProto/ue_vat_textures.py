# Unreal (editor python). PROTOTYPE 2: imports the baked VAT textures with settings a vertex shader can read
# exactly: RGBA16F (HDR compression), linear, no mips, never streamed, clamp. Bilinear filtering stays on so
# neighbouring frames (rows) blend; the shader samples texel centres in U so pieces never blend into each other.
import unreal

EAL = unreal.EditorAssetLibrary
FOLDER = '/Game/RTS_Survival/_Prototypes/DeployAnim/VAT/Textures'
SRC = 'D:/UE5Projects/RTS_Survival/Saved/DeployAnimProto/'
EAL.make_directory(FOLDER)
tasks = []
for name in ('T_HQDeploy_VAT_Offset', 'T_HQDeploy_VAT_Rot'):
    t = unreal.AssetImportTask()
    t.filename = SRC + name + '.exr'
    t.destination_path = FOLDER
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name in ('T_HQDeploy_VAT_Offset', 'T_HQDeploy_VAT_Rot'):
    tex = unreal.load_asset(FOLDER + '/' + name)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('never_stream', True)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_16_BIT_DATA)
    print(name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y(), tex.get_editor_property('compression_settings'))
    print('saved', EAL.save_loaded_asset(tex, False))
