"""Registers the small German pits as field defenses of the German HQ (see Docs/UI/FieldDefenses.md).

Run inside the editor (bridge pyfile or the editor's Python console) after build_gersmallpit_bxp.py:
  * the option pictures (GerDefenses/Scripts/ger_small_pit_icons.py) as UI textures and slate brushes in GerPits/UI;
  * the rows Train_BTX_GerPuppchenPit / Train_BTX_GerPanzerschreckPit of DT_TrainingOptions (picture, title, tooltip,
    description); existing rows are replaced, and the table is only saved when every other row survives the JSON
    round trip unchanged;
  * the async spawner's BuildingExpansionMap (expansion class) and BxpPreviewMeshMap (placement preview: the pit mesh).
The training options and the HQ layout slots are C++ (UTrainingMenuManager::InitAllGameFieldDefenseTrainingOptions,
UTrainingLayoutDataAsset::MakeDefaultHeadquartersLayout).
"""
import json

import unreal

ICON_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Icons'
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits'
UI = ROOT + '/UI'
TEMPLATE_BRUSH = '/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionOptions/SB_OptionImage_37mmFLak'
TABLE_PATH = '/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions'
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
TEXT_NAMESPACE = 'FieldDefenses'
PITS = {
    'Puppchen': {
        'type': unreal.BuildingExpansionType.BTX_GER_PUPPCHEN_PIT, 'row': 'Train_BTX_GerPuppchenPit',
        'name': 'Puppchen Pit',
        'description': 'An 8.8 cm Raketenwerfer 43 "Puppchen" in a small sandbag ring. A loader slides each anti-tank '
                       'rocket into the breech; it outranges the Panzerschreck. Cheap and quick to build. Place it on '
                       'the map; the HQ builds it there once it is its turn in the queue.'},
    'Panzerschreck': {
        'type': unreal.BuildingExpansionType.BTX_GER_PANZERSCHRECK_PIT, 'row': 'Train_BTX_GerPanzerschreckPit',
        'name': 'Panzerschreck Foxhole',
        'description': 'A Panzerschreck team behind a ring of sandbags: deadly to tanks at short range, but little '
                       'cover. The smallest and cheapest field defense. Place it on the map; the HQ builds it there '
                       'once it is its turn in the queue.'},
}
eal = unreal.EditorAssetLibrary


def import_picture(pit):
    name = f'T_BxpOption_GerPit_{pit}'
    task = unreal.AssetImportTask()
    task.filename = f'{ICON_DIR}/{name}.png'
    task.destination_path = UI
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(f'{UI}/{name}')
    texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    eal.save_asset(f'{UI}/{name}', False)
    return texture


def make_brush(pit, texture):
    path = f'{UI}/SB_OptionImage_GerPit_{pit}'
    if not eal.does_asset_exist(path):
        eal.duplicate_asset(TEMPLATE_BRUSH, path)
    brush_asset = unreal.load_asset(path)
    brush = brush_asset.get_editor_property('brush')
    brush.set_editor_property('resource_object', texture)
    brush_asset.set_editor_property('brush', brush)
    eal.save_asset(path, False)
    return path


def make_text(key, value):
    escaped = value.replace('\\', '\\\\').replace('"', '\\"')
    return f'NSLOCTEXT("{TEXT_NAMESPACE}", "{key}", "{escaped}")'


def make_row(spec, brush_path):
    row_key = spec['row']
    brush_name = brush_path.split('/')[-1]
    return {
        'Name': row_key,
        'ImageBrush': f"/Script/Engine.SlateBrushAsset'{brush_path}.{brush_name}'",
        'Description': make_text(row_key + '_Description', spec['description']),
        'Tooltip': make_text(row_key + '_Tooltip', spec['name']),
        'Title': make_text(row_key + '_Title', f"<Text_Title>{spec['name']}</>"),
    }


def add_rows(new_rows):
    table = unreal.load_asset(TABLE_PATH)
    original_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    replaced = {row['Name'] for row in new_rows}
    kept = [row for row in original_rows if row['Name'] not in replaced]
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(kept + new_rows))
    round_trip = {row['Name']: row for row in
                  json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
    changed = [row['Name'] for row in kept if round_trip.get(row['Name']) != row]
    missing = [row['Name'] for row in new_rows if row['Name'] not in round_trip]
    if changed or missing:
        raise RuntimeError(f'DT_TrainingOptions not saved; changed rows: {changed} missing rows: {missing}')
    print('GERSMALLPIT_REGISTER rows saved', eal.save_loaded_asset(table, only_if_is_dirty=False), len(round_trip))


def register_spawner():
    spawner = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(spawner.generated_class())
    spawner.modify()
    classes = cdo.get_editor_property('building_expansion_map')
    previews = cdo.get_editor_property('bxp_preview_mesh_map')
    for pit, spec in PITS.items():
        classes[spec['type']] = unreal.load_asset(f'{ROOT}/Ger_BXP_GerPit_{pit}').generated_class()
        previews[spec['type']] = unreal.load_asset(f'{ROOT}/Meshes/SM_Ger{pit}Pit')
    cdo.set_editor_property('building_expansion_map', classes)
    cdo.set_editor_property('bxp_preview_mesh_map', previews)
    saved = unreal.EditorLoadingAndSavingUtils.save_packages([spawner.get_outermost()], False)
    cdo = unreal.get_default_object(spawner.generated_class())
    for spec in PITS.values():
        print('GERSMALLPIT_REGISTER spawner', spec['type'], cdo.get_editor_property('building_expansion_map')[spec['type']],
              cdo.get_editor_property('bxp_preview_mesh_map')[spec['type']].get_name(), 'saved', saved)


def run():
    rows = [make_row(spec, make_brush(pit, import_picture(pit))) for pit, spec in PITS.items()]
    add_rows(rows)
    register_spawner()


run()
