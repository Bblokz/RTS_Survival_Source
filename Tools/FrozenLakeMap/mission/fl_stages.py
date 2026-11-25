"""Frozen Lake (Ger_FrozenLake) mission stages: one UMissionBase Blueprint per stage.

Design: Source/Docs/Campaign/FrozenLakeMission.md ("Mission stages"). Rules: one Blueprint per stage, a flat Missions[]
array, the flow in the event graph, fixed steps as Blueprint functions, waits through ScheduleSingleCallback.

Main phases live in FrozenLake/Main (Mission_FL_<NN>_<Stage>, NN = order of play), bonus objectives in
FrozenLake/Bonus (Mission_FL_Bonus_<Name>). Every stage is a dict:
  name, folder, parent, plate (title, description, objective type), vars, cdo (python defaults set after compile),
  functions {name: (inputs, outputs, body)}, graph (a function returning an fl_graph.Graph).
Places are world cm on Ger_FrozenLake (+X north, +Y east; the layout's coordinates are the world's).
"""
from fl_graph import Graph, var

ROOT = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/FrozenLake/'

ANNOUNCER = 'GermanAnnouncer'
SCAVENGER = 'GermanScavengers'
FORGE_RADIO = 'SovietHazmat'


def vec(x, y, z=0.0):
    return f'(X={x:.1f},Y={y:.1f},Z={z:.1f})'


def v_place(name, x, y, tip=''):
    return {'variable_name': name, 'variable_type': 'vector', 'default_value': vec(x, y),
            'category': 'FrozenLake|Places', 'instance_editable': True, 'tooltip': tip}


def v_float(name, value, cat='FrozenLake|Places', tip=''):
    return {'variable_name': name, 'variable_type': 'float', 'default_value': str(float(value)), 'category': cat,
            'instance_editable': True, 'tooltip': tip}


def v_int(name, value, cat='FrozenLake|Timing', tip=''):
    return {'variable_name': name, 'variable_type': 'int', 'default_value': str(int(value)), 'category': cat,
            'instance_editable': True, 'tooltip': tip}


def v_name(name, value, cat='FrozenLake|Tags', tip=''):
    return {'variable_name': name, 'variable_type': 'name', 'default_value': value, 'category': cat,
            'instance_editable': True, 'tooltip': tip}


def v_line(name, tip=''):
    return {'variable_name': name, 'variable_type': 'object', 'sub_type': 'SoundBase', 'category': 'FrozenLake|Lines',
            'instance_editable': True, 'tooltip': tip or 'Not recorded yet: an empty line plays nothing.'}


def v_unit(name, unit_type, subtype, tip=''):
    return {'variable_name': name, 'variable_type': 'struct', 'sub_type': 'TrainingOption',
            'default_value': f'(UnitType={unit_type},SubtypeValue={subtype})', 'category': 'FrozenLake|Units',
            'instance_editable': True, 'tooltip': tip}


# ---- shared functions -------------------------------------------------------------------------------------------

PLAY_LINE = ([{'name': 'Line', 'type': 'object:SoundBase'}, {'name': 'Speaker', 'type': 'enum:ERTSPortraitTypes'}], [],
             [{'op': 'is_valid', 'value': '$Line', 'then': [
                 {'op': 'call', 'func': 'PlayPortrait', 'args': {'PortraitType': '$Speaker', 'VoiceLine': '$Line'}}]}])


def sphere_trigger(centre, radius, trigger_id, logic='OverlapPlayer', max_callbacks=1):
    """Body of a Create<Name>Trigger function: a sphere trigger area of <radius> cm round <centre>."""
    return ([], [], [
        {'op': 'make_struct', 'struct': 'Rotator', 'as': 'NoRotation', 'fields': {}},
        {'op': 'make_struct', 'struct': 'Vector', 'as': 'SphereScale',
         'fields': {'X': f'=${radius} / 32', 'Y': f'=${radius} / 32', 'Z': f'=${radius} / 32'}},
        {'op': 'call', 'func': 'CreateTriggerAreaSphere',
         'args': {'Location': f'${centre}', 'Rotation': '$NoRotation', 'Scale': '$SphereScale',
                  'TriggerOverlapLogic': logic, 'DelayBetweenCallbacks': 0.0, 'MaxCallbacks': max_callbacks,
                  'TriggerId': f'${trigger_id}'}}])


def track_tagged(tag_var, prefix, missing_title, missing_text, tracking='TrackOnActorsDestroyed'):
    """Body of a Track<Name> function: every actor with the tag, counted on the plate and the minimap.
    Returns bFound; with no tagged actor it writes a placeholder text on the plate instead."""
    return ([], [{'name': 'bFound', 'type': 'bool'}], [
        {'op': 'call', 'func': 'GameplayStatics.GetAllActorsWithTag',
         'args': {'WorldContextObject': '=GetMissionManagerChecked()', 'Tag': f'${tag_var}'}, 'as': 'Tagged'},
        {'op': 'array_length', 'array': '$Tagged.OutActors', 'as': 'TaggedCount'},
        {'op': 'branch', 'condition': '=$TaggedCount > 0',
         'then': [{'op': 'call', 'func': 'TrackActorsWithMiniMapProgress',
                   'args': {'TrackingType': tracking, 'ActorsToTrack': '$Tagged.OutActors',
                            'ValidityCheckInterval': 2.0, 'Prefix': prefix, 'Partitive': '/', 'Postfix': '',
                            'RichCountType': 'Text_SubTitle'}},
                  {'op': 'return', 'values': {'bFound': True}}],
         'else': [{'op': 'call', 'func': 'SetMissionObjectiveText',
                   'args': {'Title': {'op': 'format_text', 'format': missing_title},
                            'Description': {'op': 'format_text', 'format': missing_text}}},
                  {'op': 'return', 'values': {'bFound': False}}]}])


def placeholder(message):
    """Body of a step whose system is not built yet: an on-screen note, so a test shows where it would run."""
    return ([], [], [{'op': 'print', 'value': f'[FrozenLake] {message}'}])


def move_camera(location_var, seconds_var):
    return ([], [], [
        {'op': 'make_struct', 'struct': 'MovePlayerCamera', 'as': 'CameraMove',
         'fields': {'MoveToLocation': f'${location_var}', 'TimeToMove': f'${seconds_var}',
                    'TimeCameraInputDisabled': f'${seconds_var}'}},
        {'op': 'call', 'func': 'MoveCamera', 'args': {'CameraMove': '$CameraMove'}}])


def plate(title, description, objective='Major'):
    return {'title': title, 'description': description, 'objective': objective}


# ---- 01 Arrival -------------------------------------------------------------------------------------------------

def g_arrival():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('intro', 'fn.Self.ShowFrozenLakeLocationIntro', None)])
    g.end_row()
    g.row([('evIntro', 'ev.BP_OnLocationIntroFinished', None), ('tour', 'fn.Self.PlayLakeTour', None)])
    g.end_row()
    g.row([('evTour', 'ev.BP_OnCameraTourFinished', None), ('landed', 'fn.Self.PlayLine', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.line('landed', 'LandedLine', ANNOUNCER)
    g.end_row()
    g.comment('Location intro: Lake Vygozero, Karelia, 1976', ['evStart', 'intro'])
    g.comment('Camera tour over the frozen bay: the rig, the Svarog crater rim past the dam, back to the shore '
              '(shots in LakeTourShots)', ['evIntro', 'tour'])
    g.comment('Tour over: landed line, complete. KeepHQ and ReachIce start on this.',
              ['evTour', 'landed', 'landedGet', 'complete'])
    return g


ARRIVAL = {
    'name': 'Mission_FL_01_Arrival', 'folder': 'Main', 'parent': 'MissionBase',
    'plate': plate('Land on the shore of Vygozero',
                   'The Forge keeps a whole bay of Lake Vygozero frozen. Land on the south-west shore.'),
    'vars': [v_line('TourLine', 'Plays as the camera tour starts.'), v_line('LandedLine')],
    'array_vars': [{'variable_name': 'LakeTourShots', 'variable_type': 'struct', 'sub_type': 'MissionCameraTourShot',
                    'is_array': True, 'category': 'FrozenLake|Tour', 'instance_editable': True}],
    'cdo': {'LakeTourShots': '('
            # Shots 1 and 3 checked 2026-11-24 with captures (fl_map_build.shot_tour). Shot 1 runs up the black-ice
            # vein from the player's side to the rig; shot 3 runs from the reveal spot back to the landing shore,
            # the ice edge at the top of the frame. Each reveals its own area (rig garrison included).
            '(FocusStart=(X=-4000.0,Y=-6000.0,Z=0.0),FocusEnd=(X=0.0,Y=0.0,Z=0.0),YawStart=40.0,YawEnd=20.0,'
            'PitchStart=10.0,PitchEnd=20.0,ArmLengthStart=9000.0,ArmLengthEnd=9000.0,Seconds=7.0,BlendInSeconds=1.5,'
            'RevealRadius=10000.0,Title=INVTEXT("The Forge drill rig")),'
            # Checked 2026-11-24 against the built backdrop (fl_backdrop_build.py). Pitch is added to the -70 arm
            # (positive = flatter), so +26..+30 looks north over the dam; the long arm reaches the crater (x 460-780 m).
            # The fog of war copies its edge row past the map edge, so the reveal must cover the north edge
            # (27000 lifts y -329..+209 m of it; the Shield HQ centre is 9 m outside). Both foci are inside the 293 m camera
            # square (a focus outside it is refused, not clamped).
            '(FocusStart=(X=24500.0,Y=-7000.0,Z=0.0),FocusEnd=(X=27500.0,Y=-6000.0,Z=0.0),YawStart=-8.0,'
            'YawEnd=4.0,PitchStart=26.0,PitchEnd=30.0,ArmLengthStart=20000.0,ArmLengthEnd=24000.0,Seconds=7.0,'
            'BlendInSeconds=1.5,RevealRadius=27000.0,Title=INVTEXT("The dam and the Svarog crater")),'
            '(FocusStart=(X=-9769.0,Y=-10850.0,Z=0.0),FocusEnd=(X=-18500.0,Y=-18500.0,Z=180.0),YawStart=40.0,'
            'YawEnd=45.0,PitchStart=8.0,PitchEnd=18.0,ArmLengthStart=8000.0,ArmLengthEnd=10000.0,Seconds=6.0,'
            'BlendInSeconds=1.5,RevealRadius=9000.0,Title=INVTEXT("The landing shore")))'},
    'functions': {
        'ShowFrozenLakeLocationIntro': ([], [], [
            {'op': 'call', 'func': 'ShowLocationIntro',
             'args': {'Location': {'op': 'format_text', 'format': 'Lake Vygozero'},
                      'Date': {'op': 'format_text', 'format': '1976'},
                      'DriftAxis': 'WorldX', 'DriftDistance': 1500.0, 'DurationSeconds': 7.0, 'bPanCamera': False,
                      'ID': 0,
                      'LocationSubtitle': {'op': 'format_text', 'format': 'Karelia, the Forge bay'}}}]),
        'PlayLakeTour': ([], [], [
            {'op': 'make_struct', 'struct': 'MissionCameraTourSettings', 'as': 'TourSettings',
             'fields': {'bShowLetterbox': True, 'LetterboxHeightFraction': 0.1, 'ReturnSeconds': 1.75,
                        'StartSound': '$TourLine'}},
            {'op': 'call', 'func': 'PlayCameraTour',
             'args': {'Shots': '$LakeTourShots', 'Settings': '$TourSettings', 'ID': 1}}]),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_arrival,
}


# ---- 02 KeepHQ --------------------------------------------------------------------------------------------------

def g_keep_hq():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evWaves', 'ev.CustomEvent', {'custom_name': 'OnFirstWaveTime', 'no_chain': True}),
           ('line', 'fn.Self.PlayLine', None), ('waves', 'fn.Self.StartGroundWaves', None)])
    g.wait('sched', 'evWaves', 'FirstWaveDelaySeconds')
    g.line('line', 'FirstWaveLine', ANNOUNCER)
    g.end_row()
    g.comment('The HQ dying is defeat (UDontLoseHQMission). After FirstWaveDelaySeconds the Shield ground columns '
              'start: the wave spawners named in GroundWaveSpawnerIds (N, E, S).',
              ['evStart', 'sched', 'schedSecs', 'evWaves', 'line', 'lineGet', 'waves'])
    return g


KEEP_HQ = {
    'name': 'Mission_FL_02_KeepHQ', 'folder': 'Main', 'parent': 'DontLoseHQMission',
    'plate': plate('Keep the HQ alive', 'If the HQ falls, the Forge keeps its lake.'),
    'vars': [v_int('FirstWaveDelaySeconds', 240), v_line('FirstWaveLine', 'Shield columns are coming.')],
    'array_vars': [{'variable_name': 'GroundWaveSpawnerIds', 'variable_type': 'name', 'is_array': True,
                    'category': 'FrozenLake|Enemy', 'instance_editable': True,
                    'tooltip': 'SpawnerId of each placed AEnemyWaveSpawner (columns N, E, S).'}],
    'cdo': {'GroundWaveSpawnerIds': '("FL_Wave_N","FL_Wave_E","FL_Wave_S")'},
    'functions': {
        'StartGroundWaves': ([], [], [
            {'op': 'for_each', 'array': '$GroundWaveSpawnerIds', 'as': 'SpawnerId', 'body': [
                {'op': 'call', 'func': 'EnemyWaveSpawner.FindEnemyWaveSpawner',
                 'args': {'WorldContextObject': '=GetMissionManagerChecked()', 'SpawnerIdToFind': '$SpawnerId'},
                 'as': 'Spawner'},
                {'op': 'is_valid', 'value': '$Spawner', 'then': [
                    {'op': 'call', 'func': 'EnemyWaveSpawner.StartSpawner', 'target': '$Spawner'}],
                 'else': [{'op': 'print', 'value': '[FrozenLake] No wave spawner with this SpawnerId yet'}]}]}]),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_keep_hq,
}


# ---- 03 ReachIce ------------------------------------------------------------------------------------------------

def g_reach_ice():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('mk', 'fn.Self.CreateIceEdgeTrigger', None)])
    g.end_row()
    g.row([('evTrig', 'ev.BP_OnTriggerAreaCallback', None), ('br', 'k2.Branch', None),
           ('cam', 'fn.Self.ShowRevealCamera', None), ('line', 'fn.Self.PlayLine', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evSeen', 'ev.CustomEvent', {'custom_name': 'OnRevealSeen', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    extra = g.trigger_id_branch('evTrig', 'br', 'IceEdgeTriggerId')
    g.line('line', 'RevealLine', SCAVENGER)
    g.wait('sched', 'evSeen', 'RevealHoldSeconds')
    g.end_row()
    g.comment('Make the ice-edge circle below the landing shore', ['evStart', 'mk'])
    g.comment('A player unit reaches the ice: camera to the clear black ice over the glowing vein (the reveal), '
              'Scavenger line, hold, complete. ThinIce and FreeKingTiger start on this.',
              ['evTrig', 'br', 'cam', 'line', 'lineGet', 'sched', 'schedSecs', 'evSeen', 'complete'] + extra)
    return g


REACH_ICE = {
    'name': 'Mission_FL_03_ReachIce', 'folder': 'Main', 'parent': 'MissionBase',
    'plate': plate('Reach the ice edge', 'Take a unit down to the ice below the landing shore.'),
    'vars': [v_place('IceEdgeCentre', -12624, -13073), v_float('IceEdgeRadius', 3200),
             v_int('IceEdgeTriggerId', 1, 'FrozenLake|Places'),
             v_place('RevealLocation', -9769, -10850, 'Clear black ice over the vein.'),
             v_float('RevealCameraSeconds', 2.0, 'FrozenLake|Timing'), v_int('RevealHoldSeconds', 8),
             v_line('RevealLine', 'Scavenger: something is alive under the ice.')],
    'functions': {
        'CreateIceEdgeTrigger': sphere_trigger('IceEdgeCentre', 'IceEdgeRadius', 'IceEdgeTriggerId'),
        'ShowRevealCamera': move_camera('RevealLocation', 'RevealCameraSeconds'),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_reach_ice,
}


# ---- 04 ThinIce -------------------------------------------------------------------------------------------------

def g_thin_ice():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('cam', 'fn.Self.ShowLessonCamera', None),
           ('spawn', 'fn.Self.SpawnThinIceVictim', None),
           ('schedSink', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evSunk', 'ev.CustomEvent', {'custom_name': 'OnVictimSunk', 'no_chain': True}),
           ('teach', 'fn.Self.TeachThinIce', None),
           ('schedRead', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evRead', 'ev.CustomEvent', {'custom_name': 'OnLessonRead', 'no_chain': True}),
           ('grant', 'fn.Self.GrantPrepareIceLane', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.wait('schedSink', 'evSunk', 'SinkWaitSeconds')
    g.wait('schedRead', 'evRead', 'LessonReadSeconds')
    g.end_row()
    g.comment('Camera to the grey ice; a Shield heavy appears on it and the ice breaks under it (the ice subsystem '
              'sinks any class on ice too weak for it). Then the lesson line and the Thin Ice field manual, then '
              'the engineers get Prepare ice lane. DestroyPumps and RigGarrison start on this.',
              ['evStart', 'cam', 'spawn', 'schedSink', 'schedSinkSecs', 'evSunk', 'teach', 'schedRead',
               'schedReadSecs', 'evRead', 'grant', 'complete'])
    return g


THIN_ICE = {
    'name': 'Mission_FL_04_ThinIce', 'folder': 'Main', 'parent': 'MissionBase',
    'plate': plate('The ice will not hold them',
                   'Grey ice breaks under heavy tanks; black ice holds infantry only. Heavies cross on white ice.'),
    'vars': [v_place('VictimSpawnLocation', -8610, -4015, 'Grey ice in sight of the landing shore.'),
             v_place('VictimDriveTarget', -11000, -9000, 'Where the heavy tries to drive (toward the shore).'),
             v_unit('VictimTank', 'UNType_Tank', 92, 'The Shield heavy that sinks (IS-2).'),
             v_float('LessonCameraSeconds', 2.0, 'FrozenLake|Timing'),
             v_int('SinkWaitSeconds', 14, tip='Stress, warning and sinking take about 10 s.'),
             v_int('LessonReadSeconds', 6, tip='Until the field manual exists: time to read the line.'),
             v_line('LessonLine', 'Announcer: the ice gave way under it.'),
             v_line('LaneLine', 'Announcer: engineers can thicken a lane for heavies.')],
    'functions': {
        'ShowLessonCamera': move_camera('VictimSpawnLocation', 'LessonCameraSeconds'),
        'SpawnThinIceVictim': ([], [], [
            {'op': 'make_array', 'as': 'SpawnPoints', 'items': ['$VictimSpawnLocation']},
            {'op': 'make_array', 'as': 'Units', 'items': ['$VictimTank']},
            {'op': 'make_array', 'as': 'Route', 'items': ['$VictimDriveTarget']},
            {'op': 'call', 'func': 'CreateSingleAttackMoveDifficultyConditional',
             'args': {'MinimalDifficulty': 'NewToRTS', 'SpawnLocations': '$SpawnPoints',
                      'TrainingOptions': '$Units', 'WayPoints': '$Route'}}]),
        'TeachThinIce': ([], [], [
            {'op': 'call', 'func': 'PlayLine', 'args': {'Line': '$LessonLine', 'Speaker': ANNOUNCER}},
            {'op': 'print', 'value': '[FrozenLake] Thin Ice field manual: not built yet (FrozenLakeMission.md TODOs)'}]),
        'GrantPrepareIceLane': ([], [], [
            {'op': 'print', 'value': '[FrozenLake] Prepare ice lane ability: not built yet (FrozenLakeMission.md TODOs)'},
            {'op': 'call', 'func': 'PlayLine', 'args': {'Line': '$LaneLine', 'Speaker': ANNOUNCER}}]),
        'PlayLine': PLAY_LINE,
    },
    'function_order': ['PlayLine'],
    'graph': g_thin_ice,
}


# ---- 05 RigGarrison ---------------------------------------------------------------------------------------------

def g_rig_garrison():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('track', 'fn.Self.TrackRigGarrison', None),
           ('br', 'k2.Branch', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', {'chain_from_pin': 'else'}),
           ('evSkip', 'ev.CustomEvent', {'custom_name': 'OnNoGarrisonPlaced', 'no_chain': True}),
           ('skip', 'fn.MissionBase.OnMissionComplete', None)])
    g.wire('track.bFound', 'br.Condition')
    g.wait('sched', 'evSkip', 'PlaceholderSkipSeconds')
    g.end_row()
    g.row([('evDone', 'ev.BP_OnTrackingReachedMaxCount', None), ('line', 'fn.Self.PlayLine', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.line('line', 'GarrisonClearedLine', ANNOUNCER)
    g.end_row()
    g.comment('Count every actor tagged GarrisonTag (garrison + deck guns). None placed yet: placeholder text, then '
              'skip after PlaceholderSkipSeconds so the chain can be tested.',
              ['evStart', 'track', 'br', 'sched', 'schedSecs', 'evSkip', 'skip'])
    g.comment('All dead: line, complete. BoardRig starts on this.', ['evDone', 'line', 'lineGet', 'complete'])
    return g


RIG_GARRISON = {
    'name': 'Mission_FL_05_RigGarrison', 'folder': 'Main', 'parent': 'MissionBase',
    'plate': plate('Destroy the rig garrison', 'Kill the Forge garrison and the deck guns on the drill rig.'),
    'vars': [v_name('GarrisonTag', 'FL_RigGarrison', tip='Tag on every garrison unit and deck gun placed on the rig.'),
             v_int('PlaceholderSkipSeconds', 5), v_line('GarrisonClearedLine')],
    'cdo_bool': {'complete_mission_when_tracking_done': False},
    'functions': {
        'TrackRigGarrison': track_tagged('GarrisonTag', 'Rig garrison destroyed: ', 'Destroy the rig garrison',
                                         '(Placeholder: no actor is tagged FL_RigGarrison yet. Skipping.)'),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_rig_garrison,
}


# ---- 06 BoardRig ------------------------------------------------------------------------------------------------

def g_board_rig():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('mk', 'fn.Self.CreateRigDeckTrigger', None)])
    g.end_row()
    g.row([('evTrig', 'ev.BP_OnTriggerAreaCallback', None), ('br', 'k2.Branch', None),
           ('boarded', 'fn.Self.PlayLine', None), ('thaw', 'fn.Self.StartVeinThaw', None),
           ('forge', 'fn.Self.PlayLine', None), ('air', 'fn.Self.StartRigAirDrops', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    extra = g.trigger_id_branch('evTrig', 'br', 'RigDeckTriggerId')
    g.line('boarded', 'BoardedLine', ANNOUNCER)
    g.line('forge', 'ThawLine', FORGE_RADIO)
    g.end_row()
    g.comment('Make the deck circle on the rig', ['evStart', 'mk'])
    g.comment('A player unit on the deck: line; the drill stops sedating, so black ice spreads along the vein; '
              'Forge radio; cargo planes start landing behind the Shield HQ. HaulCore starts on this.',
              ['evTrig', 'br', 'boarded', 'boardedGet', 'thaw', 'forge', 'forgeGet', 'air', 'complete'] + extra)
    return g


BOARD_RIG = {
    'name': 'Mission_FL_06_BoardRig', 'folder': 'Main', 'parent': 'MissionBase',
    'plate': plate('Board the drill rig', 'Get any unit onto the rig deck.'),
    'vars': [v_place('RigDeckCentre', 0, 0), v_float('RigDeckRadius', 2300), v_int('RigDeckTriggerId', 1, 'FrozenLake|Places'),
             v_name('RigAirDropId', 'FL_RigAirDrop', 'FrozenLake|Enemy',
                    'AirDropId of the placed cargo-plane AEnemyAirDrop (Timing.bStartOnBeginPlay off, '
                    'OnlyWhileAlive on the radio mast).'),
             v_line('BoardedLine'), v_line('ThawLine', 'Forge radio: the drill has stopped, the vein wakes.')],
    'functions': {
        'CreateRigDeckTrigger': sphere_trigger('RigDeckCentre', 'RigDeckRadius', 'RigDeckTriggerId'),
        'StartVeinThaw': placeholder('Vein thaw: not built yet (UFrozenLakeIceSubsystem StartVeinThaw, TODOs)'),
        'StartRigAirDrops': ([], [], [
            {'op': 'call', 'func': 'EnemyAirDrop.FindEnemyAirDrop',
             'args': {'WorldContextObject': '=GetMissionManagerChecked()', 'AirDropIdToFind': '$RigAirDropId'},
             'as': 'AirDrop'},
            {'op': 'is_valid', 'value': '$AirDrop', 'then': [
                {'op': 'call', 'func': 'EnemyAirDrop.StartAirDrop', 'target': '$AirDrop'}],
             'else': [{'op': 'print', 'value': '[FrozenLake] No air drop named FL_RigAirDrop placed yet'}]}]),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_board_rig,
}


# ---- 07 HaulCore ------------------------------------------------------------------------------------------------

def g_haul_core():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('mk', 'fn.Self.CreateHomeTrigger', None),
           ('cutting', 'fn.Self.PlayLine', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evCut', 'ev.CustomEvent', {'custom_name': 'OnCoreCut', 'no_chain': True}),
           ('spawn', 'fn.Self.SpawnCoreCarrier', None)])
    g.line('cutting', 'CoreCuttingLine', FORGE_RADIO)
    g.wait('sched', 'evCut', 'CoreCutSeconds')
    g.end_row()
    g.row([('evSpawned', 'ev.BP_OnAsyncSpawnComplete', None), ('br', 'k2.Branch', None),
           ('watch', 'fn.Self.WatchCarrier', None), ('ready', 'fn.Self.PlayLine', None)])
    g.getters('br', [('brEq', 'fn.KismetMathLibrary.EqualEqual_IntInt', 'Condition', 'ReturnValue')], dx=-40)
    g.wire('evSpawned.ID', 'brEq.A')
    ex, ey = g.pos['brEq']
    g.place('brId', var('CarrierSpawnId'), ex - 190, ey + 40)
    g.wire('brId.CarrierSpawnId', 'brEq.B')
    g.wire('evSpawned.SpawnedActor', 'watch.SpawnedActor')
    g.line('ready', 'CarrierReadyLine', ANNOUNCER)
    g.end_row()
    g.row([('evDied', 'ev.BP_OnCallBackTankDies', None), ('isCarrier', 'fn.Self.IsCarrier', None),
           ('brDied', 'k2.Branch', None), ('lost', 'fn.Self.PlayLine', None),
           ('schedRecut', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evRecut', 'ev.CustomEvent', {'custom_name': 'OnCoreRecut', 'no_chain': True}),
           ('respawn', 'fn.Self.SpawnCoreCarrier', None)])
    g.wire('evDied.Tank', 'isCarrier.Actor')
    g.wire('isCarrier.bIsCarrier', 'brDied.Condition')
    g.line('lost', 'CarrierLostLine', ANNOUNCER)
    g.wait('schedRecut', 'evRecut', 'CoreCutSeconds')
    g.end_row()
    g.row([('evTrig', 'ev.BP_OnTriggerAreaCallback', None), ('isHome', 'fn.Self.IsCarrier', None),
           ('brHome', 'k2.Branch', None), ('home', 'fn.Self.PlayLine', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.wire('evTrig.OverlappingActor', 'isHome.Actor')
    g.wire('isHome.bIsCarrier', 'brHome.Condition')
    g.line('home', 'CoreHomeLine', ANNOUNCER)
    g.end_row()
    g.comment('Make the HQ circle; the rig cuts the core (CoreCutSeconds), then the carrier spawns at the rig',
              ['evStart', 'mk', 'cutting', 'cuttingGet', 'sched', 'schedSecs', 'evCut', 'spawn'])
    g.comment('The carrier is in: remember it, watch for its death (sinking kills it too), line',
              ['evSpawned', 'br', 'brEq', 'brId', 'watch', 'ready', 'readyGet'])
    g.comment('The carrier died or sank: line, the rig cuts a new core, a new carrier. This stage never fails.',
              ['evDied', 'isCarrier', 'brDied', 'lost', 'lostGet', 'schedRecut', 'schedRecutSecs', 'evRecut',
               'respawn'])
    g.comment('The carrier reaches the HQ circle: line, complete. DestroyShieldHQ starts on this.',
              ['evTrig', 'isHome', 'brHome', 'home', 'homeGet', 'complete'])
    return g


HAUL_CORE = {
    'name': 'Mission_FL_07_HaulCore', 'folder': 'Main', 'parent': 'MissionBase',
    'plate': plate('Bring the core home', 'A recovery tank carries the sedated core. Get it to your HQ.'),
    'vars': [v_place('HomeCentre', -20600, -20600, 'The player HQ shelf.'), v_float('HomeRadius', 4000),
             v_int('HomeTriggerId', 1, 'FrozenLake|Places'),
             v_place('CarrierSpawnLocation', 0, -2600, "The rig's west face, by the spur to the heavy ice road."),
             {'variable_name': 'CarrierSpawnRotation', 'variable_type': 'rotator',
              'default_value': '(Pitch=0.0,Yaw=-139.0,Roll=0.0)', 'category': 'FrozenLake|Places',
              'instance_editable': True},
             v_unit('CarrierUnit', 'UNType_Tank', 44,
                    'Placeholder: Panther D until the Bergepanther-style core carrier exists (heavy nav agent).'),
             v_int('CarrierSpawnId', 7, 'FrozenLake|Units'), v_int('CoreCutSeconds', 120),
             {'variable_name': 'Carrier', 'variable_type': 'object', 'sub_type': 'TankMaster',
              'category': 'FrozenLake|State'},
             v_line('CoreCuttingLine', 'Forge radio: the rig is cutting a core.'), v_line('CarrierReadyLine'),
             v_line('CarrierLostLine'), v_line('CoreHomeLine')],
    'functions': {
        'CreateHomeTrigger': sphere_trigger('HomeCentre', 'HomeRadius', 'HomeTriggerId', max_callbacks=-1),
        'SpawnCoreCarrier': ([], [], [
            {'op': 'call', 'func': 'AsyncSpawnActorAtLocation',
             'args': {'TrainingOption': '$CarrierUnit', 'ID': '$CarrierSpawnId',
                      'SpawnLocation': '$CarrierSpawnLocation', 'Rotation': '$CarrierSpawnRotation'}}]),
        'WatchCarrier': ([{'name': 'SpawnedActor', 'type': 'object:Actor'}], [], [
            {'op': 'cast', 'value': '$SpawnedActor', 'to': 'TankMaster', 'as': 'SpawnedTank', 'then': [
                {'op': 'set_var', 'var': 'Carrier', 'value': '$SpawnedTank'},
                {'op': 'call', 'func': 'RegisterCallbackOnTankDies', 'args': {'Tank': '$SpawnedTank'}}]}]),
        'IsCarrier': ([{'name': 'Actor', 'type': 'object:Actor'}], [{'name': 'bIsCarrier', 'type': 'bool'}], [
            {'op': 'return', 'values': {'bIsCarrier': '=IsValid($Carrier) && $Actor == $Carrier'}}]),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_haul_core,
}


# ---- 08 DestroyShieldHQ -----------------------------------------------------------------------------------------

def g_destroy_hq():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('track', 'fn.Self.TrackShieldHQ', None)])
    g.end_row()
    g.row([('evDone', 'ev.BP_OnTrackingReachedMaxCount', None), ('line', 'fn.Self.PlayLine', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDown', 'ev.CustomEvent', {'custom_name': 'OnShieldHQDown', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None),
           ('victory', 'fn.MissionManager.TriggerVictory', None)])
    g.line('line', 'ShieldHQDownLine', ANNOUNCER)
    g.wait('sched', 'evDown', 'VictoryDelaySeconds')
    g.getters('victory', [('mgr', 'fn.MissionBase.GetMissionManagerChecked', 'self', 'ReturnValue')])
    g.default('victory', 'bShowBackToMenu', True)
    g.end_row()
    g.comment('Count the Shield HQ (actors tagged ShieldHQTag; none placed yet shows a placeholder text)',
              ['evStart', 'track'])
    g.comment('The HQ is down: line, wait, complete and win. (Debrief core evidence + tech card: not built.)',
              ['evDone', 'line', 'lineGet', 'sched', 'schedSecs', 'evDown', 'complete', 'victory', 'mgr'])
    return g


DESTROY_HQ = {
    'name': 'Mission_FL_08_DestroyShieldHQ', 'folder': 'Main', 'parent': 'MissionBase',
    'plate': plate('Destroy the Shield HQ', 'The core is home. Break the Shield garrison on the north-east shore.'),
    'vars': [v_name('ShieldHQTag', 'FL_ShieldHQ', tip='Tag on the placed Shield HQ building.'),
             v_int('VictoryDelaySeconds', 5), v_line('ShieldHQDownLine')],
    'cdo_bool': {'complete_mission_when_tracking_done': False},
    'functions': {
        'TrackShieldHQ': track_tagged('ShieldHQTag', 'Shield HQ destroyed: ', 'Destroy the Shield HQ',
                                      '(Placeholder: no actor is tagged FL_ShieldHQ yet.)'),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_destroy_hq,
}


# ---- Bonus: FreeKingTiger ---------------------------------------------------------------------------------------

def g_free_king_tiger():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('reg', 'fn.Self.RegisterFrozenKingTiger', None)])
    g.end_row()
    g.row([('evScav', 'ev.BP_OnCallBackScavengableObjectScavenged', None), ('swap', 'fn.Self.SwapCollarToFreed', None),
           ('spawn', 'fn.Self.SpawnPlayerKingTiger', None), ('line', 'fn.Self.PlayLine', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.line('line', 'FreedLine', SCAVENGER)
    g.end_row()
    g.row([('evFlood', 'ev.CustomEvent', {'custom_name': 'OnZone2Flooded'}), ('sunk', 'fn.Self.PlayLine', None),
           ('fail', 'fn.MissionBase.OnMissionFailed', None)])
    g.line('sunk', 'SunkLine', SCAVENGER)
    g.end_row()
    g.comment('Watch the frozen King Tiger (the scavengeable tagged KingTigerTag; 180 s fixed)', ['evStart', 'reg'])
    g.comment('Scavengers cut it out: freed collar, the King Tiger is yours, line, complete',
              ['evScav', 'swap', 'spawn', 'line', 'lineGet', 'complete'])
    g.comment('Zone 2 flooded first (called by the pump flood once it exists): it sinks, the bonus fails',
              ['evFlood', 'sunk', 'sunkGet', 'fail'])
    return g


FREE_KING_TIGER = {
    'name': 'Mission_FL_Bonus_FreeKingTiger', 'folder': 'Bonus', 'parent': 'MissionBase',
    'plate': plate('Free the King Tiger',
                   'Scavengers can cut the frozen King Tiger out of the shore ice. Flooding zone 2 first sinks it.',
                   'Bonus'),
    'vars': [v_name('KingTigerTag', 'FL_FrozenKingTiger', tip='Tag on the frozen King Tiger scavengeable object.'),
             v_name('FrozenCollarTag', 'FL_KingTigerCollarFrozen', tip='Ice collar shown while it is frozen (C01).'),
             v_name('FreedCollarTag', 'FL_KingTigerCollarFreed', tip='Broken collar shown once it is freed (C02).'),
             v_unit('KingTigerUnit', 'UNType_Tank', 55, 'King Tiger.'),
             v_place('KingTigerLocation', -10187, -18377),
             {'variable_name': 'KingTigerRotation', 'variable_type': 'rotator',
              'default_value': '(Pitch=0.0,Yaw=45.0,Roll=0.0)', 'category': 'FrozenLake|Places',
              'instance_editable': True},
             v_int('KingTigerSpawnId', 55, 'FrozenLake|Units'), v_line('FreedLine'), v_line('SunkLine')],
    'functions': {
        'RegisterFrozenKingTiger': ([], [], [
            {'op': 'call', 'func': 'GameplayStatics.GetAllActorsWithTag',
             'args': {'WorldContextObject': '=GetMissionManagerChecked()', 'Tag': '$KingTigerTag'}, 'as': 'Tagged'},
            {'op': 'for_each', 'array': '$Tagged.OutActors', 'as': 'Found', 'body': [
                {'op': 'cast', 'value': '$Found', 'to': 'ScavengeableObject', 'as': 'Frozen', 'then': [
                    {'op': 'call', 'func': 'RegisterCallbackOnScavengableObjectScavenged',
                     'args': {'ScavengableObject': '$Frozen'}}]}]},
            {'op': 'array_length', 'array': '$Tagged.OutActors', 'as': 'TaggedCount'},
            {'op': 'branch', 'condition': '=$TaggedCount == 0', 'then': [
                {'op': 'call', 'func': 'SetMissionObjectiveText',
                 'args': {'Title': {'op': 'format_text', 'format': 'Free the King Tiger'},
                          'Description': {'op': 'format_text',
                                          'format': '(Placeholder: no scavengeable is tagged FL_FrozenKingTiger yet.)'}}}]}]),
        'SwapCollarToFreed': ([], [], [
            {'op': 'call', 'func': 'GameplayStatics.GetAllActorsWithTag',
             'args': {'WorldContextObject': '=GetMissionManagerChecked()', 'Tag': '$FrozenCollarTag'}, 'as': 'Frozen'},
            {'op': 'for_each', 'array': '$Frozen.OutActors', 'as': 'Collar', 'body': [
                {'op': 'call', 'func': 'Actor.K2_DestroyActor', 'target': '$Collar'}]},
            {'op': 'call', 'func': 'GameplayStatics.GetAllActorsWithTag',
             'args': {'WorldContextObject': '=GetMissionManagerChecked()', 'Tag': '$FreedCollarTag'}, 'as': 'Freed'},
            {'op': 'for_each', 'array': '$Freed.OutActors', 'as': 'Collar', 'body': [
                {'op': 'call', 'func': 'Actor.SetActorHiddenInGame', 'target': '$Collar',
                 'args': {'bNewHidden': False}},
                {'op': 'call', 'func': 'Actor.SetActorEnableCollision', 'target': '$Collar',
                 'args': {'bNewActorEnableCollision': True}}]}]),
        'SpawnPlayerKingTiger': ([], [], [
            {'op': 'call', 'func': 'AsyncSpawnActorAtLocation',
             'args': {'TrainingOption': '$KingTigerUnit', 'ID': '$KingTigerSpawnId',
                      'SpawnLocation': '$KingTigerLocation', 'Rotation': '$KingTigerRotation'}}]),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_free_king_tiger,
}


# ---- Bonus: DestroyPumps ----------------------------------------------------------------------------------------

def g_destroy_pumps():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('intro', 'fn.Self.PlayLine', None),
           ('track', 'fn.Self.TrackPumps', None), ('mk', 'fn.Self.CreateConvoyTrigger', None)])
    g.line('intro', 'PumpsIntroLine', FORGE_RADIO)
    g.end_row()
    g.row([('evTrig', 'ev.BP_OnTriggerAreaCallback', None), ('br', 'k2.Branch', None),
           ('intel', 'fn.Self.PlayLine', None)])
    extra = g.trigger_id_branch('evTrig', 'br', 'ConvoyTriggerId')
    g.line('intel', 'ConvoyIntelLine', SCAVENGER)
    g.end_row()
    g.row([('evDone', 'ev.BP_OnTrackingReachedMaxCount', None), ('line', 'fn.Self.PlayLine', None),
           ('supplies', 'fn.Self.DeliverPumpSupplies', None), ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.line('line', 'AllPumpsLine', ANNOUNCER)
    g.end_row()
    g.comment('Intercepted Forge radio explains the pumps; count the pumps (PumpTag); the burnt convoy circle',
              ['evStart', 'intro', 'introGet', 'track', 'mk'])
    g.comment('A player unit at the burnt convoy: pump-zone intel line', ['evTrig', 'br', 'intel', 'intelGet'] + extra)
    g.comment('All four pumps dead: line, Radixite/metal/vehicle parts supply drop, complete. (Each pump death also '
              'starts its own repair-crew air drop in the map: StartWhenDestroyed.)',
              ['evDone', 'line', 'lineGet', 'supplies', 'complete'])
    return g


DESTROY_PUMPS = {
    'name': 'Mission_FL_Bonus_DestroyPumps', 'folder': 'Bonus', 'parent': 'MissionBase',
    'plate': plate('Destroy the four cryo pumps',
                   'Each dead pump floods its zone for good. Only the rig roads stay frozen.', 'Bonus'),
    'vars': [v_name('PumpTag', 'FL_CryoPump', tip='Tag on the four cryo pump stations.'),
             v_place('ConvoyCentre', -21000, 19500, 'The burnt Forge convoy.'), v_float('ConvoyRadius', 2200),
             v_int('ConvoyTriggerId', 1, 'FrozenLake|Places'),
             v_line('PumpsIntroLine', 'Forge radio: the pumps keep the bay frozen.'),
             v_line('ConvoyIntelLine', 'Scavenger: the convoy carried pump-zone charts.'), v_line('AllPumpsLine')],
    'cdo_bool': {'complete_mission_when_tracking_done': False},
    'functions': {
        # The pumps are ADestructableEnvActors that collapse into a ruin (they stay in the world): count collapses.
        'TrackPumps': track_tagged('PumpTag', 'Cryo pumps destroyed: ', 'Destroy the four cryo pumps',
                                   '(Placeholder: no actor is tagged FL_CryoPump yet.)', 'TrackDestructablesCollapse'),
        'CreateConvoyTrigger': sphere_trigger('ConvoyCentre', 'ConvoyRadius', 'ConvoyTriggerId'),
        'DeliverPumpSupplies': placeholder('Pump bonus supply drop (Radixite, metal, vehicle parts): not built yet'),
        'PlayLine': PLAY_LINE,
    },
    'graph': g_destroy_pumps,
}


STAGES = [ARRIVAL, KEEP_HQ, REACH_ICE, THIN_ICE, RIG_GARRISON, BOARD_RIG, HAUL_CORE, DESTROY_HQ, FREE_KING_TIGER,
          DESTROY_PUMPS]


def path_of(stage):
    return ROOT + stage['folder'] + '/' + stage['name']
