"""Registers the German Flak 88 emplacement as a field defense of the German HQ (see Docs/UI/FieldDefenses.md).

Run inside the editor (bridge pyfile or the editor's Python console) after build_flak88pit.py, in an editor built
with BTX_GerFlak88Pit:
  * the option picture (GerDefenses/Scripts/flak88_pit_preview.py -- icon) as a UI texture and slate brush in GerPits/UI;
  * the row Train_BTX_GerFlak88Pit of DT_TrainingOptions (picture, title, tooltip, description); an existing row is
    replaced, and the table is only saved when every other row survives the JSON round trip unchanged;
  * the async spawner's BuildingExpansionMap (expansion class) and BxpPreviewMeshMap (placement preview: the pit mesh).
The training option and the HQ layout slot are C++ (UTrainingMenuManager::InitAllGameFieldDefenseTrainingOptions,
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
PIT = 'Flak88'
SPEC = {
    'type': unreal.BuildingExpansionType.BTX_GER_FLAK88_PIT, 'row': 'Train_BTX_GerFlak88Pit',
    'name': 'Flak 88 Emplacement',
    'description': 'The 8.8 cm Flak dug in behind thick sandbag walls it only just fires over. Three men keep it '
                   'firing: a runner carries the rounds from the ammo niche to the ready crates, a handler passes them '
                   'to the loader. Hits tanks hard at long range and shrugs off a lot of fire. Place it on the map; '
                   'the HQ builds it there once it is its turn in the queue.'}
eal = unreal.EditorAssetLibrary


def import_picture():
    name = f'T_BxpOption_GerPit_{PIT}'
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


def make_brush(texture):
    path = f'{UI}/SB_OptionImage_GerPit_{PIT}'
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


def make_row(brush_path):
    row_key = SPEC['row']
    brush_name = brush_path.split('/')[-1]
    return {
        'Name': row_key,
        'ImageBrush': f"/Script/Engine.SlateBrushAsset'{brush_path}.{brush_name}'",
        'Description': make_text(row_key + '_Description', SPEC['description']),
        'Tooltip': make_text(row_key + '_Tooltip', SPEC['name']),
        'Title': make_text(row_key + '_Title', f"<Text_Title>{SPEC['name']}</>"),
    }


def add_row(new_row):
    table = unreal.load_asset(TABLE_PATH)
    original_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    kept = [row for row in original_rows if row['Name'] != new_row['Name']]
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(kept + [new_row]))
    round_trip = {row['Name']: row for row in
                  json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}
    changed = [row['Name'] for row in kept if round_trip.get(row['Name']) != row]
    if changed or new_row['Name'] not in round_trip:
        raise RuntimeError(f'DT_TrainingOptions not saved; changed rows: {changed}')
    print('FLAK88PIT_REGISTER rows saved', eal.save_loaded_asset(table, only_if_is_dirty=False), len(round_trip))


def register_spawner():
    spawner = unreal.load_asset(SPAWNER)
    cdo = unreal.get_default_object(spawner.generated_class())
    spawner.modify()
    classes = cdo.get_editor_property('building_expansion_map')
    previews = cdo.get_editor_property('bxp_preview_mesh_map')
    classes[SPEC['type']] = unreal.load_asset(f'{ROOT}/Ger_BXP_GerPit_{PIT}').generated_class()
    previews[SPEC['type']] = unreal.load_asset(f'{ROOT}/Meshes/SM_Ger{PIT}Pit')
    cdo.set_editor_property('building_expansion_map', classes)
    cdo.set_editor_property('bxp_preview_mesh_map', previews)
    saved = unreal.EditorLoadingAndSavingUtils.save_packages([spawner.get_outermost()], False)
    cdo = unreal.get_default_object(spawner.generated_class())
    print('FLAK88PIT_REGISTER spawner', cdo.get_editor_property('building_expansion_map')[SPEC['type']],
          cdo.get_editor_property('bxp_preview_mesh_map')[SPEC['type']].get_name(), 'saved', saved)


def run():
    add_row(make_row(make_brush(import_picture())))
    register_spawner()


run()
