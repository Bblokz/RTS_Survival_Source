"""Adds GA_GerEliteScavengersParadrop to DA_GlobalAbilitySystem without touching any other template.

A UGA_Paratroopers copy of GA_GerParatroopers (same Ju 390 transport, parachutes, spread and UI icon) that drops two
Squad_Ger_EliteScavengers squads. Tier I, Scrap Warlord.
Cost: 250 radixite per squad, 500 for the two (user decision 2026-11-05; was 1150 = 125% of two squads' training
cost). Radixite only. Cooldown 90 s (top of the Tier I band, it lands ten soldiers).
Placeholder icon: GA_GerParatroopers' T_GlobalAbility_GerParatroopers (copied with the UI settings).

Needs an editor DLL that has EGlobalAbility::GA_GerEliteScavengersParadrop and ESquadSubtype::Squad_Ger_EliteScavengers,
so run it headless on the DebugGame build, under the da_globalabilities lock:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Saving the data asset from an editor without those values drops the entry.
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
SOURCE = "GA_GER_PARATROOPERS"
TARGET = "GA_GER_ELITE_SCAVENGERS_PARADROP"
OBJECT_NAME = "GA_Commander_GA_GerEliteScavengersParadrop"
TITLE = "Elite Scavenger Paradrop"
DESCRIPTION = ("A transport drops two squads of Elite Scavengers over the target: tougher scavengers with STG 44s "
               "and Panzerfausts. Squads take orders once all their soldiers have landed.")
SQUAD_COUNT = 2
RADIXITE_PER_SQUAD = 250  # user decision 2026-11-05, radixite only
COOLDOWN = 90
COPIED_PROPERTIES = ("m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                     "m_ui_settings", "m_ability_marker", "m_ability_tier", "m_paratroopers_settings")
LOG = "[EliteScavParadrop] "


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save(asset, retries=12):
    """Saves and proves the file changed; retries while another process holds the package (error 32)."""
    file_path = disk_path(DA_PATH)
    for attempt in range(retries):
        before = os.path.getmtime(file_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if os.path.getmtime(file_path) > before:
            print(LOG + "saved", time.ctime(os.path.getmtime(file_path)))
            return True
        print(LOG + "save did not reach disk, retry", attempt)
        time.sleep(10)
    print(LOG + "SAVE FAILED")
    return False


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    count_before = len(templates)
    enum = type(next(iter(templates.keys())))
    source = templates.get(getattr(enum, SOURCE))
    key = getattr(enum, TARGET)
    obj = templates.get(key)
    if obj is None:
        obj = unreal.new_object(source.get_class(), outer=asset, name=OBJECT_NAME)
        for prop in COPIED_PROPERTIES:
            try:
                obj.set_editor_property(prop, source.get_editor_property(prop))
            except Exception as exc:
                print(LOG + "could not copy", prop, exc)
        print(LOG + "created from", SOURCE, obj.get_class().get_name())
    settings = obj.get_editor_property("m_paratroopers_settings")
    settings.set_editor_property("squad_subtypes", [unreal.SquadSubtype.SQUAD_GER_ELITE_SCAVENGERS] * SQUAD_COUNT)
    obj.set_editor_property("m_paratroopers_settings", settings)
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", TITLE)
    ui.set_editor_property("description", DESCRIPTION)
    obj.set_editor_property("m_ui_settings", ui)
    obj.set_editor_property("m_ability_type", key)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_I)
    # FUnitCost::ResourceCosts is EditDefaultsOnly: python cannot set it on an instance; a fresh struct from text can.
    cost_state = type(obj.get_editor_property("m_ability_costs"))()
    cost_state.import_text("(Costs=(ResourceCosts=((Resource_Radixite, %d))),CoolDownTime=%d,CoolDownRemaining=0)"
                           % (RADIXITE_PER_SQUAD * SQUAD_COUNT, COOLDOWN))
    obj.set_editor_property("m_ability_costs", cost_state)
    templates[key] = obj
    asset.set_editor_property("m_ability_templates_by_type", templates)
    print(LOG + "entries before", count_before, "after", len(templates))
    save(asset)
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")[key]
    print(LOG + "class", check.get_class().get_name(),
          "settings", check.get_editor_property("m_paratroopers_settings").export_text())
    print(LOG + "ui", check.get_editor_property("m_ui_settings").export_text())
    print(LOG + "costs", check.get_editor_property("m_ability_costs").export_text(),
          "tier", check.get_editor_property("m_ability_tier"))


run()
