"""Re-runnable wiring of the Ore Mountains stages into Ger_2ndOreMountains' BP_MissionManager (editor Python).

Generated from Source/Tools/BerlinMap/mission/wire_berlin_stages.py (same trigger kinds and keep-instance rules); only
STAGE_DIR, STAGES and INSTANCE_ASSETS differ. Order and triggers: OreMountainsMission.md, "Missions[] stage plan".
Run in the editor with the map open (run_wire_ore_stages.py sends it over UECP), then save the level.
"""
import unreal

STAGE_DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/'

STAGES = [
    ('Mission_Ore_Opening', None),
    ('Mission_Ore_KeepHQ', ('after', 'Mission_Ore_Opening')),
    ('Mission_Ore_Convoy1_FuelRun', ('after', 'Mission_Ore_Opening')),
    ('Mission_Ore_Convoy2_Weighbridge', ('after', 'Mission_Ore_Convoy1_FuelRun')),
    ('Mission_Ore_LightCounterattack', ('after', 'Mission_Ore_Convoy2_Weighbridge')),
    ('Mission_Ore_Convoy3_AmmoSplit', ('after', 'Mission_Ore_Convoy2_Weighbridge')),
    ('Mission_Ore_Convoy4_Hunters', ('after', 'Mission_Ore_Convoy3_AmmoSplit')),
    ('Mission_Ore_Convoy5_Cases', ('after', 'Mission_Ore_Convoy4_Hunters')),
    ('Mission_Ore_HeavyCounterattack', ('after', 'Mission_Ore_Convoy5_Cases')),
    ('Mission_Ore_Convoy6_Heavy', ('after', 'Mission_Ore_Convoy5_Cases')),
    ('Mission_Ore_RadioOutpost', ('after', 'Mission_Ore_Opening')),
    ('Mission_Ore_RecoverCases', ('after', 'Mission_Ore_RadioOutpost', ['Mission_Ore_Convoy4_Hunters'])),
    ('Mission_Ore_DestroyBaseB', ('after', 'Mission_Ore_Opening')),
    ('Mission_Ore_BaseA', ('near', (2010.0, 24120.0, 1400.0), 9000.0)),
    ('Mission_Ore_Battery', ('near', (18760.0, 13400.0, 950.0), 9000.0)),
    ('Mission_Ore_Ending', ('after', 'Mission_Ore_DestroyBaseB')),
]

# Per-instance properties (the Intro map showed a data asset equal to the CDO value coming back empty on load).
INSTANCE_ASSETS = {
    'Mission_Ore_LightCounterattack': {'settings': STAGE_DIR + 'DA_Ore_LightCounterattack'},
    'Mission_Ore_HeavyCounterattack': {'settings': STAGE_DIR + 'DA_Ore_HeavyCounterattack'},
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
        also = [classes.get(n) for n in (spec[2] if len(spec) > 2 else [])]
        return (isinstance(trigger, unreal.MissionClassCompletedMissionTrigger)
                and trigger.get_editor_property('mission_class_to_watch') == classes.get(spec[1])
                and trigger.get_editor_property('also_trigger_when_failed') == (kind == 'after_or_failed')
                and list(trigger.get_editor_property('also_watch_mission_classes')) == also)
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
        if len(spec) > 2:
            trigger.set_editor_property('also_watch_mission_classes', [stage_class(n) for n in spec[2]])
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
