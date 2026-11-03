"""Read-only check of the commander kits against DA_GlobalAbilitySystem and the commander config: every kit
ability has a template, its tier matches the slot plan, its costs are set, and the copied templates carry the
right contents. Logs lines prefixed [CommanderVerify]; run like author_commander_abilities.py."""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
SLOT_TIERS = ("TIER_I", "TIER_I", "TIER_II", "TIER_III", ("TIER_III", "TIER_IV"), "TIER_IV")


def log(line):
    unreal.log("[CommanderVerify] " + line)


def name_of(value):
    return str(value).split(".")[-1].split(":")[0]


def describe(key, obj):
    cls = obj.get_class().get_name()
    cost = obj.get_editor_property("m_ability_costs")
    costs = {name_of(k): v for k, v in cost.get_editor_property("costs").get_editor_property("resource_costs").items()}
    parts = ["%s %s tier=%s cd=%d costs=%s" % (name_of(key), cls, name_of(obj.get_editor_property("m_ability_tier")),
                                                cost.get_editor_property("cool_down_time"), costs)]
    for prop, field in (("m_aircraft_drop_settings", "tanks_with_offsets"), ("m_glider_drop_settings", "tank_subtype"),
                        ("m_aircraft_strafing_settings", "aircraft_classes"), ("m_barrage_settings", "amount_shells")):
        try:
            value = obj.get_editor_property(prop).get_editor_property(field)
        except Exception:
            continue
        if field == "tanks_with_offsets":
            value = [name_of(t.get_editor_property("tank_subtype")) for t in value]
            squads = obj.get_editor_property(prop).get_editor_property("squad_subtypes")
            value = "tanks=%s squads=%s" % (value, [name_of(s) for s in squads])
        elif field == "aircraft_classes":
            value = [str(c).split("/")[-1] for c in value]
        elif field == "tank_subtype":
            override = obj.get_editor_property(prop).get_editor_property("tank_class_override")
            value = "%s override=%s" % (name_of(value), override)
        parts.append("%s=%s" % (field, value))
    title = obj.get_editor_property("m_ui_settings").get_editor_property("title")
    parts.append("title=%s" % title)
    return " | ".join(parts)


def run():
    templates = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    log("entries=%d" % len(templates))
    by_name = {name_of(k): (k, v) for k, v in templates.items()}
    settings = unreal.get_default_object(unreal.CommanderSettings)
    problems = 0
    for commander, kit in settings.get_editor_property("commander_settings_by_type").items():
        abilities = list(kit.get_editor_property("global_abilities"))
        log("commander %s: %d abilities" % (name_of(commander), len(abilities)))
        for slot, ability in enumerate(abilities):
            entry = by_name.get(name_of(ability))
            if entry is None:
                log("  slot %d %s: NO TEMPLATE" % (slot + 1, name_of(ability)))
                problems += 1
                continue
            obj = entry[1]
            tier = name_of(obj.get_editor_property("m_ability_tier"))
            expected = SLOT_TIERS[slot] if slot < len(SLOT_TIERS) else ()
            ok_tier = tier in expected if isinstance(expected, tuple) else tier == expected
            costs = obj.get_editor_property("m_ability_costs").get_editor_property("costs").get_editor_property(
                "resource_costs")
            if not ok_tier or not costs:
                problems += 1
            log("  slot %d %s tier=%s%s%s" % (slot + 1, name_of(ability), tier, "" if ok_tier else " (SLOT MISMATCH)",
                                              "" if costs else " (NO COST)"))
    for key, obj in templates.items():
        log(describe(key, obj))
    log("problems=%d" % problems)


run()
