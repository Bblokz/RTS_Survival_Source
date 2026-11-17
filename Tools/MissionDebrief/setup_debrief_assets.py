"""Headless, after the C++ (AMissionDebriefStage / UMissionDebriefBurnField) is built:
- MI_Debrief_Flame / MI_Debrief_Ember: additive sprite instances with the flame and dot textures;
- DA_Debrief_BurnField: the CPU burn field grids from the mock-up export (timeline.json);
- reports what still references the old victory/defeat widget Blueprints (DEBRIEF_SETUP referencer ...).
Env DEBRIEF_EXPORT_DIR: the export folder with timeline.json.
"""
import json
import os

import unreal

ROOT = "/Game/RTS_Survival/Blueprints/Missions/MissionDebrief"
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
SRC = os.environ.get("DEBRIEF_EXPORT_DIR", "")


def log(*a):
    unreal.log_warning("DEBRIEF_SETUP " + " ".join(str(x) for x in a))


def make_sprite_mi(name, texture):
    path = f"{ROOT}/Materials/{name}"
    if EAL.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = AT.create_asset(name, f"{ROOT}/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, unreal.load_asset(f"{ROOT}/Materials/M_Debrief_SpriteAdd"))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, "Sprite", unreal.load_asset(f"{ROOT}/Textures/{texture}"))
    EAL.save_loaded_asset(mi)
    log("material instance", name)


def make_burn_field():
    data = json.load(open(os.path.join(SRC, "timeline.json")))
    path = f"{ROOT}/DA_Debrief_BurnField"
    if EAL.does_asset_exist(path):
        da = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.MissionDebriefBurnField)
        da = AT.create_asset("DA_Debrief_BurnField", ROOT, unreal.MissionDebriefBurnField, factory)
    gv, gd = data["victory"]["burnGrid"], data["defeat"]["burnGrid"]
    da.set_editor_property("grid_width", gv["w"])
    da.set_editor_property("grid_height", gv["h"])
    da.set_editor_property("victory_field", [float(x) for x in gv["v"]])
    da.set_editor_property("defeat_field", [float(x) for x in gd["v"]])
    EAL.save_loaded_asset(da)
    log("burn field", gv["w"], "x", gv["h"])


def report_old_widget_referencers():
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for pkg in ("/Game/RTS_Survival/Blueprints/Missions/MissionUI/BP_W_Victory", "/Game/RTS_Survival/Blueprints/Missions/MissionUI/BP_W_Defeat"):
        refs = ar.get_referencers(pkg, unreal.AssetRegistryDependencyOptions(True, True, True, True, True)) or []
        log("referencers of", pkg, ":", [str(r) for r in refs])


make_sprite_mi("MI_Debrief_Flame", "T_Debrief_Sprite_Flame")
make_sprite_mi("MI_Debrief_Ember", "T_Debrief_Sprite_Dot")
make_burn_field()
report_old_widget_referencers()
log("done")
