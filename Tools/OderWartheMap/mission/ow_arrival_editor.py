"""Editor-side part of stage M00 (run in the throwaway Ger_OderWarthe editor through ow_cmd.py, after
build_arrival_stage.py and layout_arrival_eventgraph.py built the Blueprint over UECP):

  set_stage_defaults()  arrays and structs the UECP add_variable cannot default (route, escort, fallback HQ)
  wire_stage()          Mission_OW_Arrival as Missions[0] of the map's BP_MissionManager (no trigger: starts at begin
                        play), plus the unexplored-fog reveal of the mission flow (Phase 0 step 4)
  describe()            one line per manager mission
The map step that moves the player start onto the Berlin road is ow_map_build.step_arrival_start().
"""
import json

import unreal

STAGE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OderWarthe/Mission_OW_Arrival'
LAYOUT_FILE = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/terrain/OW_layout.json'
# road_berlin points: the column drives 60 -> 63 -> 69, then fans out on the ridge.
ROUTE_INDICES = (60, 63, 69)
ROUTE_END_PAST_ROAD_CM = 1500.0
# Lead car ahead of the HQ, rear car behind it (the deck spawns on point 55, ow_map_build.ARRIVAL_ROAD_INDEX).
ESCORT_INDICES = (58, 52)
ESCORT_SUBTYPES = ('TANK_PUMA', 'TANK_SDKFZ_232_3')  # the Sd.Kfz. 221 is not in the async spawner's map
SPAWN_LIFT_CM = 100.0
MISSION_TITLE = 'Oder-Warthe-Bogen'
MISSION_DESCRIPTION = 'The column drives up onto the ridge.'


def log(text):
    unreal.log_warning('OW_ARRIVAL ' + text)


def road_points():
    with open(LAYOUT_FILE, 'r') as handle:
        return json.load(handle)['roads']['road_berlin']['points']


def enum_value(enum_class, name):
    value = getattr(enum_class, name, None)
    if value is None:
        raise RuntimeError('%s has no %s; members: %s' % (enum_class, name, [m for m in dir(enum_class) if m.isupper()]))
    return value


def training_option(unit_type_name, subtype_enum, subtype_name):
    """FTrainingOption's fields are EditDefaultsOnly, so Python cannot set them on an instance: import text instead.
    unit_type_name is the C++ enumerator (UNType_Tank)."""
    option = unreal.TrainingOption()
    subtype = int(enum_value(subtype_enum, subtype_name).value)
    option.import_text('(UnitType=%s,SubtypeValue=%d)' % (unit_type_name, subtype))
    return option


def stage_cdo():
    blueprint = unreal.load_asset(STAGE)
    if blueprint is None:
        raise RuntimeError('no ' + STAGE + ' (run build_arrival_stage.py first)')
    return blueprint, unreal.get_default_object(blueprint.generated_class())


def set_stage_defaults():
    points = road_points()
    route = [unreal.Vector(points[i][0], points[i][1], points[i][2] + SPAWN_LIFT_CM) for i in ROUTE_INDICES]
    last, before = points[ROUTE_INDICES[-1]], points[ROUTE_INDICES[-2]]
    dx, dy = last[0] - before[0], last[1] - before[1]
    length = (dx * dx + dy * dy) ** 0.5
    route.append(unreal.Vector(last[0] + dx / length * ROUTE_END_PAST_ROAD_CM,
                               last[1] + dy / length * ROUTE_END_PAST_ROAD_CM, last[2] + SPAWN_LIFT_CM))
    escort_points = [unreal.Vector(points[i][0], points[i][1], points[i][2] + SPAWN_LIFT_CM) for i in ESCORT_INDICES]
    escort = [training_option('UNType_Tank', unreal.TankSubtype, name) for name in ESCORT_SUBTYPES]
    hq = training_option('UNType_Nomadic', unreal.NomadicSubtype, 'NOMADIC_GER_HQ')

    blueprint, cdo = stage_cdo()
    cdo.set_editor_property('ColumnRoute', route)
    cdo.set_editor_property('EscortSpawnPoints', escort_points)
    cdo.set_editor_property('EscortUnits', escort)
    cdo.set_editor_property('FallbackHQ', hq)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved = unreal.EditorAssetLibrary.save_asset(STAGE, only_if_is_dirty=False)
    log('defaults route=%s escort=%s saved=%s' % ([(round(v.x), round(v.y)) for v in route],
                                                   [e.export_text() for e in escort], saved))
    return saved


def find_manager(world):
    managers = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                if a.get_class().get_name() == 'BP_MissionManager_C']
    if len(managers) != 1:
        raise RuntimeError('expected one BP_MissionManager, found %d' % len(managers))
    return managers[0]


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def wire_stage():
    world = editor_world()
    manager = find_manager(world)
    manager.modify()
    stage_class = unreal.load_asset(STAGE).generated_class()
    existing = [m for m in manager.get_editor_property('missions') if m is not None]
    mission = next((m for m in existing if m.get_class() == stage_class), None)
    changes = []
    if mission is None:
        mission = unreal.new_object(stage_class, outer=manager)
        changes.append('new Mission_OW_Arrival')
    if mission.get_editor_property('mission_trigger') is not None:
        mission.set_editor_property('mission_trigger', None)
        changes.append('trigger cleared (starts at begin play)')
    state = mission.get_editor_property('mission_widget_state')
    if str(state.get_editor_property('mission_title')) != MISSION_TITLE:
        state.set_editor_property('mission_title', MISSION_TITLE)
        state.set_editor_property('mission_description', MISSION_DESCRIPTION)
        mission.set_editor_property('mission_widget_state', state)
        changes.append('plate text')
    others = [m for m in existing if m is not mission]
    manager.set_editor_property('missions', [mission] + others)
    try:
        if not manager.get_editor_property('reveal_unexplored_fog'):
            manager.set_editor_property('reveal_unexplored_fog', True)
            changes.append('reveal_unexplored_fog on')
    except Exception as error:  # the property name differs in older builds
        changes.append('reveal_unexplored_fog not set: %s' % error)
    log('wire %s' % changes)
    return changes


def describe():
    lines = []
    for index, mission in enumerate(find_manager(editor_world()).get_editor_property('missions')):
        if mission is None:
            lines.append('Missions[%d]: None' % index)
            continue
        trigger = mission.get_editor_property('mission_trigger')
        title = mission.get_editor_property('mission_widget_state').get_editor_property('mission_title')
        lines.append('Missions[%d]: %s [%s] "%s"' % (index, mission.get_class().get_name(),
                                                    trigger.get_class().get_name() if trigger else 'no trigger',
                                                    title))
    return lines


STAGE_DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OderWarthe/'
# Stages after M00: (Blueprint, stage that must complete first, plate title, plate description). Missions[] order.
LATER_STAGES = [
    ('Mission_OW_ListeningPost', 'Mission_OW_Arrival', 'Take the listening post',
     'Take DF 1, the pump house below the north ramp.'),
    ('Mission_OW_FirstAttack', 'Mission_OW_Arrival', 'Hold the ridge',
     'They attack once we start listening, and soon anyway.'),
    ('Mission_OW_SecondBearing', 'Mission_OW_FirstAttack', 'Cross the Packlitz dam',
     "Take DF 2 on the forester's hill for a second bearing."),
    ('Mission_OW_BonusAllPosts', 'Mission_OW_FirstAttack', 'Take all five listening posts',
     'Hold all five at once: the command post has nowhere left to hide.'),
    ('Mission_OW_BonusTunnelMouth', 'Mission_OW_FirstAttack', 'Collapse a tunnel mouth',
     'Only heavy guns bring one down. That bunker sends no more columns.'),
    ('Mission_OW_BonusRadioTruck', 'Mission_OW_FirstAttack', 'Destroy the radio truck',
     'It sends false bearings from behind their line. Find it and burn it.'),
]
BONUS_STAGES = ('Mission_OW_BonusAllPosts', 'Mission_OW_BonusTunnelMouth', 'Mission_OW_BonusRadioTruck')
# Stages whose trigger only fires from this difficulty up (the radio truck exists on Hard and above).
STAGE_MINIMUM_DIFFICULTY = {'Mission_OW_BonusRadioTruck': 'HARD'}


def stage_class(name):
    blueprint = unreal.load_asset(STAGE_DIR + name)
    return blueprint.generated_class() if blueprint is not None else None


def wire_later_stages():
    """Adds or updates each LATER_STAGES stage on the manager with a Mission Class Completed trigger and its plate."""
    manager = find_manager(editor_world())
    manager.modify()
    missions = [m for m in manager.get_editor_property('missions') if m is not None]
    changes = []
    for name, after, title, description in LATER_STAGES:
        cls, after_cls = stage_class(name), stage_class(after)
        if cls is None or after_cls is None:
            changes.append('skipped %s (no Blueprint)' % name)
            continue
        mission = next((m for m in missions if m.get_class() == cls), None)
        if mission is None:
            mission = unreal.new_object(cls, outer=manager)
            missions.append(mission)
            changes.append('new ' + name)
        trigger = mission.get_editor_property('mission_trigger')
        if not (isinstance(trigger, unreal.MissionClassCompletedMissionTrigger)
                and trigger.get_editor_property('mission_class_to_watch') == after_cls):
            trigger = unreal.new_object(unreal.MissionClassCompletedMissionTrigger, outer=mission)
            trigger.set_editor_property('mission_class_to_watch', after_cls)
            mission.set_editor_property('mission_trigger', trigger)
            changes.append('%s after %s' % (name, after))
        # unreal.RTSGameDifficulty is the struct of that name (it shadows the enum): take the enum from the property.
        difficulty_enum = type(trigger.get_editor_property('minimum_difficulty'))
        minimum = getattr(difficulty_enum, STAGE_MINIMUM_DIFFICULTY.get(name, 'NEW_TO_RTS'))
        if trigger.get_editor_property('minimum_difficulty') != minimum:
            trigger.set_editor_property('minimum_difficulty', minimum)
            changes.append('%s from %s' % (name, minimum))
        state = mission.get_editor_property('mission_widget_state')
        state.set_editor_property('mission_title', title)
        state.set_editor_property('mission_description', description)
        state.set_editor_property('objective_type', unreal.MissionObjectiveType.BONUS if name in BONUS_STAGES
                                  else unreal.MissionObjectiveType.MAJOR)
        mission.set_editor_property('mission_widget_state', state)
    manager.set_editor_property('missions', missions)
    log('wire later %s' % changes)
    return changes
