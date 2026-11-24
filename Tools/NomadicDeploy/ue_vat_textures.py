# Unreal (editor python): imports a building's baked VAT textures with settings a vertex shader reads exactly:
# RGBA16F (HDR), linear, no mips, never streamed, clamp. Bilinear stays on so neighbouring frames (rows) blend; the
# shader samples texel centres in U so pieces never blend into each other.
# Globals: WORK (building work dir with vat_offset.exr / vat_rot.exr), FOLDER (content folder), BUILDING (label).
import unreal

EAL = unreal.EditorAssetLibrary
EAL.make_directory(FOLDER)
names = {'vat_offset': 'T_%s_DeployVatOffset' % BUILDING, 'vat_rot': 'T_%s_DeployVatRot' % BUILDING}
tasks = []
for src, name in names.items():
    t = unreal.AssetImportTask()
    t.filename = WORK + '/' + src + '.exr'
    t.destination_path = FOLDER
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name in names.values():
    tex = unreal.load_asset(FOLDER + '/' + name)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('never_stream', True)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_16_BIT_DATA)
    print(name, tex.blueprint_get_size_x(), 'x', tex.blueprint_get_size_y(), 'saved', EAL.save_loaded_asset(tex, False))
