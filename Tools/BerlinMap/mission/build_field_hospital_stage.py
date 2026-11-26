"""Builds Mission_Berlin_FieldHospital (BerlinMissionScripting.md section 13): BONUS "The field hospital".

A UPlayerUnitsNearMissionTrigger round the Red Cross cellar starts it (wire_berlin_stages.py). On start: the
announcer's line (Berlin_Cinematic_FieldHospital), find the POI by its actor tag, the first Squad_Ger_Medic squad
walks out of the cellar door to its rally point (reinforcements ping), SecondSquadDelaySeconds later the second
squad follows, CompleteDelaySeconds later the plate completes (after the 9.9 s line).
Modelled on Mission_Berlin_CellarPeople (PlayAdvisorLine copied from it).
Usage: python build_field_hospital_stage.py <port>     (builds functions, defaults and the event graph, compiles)
"""
import sys

from layout_eventgraphs import BASE, Graph, var
from uecp_client import action, call

NAME = 'Mission_Berlin_FieldHospital'
BP = BASE + NAME
LINE = ('/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/Berlin/'
        'Berlin_Cinematic_FieldHospital.Berlin_Cinematic_FieldHospital')
MANAGER = {'op': 'call', 'func': 'Self.GetMissionManagerChecked'}
MEDIC_SUBTYPE = 49  # ESquadSubtype::Squad_Ger_Medic (checked with unreal.SquadSubtype in editor Python)

# Offsets in the POI actor's frame (BP_BerlinFieldHospital, pivot = the E5 shell's pivot, +X = the street front).
# The cellar door of SM_BerlinRuin_E5_Shell1St_A is centred at (500, 210); its wall hull ends ~550, so the squads
# appear on the step in front of the door, between the sandbag cheeks, and walk out to the yard.
DOOR_OFFSET = '(X=640,Y=210,Z=0)'
RALLY_OFFSET_1 = '(X=1150,Y=40,Z=0)'
RALLY_OFFSET_2 = '(X=1150,Y=380,Z=0)'

VARIABLES = [
    {'variable_name': 'HospitalLine', 'variable_type': 'object:SoundBase', 'instance_editable': True,
     'category': 'Berlin|Lines', 'tooltip': 'The announcer when the Red Cross cellar is found.'},
    {'variable_name': 'HospitalTag', 'variable_type': 'name', 'default_value': 'BerlinFieldHospital',
     'instance_editable': True, 'category': 'Berlin|Hospital',
     'tooltip': 'Actor tag of the placed BP_BerlinFieldHospital; the door and rally points are read from it.'},
    {'variable_name': 'MedicSquadOption', 'variable_type': 'struct:TrainingOption', 'instance_editable': True,
     'category': 'Berlin|Reward', 'tooltip': 'One medic squad; two are given.'},
    {'variable_name': 'DoorOffset', 'variable_type': 'vector', 'default_value': DOOR_OFFSET,
     'category': 'Berlin|Hospital', 'tooltip': 'Where the squads appear, in the hospital actor\'s frame.'},
    {'variable_name': 'RallyOffset1', 'variable_type': 'vector', 'default_value': RALLY_OFFSET_1,
     'category': 'Berlin|Hospital', 'tooltip': 'Where the first squad walks to, in the hospital actor\'s frame.'},
    {'variable_name': 'RallyOffset2', 'variable_type': 'vector', 'default_value': RALLY_OFFSET_2,
     'category': 'Berlin|Hospital', 'tooltip': 'Where the second squad walks to, in the hospital actor\'s frame.'},
    {'variable_name': 'DoorLocation', 'variable_type': 'vector', 'instance_editable': True,
     'category': 'Berlin|Hospital', 'tooltip': 'World spawn point; set from the tagged actor (this value is only '
                                               'used when no actor carries HospitalTag).'},
    {'variable_name': 'RallyLocation1', 'variable_type': 'vector', 'instance_editable': True,
     'category': 'Berlin|Hospital', 'tooltip': 'World rally point of squad 1; set from the tagged actor.'},
    {'variable_name': 'RallyLocation2', 'variable_type': 'vector', 'instance_editable': True,
     'category': 'Berlin|Hospital', 'tooltip': 'World rally point of squad 2; set from the tagged actor.'},
    {'variable_name': 'DoorRotation', 'variable_type': 'rotator', 'category': 'Berlin|Hospital'},
    {'variable_name': 'NoBehaviours', 'variable_type': 'class', 'object_class': 'Behaviour', 'is_array': True,
     'category': 'Berlin|Reward', 'tooltip': 'Empty: the medics get no extra behaviours (the array pin must be wired).'},
    {'variable_name': 'SecondSquadDelaySeconds', 'variable_type': 'int', 'default_value': '4',
     'instance_editable': True, 'category': 'Berlin|Timing',
     'tooltip': 'Seconds between the first and the second squad coming out.'},
    {'variable_name': 'CompleteDelaySeconds', 'variable_type': 'int', 'default_value': '6',
     'instance_editable': True, 'category': 'Berlin|Timing',
     'tooltip': 'Seconds after the second squad before the plate completes (the line is 9.9 s).'},
]

PLATE = ('(MissionTitle=INVTEXT("The field hospital"),MissionDescription=INVTEXT("A Red Cross flag over a cellar in '
         'the ruins. Two medic squads join the raid."),TextSpeed=TS_None,NextButtonType=Btn_Next,ObjectiveType=Bonus)')

PLAY_LINE = {  # as Mission_Berlin_CellarPeople.PlayAdvisorLine
    'function': 'PlayAdvisorLine',
    'inputs': [{'name': 'Line', 'type': 'object:SoundBase'}],
    'body': [{'op': 'is_valid', 'value': '$Line', 'then': [
        {'op': 'call', 'func': 'Self.PlayPortrait', 'args': {'PortraitType': 'GermanAnnouncer', 'VoiceLine': '$Line'}}]}],
}


def hospital_point(var_name, offset_var):
    return {'op': 'call', 'func': 'KismetMathLibrary.TransformLocation',
            'args': {'T': '$HospitalTransform', 'Location': f'${offset_var}'}, 'as': f'{var_name}World'}


FIND = {
    'function': 'FindHospital',
    'outputs': [{'name': 'bFound', 'type': 'bool'}],
    'body': [
        {'op': 'call', 'func': 'GameplayStatics.GetAllActorsWithTag',
         'args': {'WorldContextObject': MANAGER, 'Tag': '$HospitalTag'}, 'as': 'Hospitals'},
        {'op': 'branch', 'condition': '=len($Hospitals) > 0', 'then': [
            {'op': 'call', 'func': 'Actor.GetTransform', 'target': '=$Hospitals[0]', 'as': 'HospitalTransform'},
            hospital_point('Door', 'DoorOffset'),
            hospital_point('Rally1', 'RallyOffset1'),
            hospital_point('Rally2', 'RallyOffset2'),
            {'op': 'set_var', 'var': 'DoorLocation', 'value': '$DoorWorld'},
            {'op': 'set_var', 'var': 'RallyLocation1', 'value': '$Rally1World'},
            {'op': 'set_var', 'var': 'RallyLocation2', 'value': '$Rally2World'},
            {'op': 'call', 'func': 'Actor.K2_GetActorRotation', 'target': '=$Hospitals[0]', 'as': 'HospitalRotation'},
            {'op': 'set_var', 'var': 'DoorRotation', 'value': '$HospitalRotation'},
            {'op': 'return', 'values': {'bFound': True}}]},
        {'op': 'return', 'values': {'bFound': False}},
    ],
}

SPAWN = {
    'function': 'SpawnMedicSquad',
    'inputs': [{'name': 'RallyPoint', 'type': 'vector'}],
    'body': [
        {'op': 'make_struct', 'struct': 'MissionSpawnCommandQueueOrder', 'as': 'WalkOut',
         'fields': {'AbilityID': 'IdMove', 'TargetLocation': '$RallyPoint'}},
        {'op': 'make_array', 'as': 'Orders', 'items': ['$WalkOut']},
        {'op': 'call', 'func': 'Self.AsyncSpawnActorAtLocationWithQueue', 'args': {
            'TrainingOption': '$MedicSquadOption', 'ID': 0, 'SpawnLocation': '$DoorLocation',
            'Rotation': '$DoorRotation', 'BehavioursToApply': '$NoBehaviours', 'CommandQueue': '$Orders'}},
    ],
}


def event_graph():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('find', 'fn.Self.FindHospital', None), ('spawn1', 'fn.Self.SpawnMedicSquad', None),
           ('ping', 'fn.MissionBase.ShowReinforcementsOnMiniMap', None),
           ('sched1', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evSecond', 'ev.CustomEvent', {'custom_name': 'OnSecondSquad', 'no_chain': True}),
           ('spawn2', 'fn.Self.SpawnMedicSquad', None), ('sched2', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnHospitalDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('play', [('getLine', var('HospitalLine'), 'Line', 'HospitalLine')])
    g.getters('spawn1', [('getRally1', var('RallyLocation1'), 'RallyPoint', 'RallyLocation1')])
    g.getters('ping', [('getDoor', var('DoorLocation'), 'WorldLocation', 'DoorLocation')])
    g.default('ping', 'DurationSeconds', 8)
    g.getters('sched1', [('getSecond', var('SecondSquadDelaySeconds'), 'DelaySeconds', 'SecondSquadDelaySeconds')])
    g.wire('evSecond.OutputDelegate', 'sched1.Callback')
    g.getters('spawn2', [('getRally2', var('RallyLocation2'), 'RallyPoint', 'RallyLocation2')])
    g.getters('sched2', [('getDelay', var('CompleteDelaySeconds'), 'DelaySeconds', 'CompleteDelaySeconds')])
    g.wire('evDone.OutputDelegate', 'sched2.Callback')
    g.end_row()
    g.comment('Red Cross cellar found: line, first medic squad walks out of the cellar (ping), the second one after '
              'SecondSquadDelaySeconds, complete after CompleteDelaySeconds',
              ['evStart', 'play', 'getLine', 'find', 'spawn1', 'getRally1', 'ping', 'getDoor', 'sched1', 'getSecond',
               'evSecond', 'spawn2', 'getRally2', 'sched2', 'getDelay', 'evDone', 'complete'])
    return g


def show(label, r):
    ok = r.get('ok') if isinstance(r, dict) else None
    err = str(r.get('error', '') if isinstance(r, dict) else r)[:900].encode('ascii', 'replace').decode()
    print(label, ok, err)


def main():
    port = int(sys.argv[1])
    show('create', action(port, 'create_asset', {'asset_type': 'blueprint', 'name': NAME, 'save_path': BASE[:-1],
                                                 'parent_class': '/Script/RTS_Survival.MissionBase'}))
    for item in VARIABLES:
        show('var ' + item['variable_name'], action(port, 'add_variable', dict(item, blueprint_path=BP)))
    for name, value in (('HospitalLine', LINE),
                        ('MedicSquadOption', f'(UnitType=UNType_Squad,SubtypeValue={MEDIC_SUBTYPE})')):
        show('default ' + name, action(port, 'set_blueprint_variable_default',
                                       {'blueprint_path': BP, 'variable_name': name, 'default_value': value}))
    show('compile vars', action(port, 'compile_blueprint', {'blueprint_path': BP}))
    for spec in (PLAY_LINE, FIND, SPAWN):
        show(spec['function'], action(port, 'synthesize_graph', dict(spec, blueprint=BP)))
    show('compile funcs', action(port, 'compile_blueprint', {'blueprint_path': BP}))
    g = event_graph()
    show('event graph', action(port, 'build_blueprint_graph', {
        'blueprint_path': BP, 'graph_name': 'EventGraph', 'clear_before_build': True, 'clear_force': True,
        'nodes': g.nodes, 'connections': g.connections, 'defaults': g.defaults, 'comments': g.comments}))
    c = action(port, 'compile_blueprint', {'blueprint_path': BP})
    print('compile', c.get('error_count'), str(c.get('errors', ''))[:900])
    # The plate, on the class default (set before the stage goes into Missions[]: an instance placed earlier keeps
    # "Mission Title"). Then save.
    code = (f'import unreal\nbp = unreal.load_asset("{BP}")\ncdo = unreal.get_default_object(bp.generated_class())\n'
            f'st = cdo.get_editor_property("mission_widget_state")\nst.import_text({PLATE!r})\n'
            f'cdo.set_editor_property("mission_widget_state", st)\nunreal.BlueprintEditorLibrary.compile_blueprint(bp)\n'
            f'print(unreal.EditorAssetLibrary.save_loaded_asset(bp, False))\n')
    show('plate + save', call(port, 'execute_python', {'code': code, 'description': 'field hospital plate'}))
    # Function graphs: python tidy_graphs.py <port> 30 20 Mission_Berlin_FieldHospital, then splice knots once more.


if __name__ == '__main__':
    main()
