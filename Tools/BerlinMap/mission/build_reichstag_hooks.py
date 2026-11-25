"""Adds the Reichstag training hooks to two Berlin stages (BerlinMissionScripting.md section 10):
  Mission_Berlin_Arrival        variable ReichstagTraining (DA_Berlin_ReichstagTraining) + SetUpReichstagTraining
  Mission_Berlin_ResponseForce  StartReichstagAttacks (the alarm: base groups go for the player's HQ)
The event graphs are re-laid by layout_eventgraphs.py afterwards. Usage: python build_reichstag_hooks.py <port>
"""
import sys

from uecp_client import action

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
DATA = '/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/DA_Berlin_ReichstagTraining'
MANAGER = {'op': 'call', 'func': 'Self.GetMissionManagerChecked'}
SUBSYSTEM = {'op': 'call', 'func': 'BerlinReichstagTrainingSubsystem.GetBerlinReichstagTrainingSubsystem',
             'args': {'WorldContextObject': MANAGER}, 'as': 'TrainingSubsystem'}

SET_UP = {
    'function': 'SetUpReichstagTraining',
    'body': [
        {'op': 'call', 'func': 'Self.GetGameDifficulty', 'as': 'Difficulty'},
        SUBSYSTEM,
        {'op': 'is_valid', 'value': '$TrainingSubsystem', 'then': [
            {'op': 'call', 'func': 'BerlinReichstagTrainingSubsystem.StartReichstagTraining',
             'target': '$TrainingSubsystem',
             'args': {'TrainingData': '$ReichstagTraining', 'Difficulty': '$Difficulty.DifficultyLevel'}}]},
    ],
}

START_ATTACKS = {
    'function': 'StartReichstagAttacks',
    'body': [
        {'op': 'call', 'func': 'Self.GetPlayerNomadicsOfType', 'args': {'NomadicSubtype': 'Nomadic_GerHq'},
         'as': 'HQs'},
        SUBSYSTEM,
        {'op': 'is_valid', 'value': '$TrainingSubsystem', 'then': [
            {'op': 'branch', 'condition': '=len($HQs) > 0', 'then': [
                {'op': 'call', 'func': 'BerlinReichstagTrainingSubsystem.StartAttacksOnPlayerBase',
                 'target': '$TrainingSubsystem', 'args': {'Target': '=$HQs[0]'}}]}]},
    ],
}


def main():
    port = int(sys.argv[1])
    arrival = BASE + 'Mission_Berlin_Arrival'
    r = action(port, 'add_variable', {
        'blueprint_path': arrival, 'variable_name': 'ReichstagTraining',
        'variable_type': 'object:BerlinReichstagTrainingData', 'instance_editable': True, 'category': 'Berlin|Forces',
        'tooltip': 'The Reichstag base training for every difficulty (units, pace, cap, patrols and attacks).'})
    print('var', r.get('ok'), r.get('variable_type'), r.get('error', ''))
    r = action(port, 'set_blueprint_variable_default', {
        'blueprint_path': arrival, 'variable_name': 'ReichstagTraining',
        'default_value': DATA + '.DA_Berlin_ReichstagTraining'})
    print('default', r.get('ok'), str(r.get('error', ''))[:200])
    for bp, spec in ((arrival, SET_UP), (BASE + 'Mission_Berlin_ResponseForce', START_ATTACKS)):
        r = action(port, 'synthesize_graph', dict(spec, blueprint=bp))
        print(spec['function'], r.get('ok'), str(r.get('error', ''))[:900].encode('ascii', 'replace').decode())
        c = action(port, 'compile_blueprint', {'blueprint_path': bp})
        print('  compile', c.get('error_count'), str(c.get('errors', ''))[:600])


if __name__ == '__main__':
    main()
