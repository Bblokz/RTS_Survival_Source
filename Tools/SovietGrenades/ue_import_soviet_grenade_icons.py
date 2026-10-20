"""Imports the Soviet grenade card icons made by make_soviet_grenade_icons.py, with the texture settings of
T_AB_Molotov. Run through the UECP bridge's execute_python."""
import unreal

SRC = "D:/UE5Projects/RTS_Survival/Saved/SovietGrenadeIcons/"
DEST = "/Game/RTS_Survival/Textures/GameUI/ActionUI"
TEMPLATE = DEST + "/T_AB_Molotov"
NAMES = [prefix + name for name in ("RGD33Grenade", "F1Grenade", "RG42Grenade", "RPG43Grenade")
         for prefix in ("T_AB_", "T_AB_Cancel")]
COPIED = ["compression_settings", "lod_group", "mip_gen_settings", "srgb", "never_stream", "filter",
          "address_x", "address_y"]

template = unreal.load_asset(TEMPLATE)
tasks = []
for name in NAMES:
    task = unreal.AssetImportTask()
    task.filename = SRC + name + ".png"
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for name in NAMES:
    tex = unreal.load_asset(DEST + "/" + name)
    for prop in COPIED:
        tex.set_editor_property(prop, template.get_editor_property(prop))
    print(name, tex.blueprint_get_size_x(), "saved", unreal.EditorAssetLibrary.save_asset(DEST + "/" + name, only_if_is_dirty=False))
print("template", {p: str(template.get_editor_property(p)) for p in COPIED})
