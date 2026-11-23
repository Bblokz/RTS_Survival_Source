"""Read-only check (commandlet is fine): load Ger_Berlin from disk and log the mission manager's Missions array, the
per-instance values the stages depend on, the detection zones and the rail-yard actors."""
import sys

import unreal

sys.path.append('D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/mission')
import wire_berlin_stages as wbs  # noqa: E402

# Instance values that must survive the save (the Intro map lost a data asset equal to the CDO value once).
INSTANCE_CHECKS = {
    'Mission_Berlin_Counterattack_C': ['settings'],
    'Mission_Berlin_StayUndetected_C': ['FallbackSettings', 'ForecourtRadius'],
    'Mission_Berlin_ResponseForce_C': ['RailYardStageClass', 'ReachLeitturmStageClass', 'SabotageDelaySeconds'],
    'Mission_Berlin_RailYard_C': ['AlarmStageClass'],
    'Mission_Berlin_StopRunners_C': ['AlarmStageClass'],
    'Mission_Berlin_CellarPeople_C': ['BurrowerSquadOption'],
    'Mission_Berlin_RaidReichstag_C': ['bCompleteMissionWhenTrackingDone'],
    'Mission_Berlin_DemeterPod_C': ['bIsTextOnlyMission'],
    'Mission_Berlin_Arrival_C': ['CraterLine', 'LeitturmLine', 'StayQuietLine'],
}


def log(text):
    unreal.log_warning('BERLIN_VERIFY ' + text)


def as_text(value):
    if hasattr(value, 'export_text'):
        return value.export_text()[:160]
    if hasattr(value, 'get_path_name'):
        return value.get_path_name()
    return str(value)


world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin')
for line in wbs.describe(world):
    log(line)
for mission in wbs.find_manager(world).get_editor_property('missions'):
    for prop in INSTANCE_CHECKS.get(mission.get_class().get_name(), []):
        log(f'{mission.get_class().get_name()}.{prop} = {as_text(mission.get_editor_property(prop))}')
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
log('navmesh actors: {}'.format(len([a for a in actors if a.get_class().get_name() == 'RecastNavMesh'])))
log('RY_ actors: {}'.format(len([a for a in actors if a.get_actor_label().startswith('RY_')])))
for zone in [a for a in actors if a.get_class().get_name() == 'BP_BerlinDetectionZone_C']:
    log(f'zone {zone.get_actor_label()} at {zone.get_actor_location()} r {zone.get_editor_property("Radius")}')
