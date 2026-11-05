"""Adds the Sonnengewehr template (GA_GerSonnengewehr, UGA_Sonnengewehr) to DA_GlobalAbilitySystem.

Run headless with a DebugGame editor DLL that knows UGA_Sonnengewehr, holding
Saved/AgentLocks/da_globalabilities.lockdir (see run_sonnengewehr_headless.py):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<run_sonnengewehr_headless.py>
Saving the data asset from an editor whose DLL lacks UGA_Sonnengewehr drops this template.

Idempotent: touches only the GA_GerSonnengewehr key; re-running updates the existing template. Every struct is built
fresh and filled by import_text, never edited in place (struct wrappers fetched from another template alias it).
Saves only when the materials exist. Design: Source/Docs/Abilities/Commanders.md (Light Bringer, Tier III bands).
"""
import time

import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
FOLDER = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/Sonnengewehr/"
# Placeholder icon until a Sonnengewehr render exists: the warm flare curtain icon.
ICON = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/T_GlobalAbility_GerFlareCurtain"
BEACON = "/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Niagara/NS_GlobalAbility_Beacon"
WARNING_SOUND = "/Game/RTS_Survival/Sounds/WeaponSounds/Lasers/HeavyLaser/SuperHeavyLaser_Charging"
BEAM_SOUND = "/Game/RTS_Survival/Sounds/WeaponSounds/Lasers/MedLaser/Impact_Laser_Long"
LOG_PREFIX = "[Sonnengewehr] "
SAVE_ATTEMPTS = 150
SAVE_RETRY_SECONDS = 2.0

KEY_NAME = "GA_GER_SONNENGEWEHR"
OBJECT_NAME = "GA_Sonnengewehr_Ger"
TITLE = "Sonnengewehr"
DESCRIPTION = ("The orbital sun gun burns the line you draw. After a 4 second warning a blinding beam comes down at "
               "the start and sweeps to the end in 5 seconds, burning everything in its 7 m path with heavy laser "
               "damage that ignores armour thickness. It hurts both sides.")

# Tuning (Commanders.md: every global ability costs radixite only, Tier III cooldown 180-240 s).
COOLDOWN = 240
RADIXITE = 800
BEAM_HALF_WIDTH = 350.0
MIN_STRIP = 1500.0
MAX_STRIP = 6000.0
LENGTH_BEFORE_ANCHOR = 3000.0

SETTINGS = ("(WarningSeconds=4.000000,IgnitionSeconds=0.600000,SweepSeconds=5.000000,FadeSeconds=0.800000,"
            "BeamDamage=1400.000000,DamageIntervalSeconds=0.100000,AffectedUnits=OverlapBoth,"
            "ScorchCoolSeconds=8.000000,ScorchLifeSeconds=90.000000)")


# The beam's look, written out so a re-run restores it (C++ defaults of FSonnengewehrVisuals match).
LOOK = ("CoreColor=(R=1.0,G=0.85,B=0.3,A=1.0),GlowColor=(R=1.0,G=0.72,B=0.2,A=1.0),"
        "HaloColor=(R=1.0,G=0.36,B=0.04,A=1.0),CoreDiameter=180.0,GlowDiameter=520.0,HaloDiameter=1500.0,"
        "CoreIntensity=40.0,GlowIntensity=10.0,HaloIntensity=1.5,BeamHeight=60000.0,ImpactGlowDiameter=1900.0,"
        "ImpactIntensity=12.0,ImpactFlashDiameter=650.0,LightColor=(R=1.0,G=0.78,B=0.38,A=1.0),"
        "LightIntensity=8000000.0,LightRadius=5000.0")


def path(asset):
    return "%s.%s" % (asset, asset.split("/")[-1])


def visuals():
    return ('(BeamMaterial="%s",ImpactMaterial="%s",ScorchMaterial="%s",WarningAreaMaterial="%s",WarningSound="%s",'
            'BeamSound="%s",%s)' % (path(FOLDER + "M_SonnengewehrBeam"), path(FOLDER + "M_SonnengewehrImpact"),
                                 path(FOLDER + "M_SonnengewehrScorch"), path(FOLDER + "M_TargetArea_Sonnengewehr"),
                                 path(WARNING_SOUND), path(BEAM_SOUND), LOOK))


def aim():
    return ('(AimType=None,AimRadius=500.000000,AreaAim=(Shape=Strip,AreaMaterial="%s",AreaRadius=%f,InnerRadius=0.0,'
            'MinStripLength=%f,MaxStripLength=%f,LengthBeforeAnchor=%f,ShaderMarkerCount=0,DropPointMesh=None,'
            'DropPointMaterial=None,DropPointScale=3.000000))'
            % (path(FOLDER + "M_TargetArea_Sonnengewehr"), BEAM_HALF_WIDTH, MIN_STRIP, MAX_STRIP,
               LENGTH_BEFORE_ANCHOR))


def fresh(obj, prop, text):
    """A new struct filled from text: edited copies of get_editor_property have leaked between templates before."""
    value = type(obj.get_editor_property(prop))()
    value.import_text(text)
    obj.set_editor_property(prop, value)
    return value


def log_tier_iii_costs(templates):
    for key, template in templates.items():
        if template is None or str(template.get_editor_property("m_ability_tier")).find("TIER_III") < 0:
            continue
        costs = template.get_editor_property("m_ability_costs")
        resources = costs.get_editor_property("costs").get_editor_property("resource_costs")
        unreal.log(LOG_PREFIX + "TIER_III %s %s cooldown=%d" % (
            key.name, {str(k).split(".")[-1]: v for k, v in resources.items()},
            costs.get_editor_property("cool_down_time")))


def run():
    for material in ("M_SonnengewehrBeam", "M_SonnengewehrImpact", "M_SonnengewehrScorch", "M_TargetArea_Sonnengewehr"):
        if not unreal.EditorAssetLibrary.does_asset_exist(FOLDER + material):
            return ["ABORT missing %s, run build_sonnengewehr_assets.py first" % material]

    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    enum_type = type(next(iter(templates.keys())))
    key = getattr(enum_type, KEY_NAME)
    report = ["%d entries before" % len(templates)]
    log_tier_iii_costs(templates)

    obj = templates.get(key)
    if obj is None or obj.get_class().get_name() != "GA_Sonnengewehr":
        obj = unreal.new_object(unreal.GA_Sonnengewehr, outer=asset, name=OBJECT_NAME)
        report.append("created template")
    obj.set_editor_property("m_ability_type", key)
    obj.set_editor_property("m_ability_tier", unreal.GlobalAbilityTier.TIER_III)

    # ResourceCosts is EditDefaultsOnly: python refuses set_editor_property on a struct copy, import_text works.
    costs = fresh(obj, "m_ability_costs",
                  "(Costs=(ResourceCosts=((Resource_Radixite, %d))),CoolDownTime=%d,"
                  "CoolDownRemaining=0)" % (RADIXITE, COOLDOWN))
    report.append("costs=%s" % costs.export_text())

    fresh(obj, "m_aim_settings", aim())
    fresh(obj, "m_ability_marker", '(MarkerEffect="%s",MarkerColor=(R=1.000000,G=0.780000,B=0.200000,A=1.000000),'
                                   'EffectTotalTime=12.000000)' % path(BEACON))
    ui = type(obj.get_editor_property("m_ui_settings"))()
    ui.set_editor_property("ability_icon", unreal.load_asset(ICON))
    ui.set_editor_property("title", unreal.Text("<Text_Title>%s</>" % TITLE))
    ui.set_editor_property("description", unreal.Text(DESCRIPTION))
    obj.set_editor_property("m_ui_settings", ui)
    fresh(obj, "m_sonnengewehr_settings", SETTINGS)
    fresh(obj, "m_sonnengewehr_visuals", visuals())

    templates[key] = obj
    asset.set_editor_property("m_ability_templates_by_type", templates)
    # Other sessions' -game runs (RTS_Survival_AddOnTest shares this Content) hold the file open while they run:
    # the save fails with error 32 until one ends, so keep retrying for a while.
    saved = False
    for attempt in range(SAVE_ATTEMPTS):
        saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
        if saved:
            report.append("save attempt %d ok" % (attempt + 1))
            break
        time.sleep(SAVE_RETRY_SECONDS)
    check = asset.get_editor_property("m_ability_templates_by_type")[key]
    report.append("saved=%s entries=%d class=%s tier=%s cooldown=%d" % (
        saved, len(templates), check.get_class().get_name(), check.get_editor_property("m_ability_tier"),
        check.get_editor_property("m_ability_costs").get_editor_property("cool_down_time")))
    report.append("aim=%s" % check.get_editor_property("m_aim_settings").export_text())
    report.append("settings=%s" % check.get_editor_property("m_sonnengewehr_settings").export_text())
    report.append("visuals=%s" % check.get_editor_property("m_sonnengewehr_visuals").export_text())
    return report


if __name__ == "__main__":
    for line in run():
        unreal.log(LOG_PREFIX + line)
