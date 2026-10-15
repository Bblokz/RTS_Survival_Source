"""Build option pictures of the German gun pits: texture, slate brush and DT_Items row per pit.

Pictures come from D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Scripts/gerpit_icons.py.
Run inside the editor (bridge pyfile or the editor's Python console). Rows copy the 37mm flak option and swap in the
pit's picture, name and description; rerunning replaces them.
"""
import json

import unreal

ICON_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Icons'
UI = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/UI'
TEMPLATE_BRUSH = '/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionOptions/SB_OptionImage_37mmFLak'
DATA_TABLE = '/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionItems/DT_Items'
TEMPLATE_ROW = 'BTX_37mmFlak'
PITS = {
    'Pak40': ('BTX_GerPak40Pit', 'Pak 40 Pit',
              'A Pak 40 75mm anti-tank gun in a low sandbagged pit, so it fires over the rim. A loader and an ammunition '
              'handler keep it fed; the gun and its crew turn on the plank deck to face any direction.'),
    'LeFH18': ('BTX_GerLeFH18Pit', 'leFH 18 Pit',
               'A leFH 18 105mm field howitzer behind a high sandbag wall with camo canvas. Fast-firing artillery: the '
               'handler passes each shell to the loader while the next one is fetched from the ready crate.'),
    'SFH18': ('BTX_GerSFH18Pit', 'sFH 18 Pit',
              'An sFH 18 150mm heavy howitzer behind a high sandbag wall with camo canvas. Heavy, slow-loading '
              'artillery with long reach; its crew carry each shell from the ready crate to the breech.'),
}
eal = unreal.EditorAssetLibrary


def import_picture(gun):
    name = f'T_BxpOption_GerPit_{gun}'
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


def make_brush(gun, texture):
    path = f'{UI}/SB_OptionImage_GerPit_{gun}'
    if not eal.does_asset_exist(path):
        eal.duplicate_asset(TEMPLATE_BRUSH, path)
    brush_asset = unreal.load_asset(path)
    brush = brush_asset.get_editor_property('brush')
    brush.set_editor_property('resource_object', texture)
    brush_asset.set_editor_property('brush', brush)
    eal.save_asset(path, False)
    return path


def make_row(template, row_name, display_name, description, brush_path):
    row = dict(template)
    row['Name'] = row_name
    brush_name = brush_path.split('/')[-1]
    for key in list(row.keys()):
        if key.startswith('Built_ImageBrush'):
            row[key] = f"/Script/Engine.SlateBrushAsset'{brush_path}.{brush_name}'"
        elif key.startswith('Name_') or key.startswith('Tooltip_'):
            row[key] = display_name
        elif key.startswith('Description_'):
            row[key] = description
    return row


def run():
    table = unreal.load_asset(DATA_TABLE)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    template = next(r for r in rows if r['Name'] == TEMPLATE_ROW)
    new_names = {row_name for row_name, _, _ in PITS.values()}
    rows = [r for r in rows if r['Name'] not in new_names]
    for gun, (row_name, display_name, description) in PITS.items():
        brush_path = make_brush(gun, import_picture(gun))
        rows.append(make_row(template, row_name, display_name, description, brush_path))
    print('GERPIT_ICONS fill', unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)))
    print('GERPIT_ICONS saved', eal.save_asset(DATA_TABLE, False))
    print('GERPIT_ICONS rows', [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)][-4:])


run()
