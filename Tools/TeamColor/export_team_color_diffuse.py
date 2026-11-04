"""Exports the camo texture of every team colour target, so pick_team_color_keys.py can find each camo's key band.

Run headless after inspect_team_color_targets.py (read only, writes PNGs under Saved/TeamColorTest/diffuse):
UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi -nosound -DisablePlugins=Fab
Writes Saved/TeamColorTest/team_color_diffuse.json: target material (root or instance) -> texture and PNG file.
"""
import json
import os
import re

import unreal

SAVED = os.path.join(unreal.Paths.project_saved_dir(), "TeamColorTest")
INSPECT_FILE = os.path.join(SAVED, "team_color_targets_inspect.json")
OUTPUT_FILE = os.path.join(SAVED, "team_color_diffuse.json")
PNG_FOLDER = os.path.join(SAVED, "diffuse")
DIFFUSE_PARAMETERS = ["TankDiffuse", "Diffuse", "DiffuseColorMap", "D"]
OVERLAY_PARAMETER = "Camo"
OVERLAY_ROOT = "MM_Ger_PostApoTank"
# Texture names that are never the camo: masks, normals, packed maps, the shared icon and flag atlases.
NOT_CAMO = re.compile(r"(normal|_n$|_nm|rough|metal|_orm|_arm|_ao|_r_ao|spec|mask|emiss|height|faction|atlas_diffuse|"
                      r"camogermanwwii|desertgreen_scratched|t_ger_tank_atlas)", re.IGNORECASE)


def get_expressions(material):
    return [expression for expression in unreal.ObjectIterator(unreal.MaterialExpressionTextureBase)
            if expression.get_outer() == material]


def get_texture_size(texture):
    try:
        return texture.blueprint_get_size_x() * texture.blueprint_get_size_y()
    except Exception:
        return 0


def find_root_camo_texture(material):
    candidates = []
    for expression in get_expressions(material):
        texture = expression.get_editor_property("texture")
        if not isinstance(texture, unreal.Texture2D) or NOT_CAMO.search(texture.get_name()):
            continue
        if not texture.get_editor_property("srgb"):
            continue
        candidates.append((get_texture_size(texture), texture))
    candidates.sort(key=lambda entry: -entry[0])
    return candidates[0][1] if candidates else None


def find_instance_camo_texture(instance):
    for parameter in DIFFUSE_PARAMETERS:
        texture = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance, parameter)
        if texture is not None and not NOT_CAMO.search(texture.get_name()):
            return texture
    return None


def export_png(texture, exported):
    if texture.get_path_name() in exported:
        return exported[texture.get_path_name()]
    # Different vehicles reuse texture names (Hull_low_BaseColor), so the file name carries the folder too.
    package = texture.get_path_name().split(".")[0]
    file_name = package.replace("/Game/RTS_Survival/Blueprints/", "").replace("/", "__")
    file_path = os.path.join(PNG_FOLDER, file_name + ".png")
    task = unreal.AssetExportTask()
    task.object = texture
    task.filename = file_path
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.TextureExporterPNG()
    ok = unreal.Exporter.run_asset_export_task(task)
    exported[texture.get_path_name()] = file_path if ok else ""
    return exported[texture.get_path_name()]


def main():
    os.makedirs(PNG_FOLDER, exist_ok=True)
    with open(INSPECT_FILE, encoding="utf-8") as handle:
        inspect = json.load(handle)
    exported = {}
    result = {}
    for root_path, info in inspect["roots"].items():
        texture = find_root_camo_texture(unreal.load_asset(root_path))
        result[root_path] = {"kind": "master" if info.get("is_master") else "root",
                             "texture": texture.get_path_name() if texture else "None",
                             "png": export_png(texture, exported) if texture else ""}
    for instance_path, info in inspect["instances"].items():
        instance = unreal.load_asset(instance_path)
        texture = find_instance_camo_texture(instance)
        result[instance_path] = {"kind": "instance", "root": info["root"], "units": info["units"],
                                 "texture": texture.get_path_name() if texture else "None",
                                 "png": export_png(texture, exported) if texture else ""}
        # MM_Ger_PostApoTank blends a camo pattern over the diffuse before Base Color, where the team colour is
        # spliced in: that overlay is the camo the keys must match.
        overlay = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(instance, OVERLAY_PARAMETER)
        if overlay is not None and info["root"].endswith("/" + OVERLAY_ROOT):
            result[instance_path]["overlay_png"] = export_png(overlay, exported)
    with open(OUTPUT_FILE, "w", encoding="utf-8") as handle:
        json.dump(result, handle, indent=1)
    print(f"TeamColorDiffuse: {len(result)} targets, {len(exported)} textures -> {OUTPUT_FILE}")


main()
