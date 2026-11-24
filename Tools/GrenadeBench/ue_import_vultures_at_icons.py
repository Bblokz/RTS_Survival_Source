"""Imports the Vultures' AT stick grenade card icons made by make_vultures_at_icons.py (texture settings of
T_AB_Molotov) and points card key 5 (StickGrenadeVultures) of IdThrowGrenade / IdCancelThrowGrenade in
Buttons_DataTableb at them. Run through the UECP bridge's execute_uecp_script."""
import re

import unreal

SRC = "D:/UE5Projects/RTS_Survival/Saved/VulturesATIcon/"
DEST = "/Game/RTS_Survival/Textures/GameUI/ActionUI"
TEMPLATE = DEST + "/T_AB_Molotov"
TABLE = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"
ICONS = {"IdThrowGrenade": "T_AB_ATStickGrenade", "IdCancelThrowGrenade": "T_AB_CancelATStickGrenade"}
KEY = 5
COPIED = ["compression_settings", "lod_group", "mip_gen_settings", "srgb", "never_stream", "filter",
          "address_x", "address_y"]

template = unreal.load_asset(TEMPLATE)
tasks = []
for name in ICONS.values():
    task = unreal.AssetImportTask()
    task.filename = SRC + name + ".png"
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name in ICONS.values():
    tex = unreal.load_asset(DEST + "/" + name)
    for prop in COPIED:
        tex.set_editor_property(prop, template.get_editor_property(prop))
    print(name, tex.blueprint_get_size_x(), "saved",
          unreal.EditorAssetLibrary.save_asset(DEST + "/" + name, only_if_is_dirty=False))

rows = uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ICONS))["rows"]
edits = []
for row in rows:
    mapping = row["fields"]["CustomTypeMapping"]
    icon = ICONS[row["row_name"]]
    # Only the icon path inside the key-5 entry changes.
    entry = re.search(r"\(%d, \(.*?\"\)\)" % KEY, mapping)
    if entry is None:
        print(row["row_name"], "has no key", KEY, "- run add_vultures_grenade_card.py first")
        continue
    new_entry = re.sub(r"ActionUI/T_AB_\w+\.T_AB_\w+'", "ActionUI/%s.%s'" % (icon, icon), entry.group(0))
    edits.append({"row_name": row["row_name"],
                  "fields": {"CustomTypeMapping": mapping[:entry.start()] + new_entry + mapping[entry.end():]}})
if edits:
    print(uecp.data.edit_data_table_rows(table_path=TABLE, edits=edits))
print("table saved", unreal.EditorAssetLibrary.save_asset(TABLE, only_if_is_dirty=False))
for row in uecp.data.get_data_table_rows(table_path=TABLE, row_names=list(ICONS))["rows"]:
    entry = re.search(r"\(%d, \(.*?\"\)\)" % KEY, row["fields"]["CustomTypeMapping"])
    print(row["row_name"], re.findall(r"T_AB_\w+'", entry.group(0)))
