"""Re-runnable rewire of the Ger_IntroMission mission chain for the ridge missions (editor Python, editor world).

Result on the level's BP_MissionManager:
  Missions[0] = Mission_Intro_MoveTo -> SovietPatrols -> HangarT26s -> RidgeArmor   (NextMission chain)
  Missions[1] = Mission_Intro_ClearSovietRig  trigger: Mission Class Completed = Mission_Intro_RidgeArmor
  Missions[2] = Mission_Intro_BaseBuilding    trigger: Mission Class Completed = Mission_Intro_ClearSovietRig
Missions[1] and [2] are activated at begin play and wait on their trigger (the manager ticks it), so a finished mission
hands on without one more level of NextMission nesting under Missions[0].

Run it in an editor that has Ger_IntroMission open (UECP execute_python, or `py` in a throwaway editor); it does not
save. Call rewire() and save the map yourself. Running it again keeps instances that are already right.
"""
import unreal

MISSION_DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/'


def mission_class(name):
    return unreal.load_asset(MISSION_DIR + name).generated_class()


def class_completed_trigger(outer, watched_class):
    trigger = unreal.new_object(unreal.MissionClassCompletedMissionTrigger, outer=outer)
    trigger.set_editor_property('mission_class_to_watch', watched_class)
    return trigger


def watched_class_of(mission):
    trigger = mission.get_editor_property('mission_trigger')
    if trigger is None or not isinstance(trigger, unreal.MissionClassCompletedMissionTrigger):
        return None
    return trigger.get_editor_property('mission_class_to_watch')


def ensure_triggered_mission(manager, existing, mission_cls, watched_cls):
    """A top-level mission of mission_cls (outer = manager) whose trigger waits for watched_cls to complete."""
    if existing is not None and existing.get_class() == mission_cls and watched_class_of(existing) == watched_cls:
        return existing, False
    mission = unreal.new_object(mission_cls, outer=manager)
    mission.set_editor_property('mission_trigger', class_completed_trigger(mission, watched_cls))
    return mission, True


def rewire(world=None):
    if world is None:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    managers = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                if a.get_class().get_name() == 'BP_MissionManager_C']
    if len(managers) != 1:
        raise RuntimeError(f'expected one BP_MissionManager, found {len(managers)}')
    manager = managers[0]
    ridge_cls = mission_class('Mission_Intro_RidgeArmor')
    rig_cls = mission_class('Mission_Intro_ClearSovietRig')
    base_cls = mission_class('Mission_Intro_BaseBuilding')
    hangar_cls = mission_class('Mission_Intro_HangarT26s')
    manager.modify()
    missions = list(manager.get_editor_property('missions'))

    hangar = missions[0]
    while hangar is not None and hangar.get_class() != hangar_cls:
        hangar = hangar.get_editor_property('next_mission')
    if hangar is None:
        raise RuntimeError('Mission_Intro_HangarT26s is not in the Missions[0] chain')
    changes = []
    ridge = hangar.get_editor_property('next_mission')
    if ridge is None or ridge.get_class() != ridge_cls:
        ridge = unreal.new_object(ridge_cls, outer=hangar)
        hangar.modify()
        hangar.set_editor_property('next_mission', ridge)
        changes.append('HangarT26s.NextMission = new Mission_Intro_RidgeArmor')
    if ridge.get_editor_property('next_mission') is not None:
        ridge.set_editor_property('next_mission', None)
        changes.append('RidgeArmor.NextMission cleared')

    while len(missions) < 3:
        missions.append(None)
    rig, made = ensure_triggered_mission(manager, missions[1], rig_cls, ridge_cls)
    if made:
        changes.append('Missions[1] = new Mission_Intro_ClearSovietRig (trigger: RidgeArmor completed)')
    base, made = ensure_triggered_mission(manager, missions[2], base_cls, rig_cls)
    if made:
        changes.append('Missions[2] = new Mission_Intro_BaseBuilding (trigger: ClearSovietRig completed)')
    missions[1], missions[2] = rig, base
    manager.set_editor_property('missions', missions)
    return changes


def describe(world=None):
    """One line per manager mission: the NextMission chain and the trigger it waits on."""
    if world is None:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    manager = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
               if a.get_class().get_name() == 'BP_MissionManager_C'][0]
    lines = []
    for index, mission in enumerate(manager.get_editor_property('missions')):
        chain = []
        while mission is not None:
            watched = watched_class_of(mission)
            chain.append(mission.get_class().get_name() + (f' [after {watched.get_name()}]' if watched else ''))
            mission = mission.get_editor_property('next_mission')
        lines.append(f'Missions[{index}]: ' + ' -> '.join(chain))
    return lines
