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
    'Mission_Berlin_Patrols_C': ['ForcesPerDifficulty', 'PatrolSpawnerIds'],
    'Mission_Berlin_SupplyPoint_C': ['IntroLine', 'RewardPayload', 'RewardLandingOffset'],
}


def log(text):
    unreal.log_warning('BERLIN_VERIFY ' + text)


def as_text(value):
    if hasattr(value, 'items'):
        return '{} entries: '.format(len(value)) + ', '.join(f'{k}={v}' for k, v in value.items())[:300]
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
for spawner in [a for a in actors if a.get_class().get_name() == 'EnemyWaveSpawner']:
    patrol = spawner.get_editor_property('patrol_settings')
    log(f"spawner {spawner.get_actor_label()} id={spawner.get_editor_property('spawner_id')} "
        f"at {spawner.get_actor_location()} wps={len(spawner.get_editor_property('waypoints'))} "
        f"start_on_begin_play={spawner.get_editor_property('timing').get_editor_property('start_on_begin_play')} "
        f"flee={patrol.get_editor_property('infantry_flees_when_vehicles_lost')} "
        f"minimap={spawner.get_editor_property('minimap').get_editor_property('show_on_minimap')}")
