"""Re-imports the placard unit icons (placard_portraits.py output) over the existing in-game textures.

Each texture keeps its asset path and its own texture settings, so brushes, DT_TrainingOptions rows and widgets that
point at it pick up the new image without being touched. Only textures that already exist are replaced; a missing
texture or PNG is reported and skipped.

Run with the editor closed:
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>"
or in a running editor:  exec(open(r"<this file>").read())
Plan files: PLANS in Source/Tools/TacticalIconBackgrounds/plans; PNGs: D:/UE_Assets/Icons/TrainingUI/Placard.
"""
import unreal

# No os/glob: the UECP editor bridge sandbox refuses those imports, so plans are listed and paths checked by unreal.
PLAN_DIR = "D:/UE5Projects/RTS_Survival/Source/Tools/TacticalIconBackgrounds/plans"
PLANS = ("light_medium.txt", "heavy_armoredcar.txt", "infantry_guns_air.txt", "variants_other.txt")
PNG_ROOT = "D:/UE_Assets/Icons/TrainingUI/Placard"
UNIT_ICONS_ROOT = "/Game/RTS_Survival/Textures/GameUI/UnitIcons/"
KEPT_SETTINGS = ("compression_settings", "lod_group", "mip_gen_settings", "srgb", "never_stream", "filter",
                 "max_texture_size", "compression_no_alpha")


def plan_textures():
    for plan in PLANS:
        with open(PLAN_DIR + "/" + plan, encoding="utf-8-sig") as handle:
            for line in handle:
                line = line.strip()
                if line and not line.startswith("#") and "=" in line:
                    yield line.split("=", 1)[0].strip()


def png_for(texture_path):
    relative = texture_path[len(UNIT_ICONS_ROOT):] if texture_path.startswith(UNIT_ICONS_ROOT) \
        else texture_path.rsplit("/", 1)[-1]
    return PNG_ROOT + "/" + relative + ".png"


def reimport(texture_path):
    png = png_for(texture_path)
    if not unreal.EditorAssetLibrary.does_asset_exist(texture_path):
        return "SKIP no asset"
    if not unreal.Paths.file_exists(png):
        return "SKIP no png " + png
    before = unreal.load_asset(texture_path)
    settings = {name: before.get_editor_property(name) for name in KEPT_SETTINGS}
    folder, name = texture_path.rsplit("/", 1)
    task = unreal.AssetImportTask()
    task.filename = png
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.replace_existing_settings = False
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(texture_path)
    for setting, value in settings.items():
        texture.set_editor_property(setting, value)
    # save_asset can return False right after a re-import over the bridge; save_loaded_asset writes it.
    saved = unreal.EditorAssetLibrary.save_asset(texture_path, only_if_is_dirty=False) or \
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    if not saved:
        return "FAIL save"
    return "OK %dx%d" % (texture.blueprint_get_size_x(), texture.blueprint_get_size_y())


def main():
    results = {}
    for texture_path in plan_textures():
        results[texture_path] = reimport(texture_path)
        unreal.log_warning("PLACARD_ICONS %s %s" % (results[texture_path], texture_path))
    ok = sum(1 for result in results.values() if result.startswith("OK"))
    unreal.log_warning("PLACARD_ICONS done ok=%d of %d" % (ok, len(results)))


main()
