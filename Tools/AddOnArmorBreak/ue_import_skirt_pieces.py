"""Imports the test skirt pieces made by make_skirt_pieces.py into /Game/RTS_Survival/Tests/AddOnArmorBreak.

Test content only: nothing outside the add-on armour break test references this folder, so it is never cooked.
Creates M_TestSkirtSteel (usable on geometry collections, Niagara mesh particles and instanced meshes), imports the
shards, the panel and the rail with Nanite off, assigns the material and checks each shard's bounds against the
layout written by the generator (catches unit or axis mistakes in the FBX round trip).

Run: UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -nullrhi
"""
import json

import unreal

EXPORTS = 'D:/UE5Projects/RTS_Survival/Saved/AddOnArmorBreak/Exports'
DESTINATION = '/Game/RTS_Survival/Tests/AddOnArmorBreak'
MESH_FOLDER = DESTINATION + '/Meshes'
MATERIAL_PATH = DESTINATION + '/M_TestSkirtSteel'
# Dark yellow (Dunkelgelb) in linear colour, as the late-war skirts were painted.
STEEL_COLOUR = unreal.LinearColor(0.30, 0.235, 0.115, 1.0)
BOUNDS_TOLERANCE = 0.5
FBX_UNIFORM_SCALE = 100.0
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def make_material():
    if eal.does_asset_exist(MATERIAL_PATH):
        eal.delete_asset(MATERIAL_PATH)
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset('M_TestSkirtSteel', DESTINATION, unreal.Material, unreal.MaterialFactoryNew())
    colour = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, 0)
    colour.set_editor_property('constant', STEEL_COLOUR)
    roughness = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 200)
    roughness.set_editor_property('r', 0.62)
    metallic = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 300)
    metallic.set_editor_property('r', 0.25)
    mel.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metallic, '', unreal.MaterialProperty.MP_METALLIC)
    material.set_editor_property('two_sided', True)
    for usage in ('used_with_geometry_collections', 'used_with_niagara_mesh_particles',
                  'used_with_instanced_static_meshes', 'used_with_static_lighting'):
        material.set_editor_property(usage, True)
    mel.recompile_material(material)
    eal.save_asset(MATERIAL_PATH, only_if_is_dirty=False)
    return material


def static_options():
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
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    # The legacy FBX importer converts the exported 0.01 scale back to metres; the bounds check below proves the
    # pieces come in 1:1 in centimetres with this.
    data.set_editor_property('import_uniform_scale', FBX_UNIFORM_SCALE)
    return options


def import_mesh(name, material):
    task = unreal.AssetImportTask()
    task.filename = f'{EXPORTS}/{name}.fbx'
    task.destination_path = MESH_FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = static_options()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    path = f'{MESH_FOLDER}/{name}'
    mesh = unreal.load_asset(path)
    if mesh is None:
        raise RuntimeError('Import produced nothing for ' + name)
    for index in range(len(mesh.static_materials)):
        mesh.set_material(index, material)
    settings = mesh.get_editor_property('nanite_settings')
    settings.enabled = False
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)
    eal.save_asset(path, only_if_is_dirty=False)
    return unreal.load_asset(path)


def check_shard_bounds(mesh, shard):
    box = mesh.get_bounding_box()
    centre_x, _, centre_z = shard['centroid']
    xs = [point[0] - centre_x for point in shard['outline']]
    zs = [point[1] - centre_z for point in shard['outline']]
    expected = (min(xs), max(xs), min(zs), max(zs))
    actual = (box.min.x, box.max.x, box.min.z, box.max.z)
    matches = all(abs(a - e) < BOUNDS_TOLERANCE for a, e in zip(actual, expected))
    print('SKIRT_IMPORT shard', shard['mesh'], 'match', matches, 'actual', [round(v, 2) for v in actual],
          'expected', [round(v, 2) for v in expected], 'y', round(box.min.y, 2), round(box.max.y, 2),
          'nanite', mesh.get_editor_property('nanite_settings').enabled)
    return matches


def main():
    with open(f'{EXPORTS}/skirt_layout.json', encoding='utf-8') as handle:
        layout = json.load(handle)
    material = make_material()
    all_match = True
    for shard in layout['shards']:
        mesh = import_mesh(shard['mesh'], material)
        all_match = check_shard_bounds(mesh, shard) and all_match
    for name in ('SM_TestSkirtPanel', 'SM_TestSkirtRail'):
        mesh = import_mesh(name, material)
        box = mesh.get_bounding_box()
        print('SKIRT_IMPORT', name, 'box', [round(v, 2) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)])
    print('SKIRT_IMPORT done shards', len(layout['shards']), 'all_bounds_match', all_match)


main()
