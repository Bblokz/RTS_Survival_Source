"""Re-authors GA_GerRedSpineDrop and GA_GerRedSpinePairDrop in DA_GlobalAbilitySystem; touches no other template.

Both are UGA_AircraftDrop templates (created by Tools/Commanders/author_commander_abilities.py as copies of
GA_GerSpotterPzIIIDrop: a Ju 390 that lands, lets its payload out and takes off again). Walkers cannot parachute,
so they are delivered by the landed aircraft like team weapons in the survival deployment.

  GA_GerRedSpineDrop      1x Squad_Ger_RedSpine, Tier I,  cooldown 90 s,  825 radixite
  GA_GerRedSpinePairDrop  2x Squad_Ger_RedSpine, Tier II, cooldown 150 s, 1625 radixite
Cost: about 125% of the squads' radixite training cost (RedSpine_Radixite = 650 in DeveloperSettings.h):
650 x 1.25 = 812.5 -> 825, 2 x 650 x 1.25 = 1625. Radixite only (user rule 2026-11-05: global abilities cost
radixite and nothing else). Cooldowns: top of the Tier I band (60-90 s), middle of the Tier II band (120-180 s).

Struct values are copied through export_text/import_text: a struct wrapper from get_editor_property can alias the
memory of the template it came from (GA_GerParatroopers was overwritten that way on 2026-11-05). The script prints the
copy-source template before and after and fails loudly if it changed.

Needs an editor DLL with ESquadSubtype::Squad_Ger_RedSpine squad data and every class in the data asset, so run it
headless on the DebugGame build under the da_globalabilities lock (and the build lock):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Saving the data asset from an editor that lacks one of its template classes drops those templates.
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
# The template both were copied from; only read, and checked to be unchanged afterwards.
SOURCE = "GA_GerSpotterPzIIIDrop"
SQUAD = "Squad_Ger_RedSpine"
# (ability, squads, tier, radixite cost, cooldown s, title, description)
DROPS = [
    ("GA_GerRedSpineDrop", 1, "TierI", 825, 90, "Red Spine Walker Drop",
     "A Ju 390 lands at the target and lets out a squad of three Red Spine light walkers with MP46s."),
    ("GA_GerRedSpinePairDrop", 2, "TierII", 1625, 150, "Red Spine Walker Pair Drop",
     "A Ju 390 lands at the target and lets out two squads of Red Spine light walkers: six walkers with MP46s."),
]
# Squads stand this far from the landing point (radius of the circle they are spread on). None keeps the template's.
RADIUS_SPAWN_SQUADS = globals().get("RS_RADIUS_SPAWN_SQUADS", None)
LOG = "[RedSpineDrops] "
FAILURES = []


def member(enum_type, name):
    """Enum member by its C++ name, whatever the Python spelling (GA_GerRedSpineDrop -> GA_GER_RED_SPINE_DROP)."""
    wanted = name.replace("_", "").lower()
    for attr in dir(enum_type):
        if attr.replace("_", "").lower() == wanted:
            return getattr(enum_type, attr)
    raise RuntimeError("no %s member %s" % (enum_type.__name__, name))


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save(asset, retries=30):
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
    FAILURES.append("save DA")
    print(LOG + "SAVE FAILED")
    return False


def detached_copy(value):
    """A struct copy that shares no memory with the template it came from."""
    text = value.export_text()
    copy = type(value)()
    copy.import_text(text)
    if copy.export_text() != text:
        FAILURES.append("round trip " + type(value).__name__)
        print(LOG + "struct did not round-trip through text:", type(value).__name__)
    return copy


def template_text(obj):
    parts = []
    for prop in ("m_aircraft_drop_settings", "m_ability_costs", "m_ui_settings", "m_ability_tier"):
        value = obj.get_editor_property(prop)
        parts.append(value.export_text() if hasattr(value, "export_text") else str(value))
    return " | ".join(parts)


def edit_drop(obj, key, squad_count, tier, radixite, cooldown, title, description):
    settings = detached_copy(obj.get_editor_property("m_aircraft_drop_settings"))
    settings.set_editor_property("tanks_with_offsets", [])
    settings.set_editor_property("squad_subtypes", [member(unreal.SquadSubtype, SQUAD)] * squad_count)
    # Every squad of a drop rides in one aircraft, so one Ju 390 lands for either drop.
    settings.set_editor_property("amount_infantry_per_aircraft", max(squad_count, 1))
    if RADIUS_SPAWN_SQUADS is not None:
        settings.set_editor_property("radius_spawn_squads", float(RADIUS_SPAWN_SQUADS))
    obj.set_editor_property("m_aircraft_drop_settings", settings)

    ui = detached_copy(obj.get_editor_property("m_ui_settings"))
    ui.set_editor_property("title", title)
    ui.set_editor_property("description", description)
    obj.set_editor_property("m_ui_settings", ui)

    # FUnitCost::ResourceCosts cannot be set on a struct instance from Python ("cannot be edited on instances"), so
    # the whole cost state is written as text: radixite only, no time cost, no remaining cooldown.
    costs = detached_copy(obj.get_editor_property("m_ability_costs"))
    costs.import_text("(Costs=(ResourceCosts=((Resource_Radixite, %d))),CoolDownTime=%d,CoolDownRemaining=0)"
                      % (radixite, cooldown))
    obj.set_editor_property("m_ability_costs", costs)
    print(LOG + str(key), "costs", costs.export_text())

    obj.set_editor_property("m_ability_tier", member(unreal.GlobalAbilityTier, tier))
    obj.set_editor_property("m_ability_type", key)


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    count_before = len(templates)
    enum = type(next(iter(templates.keys())))
    source = templates.get(member(enum, SOURCE))
    source_before = template_text(source) if source is not None else None
    others_before = {str(k): template_text(v) for k, v in templates.items()
                     if v is not None and hasattr(v, "get_editor_property") and str(k).find("RED_SPINE") < 0
                     and v.get_class().get_name() == "GA_AircraftDrop"}

    for name, squad_count, tier, radixite, cooldown, title, description in DROPS:
        key = member(enum, name)
        obj = templates.get(key)
        if obj is None:
            FAILURES.append("missing template " + name)
            print(LOG + "MISSING", name, "(run Tools/Commanders/author_commander_abilities.py round3 first)")
            continue
        print(LOG + name, "class", obj.get_class().get_name(), "BEFORE", template_text(obj))
        edit_drop(obj, key, squad_count, tier, radixite, cooldown, title, description)
        templates[key] = obj

    asset.set_editor_property("m_ability_templates_by_type", templates)
    print(LOG + "entries before", count_before, "after", len(templates))
    if count_before != len(templates):
        FAILURES.append("entry count changed")

    # Nothing else may have changed through an aliased struct.
    if source is not None and template_text(source) != source_before:
        FAILURES.append("source template changed")
        print(LOG + "SOURCE CHANGED", SOURCE)
    for key, value in templates.items():
        text = others_before.get(str(key))
        if text is not None and template_text(value) != text:
            FAILURES.append("other template changed " + str(key))
    if FAILURES:
        print(LOG + "NOT SAVING:", ", ".join(FAILURES))
        return
    save(asset)

    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    for name, *_ in DROPS:
        obj = check[member(enum, name)]
        print(LOG + name, "AFTER", template_text(obj))


run()
print(LOG + ("DONE OK" if not FAILURES else "DONE WITH FAILURES " + ", ".join(FAILURES)))
