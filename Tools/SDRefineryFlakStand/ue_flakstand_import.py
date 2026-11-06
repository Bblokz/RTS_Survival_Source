"""Imports the SD refinery flak stand meshes (run in the editor's Python, e.g. UECP execute_python).

Blender source and rebuild: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/StrikeDivisionBXPs/Refinery_FlakFrame/
Model/README.md. Pattern: the Kaskade import (legacy FBX with Interchange off around it, Nanite off, slot materials by
slot name, complex collision = the _Collision mesh with complex-as-simple, like the Barracks FuelCell).

import_all(): _Collision first, then the main, _At50 and _PreviewMesh meshes. Safe to rerun after a Blender rebuild.
"""
import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/StrikeDivisionBXPs/Refinery_FlakFrame/Model/'
HOME = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Refinery/FlakStand'
NAME = 'SM_GerRefinery_FlakStand'
COLLISION = NAME + '_Collision'
REFINERY_TEX = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/Refinery_Meshes/Textures/'
SLOT_MATERIALS = {
    'M_rusty_pipe_Radixite': REFINERY_TEX + 'M_rusty_pipe_Radixite',
    'M_SandbagScaffoldAtals': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
    'M_PantherII_Sheets_001': '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/PantherII/Textures/M_PantherII_Sheets',
    'M_Old_Concrete_2': REFINERY_TEX + 'M_Old_Concrete_2',
    'MI_ConstructionPrev': '/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_ConstructionPrev',
    'WorldGridMaterial': None,
}
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def import_fbx(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        ASSET_TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return unreal.load_asset(HOME + '/' + name)


def import_one(name):
    mesh = import_fbx(name)
    if mesh is None:
        print('IMPORT FAILED', name)
        return None
    ns = mesh.get_editor_property('nanite_settings')
    if ns.enabled:
        ns.enabled = False
        mesh.set_editor_property('nanite_settings', ns)
    for i, slot in enumerate(mesh.static_materials):
        key = str(slot.material_slot_name)
        path = SLOT_MATERIALS.get(key, 'missing')
        if path == 'missing':
            print('NO MATERIAL FOR SLOT', name, key)
            continue
        if path:
            mesh.set_material(i, unreal.load_asset(path))
    socket = mesh.find_socket('Turret')
    if socket:      # FBX empties can arrive tilted; the turret socket is a plain position
        socket.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))
        socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
    if name != COLLISION:
        col = unreal.load_asset(HOME + '/' + COLLISION)
        if col is not None:
            mesh.set_editor_property('complex_collision_mesh', col)
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    b = mesh.get_bounding_box()
    print('IMPORTED', name, 'saved', ok, 'tris', mesh.get_num_triangles(0), 'bounds', b.min, b.max,
          'socket', socket.get_editor_property('relative_location') if socket else None,
          [(str(s.material_slot_name), s.material_interface.get_name() if s.material_interface else None)
           for s in mesh.static_materials])
    return mesh


def import_all():
    for name in (COLLISION, NAME, NAME + '_At50', NAME + '_PreviewMesh'):
        import_one(name)
