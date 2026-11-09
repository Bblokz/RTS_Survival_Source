"""Imports the Soviet field trench kit (CustomMapItems/SovietsStuff/FieldTrenches/generate_sov_field_trenches.py).

  Textures/T_SovFieldTrench_Wood_D, _N   silvered pine log atlas (generate_sov_trench_wood.py), normal map DirectX
  Materials/M_SovFieldTrench_Wood        base colour + normal, rough, not metallic
  Meshes/SM_SovFieldTrench_*             one static mesh per kit piece, same pieces/ports/cargo_N sockets as the German
                                         kit (FieldTrenchLayout::GetPieceMeshPath(Piece, EFieldTrenchKit::Soviet)):
                                         slots SovTrench_Wood -> M_SovFieldTrench_Wood, RL_Sandbag -> MI_Rus_SandBag_01
                                         (the Soviet gun pits' bags); complex collision, LOD group LargeProp, no Nanite.

Mesh settings and the socket fix-up (pure yaw from the manifest; Unreal yaw = -Blender yaw) are the German import's
(ue_field_trench_import.py). sov_field_trench_kit.json is a copy of the Blender Exports manifest: copy it again after
regenerating the kit.

Run inside the editor (execute_python): exec(open(<this file>).read()) with STEPS set, e.g. STEPS = ['textures',
'material', 'meshes', 'report'].
"""
import json

import unreal

BLENDER_ROOT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/FieldTrenches'
BLENDER_EXPORTS = BLENDER_ROOT + '/Exports'
MANIFEST = 'D:/UE5Projects/RTS_Survival/Source/Tools/FieldTrenches/sov_field_trench_kit.json'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Trenches/SovFieldTrench'
MESH_DIR = ROOT + '/Meshes'
TEXTURE_DIR = ROOT + '/Textures'
MATERIAL_DIR = ROOT + '/Materials'
WOOD_MATERIAL = MATERIAL_DIR + '/M_SovFieldTrench_Wood'
SANDBAG_MATERIAL = '/Game/SandBagPack/Materials/MI_Rus_SandBag_01'
MATERIALS = {'SovTrench_Wood': WOOD_MATERIAL, 'RL_Sandbag': SANDBAG_MATERIAL}
WOOD_ROUGHNESS = 0.85
WOOD_BRIGHTNESS = 1.25
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

try:
    STEPS
except NameError:
    STEPS = ['textures', 'material', 'meshes', 'report']


def import_textures():
    for name, is_normal in (('T_SovFieldTrench_Wood_D', False), ('T_SovFieldTrench_Wood_N', True)):
        task = unreal.AssetImportTask()
        task.filename = f'{BLENDER_ROOT}/Textures/{name}.png'
        task.destination_path = TEXTURE_DIR
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        asset_tools.import_asset_tasks([task])
        texture = unreal.load_asset(f'{TEXTURE_DIR}/{name}')
        if is_normal:
            texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property('srgb', False)
            texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        else:
            texture.set_editor_property('srgb', True)
            texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        print('SOVTRENCH_TEXTURE', name, 'saved', eal.save_loaded_asset(texture, False))


def make_material():
    if eal.does_asset_exist(WOOD_MATERIAL):
        material = unreal.load_asset(WOOD_MATERIAL)
        mel.delete_all_material_expressions(material)
    else:
        material = asset_tools.create_asset('M_SovFieldTrench_Wood', MATERIAL_DIR, unreal.Material,
                                            unreal.MaterialFactoryNew())
    diffuse = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -500, -100)
    diffuse.set_editor_property('texture', unreal.load_asset(TEXTURE_DIR + '/T_SovFieldTrench_Wood_D'))
    normal = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -500, 250)
    normal.set_editor_property('texture', unreal.load_asset(TEXTURE_DIR + '/T_SovFieldTrench_Wood_N'))
    normal.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 120)
    roughness.set_editor_property('r', WOOD_ROUGHNESS)
    # Brightness lifts the silvered logs against the dark Soviet bags; tune it per map in a material instance.
    brightness = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, -250)
    brightness.set_editor_property('parameter_name', 'Brightness')
    brightness.set_editor_property('default_value', WOOD_BRIGHTNESS)
    lifted = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, -100)
    mel.connect_material_expressions(diffuse, 'RGB', lifted, 'A')
    mel.connect_material_expressions(brightness, '', lifted, 'B')
    mel.connect_material_property(lifted, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    print('SOVTRENCH_MATERIAL saved', eal.save_loaded_asset(material, False))


def _static_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    data = options.static_mesh_import_data
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def fix_sockets(mesh, record):
    """Pure-yaw socket rotations from the manifest; raises when a socket is missing or misplaced."""
    for entry in record['sockets']:
        socket = mesh.find_socket(entry['name'])
        if socket is None:
            raise RuntimeError(f"{record['name']}: socket {entry['name']} missing after import")
        x, y, z = entry['location']
        location = socket.relative_location
        error = max(abs(location.x - x), abs(location.y + y), abs(location.z - z))
        if error > 0.05:
            raise RuntimeError(f"{record['name']} {entry['name']}: at {location}, expected ({x}, {-y}, {z})")
        socket.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=-entry['yaw']))
        socket.set_editor_property('relative_scale', unreal.Vector(1.0, 1.0, 1.0))
    mesh.modify()


def import_meshes():
    with open(MANIFEST) as handle:
        records = json.load(handle)['pieces']
    for record in records:
        name = record['name']
        task = unreal.AssetImportTask()
        task.filename = f'{BLENDER_EXPORTS}/{name}.fbx'
        task.destination_path = MESH_DIR
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        task.options = _static_options()
        asset_tools.import_asset_tasks([task])
        mesh = unreal.load_asset(f'{MESH_DIR}/{name}')
        if mesh is None:
            raise RuntimeError('Import produced nothing for ' + name)
        for index, slot in enumerate(mesh.static_materials):
            target = MATERIALS.get(str(slot.material_slot_name))
            if target:
                mesh.set_material(index, unreal.load_asset(target))
        body = mesh.get_editor_property('body_setup')
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        mesh.set_editor_property('lod_group', 'LargeProp')
        fix_sockets(mesh, record)
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(
            mesh, unreal.MeshNaniteSettings(enabled=False), apply_changes=True)
        print('SOVTRENCH_IMPORT', name, 'tris', mesh.get_num_triangles(0), 'manifest', record['triangles'],
              'saved', eal.save_loaded_asset(mesh, False))


def report():
    with open(MANIFEST) as handle:
        records = json.load(handle)['pieces']
    for record in records:
        mesh = unreal.load_asset(f"{MESH_DIR}/{record['name']}")
        slots = [(str(s.material_slot_name), s.material_interface.get_name() if s.material_interface else None)
                 for s in mesh.static_materials]
        sockets = mesh.get_sockets_by_tag('')
        print('SOVTRENCH_REPORT', record['name'], 'tris', mesh.get_num_triangles(0), 'sockets', len(sockets),
              '/', len(record['sockets']), slots)


STEP_FUNCTIONS = {'textures': import_textures, 'material': make_material, 'meshes': import_meshes, 'report': report}
for step in STEPS:
    STEP_FUNCTIONS[step]()
