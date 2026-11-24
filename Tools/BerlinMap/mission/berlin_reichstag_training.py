"""Reichstag base training (BerlinMissionScripting.md section 10). Editor Python in my own editor with Ger_Berlin open
(UECP execute_python: import this module and call the steps). Nothing here saves the map; the asset steps save assets.

  move_data_assets()     every Berlin data asset into DATA_DIR (next to the map), redirectors fixed up
  make_training_asset()  DA_Berlin_ReichstagTraining with the owner's ladder (2026-11-24): units A, pace 2/4/6/9,
                         cap 6/8/10/12; re-running overwrites the values
  place_routes()         two ABerlinReichstagPatrolRoute actors (ring round the ruin, forecourt to the Gate), points
                         projected onto the navmesh; move them freely afterwards
  setup_controller()     the placed enemy controller gets its own copy of the survival decision tree (no tree = no
                         training) and starts with training OFF: Mission_Berlin_Arrival switches it on per difficulty
"""
import unreal

DATA_DIR = '/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets'
OLD_DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin'
MOVE = ['DA_BerlinForces_NewToRTS', 'DA_BerlinForces_Normal', 'DA_BerlinForces_Hard', 'DA_BerlinForces_Brutal',
        'DA_Berlin_ResponseForce']
TRAINING_ASSET = 'DA_Berlin_ReichstagTraining'
WAVE_SETTINGS = '/Game/RTS_Survival/SurvivalMode/Data/Waves/DA_SurvivalWaveSettings'
FOLDER = 'Mission/ReichstagBase'

SQ = unreal.SquadSubtype
TK = unreal.TankSubtype
# Owner picks 2026-11-24: ladder A, pace A (points/min), cap A. Routing numbers are starting values to tune.
LEVELS = {
    'new_to_rts': dict(squads=[SQ.SQUAD_RUS_MOSIN, SQ.SQUAD_RUS_OKHOTNIK], tanks=[], ppm=2.0, cap=6,
                       patrol=2, patrols=1, attack=2, share=0.5),
    'normal': dict(squads=[SQ.SQUAD_RUS_MOSIN, SQ.SQUAD_RUS_OKHOTNIK], tanks=[TK.TANK_BT7], ppm=4.0, cap=8,
                   patrol=2, patrols=1, attack=3, share=0.5),
    'hard': dict(squads=[SQ.SQUAD_RUS_MOSIN, SQ.SQUAD_RUS_OKHOTNIK], tanks=[TK.TANK_BT7, TK.TANK_T34_76], ppm=6.0,
                 cap=10, patrol=3, patrols=2, attack=3, share=0.5),
    'brutal': dict(squads=[SQ.SQUAD_RUS_MOSIN, SQ.SQUAD_RUS_OKHOTNIK],
                   tanks=[TK.TANK_BT7, TK.TANK_T34_76, TK.TANK_T34_85], ppm=9.0, cap=12,
                   patrol=3, patrols=2, attack=4, share=0.6),
}

# Patrol routes in world XY (the ruin stands at (8112, -15995), portico south; the Gate ruin at (0, -8300)).
ROUTES = {
    'RT_PatrolRoute_Ring': [(15200, -11300), (15200, -20700), (8100, -22300), (1500, -20500), (1500, -11500),
                            (8100, -9800)],
    'RT_PatrolRoute_Gate': [(1200, -16500), (2600, -9800), (-3200, -8300), (-500, -13500)],
}
NAV_EXTENT = unreal.Vector(800, 800, 3000)


def log(text):
    unreal.log_warning('RT_TRAINING ' + str(text))


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def move_data_assets():
    lib = unreal.EditorAssetLibrary
    if not lib.does_directory_exist(DATA_DIR):
        lib.make_directory(DATA_DIR)
    for name in MOVE:
        src, dst = f'{OLD_DIR}/{name}', f'{DATA_DIR}/{name}'
        if lib.does_asset_exist(dst):
            log(f'{name}: already in {DATA_DIR}')
            continue
        ok = lib.rename_asset(src, dst)
        log(f'{name}: moved {ok}')
    # Then save the map (its stage instances hold the references) and remove the redirectors in OLD_DIR with UECP
    # fix_up_redirectors(folder_path=OLD_DIR, delete_redirectors=True); UE 5.5 Python has no fixup call.


def _make_override(spec):
    o = unreal.EnemyMissionTrainingOverride()
    o.set_editor_property('allow_unit_training', True)
    o.set_editor_property('squads', spec['squads'])
    o.set_editor_property('tanks', spec['tanks'])
    o.set_editor_property('training_points_per_minute', spec['ppm'])
    o.set_editor_property('training_points_start', 0)
    o.set_editor_property('min_units_trained_per_batch', 1)
    o.set_editor_property('max_units_trained_per_batch', 1)
    o.set_editor_property('max_alive_building_trained_units', spec['cap'])
    return o


def make_training_asset():
    path = f'{DATA_DIR}/{TRAINING_ASSET}'
    lib = unreal.EditorAssetLibrary
    if lib.does_asset_exist(path):
        asset = unreal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.BerlinReichstagTrainingData)
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            TRAINING_ASSET, DATA_DIR, unreal.BerlinReichstagTrainingData, factory)
    for field, spec in LEVELS.items():
        level = unreal.BerlinReichstagTrainingLevel()
        level.set_editor_property('training', _make_override(spec))
        level.set_editor_property('patrol_group_size', spec['patrol'])
        level.set_editor_property('max_reichstag_patrols', spec['patrols'])
        level.set_editor_property('attack_group_size', spec['attack'])
        level.set_editor_property('attack_share_after_alarm', spec['share'])
        asset.set_editor_property(field, level)
    saved = lib.save_loaded_asset(asset, only_if_is_dirty=False)
    log(f'{path}: saved {saved}')
    return asset


def _project(w, x, y):
    for dx, dy in ((0, 0), (600, 0), (-600, 0), (0, 600), (0, -600), (1200, 1200), (-1200, -1200)):
        p = unreal.NavigationSystemV1.project_point_to_navigation(w, unreal.Vector(x + dx, y + dy, 0), None, None,
                                                                  NAV_EXTENT)
        if p is not None and not (abs(p.x) < 1 and abs(p.y) < 1 and abs(p.z) < 1):
            return p
    return None


def place_routes():
    w = world()
    existing = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(
        w, unreal.BerlinReichstagPatrolRoute)}
    for label, xy in ROUTES.items():
        pts = [p for p in (_project(w, x, y) for x, y in xy) if p is not None]
        if len(pts) < 2:
            log(f'{label}: only {len(pts)} points on the navmesh, not placed')
            continue
        centre = unreal.Vector(sum(p.x for p in pts) / len(pts), sum(p.y for p in pts) / len(pts), 0)
        actor = existing.get(label)
        if actor is None:
            actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
                unreal.BerlinReichstagPatrolRoute, centre)
            actor.set_actor_label(label)
        actor.set_actor_location(centre, False, False)
        actor.set_folder_path(FOLDER)
        actor.set_editor_property('patrol_points', [unreal.Vector(p.x - centre.x, p.y - centre.y, p.z) for p in pts])
        log(f'{label}: {len(pts)} points ' + ' '.join(f'({p.x:.0f},{p.y:.0f})' for p in pts))


def setup_controller():
    w = world()
    ec = [a for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)
          if a.get_class().get_name() == 'RTS_BP_EnemyController_C'][0]
    if ec.get_editor_property('m_stochastic_decision_tree') is None:
        template = unreal.load_asset(WAVE_SETTINGS).get_editor_property('strategic_ai').get_editor_property(
            'decision_tree_template')
        ec.call_method('CopyStrategicDecisionTreeFrom', (template,))
    tree = ec.get_editor_property('m_stochastic_decision_tree')
    s = ec.get_editor_property('m_enemy_ai_mission_settings')
    # Off until Mission_Berlin_Arrival applies the difficulty (the map's defaults would train anything).
    s.set_editor_property('allow_unit_training', False)
    ec.set_editor_property('m_enemy_ai_mission_settings', s)
    log(f"controller tree {tree.get_path_name() if tree else None}, training "
        f"{ec.get_editor_property('m_enemy_ai_mission_settings').get_editor_property('allow_unit_training')}, "
        f"direct control {s.get_editor_property('allow_direct_control_stochastic_decision_tree')}")
