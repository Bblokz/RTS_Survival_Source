"""Builds Mission_OW_Arrival, stage M00 of Oder-Warthe (OderWartheMissionFlow.md 10.3 and 12).

One skippable camera tour (hold Space): the location title fades in from black over the misty lakes (B1), an overview
of the bunker line (B2), the five Werke (B3 on Werk 1), then the start column climbing the Berlin road onto the ridge.
The deck spawns on the road (ow_map_build.step_arrival_start); the mission adds two light armoured cars as escort and
drives everything up the road when the column shot comes (or when the tour ends, after a skip). No HQ after the
deck loaded: a fallback HQ spawns on the ridge. The stage completes once the column is released and the tour is over.

Usage: python build_arrival_stage.py <uecp port>     (array and struct defaults: set_arrival_defaults.py in the editor)
"""
import sys

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/mission')
from uecp_client import action  # noqa: E402

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OderWarthe/'
NAME = 'Mission_OW_Arrival'
BP = BASE + NAME
LINES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OderWarthe/'

# Werke from Saved/OderWarthe/terrain/OW_layout.json (cm); shown in this order on the tour.
WERKE = [('1', 8800, -4300), ('4', 18900, 15000), ('2', -3000, 10500), ('3', -14600, 19000), ('5', -25800, 26800)]
WERK_FOCUS_Z = 600

VARIABLES = [
    {'variable_name': 'B1Line', 'variable_type': 'object:SoundBase', 'instance_editable': True,
     'category': 'OW|Lines', 'tooltip': 'Advisor over the lakes: every cell has gone deaf, Shield jams us.'},
    {'variable_name': 'B2Line', 'variable_type': 'object:SoundBase', 'instance_editable': True,
     'category': 'OW|Lines', 'tooltip': 'Advisor over the bunker line: their own orders cut through the jam.'},
    {'variable_name': 'B3Line', 'variable_type': 'object:SoundBase', 'instance_editable': True,
     'category': 'OW|Lines', 'tooltip': 'Advisor on Werk 1: five bunker groups, one is talking, find it.'},
    {'variable_name': 'ColumnReleaseShot', 'variable_type': 'int', 'default_value': '6', 'instance_editable': True,
     'category': 'OW|Column', 'tooltip': 'Tour shot whose start sends the column up the road (6 = the last Werk; the '
                                         'column shot follows).'},
    {'variable_name': 'GatherCentre', 'variable_type': 'vector', 'default_value': '(X=-24878,Y=-26984,Z=1771)',
     'instance_editable': True, 'category': 'OW|Column',
     'tooltip': 'Where the deck spawns (the player start on the Berlin road); units around it drive up.'},
    {'variable_name': 'GatherRadius', 'variable_type': 'float', 'default_value': '3000', 'instance_editable': True,
     'category': 'OW|Column'},
    {'variable_name': 'ColumnEndSpacing', 'variable_type': 'float', 'default_value': '900', 'instance_editable': True,
     'category': 'OW|Column', 'tooltip': 'Spacing of the column on the ridge when it stops.'},
    {'variable_name': 'RoadRotation', 'variable_type': 'rotator', 'default_value': '(Pitch=0,Yaw=24.7,Roll=0)',
     'instance_editable': True, 'category': 'OW|Column', 'tooltip': 'Facing up the road.'},
    {'variable_name': 'FallbackHQLocation', 'variable_type': 'vector', 'default_value': '(X=-19700,Y=-24602,Z=2075)',
     'instance_editable': True, 'category': 'OW|Column',
     'tooltip': 'No HQ once the deck loaded (a test start): one spawns here, on the ridge.'},
    {'variable_name': 'MaxReleaseAttempts', 'variable_type': 'int', 'default_value': '15', 'instance_editable': True,
     'category': 'OW|Column', 'tooltip': 'Seconds to wait for the deck after a skip before driving what is there.'},
    {'variable_name': 'ReleaseRetrySeconds', 'variable_type': 'int', 'default_value': '1', 'instance_editable': True,
     'category': 'OW|Column'},
    {'variable_name': 'ReleaseAttempts', 'variable_type': 'int', 'default_value': '0', 'category': 'OW|State'},
    {'variable_name': 'DeckSettleAttempts', 'variable_type': 'int', 'default_value': '2', 'instance_editable': True,
     'category': 'OW|Column', 'tooltip': 'After a skip: tries (one per ReleaseRetrySeconds) between the deck spawning '
                                         'and the drive, so fresh units take the orders.'},
    {'variable_name': 'DeckSeenAttempts', 'variable_type': 'int', 'default_value': '0', 'category': 'OW|State'},
    {'variable_name': 'bColumnReleased', 'variable_type': 'bool', 'default_value': 'false', 'category': 'OW|State'},
    {'variable_name': 'bTourFinished', 'variable_type': 'bool', 'default_value': 'false', 'category': 'OW|State'},
    {'variable_name': 'bReleaseRetryScheduled', 'variable_type': 'bool', 'default_value': 'false',
     'category': 'OW|State'},
]

# Arrays and structs: their type strings are tried in order (the UECP type parser varies between builds).
ARRAY_VARIABLES = [
    ('ColumnRoute', ['TArray<vector>', 'TArray<FVector>', 'array:vector'],
     'Waypoints up the Berlin road onto the ridge; the last one is where the column fans out.'),
    ('EscortSpawnPoints', ['TArray<vector>', 'TArray<FVector>', 'array:vector'],
     'Lead car ahead of the HQ, rear car behind it, on the road.'),
    ('EscortUnits', ['TArray<struct:TrainingOption>', 'TArray<TrainingOption>', 'TArray<FTrainingOption>'],
     'The light escort (placeholder: Puma ahead, Sd.Kfz. 232/3 behind).'),
]
STRUCT_VARIABLES = [
    ('FallbackHQ', 'struct:TrainingOption', 'The German HQ truck, used only when the deck brings none.'),
]


def text(value):
    return {'op': 'format_text', 'format': value}


def vec(x, y, z):
    return '=MakeVector(%s, %s, %s)' % (x, y, z)


def shot(name, focus_start, focus_end, yaw, pitch, arm, seconds, blend=1.5, reveal=0, title='', subtitle='',
         line=None, line_delay=0.4):
    fields = {'FocusStart': focus_start, 'FocusEnd': focus_end, 'YawStart': yaw[0], 'YawEnd': yaw[1],
              'PitchStart': pitch[0], 'PitchEnd': pitch[1], 'ArmLengthStart': arm[0], 'ArmLengthEnd': arm[1],
              'Seconds': seconds, 'BlendInSeconds': blend, 'RevealRadius': reveal}
    # format_text refuses an empty format: an empty caption is the pin's default.
    if title:
        fields['Title'] = text(title)
    if subtitle:
        fields['Subtitle'] = text(subtitle)
    if line:
        fields.update({'VoiceLine': line, 'VoiceLinePortrait': 'GermanAnnouncer', 'VoiceLineDelaySeconds': line_delay})
    return {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': name, 'fields': fields}


def werk_shot(index, werk, line=None):
    werk_id, x, y = werk
    # Looking north-east (from the player's side); a slow push in and a small turn.
    yaw = 30 + 6 * index
    return shot('Werk%s' % werk_id, vec(x - 1800, y - 1300, WERK_FOCUS_Z), vec(x, y, WERK_FOCUS_Z), (yaw, yaw + 8),
                (12, 16), (7000, 6000), 3.5, blend=1.2, reveal=4500, title='Werk %s' % werk_id, line=line)


TOUR = {
    'function': 'PlayArrivalTour',
    'body': [
        # Dawn over the home basin from the ridge; the title card fades in from black (B1).
        shot('Lakes', vec(-9000, -19000, 200), vec(-5000, -16000, 200), (35, 45), (28, 24), (11000, 12000), 12,
             blend=0, reveal=6000, line='$B1Line', line_delay=2.5),
        # The whole bunker line across the lakes (B2).
        shot('Line', vec(-6000, 2000, 200), vec(0, 8000, 200), (40, 50), (10, 14), (22000, 24000), 7, blend=2.0,
             line='$B2Line', line_delay=0.3),
        werk_shot(0, WERKE[0], line='$B3Line'),
        werk_shot(1, WERKE[1]),
        werk_shot(2, WERKE[2]),
        werk_shot(3, WERKE[3]),
        werk_shot(4, WERKE[4]),
        # The column climbing the Berlin road onto the ridge.
        shot('Column', vec(-24060, -26608, 1875), vec(-21880, -25605, 1975), (24, 30), (14, 18), (5500, 6500), 7,
             reveal=3000, title='The ridge', subtitle='Deploy the HQ up here'),
        {'op': 'make_array', 'as': 'Shots',
         'items': ['$Lakes', '$Line', '$Werk1', '$Werk4', '$Werk2', '$Werk3', '$Werk5', '$Column']},
        {'op': 'make_struct', 'struct': 'LocationIntroTexts', 'as': 'TitleTexts', 'fields': {
            'Location': text('Oder-Warthe-Bogen'), 'LocationSubtitle': text('Festungsfront, Lebus district'),
            'Date': text('1976'), 'TimeOfDay': text('05:40 hrs')}},
        {'op': 'make_struct', 'struct': 'LocationIntroPresentation', 'as': 'TitlePresentation',
         'fields': {'bFadeFromBlack': True}},
        {'op': 'make_struct', 'struct': 'MissionCameraTourOpeningTitle', 'as': 'Title', 'fields': {
            'bShowTitle': True, 'Texts': '$TitleTexts', 'Presentation': '$TitlePresentation', 'DurationSeconds': 9}},
        {'op': 'make_struct', 'struct': 'MissionCameraTourSettings', 'as': 'TourSettings',
         'fields': {'bAllowSkipping': True, 'OpeningTitle': '$Title'}},
        {'op': 'call', 'func': 'Self.PlayCameraTour', 'args': {'Shots': '$Shots', 'Settings': '$TourSettings',
                                                                'ID': 1}},
    ],
}

SPAWN_ESCORT = {
    'function': 'SpawnEscort',
    'body': [
        {'op': 'call', 'func': 'Self.AsyncSpawnActorAtLocation', 'args': {
            'TrainingOption': '=$EscortUnits[0]', 'ID': 101, 'SpawnLocation': '=$EscortSpawnPoints[0]',
            'Rotation': '$RoadRotation'}},
        {'op': 'call', 'func': 'Self.AsyncSpawnActorAtLocation', 'args': {
            'TrainingOption': '=$EscortUnits[1]', 'ID': 102, 'SpawnLocation': '=$EscortSpawnPoints[1]',
            'Rotation': '$RoadRotation'}},
    ],
}

IS_COLUMN_SHOT = {
    'function': 'IsColumnShot',
    'inputs': [{'name': 'ShotIndex', 'type': 'int'}],
    'outputs': [{'name': 'bIsColumnShot', 'type': 'bool'}],
    'body': [{'op': 'return', 'values': {'bIsColumnShot': '=$ShotIndex == $ColumnReleaseShot'}}],
}

DRIVE = {
    'function': 'DriveColumnUp',
    'body': [
        {'op': 'call', 'func': 'Self.MovePlayerUnitsAlongRoute', 'args': {
            'GatherCentre': '$GatherCentre', 'GatherRadius': '$GatherRadius', 'Route': '$ColumnRoute',
            'EndSpacing': '$ColumnEndSpacing'}},
        {'op': 'call', 'func': 'Self.GetPlayerNomadicsOfType', 'args': {'NomadicSubtype': 'Nomadic_GerHq'},
         'as': 'HQs'},
        {'op': 'branch', 'condition': '=len($HQs) == 0', 'then': [
            {'op': 'call', 'func': 'Self.AsyncSpawnActorAtLocation', 'args': {
                'TrainingOption': '$FallbackHQ', 'ID': 200, 'SpawnLocation': '$FallbackHQLocation',
                'Rotation': '$RoadRotation'}}]},
        {'op': 'set_var', 'var': 'bColumnReleased', 'value': True},
    ],
}

TRY_DRIVE = {
    'function': 'TryDriveColumnUp',
    'outputs': [{'name': 'bDone', 'type': 'bool'}],
    'body': [
        {'op': 'set_var', 'var': 'ReleaseAttempts', 'value': '=$ReleaseAttempts + 1'},
        {'op': 'call', 'func': 'Self.GetHasPlayerProfileLoaded', 'as': 'bDeckLoaded'},
        {'op': 'branch', 'condition': '$bDeckLoaded', 'then': [
            {'op': 'set_var', 'var': 'DeckSeenAttempts', 'value': '=$DeckSeenAttempts + 1'},
            # After a skip the deck may have spawned a moment ago and its units do not take orders yet: give them
            # DeckSettleAttempts more tries. The column shot (tour still running) comes long after the deck.
            {'op': 'branch', 'condition': '$bTourFinished', 'then': [
                {'op': 'branch', 'condition': '=$DeckSeenAttempts <= $DeckSettleAttempts', 'then': [
                    {'op': 'return', 'values': {'bDone': False}}]}]},
            {'op': 'call', 'func': 'Self.DriveColumnUp'},
            {'op': 'return', 'values': {'bDone': True}}]},
        # The deck has not spawned yet (a skip in the first seconds): wait, at most MaxReleaseAttempts tries.
        {'op': 'branch', 'condition': '=$ReleaseAttempts < $MaxReleaseAttempts', 'then': [
            {'op': 'return', 'values': {'bDone': False}}]},
        {'op': 'call', 'func': 'Self.DriveColumnUp'},
        {'op': 'return', 'values': {'bDone': True}},
    ],
}

FUNCTIONS = (TOUR, SPAWN_ESCORT, IS_COLUMN_SHOT, DRIVE, TRY_DRIVE)


def add_typed(port, name, types, tooltip):
    for variable_type in types:
        r = action(port, 'add_variable', {'blueprint_path': BP, 'variable_name': name, 'variable_type': variable_type,
                                          'instance_editable': True, 'category': 'OW|Column', 'tooltip': tooltip})
        print('var', name, variable_type, r.get('ok'), r.get('variable_type'), str(r.get('error', ''))[:160])
        if r.get('ok'):
            return True
    return False


def main():
    port = int(sys.argv[1])
    only = sys.argv[2:]
    if only:
        for spec in FUNCTIONS:
            if spec['function'] in only:
                r = action(port, 'synthesize_graph', dict(spec, blueprint=BP))
                print(spec['function'], r.get('ok'), str(r.get('error', ''))[:900].encode('ascii', 'replace').decode())
        c = action(port, 'compile_blueprint', {'blueprint_path': BP})
        print('compile', c.get('error_count'), str(c.get('errors', ''))[:900])
        return
    r = action(port, 'create_asset', {'asset_type': 'blueprint', 'name': NAME, 'save_path': BASE[:-1],
                                      'parent_class': '/Script/RTS_Survival.MissionBase'})
    print('create', r.get('ok'), str(r.get('error', ''))[:200])
    for item in VARIABLES:
        r = action(port, 'add_variable', dict(item, blueprint_path=BP))
        print('var', item['variable_name'], r.get('ok'), r.get('variable_type'), str(r.get('error', ''))[:160])
    for name, types, tooltip in ARRAY_VARIABLES:
        add_typed(port, name, types, tooltip)
    for name, variable_type, tooltip in STRUCT_VARIABLES:
        add_typed(port, name, [variable_type], tooltip)
    for name, line in (('B1Line', 'OderWarthe_B1_Deaf'), ('B2Line', 'OderWarthe_B2_Bursts'),
                       ('B3Line', 'OderWarthe_B3_FindIt')):
        r = action(port, 'set_blueprint_variable_default', {'blueprint_path': BP, 'variable_name': name,
                                                            'default_value': LINES + line + '.' + line})
        print('default', name, r.get('ok'), str(r.get('error', ''))[:200])
    for spec in FUNCTIONS:
        r = action(port, 'synthesize_graph', dict(spec, blueprint=BP))
        print(spec['function'], r.get('ok'), str(r.get('error', ''))[:900].encode('ascii', 'replace').decode())
    c = action(port, 'compile_blueprint', {'blueprint_path': BP})
    print('compile', c.get('error_count'), str(c.get('errors', ''))[:900])


if __name__ == '__main__':
    main()
