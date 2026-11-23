"""Re-runnable wiring of the Berlin mission stages into Ger_Berlin's BP_MissionManager (editor Python, editor world).

Order and triggers: Source/Docs/Campaign/BerlinMissionScripting.md, sections 6 and 7. Each stage gets its own index in
Missions[] and its own trigger:
  ('after', Stage)              Mission Class Completed watching Stage
  ('after_or_failed', Stage)    the same, also fires when Stage FAILED (the alarm)
  ('near', (x, y, z), radius)   UPlayerUnitsNearMissionTrigger (a player unit comes within radius)
  ('fleeing', None)             UEnemyFleeingMissionTrigger (the first enemy unit flees)
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
]

# Per-instance properties (the Intro map showed a data asset equal to the CDO value coming back empty on load).
INSTANCE_ASSETS = {
    'Mission_Berlin_Counterattack': {'settings': STAGE_DIR + 'DA_Berlin_ResponseForce'},
}


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
    raise ValueError(spec)


def wire(world=None):
    if world is None:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    manager = find_manager(world)
    manager.modify()
    existing = [m for m in manager.get_editor_property('missions') if m is not None]
    classes = {name: stage_class(name) for name, _ in STAGES}
    changes, wired = [], []
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
        wired.append(mission)
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
        else:
            trigger_text = trigger.get_class().get_name() if trigger else 'no trigger'
        title = mission.get_editor_property('mission_widget_state').get_editor_property('mission_title')
        lines.append(f'Missions[{index}]: {mission.get_class().get_name()} [{trigger_text}] "{title}"')
    return lines
