"""Authors GA_GerFelsKriegerDrop in DA_GlobalAbilitySystem; touches no other template. Idempotent.

The Stahlläufer's Tier III signature (kit slot 6): a Ju 390 lands at the target and lets out two Fels Krieger squads
(six medium mech walkers with Ripper Guns), then takes off again. It is a UGA_AircraftDrop like the Red Spine drops:
walkers cannot parachute, so the landed aircraft delivers them. Created (first run) as a copy of
GA_GerRedSpinePairDrop with all its settings (landing Ju 390, RadiusSpawnSquads 1500 so the walkers stand clear of
the wings, sounds, aim, marker, icon), then:
  squads      2x Squad_Ger_FelsKrieger, both in one Ju 390 (AmountInfantryPerAircraft 2)
  tier        III (slot 6 takes III or IV)
  cost        2125 radixite only = 2 squads x 850 radixite training cost (FelsKrieger_Radixite) x 1.25
  cooldown    240 s (top of the Tier III band 180-240 s, like the Ju 87 G and the field workshop)
Struct values are copied through export_text/import_text: a struct wrapper from get_editor_property can alias the
memory of the template it came from. Every other template is fingerprinted before and after; any change, or an entry
count other than before (+1 on the first run), stops the save.

Needs an editor DLL with GA_GerFelsKriegerDrop (EGlobalAbilityType.h, 2026-11-05 15:12) and every class in the data
asset, so run it headless on the DebugGame build under the da_globalabilities lock (and the build lock):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Saving the data asset from an editor that lacks one of its template classes or keys drops those templates.
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
KEY_NAME = "GA_GerFelsKriegerDrop"
SOURCE = "GA_GerRedSpinePairDrop"
SQUAD = "Squad_Ger_FelsKrieger"
SQUAD_COUNT = 2
TIER = "TierIII"
RADIXITE = 2125
COOLDOWN = 240
RADIUS_SPAWN_SQUADS = 1500.0
TITLE = "Fels Krieger Walker Drop"
DESCRIPTION = ("A Ju 390 lands at the target and lets out two squads of Fels Krieger medium walkers: six armoured "
               "walkers with Ripper Guns.")
OBJECT_NAME = "GA_FelsKrieger_" + KEY_NAME
COPIED_PROPERTIES = ("m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                     "m_ui_settings", "m_ability_marker", "m_ability_tier", "m_aircraft_drop_settings")
LOG = "[FelsKriegerDrop] "
FAILURES = []


def member(enum_type, name):
    """Enum member by its C++ name, whatever the Python spelling (GA_GerFelsKriegerDrop -> GA_GER_FELS_KRIEGER_DROP)."""
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
    if not hasattr(value, "export_text"):
        return value
    text = value.export_text()
    copy = type(value)()
    copy.import_text(text)
    if copy.export_text() != text:
        FAILURES.append("round trip " + type(value).__name__)
        print(LOG + "struct did not round-trip through text:", type(value).__name__)
    return copy


def fingerprint(obj):
    """Text of every copied property the template has (other classes: whatever of these they carry)."""
    if obj is None or not hasattr(obj, "get_editor_property"):
        return str(obj)
    parts = [obj.get_class().get_name()]
    for prop in COPIED_PROPERTIES + ("m_ability_type",):
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        parts.append(value.export_text() if hasattr(value, "export_text") else str(value))
    return " | ".join(parts)


def create_from_source(asset, source):
    obj = unreal.new_object(source.get_class(), outer=asset, name=OBJECT_NAME)
    for prop in COPIED_PROPERTIES:
        obj.set_editor_property(prop, detached_copy(source.get_editor_property(prop)))
    print(LOG + "created", KEY_NAME, "from", SOURCE, "class", source.get_class().get_name())
    return obj


def edit_drop(obj, key):
    settings = detached_copy(obj.get_editor_property("m_aircraft_drop_settings"))
    settings.set_editor_property("tanks_with_offsets", [])
    settings.set_editor_property("squad_subtypes", [member(unreal.SquadSubtype, SQUAD)] * SQUAD_COUNT)
    # Both squads ride in one aircraft, so one Ju 390 lands.
    settings.set_editor_property("amount_infantry_per_aircraft", SQUAD_COUNT)
    settings.set_editor_property("radius_spawn_squads", RADIUS_SPAWN_SQUADS)
    # Fixes workstream (2026-11-05 15:5x): landed squad drops can walk their squads out of the Ju 390's cargo door.
    # Only DLLs built after that change have the field; older ones keep the plain spawn around the aircraft.
    try:
        settings.set_editor_property("squads_walk_out_of_cargo_door", True)
        print(LOG + "squads_walk_out_of_cargo_door = True")
    except Exception as error:
        print(LOG + "WARNING no squads_walk_out_of_cargo_door in this DLL:", error)
    obj.set_editor_property("m_aircraft_drop_settings", settings)

    # User decision 2026-11-05: no template keeps a copied RequiredTechnology (it silently blocks the click);
    # the required unit stays. A fresh struct, never the source's.
    requirements = detached_copy(obj.get_editor_property("m_ability_requirements"))
    requirements.set_editor_property("required_technology", member(unreal.Technology, "Tech_NONE"))
    obj.set_editor_property("m_ability_requirements", requirements)
    print(LOG + "requirements", obj.get_editor_property("m_ability_requirements").export_text())

    ui = detached_copy(obj.get_editor_property("m_ui_settings"))
    ui.set_editor_property("title", TITLE)
    ui.set_editor_property("description", DESCRIPTION)
    obj.set_editor_property("m_ui_settings", ui)

    # FUnitCost::ResourceCosts cannot be set on a struct instance from Python ("cannot be edited on instances"), so
    # the whole cost state is written as text: radixite only, no remaining cooldown.
    costs = detached_copy(obj.get_editor_property("m_ability_costs"))
    costs.import_text("(Costs=(ResourceCosts=((Resource_Radixite, %d))),CoolDownTime=%d,CoolDownRemaining=0)"
                      % (RADIXITE, COOLDOWN))
    obj.set_editor_property("m_ability_costs", costs)

    obj.set_editor_property("m_ability_tier", member(unreal.GlobalAbilityTier, TIER))
    obj.set_editor_property("m_ability_type", key)


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    count_before = len(templates)
    enum = type(next(iter(templates.keys())))
    key = member(enum, KEY_NAME)
    source = templates.get(member(enum, SOURCE))
    if source is None:
        FAILURES.append("missing source " + SOURCE)
        print(LOG + "MISSING", SOURCE, "(run Tools/RedSpine/author_red_spine_drops.py first)")
        return
    others_before = {str(k): fingerprint(v) for k, v in templates.items() if k != key}

    obj = templates.get(key)
    created = obj is None
    if created:
        obj = create_from_source(asset, source)
    else:
        print(LOG + KEY_NAME, "exists, BEFORE", fingerprint(obj))
    edit_drop(obj, key)
    templates[key] = obj
    asset.set_editor_property("m_ability_templates_by_type", templates)

    expected = count_before + (1 if created else 0)
    print(LOG + "entries before", count_before, "after", len(templates), "expected", expected)
    if len(templates) != expected:
        FAILURES.append("entry count %d expected %d" % (len(templates), expected))
    for other_key, value in templates.items():
        text = others_before.get(str(other_key))
        if text is not None and fingerprint(value) != text:
            FAILURES.append("other template changed " + str(other_key))
    if FAILURES:
        print(LOG + "NOT SAVING:", ", ".join(FAILURES))
        return
    save(asset)

    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    print(LOG + KEY_NAME, "AFTER", fingerprint(check[key]))
    print(LOG + "entries on reload", len(check))


run()
print(LOG + ("DONE OK" if not FAILURES else "DONE WITH FAILURES " + ", ".join(FAILURES)))
