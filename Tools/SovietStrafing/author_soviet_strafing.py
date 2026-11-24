"""Authors the enemy's Soviet strafing abilities in DA_GlobalAbilitySystem (2026-11-24):
GA_2xPE2Strafing, GA_4xPE2Strafing (guns-only Pe-2 Recon airframe) and GA_2xIl2Strafing (Il-2 Sturmovik).
The strategic AI (light/medium air strike bucket), the survival air raids and the attack-aircraft warnings already
list these types; until now the data asset had no template for them, so they never ran.

Each template is a copy of the 2x Bf 110 strafe (same strafe length, time and dive settings) with Soviet aircraft,
no resource costs (enemy abilities, as the Pe-8 carpet bombing), the Soviet beacon marker and the Pe-2 recon icon.
Idempotent: existing entries are updated. Saves only when every step succeeded.

Run headless on the DebugGame build (the data asset holds templates of classes the Development DLL lacks) while
holding Saved/AgentLocks/da_globalabilities.lockdir:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -nullrhi
"""
import os
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG_PREFIX = "[SovietStrafing] "
PE2_CLASS = "/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/PE2/Rus_PE2_Aircraft_Recon.Rus_PE2_Aircraft_Recon_C"
IL2_CLASS = "/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/Sturmovic/Rus_Aircraft_Sturmovic.Rus_Aircraft_Sturmovic_C"
ICON_PATH = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/T_GlobalAbility_SovietReconFlight.T_GlobalAbility_SovietReconFlight"
SOURCE_STRAFE = "GA_2xBf110Strafing"
SOURCE_MARKER = "GA_SovietReconFlight"

# (ability, aircraft class, count, cooldown seconds, title, description)
TEMPLATES = (
    ("GA_2xPE2Strafing", PE2_CLASS, 2, 210, "2x Pe-2 Strafing",
     "Two Pe-2s strafe the target with their nose machine guns."),
    ("GA_4xPE2Strafing", PE2_CLASS, 4, 300, "4x Pe-2 Strafing",
     "Four Pe-2s strafe the target with their nose machine guns."),
    ("GA_2xIl2Strafing", IL2_CLASS, 2, 240, "2x Il-2 Strafing",
     "Two Il-2 Sturmoviks strafe the target with their wing machine guns."),
)

COPIED_PROPERTIES = ("m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                     "m_ui_settings", "m_ability_marker", "m_ability_tier", "m_aircraft_strafing_settings")


def pythonize(ue_name):
    """Unreal's Python name for an enum value (see Source/Tools/Commanders/author_commander_abilities.py)."""
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


def detached_copy(value):
    """A struct copy that shares no memory with the template it came from."""
    if not hasattr(value, "export_text"):
        return value
    text = value.export_text()
    copy = type(value)()
    copy.import_text(text)
    return copy if copy.export_text() == text else value


def copy_props(source, target):
    for prop in COPIED_PROPERTIES:
        try:
            value = source.get_editor_property(prop)
        except Exception:
            continue
        target.set_editor_property(prop, detached_copy(value))


def set_aircraft(obj, class_path, count):
    settings = obj.get_editor_property("m_aircraft_strafing_settings")
    last_error = None
    for value in (unreal.SoftClassPath(class_path), unreal.load_class(None, class_path)):
        try:
            settings.set_editor_property("aircraft_classes", [value] * count)
            break
        except Exception as error:
            last_error = error
    else:
        raise last_error
    # Guns only (user decision 2026-11-24): both airframes carry bombs, which a strafe drops at every pull-out.
    settings.set_editor_property("drop_bombs_at_pull_out", False)
    obj.set_editor_property("m_aircraft_strafing_settings", settings)


def set_enemy_costs(obj, cooldown):
    """Enemy abilities cost nothing, as the Pe-8 carpet bombing; only the cooldown counts."""
    cost_state = obj.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", {})
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", cooldown)
    cost_state.set_editor_property("cool_down_remaining", 0)
    obj.set_editor_property("m_ability_costs", cost_state)


def clear_required_technology(obj):
    requirements = detached_copy(obj.get_editor_property("m_ability_requirements"))
    for attr in dir(unreal.Technology):
        if attr.replace("_", "").lower() == "technone":
            requirements.set_editor_property("required_technology", getattr(unreal.Technology, attr))
    obj.set_editor_property("m_ability_requirements", requirements)


def set_ui(obj, title, description):
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", "<Text_Title>%s</>" % title)
    ui.set_editor_property("description", description)
    ui.set_editor_property("ability_icon", unreal.load_asset(ICON_PATH))
    obj.set_editor_property("m_ui_settings", ui)


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save_to_disk(asset, report, retries=12):
    """Saves and proves the file changed: while another process holds the package the save fails silently."""
    file_path = disk_path(DA_PATH)
    for attempt in range(retries):
        before = os.path.getmtime(file_path)
        unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if os.path.getmtime(file_path) > before:
            report.append("saved %s" % time.ctime(os.path.getmtime(file_path)))
            return True
        report.append("save did not reach disk, retry %d" % attempt)
        time.sleep(10)
    report.append("SAVE FAILED")
    return False


def describe(name, obj):
    costs = obj.get_editor_property("m_ability_costs")
    strafing = obj.get_editor_property("m_aircraft_strafing_settings").export_text()
    ui = obj.get_editor_property("m_ui_settings")
    icon = ui.get_editor_property("ability_icon")
    return "%s %s cd=%s costs=%s tier=%s icon=%s title=%s strafing=%s" % (
        name, obj.get_name(), costs.get_editor_property("cool_down_time"),
        len(costs.get_editor_property("costs").get_editor_property("resource_costs")),
        obj.get_editor_property("m_ability_tier"), icon.get_name() if icon else None,
        ui.get_editor_property("title"), strafing[:400])


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    ga_enum = type(next(iter(templates.keys())))
    before = len(templates)
    report, errors = ["%d entries before" % before], []
    strafe_source = templates.get(member(ga_enum, SOURCE_STRAFE))
    marker_source = templates.get(member(ga_enum, SOURCE_MARKER))
    if strafe_source is None or marker_source is None:
        return report + ["ERROR source template missing", "NOT SAVED"]
    for name, class_path, count, cooldown, title, description in TEMPLATES:
        try:
            key = member(ga_enum, name)
            obj = templates.get(key)
            if obj is None:
                obj = unreal.new_object(strafe_source.get_class(), outer=asset, name="GA_Enemy_" + name)
                report.append("created %s" % name)
            copy_props(strafe_source, obj)
            obj.set_editor_property("m_ability_marker", detached_copy(marker_source.get_editor_property(
                "m_ability_marker")))
            obj.set_editor_property("m_ability_tier", marker_source.get_editor_property("m_ability_tier"))
            set_aircraft(obj, class_path, count)
            set_enemy_costs(obj, cooldown)
            clear_required_technology(obj)
            set_ui(obj, title, description)
            obj.set_editor_property("m_ability_type", key)
            templates[key] = obj
        except Exception as error:
            errors.append("%s: %s" % (name, error))
    if errors:
        return report + ["ERROR " + error for error in errors] + ["NOT SAVED"]
    asset.set_editor_property("m_ability_templates_by_type", templates)
    if not save_to_disk(asset, report):
        return report
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    report.append("entries before=%d after=%d" % (before, len(check)))
    for name, *_ in TEMPLATES:
        report.append("VERIFY " + describe(name, check[member(ga_enum, name)]))
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
