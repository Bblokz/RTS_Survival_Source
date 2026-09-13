"""Run with Unreal's PythonScript commandlet after compiling the icon system.

Creates the starter catalog and its cook label without overwriting existing artwork.
Only the catalog reference is written into the AnimatedIconSettings INI section.
"""

from pathlib import Path
import re

import unreal


ASSET_DIRECTORY = "/Game/RTS_Survival/Blueprints/GameUI/PooledUI/AnimatedIcons"
CATALOG_NAME = "DA_AnimatedIcons"
LABEL_NAME = "PAL_AnimatedIcons"


def get_or_create_data_asset(asset_name, asset_class):
    asset_path = f"{ASSET_DIRECTORY}/{asset_name}"
    existing_asset = unreal.EditorAssetLibrary.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
    if existing_asset is not None:
        if not isinstance(existing_asset, asset_class):
            raise RuntimeError(f"Unexpected asset type at {asset_path}")
        return existing_asset

    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    created_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, ASSET_DIRECTORY, asset_class, factory
    )
    if created_asset is None:
        raise RuntimeError(f"Could not create {asset_path}")
    return created_asset


def assign_catalog_in_config(catalog):
    config_path = Path(unreal.Paths.project_config_dir()) / "DefaultGame.ini"
    original_bytes = config_path.read_bytes()
    text = original_bytes.decode("utf-8-sig")
    newline = "\r\n" if "\r\n" in text else "\n"
    section_name = "/Script/RTS_Survival.AnimatedIconSettings"
    assignment = f"IconDataAsset={catalog.get_path_name()}"
    section_pattern = re.compile(r"(?m)^\[" + re.escape(section_name) + r"\][^\r\n]*\r?\n(?P<body>.*?)(?=^\[|\Z)", re.DOTALL)
    section_match = section_pattern.search(text)
    if section_match is None:
        updated_text = text.rstrip("\r\n") + newline * 2 + f"[{section_name}]" + newline + assignment + newline
    else:
        section_text = section_match.group(0)
        if re.search(r"(?m)^IconDataAsset=", section_text):
            section_text = re.sub(r"(?m)^IconDataAsset=[^\r\n]*", assignment, section_text)
        else:
            section_text = section_text.rstrip("\r\n") + newline + assignment + newline * 2
        updated_text = text[:section_match.start()] + section_text + text[section_match.end():]
    encoding = "utf-8-sig" if original_bytes.startswith(b"\xef\xbb\xbf") else "utf-8"
    if updated_text != text:
        config_path.write_bytes(updated_text.encode(encoding))


catalog = get_or_create_data_asset(CATALOG_NAME, unreal.AnimatedIconDataAsset)
if not unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False):
    raise RuntimeError("Could not save the animated icon catalog")

# A config-only soft reference is not a sufficient cook root. Explicitly cook the
# catalog; its serialized texture and widget references are then dependencies.
label = get_or_create_data_asset(LABEL_NAME, unreal.PrimaryAssetLabel)
explicit_assets = list(label.get_editor_property("explicit_assets"))
if catalog not in explicit_assets:
    explicit_assets.append(catalog)
label.set_editor_property("explicit_assets", explicit_assets)
rules = label.get_editor_property("rules")
rules.set_editor_property("cook_rule", unreal.PrimaryAssetCookRule.ALWAYS_COOK)
rules.set_editor_property("apply_recursively", True)
label.set_editor_property("rules", rules)
if not unreal.EditorAssetLibrary.save_loaded_asset(label, only_if_is_dirty=False):
    raise RuntimeError("Could not save the animated icon cook label")

assign_catalog_in_config(catalog)
unreal.log(f"ANIMATED_ICONS_SETUP_COMPLETE: {catalog.get_path_name()}")
