"""Ger_IntroMission enemy AI: train Mosin squads at the Soviet barracks (user request 2026-11-21).

Exec in a throwaway editor with the map open (rear_editor_server / rear_cmd, after rear_lib.py), Development DLL from
2026-11-21 or later (it has FEnemyAIMissionSettings::RestrictTrainingTo* and AEnemyController::CopyStrategicDecisionTreeFrom):
- gives the placed RTS_BP_EnemyController its own copy of the survival decision tree (DA_SurvivalWaveSettings).
  The map had no tree, so training pressure never built and the AI trained nothing.
- keeps its training to Squad_Rus_Mosin (no tanks), so the two Rus_Building_Barracks_Concrete train Mosin squads.
- pace (user 2026-11-21: one Mosin squad per 2 minutes): a Mosin squad costs 3 training points, so 1.5 points a minute
  from 0 start points (needs the 2026-11-21 build where TrainingPointsPerMinute is a float).
Direct control (attacks from the tree) stays as the map has it (off), so the tree only feeds training pressure.
Then waits for the navmesh build and saves the map (saving before the build finishes saves an empty navmesh).
"""
import time

WAVE_SETTINGS = '/Game/RTS_Survival/SurvivalMode/Data/Waves/DA_SurvivalWaveSettings'

require_map()
ec = [a for a in all_actors() if a.get_class().get_name() == 'RTS_BP_EnemyController_C'][0]
template = unreal.load_asset(WAVE_SETTINGS).get_editor_property('strategic_ai').get_editor_property('decision_tree_template')
print('template', template.get_path_name() if template else None)
if template is None:
    raise RuntimeError('DA_SurvivalWaveSettings has no decision tree template')
if ec.get_editor_property('m_stochastic_decision_tree') is None:
    ec.call_method('CopyStrategicDecisionTreeFrom', (template,))
tree = ec.get_editor_property('m_stochastic_decision_tree')
print('tree on controller', tree.get_path_name() if tree else None)

s = ec.get_editor_property('m_enemy_ai_mission_settings')
s.set_editor_property('restrict_training_to_squads', [unreal.SquadSubtype.SQUAD_RUS_MOSIN])
s.set_editor_property('restrict_training_to_tanks', [])
# One squad per purchase: a random 2-squad batch would save up and arrive in pairs every 4 minutes.
s.set_editor_property('min_units_trained_per_batch', 1)
s.set_editor_property('max_units_trained_per_batch', 1)
ec.set_editor_property('m_enemy_ai_mission_settings', s)
print('allow training', s.get_editor_property('allow_unit_training'),
      'direct control', s.get_editor_property('allow_direct_control_stochastic_decision_tree'),
      'squads', s.get_editor_property('restrict_training_to_squads'),
      'batch', s.get_editor_property('min_units_trained_per_batch'), s.get_editor_property('max_units_trained_per_batch'))

MOSIN_POINTS_PER_MINUTE = 1.5  # Mosin squad = 3 points -> one squad every 2 minutes
START_POINTS = 0
lt = ec.get_editor_property('m_enemy_level_training')
print('before: points/min', lt.get_editor_property('training_points_per_minute'),
      'start', lt.get_editor_property('training_points_start'))
lt.set_editor_property('training_points_per_minute', MOSIN_POINTS_PER_MINUTE)
lt.set_editor_property('training_points_start', START_POINTS)
ec.set_editor_property('m_enemy_level_training', lt)
lt = ec.get_editor_property('m_enemy_level_training')
print('after: points/min', lt.get_editor_property('training_points_per_minute'),
      'start', lt.get_editor_property('training_points_start'))

_st = {'t0': time.time()}


def _wait_nav_then_save():
    if not nav_ready():
        return False
    navs = [a for a in all_actors() if a.get_class().get_name() == 'RecastNavMesh']
    if any(a.get_actor_bounds(False)[1].x == 0 for a in navs) or time.time() - _st['t0'] < 10:
        return False
    save_level()
    return True


_defer = _wait_nav_then_save
