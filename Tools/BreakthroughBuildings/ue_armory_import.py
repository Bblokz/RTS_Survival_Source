"""Imports the Breakthrough Armory, design H Feldbahn (run with the UECP execute_python action).

Blender source, socket table and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/
Armory/Model/README.md. The main mesh is concrete and trim; the ammunition is three dressing meshes, one per atlas (rounds on the
faction's shared ammo atlas, MG cans and shell carriers on FieldSupply, crates on the faction's Dunkelgelb props instance MI_BF_Props_1).

Safe to rerun after a Blender rebuild: every mesh reimports over the same asset and the placement preview is copied again.
Run ue_building_lamps_import.py (BUILDINGS_TO_RUN = ['Armory']) afterwards: it imports the lamps and gives the concrete slot
the Armory's own lamp-pool instance, which this script puts back to the plain UVTile.
"""
import json

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/Armory/Model/'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
HOME = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughArmory/Mesh'
MAIN = 'SM_BF_Armory'
DRESSING = ('Rounds', 'FieldSupply', 'Crates')
PREVIEW = MAIN + '_Preview'
PREVIEW_MATERIAL = '/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_ConstructionPrev'
# A copy of Model/SM_BF_Armory_Sockets.json: the editor's Python may only open files inside the project. Copy it again after a rebuild.
SOCKETS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/SM_BF_Armory_Sockets.json'
# FBX slot name prefix -> material. The dressing slots carry the Blender material's name, 'arm_' plus the atlas file name.
MATERIALS = dict(M_BF_SootedFortress_Concrete=SHARED + '/MI_BF_SootedFortress_UVTile', M_BF_Trim=SHARED + '/M_BF_Trim',
                 arm_T_Ammo_Shared=SHARED + '/M_Ammo_FactionShared', arm_T_BF_FieldSupply=SHARED + '/M_BF_FieldSupply',
                 arm_T_Props_1=SHARED + '/MI_BF_Props_1')    # the faction's Dunkelgelb copy (ue_bf_props_materials.py)
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
MESHES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    # save_loaded_asset reports False on a fresh import now and then; save_packages doesn't
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):    # a save can report success and still miss the disk
        raise RuntimeError('not on disk ' + disk)


def import_mesh(name):
    """Legacy FBX: Interchange drops UCX hulls and ignores FbxImportUI."""
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False    # AssetImportTask(save=True) silently misses the disk here
    task.replace_existing = True
    task.options = ui
    ASSET_TOOLS.import_asset_tasks([task])


def finish_mesh(name):
    path = f'{HOME}/{name}'
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('import made nothing at ' + path)
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in MATERIALS if slot_name.startswith(k)), None)    # the importer may change the '.png' ending
        if key is None:
            raise RuntimeError(f'no material for slot {slot_name} on {name}')
        mesh.set_material(index, unreal.load_asset(MATERIALS[key]))    # setting it on the struct only changes a copy
    if MESHES.get_num_uv_channels(mesh, 0) >= 2:       # every Armory mesh carries LightMapUV second; a missing channel would break lighting
        mesh.set_editor_property('light_map_coordinate_index', 1)
    save(path)
    return mesh


def set_sockets(mesh):
    """FBX empties arrive tilted by 90 degrees; the positions are right, the yaws come from the json."""
    rows = json.loads(open(SOCKETS, encoding='utf-8').read())
    for name, row in rows.items():
        socket = mesh.find_socket(name)
        if socket is None:
            raise RuntimeError('missing socket ' + name)
        socket.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=row['yaw']))
        socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
        where = socket.get_editor_property('relative_location')
        off = max(abs(where.x - row['location_cm'][0]), abs(where.y - row['location_cm'][1]), abs(where.z - row['location_cm'][2]))
        if off > 1.0:
            raise RuntimeError(f'socket {name} is {off:.1f} cm off the json')
    save(f'{HOME}/{MAIN}')
    print('sockets set', len(rows))


def make_preview():
    """Placement ghost for the truck: the main mesh with every slot on the construction preview material."""
    path = f'{HOME}/{PREVIEW}'
    if ASSETS.does_asset_exist(path):
        ASSETS.delete_asset(path)
    if ASSETS.duplicate_asset(f'{HOME}/{MAIN}', path) is None:
        raise RuntimeError('preview copy failed')
    mesh = unreal.load_asset(path)
    ghost = unreal.load_asset(PREVIEW_MATERIAL)
    for index in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(index, ghost)
    save(path)
    return mesh


def report(mesh):
    box = mesh.get_bounding_box()
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    print(mesh.get_name(), 'tris', mesh.get_num_triangles(0), 'convex', hulls, 'uv', MESHES.get_num_uv_channels(mesh, 0),
          'bounds', [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
          'materials', [m.get_name() for m in (mesh.get_material(i) for i in range(len(mesh.get_editor_property('static_materials'))))])


def main():
    names = [MAIN] + [f'{MAIN}_{part}' for part in DRESSING]
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in names:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    main_mesh = finish_mesh(MAIN)
    set_sockets(main_mesh)
    report(main_mesh)
    for name in names[1:]:
        report(finish_mesh(name))
    report(make_preview())


main()
