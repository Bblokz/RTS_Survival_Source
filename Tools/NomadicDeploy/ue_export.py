# Unreal (editor python): exports a building mesh to FBX for the Blender half of the pipeline.
# Globals: SOURCE_MESH, OUT (file path; its folder must exist or the export "succeeds" without a file).
import unreal

t = unreal.AssetExportTask()
t.object = unreal.load_asset(SOURCE_MESH)
t.filename = OUT
t.exporter = unreal.StaticMeshExporterFBX()
t.automated = True
t.prompt = False
t.replace_identical = True
opts = unreal.FbxExportOption()
opts.collision = False
opts.level_of_detail = False
t.options = opts
print('export', SOURCE_MESH, '->', OUT, unreal.Exporter.run_asset_export_task(t))
