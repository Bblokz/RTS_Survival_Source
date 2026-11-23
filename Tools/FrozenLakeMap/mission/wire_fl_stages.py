"""Re-runnable wiring of the Frozen Lake stages into Ger_FrozenLake's BP_MissionManager (editor Python, editor world).

Order and triggers: Source/Docs/Campaign/FrozenLakeMission.md ("Mission stages"). Each stage gets its own index in
Missions[] and its own trigger:
  ('after', Stage)   Mission Class Completed watching Stage
  None               no trigger: starts at begin play
An existing instance of the right class keeps its object (so per-instance edits survive) and only gets its trigger
replaced when it differs. Missions already in the array that are not listed here are kept after the listed ones.

It also tags the placed actors the stages look for (TAGS). It does not save: run_wire_fl_stages.py wires, logs
describe(), rebuilds navigation and saves.
"""
import unreal

ROOT = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/FrozenLake/'

STAGES = [
    ('Main/Mission_FL_01_Arrival', None),
    ('Main/Mission_FL_02_KeepHQ', ('after', 'Main/Mission_FL_01_Arrival')),
    ('Main/Mission_FL_03_ReachIce', ('after', 'Main/Mission_FL_01_Arrival')),
    ('Main/Mission_FL_04_ThinIce', ('after', 'Main/Mission_FL_03_ReachIce')),
    ('Main/Mission_FL_05_RigGarrison', ('after', 'Main/Mission_FL_04_ThinIce')),
    ('Main/Mission_FL_06_BoardRig', ('after', 'Main/Mission_FL_05_RigGarrison')),
    ('Main/Mission_FL_07_HaulCore', ('after', 'Main/Mission_FL_06_BoardRig')),
    ('Main/Mission_FL_08_DestroyShieldHQ', ('after', 'Main/Mission_FL_07_HaulCore')),
    ('Bonus/Mission_FL_Bonus_FreeKingTiger', ('after', 'Main/Mission_FL_03_ReachIce')),
    ('Bonus/Mission_FL_Bonus_DestroyPumps', ('after', 'Main/Mission_FL_04_ThinIce')),
]

# Actor label prefix -> tag the stages search for. The kit actors were placed by fl_map_build.step_kits().
TAGS = [
    ('FL_Kit_pump_', 'FL_CryoPump'),
    ('FL_Kit_tiger_collar', 'FL_KingTigerCollarFrozen'),
]


def stage_class(path):
    if not unreal.EditorAssetLibrary.does_asset_exist(ROOT + path):
        return None
    return unreal.load_asset(ROOT + path).generated_class()


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
    return (isinstance(trigger, unreal.MissionClassCompletedMissionTrigger)
            and trigger.get_editor_property('mission_class_to_watch') == classes.get(spec[1])
            and not trigger.get_editor_property('also_trigger_when_failed'))


def make_trigger(mission, spec, classes):
    if spec is None:
        return None
    trigger = unreal.new_object(unreal.MissionClassCompletedMissionTrigger, outer=mission)
    trigger.set_editor_property('mission_class_to_watch', classes[spec[1]])
    trigger.set_editor_property('also_trigger_when_failed', False)
    return trigger


def tag_actors(world):
    """Adds the stage tags to the placed kit actors (labels from fl_map_build.step_kits)."""
    changes = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        label = actor.get_actor_label()
        for prefix, tag in TAGS:
            if not label.startswith(prefix):
                continue
            tags = list(actor.tags)
            if unreal.Name(tag) in tags:
                continue
            actor.modify()
            actor.tags = tags + [unreal.Name(tag)]
            changes.append(f'tag {label} += {tag}')
    return changes


def wire(world=None):
    if world is None:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    manager = find_manager(world)
    manager.modify()
    existing = [m for m in manager.get_editor_property('missions') if m is not None]
    classes = {path: stage_class(path) for path, _ in STAGES}
    changes, wired = [], []
    for path, spec in STAGES:
        cls = classes[path]
        if cls is None:
            changes.append(f'skipped {path} (no Blueprint)')
            continue
        mission = next((m for m in existing if m.get_class() == cls), None)
        if mission is None:
            mission = unreal.new_object(cls, outer=manager)
            changes.append(f'new {path}')
        if not trigger_matches(mission, spec, classes):
            mission.set_editor_property('mission_trigger', make_trigger(mission, spec, classes))
            changes.append(f'{path} trigger = {spec}')
        wired.append(mission)
    known = [c for c in classes.values() if c is not None]
    others = [m for m in existing if m.get_class() not in known]
    if others:
        changes.append('kept other missions after the Frozen Lake stages: '
                       + ', '.join(m.get_class().get_name() for m in others))
    manager.set_editor_property('missions', wired + others)
    return changes + tag_actors(world)


def describe(world=None):
    """One line per manager mission: class, trigger and plate title."""
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
            trigger_text = f'after {watched.get_name() if watched else None}'
        else:
            trigger_text = trigger.get_class().get_name() if trigger else 'no trigger'
        title = mission.get_editor_property('mission_widget_state').get_editor_property('mission_title')
        lines.append(f'Missions[{index}]: {mission.get_class().get_name()} [{trigger_text}] "{title}"')
    return lines
