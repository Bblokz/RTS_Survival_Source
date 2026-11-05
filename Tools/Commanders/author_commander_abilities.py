"""Authors the commander kit abilities in DA_GlobalAbilitySystem. Run inside the editor (UECP execute_python) or
headless: UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this file>.
Idempotent: existing entries are updated, missing ones are copied from a source template of the same class.
Saves only when every step succeeded. Design: Source/Docs/Abilities/Commanders.md.

Needs an editor DLL that knows EGlobalAbilityTier, the 2026-11 EGlobalAbility values and UGA_GliderTankDrop;
saving the data asset from an older DLL drops entries it cannot load.

Round 3 (2026-11-05, seven-slot kits): ROUND3_NEW / ROUND3_BALANCE / ROUND3_EDITS add the data-only drops (Mammut
glider, landed Ju 390, paradrops, single Bf 109) and re-cost GA_GerParatroopers and the Ju 390 carpet bombing. With
SCOPE = "round3" (the default) a run touches only those keys; SCOPE = "all" also re-applies the round-2 lists. The data
asset holds templates of classes the Development DLL lacks (composite, resource and building drops), so run this
headless on the DebugGame build under Saved/AgentLocks/da_globalabilities.lockdir:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
"""
import os
import time

import unreal

SCOPE = "round3"

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
    # Fallback: compare names without underscores and case (dir() lists the members on recent builds).
    wanted = ue_name.replace("_", "").lower()
    for member_name in dir(enum_cls):
        if member_name.replace("_", "").lower() == wanted:
            return getattr(enum_cls, member_name)
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
    ("GA_GerRailTroopersDrop", "GA_ScavengersDrop", "Rail Trooper Drop",
     "A transport lands two squads of Rail Troopers at the target: heavily armoured infantry with rail rifles that "
     "punch through tank armour.", "drop_rail_troopers"),
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
# Costs are radixite only (user decision 2026-11-05: no vehicle parts, metal, fuel or ammo on any global ability).
BALANCE = [
    ("GA_GerReconFlight", "TierI", {R: 150}, 60),
    ("GA_GerFlareCurtain", "TierI", {R: 200}, 90),
    # Created by author_spies_infiltration.py.
    ("GA_GerSpiesInfiltration", "TierI", {R: 200}, 180),
    ("GA_HarvesterDrop", "TierI", {R: 500}, 90),
    ("GA_GerPzIHarvesterDrop", "TierI", {R: 350}, 60),
    ("GA_GerParatroopers", "TierII", {R: 650}, 150),
    ("GA_GerSpotterPzIIIDrop", "TierII", {R: 650}, 150),
    ("GA_GerRadixitePzIIMarderDrop", "TierII", None, 150),
    ("GA_ScavengersDrop", "TierII", {R: 1000}, 120),
    ("GA_GerPz38tRailgunDrop", "TierII", {R: 500}, 150),
    ("GA_GerRailTroopersDrop", "TierII", {R: 1300}, 150),
    ("GA_Ju87Strafing", "TierIII", {R: 300}, 180),
    ("GA_3xJu87Strafing", "TierIII", {R: 700}, 240),
    ("GA_2xHortentrafing", "TierIII", {R: 600}, 240),
    ("GA_2xBf110Strafing", "TierIII", {R: 500}, 210),
    ("GA_Me410Airstrike", "TierIII", {R: 400}, 180),
    ("GA_Barrage", "TierIII", {R: 500}, 180),
    ("GA_LargeBarrage", "TierIII", {R: 900}, 240),
    ("GA_GerRailBombardment", "TierIII", {R: 700}, 240),
    ("GA_GerAirMinefield", "TierIII", {R: 600}, 180),
    ("GA_GerRadixiteNuke", "TierIV", {R: 2000}, 420),
    # Created by GlobalAbilityTargetAreas/setup_v2_rocket.py.
    ("GA_GerV2Rocket", "TierIV", {R: 1800}, 360),
    ("GA_GerKingTiger105Drop", "TierIV", {R: 1600}, 360),
    ("GA_GerTigerRailDrop", "TierIV", {R: 1650}, 360),
    ("GA_GerSturmtigerDrop", "TierIV", {R: 1750}, 360),
    ("GA_GerJagdTigerDrop", "TierIV", {R: 1700}, 360),
    ("GA_GerE100Drop", "TierIV", {R: 1900}, 360),
]


def tank(name, class_path=None):
    """One tank of a Ju 390 drop. A variant needs its Blueprint as override: the spawner row of the subtype holds the
    stock tank (GA_AircraftDrop spawns the override when it is set)."""
    entry = unreal.AircraftDropTankWithOffset()
    entry.set_editor_property("tank_subtype", tank_subtype(name))
    if class_path:
        set_class_override(entry, "tank_class_override", VEHICLES + class_path)
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


def drop_rail_troopers(obj):
    edit_drop(obj, [], [squad("Squad_Ger_RailTroopers"), squad("Squad_Ger_RailTroopers")])


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


VEHICLES = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
HEAVY = VEHICLES + "Ger_Heavy/"


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


# ---------------------------------------------------------------- round 3 (seven-slot kits, 2026-11-05)

def glider_e50(obj):
    glider(obj, "Tank_E50", "E50/Ger_Vehicle_E50.Ger_Vehicle_E50_C")


def glider_e75(obj):
    glider(obj, "Tank_E75", "E75/Ger_Vehicle_E75.Ger_Vehicle_E75_C")


def glider_maus(obj):
    glider(obj, "Tank_Maus", "Maus/Ger_Vehicle_Maus.Ger_Vehicle_Maus_C")


def glider_jagdtiger_laser(obj):
    # The stock JagdTiger fires the Sonnensturm 128L laser; the E-100 source's override must be replaced explicitly.
    glider(obj, "Tank_JagdTiger", "JagdTiger/Ger_Vehicle_JagdTiger.Ger_Vehicle_JagdTiger_C")


def glider_jagdpanther_flamer(obj):
    glider(obj, "Tank_JagdPanther", "JagdPanther/Flamer/Ger_Vehicle_JagdPanther_Flamer.Ger_Vehicle_JagdPanther_Flamer_C")


RADIXITE_PZII = "Ger_Light/PanzerII/Ger_Vehicle_PanzerII_F_RadixiteAutocannon.Ger_Vehicle_PanzerII_F_RadixiteAutocannon_C"
RADIXITE_PZIII = ("Ger_Medium/PanzerIII/RadixiteCannon/Ger_Vehicle_PanzerIII_J_RadixiteCannon."
                  "Ger_Vehicle_PanzerIII_J_RadixiteCannon_C")
RADIXITE_WESPE = "Ger_Light/Wespe/Ger_Vehicle_Wespe_RadixiteHowitzer.Ger_Vehicle_Wespe_RadixiteHowitzer_C"
PZJAGER_LASER = "Ger_Light/PanzerJager1/Ger_Vehicle_PanzerJager_Laser.Ger_Vehicle_PanzerJager_Laser_C"
PZIII_FLAMM = "Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_Flamm.Ger_Vehicle_PanzerIII_Flamm_C"


def drop_radixite_pz2(obj):
    edit_drop(obj, [tank("Tank_PzII_F", RADIXITE_PZII), tank("Tank_PzII_F", RADIXITE_PZII)])


def drop_radixite_pz3_wespe(obj):
    edit_drop(obj, [tank("Tank_PzIII_J", RADIXITE_PZIII), tank("Tank_Wespe", RADIXITE_WESPE)])


def drop_pzjager_laser(obj):
    edit_drop(obj, [tank("Tank_PzJagerLaser", PZJAGER_LASER)] * 3)


def drop_pz3_flamm(obj):
    edit_drop(obj, [tank("Tank_PzIII_FLamm", PZIII_FLAMM), tank("Tank_PzIII_FLamm", PZIII_FLAMM)])


def drop_red_spine(obj):
    # Walkers are squad units, so they land like infantry. Tools/RedSpine/author_red_spine_drops.py also sets
    # RadiusSpawnSquads 1500 (clear of the Ju 390's wings): run it after this script.
    edit_drop(obj, [], [squad("Squad_Ger_RedSpine")])


def drop_red_spine_pair(obj):
    edit_drop(obj, [], [squad("Squad_Ger_RedSpine"), squad("Squad_Ger_RedSpine")])


def set_paradrop_squads(obj, squad_names):
    """A fresh settings struct filled from the template's own text: a wrapper from get_editor_property can alias the
    memory of the template it was copied from (that is how GA_GerParatroopers got overwritten on 2026-11-05)."""
    settings = detached_copy(obj.get_editor_property("m_paratroopers_settings"))
    settings.set_editor_property("squad_subtypes", [squad(name) for name in squad_names])
    obj.set_editor_property("m_paratroopers_settings", settings)


def paradrop_rail_troopers(obj):
    set_paradrop_squads(obj, ["Squad_Ger_RailTroopers"])


def paradrop_light_bringers(obj):
    set_paradrop_squads(obj, ["Squad_Ger_LightBringers"])


def paradrop_feuersturm(obj):
    set_paradrop_squads(obj, ["Squad_Ger_FeuerSturm", "Squad_Ger_FeuerSturm"])


def paradrop_vultures_panzergrenadiere(obj):
    set_paradrop_squads(obj, ["Squad_Ger_Vultures", "Squad_Ger_Vultures",
                              "Squad_Ger_PanzerGrenadiere", "Squad_Ger_PanzerGrenadiere"])


def strafe_single_bf109(obj):
    # Ger_Aircraft_BF110Airborne is an airborne Bf 109: it uses the Bf 109 mesh and AnimBP (there is no Bf 110 model).
    # Ger_Aircraft_BF109 itself starts landed (bM_StartedAsLanded) and would take off first.
    set_strafing_aircraft(obj, BF110_CLASS, 1)


# The Ju 390's run was 500 m (50000), against 30 m for every PE-8 template and a 60 m longest aimable strip: a stray
# zero. 50 m keeps the heavier bomber's run longer than the PE-8's.
JU390_CARPET_LENGTH = 5000.0


def carpet_ju390(obj):
    settings = detached_copy(obj.get_editor_property("m_aircraft_bombing_settings"))
    settings.set_editor_property("carpet_bombing_length", JU390_CARPET_LENGTH)
    obj.set_editor_property("m_aircraft_bombing_settings", settings)


# New round-3 templates: (ability, copy-from ability, title, description, settings edit function).
ROUND3_NEW = [
    ("GA_GerE50Drop", "GA_GerE100Drop", "Mammut: E-50 Delivery",
     "A Mammut heavy-lift glider delivers an E-50 at the end of the marked run: King Tiger armour, the long 88 mm "
     "gun and more speed.", "glider_e50"),
    ("GA_GerE75Drop", "GA_GerE100Drop", "Mammut: E-75 Delivery",
     "A Mammut heavy-lift glider delivers an E-75 with the 128 mm gun at the end of the marked run.", "glider_e75"),
    ("GA_GerMausDrop", "GA_GerE100Drop", "Mammut: Maus Delivery",
     "A Mammut heavy-lift glider delivers the Maus super-heavy tank at the end of the marked run.", "glider_maus"),
    ("GA_GerJagdTigerLaserDrop", "GA_GerE100Drop", "Mammut: JagdTiger Delivery",
     "A Mammut heavy-lift glider delivers a JagdTiger with the Sonnensturm 128L laser, whose beam hits several "
     "targets in a line, at the end of the marked run.", "glider_jagdtiger_laser"),
    ("GA_GerJagdPantherFlamerDrop", "GA_GerE100Drop", "Mammut: Jagdpanther Flamer Delivery",
     "A Mammut heavy-lift glider delivers a Jagdpanther rebuilt around a heavy flamethrower at the end of the marked "
     "run.", "glider_jagdpanther_flamer"),
    ("GA_GerRadixitePzIIDrop", "GA_GerSpotterPzIIIDrop", "Radixite Pz II Drop",
     "Transports land two Pz II with radixite autocannons at the target.", "drop_radixite_pz2"),
    ("GA_GerRadixitePzIIIWespeDrop", "GA_GerSpotterPzIIIDrop", "Radixite Pz III and Wespe Drop",
     "Transports land a Pz III with a radixite cannon and a Wespe with a radixite howitzer at the target.",
     "drop_radixite_pz3_wespe"),
    ("GA_GerPzJagerLaserDrop", "GA_GerSpotterPzIIIDrop", "Panzerjäger Laser Drop",
     "Transports land three Panzerjäger I laser tank destroyers at the target.", "drop_pzjager_laser"),
    ("GA_GerPzIIIFlammDrop", "GA_GerSpotterPzIIIDrop", "Pz III Flamm Drop",
     "Transports land two Pz III flamethrower tanks at the target.", "drop_pz3_flamm"),
    ("GA_GerRedSpineDrop", "GA_GerSpotterPzIIIDrop", "Red Spine Walker Drop",
     "A transport lands a squad of Red Spine light walkers at the target.", "drop_red_spine"),
    ("GA_GerRedSpinePairDrop", "GA_GerSpotterPzIIIDrop", "Red Spine Walker Pair Drop",
     "Transports land two squads of Red Spine light walkers at the target.", "drop_red_spine_pair"),
    ("GA_GerRailTroopersParadrop", "GA_GerEliteScavengersParadrop", "Rail Trooper Paradrop",
     "A transport drops a squad of Rail Troopers over the target: armoured infantry with rail rifles that punch "
     "through tank armour.", "paradrop_rail_troopers"),
    ("GA_GerLightBringersParadrop", "GA_GerEliteScavengersParadrop", "Light Bringer Paradrop",
     "A transport drops a squad of Light Bringers with laser rifles over the target.", "paradrop_light_bringers"),
    ("GA_GerFeuerSturmParadrop", "GA_GerEliteScavengersParadrop", "Feuer Sturm Paradrop",
     "A transport drops two Feuer Sturm flamethrower squads over the target.", "paradrop_feuersturm"),
    ("GA_Bf109Strafing", "GA_Ju87Strafing", "Bf 109 Strafing",
     "A single Bf 109 strafes along the marked strip: a fast, cheap strike.", "strafe_single_bf109"),
]

# Existing templates the round-3 kits change: (ability, title or None to keep, description or None, edit function).
ROUND3_EDITS = [
    ("GA_GerParatroopers", None,
     "A transport drops two Vulture squads and two Panzergrenadiere squads over the target.",
     "paradrop_vultures_panzergrenadiere"),
    ("GA_HeavyCarpet_Ju390B_250Gr", None, None, "carpet_ju390"),
]

# Radixite only (user decision 2026-11-05): about 125% of the radixite in the contents' training cost
# (DeveloperSettings.h UnitCosts, quoted per row), rounded to 25; their other resources are not charged. Radixite
# variants have no cost row of their own: the base unit's cost. Red Spine has no squad data at all: a light-walker guess
# (350 R per squad), to be replaced once the squad exists.
ROUND3_BALANCE = [
    ("GA_GerE50Drop", "TierIV", {R: 1450}, 360),          # E-50 1150 R + 600 VP
    ("GA_GerE75Drop", "TierIV", {R: 1800}, 360),         # E-75 1450 R + 850 VP
    ("GA_GerMausDrop", "TierIV", {R: 1875}, 360),        # Maus 1500 R + 1000 VP
    ("GA_GerJagdTigerLaserDrop", "TierIV", {R: 1700}, 360),  # JagdTiger 1300 R + 850 VP, as the Scrap one
    ("GA_GerJagdPantherFlamerDrop", "TierIV", {R: 750}, 360),  # Jagdpanther 600 R + 300 VP
    ("GA_GerRadixitePzIIDrop", "TierI", {R: 500}, 90),    # 2x Pz II F 200 R + 50 VP
    ("GA_GerRadixitePzIIIWespeDrop", "TierII", {R: 650}, 150),  # Pz III J 250+100, Wespe 275+120
    ("GA_GerPzJagerLaserDrop", "TierII", {R: 375}, 150),  # 3x Panzerjager laser 100 R + 45 VP
    ("GA_GerPzIIIFlammDrop", "TierII", {R: 1000}, 180),   # 2x Pz III Flamm 400 R + 300 VP
    ("GA_GerRedSpineDrop", "TierI", {R: 825}, 90),         # 1.25x one squad's 650 R training cost
    ("GA_GerRedSpinePairDrop", "TierII", {R: 1625}, 150),  # 1.25x two squads
    ("GA_GerRailTroopersParadrop", "TierI", {R: 950}, 90),  # Rail Troopers 750 R + 350 M
    ("GA_GerLightBringersParadrop", "TierI", {R: 1000}, 90),  # Light Bringers 800 R + 200 M
    ("GA_GerFeuerSturmParadrop", "TierI", {R: 1250}, 90),  # 2x Feuer Sturm 500 R + 200 M
    ("GA_Bf109Strafing", "TierI", {R: 200}, 90),           # strike band, below the single Ju 87 (Tier III)
    ("GA_GerParatroopers", "TierII", {R: 3000}, 180),      # 2x Vultures 525+100, 2x Panzergrenadiere 675+200
    ("GA_HeavyCarpet_Ju390B_250Gr", "TierIII", {R: 800}, 240),  # strike band, beside the 3x Ju 87
]


COPIED_PROPERTIES = ("m_ability_costs", "m_ability_requirements", "m_aim_settings", "m_ability_sound_settings",
                     "m_ui_settings", "m_ability_marker", "m_ability_tier", "m_aircraft_drop_settings",
                     "m_aircraft_strafing_settings", "m_barrage_settings", "m_projectile_mover", "m_weapon_data",
                     "m_weapon_vfx", "m_glider_drop_settings", "m_paratroopers_settings")


def detached_copy(value):
    """A struct copy that shares no memory with the template it came from (see set_paradrop_squads)."""
    if not hasattr(value, "export_text"):
        return value
    text = value.export_text()
    copy = type(value)()
    copy.import_text(text)
    if copy.export_text() != text:
        # The text did not round-trip (an unimportable field): set_editor_property copies the value anyway.
        unreal.log_warning(LOG_PREFIX + "struct %s did not round-trip through text" % type(value).__name__)
        return value
    return copy


def copy_props(source, target):
    copied = []
    for prop in COPIED_PROPERTIES:
        try:
            value = source.get_editor_property(prop)
        except Exception:
            continue  # not a property of this class
        target.set_editor_property(prop, detached_copy(value))
        copied.append(prop)
    return copied


def set_ui(obj, title, description):
    ui = obj.get_editor_property("m_ui_settings")
    if title is not None:
        ui.set_editor_property("title", "<Text_Title>%s</>" % title)
    if description is not None:
        ui.set_editor_property("description", description)
    obj.set_editor_property("m_ui_settings", ui)


def set_balance(obj, tier_name, costs, cooldown):
    obj.set_editor_property("m_ability_tier", tier(tier_name))
    cost_state = obj.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs", costs)
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", cooldown)
    cost_state.set_editor_property("cool_down_remaining", 0)
    obj.set_editor_property("m_ability_costs", cost_state)


def marder_drop_cost(obj):
    """1.25x the training cost of what the radixite Pz II + Marder drop delivers, rounded to 25."""
    unit_costs = {str(tank_subtype("Tank_PzII_F")): (200, 50), str(tank_subtype("Tank_Marder")): (180, 80)}
    radixite = parts = 0
    drop = obj.get_editor_property("m_aircraft_drop_settings")
    for entry in drop.get_editor_property("tanks_with_offsets"):
        r, p = unit_costs.get(str(entry.get_editor_property("tank_subtype")), (250, 100))
        radixite, parts = radixite + r, parts + p
    # Radixite only (user decision 2026-11-05): every global ability costs radixite and nothing else.
    return {R: int(round(radixite * 1.25 / 25.0)) * 25}


def author_new(asset, templates, entries, report, errors):
    for name, source_name, title, description, edit in entries:
        try:
            key = ga(name)
            obj = templates.get(key)
            if obj is None:
                source = templates.get(ga(source_name))
                if source is None:
                    raise RuntimeError("source template %s missing" % source_name)
                obj = unreal.new_object(source.get_class(), outer=asset, name="GA_Commander_" + name)
                copied = copy_props(source, obj)
                report.append("created %s from %s (%s, %d properties)" % (
                    name, source_name, source.get_class().get_name(), len(copied)))
            globals()[edit](obj)
            set_ui(obj, title, description)
            obj.set_editor_property("m_ability_type", key)
            templates[key] = obj
        except Exception as error:
            errors.append("%s: %s" % (name, error))


def apply_edits(templates, entries, errors):
    for name, title, description, edit in entries:
        try:
            obj = templates.get(ga(name))
            if obj is None:
                raise RuntimeError("no template")
            globals()[edit](obj)
            set_ui(obj, title, description)
        except Exception as error:
            errors.append("%s: %s" % (name, error))


def apply_balance(templates, entries, report, errors):
    for name, tier_name, costs, cooldown in entries:
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


def disk_path(asset_path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, asset_path[len("/Game/"):] + ".uasset")


def save_to_disk(asset, report, retries=12):
    """Saves and proves the file changed: while another process holds the package the save fails silently (error 32),
    so retry. Never wait for the file inside this process: it holds its own loaded copy."""
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
    text = "%s %s %s tier=%s cd=%s costs=%s" % (
        name, obj.get_name(), obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
        costs.get_editor_property("cool_down_time"),
        {str(k).split(".")[-1]: v for k, v in costs.get_editor_property("costs").get_editor_property(
            "resource_costs").items()})
    for prop in ("m_glider_drop_settings", "m_aircraft_drop_settings", "m_paratroopers_settings",
                 "m_aircraft_strafing_settings", "m_aircraft_bombing_settings"):
        try:
            value = obj.get_editor_property(prop)
        except Exception:
            continue
        exported = value.export_text()
        if prop == "m_paratroopers_settings":
            start = exported.find("SquadSubtypes")
            exported = exported[start:start + 160]
        text += " %s=%s" % (prop, exported[:700])
    icon = obj.get_editor_property("m_ui_settings").get_editor_property("ability_icon")
    text += " icon=%s title=%s" % (icon.get_name() if icon else None,
                                   obj.get_editor_property("m_ui_settings").get_editor_property("title"))
    return text


def run():
    global GA_ENUM
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    GA_ENUM = type(next(iter(templates.keys())))
    before = len(templates)
    report, errors = ["scope=%s enum type %s, %d entries before" % (SCOPE, GA_ENUM.__name__, before)], []
    if SCOPE == "all":
        author_new(asset, templates, NEW, report, errors)
        for name, title, description in UI_FIXES:
            try:
                set_ui(templates[ga(name)], title, description)
            except Exception as error:
                errors.append("%s: %s" % (name, error))
        apply_balance(templates, BALANCE, report, errors)
    author_new(asset, templates, ROUND3_NEW, report, errors)
    apply_edits(templates, ROUND3_EDITS, errors)
    apply_balance(templates, ROUND3_BALANCE, report, errors)
    if errors:
        report += ["ERROR " + e for e in errors] + ["NOT SAVED"]
        return report
    asset.set_editor_property("m_ability_templates_by_type", templates)
    if not save_to_disk(asset, report):
        return report
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    report.append("entries before=%d after=%d" % (before, len(check)))
    touched = [entry[0] for entry in ROUND3_NEW] + [entry[0] for entry in ROUND3_EDITS]
    for name in touched:
        report.append("VERIFY " + describe(name, check[ga(name)]))
    return report


for line in run():
    unreal.log(LOG_PREFIX + line)
