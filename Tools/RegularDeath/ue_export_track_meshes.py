"""Exports the Panzer IV and T-34 track skeletal meshes (tracks plus road wheels) to FBX so Blender can cut one road
wheel out of each for the WheelThrow regular death. Run headless:
  UnrealEditor-Cmd.exe RTS_Survival.uproject -ExecutePythonScript=<this file> -unattended -nop4 -RenderOffscreen
Not as a -run=pythonscript commandlet: the skeletal FBX exporter needs a rendered skinned mesh and asserts there.
"""
import os
import unreal

OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'WheelExport')
ROOT = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
MESHES = {
    'PanzerIV_Tracks': ROOT + 'Ger_Medium/PanzerIV/Meshes/SK_PanzerIV_Tracks',
    'T34_Tracks': ROOT + 'Rus_Medium/T34/Meshes/SK_T34_Tracks',
}

for label, path in MESHES.items():
    mesh = unreal.load_asset(path)
    if mesh is None:
        print('RTS_WEX missing', path)
        continue
    for index, material in enumerate(mesh.get_editor_property('materials')):
        interface = material.get_editor_property('material_interface')
        print('RTS_WEX', label, 'slot', index, material.get_editor_property('material_slot_name'),
              interface.get_path_name() if interface else None)
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = os.path.join(OUT_DIR, label + '.fbx')
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.SkeletalMeshExporterFBX()
    options = unreal.FbxExportOption()
    options.collision = False
    options.level_of_detail = False
    # The skeletal FBX exporter builds material-baking mesh data anyway; run with -RenderOffscreen, never -NullRHI.
    options.bake_material_inputs = unreal.FbxMaterialBakeMode.DISABLED
    task.options = options
    print('RTS_WEX export', label, unreal.Exporter.run_asset_export_task(task), task.filename)

# Run as -ExecutePythonScript in a full (offscreen) editor; leave it once done.
unreal.SystemLibrary.quit_editor()
