"""Authors the commander kit abilities in DA_GlobalAbilitySystem. Run inside the editor (UECP execute_python) or
headless: UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file>.
Idempotent: existing entries are updated, missing ones are copied from a source template of the same class.
Saves only when every step succeeded. Design: Source/Docs/Abilities/Commanders.md.

Needs an editor DLL that knows EGlobalAbilityTier, the 2026-11 EGlobalAbility values and UGA_GliderTankDrop;
saving the data asset from an older DLL drops entries it cannot load.
"""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
BF110_CLASS = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BF109/Ger_Aircraft_BF110Airborne.Ger_Aircraft_BF110Airborne_C"
LOG_PREFIX = "[CommanderAbilities] "


def pythonize(ue_name):
    """Unreal's Python name for an enum value: an underscore goes before a capital that follows a lower-case
    letter or digit-free lower run, or that starts a new word (next char lower-case); then upper-case.
    E.g. GA_HeavyCarpet_Ju390B_250Gr -> GA_HEAVY_CARPET_JU390B_250_GR, GA_3xJu87Strafing -> GA_3X_JU87_STRAFING."""
    out = []
    for i, ch in enumerate(ue_name):
        prev = ue_name[i - 1] if i else ""
        nxt = ue_name[i + 1] if i + 1 < len(ue_name) else ""
        if ch.isupper() and prev and prev != "_" and (prev.islower() or (nxt.islower() and nxt != "")):
            out.append("_")
        out.append(ch)
    return "".join(out).upper()


def member(enum_cls, ue_name):
    """Enum member by its C++ name. dir() does not list Unreal enum members, so derive the Python name."""
    for candidate in (pythonize(ue_name), ue_name.upper()):
        value = getattr(enum_cls, candidate, None)
        if value is not None:
            return value
    raise KeyError("%s has no member %s (tried %s)" % (enum_cls.__name__, ue_name, pythonize(ue_name)))


# unreal.GlobalAbility is the UGlobalAbility class (UGlobalAbility and EGlobalAbility share a Python name), so the
# enum type is taken from the data asset's map keys in run().
GA_ENUM = None


def ga(name):
    return member(GA_ENUM, name)


def tank_subtype(name):
    return member(unreal.TankSubtype, name)


def squad(name):
    return member(unreal.SquadSubtype, name)


def tier(name):
    return member(unreal.GlobalAbilityTier, name)


R = member(unreal.RTSResourceType, "Resource_Radixite")
VP = member(unreal.RTSResourceType, "Resource_VehicleParts")
M = member(unreal.RTSResourceType, "Resource_Metal")

# New templates: (ability, copy-from ability, title, description, settings edit function).
NEW = [
    ("GA_HarvesterDrop", "GA_GerSpotterPzIIIDrop", "Panzer IV Harvester Drop",
     "A transport lands a Panzer IV harvester at the target, ready to gather.", "drop_pz4_harvester"),
    ("GA_GerPzIHarvesterDrop", "GA_GerSpotterPzIIIDrop", "Panzer I Harvester Drop",
     "A transport lands a salvaged Panzer I harvester at the target: cheap and early.", "drop_pz1_harvester"),
    ("GA_GerPz38tRailgunDrop", "GA_GerSpotterPzIIIDrop", "Pz 38(t) Railgun Drop",
     "A transport lands two Pz 38(t) railgun tank destroyers at the target.", "drop_pz38t_railguns"),
    ("GA_ScavengersDrop", "GA_GerSpotterPzIIIDrop", "Scavenger Drop",
     "A transport lands two scavenger squads and a mortar team at the target.", "drop_scavengers"),
    ("GA_Ju87Strafing", "GA_3xJu87Strafing", "Ju 87 Strafing",
     "A single Ju 87 attacks along the marked strip: a cheaper, smaller strike.", "strafe_single_ju87"),
    ("GA_Me410Airstrike", "GA_3xJu87Strafing", "Me 410 Strike",
     "A single Me 410 heavy fighter attacks along the marked strip.", "strafe_me410"),
    ("GA_2xBf110Strafing", "GA_2xHortentrafing", "2x Bf 110 Strafing",
     "Two Bf 110 heavy fighters strafe along the marked strip.", "strafe_bf110"),
    ("GA_LargeBarrage", "GA_Barrage", "Large Barrage",
     "A long off-map barrage saturates a wide area with heavy shells.", "barrage_large"),
    ("GA_GerRailBombardment", "GA_Barrage", "Rail Bombardment",
     "A few high-penetration rail slugs fall from high altitude on the target: built to crack heavy armour.",
     "barrage_rail"),
    ("GA_GerSturmtigerDrop", "GA_GerE100Drop", "Mammut: Sturmtiger Delivery",
     "A Mammut heavy-lift glider delivers a Sturmtiger at the end of the marked run.", "glider_sturmtiger"),
    ("GA_GerJagdTigerDrop", "GA_GerE100Drop", "Mammut: Scrap JagdTiger Delivery",
     "A Mammut heavy-lift glider delivers the Scrap Warlord's JagdTiger, rearmed with a 128 mm cannon, at the end "
     "of the marked run.", "glider_jagdtiger"),
    ("GA_GerKingTiger105Drop", "GA_GerE100Drop", "Mammut: King Tiger 105 Delivery",
     "A Mammut heavy-lift glider delivers a King Tiger with the 105 mm gun at the end of the marked run.",
     "glider_kt105"),
    ("GA_GerTigerRailDrop", "GA_GerE100Drop", "Mammut: Tiger Rail Delivery",
     "A Mammut heavy-lift glider delivers a Tiger Rail at the end of the marked run.", "glider_tigerrail"),
]

# Existing templates whose UI text was never filled in but that are in a commander kit.
UI_FIXES = [
    ("GA_Barrage", "Artillery Barrage", "An off-map battery shells the target area with heavy rounds."),
]

# Tier, costs ({resource: amount}, None = computed) and cooldown in seconds (Commanders.md, cost bands).
BALANCE = [
    ("GA_GerReconFlight", "TierI", {R: 150}, 60),
    ("GA_GerFlareCurtain", "TierI", {R: 200}, 90),
    # Created by author_spies_infiltration.py.
    ("GA_GerSpiesInfiltration", "TierI", {R: 200}, 180),
    ("GA_HarvesterDrop", "TierI", {R: 500}, 90),
    ("GA_GerPzIHarvesterDrop", "TierI", {R: 350}, 60),
    ("GA_GerParatroopers", "TierII", {R: 650, M: 125}, 150),
    ("GA_GerSpotterPzIIIDrop", "TierII", {R: 650, VP: 300}, 150),
    ("GA_GerRadixitePzIIMarderDrop", "TierII", None, 150),
    ("GA_ScavengersDrop", "TierII", {R: 1000, M: 150}, 120),
    ("GA_GerPz38tRailgunDrop", "TierII", {R: 500, VP: 250}, 150),
    ("GA_Ju87Strafing", "TierIII", {R: 300, VP: 120}, 180),
    ("GA_3xJu87Strafing", "TierIII", {R: 700, VP: 300}, 240),
    ("GA_2xHortentrafing", "TierIII", {R: 600, VP: 250}, 240),
    ("GA_2xBf110Strafing", "TierIII", {R: 500, VP: 200}, 210),
    ("GA_Me410Airstrike", "TierIII", {R: 400, VP: 150}, 180),
    ("GA_Barrage", "TierIII", {R: 500}, 180),
    ("GA_LargeBarrage", "TierIII", {R: 900}, 240),
    ("GA_GerRailBombardment", "TierIII", {R: 700, VP: 200}, 240),
    ("GA_GerAirMinefield", "TierIII", {R: 600}, 180),
    ("GA_GerRadixiteNuke", "TierIV", {R: 2000, VP: 500}, 420),
    # Created by GlobalAbilityTargetAreas/setup_v2_rocket.py.
    ("GA_GerV2Rocket", "TierIV", {R: 1800, VP: 450}, 360),
    ("GA_GerKingTiger105Drop", "TierIV", {R: 1600, VP: 850}, 360),
    ("GA_GerTigerRailDrop", "TierIV", {R: 1650, VP: 850}, 360),
    ("GA_GerSturmtigerDrop", "TierIV", {R: 1750, VP: 800}, 360),
    ("GA_GerJagdTigerDrop", "TierIV", {R: 1700, VP: 1050}, 360),
    ("GA_GerE100Drop", "TierIV", {R: 1900, VP: 1250}, 360),
]


def tank(name):
    entry = unreal.AircraftDropTankWithOffset()
    entry.set_editor_property("tank_subtype", tank_subtype(name))
    return entry


def edit_drop(obj, tanks, squads=()):
    settings = obj.get_editor_property("m_aircraft_drop_settings")
    settings.set_editor_property("tanks_with_offsets", tanks)
    settings.set_editor_property("squad_subtypes", list(squads))
    obj.set_editor_property("m_aircraft_drop_settings", settings)


def drop_pz4_harvester(obj):
    edit_drop(obj, [tank("Tank_PzIV_Harvester")])


def drop_pz1_harvester(obj):
    edit_drop(obj, [tank("Tank_PzI_Harvester")])


def drop_pz38t_railguns(obj):
    edit_drop(obj, [tank("Tank_Pz38t_RailGun"), tank("Tank_Pz38t_RailGun")])


def drop_scavengers(obj):
    edit_drop(obj, [], [squad("Squad_Ger_Scavengers"), squad("Squad_Ger_Scavengers"), squad("Squad_Ger_GrW42_80mm")])


def strafe_single_ju87(obj):
    settings = obj.get_editor_property("m_aircraft_strafing_settings")
    settings.set_editor_property("aircraft_classes", list(settings.get_editor_property("aircraft_classes"))[:1])
    obj.set_editor_property("m_aircraft_strafing_settings", settings)


ME410_CLASS = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/ME410/Ger_Aircraft_ME5_Airbrone.Ger_Aircraft_ME5_Airbrone_C"


def set_strafing_aircraft(obj, class_path, count):
    settings = obj.get_editor_property("m_aircraft_strafing_settings")
    last_error = None
    for value in (unreal.load_class(None, class_path), unreal.SoftClassPath(class_path)):
        try:
            settings.set_editor_property("aircraft_classes", [value] * count)
            break
        except Exception as error:
            last_error = error
    else:
        raise last_error
    obj.set_editor_property("m_aircraft_strafing_settings", settings)


def strafe_me410(obj):
    set_strafing_aircraft(obj, ME410_CLASS, 1)


def strafe_bf110(obj):
    settings = obj.get_editor_property("m_aircraft_strafing_settings")
    last_error = None
    for value in (unreal.SoftClassPath(BF110_CLASS), unreal.load_class(None, BF110_CLASS)):
        try:
            settings.set_editor_property("aircraft_classes", [value, value])
            break
        except Exception as error:
            last_error = error
    else:
        raise last_error
    obj.set_editor_property("m_aircraft_strafing_settings", settings)


def barrage_large(obj):
    settings = obj.get_editor_property("m_barrage_settings")
    settings.set_editor_property("amount_shells", 40)
    settings.set_editor_property("radius", 2500.0)
    obj.set_editor_property("m_barrage_settings", settings)


def barrage_rail(obj):
    settings = obj.get_editor_property("m_barrage_settings")
    settings.set_editor_property("amount_shells", 6)
    settings.set_editor_property("burst_amount", 1)
    settings.set_editor_property("radius", 600.0)
    settings.set_editor_property("min_time_between_burst", 0.6)
    settings.set_editor_property("max_time_between_burst", 1.2)
    obj.set_editor_property("m_barrage_settings", settings)
    weapon = obj.get_editor_property("m_weapon_data")
    # Kinetic slugs: high penetration and direct damage, almost no blast.
    weapon.set_editor_property("armor_pen", 400.0)
    weapon.set_editor_property("armor_pen_max_range", 380.0)
    weapon.set_editor_property("base_damage", 900.0)
    weapon.set_editor_property("tnt_explosive_grams", 5.0)
    weapon.set_editor_property("shrapnel_range", 150.0)
    weapon.set_editor_property("shrapnel_damage", 20.0)
    weapon.set_editor_property("shrapnel_particles", 6)
    obj.set_editor_property("m_weapon_data", weapon)


HEAVY = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/"


def set_class_override(struct, prop, class_path):
    """Sets a soft class pointer to a Blueprint class and proves it by reading it back. (Clearing one from Python
    is silently ignored, so copied templates get their own class rather than an empty override.)"""
    for value in (unreal.load_class(None, class_path), unreal.SoftClassPath(class_path)):
        try:
            struct.set_editor_property(prop, value)
        except Exception:
            continue
        if class_path.split(".")[-1] in str(struct.get_editor_property(prop)):
            return
    raise RuntimeError("could not set %s to %s (still %s)" % (prop, class_path, struct.get_editor_property(prop)))


def glider(obj, tank_name, class_path):
    settings = obj.get_editor_property("m_glider_drop_settings")
    settings.set_editor_property("tank_subtype", tank_subtype(tank_name))
    # The E-100 source template carries the E-100 Blueprint as override; replace it with this drop's tank.
    set_class_override(settings, "tank_class_override", HEAVY + class_path)
    obj.set_editor_property("m_glider_drop_settings", settings)


def glider_sturmtiger(obj):
    glider(obj, "Tank_SturmTiger", "SturmTiger/Ger_Vehicle_SturmTiger.Ger_Vehicle_SturmTiger_C")


def glider_jagdtiger(obj):
    # The Scrap Warlord's JagdTiger: the stock JagdTiger fires the Sonnensturm laser, this one the Maus's 128 mm.
    glider(obj, "Tank_JagdTiger", "JagdTiger/Scrap128/Ger_Vehicle_JagdTiger_Scrap128.Ger_Vehicle_JagdTiger_Scrap128_C")


def glider_kt105(obj):
    glider(obj, "Tank_Tiger105", "TigerII/Ger_Vehicle_TigerII_105.Ger_Vehicle_TigerII_105_C")


def glider_tigerrail(obj):
    glider(obj, "Tank_TigerRail", "Tiger/Ger_Vehicle_TigerH1_Rail.Ger_Vehicle_TigerH1_Rail_C")


COPIED_PROPERTIES = ("m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                     "m_ui_settings", "m_ability_marker", "m_ability_tier", "m_aircraft_drop_settings",
                     "m_aircraft_strafing_settings", "m_barrage_settings", "m_projectile_mover", "m_weapon_data",
                     "m_weapon_vfx", "m_glider_drop_settings")


def copy_props(source, target):
    copied = []
    for prop in COPIED_PROPERTIES:
        try:
            value = source.get_editor_property(prop)
        except Exception:
            continue  # not a property of this class
        target.set_editor_property(prop, value)
        copied.append(prop)
    return copied


def set_ui(obj, title, description):
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("title", "<Text_Title>%s</>" % title)
    ui.set_editor_property("description", description)
    obj.set_editor_property("m_ui_settings", ui)


def set_balance(obj, tier_name, costs, cooldown):
    obj.set_editor_property("m_ability_tier", tier(tier_name))
    cost_state = obj.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", costs)
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", cooldown)
    obj.set_editor_property("m_ability_costs", cost_state)


def marder_drop_cost(obj):
    """1.25x the training cost of what the radixite Pz II + Marder drop delivers, rounded to 25."""
    unit_costs = {str(tank_subtype("Tank_PzII_F")): (200, 50), str(tank_subtype("Tank_Marder")): (180, 80)}
    radixite = parts = 0
    drop = obj.get_editor_property("m_aircraft_drop_settings")
    for entry in drop.get_editor_property("tanks_with_offsets"):
        r, p = unit_costs.get(str(entry.get_editor_property("tank_subtype")), (250, 100))
        radixite, parts = radixite + r, parts + p
    return {R: int(round(radixite * 1.25 / 25.0)) * 25, VP: int(round(parts * 1.25 / 25.0)) * 25}


def run():
    global GA_ENUM
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    GA_ENUM = type(next(iter(templates.keys())))
    report, errors = ["enum type %s, %d entries before" % (GA_ENUM.__name__, len(templates))], []
    for name, source_name, title, description, edit in NEW:
        try:
            key = ga(name)
            obj = templates.get(key)
            if obj is None:
                source = templates.get(ga(source_name))
                if source is None:
                    raise RuntimeError("source template %s missing" % source_name)
                obj = unreal.new_object(source.get_class(), outer=asset, name="GA_Commander_" + name)
                copied = copy_props(source, obj)
                report.append("created %s from %s (%d properties)" % (name, source_name, len(copied)))
            globals()[edit](obj)
            set_ui(obj, title, description)
            obj.set_editor_property("m_ability_type", key)
            templates[key] = obj
        except Exception as error:
            errors.append("%s: %s" % (name, error))
    for name, title, description in UI_FIXES:
        try:
            set_ui(templates[ga(name)], title, description)
        except Exception as error:
            errors.append("%s: %s" % (name, error))
    for name, tier_name, costs, cooldown in BALANCE:
        try:
            obj = templates.get(ga(name))
            if obj is None:
                raise RuntimeError("no template")
            if costs is None:
                costs = marder_drop_cost(obj)
            set_balance(obj, tier_name, costs, cooldown)
            report.append("balanced %s %s %s cd=%d" % (name, tier_name,
                                                       {str(k).split(".")[-1]: v for k, v in costs.items()}, cooldown))
        except Exception as error:
            errors.append("%s: %s" % (name, error))
    if errors:
        report += ["ERROR " + e for e in errors] + ["NOT SAVED"]
        return report
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    report.append("saved=%s entries=%d" % (saved, len(templates)))
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
