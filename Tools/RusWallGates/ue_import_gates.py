"""Unreal editor python: import the Soviet wall gate meshes, atlas and VAT textures (build_gates.py output).

Globals expected (set by the caller before exec): BUILD (dir with the FBX/EXR/PNG files).
Content: /Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/{Meshes,Textures}
"""
import unreal

EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates'
MESHES, TEXTURES = ROOT + '/Meshes', ROOT + '/Textures'
for f in (MESHES, TEXTURES):
    EAL.make_directory(f)


def import_files(pairs, folder):
    existing = set(str(p).split('.')[0] for p in EAL.list_assets(folder, recursive=False))
    tasks = []
    for src, name in pairs:
        t = unreal.AssetImportTask()
        t.filename = BUILD + '/' + src
        t.destination_path = folder
        t.destination_name = name
        t.replace_existing = True
        t.automated = True
        t.save = False
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for ad in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, recursive=False):
        package = str(ad.package_name)
        if package not in existing and str(ad.asset_class_path.asset_name) in ('Material', 'MaterialInstanceConstant'):
            print('deleting imported', package, EAL.delete_asset(package))


# atlas
import_files([('../T_RusWallGate_Atlas.png', 'T_RusWallGate_Atlas')], TEXTURES)
atlas = unreal.load_asset(TEXTURES + '/T_RusWallGate_Atlas')
atlas.set_editor_property('srgb', True)
print('atlas', atlas.blueprint_get_size_x(), EAL.save_loaded_asset(atlas, False))

# VAT textures: RGBA16F, linear, no mips, never streamed, clamp (as NomadicDeploy/ue_vat_textures.py)
vat = []
for k in ('A', 'C'):
    for n in ('VAT_Offset', 'VAT_Rot'):
        vat.append(('T_RusWallGate%s_%s.exr' % (k, n), 'T_RusWallGate%s_%s' % (k, n)))
import_files(vat, TEXTURES)
for _, name in vat:
    tex = unreal.load_asset(TEXTURES + '/' + name)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_HDR)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('never_stream', True)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_16_BIT_DATA)
    print(name, tex.blueprint_get_size_x(), 'x', tex.blueprint_get_size_y(), EAL.save_loaded_asset(tex, False))

# meshes
names = ['SM_RusWallGateA_Frame', 'SM_RusWallGateA_Leaf', 'SM_RusWallGateC_Frame', 'SM_RusWallGateC_Leaf']
import_files([(n + '.fbx', n) for n in names], MESHES)
for n in names:
    mesh = unreal.load_asset(MESHES + '/' + n)
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.enabled = False
    mesh.set_editor_property('nanite_settings', nanite)
    build = SMS.get_lod_build_settings(mesh, 0)
    build.use_full_precision_u_vs = True
    build.generate_lightmap_u_vs = False
    build.recompute_normals = False
    build.recompute_tangents = True
    SMS.set_lod_build_settings(mesh, 0, build)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    print(n, 'verts', SMS.get_number_verts(mesh, 0), 'uv', SMS.get_num_uv_channels(mesh, 0),
          'bounds', mesh.get_bounds().origin, mesh.get_bounds().box_extent,
          'slots', [str(s.material_slot_name) for s in mesh.get_editor_property('static_materials')],
          'saved', EAL.save_loaded_asset(mesh, False))
