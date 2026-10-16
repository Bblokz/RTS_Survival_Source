"""Imports the RedLine field fortifications built by redline_fieldworks.py (Blender) into the editor.

Run as a commandlet:  UnrealEditor-Cmd <uproject> -run=pythonscript -script=<this file> -unattended -NullRHI
Meshes land in /Game/RTS_Survival/Blueprints/Environment/RedLine/Meshes with the game's earth, timber, sandbag,
concrete and steel materials, their sockets (cargo_N trench cover, crouch_N, TurretMount) and complex collision
so trench floors stay walkable while the parapets block.

The game does not render Nanite: after (re)importing a pit or fieldwork mesh, cut it with
Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py and run Tools/GunPits/optimize_pit_meshes.py heavy, which
imports the cut mesh and turns Nanite off (see Docs/Units/GunPitMeshes.md).
"""
import unreal

EXPORT_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/RedLine'
MESH_DIR = '/Game/RTS_Survival/Blueprints/Environment/RedLine/Meshes'
MESHES = ['SM_RL_Trench_Straight', 'SM_RL_Trench_Bend', 'SM_RL_CommTrench', 'SM_RL_TrenchEnd', 'SM_RL_MGNest',
          'SM_RL_GunBay', 'SM_RL_TurretPillbox', 'SM_RL_Dugout']
# Pieces carrying hundreds of sandbags are thinned with distance.
HEAVY_MESHES = {'SM_RL_MGNest', 'SM_RL_GunBay', 'SM_RL_Trench_Straight', 'SM_RL_Trench_Bend', 'SM_RL_TrenchEnd'}
MATERIALS = {
    'RL_Earth': '/Game/RTS_Survival/Blueprints/Environment/GroundEnvActors/Desert/MI_RTS_DesertGround_Stones',
    'RL_Timber': '/Game/RTS_Survival/StarterContent/Materials/M_Wood_Pine',
    'RL_Sandbag': '/Game/SandBagPack/Materials/MI_Rus_SandBag_01',
    'RL_Concrete': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/MI_Rus_BunkerConcrete_05',
    'RL_Steel': '/Game/RTS_Survival/StarterContent/Materials/M_Metal_Burnished_Steel',
}


def import_mesh(name):
    task = unreal.AssetImportTask()
    task.filename = '%s/%s.fbx' % (EXPORT_DIR, name)
    task.destination_path = MESH_DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property('combine_meshes', True)
    options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
    options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs', False)
    task.options = options
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = '%s/%s' % (MESH_DIR, name)
    mesh = unreal.load_asset(path)
    for index, slot in enumerate(mesh.static_materials):
        target = MATERIALS.get(str(slot.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if name in HEAVY_MESHES:
        mesh.set_editor_property('lod_group', 'LargeProp')
    component = unreal.StaticMeshComponent()
    component.set_static_mesh(mesh)
    sockets = [str(s) for s in component.get_all_socket_names()]
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    unreal.log_warning('RLIMPORT %s sockets=%d %s saved=%s' % (name, len(sockets), sockets[:4], saved))


for mesh_name in MESHES:
    import_mesh(mesh_name)
