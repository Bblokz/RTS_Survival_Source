"""Re-runnable wiring of the Berlin mission stages into Ger_Berlin's BP_MissionManager (editor Python, editor world).

Order and triggers: Source/Docs/Campaign/BerlinMissionScripting.md, sections 6 and 7. Each stage gets its own index in
Missions[] and its own trigger:
  ('after', Stage)              Mission Class Completed watching Stage
  ('after_or_failed', Stage)    the same, also fires when Stage FAILED (the alarm)
  ('near', (x, y, z), radius)   UPlayerUnitsNearMissionTrigger (a player unit comes within radius)
  ('fleeing', None)             UEnemyFleeingMissionTrigger (the first enemy unit flees)
  ('time', seconds)             UTimeElapsedMissionTrigger (the game has run that many seconds)
  ('keep', None)                keep the trigger the existing instance has (stages that bring their own trigger)
  None                          no trigger: starts at begin play
Stages whose Blueprint does not exist yet are skipped. Missions already in the array that are not in STAGES are kept
after the listed ones. An existing instance of the right class keeps its object (so per-instance settings survive) and
only gets its trigger replaced when it differs.

Run it in a FULL editor with Ger_Berlin open (a commandlet crashes saving a landscape map). It does not save: call
wire(), check describe(), then save the level (run_wire_berlin_stages.py does all three).
"""
import unreal

STAGE_DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
DATA_DIR = '/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/'  # Berlin data assets live next to the map

STAGES = [
    ('Mission_Berlin_Arrival', None),
    ('Mission_Berlin_ReachLeitturm', ('after', 'Mission_Berlin_Arrival')),
    ('Mission_Berlin_HoldRelay', ('after', 'Mission_Berlin_ReachLeitturm')),
    ('Mission_Berlin_KeepHQ', ('after', 'Mission_Berlin_Arrival')),
    ('Mission_Berlin_StayUndetected', ('after', 'Mission_Berlin_Arrival')),
    ('Mission_Berlin_ResponseForce', ('after_or_failed', 'Mission_Berlin_StayUndetected')),
    ('Mission_Berlin_StopRunners', ('fleeing', None)),
    ('Mission_Berlin_CellarPeople', ('near', (-8939.0, 35946.0, 0.0), 3000.0)),
    ('Mission_Berlin_RailYard', ('near', (-2500.0, 18300.0, 0.0), 9000.0)),
    ('Mission_Berlin_ForgeStation', ('keep', None)),
    ('Mission_Berlin_RaidReichstag', ('near', (8112.0, -15994.0, 0.0), 9000.0)),
    ('Mission_Berlin_DemeterPod', ('near', (-20042.0, 1667.0, 0.0), 2500.0)),
    ('Mission_Berlin_Counterattack', ('after', 'Mission_Berlin_ResponseForce')),
    ('Mission_Berlin_Patrols', None),
    ('Mission_Berlin_SupplyPoint', ('time', 140.0)),
    # South POIs (2026-11-26): the Ju 52 crash crater (poi/south_pois.py); the crew appears on its west rim.
    ('Mission_Berlin_Ju52Crash', ('near', (-16800.0, 3200.0, 0.0), 2800.0)),
    # Field hospital (2026-11-26): Red Cross cellar between the mud craters (poi/field_hospital/fh_place.py).
    ('Mission_Berlin_FieldHospital', ('near', (-16500.0, -5800.0, 0.0), 2500.0)),
]

# Per-instance properties (the Intro map showed a data asset equal to the CDO value coming back empty on load).
INSTANCE_ASSETS = {
    'Mission_Berlin_Counterattack': {'settings': DATA_DIR + 'DA_Berlin_ResponseForce'},
    'Mission_Berlin_Arrival': {'ReichstagTraining': DATA_DIR + 'DA_Berlin_ReichstagTraining'},
}

# Per-instance properties copied from another stage's instance. Python cannot build a map keyed by
# ERTSGameDifficulty (the struct FRTSGameDifficulty shadows the enum's Python name), and a Blueprint map variable takes
# no literal default, so the patrols take the Forge station's DA_BerlinForces_* map.
INSTANCE_COPIES = {
    'Mission_Berlin_Patrols': {'ForcesPerDifficulty': ('Mission_Berlin_ForgeStation', 'forces_per_difficulty')},
}


def copy_instance_values(name, mission, wired_by_name, changes):
    for prop, (source_name, source_prop) in INSTANCE_COPIES.get(name, {}).items():
        source = wired_by_name.get(source_name)
        if source is None:
            changes.append(f'{name}.{prop}: no {source_name} to copy from')
            continue
        values = source.get_editor_property(source_prop)
        if len(mission.get_editor_property(prop)) != len(values):
            mission.set_editor_property(prop, values)
            changes.append(f'{name}.{prop} = {len(values)} entries from {source_name}')


def stage_class(name):
    if not unreal.EditorAssetLibrary.does_asset_exist(STAGE_DIR + name):
        return None
    return unreal.load_asset(STAGE_DIR + name).generated_class()


def find_manager(world):
    managers = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                if a.get_class().get_name() == 'BP_MissionManager_C']
    if len(managers) != 1:
        raise RuntimeError(f'expected one BP_MissionManager, found {len(managers)}')
    return managers[0]


def trigger_matches(mission, spec, classes):
    trigger = mission.get_editor_property('mission_trigger')
    if spec is None:
        return trigger is None
    kind = spec[0]
    if kind == 'keep':
        return True
    if kind == 'fleeing':
        return isinstance(trigger, unreal.EnemyFleeingMissionTrigger)
    if kind == 'time':
        return (isinstance(trigger, unreal.TimeElapsedMissionTrigger)
                and abs(trigger.get_editor_property('seconds') - spec[1]) < 0.01)
    if kind in ('after', 'after_or_failed'):
        return (isinstance(trigger, unreal.MissionClassCompletedMissionTrigger)
                and trigger.get_editor_property('mission_class_to_watch') == classes.get(spec[1])
                and trigger.get_editor_property('also_trigger_when_failed') == (kind == 'after_or_failed'))
    if kind == 'near':
        if not isinstance(trigger, unreal.PlayerUnitsNearMissionTrigger):
            return False
        loc = trigger.get_editor_property('centre_location')
        return (abs(loc.x - spec[1][0]) < 1 and abs(loc.y - spec[1][1]) < 1
                and abs(trigger.get_editor_property('radius') - spec[2]) < 1)
    return False


def make_trigger(mission, spec, classes):
    if spec is None:
        return None
    kind = spec[0]
    if kind in ('after', 'after_or_failed'):
        trigger = unreal.new_object(unreal.MissionClassCompletedMissionTrigger, outer=mission)
        trigger.set_editor_property('mission_class_to_watch', classes[spec[1]])
        trigger.set_editor_property('also_trigger_when_failed', kind == 'after_or_failed')
        return trigger
    if kind == 'near':
        trigger = unreal.new_object(unreal.PlayerUnitsNearMissionTrigger, outer=mission)
        trigger.set_editor_property('centre_location', unreal.Vector(*spec[1]))
        trigger.set_editor_property('radius', spec[2])
        return trigger
    if kind == 'fleeing':
        return unreal.new_object(unreal.EnemyFleeingMissionTrigger, outer=mission)
    if kind == 'time':
        trigger = unreal.new_object(unreal.TimeElapsedMissionTrigger, outer=mission)
        trigger.set_editor_property('seconds', spec[1])
        return trigger
    raise ValueError(spec)


def wire(world=None):
    if world is None:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    manager = find_manager(world)
    manager.modify()
    existing = [m for m in manager.get_editor_property('missions') if m is not None]
    classes = {name: stage_class(name) for name, _ in STAGES}
    changes, wired, wired_by_name = [], [], {}
    for name, spec in STAGES:
        cls = classes[name]
        if cls is None:
            changes.append(f'skipped {name} (no Blueprint yet)')
            continue
        mission = next((m for m in existing if m.get_class() == cls), None)
        if mission is None:
            mission = unreal.new_object(cls, outer=manager)
            changes.append(f'new {name}')
        if not trigger_matches(mission, spec, classes):
            mission.set_editor_property('mission_trigger', make_trigger(mission, spec, classes))
            changes.append(f'{name} trigger = {spec}')
        for prop, asset_path in INSTANCE_ASSETS.get(name, {}).items():
            asset = unreal.load_asset(asset_path)
            if mission.get_editor_property(prop) != asset:
                mission.set_editor_property(prop, asset)
                changes.append(f'{name}.{prop} = {asset_path}')
        copy_instance_values(name, mission, wired_by_name, changes)
        wired.append(mission)
        wired_by_name[name] = mission
    known = [c for c in classes.values() if c is not None]
    others = [m for m in existing if m.get_class() not in known]
    manager.set_editor_property('missions', wired + others)
    return changes


def describe(world=None):
    """One line per manager mission: class, trigger, and plate title."""
    if world is None:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    lines = []
    for index, mission in enumerate(find_manager(world).get_editor_property('missions')):
        if mission is None:
            lines.append(f'Missions[{index}]: None')
            continue
        trigger = mission.get_editor_property('mission_trigger')
        if isinstance(trigger, unreal.MissionClassCompletedMissionTrigger):
            watched = trigger.get_editor_property('mission_class_to_watch')
            failed = ' or failed' if trigger.get_editor_property('also_trigger_when_failed') else ''
            trigger_text = f'after {watched.get_name() if watched else None}{failed}'
        elif isinstance(trigger, unreal.PlayerUnitsNearMissionTrigger):
            loc = trigger.get_editor_property('centre_location')
            trigger_text = f'near ({loc.x:.0f}, {loc.y:.0f}) r {trigger.get_editor_property("radius"):.0f}'
        elif isinstance(trigger, unreal.TimeElapsedMissionTrigger):
            trigger_text = f'after {trigger.get_editor_property("seconds"):.0f} s'
        else:
            trigger_text = trigger.get_class().get_name() if trigger else 'no trigger'
        title = mission.get_editor_property('mission_widget_state').get_editor_property('mission_title')
        lines.append(f'Missions[{index}]: {mission.get_class().get_name()} [{trigger_text}] "{title}"')
    return lines
