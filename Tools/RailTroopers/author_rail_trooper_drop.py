"""Adds GA_GerRailTroopersDrop to DA_GlobalAbilitySystem without touching any other template.

Run inside the editor (UECP execute_python with this file's text). Needs an editor DLL that has
EGlobalAbility::GA_GerRailTroopersDrop and ESquadSubtype::Squad_Ger_RailTroopers.
A copy of GA_ScavengersDrop (same drop class and transport) that lands two Rail Trooper squads. Tier II.
The same entry is in Source/Tools/Commanders/author_commander_abilities.py, so a full re-run there keeps it.
"""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
SOURCE = "GA_SCAVENGERS_DROP"
TARGET = "GA_GER_RAIL_TROOPERS_DROP"
TITLE = "Rail Trooper Drop"
DESCRIPTION = ("A transport lands two squads of Rail Troopers at the target: heavily armoured infantry with rail "
               "rifles that punch through tank armour.")
COSTS = {"RESOURCE_RADIXITE": 1300, "RESOURCE_METAL": 500}
COOLDOWN = 150
COPIED_PROPERTIES = ("m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                     "m_ui_settings", "m_ability_marker", "m_ability_tier", "m_aircraft_drop_settings")
LOG = "[RailTrooperDrop] "


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    enum = type(next(iter(templates.keys())))
    source = templates.get(getattr(enum, SOURCE))
    key = getattr(enum, TARGET)
    obj = templates.get(key)
    if obj is None:
        obj = unreal.new_object(source.get_class(), outer=asset, name="GA_Commander_GA_GerRailTroopersDrop")
        # The property set author_commander_abilities.py copies (transport, offsets, UI icon, sounds, marker).
        for prop in COPIED_PROPERTIES:
            try:
                obj.set_editor_property(prop, source.get_editor_property(prop))
            except Exception:
                pass
        print(LOG + "created from", SOURCE)
    drop = obj.get_editor_property("m_aircraft_drop_settings")
    drop.set_editor_property("tanks_with_offsets", [])
    drop.set_editor_property("squad_subtypes", [unreal.SquadSubtype.SQUAD_GER_RAIL_TROOPERS] * 2)
    obj.set_editor_property("m_aircraft_drop_settings", drop)
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", "<Text_Title>%s</>" % TITLE)
    ui.set_editor_property("description", DESCRIPTION)
    obj.set_editor_property("m_ui_settings", ui)
    obj.set_editor_property("m_ability_type", key)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_II)
    cost_state = obj.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", {getattr(unreal.RTSResourceType, k): v for k, v in COSTS.items()})
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", COOLDOWN)
    obj.set_editor_property("m_ability_costs", cost_state)
    templates[key] = obj
    asset.set_editor_property("m_ability_templates_by_type", templates)
    print(LOG + "saved", unreal.EditorAssetLibrary.save_loaded_asset(asset, False), "entries", len(templates))
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")[key]
    print(LOG + "squads", check.get_editor_property("m_aircraft_drop_settings").get_editor_property("squad_subtypes"),
          "tier", check.get_editor_property("m_ability_tier"),
          "costs", check.get_editor_property("m_ability_costs").get_editor_property("costs").get_editor_property(
              "resource_costs"))


run()
