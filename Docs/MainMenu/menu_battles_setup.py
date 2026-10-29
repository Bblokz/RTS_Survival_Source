"""Builds the main menu battles into the StartGame map. Re-runnable: replaces the arenas and the director it made.

Run in a DebugGame editor (the menu battle classes must be in the loaded DLL). The -run=pythonscript commandlet
crashes while saving the map (landscape collision PreSave compiles shaders), so use a full editor:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject -ExecutePythonScript=<this file> -unattended -RenderOffscreen
The editor quits once the script returns. The script rebuilds the navmesh before saving: a scripted save drops the
baked navmesh, and the ResavePackages -BuildNavigationData commandlet crashes on this map for the same reason as above.
"""
import os
import sys

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/MainMenu")
import menu_battles_world  # noqa: E402

MAP = "/Game/RTS_Survival/Blueprints/GameUI/StartGame/StartGame"
GAME_MODE_BP = "/Game/RTS_Survival/Blueprints/GameUI/StartGame/BP_GM_StartGame"
BP = "/Game/RTS_Survival/Blueprints/"

# Test units the level had standing next to each other; the director stages all fighting now.
PLACED_TEST_UNIT_CLASSES = ("Ger_Vehicle_PanzerIV_G_C", "Rus_Vehicle_T-70_C", "Ger_Turret_PanzerIV_F2_C", "Rus_Turret_T70_C")

# label, location, yaw (German line -> Soviet line), settings overrides, region. The battles travel: the next one is
# always in another region. The desert's four arenas share its region weight. The fog manager's capture is widened
# to cover every landscape (FOG_MAP_EXTENT), so every battle plays in the fog of war.
REMOTE = {}
FOG_MAP_EXTENT = 220000.0
# The desert landscape ends at +-25200 cm: its arenas use shorter approach and reinforcement depths and sit so their
# deepest slots stay on it (past the edge the camera sees the bare sky atmosphere ground).
DESERT = {"pick_weight": 0.5, "approach_distance": 2500.0, "reinforcement_depth": 2000.0}
# The start camera (x -7650, looking west) looks down this arena's axis from behind the German line; it hosts the first
# battle so the menu opens on a fight where it starts looking.
START_VIEW_ARENA = ("MenuArena_StartView", (-11500, 0), 180.0, dict(DESERT, line_separation=7500.0), "Desert")
ARENAS = [
    START_VIEW_ARENA,
    ("MenuArena_EastPlain", (7000, 2500), 0.0, DESERT, "Desert"),
    ("MenuArena_NorthWestField", (-17000, -14000), 90.0, DESERT, "Desert"),
    ("MenuArena_SouthWestFlats", (-14500, 16000), 0.0, DESERT, "Desert"),
    ("MenuArena_SouthEastRolling", (16000, 6000), 90.0, dict(DESERT, camera_distance=5000.0), "Desert"),
    ("MenuArena_SteppeFarm", (0, 80000), 0.0, dict(REMOTE, pick_weight=2.0), "Steppe"),
    ("MenuArena_MeadowForest", (80000, 0), 90.0, dict(REMOTE, pick_weight=2.0), "Meadow"),
    ("MenuArena_WinterRidge", (0, -80000), 0.0, dict(REMOTE, pick_weight=2.0, camera_height=2600.0), "Winter"),
    ("MenuArena_Farmstead", (-80000, 0), 0.0, dict(REMOTE, pick_weight=2.0), "Farm"),
]

VARIANT_ASSET = "/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_UnitVariants"
# Variants that do not fight (or carry the Sdkfz 250/37 hull's track physics bug) stay out of the battles.
VARIANT_EXCLUDE = ("Ambulance", "AmmoCarrier", "AmmoStock", "Searchlight", "Sdkfz250_37", "RailBunker",
                   "CamouflagedNest", "MobileBattery")


def cls(path):
    name = path.split("/")[-1]
    return BP + path + "." + name + "_C"


# Left out: Ger_ArmoredCar_Sdkfz250_37 (its UTrackPhysicsMovement async tick reports an invalid M_TankMesh every
# physics step, 1,600+ error dialogs per battle; also seen in the turret toss demo logs).
GERMAN_VEHICLES = [cls(p) for p in [
    "GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G",
    "GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H",
    "GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J",
    "GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_M",
    "GroundVehicles/TrackBasedVehicle/Ger_Medium/Stug/Ger_Vehicle_StugIII",
    "GroundVehicles/TrackBasedVehicle/Ger_Heavy/PantherD/Ger_Vehicle_PantherD",
    "GroundVehicles/TrackBasedVehicle/Ger_Heavy/PantherG/Ger_Vehicle_PantherG",
    "GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/Ger_Vehicle_TigerH1",
    "GroundVehicles/TrackBasedVehicle/Ger_Heavy/TigerII/Ger_Vehicle_TigerII",
    "GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T",
    "GroundVehicles/TrackBasedVehicle/Ger_Light/PanzerII/Ger_Vehicle_PanzerII_F",
    "GroundVehicles/TrackBasedVehicle/Ger_Light/Hetzer/Ger_Vehicle_Hetzer",
    "GroundVehicles/TrackBasedVehicle/Ger_Light/Marder1/Ger_Vehicle_Marder",
    "GroundVehicles/TrackBasedVehicle/Ger_Medium/JagdPanzerIV70/Ger_Vehicle_JagdPanzerIV70",
    "GroundVehicles/TrackBasedVehicle/Ger_Heavy/JagdPanther/Ger_Vehicle_JagdPanther",
    "GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Puma/Ger_ArmoredCar_Puma",
    "GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz231/Ger_ArmoredCar_Sdkfz_231",
    "GroundVehicles/TrackBasedVehicle/Ger_Medium/Brumbar/Ger_Vehicle_Brummbar",
]]
GERMAN_SQUADS = [cls(p) for p in [
    "Squads/Ger/T2/PanzerGrenadiers/Squad_Ger_PanzerGrenadiers",
    "Squads/Ger/T2/SturmPioneer/Squad_Ger_SturmPioneers",
    "Squads/Ger/Barracks_Infantry/FlameSquad/Squad_Ger_FlameTroopers",
    "Squads/Ger/ArmoryUnlocked/Squad_Ger_HazmatVultures",
    "Squads/Ger/EliteInfantry/LightBringers/Squad_Ger_LightBringers",
]]
GERMAN_AIRCRAFT = [cls(p) for p in [
    "Aircraft/GerAircraft/BF109/Ger_Aircraft_BF109",
    "Aircraft/GerAircraft/JU_87VTO/Ger_Aircraft_StukaJu87",
]]
SOVIET_VEHICLES = [cls(p) for p in [
    "GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476",
    "GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3485",
    "GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T34E",
    "GroundVehicles/TrackBasedVehicle/Rus_Heavy/KV1/Rus_Vehicle_KV-1E",
    "GroundVehicles/TrackBasedVehicle/Rus_Heavy/KV1/Rus_Vehicle_KV-1_L11",
    "GroundVehicles/TrackBasedVehicle/Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2",
    "GroundVehicles/TrackBasedVehicle/Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1",
    "GroundVehicles/TrackBasedVehicle/Rus_Heavy/KV2/Rus_Vehicle_KV-2",
    "GroundVehicles/TrackBasedVehicle/Rus_Light/T70/Rus_Vehicle_T-70",
    "GroundVehicles/TrackBasedVehicle/Rus_Light/T26/Rus_Vehicle_T26",
    "GroundVehicles/TrackBasedVehicle/Rus_Light/BT-7/Rus_Vehicle_BT-7",
    "GroundVehicles/TrackBasedVehicle/Rus_Light/SU76/Rus_Vehicle_SU76",
    "GroundVehicles/TrackBasedVehicle/Rus_Medium/SU85/Rus_Vehicle_Su85",
    "GroundVehicles/TrackBasedVehicle/Rus_Medium/Su100/Rus_Vehicle_Su100",
    "GroundVehicles/TrackBasedVehicle/Rus_Medium/Su122/Rus_Vehicle_Su122",
    "GroundVehicles/TrackBasedVehicle/Rus_Heavy/T28/Rus_Vehicle_T-28",
    "GroundVehicles/TrackBasedVehicle/Rus_Heavy/ISU152/Rus_Vehicle_ISU152",
    "GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/BA-11/Rus_ArmoredCar_BA11",
]]
# Left out: Rus_Squad_LargeMixBasic (more soldiers than the squad spawn grid's 8 points: an error per spawn).
SOVIET_SQUADS = [cls(p) for p in [
    "Squads/Rus/Rus_Squad_Mosin",
    "Squads/Rus/Rus_Squad_SVT",
    "Squads/Rus/Rus_Squad_PTRS",
    "Squads/Rus/Rus_Squad_Fedrov",
    "Squads/Rus/Rus_Squad_HazmatBazooka",
]]
GERMAN_PITS = [cls(p) for p in [
    "Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Pak40",
    "Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Flak37",
    "Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Flak88",
    "Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Mortar",
    "Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Panzerschreck",
    "Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Puppchen",
    "Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_LeFH18",
    "Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42",
]]
SOVIET_PITS = [cls(p) for p in [
    "Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit",
    "Buildings/BuildingExpansions/RusAmpulometPit/Rus_BXP_AmpulometPit",
    "Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit",
    "Buildings/BuildingExpansions/RusDShKPit/Rus_BXP_DShKPit",
    "Buildings/BuildingExpansions/RusMortarPit/Rus_BXP_120MortarPit",
    "Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Rus_BXP_ML20Pit",
]]
SOVIET_AIRCRAFT = [cls(p) for p in [
    "Aircraft/RusAircraft/Sturmovic/Rus_Aircraft_Sturmovic",
    "Aircraft/RusAircraft/PE2/Rus_PE2_Aircraft_3x250",
]]


def log(text):
    unreal.log_warning("MENU_BATTLES_SETUP " + str(text))


def set_prop(obj, names, value):
    """Private UPROPERTYs reach python under a converted name; try the likely spellings."""
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception:
            continue
    raise RuntimeError("no property %s on %s" % (names, obj))


def get_prop(obj, names):
    for name in names:
        try:
            return obj.get_editor_property(name), name
        except Exception:
            continue
    raise RuntimeError("no property %s on %s" % (names, obj))


def load_classes(paths):
    classes = []
    for path in paths:
        loaded = unreal.load_class(None, path)
        if loaded is None:
            log("MISSING class " + path)
            continue
        classes.append(loaded)
    return classes


def harvest_variants():
    """Combat variants from DA_UnitVariants, split by faction: (vehicles, squads) per side."""
    asset = unreal.load_asset(VARIANT_ASSET)
    result = {"German": ([], []), "Soviet": ([], [])}
    for property_name, kind in (("m_tank_variants", 0), ("m_squad_variants", 1)):
        for entry in asset.get_editor_property(property_name).values():
            for variant in list(entry.get_editor_property("variants"))[1:]:
                unit_class = variant.get_editor_property("unit_class")
                path = unit_class.get_path_name() if unit_class is not None and hasattr(unit_class, "get_path_name") else None
                if not path or any(word in path for word in VARIANT_EXCLUDE):
                    continue
                side = "Soviet" if "/Rus" in path else "German"
                result[side][kind].append(path)
    for side, (vehicles, squads) in result.items():
        log("variants %s vehicles=%d squads=%d" % (side, len(vehicles), len(squads)))
    return result


def make_pool(vehicles, squads, aircraft, variants, pits):
    pool = unreal.RTSMenuBattleFactionPool()
    pool.set_editor_property("pits", load_classes(pits))
    pool.set_editor_property("vehicles", load_classes(vehicles))
    pool.set_editor_property("squads", load_classes(squads))
    pool.set_editor_property("aircraft", load_classes(aircraft))
    pool.set_editor_property("variant_vehicles", load_classes(variants[0]))
    pool.set_editor_property("variant_squads", load_classes(variants[1]))
    return pool


def remove_previous_and_test_units(actor_subsystem):
    for actor in actor_subsystem.get_all_level_actors():
        name = actor.get_class().get_name()
        if name in PLACED_TEST_UNIT_CLASSES or name in ("RTSMenuBattleArena", "RTSMenuBattleDirector"):
            log("removing %s (%s)" % (actor.get_actor_label(), name))
            actor_subsystem.destroy_actor(actor)


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 30000), unreal.Vector(x, y, -30000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit is not None else 100.0


def spawn_arenas(world, actor_subsystem, terrains):
    arenas_by_label = {}
    for label, (x, y), yaw, overrides, region in ARENAS:
        z = terrains[region].height(x, y) if region in terrains else ground_z(world, x, y)
        location = unreal.Vector(x, y, z)
        arena = actor_subsystem.spawn_actor_from_class(unreal.RTSMenuBattleArena, location,
                                                       unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        arena.set_actor_label(label)
        settings, name = get_prop(arena, ["m_settings", "M_Settings"])
        settings.set_editor_property("region", region)
        for key, value in overrides.items():
            settings.set_editor_property(key, value)
        set_prop(arena, [name], settings)
        arenas_by_label[label] = arena
        log("arena %s at %s yaw %.0f" % (label, location, yaw))
    return arenas_by_label


def spawn_director(actor_subsystem, opening_arena):
    director = actor_subsystem.spawn_actor_from_class(unreal.RTSMenuBattleDirector, unreal.Vector(0, 0, 500),
                                                      unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    director.set_actor_label("MenuBattleDirector")
    set_prop(director, ["m_opening_arena", "M_OpeningArena"], opening_arena)
    variants = harvest_variants()
    set_prop(director, ["m_german_pool", "M_GermanPool"],
             make_pool(GERMAN_VEHICLES, GERMAN_SQUADS, GERMAN_AIRCRAFT, variants["German"], GERMAN_PITS))
    set_prop(director, ["m_soviet_pool", "M_SovietPool"],
             make_pool(SOVIET_VEHICLES, SOVIET_SQUADS, SOVIET_AIRCRAFT, variants["Soviet"], SOVIET_PITS))
    german, _ = get_prop(director, ["m_german_pool", "M_GermanPool"])
    log("director german vehicles=%d squads=%d aircraft=%d variants=%d" % (
        len(german.get_editor_property("vehicles")), len(german.get_editor_property("squads")),
        len(german.get_editor_property("aircraft")), len(german.get_editor_property("variant_vehicles"))))


def quiet_enemy_controller(actor_subsystem):
    """The menu's enemy controller owns the Soviet units but must not train, build or take them over."""
    for actor in actor_subsystem.get_all_level_actors():
        if not actor.get_class().get_name().startswith("RTS_BP_EnemyController"):
            continue
        settings, name = get_prop(actor, ["m_enemy_ai_mission_settings", "M_EnemyAIMissionSettings"])
        settings.set_editor_property("strategic_ai_thinking_start_condition",
                                     unreal.EnemyStrategicAIThinkingStartCondition.START_AFTER_MISSION_TRIGGER)
        settings.set_editor_property("allow_unit_training", False)
        settings.set_editor_property("allow_direct_control_stochastic_decision_tree", False)
        settings.set_editor_property("allow_building", False)
        set_prop(actor, [name], settings)
        tree, tree_name = get_prop(actor, ["m_stochastic_decision_tree", "M_StochasticDecisionTree"])
        if tree is None:
            tree = unreal.new_object(unreal.StochasticDecisionTree, outer=actor)
            set_prop(actor, [tree_name], tree)
        log("enemy controller %s quieted, tree=%s" % (actor.get_actor_label(), tree))


def widen_fog_capture(actor_subsystem):
    """The fog post process darkens everything outside its capture: make it cover the three new landscapes too."""
    for actor in actor_subsystem.get_all_level_actors():
        if actor.get_class().get_name().startswith("BP_FogOfWar_Manager"):
            set_prop(actor, ["map_extent", "MapExtent"], FOG_MAP_EXTENT)
            log("fog manager map extent %.0f" % FOG_MAP_EXTENT)


def setup_blueprints():
    game_mode = unreal.load_asset(GAME_MODE_BP)
    unreal.BlueprintEditorLibrary.reparent_blueprint(game_mode, unreal.RTSMainMenuGameMode.static_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(game_mode)
    # ARTSMainMenuGameMode marks RTS_BP_StartController as the main menu controller when it spawns it.
    log("game mode saved=%s" % unreal.EditorAssetLibrary.save_loaded_asset(game_mode, False))


def main():
    setup_blueprints()
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    remove_previous_and_test_units(actor_subsystem)
    quiet_enemy_controller(actor_subsystem)
    widen_fog_capture(actor_subsystem)
    terrains = menu_battles_world.build(world, actor_subsystem, ARENAS, (START_VIEW_ARENA[1], START_VIEW_ARENA[2]))
    arenas = spawn_arenas(world, actor_subsystem, terrains)
    spawn_director(actor_subsystem, arenas[START_VIEW_ARENA[0]])
    # Synchronous full build of every RecastNavMesh (the vehicles only drive on baked navmesh).
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    # MENU_BATTLES_SAVE_AS saves a test copy instead, for when another editor has StartGame open (its save then fails).
    save_path = os.environ.get("MENU_BATTLES_SAVE_AS", MAP)
    if save_path != MAP and unreal.EditorAssetLibrary.does_asset_exist(save_path):
        # Saving a world over another existing map package fails ("Unable to overwrite existing package").
        unreal.EditorAssetLibrary.delete_asset(save_path)
    log("level saved=%s path=%s" % (unreal.EditorLoadingAndSavingUtils.save_map(world, save_path), save_path))


try:
    main()
except Exception:
    import traceback
    log("FAILED " + traceback.format_exc())
