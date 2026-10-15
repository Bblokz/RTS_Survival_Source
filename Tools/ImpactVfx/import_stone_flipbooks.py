"""Headless editor step: imports the stone dust flipbooks and makes stone copies of the two sand flipbook materials.
Run: UnrealEditor-Cmd <uproject> -run=pythonscript -script="import_stone_flipbooks.py <folder>" """
import sys
import unreal

FOLDER = sys.argv[-1]
TARGET = '/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Howitzer/Stone'
SAND_MATERIALS = '/Game/RTS_Survival/SpecialEffects/AllExplosions/Materials/'
PAIRS = [('MI_Explosion_Sand_11', 'T_HowitzerStoneDust_1', 'MI_HowitzerStoneDust_1'),
         ('MI_Explosion_Sand_12', 'T_HowitzerStoneDust_2', 'MI_HowitzerStoneDust_2')]

tasks = []
for _, texture_name, _ in PAIRS:
    task = unreal.AssetImportTask()
    task.filename = f'{FOLDER}/{texture_name}.png'
    task.destination_path = TARGET
    task.destination_name = texture_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
for sand_name, texture_name, material_name in PAIRS:
    sand = unreal.load_asset(SAND_MATERIALS + sand_name)
    texture = unreal.load_asset(f'{TARGET}/{texture_name}')
    source_texture = sand.texture_parameter_values[0].parameter_value
    for prop in ['compression_settings', 'srgb', 'lod_group']:
        texture.set_editor_property(prop, source_texture.get_editor_property(prop))
    texture.modify()
    material_path = f'{TARGET}/{material_name}'
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        material = unreal.load_asset(material_path)
    else:
        material = asset_tools.duplicate_asset(material_name, TARGET, sand)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(material, 'Texture', texture)
    unreal.MaterialEditingLibrary.update_material_instance(material)
    saved_texture = unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    saved_material = unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    unreal.log(f'RTS_STONE_TEX made {material.get_path_name()} with {texture.get_path_name()} '
               f'{texture.blueprint_get_size_x()} saved={saved_texture},{saved_material}')
