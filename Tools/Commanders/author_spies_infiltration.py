"""Adds the Spies Infiltration template (GA_GerSpiesInfiltration, UGA_SpiesInfiltration) to DA_GlobalAbilitySystem.
Run headless: UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file>, or exec() it in the editor.
Idempotent: re-running updates the existing template. Needs an editor DLL with UGA_SpiesInfiltration; saving the data
asset from an older DLL drops the entries it cannot load. Icon: GlobalAbilityIcons/import_ability_icons.py.
"""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[SpiesInfiltration] "
KEY_NAME = "GA_GER_SPIES_INFILTRATION"
TITLE = "Spies Infiltration"
DESCRIPTION = ("Spies behind the enemy lines report 6 enemy units hidden in the fog, picked at random. The fog lifts "
               "around each of them for 25 seconds and follows them while they move. No aiming needed.")
COOLDOWN = 180
RADIXITE = 200


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    enum_type = type(next(iter(templates.keys())))
    key = getattr(enum_type, KEY_NAME)
    report = ["%d entries before" % len(templates)]

    obj = templates.get(key)
    if obj is None:
        obj = unreal.new_object(unreal.GA_SpiesInfiltration, outer=asset, name="GA_SpiesInfiltration_Ger")
        report.append("created template")
    obj.set_editor_property("m_ability_type", key)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_I)

    costs = obj.get_editor_property("m_ability_costs")
    unit_cost = costs.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", {unreal.RTSResourceType.RESOURCE_RADIXITE: RADIXITE})
    costs.set_editor_property("costs", unit_cost)
    costs.set_editor_property("cool_down_time", COOLDOWN)
    obj.set_editor_property("m_ability_costs", costs)

    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", "<Text_Title>%s</>" % TITLE)
    ui.set_editor_property("description", DESCRIPTION)
    obj.set_editor_property("m_ui_settings", ui)

    spies = obj.get_editor_property("m_spies_settings")
    spies.set_editor_property("units_to_reveal", 6)
    spies.set_editor_property("reveal_radius", 600.0)
    spies.set_editor_property("reveal_hold_seconds", 25.0)
    obj.set_editor_property("m_spies_settings", spies)

    templates[key] = obj
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    check = asset.get_editor_property("m_ability_templates_by_type")[key]
    report.append("saved=%s entries=%d tier=%s cooldown=%d settings=%s" % (
        saved, len(templates), check.get_editor_property("m_ability_tier"),
        check.get_editor_property("m_ability_costs").get_editor_property("cool_down_time"),
        check.get_editor_property("m_spies_settings").export_text()))
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
