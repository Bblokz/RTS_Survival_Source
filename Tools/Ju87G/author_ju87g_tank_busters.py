"""Authors GA_GerJu87GTankBusters in DA_GlobalAbilitySystem (Sky-Drop Quartermaster, slot 5, Tier III).

UGA_AircraftStrafing copied from GA_3xJu87Strafing (aim, sounds, marker, requirements and strafe timing via
export_text/import_text, so nothing aliases the source), with 2 Ju 87 G (Ger_Aircraft_StukaJu87G_Airborne: two
BK 3,7 pods firing 37 mm APCR, no bombs, prefers tanks) instead of 3 Ju 87. Touches only this key.

The data asset holds templates of classes the Development DLL lacks, so run headless on the DebugGame build under
Saved/AgentLocks/da_globalabilities.lockdir (and the build lock):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Idempotent: rebuilt from the source template and these numbers on every run. Radixite-only cost (user decision
2026-11-05). The source's RequiredTechnology (Tech_HarvesterSpeed, a copy bug in the Ju 87 strafes) is NOT copied:
user decision 2026-11-05, the template keeps only the source's RequiredUnit. Test: -Ju87GTankBustersValidate.
"""
import re
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[Ju87GTankBusters] "
SAVE_ATTEMPTS = 6
SAVE_RETRY_SECONDS = 10
ICON_DIR = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/"
JU87G_CLASS = ("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Ju87G/Ger_Aircraft_StukaJu87G_Airborne."
               "Ger_Aircraft_StukaJu87G_Airborne_C")

SPEC = {
    "key": "GA_GerJu87GTankBusters",
    "source": "GA_3xJu87Strafing",
    "title": "Ju 87 G Tank Busters",
    "description": ("Two Ju 87 G 'Kanonenvogel' dive on the target, each with two 3.7 cm BK 3,7 cannon pods "
                    "firing tungsten-core APCR. No bombs: after the strafing run they hunt the nearest tanks until "
                    "they withdraw. Weak against infantry."),
    # Placeholder icon until a Ju 87 G render exists.
    "icon": "T_GlobalAbility_3xJu87Strafing",
    "aircraft": 2,
    # Tier III strike band: GA_Ju87Strafing 300 R (1 Ju 87), GA_3xJu87Strafing 700 R (3 Ju 87).
    "costs": {"Resource_Radixite": 650},
    "cooldown": 240,
}
COPIED = ("m_ability_requirements", "m_aim_settings", "m_ability_sound_settings", "m_ability_marker",
          "m_aircraft_strafing_settings")


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
    raise KeyError("%s has no member %s" % (enum_cls.__name__, ue_name))


def import_struct(obj, prop, text):
    value = obj.get_editor_property(prop)
    value.import_text(text)
    obj.set_editor_property(prop, value)
    return obj.get_editor_property(prop).export_text()


def source_text(obj):
    return "|".join(obj.get_editor_property(prop).export_text() for prop in COPIED)


def apply_aircraft(obj):
    text = obj.get_editor_property("m_aircraft_strafing_settings").export_text()
    classes = ",".join('"%s"' % JU87G_CLASS for _ in range(SPEC["aircraft"]))
    new_text, count = re.subn(r"AircraftClasses=\([^()]*\)", "AircraftClasses=(%s)" % classes, text, count=1)
    if count != 1:
        raise RuntimeError("AircraftClasses not in %s" % text[:300])
    after = import_struct(obj, "m_aircraft_strafing_settings", new_text)
    if after.count("Ger_Aircraft_StukaJu87G_Airborne_C") != SPEC["aircraft"]:
        raise RuntimeError("aircraft classes not applied: %s" % after[:400])
    return after


def clear_required_technology(obj):
    """Drops the copied Tech_HarvesterSpeed gate (user decision 2026-11-05); a fresh struct, never the source's."""
    text = obj.get_editor_property("m_ability_requirements").export_text()
    fresh = type(obj.get_editor_property("m_ability_requirements"))()
    fresh.import_text(text)
    fresh.set_editor_property("required_technology", member(unreal.Technology, "Tech_NONE"))
    obj.set_editor_property("m_ability_requirements", fresh)
    after = obj.get_editor_property("m_ability_requirements").export_text()
    if "Tech_HarvesterSpeed" in after:
        raise RuntimeError("required technology not cleared: %s" % after)
    return after


def apply_ui_and_balance(obj):
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", "<Text_Title>%s</>" % SPEC["title"])
    ui.set_editor_property("description", SPEC["description"])
    icon = unreal.load_asset(ICON_DIR + SPEC["icon"])
    if icon is None:
        raise RuntimeError("missing icon " + SPEC["icon"])
    ui.set_editor_property("ability_icon", icon)
    obj.set_editor_property("m_ui_settings", ui)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_III)
    # FUnitCost::ResourceCosts is EditDefaultsOnly: python cannot set it on an instance, import_text can.
    resources = ",".join("(%s, %d)" % (name, amount) for name, amount in SPEC["costs"].items())
    cost_state = type(obj.get_editor_property("m_ability_costs"))()
    cost_state.import_text("(Costs=(ResourceCosts=(%s)),CoolDownTime=%d,CoolDownRemaining=0)"
                           % (resources, SPEC["cooldown"]))
    obj.set_editor_property("m_ability_costs", cost_state)


def run():
    if unreal.load_class(None, JU87G_CLASS) is None:
        return ["ERROR Ju 87 G Blueprint missing (run author_ju87g_blueprint.py first)", "NOT SAVED"]
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    enum_type = type(next(iter(templates.keys())))
    key = member(enum_type, SPEC["key"])
    source = templates.get(member(enum_type, SPEC["source"]))
    report = ["%d entries before" % len(templates)]
    if source is None:
        return report + ["ERROR source %s missing" % SPEC["source"], "NOT SAVED"]
    before = source_text(source)
    obj = templates.get(key)
    if obj is not None and obj.get_class() != unreal.GA_AircraftStrafing.static_class():
        return report + ["ERROR existing template is a %s" % obj.get_class().get_name(), "NOT SAVED"]
    if obj is None:
        obj = unreal.new_object(unreal.GA_AircraftStrafing, outer=asset, name="GA_AircraftStrafing_" + SPEC["key"])
        report.append("created %s" % SPEC["key"])
    for prop in COPIED:
        import_struct(obj, prop, source.get_editor_property(prop).export_text())
    requirements = clear_required_technology(obj)
    report.append("requirements %s" % requirements)
    obj.set_editor_property("m_ability_type", key)
    strafing = apply_aircraft(obj)
    apply_ui_and_balance(obj)
    templates[key] = obj
    if source_text(source) != before:
        return report + ["ERROR source template changed while authoring", "NOT SAVED"]
    report.append("strafing %s" % strafing[:900])
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = False
    attempt = 0
    for attempt in range(SAVE_ATTEMPTS):
        # Another process reading the package (a -game run just exiting) fails the save with error 32; retry.
        saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if saved:
            break
        time.sleep(SAVE_RETRY_SECONDS)
    report.append("saved=%s attempts=%d entries=%d" % (saved, attempt + 1, len(templates)))
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type").get(key)
    if check is None:
        report.append("VERIFY missing after save")
    else:
        report.append("VERIFY class=%s tier=%s costs=%s ui=%s" % (
            check.get_class().get_name(), check.get_editor_property("m_ability_tier"),
            check.get_editor_property("m_ability_costs").export_text(),
            check.get_editor_property("m_ui_settings").export_text()[:600]))
    report.append("source %s unchanged=%s" % (SPEC["source"], source_text(source) == before))
    return report


try:
    lines = run()
except Exception as error:
    lines = ["ERROR %s" % error, "NOT SAVED"]
for line in lines:
    unreal.log(LOG_PREFIX + line)
