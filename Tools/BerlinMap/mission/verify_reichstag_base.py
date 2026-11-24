"""Read-only check (commandlet is fine): load Ger_Berlin from disk and log the Reichstag base actors (RT_Base_*), their
tags, owners, the factory's training exit, and the navmesh actor count."""
import unreal


def log(text):
    unreal.log_warning('RT_BASE_VERIFY ' + text)


world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin')
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
base = sorted((a for a in actors if a.get_actor_label().startswith('RT_Base_')), key=lambda a: a.get_actor_label())
for a in base:
    loc = a.get_actor_location()
    rtsc = a.get_components_by_class(unreal.RTSComponent)
    owner = rtsc[0].get_editor_property('owning_player') if rtsc else '?'
    log(f'{a.get_actor_label()} at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}) yaw {a.get_actor_rotation().yaw:.0f} '
        f'owner {owner} folder {a.get_folder_path()} tags {[str(t) for t in a.tags]}')
    for c in a.get_components_by_class(unreal.SceneComponent):
        if c.get_name() == 'AITrainingExit':
            p = c.get_world_location()
            log(f'  AITrainingExit ({p.x:.0f}, {p.y:.0f}, {p.z:.0f}) yaw {c.get_world_rotation().yaw:.0f}')
log(f'{len(base)} base actors; factory-tagged: '
    f'{sum(1 for a in base if "BerlinReichstagFactory" in [str(t) for t in a.tags])}')
log('navmesh actors: {}'.format(len([a for a in actors if a.get_class().get_name() == 'RecastNavMesh'])))
# Reichstag training (section 10): patrol routes, the enemy controller's tree and start flag, Arrival's asset.
for route in [a for a in actors if a.get_class().get_name() == 'BerlinReichstagPatrolRoute']:
    log(f"route {route.get_actor_label()}: {len(route.get_world_patrol_points())} points")
for ec in [a for a in actors if a.get_class().get_name() == 'RTS_BP_EnemyController_C']:
    tree = ec.get_editor_property('m_stochastic_decision_tree')
    log(f"controller tree {'set' if tree else 'MISSING'}, training at load "
        f"{ec.get_editor_property('m_enemy_ai_mission_settings').get_editor_property('allow_unit_training')}")
for mgr in [a for a in actors if a.get_class().get_name() == 'BP_MissionManager_C']:
    for m in mgr.get_editor_property('missions'):
        if m.get_class().get_name() == 'Mission_Berlin_Arrival_C':
            asset = m.get_editor_property('ReichstagTraining')
            log(f"Arrival.ReichstagTraining = {asset.get_path_name() if asset else None}")
