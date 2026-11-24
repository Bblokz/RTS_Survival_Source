"""Imports the supply point meshes (concept B, model v01) and makes MI_Emblem_DirectorOfHarvest.

Blender source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SupplyPoint (SupplyPoint_Brief.md).
Headless, new assets only (the open editor must not have them loaded):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
      -DisablePlugins=Fab,BpGeneratorUltimate
Legacy FBX import (Interchange drops UCX hulls). Existing materials only. Prints SUPPLYMESH lines; the last one is
SUPPLYMESH RESULT OK|FAIL.
"""
import math

import unreal

FBX = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SupplyPoint/FBX/'
HOME = globals().get('SUPPLY_HOME', '/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Meshes')
# A caller may exec this file with ONLY = [mesh names] to reimport a subset, and SUPPLY_IMPORT_LIBRARY = True to get
# the functions without running main (ue_update_supply_point_v02.py, ue_render_supply_point_v02.py).
ONLY = globals().get('ONLY')
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/'
EMBLEMS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/Emblems/'
WEAR = '/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Materials/'
# Keys are the FBX slot names (the Blender material names); the crane slots get the supply-point-only worn materials of
# the wear pass (Source/Tools/SupplyPoint/Wear, 2026-11-17) instead of the shared Breakthrough M_BF_* ones.
MATERIALS = {
    'M_BF_Trim': WEAR + 'MI_SupplyPoint_Timber',
    'M_BF_Crane_PaintedSteel': WEAR + 'MI_SupplyPoint_CraneSteel',
    'M_BF_Crane_WornSteel': WEAR + 'MI_SupplyPoint_Rope',
    'M_BF_ResourceFill': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughHQ/Fills/M_BF_ResourceFill',
    'MI_Crystal1': '/Game/RTS_Survival/Blueprints/Resources/Crystals/Textures/MI_Crystal1',
    'MI_Emblem_DirectorOfShield': EMBLEMS + 'MI_Emblem_DirectorOfShield',
    # v02: the game's metal-resource shader (harvestable metal nodes), on the metal fills and the metal load
    'M_RTS_MetalResource': '/Game/RTS_Survival/Blueprints/Resources/Metal/Meshes/Materials/M_RTS_MetalResource',
}
PARTS = ['SM_SupplyPoint_Base', 'SM_SupplyPoint_UpperWorks', 'SM_SupplyPoint_Trolley', 'SM_SupplyPoint_Rope',
         'SM_SupplyPoint_HookBlock', 'SM_SupplyPoint_Flag']
LOADS = ['SM_SupplyLoad_Radixite', 'SM_SupplyLoad_Metal', 'SM_SupplyLoad_VehicleParts']
FILLS = [f'SM_SupplyPoint_{k}At{lv}' for k in ('Radixite', 'Metal', 'VehicleParts') for lv in (10, 25, 50, 75, 100)]
# Intended socket transforms in Unreal (cm, yaw), from sp_layout.py: UE X = 100 x, UE Y = -100 y, UE yaw = -yaw.
EXPECTED_SOCKETS = {
    'SM_SupplyPoint_Base': {
        'Slew': ((0.0, -220.0, 675.0), 0.0),
        'FlagLowered': ((-694.4, -790.2, 220.0), -215.8),
        'Stock_Radixite': ((-554.4, -421.8, 12.0), -70.0),
        'Stock_Metal': ((0.0, -810.0, 12.0), 0.0),
        'Stock_VehicleParts': ((554.4, -421.8, 12.0), 70.0),
        'Pick_Radixite': ((-361.8, -351.7, 12.0), -160.0),
        'Pick_Metal': ((0.0, -605.0, 12.0), -90.0),
        'Pick_VehicleParts': ((361.8, -351.7, 12.0), -20.0),
        'TruckApproach': ((-1100.0, -150.0, 0.0), 0.0),
        'TruckPark': ((125.0, 330.0, 0.0), 0.0),
        'TruckExit': ((1350.0, -150.0, 0.0), 0.0),
    },
    'SM_SupplyPoint_UpperWorks': {'Trolley': ((0.0, 0.0, 0.0), 0.0)},
    'SM_SupplyPoint_Trolley': {'Rope': ((0.0, 0.0, 0.0), 0.0)},
    'SM_SupplyPoint_HookBlock': {'Load': ((0.0, 0.0, -230.0), 0.0)},
}
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
OK = [True]


def log(message):
    unreal.log('SUPPLYMESH ' + message)


def fail(message):
    OK[0] = False
    log('FAIL ' + message)


def folder(name):
    if name in LOADS:
        return HOME + '/Loads'
    if name in FILLS:
        return HOME + '/Fills'
    return HOME


def import_mesh(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = FBX + name + '.FBX'
    task.destination_path = folder(name)
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    TOOLS.import_asset_tasks([task])
    return unreal.load_asset(folder(name) + '/' + name)


def set_materials(mesh):
    """Struct elements of static_materials come back as copies, so assign through set_material by index."""
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in MATERIALS if slot_name.startswith(k)), None)
        if key is None:
            fail(f'{mesh.get_name()} slot {slot_name} has no material mapping')
            continue
        mesh.set_material(index, unreal.load_asset(MATERIALS[key]))


def settle(mesh):
    """Nanite off, one LOD, simple collision only where the FBX brought UCX hulls (the base)."""
    nanite = mesh.get_editor_property('nanite_settings')
    nanite.set_editor_property('enabled', False)
    mesh.set_editor_property('nanite_settings', nanite)
    body = mesh.get_editor_property('body_setup')
    if body:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_DEFAULT)


def rot_close(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0) < 0.5


def check_sockets(mesh):
    expected = EXPECTED_SOCKETS.get(mesh.get_name(), {})
    sockets = {str(s.get_editor_property('socket_name')): s for s in mesh.get_sockets_by_tag('')}
    for name, socket in sockets.items():
        loc = socket.get_editor_property('relative_location')
        rot = socket.get_editor_property('relative_rotation')
        tilted = abs(rot.roll) > 0.5 or abs(rot.pitch) > 0.5
        want = expected.get(name)
        note = ''
        if want:
            (wx, wy, wz), wyaw = want
            if math.dist((loc.x, loc.y, loc.z), (wx, wy, wz)) > 1.0:
                note += f' LOCATION_OFF(want {wx},{wy},{wz})'
            if tilted or not rot_close(rot.yaw, wyaw):
                note += f' ROTATION_FIXED(was r{rot.roll:.1f} p{rot.pitch:.1f} y{rot.yaw:.1f})'
                socket.set_editor_property('relative_rotation', unreal.Rotator(0.0, 0.0, wyaw))
                rot = socket.get_editor_property('relative_rotation')
        else:
            note = ' UNEXPECTED'
        log(f'socket {mesh.get_name()}.{name} loc ({loc.x:.1f}, {loc.y:.1f}, {loc.z:.1f}) '
            f'rot (r{rot.roll:.1f} p{rot.pitch:.1f} y{rot.yaw:.1f}){note}')
        if 'LOCATION_OFF' in note or 'UNEXPECTED' in note:
            fail(f'socket {name} on {mesh.get_name()}')
    for name in expected:
        if name not in sockets:
            fail(f'missing socket {name} on {mesh.get_name()}')


def report(mesh):
    tris = mesh.get_num_triangles(0) if hasattr(mesh, 'get_num_triangles') else -1
    hulls = SMS.get_convex_collision_count(mesh)
    boxes = SMS.get_simple_collision_count(mesh) if hasattr(SMS, 'get_simple_collision_count') else -1
    mats = [(str(s.get_editor_property('material_slot_name')),
             s.get_editor_property('material_interface').get_name() if s.get_editor_property('material_interface') else None)
            for s in mesh.get_editor_property('static_materials')]
    bounds = mesh.get_bounding_box()
    log(f'mesh {mesh.get_path_name()} tris {tris} lods {mesh.get_num_lods()} convex {hulls} simple {boxes} '
        f'nanite {mesh.get_editor_property("nanite_settings").get_editor_property("enabled")} materials {mats} '
        f'bounds ({bounds.min.x:.0f},{bounds.min.y:.0f},{bounds.min.z:.0f})..({bounds.max.x:.0f},{bounds.max.y:.0f},{bounds.max.z:.0f})')
    if mesh.get_name() == 'SM_SupplyPoint_Base' and hulls != 6:
        fail(f'base has {hulls} convex hulls, want 6')


def harvest_emblem():
    path = EMBLEMS + 'MI_Emblem_DirectorOfHarvest'
    texture = unreal.load_asset('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/'
                                'Textures/Emblems/T_Propaganda_DirectorOfHarvest')
    shield = unreal.load_asset(EMBLEMS + 'MI_Emblem_DirectorOfShield')
    if EAL.does_asset_exist(path):
        log('emblem exists already: ' + path)
        return
    mi = TOOLS.create_asset('MI_Emblem_DirectorOfHarvest', EMBLEMS.rstrip('/'), unreal.MaterialInstanceConstant,
                            unreal.MaterialInstanceConstantFactoryNew())
    edit = unreal.MaterialEditingLibrary
    edit.set_material_instance_parent(mi, shield.get_editor_property('parent'))
    for value in shield.get_editor_property('scalar_parameter_values'):
        edit.set_material_instance_scalar_parameter_value(
            mi, value.get_editor_property('parameter_info').get_editor_property('name'),
            value.get_editor_property('parameter_value'))
    edit.set_material_instance_texture_parameter_value(mi, 'Banner', texture)
    edit.update_material_instance(mi)
    saved = EAL.save_loaded_asset(mi, False)
    got = edit.get_material_instance_texture_parameter_value(mi, 'Banner')
    log(f'emblem {path} saved={saved} Banner={got.get_name() if got else None}')
    if not saved or got is None:
        fail('emblem')


def main():
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in [n for n in PARTS + LOADS + FILLS if ONLY is None or n in ONLY]:
            mesh = import_mesh(name)
            if mesh is None:
                fail('import ' + name)
                continue
            set_materials(mesh)
            settle(mesh)
            check_sockets(mesh)
            if not EAL.save_loaded_asset(mesh, False):
                fail('save ' + name)
            report(mesh)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    if ONLY is None:
        harvest_emblem()


if not globals().get('SUPPLY_IMPORT_LIBRARY'):
    main()
    log('RESULT ' + ('OK' if OK[0] else 'FAIL'))
