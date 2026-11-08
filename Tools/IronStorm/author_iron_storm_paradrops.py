"""Authors the Sturmkommandant's Iron Storm paradrops in DA_GlobalAbilitySystem (round 4, 2026-11-05):
  GA_GerIronStormParadrop        slot 2, Tier I:  1x Squad_Ger_IronStorm
  GA_GerPzGrenIronStormParadrop  slot 4, Tier II: 2x Squad_Ger_PanzerGrenadiere + 2x Squad_Ger_IronStorm
Both are UGA_Paratroopers copies of GA_GerEliteScavengersParadrop (same Ju 390 transport, parachutes, sounds, marker
and placeholder icon T_GlobalAbility_GerParatroopers). Touches only these two keys: every other template is
fingerprinted before the edit, before the save and after the save, and the run refuses to save on any difference.

Costs (radixite only, user rule 2026-11-05), about 1.25x the squads' radixite training cost (DeveloperSettings.h:
IronStorm_Radixite 700, PanzerGrenadiere_Radixite 675), rounded up to 25:
  Iron Storm paradrop          700 x 1.25 = 875                         -> 875 R, cooldown 90 s (Tier I band 60-90)
  PzGren + Iron Storm paradrop (2 x 675 + 2 x 700) x 1.25 = 3437.5      -> 3450 R, cooldown 180 s (Tier II band 120-180)
The four-squad drop uses the larger landing spread of GA_GerParatroopers (also four squads) when it is wider.

Struct values are copied through export_text/import_text: a struct wrapper from get_editor_property can alias the
memory of the template it came from (GA_GerParatroopers was overwritten that way on 2026-11-05).

The keys are newer than the 15:01 DLLs, so run it headless on a DebugGame build that has them, holding
Saved/AgentLocks/da_globalabilities.lockdir and debuggame_build.lockdir:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Saving the data asset from an editor without these enum values drops the entries. Idempotent: existing templates are
updated in place. Report: Saved/IronStormTest/author_iron_storm_<time>.txt and the log ([IronStorm] prefix).
Test: -IronStormValidate (Units/Squads/Tests/IronStorm).
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[IronStorm] "
SAVE_RETRIES = 12
SAVE_RETRY_SECONDS = 10.0

SOURCE = "GA_GerEliteScavengersParadrop"
# Read only: its landing spread for the four-squad drop.
SPREAD_SOURCE = "GA_GerParatroopers"

# (key, squads, tier, radixite, cooldown, title, description)
DROPS = [
    ("GA_GerIronStormParadrop", ["Squad_Ger_IronStorm"], "TierI", 875, 90, "Iron Storm Paradrop",
     "A transport drops a squad of Iron Storm over the target: four cyborg storm troopers in leg armour with STG 44s. "
     "The squad takes orders once all its soldiers have landed."),
    ("GA_GerPzGrenIronStormParadrop",
     ["Squad_Ger_PanzerGrenadiere", "Squad_Ger_PanzerGrenadiere", "Squad_Ger_IronStorm", "Squad_Ger_IronStorm"],
     "TierII", 3450, 180, "Panzergrenadier and Iron Storm Paradrop",
     "A transport drops two squads of Panzergrenadiere and two squads of Iron Storm over the target: armoured "
     "cyborg infantry with rockets and STG 44s for a breakthrough. Squads take orders once all their soldiers have "
     "landed."),
]
MY_KEYS = tuple(drop[0] for drop in DROPS)

COPIED_PROPERTIES = (
    "m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings", "m_ui_settings",
    "m_ability_marker", "m_ability_tier", "m_paratroopers_settings")

# Every template property a fingerprint looks at; missing ones are skipped per class.
FINGERPRINT_PROPERTIES = (
    "m_ability_type", "m_ability_tier", "m_ability_costs", "m_ability_requirements", "m_aim_settings",
    "m_ability_sound_settings", "m_ui_settings", "m_ability_marker", "m_aircraft_drop_settings",
    "m_aircraft_strafing_settings", "m_aircraft_bombing_settings", "m_barrage_settings", "m_glider_drop_settings",
    "m_paratroopers_settings", "m_strip_flight_settings", "m_flare_curtain_settings", "m_resource_drop_settings",
    "m_projectile_mover", "m_weapon_data", "m_weapon_vfx")


def pythonize(ue_name):
    """Unreal's Python name for an enum value (see Tools/Commanders/author_commander_abilities.py)."""
    out = []
    for i, ch in enumerate(ue_name):
        prev = ue_name[i - 1] if i else ""
        nxt = ue_name[i + 1] if i + 1 < len(ue_name) else ""
        if ch.isupper() and prev and prev != "_" and (prev.islower() or (nxt.islower() and nxt != "")):
            out.append("_")
        out.append(ch)
    return "".join(out).upper()


def member(enum_cls, ue_name):
    for candidate in (pythonize(ue_name), ue_name.upper()):
        value = getattr(enum_cls, candidate, None)
        if value is not None:
            return value
    wanted = ue_name.replace("_", "").lower()
    for member_name in dir(enum_cls):
        if member_name.replace("_", "").lower() == wanted:
            return getattr(enum_cls, member_name)
    raise KeyError("%s has no member %s (tried %s)" % (enum_cls.__name__, ue_name, pythonize(ue_name)))


GA_ENUM = None
R = member(unreal.RTSResourceType, "Resource_Radixite")


def ga(name):
    return member(GA_ENUM, name)


def detached_copy(value):
    """A struct copy that shares no memory with the template it came from."""
    if not hasattr(value, "export_text"):
        return value
    text = value.export_text()
    copy = type(value)()
    copy.import_text(text)
    if copy.export_text() != text:
        raise RuntimeError("struct %s did not round-trip through text" % type(value).__name__)
    return copy


def fingerprint(obj):
    parts = [obj.get_class().get_name(), obj.get_name()]
    for prop in FINGERPRINT_PROPERTIES:
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        parts.append("%s=%s" % (prop, value.export_text() if hasattr(value, "export_text") else str(value)))
    return "|".join(parts)


def find_group(text, field):
    """(start, end) of 'field=(...)' in an exported struct text, balanced parentheses; None when absent."""
    start = text.find(field + "=")
    if start < 0:
        return None
    depth = 0
    for end in range(start + len(field) + 1, len(text)):
        if text[end] == "(":
            depth += 1
        elif text[end] == ")":
            depth -= 1
            if depth == 0:
                return start, end + 1
    raise RuntimeError("unbalanced %s in %s" % (field, text))


def set_balance(obj, tier_name, radixite, cooldown):
    """FUnitCost::ResourceCosts is EditDefaultsOnly, so Python cannot set it on an instance: the cost state is
    rebuilt from its exported text with a radixite-only ResourceCosts (as Tools/SkyDrop/author_sky_drop_abilities.py)."""
    obj.set_editor_property("m_ability_tier", member(unreal.GlobalAbilityTier, tier_name))
    cost_state = obj.get_editor_property("m_ability_costs")
    text = cost_state.export_text()
    new_group = "ResourceCosts=((Resource_Radixite, %d))" % radixite
    group = find_group(text, "ResourceCosts")
    if group is not None:
        new_text = text[:group[0]] + new_group + text[group[1]:]
    else:
        costs_group = find_group(text, "Costs")
        if costs_group is None or text[costs_group[0]:costs_group[1]] != "Costs=()":
            raise RuntimeError("unexpected cost text %s" % text)
        new_text = text[:costs_group[0]] + "Costs=(%s)" % new_group + text[costs_group[1]:]
    fresh = type(cost_state)()
    fresh.import_text(new_text)
    fresh.set_editor_property("cool_down_time", cooldown)
    fresh.set_editor_property("cool_down_remaining", 0)
    got = dict(fresh.get_editor_property("costs").get_editor_property("resource_costs"))
    if got != {R: radixite}:
        raise RuntimeError("cost import gave %s (text %s)" % (got, new_text))
    obj.set_editor_property("m_ability_costs", fresh)


def get_or_create(asset, templates, name, source, report):
    key = ga(name)
    obj = templates.get(key)
    if obj is not None:
        if obj.get_class() != unreal.GA_Paratroopers.static_class():
            raise RuntimeError("existing template is a %s" % obj.get_class().get_name())
        report.append("updating existing %s (%s)" % (name, obj.get_name()))
        return obj
    obj = unreal.new_object(unreal.GA_Paratroopers, outer=asset, name="GA_IronStorm_" + name)
    for prop in COPIED_PROPERTIES:
        obj.set_editor_property(prop, detached_copy(source.get_editor_property(prop)))
    report.append("created %s from %s (%d properties)" % (name, SOURCE, len(COPIED_PROPERTIES)))
    return obj


def title_like_source(source, title):
    """Keeps the source's rich-text wrapping (plain or <Text_Title>...</>) so every panel title looks the same."""
    source_title = str(source.get_editor_property("m_ui_settings").get_editor_property("title"))
    if source_title.startswith("<") and source_title.endswith("</>") and ">" in source_title:
        tag = source_title[:source_title.index(">") + 1]
        return "%s%s</>" % (tag, title)
    return title


def author_drop(asset, templates, source, spread_source, drop, report):
    name, squads, tier, radixite, cooldown, title, description = drop
    obj = get_or_create(asset, templates, name, source, report)
    settings = detached_copy(obj.get_editor_property("m_paratroopers_settings"))
    settings.set_editor_property("squad_subtypes", [member(unreal.SquadSubtype, squad) for squad in squads])
    source_settings = source.get_editor_property("m_paratroopers_settings")
    spread = source_settings.get_editor_property("landing_spread_radius")
    if len(squads) > 2 and spread_source is not None:
        spread = max(spread, spread_source.get_editor_property("m_paratroopers_settings").get_editor_property(
            "landing_spread_radius"))
    settings.set_editor_property("landing_spread_radius", spread)
    obj.set_editor_property("m_paratroopers_settings", settings)

    ui = detached_copy(obj.get_editor_property("m_ui_settings"))
    ui.set_editor_property("title", title_like_source(source, title))
    ui.set_editor_property("description", description)
    obj.set_editor_property("m_ui_settings", ui)

    set_balance(obj, tier, radixite, cooldown)
    obj.set_editor_property("m_ability_type", ga(name))
    templates[ga(name)] = obj
    report.append("%s squads=%s spread=%s" % (name, squads, spread))
    return obj


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save_to_disk(asset, report):
    """While another process holds the package the save fails silently (error 32): retry and prove the file changed."""
    file_path = disk_path(DA_PATH)
    for attempt in range(SAVE_RETRIES):
        before = os.path.getmtime(file_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if os.path.getmtime(file_path) > before:
            report.append("saved %s" % time.ctime(os.path.getmtime(file_path)))
            return True
        report.append("save did not reach disk, retry %d" % attempt)
        time.sleep(SAVE_RETRY_SECONDS)
    report.append("SAVE FAILED")
    return False


def describe(name, obj):
    costs = obj.get_editor_property("m_ability_costs")
    ui = obj.get_editor_property("m_ui_settings")
    return "%s %s %s tier=%s cd=%s costs=%s title=%s desc=%s settings=%s" % (
        name, obj.get_name(), obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
        costs.get_editor_property("cool_down_time"),
        {str(k).split(".")[-1]: v for k, v in costs.get_editor_property("costs").get_editor_property(
            "resource_costs").items()},
        ui.get_editor_property("title"), ui.get_editor_property("description"),
        obj.get_editor_property("m_paratroopers_settings").export_text()[:1200])


def run():
    global GA_ENUM
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    GA_ENUM = type(next(iter(templates.keys())))
    count_before = len(templates)
    mine = set(str(ga(name)) for name in MY_KEYS)
    new_keys = [name for name in MY_KEYS if templates.get(ga(name)) is None]
    others_before = {str(key): fingerprint(obj) for key, obj in templates.items() if str(key) not in mine}
    report, errors = ["%d entries before, new keys %s" % (count_before, new_keys)], []
    source = templates.get(ga(SOURCE))
    spread_source = templates.get(ga(SPREAD_SOURCE))
    if source is None or source.get_class() != unreal.GA_Paratroopers.static_class():
        return report + ["ERROR source %s missing or not a GA_Paratroopers" % SOURCE, "NOT SAVED"]
    report.append("source " + describe(SOURCE, source))
    for drop in DROPS:
        try:
            author_drop(asset, templates, source, spread_source, drop, report)
        except Exception as error:
            errors.append("%s: %s" % (drop[0], error))
    others_unsaved = {str(key): fingerprint(obj) for key, obj in templates.items() if str(key) not in mine}
    changed = sorted(key for key in others_before if others_unsaved.get(key) != others_before[key])
    if changed:
        errors.append("other templates changed before the save: %s" % changed)
    if len(templates) != count_before + len(new_keys):
        errors.append("entry count %d, expected %d" % (len(templates), count_before + len(new_keys)))
    if errors:
        return report + ["ERROR " + e for e in errors] + ["NOT SAVED"]
    asset.set_editor_property("m_ability_templates_by_type", templates)
    if not save_to_disk(asset, report):
        return report
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    report.append("entries before=%d after=%d" % (count_before, len(check)))
    others_after = {str(key): fingerprint(obj) for key, obj in check.items() if str(key) not in mine}
    changed = sorted(key for key in others_before if others_after.get(key) != others_before[key])
    if changed:
        report.append("ERROR other templates changed after save: %s" % changed)
    else:
        report.append("other templates unchanged after save (%d checked)" % len(others_before))
    report.append("source unchanged: %s" % (others_after.get(str(ga(SOURCE))) == others_before.get(str(ga(SOURCE)))))
    for name in MY_KEYS:
        report.append("AFTER " + describe(name, check[ga(name)]))
    return report


def write_report(lines):
    folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "IronStormTest")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, "author_iron_storm_%s.txt" % time.strftime("%H%M%S"))
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


lines = run()
for line in lines:
    print(LOG_PREFIX + line)
ok = not any(line.startswith("ERROR") or line == "SAVE FAILED" or line == "NOT SAVED" for line in lines)
print(LOG_PREFIX + "report " + write_report(lines))
print(LOG_PREFIX + ("DONE OK" if ok else "DONE WITH FAILURES"))
