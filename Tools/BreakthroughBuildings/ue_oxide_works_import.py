"""Imports the Breakthrough Oxide Works (the refinery) meshes (run with the UECP execute_python action).

Blender source, socket table and rebuild steps: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/
OxideWorks/Model/README.md. The concrete gets its own instance, MI_BF_OxideWorks_Concrete, a child of the faction's
MI_BF_SootedFortress_UVTile with the building's hot spots set, so heat and soot gather round the melt. The glow slot
gets the molten slag, M_BF_OxideGlow.

Safe to rerun after a Blender rebuild: the meshes reimport over the same assets. Copy the build's
SM_BF_OxideWorks_Sockets.json, SM_BF_OxideWorks_HeatSources.json and SM_BF_OxideWorks_Lamps.json (SharedParts/bf_lamps.py)
next to this script first: the editor's Python may only open files inside the project.
"""
import importlib
import json
import sys

import unreal

if 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings' not in sys.path:
    sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings')
import bf_ue_lamps  # noqa: E402
importlib.reload(bf_ue_lamps)     # the editor's Python keeps modules between runs

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/OxideWorks/Model/'
HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
HOME = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughOxideWorks/Mesh'
MAIN = 'SM_BF_OxideWorks'
CONCRETE = HOME + '/MI_BF_OxideWorks_Concrete'
PARTS = ('', '_Sandbags', '_Stirrer', '_SkipCar', '_DockGlow1', '_DockGlow2', '_FieldSupply', '_Lamps')
MATERIALS = dict(M_BF_SootedFortress_Concrete=CONCRETE, M_BF_Trim=SHARED + '/M_BF_Trim', M_BF_OxideGlow=SHARED + '/M_BF_OxideGlow',
                 M_BF_FieldSupply=SHARED + '/M_BF_FieldSupply',
                 M_GerSandBagScaffold_Atlas='/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
                 **bf_ue_lamps.MATERIALS)
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
EDIT = unreal.MaterialEditingLibrary


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    # save_asset and save_loaded_asset report False on some of these packages; save_packages doesn't
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):
        raise RuntimeError('not on disk ' + disk)


def concrete_instance():
    """The faction concrete with this building's hot spots. Rerunning updates them."""
    if unreal.load_asset(CONCRETE) is None:
        ASSET_TOOLS.create_asset(CONCRETE.rsplit('/', 1)[1], HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(CONCRETE)
    EDIT.set_material_instance_parent(mi, unreal.load_asset(SHARED + '/MI_BF_SootedFortress_UVTile'))
    data = json.loads(open(HERE + MAIN + '_HeatSources.json', encoding='utf-8').read())
    EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseHeatSoot', True)    # compiled in only where used
    for k in range(8):
        row = data['heat'][k] if k < len(data['heat']) else dict(pos=[0, 0, 0], radius=0)
        EDIT.set_material_instance_vector_parameter_value(mi, f'BF_HeatSource{k}', unreal.LinearColor(*row['pos'], row['radius']))
    for k in range(2):
        row = data['soot'][k] if k < len(data['soot']) else dict(pos=[0, 0, 0], radius=0)
        EDIT.set_material_instance_vector_parameter_value(mi, f'BF_SootSource{k}', unreal.LinearColor(*row['pos'], row['radius']))
    lamps = bf_ue_lamps.apply_lamps(mi, HERE + MAIN + '_Lamps.json')
    save(CONCRETE)
    print('concrete instance', len(data['heat']), 'heat sources', len(data['soot']), 'soot sources', lamps, 'lamp pools')


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
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in MATERIALS if slot_name.startswith(k)), None)    # Blender may add a .001 suffix
        if key is None:
            raise RuntimeError(f'no material for slot {slot_name} on {name}')
        mesh.set_material(index, unreal.load_asset(MATERIALS[key]))    # setting it on the struct only changes a copy
    mesh.set_editor_property('light_map_coordinate_index', 1)
    save(path)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    print(name, 'tris', mesh.get_num_triangles(0), 'convex', hulls,
          'slots', [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')])
    return mesh


def set_sockets(mesh):
    """FBX empties arrive tilted by 90 degrees; the positions are right, the yaws come from the json."""
    rows = json.loads(open(HERE + MAIN + '_Sockets.json', encoding='utf-8').read())
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


def main():
    concrete_instance()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for part in PARTS:
            import_mesh(MAIN + part)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    set_sockets(finish_mesh(MAIN))
    for part in PARTS[1:]:
        finish_mesh(MAIN + part)


def lamps_only():
    """Reimport just the lamps mesh: the concrete instance (and its shader permutation) stays untouched."""
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        import_mesh(MAIN + '_Lamps')
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    finish_mesh(MAIN + '_Lamps')
    print('lamps reimported', MAIN)


# LAMPS_ONLY = True before exec reimports only the lamps mesh
if globals().get('LAMPS_ONLY'):
    lamps_only()
else:
    main()
