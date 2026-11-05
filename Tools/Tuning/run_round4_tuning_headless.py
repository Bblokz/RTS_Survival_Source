"""Round-4 tuning (2026-11-05, user decisions): one headless DebugGame-Cmd run that re-runs every affected authoring
script, so the data asset and Blueprints match the scripts' values.

  Ju 87 G           Tools/Ju87G/author_ju87g_blueprint.py (CannonArmorPen 120 on the pod component)
                    Tools/Ju87G/author_ju87g_tank_busters.py (no Tech_HarvesterSpeed requirement)
  Sonnengewehr      Tools/Sonnengewehr/author_sonnengewehr_template.py (675 R, bBurnOwnUnits False)
  Field workshop    Tools/FieldWorkshop/author_field_workshop.py (reload-only rearm aura on squads, dust pack-up)
  Resource drops    Tools/ResourceDrop/author_resource_drops.py (1.5x the cost at x1)
  Elite Scavengers  Tools/EliteScavengers/author_elite_scavengers_paradrop.py (500 R)
  Sky-Drop          Tools/SkyDrop/author_sky_drop_abilities.py, only GA_GerMassiveFlare + GA_GerVehiclePartsSupplyDrop

Hold Saved/AgentLocks/debuggame_build.lockdir and da_globalabilities.lockdir:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended
  -NoLiveCoding
TUNING_STEPS (comma list of the step names below) runs a subset. Every template outside MY_KEYS is fingerprinted before
and after; the run logs any difference ([Tuning] prefix).
"""
import os
import traceback

import unreal

TOOLS = "D:/UE5Projects/RTS_Survival/Source/Tools"
DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG = "[Tuning] "
MY_KEYS = ("GA_GerJu87GTankBusters", "GA_GerSonnengewehr", "GA_GerFieldWorkshopDrop", "GA_GerRadixiteMetalSupplyDrop",
           "GA_GerConstructionSupplyDrop", "GA_GerRadixiteCacheDrop", "GA_GerVehiclePartsSupplyDrop",
           "GA_GerEliteScavengersParadrop", "GA_GerMassiveFlare")
STEPS = (
    ("ju87g_bp", "Ju87G/author_ju87g_blueprint.py", {}),
    ("ju87g_da", "Ju87G/author_ju87g_tank_busters.py", {}),
    ("sonnengewehr", "Sonnengewehr/author_sonnengewehr_template.py", {}),
    ("field_workshop", "FieldWorkshop/author_field_workshop.py", {}),
    ("resource_drops", "ResourceDrop/author_resource_drops.py", {}),
    ("elite_paradrop", "EliteScavengers/author_elite_scavengers_paradrop.py", {}),
    ("sky_drop", "SkyDrop/author_sky_drop_abilities.py",
     {"SKYDROP_ONLY_KEYS": ("GA_GerMassiveFlare", "GA_GerVehiclePartsSupplyDrop")}),
)
FINGERPRINT_PROPERTIES = (
    "m_ability_type", "m_ability_tier", "m_ability_costs", "m_ability_requirements", "m_aim_settings",
    "m_ability_sound_settings", "m_ui_settings", "m_ability_marker", "m_aircraft_drop_settings",
    "m_aircraft_strafing_settings", "m_aircraft_bombing_settings", "m_barrage_settings", "m_glider_drop_settings",
    "m_paratroopers_settings", "m_strip_flight_settings", "m_flare_curtain_settings", "m_resource_drop_settings",
    "m_projectile_mover", "m_weapon_data", "m_weapon_vfx")


def log(line):
    unreal.log(LOG + line)


def fingerprint(obj):
    parts = [obj.get_class().get_name(), obj.get_name()]
    for prop in FINGERPRINT_PROPERTIES:
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        parts.append("%s=%s" % (prop, value.export_text() if hasattr(value, "export_text") else str(value)))
    return "|".join(parts)


def key_name(key):
    # str(key) reads like "<GlobalAbility.GA_GER_SONNENGEWEHR: 64>".
    text = str(key).split(".")[-1].split(":")[0]
    return text.replace("_", "").lower()


def is_mine(key):
    return key_name(key) in set(name.replace("_", "").lower() for name in MY_KEYS)


def snapshot():
    templates = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    others = {str(key): fingerprint(obj) for key, obj in templates.items() if obj is not None and not is_mine(key)}
    mine = {}
    for key, obj in templates.items():
        if obj is None or not is_mine(key):
            continue
        costs = obj.get_editor_property("m_ability_costs")
        mine[str(key)] = "class=%s tier=%s cooldown=%s costs=%s requirements=%s" % (
            obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
            costs.get_editor_property("cool_down_time"),
            {str(k).split(".")[-1]: v for k, v in
             costs.get_editor_property("costs").get_editor_property("resource_costs").items()},
            obj.get_editor_property("m_ability_requirements").export_text())
    return len(templates), others, mine


def run_step(name, relative_path, extra_globals):
    path = os.path.join(TOOLS, relative_path)
    log("STEP %s %s" % (name, relative_path))
    scope = {"__name__": "__main__", "__file__": path}
    scope.update(extra_globals)
    with open(path, encoding="utf-8") as handle:
        code = compile(handle.read(), path, "exec")
    try:
        exec(code, scope)
        log("STEP %s done" % name)
    except Exception as error:
        log("STEP %s ERROR %s | %s" % (name, error, traceback.format_exc().replace("\n", " | ")))


def run():
    wanted = [step.strip() for step in os.environ.get("TUNING_STEPS", "").split(",") if step.strip()]
    count_before, others_before, mine_before = snapshot()
    log("entries before %d" % count_before)
    for key, text in sorted(mine_before.items()):
        log("BEFORE %s %s" % (key, text))
    for name, relative_path, extra_globals in STEPS:
        if wanted and name not in wanted:
            continue
        run_step(name, relative_path, extra_globals)
    count_after, others_after, mine_after = snapshot()
    changed = sorted(key for key in others_before if others_after.get(key) != others_before[key])
    missing = sorted(key for key in others_before if key not in others_after)
    log("entries after %d; other templates checked %d changed=%s missing=%s" % (
        count_after, len(others_before), changed, missing))
    for key, text in sorted(mine_after.items()):
        log("AFTER %s %s" % (key, text))


run()
