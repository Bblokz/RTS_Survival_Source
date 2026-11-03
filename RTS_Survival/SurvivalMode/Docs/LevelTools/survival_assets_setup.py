# Survival mode — data assets, Blueprints and level actors (run in an editor that has the SurvivalMode classes).
# Idempotent: existing assets are updated, existing survival actors in the open map are replaced.
# Usage: execute_python with this file's text, with SM_Desert_01 (or another survival map) open.
import json
import math
import unreal

ROOT = "/Game/RTS_Survival/SurvivalMode"
DATA = ROOT + "/Data"
BPS = ROOT + "/Blueprints"
DOCS = "D:/UE5Projects/RTS_Survival/Source/RTS_Survival/SurvivalMode/Docs/"
GAME_MODE_PARENT = "/Game/RTS_Survival/Blueprints/GameSettings/RTS_BP_GamemodeBase.RTS_BP_GamemodeBase_C"
# Must start airborne: the DropShips/BP_DropShip_* Blueprints are "started as landed" and never take off when spawned
# in the air.
DROP_AIRCRAFT = ("/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU390B/Ger_Aircraft_JU390B_AirborneUnarmed."
                 "Ger_Aircraft_JU390B_AirborneUnarmed_C")

tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
saved = []


def data_asset(path, cls):
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit("/", 1)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, folder, cls, factory)


def blueprint(path, parent):
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    folder, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    return tools.create_asset(name, folder, unreal.Blueprint, factory)


def save(asset):
    eal.save_loaded_asset(asset, False)
    saved.append(asset.get_path_name())


# ---------------------------------------------------------------- sub data assets (constructors hold defaults)
start_content = data_asset(DATA + "/DA_SurvivalStartContent", unreal.SurvivalStartContentSettings)
wave_settings = data_asset(DATA + "/Waves/DA_SurvivalWaveSettings", unreal.SurvivalWaveSettings)
soviet_bases = data_asset(DATA + "/SovietBases/DA_SurvivalSovietBases", unreal.SurvivalSovietBaseSettings)
for a in (start_content, wave_settings, soviet_bases):
    save(a)

# ---------------------------------------------------------------- main settings
settings = data_asset(DATA + "/DA_SurvivalMode_Default", unreal.SurvivalModeSettings)
supply = json.load(open(DOCS + "supply_costs.json", encoding="utf-8"))
entries = []
for unit in supply["units"]:
    variants = unit.get("variants") or {}
    variant_text = ",".join("(%s, %d)" % (k, v) for k, v in variants.items())
    # Super-heavies (40+ supply: King Tiger, Maus, E-100, ...) once per army so a start stays a trade-off.
    max_copies = 1 if unit["base_supply"] >= 40 else 0
    entries.append("(Unit=(UnitType=%s,SubtypeValue=%d),BaseSupply=%d,VariantSupply=(%s),MaxCopies=%d)" % (
        unit["unit_type"], unit["subtype_value"], unit["base_supply"], variant_text, max_copies))
fallback = ",".join("(%s, %d)" % (k, v) for k, v in supply["fallback_per_unit_type"].items())
defaults = supply["suggested_defaults"]
army_text = ("(SlotCount=%d,SupplyBudget=%d,UnitSupplyCosts=(%s),FallbackSupplyPerUnitType=(%s),"
             "AllowedUnitTypes=(UNType_Squad,UNType_Tank,UNType_Harvester,UNType_Nomadic))") % (
    defaults["SlotCount"], defaults["SupplyBudget"], ",".join(entries), fallback)
army = unreal.SurvivalArmySettings()
army.import_text(army_text)
settings.set_editor_property("army", army)

deployment = settings.get_editor_property("deployment")
deployment.set_editor_property("vehicle_drop_aircraft_class", unreal.load_class(None, DROP_AIRCRAFT))
settings.set_editor_property("deployment", deployment)
settings.set_editor_property("grace_period_seconds", 180.0)
settings.set_editor_property("start_content", start_content)
settings.set_editor_property("wave_settings", wave_settings)
settings.set_editor_property("soviet_base_settings", soviet_bases)
save(settings)
print("supply entries", len(settings.get_editor_property("army").get_editor_property("unit_supply_costs")))

# ---------------------------------------------------------------- Blueprints
bp_director = blueprint(BPS + "/BP_SurvivalModeDirector", unreal.SurvivalModeDirector)
bp_start = blueprint(BPS + "/BP_SurvivalStartLocation", unreal.SurvivalStartLocation)
bp_edge = blueprint(BPS + "/BP_SurvivalEdgeSpawnZone", unreal.SurvivalEdgeSpawnZone)
bp_mode = blueprint(BPS + "/BP_SurvivalGameMode", unreal.load_class(None, GAME_MODE_PARENT))
director_cdo = unreal.get_default_object(unreal.load_class(None, bp_director.get_path_name() + "_C"))
director_cdo.set_editor_property("settings", settings)
for wbp_name, prop in (("WBP_SurvivalArmySelection", "army_selection_widget_class"),
                       ("WBP_SurvivalStartLocationPicker", "start_location_picker_widget_class"),
                       ("WBP_SurvivalHud", "hud_widget_class")):
    widget_class = unreal.load_class(None, "%s/UI/%s.%s_C" % (ROOT, wbp_name, wbp_name))
    if widget_class:
        try:
            director_cdo.set_editor_property(prop, widget_class)
        except Exception as e:
            print("widget class not set", wbp_name, e)
    else:
        print("missing widget", wbp_name)
unreal.BlueprintEditorLibrary.compile_blueprint(bp_director)
for bp in (bp_director, bp_start, bp_edge, bp_mode):
    save(bp)

# ---------------------------------------------------------------- level actors (SM_Desert_01 layout)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actor_subsystem.get_all_level_actors():
    if isinstance(actor, (unreal.SurvivalModeDirector, unreal.SurvivalStartLocation, unreal.SurvivalEdgeSpawnZone)):
        actor_subsystem.destroy_actor(actor)

layout = json.load(open(DOCS + "LevelTools/SM_Desert_01_layout.json", encoding="utf-8")) \
    if eal is not None and False else None

START_NAMES = ["Dune Ridge", "Salt Flats", "Scorpion Basin", "Red Mesa", "Dry Wells"]
START_RADIUS = 42000.0
# S0 local defense points relative to its centre (hill tops at Z 1350, ramp heads at ~1347, centre Z 600).
S0_POINTS = [(9192, -9192, 750), (-9192, -9192, 750), (9192, 9192, 750), (-9192, 9192, 750),
             (8273, -8273, 747), (-8273, -8273, 747)]
start_class = unreal.load_class(None, bp_start.get_path_name() + "_C")
for k in range(5):
    angle = math.radians(90 + 72 * k)
    center = unreal.Vector(round(START_RADIUS * math.cos(angle)), round(START_RADIUS * math.sin(angle)), 600)
    rot = math.radians(72 * k)
    points = []
    for (x, y, z) in S0_POINTS:
        points.append(unreal.Vector(x * math.cos(rot) - y * math.sin(rot), x * math.sin(rot) + y * math.cos(rot), z))
    actor = actor_subsystem.spawn_actor_from_class(start_class, center, unreal.Rotator(0, 0, 0))
    actor.set_actor_label("SurvivalStart_%d" % k)
    actor.set_editor_property("display_name", START_NAMES[k])
    actor.set_editor_property("build_area_radius", 8000.0)
    actor.set_editor_property("designer_defense_points", points)
    actor.set_folder_path("Survival")

EDGES = [((-42000, 72000), (0, -1)), ((42000, 72000), (0, -1)), ((72000, 48000), (-1, 0)),
         ((72000, -22000), (-1, 0)), ((52000, -72000), (0, 1)), ((0, -72000), (0, 1)),
         ((-52000, -72000), (0, 1)), ((-72000, -22000), (1, 0)), ((-72000, 48000), (1, 0))]
edge_class = unreal.load_class(None, bp_edge.get_path_name() + "_C")
for i, ((x, y), (dx, dy)) in enumerate(EDGES):
    yaw = math.degrees(math.atan2(dy, dx))
    actor = actor_subsystem.spawn_actor_from_class(edge_class, unreal.Vector(x, y, 1000), unreal.Rotator(0, 0, yaw))
    actor.set_actor_label("SurvivalEdge_%d" % i)
    box = actor.get_editor_property("spawn_area")
    box.set_box_extent(unreal.Vector(1250, 8000, 1000))
    actor.set_folder_path("Survival")

director_class = unreal.load_class(None, bp_director.get_path_name() + "_C")
director = actor_subsystem.spawn_actor_from_class(director_class, unreal.Vector(0, 0, 3000), unreal.Rotator(0, 0, 0))
director.set_actor_label("SurvivalModeDirector")
director.set_folder_path("Survival")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode",
                                               unreal.load_class(None, bp_mode.get_path_name() + "_C"))
ok = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print("level saved", ok)
print("saved assets", saved)
