"""Points GA_GerSFH18Drop ("Howitzer Battery Airdrop") at two sFH 18 squads and recosts it. Touches only that key.

The template (UGA_Paratroopers, authored by Tools/ParachutePayloads/author_parachute_payload_drops.py) used to drop two
leFH 18 squads because no sFH 18 team weapon squad existed. This keeps its aircraft, canopies (3 per gun), flight
height and spawn ring, and changes:
  SquadSubtypes  2x Squad_Ger_SFH18_150mm
  Cost           radixite only (user rule 2026-11-05): 2 squads x 750 radixite (SFH18_150mm_Radixite) x 1.25 = 1875
  Cooldown 180 s, Tier II (Docs/Abilities/Commanders.md bands), title/description for the 15 cm guns.
Structs are rebuilt from export_text/import_text: a struct from get_editor_property can alias template memory.

Needs a DLL with Squad_Ger_SFH18_150mm and the new UGA_Paratroopers fields; run it headless on the DebugGame build
under Saved/AgentLocks/da_globalabilities.lockdir:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Never save DA_GlobalAbilitySystem from the shared editor while its DLL lacks the round-3 classes.
Re-running author_parachute_payload_drops.py reverts this entry to leFH 18 squads; run this script after it.
"""
import os
import re
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
KEY = "GA_GerSFH18Drop"
SUBTYPE = "Squad_Ger_SFH18_150mm"
SQUAD_COUNT = 2
SQUAD_RADIXITE = 750
RADIXITE = int(round(SQUAD_COUNT * SQUAD_RADIXITE * 1.25))
COOLDOWN = 180
TITLE = "<Text_Title>Howitzer Battery Airdrop</>"
DESCRIPTION = ("Two 15 cm sFH 18 heavy howitzers come down over the target, each under a cluster of three cargo "
               "chutes, with their crews jumping after them.")
LOG = "[SFH18Drop] "


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


def enum_member(enum_type, name):
    wanted = name.replace("_", "").lower()
    for member_name in dir(enum_type):
        if member_name.replace("_", "").lower() == wanted:
            return getattr(enum_type, member_name)
    raise KeyError("%s has no member %s" % (enum_type, name))


def fingerprint(obj):
    """Class and every exported struct of a template, to prove other templates are untouched."""
    parts = [obj.get_class().get_name()]
    for prop in ("m_ability_costs", "m_ability_tier", "m_ui_settings"):
        try:
            value = obj.get_editor_property(prop)
            parts.append(value.export_text() if hasattr(value, "export_text") else str(value))
        except Exception as exc:
            parts.append("n/a " + str(exc))
    return "|".join(parts)


def describe(obj):
    costs = obj.get_editor_property("m_ability_costs")
    settings = obj.get_editor_property("m_paratroopers_settings")
    return "%s tier=%s cooldown=%s costs=%s squads=%s ring=%s cargo=%s title=%s" % (
        obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
        costs.get_editor_property("cool_down_time"),
        costs.get_editor_property("costs").get_editor_property("resource_costs"),
        settings.get_editor_property("squad_subtypes"), settings.get_editor_property("squad_spawn_ring_radius"),
        settings.get_editor_property("team_weapon_cargo_descent").export_text(),
        obj.get_editor_property("m_ui_settings").get_editor_property("title"))


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    count_before = len(templates)
    enum = type(next(iter(templates.keys())))
    key = enum_member(enum, KEY)
    obj = templates.get(key)
    if obj is None or obj.get_class() != unreal.GA_Paratroopers.static_class():
        raise RuntimeError("GA_GerSFH18Drop is missing or not a UGA_Paratroopers template; run the parachute "
                           "payload authoring first")
    others_before = {str(k): fingerprint(v) for k, v in templates.items() if k != key}
    print(LOG + "before", describe(obj))

    old_settings = obj.get_editor_property("m_paratroopers_settings")
    text = old_settings.export_text()
    text, count = re.subn(r"SquadSubtypes=\([^)]*\)",
                          "SquadSubtypes=(%s)" % ",".join([SUBTYPE] * SQUAD_COUNT), text)
    if count != 1:
        raise RuntimeError("SquadSubtypes not found once in " + text)
    settings = type(old_settings)()
    settings.import_text(text)
    obj.set_editor_property("m_paratroopers_settings", settings)

    # FUnitCost.ResourceCosts is EditDefaultsOnly (refused on instances): rewrite it in the exported text.
    old_costs = obj.get_editor_property("m_ability_costs")
    cost_text = old_costs.export_text()
    cost_text, count = re.subn(r"ResourceCosts=\((?:\([^()]*\),?)*\)",
                               "ResourceCosts=((Resource_Radixite, %d))" % RADIXITE, cost_text)
    if count != 1:
        raise RuntimeError("ResourceCosts not found once in " + cost_text)
    cost_state = type(old_costs)()
    cost_state.import_text(cost_text)
    cost_state.set_editor_property("cool_down_time", COOLDOWN)
    obj.set_editor_property("m_ability_costs", cost_state)
    print(LOG + "cost text", cost_text, "->", obj.get_editor_property("m_ability_costs").export_text())

    old_ui = obj.get_editor_property("m_ui_settings")
    ui = type(old_ui)()
    ui.import_text(old_ui.export_text())
    ui.set_editor_property("title", TITLE)
    ui.set_editor_property("description", DESCRIPTION)
    obj.set_editor_property("m_ui_settings", ui)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_II)

    templates[key] = obj
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = save(asset)

    check_templates = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    others_after = {str(k): fingerprint(v) for k, v in check_templates.items() if k != key}
    changed = [k for k in others_before if others_after.get(k) != others_before[k]]
    print(LOG + "entries before", count_before, "after", len(check_templates), "other templates changed", changed)
    print(LOG + "after", describe(check_templates[key]))
    ok = saved and not changed and len(check_templates) == count_before
    print(LOG + ("DONE OK" if ok else "DONE WITH FAILURES"))


run()
