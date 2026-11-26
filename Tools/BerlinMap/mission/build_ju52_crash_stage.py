"""Builds Mission_Berlin_Ju52Crash (BerlinMissionScripting.md section 14): BONUS "The downed transport".

A UPlayerUnitsNearMissionTrigger round the crash crater starts it (wire_berlin_stages.py). On start: the announcer's
line (Berlin_Cinematic_Ju52Crash), one Squad_Ger_JagerTruppKar98k (the surviving crew) appears on the crater's west
rim and walks out to SurvivorRallyLocation (reinforcements ping), CompleteDelaySeconds later the plate completes.
The crash site itself (DC-3 stand-in hull, parts, scavengeable cargo on the rim) is map content:
Tools/BerlinMap/poi/south_pois.py. The crater floor is a navmesh island, so the crew spawns on the rim.
Modelled on build_field_hospital_stage.py / Mission_Berlin_CellarPeople.
Usage: python build_ju52_crash_stage.py <port>     (builds functions, defaults and the event graph, compiles, saves)
"""
import sys

from build_field_hospital_stage import PLAY_LINE, show
from layout_eventgraphs import BASE, Graph, var
from uecp_client import action, call

NAME = 'Mission_Berlin_Ju52Crash'
BP = BASE + NAME
LINE = ('/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/Berlin/'
        'Berlin_Cinematic_Ju52Crash.Berlin_Cinematic_Ju52Crash')
JAGER_SUBTYPE = 2  # ESquadSubtype::Squad_Ger_JagerTruppKar98k (checked with unreal.SquadSubtype in editor Python)

VARIABLES = [
    {'variable_name': 'Ju52Line', 'variable_type': 'object:SoundBase', 'instance_editable': True,
     'category': 'Berlin|Lines', 'tooltip': 'The announcer when the crash site is found.'},
    {'variable_name': 'SurvivorSquadOption', 'variable_type': 'struct:TrainingOption', 'instance_editable': True,
     'category': 'Berlin|Reward', 'tooltip': 'The surviving crew (a Jaeger Kar98k squad: no aircrew squad exists).'},
    {'variable_name': 'SurvivorSpawnLocation', 'variable_type': 'vector', 'default_value': '(X=-17350,Y=2750,Z=0)',
     'instance_editable': True, 'category': 'Berlin|Reward',
     'tooltip': 'On the crater\'s west rim by the cargo: the crater floor is a navmesh island.'},
    {'variable_name': 'SurvivorRallyLocation', 'variable_type': 'vector', 'default_value': '(X=-16900,Y=1700,Z=0)',
     'instance_editable': True, 'category': 'Berlin|Reward', 'tooltip': 'Where the crew walks to after appearing.'},
    {'variable_name': 'SurvivorRotation', 'variable_type': 'rotator', 'default_value': '(Pitch=0,Yaw=-60,Roll=0)',
     'category': 'Berlin|Reward'},
    {'variable_name': 'NoBehaviours', 'variable_type': 'class', 'object_class': 'Behaviour', 'is_array': True,
     'category': 'Berlin|Reward', 'tooltip': 'Empty: the crew gets no extra behaviours (the array pin must be wired).'},
    {'variable_name': 'CompleteDelaySeconds', 'variable_type': 'int', 'default_value': '10',
     'instance_editable': True, 'category': 'Berlin|Timing',
     'tooltip': 'Seconds after the crew appears before the plate completes (the line is 13.1 s).'},
]

PLATE = ('(MissionTitle=INVTEXT("The downed transport"),MissionDescription=INVTEXT("A Ju 52 came down in a crater. '
         'Its surviving crew joins the raid; scavengers can salvage the cargo on the rim."),TextSpeed=TS_None,'
         'NextButtonType=Btn_Next,ObjectiveType=Bonus)')

SPAWN = {
    'function': 'SpawnSurvivors',
    'body': [
        {'op': 'make_struct', 'struct': 'MissionSpawnCommandQueueOrder', 'as': 'WalkOut',
         'fields': {'AbilityID': 'IdMove', 'TargetLocation': '$SurvivorRallyLocation'}},
        {'op': 'make_array', 'as': 'Orders', 'items': ['$WalkOut']},
        {'op': 'call', 'func': 'Self.AsyncSpawnActorAtLocationWithQueue', 'args': {
            'TrainingOption': '$SurvivorSquadOption', 'ID': 0, 'SpawnLocation': '$SurvivorSpawnLocation',
            'Rotation': '$SurvivorRotation', 'BehavioursToApply': '$NoBehaviours', 'CommandQueue': '$Orders'}},
    ],
}


def event_graph():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('spawn', 'fn.Self.SpawnSurvivors', None),
           ('ping', 'fn.MissionBase.ShowReinforcementsOnMiniMap', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnCrashSiteDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('play', [('getLine', var('Ju52Line'), 'Line', 'Ju52Line')])
    g.getters('ping', [('getSpawn', var('SurvivorSpawnLocation'), 'WorldLocation', 'SurvivorSpawnLocation')])
    g.default('ping', 'DurationSeconds', 8)
    g.getters('sched', [('getDelay', var('CompleteDelaySeconds'), 'DelaySeconds', 'CompleteDelaySeconds')])
    g.wire('evDone.OutputDelegate', 'sched.Callback')
    g.end_row()
    g.comment('Crash site found: line, the surviving crew climbs out on the rim (ping), complete after '
              'CompleteDelaySeconds',
              ['evStart', 'play', 'getLine', 'spawn', 'ping', 'getSpawn', 'sched', 'getDelay', 'evDone', 'complete'])
    return g


def main():
    port = int(sys.argv[1])
    show('create', action(port, 'create_asset', {'asset_type': 'blueprint', 'name': NAME, 'save_path': BASE[:-1],
                                                 'parent_class': '/Script/RTS_Survival.MissionBase'}))
    for item in VARIABLES:
        show('var ' + item['variable_name'], action(port, 'add_variable', dict(item, blueprint_path=BP)))
    for name, value in (('Ju52Line', LINE),
                        ('SurvivorSquadOption', f'(UnitType=UNType_Squad,SubtypeValue={JAGER_SUBTYPE})')):
        show('default ' + name, action(port, 'set_blueprint_variable_default',
                                       {'blueprint_path': BP, 'variable_name': name, 'default_value': value}))
    show('compile vars', action(port, 'compile_blueprint', {'blueprint_path': BP}))
    for spec in (PLAY_LINE, SPAWN):
        show(spec['function'], action(port, 'synthesize_graph', dict(spec, blueprint=BP)))
    show('compile funcs', action(port, 'compile_blueprint', {'blueprint_path': BP}))
    g = event_graph()
    show('event graph', action(port, 'build_blueprint_graph', {
        'blueprint_path': BP, 'graph_name': 'EventGraph', 'clear_before_build': True, 'clear_force': True,
        'nodes': g.nodes, 'connections': g.connections, 'defaults': g.defaults, 'comments': g.comments}))
    c = action(port, 'compile_blueprint', {'blueprint_path': BP})
    print('compile', c.get('error_count'), str(c.get('errors', ''))[:900])
    code = (f'import unreal\nbp = unreal.load_asset("{BP}")\ncdo = unreal.get_default_object(bp.generated_class())\n'
            f'st = cdo.get_editor_property("mission_widget_state")\nst.import_text({PLATE!r})\n'
            f'cdo.set_editor_property("mission_widget_state", st)\nunreal.BlueprintEditorLibrary.compile_blueprint(bp)\n'
            f'print(unreal.EditorAssetLibrary.save_loaded_asset(bp, False))\n')
    show('plate + save', call(port, 'execute_python', {'code': code, 'description': 'Ju 52 crash plate'}))


if __name__ == '__main__':
    main()
