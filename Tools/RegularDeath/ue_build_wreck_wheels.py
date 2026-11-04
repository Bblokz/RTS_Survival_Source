"""Builds the WheelThrow regular death assets: imports the road wheels and sprockets cut by blender_cut_wheels.py,
makes one wheel material per nation (M_WreckDebrisChunk with the tracks' wheel atlas) and one Niagara system per
nation, copied from NS_WreckDebrisBurst so it keeps the throw, collision, rest and sink set-up.
Run headless with an editor that has the RTS_Survival editor helper (Development or DebugGame -Cmd):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nop4 -NullRHI
"""
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
FBX_DIR = os.path.join(HERE, 'WheelExport')
DEBRIS_DIR = '/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/'
WHEEL_DIR = DEBRIS_DIR + 'Wheels'
SOURCE_SYSTEM = DEBRIS_DIR + 'NS_WreckDebrisBurst'
CHUNK_MATERIAL = DEBRIS_DIR + 'M_WreckDebrisChunk'
# The tracks' wheel atlases; the cut wheels keep their UVs into these.
TRACK_TEXTURES = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/Textures/'
# blender_cut_wheels.py exports with apply_unit_scale, so the FBX already reads in centimetres.
IMPORT_SCALE = 1.0
# Road wheels three times as often as the sprocket.
NATIONS = {
    'Ger': {'wheel_texture': TRACK_TEXTURES + 'T_PZIV_Tracks_Wheels',
            'meshes': ['SM_WreckWheel_Ger_Road'] * 3 + ['SM_WreckWheel_Ger_Sprocket']},
    'Rus': {'wheel_texture': TRACK_TEXTURES + 'T_Rus_Tracks_Wheels',
            'meshes': ['SM_WreckWheel_Rus_Road'] * 3 + ['SM_WreckWheel_Rus_Sprocket']},
}
# M_WreckDebrisChunk multiplies the colour by this (the chunks use 0.85).
WHEEL_BURNT_DARKENING = 1.0
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def import_wheel(name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(FBX_DIR, name + '.fbx')
    task.destination_path = WHEEL_DIR
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.static_mesh_import_data.combine_meshes = True
    ui.static_mesh_import_data.auto_generate_collision = False
    ui.static_mesh_import_data.build_nanite = False
    ui.static_mesh_import_data.import_uniform_scale = IMPORT_SCALE
    task.options = ui
    asset_tools.import_asset_tasks([task])
    mesh = unreal.load_asset(WHEEL_DIR + '/' + name)
    box = mesh.get_bounding_box()
    print('RTS_WWB mesh', name, 'size_cm', box.max - box.min, 'tris', mesh.get_num_triangles(0))
    return mesh


def make_wheel_material(nation, wheel_texture_path):
    texture = unreal.load_asset(wheel_texture_path)
    name = 'MI_WreckWheel_' + nation
    path = WHEEL_DIR + '/' + name
    material = unreal.load_asset(path) if eal.does_asset_exist(path) else asset_tools.create_asset(
        name, WHEEL_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    material.set_editor_property('parent', unreal.load_asset(CHUNK_MATERIAL))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material, 'HullBaseColor', texture)
    # The wheel atlas is already dark steel and rubber; the chunk's burnt darkening turned the wheels black.
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(material, 'BurntDarkening',
                                                                              WHEEL_BURNT_DARKENING)
    unreal.MaterialEditingLibrary.update_material_instance(material)
    print('RTS_WWB material', name, 'texture', texture.get_path_name() if texture else None)
    return material


def make_system(nation, meshes, material):
    path = DEBRIS_DIR + 'NS_WreckWheelBurst_' + nation
    if not eal.does_asset_exist(path):
        eal.duplicate_asset(SOURCE_SYSTEM, path)
    system = unreal.load_asset(path)
    ok = unreal.WreckDebrisAssetTools.setup_debris_mesh_renderer(system, meshes, [1.0] * len(meshes),
                                                                 'DebrisMaterial', material)
    print('RTS_WWB system', path, 'setup', ok)
    return path


saved = []
for nation, spec in NATIONS.items():
    meshes = {name: import_wheel(name) for name in sorted(set(spec['meshes']))}
    material = make_wheel_material(nation, spec['wheel_texture'])
    system_path = make_system(nation, [meshes[name] for name in spec['meshes']], material)
    saved += [WHEEL_DIR + '/' + name for name in meshes] + [material.get_path_name().split('.')[0], system_path]
for path in saved:
    print('RTS_WWB saved', path, eal.save_asset(path, False))
