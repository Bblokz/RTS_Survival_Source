"""Adds the V2 rocket (GA_GerV2Rocket) to DA_GlobalAbilitySystem and builds its fire-coloured target area and
shockwave shaders. It is a UGA_RadixiteNuke template: the rocket flies in on a low arc, the blast is bigger than the
nuke's but there is no radiation. Only touches its own template; the first run copies the nuke's requirements, sounds
and marker. Saves only when every step succeeded. Needs an editor DLL that knows GA_GerV2Rocket and the arc settings.
    UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file>
Run import_ability_icons.py afterwards for the icon."""
import unreal

# Loaded as a namespace rather than imported: the editor's execute_python bridge does not allow sys.path edits.
MATERIAL_BUILDER = {"__name__": "build_target_area_materials"}
exec(open("D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityTargetAreas/build_target_area_materials.py",
          encoding="utf-8").read(), MATERIAL_BUILDER)

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
AREAS = "/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/"
LOG_PREFIX = "[V2Rocket] "
SOURCE = "GA_GerRadixiteNuke"
OBJECT_NAME = "GA_V2Rocket_Ger"
COPIED_PROPERTIES = ("m_ability_requirements", "m_ability_sound_settings", "m_ability_marker", "m_ui_settings",
                     "m_aim_settings", "m_nuke_visuals")

TITLE = "V2 Rocket"
DESCRIPTION = ("A V2 rocket comes in on a low arc and lands 7 seconds after the order. It hits harder than the "
               "radixite warhead with the same 45 m blast, but leaves no radiation behind. It hurts both sides.")
# Nuke: core 4000, blast 1400, 30 s of radiation; the V2 trades the radiation for a 1.5x blast.
SETTINGS = ("(WarningSeconds=7.000000,FallSeconds=7.000000,FallStartHeight=9000.000000,"
            "ArcApproachDistance=30000.000000,ArcStartHeight=1500.000000,ArcApexHeight=4000.000000,"
            "CoreDamage=6000.000000,BlastDamage=2100.000000,BlastFalloffExponent=1.300000,"
            "BlastFullDamageArmor=120.000000,BlastArmorFalloff=1.200000,BlastMaxArmor=400.000000,"
            "RadiationDamagePerPulse=0.000000,RadiationPulseSeconds=2.000000,RadiationSeconds=0.000000,"
            "AffectedUnits=OverlapBoth)")
TIER = "TierIV"
COOLDOWN = 360


def path(asset):
    return "%s.%s" % (asset, asset.split("/")[-1])


VISUALS = {
    "WarheadMesh": path("/Game/Ballistic_Missiles/Mesh/v2"),
    "WarheadTrailEffect": path("/Game/RocketThrusterExhaustFX/FX/NS_RocketExhaust_Realistic"),
    "WarheadSmokeTrailEffect": path("/Game/RTS_Survival/SpecialEffects/ArmyVFX/Niagara/Projectiles/NS_Rocket_Smoke_1"),
    "DetonationEffect": path("/Game/BigExplosions/Niagara/NS_Nuke_3"),
    "RadiationEffect": None,
    "WarningAreaMaterial": path(AREAS + "M_TargetArea_V2Rocket"),
    "ShockwaveMaterial": path(AREAS + "M_V2RocketShockwave"),
    "ScarMaterial": path(AREAS + "M_RadixiteNukeScar"),
    "WarningSound": path("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Megaphone/Sound/S_SirenStart"),
}
VISUAL_TEXT = ("WarheadScale=1.000000,WarheadRotation=(Pitch=-90.000000,Yaw=0.000000,Roll=0.000000),"
               "WarheadTrailOffset=(X=0.000000,Y=0.000000,Z=0.000000),"
               "WarheadTrailRotation=(Pitch=-90.000000,Yaw=0.000000,Roll=0.000000),"
               "WarheadTrailScale=6.000000,"
               "DetonationEffectScale=3.000000,bWarningSound2D=True,"
               "FlashColor=(R=1.000000,G=0.550000,B=0.220000,A=1.000000),FlashIntensity=40000000.000000")


def member(enum_cls, names):
    for name in names:
        value = getattr(enum_cls, name, None)
        if value is not None:
            return value
    raise KeyError("%s has none of %s" % (enum_cls.__name__, names))


def fresh(obj, prop):
    """A new struct per template: edited copies of get_editor_property have leaked between templates before."""
    old = obj.get_editor_property(prop)
    new = type(old)()
    new.import_text(old.export_text())
    return new


def clear_required_technology(obj):
    """No copied technology gate (round 5, user 2026-11-05): GA_3xJu87Strafing carried Tech_HarvesterSpeed, which
    silently blocked the click, and every copy inherited it. Keeps the unit requirement."""
    old = obj.get_editor_property("m_ability_requirements")
    requirements = type(old)()
    requirements.import_text(old.export_text())
    for attr in dir(unreal.Technology):
        if attr.replace("_", "").lower() == "technone":
            requirements.set_editor_property("required_technology", getattr(unreal.Technology, attr))
    obj.set_editor_property("m_ability_requirements", requirements)


def set_visuals(obj):
    visuals = fresh(obj, "m_nuke_visuals")
    assets = ",".join('%s=%s' % (key, '"%s"' % value if value else "None") for key, value in VISUALS.items())
    visuals.import_text("(%s,%s)" % (assets, VISUAL_TEXT))
    obj.set_editor_property("m_nuke_visuals", visuals)


def set_aim(obj):
    aim = fresh(obj, "m_aim_settings")
    area = aim.get_editor_property("area_aim")
    area.set_editor_property("area_material", unreal.load_asset(AREAS + "M_TargetArea_V2Rocket"))
    aim.set_editor_property("area_aim", area)
    obj.set_editor_property("m_aim_settings", aim)


def set_balance(obj):
    obj.set_editor_property("m_ability_tier", member(unreal.GlobalAbilityTier, ["TIER_IV", "TIERIV"]))
    # ResourceCosts cannot be set on an instance from Python; text import can. Radixite only (user decision 2026-11-05).
    costs = fresh(obj, "m_ability_costs")
    costs.import_text("(Costs=(ResourceCosts=((Resource_Radixite, 1800))),"
                      "CoolDownTime=%d,CoolDownRemaining=0)" % COOLDOWN)
    obj.set_editor_property("m_ability_costs", costs)


def set_ui(obj):
    ui = fresh(obj, "m_ui_settings")
    ui.set_editor_property("title", unreal.Text("<Text_Title>%s</>" % TITLE))
    ui.set_editor_property("description", unreal.Text(DESCRIPTION))
    obj.set_editor_property("m_ui_settings", ui)


def run():
    MATERIAL_BUILDER["build_v2_rocket"]()
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    enum = type(next(iter(templates.keys())))
    report = ["%d entries before" % len(templates)]
    key = member(enum, ["GA_GER_V2_ROCKET", "GA_GER_V_2_ROCKET", "GA_GERV2_ROCKET"])
    obj = templates.get(key)
    if obj is None:
        source = templates.get(member(enum, ["GA_GER_RADIXITE_NUKE"]))
        if source is None:
            return report + ["ERROR source template %s missing" % SOURCE, "NOT SAVED"]
        # A run that failed after creating the object leaves it in the asset, outside the map.
        obj = (unreal.find_object(None, "%s:%s" % (asset.get_path_name(), OBJECT_NAME))
               or unreal.new_object(source.get_class(), outer=asset, name=OBJECT_NAME))
        for prop in COPIED_PROPERTIES:
            obj.set_editor_property(prop, fresh(source, prop))
        report.append("created from %s" % SOURCE)
    asset.modify()
    clear_required_technology(obj)
    obj.set_editor_property("m_ability_type", key)
    settings = fresh(obj, "m_nuke_settings")
    settings.import_text(SETTINGS)
    obj.set_editor_property("m_nuke_settings", settings)
    set_visuals(obj)
    set_aim(obj)
    set_balance(obj)
    set_ui(obj)
    templates[key] = obj
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    template = asset.get_editor_property("m_ability_templates_by_type")[key]
    report += ["saved=%s entries=%d" % (saved, len(templates)),
               "settings " + template.get_editor_property("m_nuke_settings").export_text(),
               "visuals " + template.get_editor_property("m_nuke_visuals").export_text(),
               "aim " + template.get_editor_property("m_aim_settings").export_text()[:400],
               "costs " + template.get_editor_property("m_ability_costs").export_text()]
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
