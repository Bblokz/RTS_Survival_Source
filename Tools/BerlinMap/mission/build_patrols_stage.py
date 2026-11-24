"""Builds Mission_Berlin_Patrols (BerlinMissionScripting.md section 8): variables, defaults and the three functions.
The event graph is laid out by layout_eventgraphs.py (Mission_Berlin_Patrols).
Usage: python build_patrols_stage.py <port>
"""
import sys

from uecp_client import action

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
BP = BASE + 'Mission_Berlin_Patrols'
MANAGER = {'op': 'call', 'func': 'Self.GetMissionManagerChecked'}

VARIABLES = [
    {'variable_name': 'ForcesPerDifficulty', 'variable_type': 'map', 'map_key_type': 'ERTSGameDifficulty',
     'map_value_type': 'softobject:BerlinMissionForcesData', 'instance_editable': True, 'category': 'Berlin|Forces',
     'tooltip': 'The Berlin forces data asset per difficulty (DA_BerlinForces_*); Ironman uses Brutal, a missing one Normal.'},
    {'variable_name': 'PatrolSpawnerIds', 'variable_type': 'name', 'is_array': True, 'instance_editable': True,
     'category': 'Berlin|Patrols',
     'tooltip': 'SpawnerId of each placed patrol wave spawner (BerlinPatrol_*); each spawns the data asset patrol.'},
    # Whole seconds: the mission scheduler takes int seconds.
    {'variable_name': 'PatrolStartDelaySeconds', 'variable_type': 'int', 'default_value': '3',
     'instance_editable': True, 'category': 'Berlin|Timing',
     'tooltip': 'Seconds after the mission starts before the patrols are sent out.'},
    {'variable_name': 'PatrolCheckSeconds', 'variable_type': 'int', 'default_value': '2',
     'instance_editable': True, 'category': 'Berlin|Timing', 'tooltip': 'Seconds between patrol counts.'},
    {'variable_name': 'PatrolForces', 'variable_type': 'object', 'sub_type': 'BerlinMissionForcesData',
     'category': 'Berlin|State'},
    {'variable_name': 'PatrolCheckTaskId', 'variable_type': 'int', 'default_value': '-1', 'category': 'Berlin|State'},
]

LOAD_PATROL_FORCES = {
    'function': 'LoadPatrolForces',
    'outputs': [{'name': 'bFound', 'type': 'bool'}],
    'body': [
        {'op': 'call', 'func': 'Self.GetGameDifficulty', 'as': 'Difficulty'},
        {'op': 'call', 'func': 'BerlinMissionForcesData.LoadForDifficulty',
         'args': {'ForcesPerDifficulty': '$ForcesPerDifficulty', 'Difficulty': '$Difficulty.DifficultyLevel'},
         'as': 'Loaded'},
        {'op': 'set_var', 'var': 'PatrolForces', 'value': '$Loaded'},
        {'op': 'is_valid', 'value': '$Loaded',
         'then': [{'op': 'return', 'values': {'bFound': True}}],
         'else': [{'op': 'return', 'values': {'bFound': False}}]},
    ],
}

START_PATROLS = {
    'function': 'StartPatrols',
    'body': [
        {'op': 'prop_get', 'object': '$PatrolForces', 'property': 'Patrols', 'as': 'PatrolsData'},
        {'op': 'break_struct', 'value': '$PatrolsData', 'into': {'PatrolUnits': 'Units', 'Fallback': 'FleeSettings'}},
        {'op': 'call', 'func': 'Self.GetEnemyControllerChecked', 'as': 'Enemy'},
        {'op': 'is_valid', 'value': '$Enemy', 'then': [
            {'op': 'call', 'func': 'EnemyController.AddToWaveSupply', 'target': '$Enemy',
             'args': {'AddSupply': '=$PatrolForces.GetPatrolUnitCount() * len($PatrolSpawnerIds)'}}]},
        {'op': 'call', 'func': 'EnemyFallbackSubsystem.GetEnemyFallbackSubsystem',
         'args': {'WorldContextObject': MANAGER}, 'as': 'FallbackSubsystem'},
        {'op': 'call', 'func': 'EnemyFallbackSubsystem.StartFallbackWatch', 'target': '$FallbackSubsystem',
         'args': {'Settings': '$FleeSettings'}},
        {'op': 'make_struct', 'struct': 'EnemyWaveSpawnerUnits', 'fields': {'BaseUnits': '$Units'},
         'as': 'SpawnerUnits'},
        {'op': 'for_each', 'array': '$PatrolSpawnerIds', 'as': 'SpawnerId', 'body': [
            {'op': 'call', 'func': 'EnemyWaveSpawner.FindEnemyWaveSpawner',
             'args': {'WorldContextObject': MANAGER, 'SpawnerIdToFind': '$SpawnerId'}, 'as': 'Spawner'},
            {'op': 'is_valid', 'value': '$Spawner', 'then': [
                {'op': 'call', 'func': 'EnemyWaveSpawner.SetUnits', 'target': '$Spawner',
                 'args': {'NewUnits': '$SpawnerUnits'}},
                {'op': 'call', 'func': 'EnemyWaveSpawner.StartSpawner', 'target': '$Spawner'}]}]},
    ],
}

REFRESH_PATROLS = {
    'function': 'RefreshPatrols',
    'outputs': [{'name': 'bAllGone', 'type': 'bool'}, {'name': 'LivingUnits', 'type': 'int'},
                {'name': 'SpawnedWaves', 'type': 'int'}],
    'body': [
        {'op': 'for_each', 'array': '$PatrolSpawnerIds', 'as': 'SpawnerId', 'body': [
            {'op': 'call', 'func': 'EnemyWaveSpawner.FindEnemyWaveSpawner',
             'args': {'WorldContextObject': MANAGER, 'SpawnerIdToFind': '$SpawnerId'}, 'as': 'Spawner'},
            {'op': 'is_valid', 'value': '$Spawner', 'then': [
                {'op': 'set_var', 'var': 'LivingUnits', 'value': '=$LivingUnits + $Spawner.GetLivingSpawnedUnitCount()'},
                {'op': 'set_var', 'var': 'SpawnedWaves', 'value': '=$SpawnedWaves + $Spawner.GetSpawnedWaveCount()'}]}]},
        {'op': 'branch', 'condition': '=$SpawnedWaves > 0', 'then': [
            {'op': 'call', 'func': 'Self.SetMissionObjectiveText', 'args': {
                # By-ref FText pins need a wired value, not a literal default.
                'Title': {'op': 'format_text', 'format': 'Soviet patrols'},
                'Description': {'op': 'format_text',
                                'format': 'Soviet patrols move near your start: {Left} enemy units left. Avoid them or '
                                          'wipe them out. Infantry that loses its tanks, or loses half its men, runs '
                                          'to the Reichstag and raises the alarm there.',
                                'args': {'Left': '$LivingUnits'}}}}]},
        {'op': 'return', 'values': {'bAllGone': '=$SpawnedWaves > 0 && $LivingUnits == 0',
                                    'LivingUnits': '$LivingUnits', 'SpawnedWaves': '$SpawnedWaves'}},
    ],
}


def main():
    port = int(sys.argv[1])
    for item in VARIABLES:
        result = action(port, 'add_variable', dict(item, blueprint_path=BP))
        print('var', item['variable_name'], result.get('ok'), result.get('variable_type'), result.get('error', ''))
    for spec in (LOAD_PATROL_FORCES, START_PATROLS, REFRESH_PATROLS):
        params = dict(spec, blueprint=BP)
        result = action(port, 'synthesize_graph', params)
        print('fn', spec['function'], result.get('ok') if isinstance(result, dict) else str(result)[:300])
        if not (isinstance(result, dict) and result.get('ok')):
            print('   ', str(result)[:1500].encode('ascii', 'replace').decode())
    compiled = action(port, 'compile_blueprint', {'blueprint_path': BP})
    print('compile', compiled if not isinstance(compiled, dict) else
          (compiled.get('error_count'), str(compiled.get('errors', compiled.get('messages', '')))[:1500]))


if __name__ == '__main__':
    main()
