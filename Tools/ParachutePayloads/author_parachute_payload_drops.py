"""Adds the five parachute payload templates to DA_GlobalAbilitySystem without touching any other template.

GA_GerFlak88Drop and GA_GerSFH18Drop are UGA_Paratroopers templates (team weapon squads: the gun on a cargo chute,
the crew jumps). GA_GerPanzerschreckPitDrop, GA_GerPakPitDrop and GA_GerFieldBunkerDrop are UGA_BuildingParachuteDrop
templates (standalone field-defense expansions under big canopies).

Run headless on a DebugGame build that has UGA_BuildingParachuteDrop and the new UGA_Paratroopers fields:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Idempotent: re-running updates the five entries in place. Hold Saved/AgentLocks/da_globalabilities.lockdir while it
runs, and never save the data asset from an editor whose DLL lacks these classes (that drops the templates).
Costs: about 125% of the delivered units' training/build cost (DeveloperSettings.h); cooldowns per the tier bands in
Docs/Abilities/Commanders.md. Icons are placeholders (existing textures).
"""
import unreal

DA_PATH = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
LOG = "[ParachutePayloads] "
SOURCE = "GA_GER_PARATROOPERS"
# GA_GerParatroopers as it was before the first run of this script, which edited it through an aliased struct
# wrapper; the new fields (ring, flight height, cargo chute) keep their C++ defaults there.
SOURCE_SETTINGS_TEXT = "".join("""
(AircraftClass="/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU390B/Ger_Aircraft_JU390B_AirborneUnarmed.
Ger_Aircraft_JU390B_AirborneUnarmed_C",SquadSubtypes=(Squad_Ger_Vultures),DropIntervalPerUnit=0.450000,
LandingSpreadRadius=750.000000,JumpBelowAircraftOffset=250.000000,ExitDistanceAfterTarget=0.000000,
DescentSettings=(ParachuteMesh="/Script/Engine.StaticMesh'/Game/RTS_Survival/Blueprints/Squads/SquadUnits/
Parachute/Mesh/SM_Parachute_German.SM_Parachute_German'",ParachuteMontage="/Script/Engine.AnimMontage'/Game/
RTS_Survival/Blueprints/Squads/SquadUnits/Parachute/Anims/Montage_Infantry_Parachute_Hang_Ger.
Montage_Infantry_Parachute_Hang_Ger'",DescentSpeed=420.000000,SwayAmplitudeDegrees=7.000000,
SwayPeriodSeconds=3.600000,SwayPivotHeight=480.000000,SwaySettleSeconds=1.500000))
""".split())
GUN_DROP_FLIGHT_HEIGHT = 5000.0
COPIED_PROPERTIES = ("m_ability_requirements", "m_aim_settings", "m_ability_sound_settings", "m_ability_marker")

AIRCRAFT = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU390B/Ger_Aircraft_JU390B_AirborneUnarmed.Ger_Aircraft_JU390B_AirborneUnarmed_C"
CANOPY = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Parachute/Mesh/SM_Parachute_German"
ICON_DIR = "/Game/RTS_Survival/Textures/GameUI/GlobalAbilities/"
PITS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/"
BUNKERS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/"
PANZERSCHRECK_PIT = PITS + "Ger_BXP_GerPit_Panzerschreck.Ger_BXP_GerPit_Panzerschreck_C"
PAK40_PIT = PITS + "Ger_BXP_GerPit_Pak40.Ger_BXP_GerPit_Pak40_C"
FLAK88_BUNKER = BUNKERS + "Flak88Bunker/Ger_BXP_Flak88Bunker.Ger_BXP_Flak88Bunker_C"

R = "RESOURCE_RADIXITE"
M = "RESOURCE_METAL"

# key, title, description, tier, costs, cooldown, icon, object name
# Costs are radixite only (user decision 2026-11-05): 125% of the contents' radixite, no metal.
GUN_DROPS = [
    dict(key="GA_GER_FLAK88_DROP", name="GA_Paratroopers_GerFlak88Drop", tier="TIER_I",
         title="Flak 88 Airdrop",
         description=("A transport drops an 8.8 cm Flak on a cargo chute over the target; its crew jumps after it and "
                      "has the gun ready to fire once everyone is down."),
         # Flak 88 squad 500 R + 220 M, x1.25.
         costs={R: 625}, cooldown=90, icon="T_GlobalAbility_GerParatroopers",
         squads=["Squad_Ger_88mmFlak"], ring=200.0,
         cargo=dict(canopy_scale=2.8, canopy_count=1, descent_speed=600.0)),
    dict(key="GA_GER_SFH18_DROP", name="GA_Paratroopers_GerSFH18Drop", tier="TIER_II",
         title="Howitzer Battery Airdrop",
         description=("Two 10.5 cm leFH 18 howitzers come down over the target, each under a cluster of three cargo "
                      "chutes, with their crews jumping after them."),
         # The project has no sFH 18 team weapon squad (only the sFH 18 pit), so the battery is two leFH 18 squads:
         # 2x (600 R + 260 M, the same data as the sFH 18), x1.25.
         costs={R: 1500}, cooldown=180, icon="T_GlobalAbility_GerParatroopers",
         squads=["Squad_Ger_LefH18", "Squad_Ger_LefH18"], ring=650.0,
         cargo=dict(canopy_scale=2.2, canopy_count=3, descent_speed=560.0)),
]

BUILDING_DROPS = [
    dict(key="GA_GER_PANZERSCHRECK_PIT_DROP", name="GA_BuildingParachuteDrop_GerPanzerschreckPitDrop", tier="TIER_I",
         title="Dual Panzerschreck Pit Drop",
         description=("Two dug-in Panzerschreck foxholes, each with a gunner and a loader, float down side by side "
                      "under big cargo canopies and hold the spot at once."),
         # 2x Panzerschreck pit (160 R + 60 M), x1.25.
         costs={R: 400}, cooldown=75, icon="T_GlobalAbility_GerSpotterPzIIIDrop",
         payloads=[dict(cls=PANZERSCHRECK_PIT, offset=(0.0, -450.0), yaw=0.0, canopy_scale=2.6, canopy_count=1),
                   dict(cls=PANZERSCHRECK_PIT, offset=(0.0, 450.0), yaw=0.0, canopy_scale=2.6, canopy_count=1)]),
    dict(key="GA_GER_PAK_PIT_DROP", name="GA_BuildingParachuteDrop_GerPakPitDrop", tier="TIER_II",
         title="Pak 40 Pit Drop",
         description=("A complete Pak 40 gun pit, crew and all, comes down under one giant canopy and is ready to "
                      "fire the moment it lands."),
         # Pak 40 pit 380 R + 200 M, x1.25.
         costs={R: 475}, cooldown=150, icon="T_GlobalAbility_GerSpotterPzIIIDrop",
         payloads=[dict(cls=PAK40_PIT, offset=(0.0, 0.0), yaw=0.0, canopy_scale=4.0, canopy_count=1)]),
    dict(key="GA_GER_FIELD_BUNKER_DROP", name="GA_BuildingParachuteDrop_GerFieldBunkerDrop", tier="TIER_III",
         title="Field Bunker Drop",
         description=("A heavy transport drops a whole Flak 88 field bunker under a cluster of three giant canopies. "
                      "It lands upright, manned and firing."),
         # Flak 88 bunker uses the Flak 88 emplacement's data (560 R + 360 M), x1.25.
         costs={R: 700}, cooldown=240, icon="T_GlobalAbility_GerSpotterPzIIIDrop",
         payloads=[dict(cls=FLAK88_BUNKER, offset=(0.0, 0.0), yaw=0.0, canopy_scale=3.6, canopy_count=3)]),
]


def enum_member(enum_type, name):
    """Python spells enum members in upper snake case; match the C++ name with underscores and case ignored."""
    wanted = name.replace("_", "").lower()
    for member_name in dir(enum_type):
        if member_name.replace("_", "").lower() == wanted:
            return getattr(enum_type, member_name)
    raise KeyError("%s has no member %s" % (enum_type, name))


def set_common(obj, key, spec, source):
    for prop in COPIED_PROPERTIES:
        try:
            obj.set_editor_property(prop, source.get_editor_property(prop))
        except Exception as error:
            unreal.log_warning(LOG + "could not copy %s: %s" % (prop, error))
    obj.set_editor_property("m_ability_type", key)
    obj.set_editor_property("m_ability_tier", getattr(unreal.GlobalAbilityTier, spec["tier"]))
    cost_state = obj.get_editor_property("m_ability_costs")
    unit_cost = cost_state.get_editor_property("costs")
    unit_cost.set_editor_property("resource_costs",
                                  {getattr(unreal.RTSResourceType, k): v for k, v in spec["costs"].items()})
    cost_state.set_editor_property("costs", unit_cost)
    cost_state.set_editor_property("cool_down_time", spec["cooldown"])
    obj.set_editor_property("m_ability_costs", cost_state)
    ui = obj.get_editor_property("m_ui_settings")
    ui.set_editor_property("ability_icon", unreal.load_asset(ICON_DIR + spec["icon"]))
    ui.set_editor_property("title", "<Text_Title>%s</>" % spec["title"])
    ui.set_editor_property("description", spec["description"])
    obj.set_editor_property("m_ui_settings", ui)


def cargo_settings(scale, count, speed, sway=4.0):
    cargo = unreal.CargoParachuteDescentSettings()
    cargo.set_editor_property("canopy_mesh", unreal.load_asset(CANOPY))
    cargo.set_editor_property("canopy_scale", scale)
    cargo.set_editor_property("canopy_count", count)
    cargo.set_editor_property("descent_speed", speed)
    cargo.set_editor_property("sway_amplitude_degrees", sway)
    return cargo


def author_gun_drop(asset, templates, enum, source, spec):
    key = enum_member(enum, spec["key"])
    obj = templates.get(key)
    if obj is None or obj.get_class() != unreal.GA_Paratroopers.static_class():
        obj = unreal.new_object(unreal.GA_Paratroopers, outer=asset, name=spec["name"])
    set_common(obj, key, spec, source)
    # Same transport, jump interval and soldier parachute as the German paratroopers. A fresh struct filled from text:
    # the wrapper get_editor_property returns can alias the source template's memory.
    settings = unreal.ParatroopersGlobalAbilitySettings()
    settings.import_text(SOURCE_SETTINGS_TEXT)
    settings.set_editor_property("min_flight_height_above_target", GUN_DROP_FLIGHT_HEIGHT)
    settings.set_editor_property("squad_subtypes", [enum_member(unreal.SquadSubtype, s) for s in spec["squads"]])
    settings.set_editor_property("squad_spawn_ring_radius", spec["ring"])
    cargo = spec["cargo"]
    settings.set_editor_property("team_weapon_cargo_descent",
                                 cargo_settings(cargo["canopy_scale"], cargo["canopy_count"], cargo["descent_speed"]))
    obj.set_editor_property("m_paratroopers_settings", settings)
    templates[key] = obj
    return key


def author_building_drop(asset, templates, enum, source, spec):
    key = enum_member(enum, spec["key"])
    obj = templates.get(key)
    if obj is None or obj.get_class() != unreal.GA_BuildingParachuteDrop.static_class():
        obj = unreal.new_object(unreal.GA_BuildingParachuteDrop, outer=asset, name=spec["name"])
    set_common(obj, key, spec, source)
    settings = obj.get_editor_property("m_building_drop_settings")
    settings.set_editor_property("aircraft_class", unreal.load_class(None, AIRCRAFT))
    settings.set_editor_property("canopy_mesh", unreal.load_asset(CANOPY))
    settings.set_editor_property("drop_interval", 0.9)
    settings.set_editor_property("drop_below_aircraft_offset", 1500.0)
    settings.set_editor_property("min_flight_height_above_target", 6000.0)
    payloads = []
    for entry in spec["payloads"]:
        payload = unreal.BuildingParachuteDropPayload()
        payload.set_editor_property("building_class", unreal.load_class(None, entry["cls"]))
        payload.set_editor_property("landing_offset", unreal.Vector2D(entry["offset"][0], entry["offset"][1]))
        payload.set_editor_property("yaw_offset", entry["yaw"])
        payload.set_editor_property("descent", cargo_settings(entry["canopy_scale"], entry["canopy_count"], 520.0, 3.0))
        payloads.append(payload)
    settings.set_editor_property("payloads", payloads)
    obj.set_editor_property("m_building_drop_settings", settings)
    templates[key] = obj
    return key


def restore_source(source):
    """One-time repair: puts GA_GerParatroopers back only while it still carries the squads the first run wrote into it.
    A legitimately retuned GA_GerParatroopers (anything else in its squad list) is left alone."""
    current = source.get_editor_property("m_paratroopers_settings").export_text()
    if "SFH18" not in current and "88mmFlak" not in current and "LefH18" not in current:
        unreal.log(LOG + "GA_GerParatroopers left as it is: " + current)
        return
    settings = unreal.ParatroopersGlobalAbilitySettings()
    settings.import_text(SOURCE_SETTINGS_TEXT)
    source.set_editor_property("m_paratroopers_settings", settings)
    unreal.log(LOG + "GA_GerParatroopers settings: " + source.get_editor_property("m_paratroopers_settings").export_text())


def describe(obj):
    costs = obj.get_editor_property("m_ability_costs")
    text = "%s %s tier=%s cooldown=%s costs=%s title=%s" % (
        obj.get_name(), obj.get_class().get_name(), obj.get_editor_property("m_ability_tier"),
        costs.get_editor_property("cool_down_time"),
        costs.get_editor_property("costs").get_editor_property("resource_costs"),
        obj.get_editor_property("m_ui_settings").get_editor_property("title"))
    if obj.get_class() == unreal.GA_Paratroopers.static_class():
        settings = obj.get_editor_property("m_paratroopers_settings")
        text += " squads=%s ring=%s cargo=%s" % (
            settings.get_editor_property("squad_subtypes"), settings.get_editor_property("squad_spawn_ring_radius"),
            settings.get_editor_property("team_weapon_cargo_descent").export_text())
    else:
        settings = obj.get_editor_property("m_building_drop_settings")
        text += " payloads=%s" % [p.get_editor_property("building_class") for p in settings.get_editor_property("payloads")]
    return text


def run():
    asset = unreal.load_asset(DA_PATH)
    templates = asset.get_editor_property("m_ability_templates_by_type")
    before = len(templates)
    # UGlobalAbility and EGlobalAbility share a python name, so the enum type comes from the map keys.
    enum = type(next(iter(templates.keys())))
    source = templates.get(enum_member(enum, SOURCE))
    restore_source(source)
    keys = [enum_member(enum, SOURCE)]
    keys += [author_gun_drop(asset, templates, enum, source, spec) for spec in GUN_DROPS]
    keys += [author_building_drop(asset, templates, enum, source, spec) for spec in BUILDING_DROPS]
    asset.set_editor_property("m_ability_templates_by_type", templates)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(asset, False)
    unreal.log(LOG + "saved=%s entries before=%d after=%d" % (saved, before, len(templates)))
    check = unreal.load_asset(DA_PATH).get_editor_property("m_ability_templates_by_type")
    for key in keys:
        unreal.log(LOG + describe(check[key]))


run()
