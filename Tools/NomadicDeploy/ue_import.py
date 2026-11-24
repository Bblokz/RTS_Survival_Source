# Unreal (editor python): imports a building's VAT deploy FBX as a NEW static mesh (SM_<Building>_Deploy).
# Globals: FBX, FOLDER, NAME, SOURCE_MESH. Nanite off, full precision UVs (piece data lives in UV4-5), no lightmap
# generation (keeps the source mesh's lightmap channel). Materials/textures the import creates are deleted; slot
# materials are assigned afterwards by ue_deploy_mis.py.
import unreal

EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
fbx_path, folder, name = FBX, FOLDER, NAME
EAL.make_directory(folder)
existing = set(str(p).split('.')[0] for p in EAL.list_assets(folder, recursive=False))
task = unreal.AssetImportTask()
task.filename = fbx_path
task.destination_path = folder
task.destination_name = name
task.replace_existing = True
task.automated = True
task.save = False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
UNWANTED = ('Material', 'MaterialInstanceConstant', 'Texture2D')
for ad in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, recursive=False):
    package = str(ad.package_name)
    if package not in existing and str(ad.asset_class_path.asset_name) in UNWANTED:
        print('deleting imported', package, EAL.delete_asset(package))
mesh = unreal.load_asset(folder + '/' + name)
nanite = mesh.get_editor_property('nanite_settings')
nanite.enabled = False
mesh.set_editor_property('nanite_settings', nanite)
build = SMS.get_lod_build_settings(mesh, 0)
build.use_full_precision_u_vs = True
build.generate_lightmap_u_vs = False
build.recompute_normals = False
build.recompute_tangents = False
SMS.set_lod_build_settings(mesh, 0, build)
# UV4-5 hold the VAT piece data: a source lightmap channel there (meshes with 6 UV sets) falls back to UV3
mesh.set_editor_property('light_map_coordinate_index',
                         min(unreal.load_asset(SOURCE_MESH).get_editor_property('light_map_coordinate_index'), 3))
print(name, 'verts', SMS.get_number_verts(mesh, 0), 'uv channels', SMS.get_num_uv_channels(mesh, 0),
      'nanite', mesh.get_editor_property('nanite_settings').enabled, 'bounds', mesh.get_bounds().box_extent)
print('slots', [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')])
print('saved', EAL.save_loaded_asset(mesh, False))
