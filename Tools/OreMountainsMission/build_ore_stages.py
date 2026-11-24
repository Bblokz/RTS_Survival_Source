"""Build the Ore Mountains (mission 2) stage Blueprints in our own editor over UECP.

Design: Source/Docs/Campaign/OreMountainsMission.md, "Mission structure" (Missions[] stage plan).
Rules (owner): one UMissionBase Blueprint per stage; the flow in the event graph; fixed steps in Blueprint functions;
waits through ScheduleSingleCallback; compact, readable graphs.

Usage (editor on the Ore map, UECP direct port in ORE_UECP_PORT, default 30000):
    python build_ore_stages.py              build every stage (existing stage assets are deleted and rebuilt)
    python build_ore_stages.py Opening ...  only these stages (names without the Mission_Ore_ prefix)

Shared pieces:
    Mission_Ore_Base   parent of the UMissionBase stages: PlayAdvisorLine, StartConvoyRoute, TrackTaggedTargets
    BPML_OreMission    macro library: WasConvoyStopped (Outcome -> bStopped; every outcome but Escaped)
"""
import json
import sys

import ue
import stage_lib as sl
from stage_lib import Graph, var

ORE = sl.ORE_BASE_CLASS
DONT_LOSE_HQ = '/Script/RTS_Survival.DontLoseHQMission'
COUNTERATTACK = '/Script/RTS_Survival.SovietCounterattackMission'
LINES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OreMountains/Briefing/'
CONVOY_FINISHED_PARAMS = [{'name': 'Route', 'type': 'object:EnemyConvoyRoute'},
                          {'name': 'Convoy', 'type': 'object:Convoy'},
                          {'name': 'Outcome', 'type': 'enum:EConvoyOutcome'}]


def sound(name):
    return f"/Script/Engine.SoundWave'{LINES}{name}.{name}'"


# ----------------------------------------------------------------------------------------------- shared parent

def build_base():
    path = ue.BASE + 'Mission_Ore_Base'
    sl.syn(path, 'PlayAdvisorLine', [
        {'op': 'is_valid', 'value': '$Line', 'then': [
            {'op': 'call', 'func': 'PlayPortrait', 'args': {'PortraitType': 'GermanAnnouncer', 'VoiceLine': '$Line'}}]}],
        inputs=[{'name': 'Line', 'type': 'object:SoundBase'}])
    sl.syn(path, 'StartConvoyRoute', [
        {'op': 'call', 'func': 'EnemyConvoyRoute.FindEnemyConvoyRoute',
         'args': {'WorldContextObject': '$self', 'RouteIdToFind': '$RouteId'}, 'as': 'Found'},
        {'op': 'is_valid', 'value': '$Found', 'then': [{'op': 'call', 'func': 'StartRoute', 'target': '$Found'}]},
        {'op': 'return', 'values': {'Route': '$Found'}}],
        inputs=[{'name': 'RouteId', 'type': 'name'}], outputs=[{'name': 'Route', 'type': 'object:EnemyConvoyRoute'}])
    sl.syn(path, 'TrackTaggedTargets', [
        {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
        {'op': 'call', 'func': 'GameplayStatics.GetAllActorsWithTag',
         'args': {'WorldContextObject': '$Manager', 'Tag': '$Tag'}, 'as': 'Tagged'},
        {'op': 'array_length', 'array': '$Tagged.OutActors', 'as': 'Count'},
        {'op': 'branch', 'condition': '=$Count > 0', 'then': [
            {'op': 'call', 'func': 'TrackActorsWithMiniMapProgress',
             'args': {'TrackingType': 'TrackOnActorsDestroyed', 'ActorsToTrack': '$Tagged.OutActors',
                      'ValidityCheckInterval': 2, 'Prefix': '$ProgressPrefix', 'Partitive': '/', 'Postfix': '',
                      'RichCountType': 'Text_SubTitle'}},
            {'op': 'return', 'values': {'bHasTargets': True}}],
         'else': [{'op': 'return', 'values': {'bHasTargets': False}}]}],
        inputs=[{'name': 'Tag', 'type': 'name'}, {'name': 'ProgressPrefix', 'type': 'string'}],
        outputs=[{'name': 'bHasTargets', 'type': 'bool'}])
    sl.ensure_vars(path, [
        ('ConvoyRoute', 'object:EnemyConvoyRoute', None, 'Ore|State', False,
         'The route this stage started (convoy stages); lookout warnings for other routes are ignored.'),
        ('BatteryId', 'name', 'ORE_Battery', 'Ore|Places', True, 'BatteryId of the placed AEnemyArtilleryBattery.'),
        ('FuelWarningLine', 'object:SoundBase', None, 'Ore|Lines|Lookout', True, 'Lookout: a fuel convoy is coming.'),
        ('AmmoWarningLine', 'object:SoundBase', None, 'Ore|Lines|Lookout', True, 'Lookout: an ammo convoy is coming.'),
        ('PartsWarningLine', 'object:SoundBase', None, 'Ore|Lines|Lookout', True, 'Lookout: a parts convoy is coming.'),
        ('ConcreteWarningLine', 'object:SoundBase', None, 'Ore|Lines|Lookout', True,
         'Lookout: a concrete convoy is coming.')])
    sl.compile_bp(path)
    sl.syn(path, 'GetOreBattery', [
        {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
        {'op': 'call', 'func': 'EnemyArtilleryBattery.FindEnemyArtilleryBattery',
         'args': {'WorldContextObject': '$Manager', 'BatteryIdToFind': '$BatteryId'}, 'as': 'Found'},
        {'op': 'return', 'values': {'Battery': '$Found'}}],
        outputs=[{'name': 'Battery', 'type': 'object:EnemyArtilleryBattery'}])
    sl.syn(path, 'GetTruckSubsystem', [
        {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
        {'op': 'call', 'func': 'SurrenderingTruckSubsystem.GetSurrenderingTruckSubsystem',
         'args': {'WorldContextObject': '$Manager'}, 'as': 'Found'},
        {'op': 'return', 'values': {'Subsystem': '$Found'}}],
        outputs=[{'name': 'Subsystem', 'type': 'object:SurrenderingTruckSubsystem'}])
    sl.syn(path, 'PlayCargoWarning', [
        {'op': 'switch', 'on': '$CargoType', 'cases': {
            'Fuel': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$FuelWarningLine'}}],
            'Ammo': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$AmmoWarningLine'}}],
            'VehicleParts': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$PartsWarningLine'}}],
            'Concrete': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$ConcreteWarningLine'}}]}}],
        inputs=[{'name': 'CargoType', 'type': 'enum:EConvoyCargoType'}])
    sl.compile_bp(path)
    sl.build_event_graph(path, base_event_graph())
    sl.compile_bp(path)
    sl.save(path)


def base_event_graph():
    """Shared by every convoy stage: listen to the lookout towers and warn about THIS stage's convoy."""
    g = Graph()
    g.row([('evListen', 'ev.CustomEvent', {'custom_name': 'ListenToLookoutTowers'}),
           ('towers', 'fn.LookoutTowerCaptureActor.GetAllLookoutTowers', None),
           ('loop', 'k2.ForEachLoop', None),
           ('bind', 'ev.DispatcherBind.LookoutTowerCaptureActor.OnConvoyWarning', {'no_chain': True})])
    g.getters('towers', [('mgrTowers', 'fn.MissionBase.GetMissionManagerChecked', 'WorldContextObject', 'ReturnValue')])
    g.wire('towers.ReturnValue', 'loop.Array')
    g.wire('loop.LoopBody', 'bind.execute')
    g.wire('loop.Array Element', 'bind.self')
    bx, by = g.pos['bind']
    g._place('bind_mk', 'k2.CreateEvent', bx - 20, by + 150)
    g.nodes[-1]['function'] = 'OnTowerConvoyWarning'
    g.wire('bind_mk.OutputDelegate', 'bind.Delegate')
    g.bottom = max(g.bottom, by + 280)
    g.end_row()
    g.row([('evWarn', 'ev.CustomEvent', {'custom_name': 'OnTowerConvoyWarning'}), ('br', 'k2.Branch', None),
           ('warn', 'fn.Self.PlayCargoWarning', None)])
    g.nodes[-3]['params'] = [{'name': 'Tower', 'type': 'object:LookoutTowerCaptureActor'},
                             {'name': 'ConvoyIndex', 'type': 'int'},
                             {'name': 'CargoType', 'type': 'enum:EConvoyCargoType'},
                             {'name': 'Route', 'type': 'object:EnemyConvoyRoute'},
                             {'name': 'Convoy', 'type': 'object:Convoy'}]
    g.getters('br', [('same', 'fn.KismetMathLibrary.EqualEqual_ObjectObject', 'Condition', 'ReturnValue')], dx=-40)
    sx, sy = g.pos['same']
    g._place('getRoute', var('ConvoyRoute'), sx + 10, sy + 75)
    g.wire('evWarn.Route', 'same.A')
    g.wire('getRoute.ConvoyRoute', 'same.B')
    g.wire('evWarn.CargoType', 'warn.CargoType')
    g.end_row()
    g.comment("Convoy stages call this at their start: bind every lookout tower's convoy warning",
              ['evListen', 'towers', 'mgrTowers', 'loop', 'bind', 'bind_mk'])
    g.comment("A held tower saw a convoy start: if it is this stage's convoy, the Advisor names its cargo",
              ['evWarn', 'br', 'same', 'getRoute', 'warn'])
    return g


def bind_named(g, bind_id, target_id_pin, event_name, extra_y=0):
    """Create Event naming `event_name`, under a bind node whose target is already wired."""
    bx, by = g.pos[bind_id]
    g._place(f'{bind_id}_mk', 'k2.CreateEvent', bx - 20, by + 150 + extra_y)
    g.nodes[-1]['function'] = event_name
    g.wire(f'{bind_id}_mk.OutputDelegate', f'{bind_id}.Delegate')
    if target_id_pin:
        g.wire(target_id_pin, f'{bind_id}.self')
    g.bottom = max(g.bottom, by + 280 + extra_y)


# ----------------------------------------------------------------------------------------------- graph pieces

def schedule_wait(g, sched_id, event_id, event_name, seconds_var):
    """Schedule node + the custom event it calls, side by side; the delay comes from a variable."""
    g.getters(sched_id, [(f'get_{sched_id}', var(seconds_var), 'DelaySeconds', seconds_var)])
    g.wire(f'{event_id}.OutputDelegate', f'{sched_id}.Callback')


def bind_route_finished(g, bind_id, route_var='ConvoyRoute'):
    """Getter for the route on the bind's target pin and the Create Event naming OnConvoyFinished, stacked below."""
    bx, by = g.pos[bind_id]
    g._place(f'{bind_id}_route', var(route_var), bx - 20, by + 150)
    g.wire(f'{bind_id}_route.{route_var}', f'{bind_id}.self')
    g._place(f'{bind_id}_mk', 'k2.CreateEvent', bx - 20, by + 215)
    g.nodes[-1]['function'] = 'OnConvoyFinished'
    g.wire(f'{bind_id}_mk.OutputDelegate', f'{bind_id}.Delegate')
    g.bottom = max(g.bottom, by + 330)


def convoy_end_row(g):
    """OnConvoyFinished -> Branch(WasConvoyStopped) -> stopped / escaped line -> complete (both arms meet at one node)."""
    g.row([('evFin', 'ev.CustomEvent', {'custom_name': 'OnConvoyFinished'}),
           ('sw', 'k2.Branch', None)])
    g.nodes[-2]['params'] = CONVOY_FINISHED_PARAMS
    g.getters('sw', [('stopped', 'mc.BPML_OreMission.WasConvoyStopped', 'Condition', 'bStopped')], dx=-40)
    g.wire('evFin.Outcome', 'stopped.Outcome')
    sx, sy = g.pos['sw']
    g.row([('playStopped', 'fn.Self.PlayAdvisorLine', None)], x=sx + 280, y=sy - 70)
    g.row([('playEscaped', 'fn.Self.PlayAdvisorLine', None)], x=sx + 280, y=sy + 135)
    g._place('getStopped', var('StoppedLine'), sx + 260, sy + 78)
    g._place('getEscaped', var('EscapedLine'), sx + 260, sy + 283)
    g.wire('getStopped.StoppedLine', 'playStopped.Line')
    g.wire('getEscaped.EscapedLine', 'playEscaped.Line')
    g.connections = [c for c in g.connections if c['to'] != 'playStopped.execute']
    g.wire('sw.then', 'playStopped.execute')
    g.wire('sw.else', 'playEscaped.execute')
    g.row([('complete', 'fn.MissionBase.OnMissionComplete', None)], x=sx + 640, y=sy + 40)
    g.wire('playStopped.then', 'complete.execute')
    g.wire('playEscaped.then', 'complete.execute')
    g.end_row()
    g.comment('The convoy is done, stopped (destroyed or captured) or escaped: a line for either, then complete. '
              'The stage always completes, so the next stage starts either way.',
              ['evFin', 'sw', 'stopped', 'playStopped', 'playEscaped', 'getStopped', 'getEscaped', 'complete'])


def convoy_vars(route_id):
    return [('RouteId', 'name', route_id, 'Ore|Convoy', True, 'RouteId of the placed AEnemyConvoyRoute this stage starts.'),
            ('StartLine', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor line when the convoy starts (empty = none).'),
            ('StoppedLine', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor line when the convoy was stopped.'),
            ('EscapedLine', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor line when the convoy escaped.')]


def start_convoy_nodes(first_id='play'):
    """PlayAdvisorLine(StartLine) -> StartConvoyRoute(RouteId) -> set ConvoyRoute -> Bind OnConvoyFinished."""
    return [(first_id, 'fn.Self.PlayAdvisorLine', None), ('start', 'fn.Self.StartConvoyRoute', None),
            ('setRoute', 'var.set.ConvoyRoute', None),
            ('bind', 'ev.DispatcherBind.EnemyConvoyRoute.OnConvoyFinished', None),
            ('listen', 'fn.Self.ListenToLookoutTowers', None)]


def wire_start_convoy(g, play_id='play'):
    g.getters(play_id, [(f'get_{play_id}', var('StartLine'), 'Line', 'StartLine')])
    g.getters('start', [('getId', var('RouteId'), 'RouteId', 'RouteId')])
    g.wire('start.Route', 'setRoute.ConvoyRoute')
    bind_route_finished(g, 'bind')


# ----------------------------------------------------------------------------------------------- objective intro tours
# 2026-11-24 owner: every major objective gets a short skippable intro tour (two shots, fog lifted along them).
# Speakers (owner pick E): the Advisor on Base B and the cases, an intercepted Soviet radio voice on the convoys and the
# counterattacks. The lines are written in a separate session; until they are set (IntroLine1/2 empty) the tours show
# captions only. A takeover stops a playing portrait, so the lines start once the tour is on screen (polled).
INTRO_TOUR_ID = 5
INTRO_LINE_POLL_SECONDS = 1  # ScheduleSingleCallback takes whole seconds


def _shot(start, end, title, subtitle):
    return {'start': start, 'end': end, 'title': title, 'subtitle': subtitle}


# speaker, 'column' (convoy stages: shot 1 follows the column as it sets off) or shot 1, then shot 2. Heights traced
# on the landscape 2026-11-24.
INTROS = {
    'Convoy2_Weighbridge': ('SovietMosin', ('Convoy 2', 'Leaving the convoy yard'),
                            _shot((600, 12000, -12), (88, 8107, 15), 'The weighbridge', 'The column stops here a minute')),
    'Convoy3_AmmoSplit': ('SovietMosin', ('Ammo convoy', 'Shells for the battery'),
                          _shot((9045, 22445, 746), (18760, 13400, 950), 'The battery road',
                                'The ammo trucks turn off here')),
    'Convoy4_Hunters': ('SovietMosin', ('Convoy 4', 'Infantry rides in the trucks'),
                        _shot((-1000, 6000, 0), (-2814, 1842, 52), 'Below our ridge', 'They dismount and climb')),
    'Convoy5_Cases': ('SovietMosin', ('Convoy 5', 'An armoured truck rides along'),
                      _shot((2500, 3000, 78), (3685, -2948, 364), 'The broken viaduct', 'One truck at a time')),
    'Convoy6_Heavy': ('SovietMosin', ('Heavy column', 'Tanks with the trucks'),
                      _shot((3685, -2948, 364), (7705, -6030, 862), 'The knoll road', 'Reinforcing Base B')),
    'LightCounterattack': ('SovietMosin',
                           _shot((2010, 22500, 0), (1000, 15000, -12), 'Soviet counterattack',
                                 'Trucks and light armour from the yard'),
                           _shot((1000, 15000, -12), (-2814, 1842, 52), 'The valley road', 'Then up to our ridge')),
    'HeavyCounterattack': ('SovietMosin',
                           _shot((2010, 22500, 0), (1000, 15000, -12), 'Heavy counterattack',
                                 'Tanks and hazmat engineers'),
                           _shot((1000, 15000, -12), (-2814, 1842, 52), 'The valley road', 'Straight for the HQ')),
    'DestroyBaseB': ('GermanAnnouncer',
                     _shot((7705, -6030, 862), (9500, -6000, 1599), 'Base B', 'The Wismut knoll'),
                     _shot((11335, -6260, 1590), (12500, -5000, 1600), 'Guns, radio mast, depot',
                           'Destroy them all')),
    'RecoverCases': ('GermanAnnouncer',
                     _shot((-1000, 6000, 0), (-2814, 1842, 52), 'The case truck', 'Capture it on the valley road'),
                     _shot((-21938, 8196, 1800), (-21938, 8196, 1800), 'The adit', 'It drives itself up here')),
}


def _vec(point):
    return '(X=%d,Y=%d,Z=%d)' % point


def intro_vars(short):
    speaker, first, second = INTROS[short]
    cat = 'Ore|Intro tour'
    items = [('IntroLine1', 'object:SoundBase', None, cat, True,
              'First line over the intro tour, played once the tour is on screen (empty = captions only).'),
             ('IntroLine2', 'object:SoundBase', None, cat, True, 'Second line, queued after the first.'),
             ('IntroPortrait', 'enum:ERTSPortraitTypes', speaker, cat, True,
              'Speaker of the intro lines (owner pick: Advisor on Base B and the cases, Soviet radio elsewhere).'),
             ('IntroVoiceWaitSeconds', 'int', '20', cat, True,
              'The tour waits (up to this long) for a playing voice line to end: a takeover would cut it.'),
             ('IntroVoiceWaited', 'int', '0', 'Ore|State', False),
             ('IntroShot1Seconds', 'float', '7', cat, True),
             ('IntroShot2Seconds', 'float', '7', cat, True, 'Lengthen both when the recorded lines run longer.'),
             ('IntroRevealRadius', 'float', '4500', cat, True, 'Fog lifted along the shots (cm).'),
             ('IntroTitle2', 'text', second['title'], cat, True), ('IntroSubtitle2', 'text', second['subtitle'], cat, True),
             ('IntroFocus2Start', 'struct:Vector', _vec(second['start']), cat, True),
             ('IntroFocus2End', 'struct:Vector', _vec(second['end']), cat, True)]
    if isinstance(first, dict):
        items += [('IntroTitle1', 'text', first['title'], cat, True),
                  ('IntroSubtitle1', 'text', first['subtitle'], cat, True),
                  ('IntroFocus1Start', 'struct:Vector', _vec(first['start']), cat, True),
                  ('IntroFocus1End', 'struct:Vector', _vec(first['end']), cat, True)]
    else:
        items += [('IntroConvoy', 'object:Convoy', None, 'Ore|State', False),
                  ('IntroTitle1', 'text', first[0], cat, True), ('IntroSubtitle1', 'text', first[1], cat, True),
                  ('IntroConvoyPan', 'float', '2500', cat, True,
                   'Shot 1 pans this far down the road from the column (cm).')]
    return items


def _second_shot():
    return {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': 'Shot2', 'fields': {
        'FocusStart': '$IntroFocus2Start', 'FocusEnd': '$IntroFocus2End', 'YawStart': 0, 'YawEnd': 15,
        'Seconds': '$IntroShot2Seconds', 'BlendInSeconds': 2.0, 'RevealRadius': '$IntroRevealRadius',
        'Title': '$IntroTitle2', 'Subtitle': '$IntroSubtitle2'}}


def _play_tour():
    return [{'op': 'make_array', 'as': 'Shots', 'items': ['$Shot1', '$Shot2']},
            {'op': 'make_struct', 'struct': 'MissionCameraTourSettings', 'as': 'Settings',
             'fields': {'bShowLetterbox': True, 'ReturnSeconds': 1.75, 'bAllowSkipping': True}},
            {'op': 'call', 'func': 'PlayCameraTour', 'args': {'Shots': '$Shots', 'Settings': '$Settings',
                                                              'ID': INTRO_TOUR_ID}}]


def add_intro_tour(path, short):
    """Vars, the tour function (PlayIntroTour, or PlayConvoyIntroTour(Convoy) on convoy stages) and the line helpers.
    The event graph adds intro_lines_row and calls the tour, then PlayIntroLinesOnScreen."""
    sl.ensure_vars(path, intro_vars(short))
    sl.compile_bp(path)
    _, first, _ = INTROS[short]
    if isinstance(first, dict):
        sl.syn(path, 'PlayIntroTour', [
            {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': 'Shot1', 'fields': {
                'FocusStart': '$IntroFocus1Start', 'FocusEnd': '$IntroFocus1End', 'YawStart': -20, 'YawEnd': 0,
                'Seconds': '$IntroShot1Seconds', 'BlendInSeconds': 1.5, 'RevealRadius': '$IntroRevealRadius',
                'Title': '$IntroTitle1', 'Subtitle': '$IntroSubtitle1'}},
            _second_shot()] + _play_tour())
    else:
        sl.syn(path, 'PlayConvoyIntroTour', [
            {'op': 'call', 'func': 'GetColumnCentre', 'target': '$Convoy', 'as': 'Centre'},
            {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': 'Shot1', 'fields': {
                'FocusStart': '$Centre', 'FocusEnd': '=$Centre + MakeVector(0, 0 - $IntroConvoyPan, 0)',
                'YawStart': -25, 'YawEnd': -5, 'Seconds': '$IntroShot1Seconds', 'BlendInSeconds': 1.5,
                'RevealRadius': '$IntroRevealRadius', 'Title': '$IntroTitle1', 'Subtitle': '$IntroSubtitle1'}},
            _second_shot()] + _play_tour(),
            inputs=[{'name': 'Convoy', 'type': 'object:Convoy'}])
    sl.syn(path, 'ShouldDelayIntroTour', [
        {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
        {'op': 'call', 'func': 'GameplayStatics.GetPlayerController',
         'args': {'WorldContextObject': '$Manager', 'PlayerIndex': 0}, 'as': 'PC'},
        {'op': 'cast', 'value': '$PC', 'to': 'CPPController', 'as': 'RTSPC', 'then': [
            {'op': 'call', 'func': 'CPPController.GetPlayerPortraitManager', 'target': '$RTSPC', 'as': 'Portraits'},
            {'op': 'is_valid', 'value': '$Portraits', 'then': [
                {'op': 'call', 'func': 'PlayerPortraitManager.GetIsPortraitPlaying', 'target': '$Portraits', 'as': 'Busy'},
                {'op': 'branch', 'condition': '=$Busy && $IntroVoiceWaited < $IntroVoiceWaitSeconds',
                 'then': [{'op': 'set_var', 'var': 'IntroVoiceWaited', 'value': '=$IntroVoiceWaited + 1'},
                          {'op': 'return', 'values': {'bWait': True}}]}]}]},
        {'op': 'return', 'values': {'bWait': False}}],
        outputs=[{'name': 'bWait', 'type': 'bool'}])
    sl.syn(path, 'PlayIntroLines', [
        {'op': 'is_valid', 'value': '$IntroLine1', 'then': [
            {'op': 'call', 'func': 'PlayPortrait', 'args': {'PortraitType': '$IntroPortrait', 'VoiceLine': '$IntroLine1'}}]},
        {'op': 'is_valid', 'value': '$IntroLine2', 'then': [
            {'op': 'call', 'func': 'PlayPortrait', 'args': {'PortraitType': '$IntroPortrait', 'VoiceLine': '$IntroLine2'}}]}])
    sl.syn(path, 'TryPlayIntroLines', [
        {'op': 'call', 'func': 'GetCameraTourShotIndex', 'as': 'Shot'},
        {'op': 'branch', 'condition': '=$Shot.ReturnValue >= 0', 'then': [
            {'op': 'call', 'func': 'PlayIntroLines'},
            {'op': 'return', 'values': {'bKeepWaiting': False}}],
         'else': [
            {'op': 'call', 'func': 'GetIsCameraTourPlaying', 'as': 'Playing'},
            {'op': 'branch', 'condition': '$Playing.ReturnValue', 'then': [
                {'op': 'return', 'values': {'bKeepWaiting': True}}],
             'else': [
                {'op': 'call', 'func': 'PlayIntroLines'},
                {'op': 'return', 'values': {'bKeepWaiting': False}}]}]}],
        outputs=[{'name': 'bKeepWaiting', 'type': 'bool'}])
    sl.compile_bp(path)


def intro_start_row(g, convoy):
    """StartIntroWhenQuiet: a voice line still plays -> check again in a second (at most IntroVoiceWaitSeconds);
    quiet -> the intro tour, then its lines once it is on screen."""
    g.row([('evQuiet', 'ev.CustomEvent', {'custom_name': 'StartIntroWhenQuiet'}),
           ('shouldWait', 'fn.Self.ShouldDelayIntroTour', None), ('brQuiet', 'k2.Branch', None),
           ('schedQuiet', 'fn.MissionBase.ScheduleSingleCallback', None)])
    g.wire('shouldWait.bWait', 'brQuiet.Condition')
    g.default('schedQuiet', 'DelaySeconds', INTRO_LINE_POLL_SECONDS)
    sx, sy = g.pos['schedQuiet']
    g._place('schedQuiet_mk', 'k2.CreateEvent', sx - 20, sy + 190)
    g.nodes[-1]['function'] = 'StartIntroWhenQuiet'
    g.wire('schedQuiet_mk.OutputDelegate', 'schedQuiet.Callback')
    tour = 'fn.Self.PlayConvoyIntroTour' if convoy else 'fn.Self.PlayIntroTour'
    g.row([('introTour', tour, None), ('introLines', 'fn.Self.PlayIntroLinesOnScreen', None)],
          x=sx + 400, y=sy)
    g.wire('brQuiet.else', 'introTour.execute')
    ids = ['evQuiet', 'shouldWait', 'brQuiet', 'schedQuiet', 'schedQuiet_mk', 'introTour', 'introLines']
    if convoy:
        tx, ty = g.pos['introTour']
        g._place('getIntroConvoy', var('IntroConvoy'), tx - 20, ty + 160)
        g.wire('getIntroConvoy.IntroConvoy', 'introTour.Convoy')
        ids.append('getIntroConvoy')
    g.bottom = max(g.bottom, sy + 300)
    g.end_row()
    g.comment('Intro tour once no voice line plays (a takeover would cut it): still busy, check again in a second',
              ids)


def intro_lines_row(g):
    """PlayIntroLinesOnScreen: the tour shows a shot -> lines; still waiting for the screen -> check again shortly;
    the tour is over without showing (screen busy 45 s) -> lines anyway."""
    g.row([('evIntroLines', 'ev.CustomEvent', {'custom_name': 'PlayIntroLinesOnScreen'}),
           ('tryLines', 'fn.Self.TryPlayIntroLines', None), ('brLines', 'k2.Branch', None),
           ('schedLines', 'fn.MissionBase.ScheduleSingleCallback', None)])
    g.wire('tryLines.bKeepWaiting', 'brLines.Condition')
    g.default('schedLines', 'DelaySeconds', INTRO_LINE_POLL_SECONDS)
    sx, sy = g.pos['schedLines']
    g._place('schedLines_mk', 'k2.CreateEvent', sx - 20, sy + 190)
    g.nodes[-1]['function'] = 'PlayIntroLinesOnScreen'
    g.wire('schedLines_mk.OutputDelegate', 'schedLines.Callback')
    g.bottom = max(g.bottom, sy + 300)
    g.end_row()
    g.comment('Intro lines once the intro tour is on screen (a takeover stops a portrait that already plays): not '
              'yet, check again shortly', ['evIntroLines', 'tryLines', 'brLines', 'schedLines', 'schedLines_mk'])


INTRO_BIND = ('bindStarted', 'ev.DispatcherBind.EnemyConvoyRoute.OnConvoyStarted', None)
INTRO_BIND_IDS = ['bindStarted', 'bindStarted_route', 'bindStarted_mk']


def short_of(path):
    return path.rsplit('Mission_Ore_', 1)[-1]


def convoy_intro_bind(g):
    """Bind OnConvoyStarted of this stage's route (the start row must already hold 'bindStarted')."""
    sx, sy = g.pos['bindStarted']
    g._place('bindStarted_route', var('ConvoyRoute'), sx - 20, sy + 150)
    bind_named(g, 'bindStarted', 'bindStarted_route.ConvoyRoute', 'OnConvoySetOff', extra_y=65)


def convoy_intro_rows(g):
    """The column has spawned (once): the intro tour follows it, then the lines."""
    g.row([('evSetOff', 'ev.CustomEvent', {'custom_name': 'OnConvoySetOff'}), ('onceIntro', 'k2.DoOnce', None),
           ('setIntroConvoy', 'var.set.IntroConvoy', None), ('introStart', 'fn.Self.StartIntroWhenQuiet', None)])
    g.nodes[-4]['params'] = [{'name': 'Route', 'type': 'object:EnemyConvoyRoute'},
                             {'name': 'Convoy', 'type': 'object:Convoy'}]
    g.connections = [c for c in g.connections if c['to'] not in ('onceIntro.execute', 'setIntroConvoy.execute')]
    g.wire('evSetOff.then', 'onceIntro.Execute')
    g.wire('onceIntro.Completed', 'setIntroConvoy.execute')
    g.wire('evSetOff.Convoy', 'setIntroConvoy.IntroConvoy')
    g.end_row()
    g.comment('The column sets off (once): keep it for the skippable intro tour (from the column to the place that '
              'matters, intercepted radio lines over it)', ['evSetOff', 'onceIntro', 'setIntroConvoy', 'introStart'])
    intro_start_row(g, convoy=True)
    intro_lines_row(g)


# ----------------------------------------------------------------------------------------------- stages

def opening(path):
    """2026-11-24 owner rework: a short panning location intro (no tour, no Advisor), then cargo planes fly in the HQ
    truck, the start vehicles (faction picks), three harvesters and the base trucks (profile units are off on
    RTS_PLayerStart); the two rifle squads come with the HQ truck. Once every plane has set its load down, briefing
    01-03, then complete. (Second pass the same day: start vehicles flown in early instead of spawned after the
    briefing.)"""
    sl.remove_own_vars(path, ['WeighbridgeLocation', 'BaseBLocation', 'BatteryLocation', 'AditLocation'])
    sl.remove_functions(path, ['PlayValleyTour'])
    sl.add_vars(path, [
        ('LocationText', 'text', 'Erzgebirge', 'Ore|Intro', True),
        ('DateText', 'text', '1976', 'Ore|Intro', True, 'Placeholder date; the owner decides.'),
        ('SubtitleText', 'text', 'Ore Mountains, the Czech border', 'Ore|Intro', True),
        ('IntroSeconds', 'float', '9', 'Ore|Intro', True, 'Length of the panning location intro.'),
        ('HQDropLocation', 'struct:Vector', '(X=-17085,Y=8040,Z=1800)', 'Ore|Drops', True,
         'Where the cargo plane sets the HQ truck down (the RTS_PLayerStart spot).'),
        ('HQDropRotation', 'struct:Rotator', '(Pitch=0,Yaw=0,Roll=0)', 'Ore|Drops', True),
        ('bBuildHQOnLanding', 'bool', 'false', 'Ore|Drops', True,
         'False: the HQ truck is only set down and the player deploys it. True: it builds where it lands.'),
        ('HarvesterSubtype', 'enum:ETankSubtype', 'Tank_PzI_Harvester', 'Ore|Drops', True),
        ('HarvesterForwardOffset', 'float', '2200', 'Ore|Drops', True,
         'Harvesters land this far north (in front, over the valley) of HQDropLocation.'),
        ('HarvesterSpacing', 'float', '900', 'Ore|Drops', True, 'Side-by-side spacing of the three harvesters.'),
        ('GammaDropLocation', 'struct:Vector', '(X=-19300,Y=4700,Z=1800)', 'Ore|Drops', True),
        ('RefineryDropLocation', 'struct:Vector', '(X=-14400,Y=10600,Z=1800)', 'Ore|Drops', True),
        ('BarracksDropLocation', 'struct:Vector', '(X=-17000,Y=4500,Z=1800)', 'Ore|Drops', True),
        ('DropIntervalSeconds', 'int', '3', 'Ore|Drops', True,
         'Seconds between the planes after the HQ plane (4 start vehicles, 3 harvesters, then the Gamma, refinery and '
         'barracks trucks).'),
        ('FollowUpDrops', 'int', '10', 'Ore|Drops', True, 'Planes after the HQ plane: 4 vehicles, 3 harvesters, 3 trucks.'),
        ('ExpectedDrops', 'int', '11', 'Ore|Drops', True, 'HQ + 4 start vehicles + 3 harvesters + 3 base trucks.'),
        ('SquadOffset', 'struct:Vector', '(X=-900,Y=0,Z=0)', 'Ore|Drops', True,
         'The two rifle squads appear this far from the HQ truck when it lands (they ride in with it).'),
        ('DropTimeoutSeconds', 'int', '150', 'Ore|Drops', True,
         'The briefing starts after this even if a plane never reported back.'),
        ('DropsSettledSeconds', 'int', '4', 'Ore|Drops', True, 'Pause between the last drop and briefing line 1.'),
        ('DropIndex', 'int', '0', 'Ore|State', False),
        ('LandedCount', 'int', '0', 'Ore|State', False),
        ('bHQStartGranted', 'bool', 'false', 'Ore|State', False),
        ('BriefingLine1', 'object:SoundBase', sound('OreMountains_Briefing_01_Weighbridge'), 'Ore|Lines', True),
        ('BriefingLine2', 'object:SoundBase', sound('OreMountains_Briefing_02_DiggingIn'), 'Ore|Lines', True),
        ('BriefingLine3', 'object:SoundBase', sound('OreMountains_Briefing_03_SpecialConvoy'), 'Ore|Lines', True),
        ('Line1Seconds', 'int', '11', 'Ore|Timing', True, 'Wait after briefing line 1 (its length, 10.5 s).'),
        ('Line2Seconds', 'int', '22', 'Ore|Timing', True, 'Wait after briefing line 2 (21.3 s).'),
        ('Line3Seconds', 'int', '12', 'Ore|Timing', True, 'Wait after briefing line 3 (11.1 s).'),
        ('StartForceLocation', 'struct:Vector', '(X=-21000,Y=6400,Z=1850)', 'Ore|Places', True,
         'Centre of the start force on the bench, west of the adit drop-off.'),
        ('StartForceRotation', 'struct:Rotator', '(Pitch=0,Yaw=0,Roll=0)', 'Ore|Places', True),
        ('EnemyWaveSupply', 'int', '40', 'Ore|Enemy', True,
         'Enemy wave supply for this map: patrols, raids, counterattacks and the director run 17-35 units at once.')])
    sl.compile_bp(path)
    sl.syn(path, 'ShowOreLocationIntro', [
        {'op': 'make_struct', 'struct': 'LocationIntroPanSettings', 'as': 'Pan',
         'fields': {'YawDegrees': 25, 'PitchDegrees': 0, 'bReturnToStartRotation': True, 'ReturnSeconds': 2.0}},
        {'op': 'call', 'func': 'ShowLocationIntro',
         'args': {'Location': '$LocationText', 'Date': '$DateText', 'DriftAxis': 'WorldY', 'DriftDistance': -1500,
                  'DurationSeconds': '$IntroSeconds', 'bPanCamera': True, 'PanSettings': '$Pan', 'ID': 0,
                  'LocationSubtitle': '$SubtitleText'}}])
    sl.syn(path, 'DeliverHQ', [
        {'op': 'call', 'func': 'DeliverPlayerNomadicByCargoPlane',
         'args': {'NomadicSubtype': 'Nomadic_GerHq', 'BuildLocation': '$HQDropLocation',
                  'BuildRotation': '$HQDropRotation', 'ID': 1, 'bBuildOnLanding': '$bBuildHQOnLanding'}}])
    zero_rot = '$HQDropRotation'
    # One small helper per kind of load; DeliverNextDrop only picks which one (readable graphs, owner rule).
    vehicle_offsets = ((0, 0), (0, 900), (700, 0), (700, 900))
    vehicle_picks = ('GetPlayerLightMediumTank_PZIII_J_Or_PZIV_F1', 'GetPlayerLightMediumTank_PZIII_J_Or_PZIV_F1',
                     'GetPlayerGerMediumTankDestroyer_StugIII_OrMarder', 'GetPlayerGerArmored_Transport_251_Or_250')
    vehicle_cases = {}
    for index, ((dx, dy), pick) in enumerate(zip(vehicle_offsets, vehicle_picks)):
        vehicle_cases[str(index)] = [
            {'op': 'call', 'func': 'RTSBlueprintFunctionLibrary.' + pick, 'args': {'WorldContext': '$Manager'},
             'as': f'Pick{index}'},
            {'op': 'call', 'func': 'DeliverPlayerTrainingOptionByCargoPlane',
             'args': {'Vehicle': f'$Pick{index}', 'DropLocation': f'=$StartForceLocation + MakeVector({dx}, {dy}, 0)',
                      'DropRotation': '$StartForceRotation', 'ID': 30 + index}}]
    sl.syn(path, 'DeliverStartVehicle', [
        {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
        {'op': 'switch', 'on': '$Index', 'cases': vehicle_cases}],
        inputs=[{'name': 'Index', 'type': 'int'}])
    sl.syn(path, 'DeliverHarvester', [
        {'op': 'call', 'func': 'DeliverPlayerVehicleByCargoPlane',
         'args': {'VehicleSubtype': '$HarvesterSubtype',
                  'DropLocation': '=$HQDropLocation + MakeVector($HarvesterForwardOffset, ($Index - 1) * $HarvesterSpacing, 0)',
                  'DropRotation': zero_rot, 'ID': '=$Index + 10'}}],
        inputs=[{'name': 'Index', 'type': 'int'}])
    trucks = (('Nomadic_GerGammaFacility', '$GammaDropLocation'), ('Nomadic_GerRefinery', '$RefineryDropLocation'),
              ('Nomadic_GerBarracks', '$BarracksDropLocation'))
    sl.syn(path, 'DeliverBaseTruck', [
        {'op': 'switch', 'on': '$Index', 'cases': {
            str(index): [{'op': 'call', 'func': 'DeliverPlayerNomadicByCargoPlane',
                          'args': {'NomadicSubtype': subtype, 'BuildLocation': location, 'BuildRotation': zero_rot,
                                   'ID': 21 + index, 'bBuildOnLanding': False}}]
            for index, (subtype, location) in enumerate(trucks)}}],
        inputs=[{'name': 'Index', 'type': 'int'}])
    sl.compile_bp(path)
    # Plane order after the HQ: 0-3 start vehicles, 4-6 harvesters, 7-9 base trucks.
    sl.syn(path, 'DeliverNextDrop', [
        {'op': 'branch', 'condition': '=$DropIndex < 4',
         'then': [{'op': 'call', 'func': 'DeliverStartVehicle', 'args': {'Index': '$DropIndex'}}],
         'else': [{'op': 'branch', 'condition': '=$DropIndex < 7',
                   'then': [{'op': 'call', 'func': 'DeliverHarvester', 'args': {'Index': '=$DropIndex - 4'}}],
                   'else': [{'op': 'call', 'func': 'DeliverBaseTruck', 'args': {'Index': '=$DropIndex - 7'}}]}]},
        {'op': 'set_var', 'var': 'DropIndex', 'value': '=$DropIndex + 1'}])
    # Two rifle squads ride in with the HQ truck: only infantry captures surrendering trucks (convoy 1 teaches it).
    body = []
    for side in (-1, 1):
        body.append({'op': 'make_struct', 'struct': 'TrainingOption', 'as': f'Squad{side + 1}',
                     'fields': {'UnitType': 'UNType_Squad', 'SubtypeValue': 2}})
        body.append({'op': 'call', 'func': 'AsyncSpawnActorAtLocation', 'args': {
            'TrainingOption': f'$Squad{side + 1}', 'ID': 0,
            'SpawnLocation': f'=$HQDropLocation + $SquadOffset + MakeVector(0, {side * 600}, 0)',
            'Rotation': '$StartForceRotation'}})
    sl.syn(path, 'SpawnStartForce', body)
    sl.syn(path, 'GrantHQStart', [
        {'op': 'branch', 'condition': '$bHQStartGranted', 'then': [{'op': 'return'}]},
        {'op': 'set_var', 'var': 'bHQStartGranted', 'value': True},
        {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
        {'op': 'call', 'func': 'GameplayStatics.GetPlayerController',
         'args': {'WorldContextObject': '$Manager', 'PlayerIndex': 0}, 'as': 'PC'},
        {'op': 'cast', 'value': '$PC', 'to': 'CPPController', 'as': 'RTSPC', 'then': [
            {'op': 'call', 'func': 'MissionOnHqSpawned', 'target': '$RTSPC'}]}])
    sl.compile_bp(path)
    sl.syn(path, 'CountLandedDrop', [
        {'op': 'branch', 'condition': '=$ID == 1', 'then': [{'op': 'call', 'func': 'GrantHQStart'},
                                                          {'op': 'call', 'func': 'SpawnStartForce'}]},
        {'op': 'set_var', 'var': 'LandedCount', 'value': '=$LandedCount + 1'},
        {'op': 'return', 'values': {'bAllLanded': '=$LandedCount >= $ExpectedDrops'}}],
        inputs=[{'name': 'ID', 'type': 'int'}], outputs=[{'name': 'bAllLanded', 'type': 'bool'}])
    sl.syn(path, 'SetUpEnemySupply', [
        {'op': 'call', 'func': 'GetEnemyControllerChecked', 'as': 'Enemy'},
        {'op': 'is_valid', 'value': '$Enemy', 'then': [
            {'op': 'call', 'func': 'SetEnemyWaveSupply', 'target': '$Enemy', 'args': {'NewSupply': '$EnemyWaveSupply'}}]}])
    sl.compile_bp(path)

    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('supply', 'fn.Self.SetUpEnemySupply', None),
           ('intro', 'fn.Self.ShowOreLocationIntro', None)])
    g.end_row()
    g.row([('evIntro', 'ev.BP_OnLocationIntroFinished', None), ('hq', 'fn.Self.DeliverHQ', None),
           ('schedTimeout', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('schedDrops', 'fn.MissionBase.ScheduleRepeatedCallback', None),
           ('evDropTick', 'ev.CustomEvent', {'custom_name': 'OnDropTick', 'no_chain': True}),
           ('nextDrop', 'fn.Self.DeliverNextDrop', None)])
    g.getters('schedTimeout', [('get_schedTimeout', var('DropTimeoutSeconds'), 'DelaySeconds', 'DropTimeoutSeconds')])
    g.getters('schedDrops', [('getInterval', var('DropIntervalSeconds'), 'IntervalSeconds', 'DropIntervalSeconds'),
                             ('getInitial', var('DropIntervalSeconds'), 'InitialDelaySeconds', 'DropIntervalSeconds')])
    g.getters('schedDrops', [('getFollowUps', var('FollowUpDrops'), 'TotalCalls', 'FollowUpDrops')])
    g.default('schedDrops', 'bFireBeforeFirstInterval', 'false')
    g.wire('evDropTick.OutputDelegate', 'schedDrops.Callback')
    g.end_row()
    g.row([('evTimeout', 'ev.CustomEvent', {'custom_name': 'OnDropTimeout'}),
           ('callAllA', 'fn.Self.OnAllDropsLanded', None)])
    g.wire('evTimeout.OutputDelegate', 'schedTimeout.Callback')
    g.end_row()
    for suffix, event in (('N', 'ev.BP_OnAirDeliveredNomadicLanded'), ('V', 'ev.BP_OnAirDeliveredVehicleLanded')):
        g.row([(f'evLanded{suffix}', event, None), (f'count{suffix}', 'fn.Self.CountLandedDrop', None),
               (f'br{suffix}', 'k2.Branch', None), (f'callAll{suffix}', 'fn.Self.OnAllDropsLanded', None)])
        g.wire(f'evLanded{suffix}.ID', f'count{suffix}.ID')
        g.wire(f'count{suffix}.bAllLanded', f'br{suffix}.Condition')
        g.connections = [c for c in g.connections if c['to'] != f'callAll{suffix}.execute']
        g.wire(f'br{suffix}.then', f'callAll{suffix}.execute')
        g.end_row()
    g.row([('evAll', 'ev.CustomEvent', {'custom_name': 'OnAllDropsLanded'}), ('once', 'k2.DoOnce', None),
           ('schedSettled', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evSettled', 'ev.CustomEvent', {'custom_name': 'OnDropsSettled', 'no_chain': True}),
           ('play1', 'fn.Self.PlayAdvisorLine', None),
           ('sched1', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evLine1', 'ev.CustomEvent', {'custom_name': 'OnLine1Done', 'no_chain': True}),
           ('play2', 'fn.Self.PlayAdvisorLine', None), ('sched2', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evLine2', 'ev.CustomEvent', {'custom_name': 'OnLine2Done', 'no_chain': True})])
    g.connections = [c for c in g.connections if c['to'] != 'once.execute' and c['from'] != 'once.then']
    g.wire('evAll.then', 'once.Execute')
    g.wire('once.Completed', 'schedSettled.execute')
    schedule_wait(g, 'schedSettled', 'evSettled', 'OnDropsSettled', 'DropsSettledSeconds')
    g.getters('play1', [('getL1', var('BriefingLine1'), 'Line', 'BriefingLine1')])
    schedule_wait(g, 'sched1', 'evLine1', 'OnLine1Done', 'Line1Seconds')
    g.getters('play2', [('getL2', var('BriefingLine2'), 'Line', 'BriefingLine2')])
    schedule_wait(g, 'sched2', 'evLine2', 'OnLine2Done', 'Line2Seconds')
    row3_bottom = g.bottom
    g.end_row()
    ex, _ = g.pos['evLine2']
    g.row([('play3', 'fn.Self.PlayAdvisorLine', None), ('sched3', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evLine3', 'ev.CustomEvent', {'custom_name': 'OnBriefingDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)],
          x=ex, y=row3_bottom + 60)
    g.wire('evLine2.then', 'play3.execute')
    g.getters('play3', [('getL3', var('BriefingLine3'), 'Line', 'BriefingLine3')])
    schedule_wait(g, 'sched3', 'evLine3', 'OnBriefingDone', 'Line3Seconds')
    g.end_row()
    g.comment('Enemy wave supply for this map, then the panning location intro (Erzgebirge, no Advisor)',
              ['evStart', 'supply', 'intro'])
    g.comment('Fly the base in: the HQ plane now, then every DropIntervalSeconds one more plane (4 start vehicles, '
              '3 harvesters in front of the HQ, then the Gamma, refinery and barracks trucks). Timeout: the briefing '
              'starts anyway',
              ['evIntro', 'hq', 'schedTimeout', 'get_schedTimeout', 'schedDrops', 'getInterval', 'getInitial',
               'getFollowUps', 'evDropTick', 'nextDrop'])
    g.comment('A plane never reported back in time: go on', ['evTimeout', 'callAllA'])
    g.comment('Each set-down load (or failed delivery) counts; the HQ landing grants the profile start (resources, '
              'cards) and brings the two rifle squads. All eleven down: go on', ['evLandedN', 'countN', 'brN', 'callAllN', 'evLandedV', 'countV',
                                                 'brV', 'callAllV'])
    g.comment('Once: a short pause, briefing 1 (weighbridge patrol), wait, briefing 2 (Shield digs in), wait',
              ['evAll', 'once', 'schedSettled', 'get_schedSettled', 'evSettled', 'play1', 'getL1', 'sched1',
               'get_sched1', 'evLine1', 'play2', 'getL2', 'sched2', 'get_sched2', 'evLine2'])
    g.comment('Briefing 3 (the special convoy), wait, complete (KeepHQ and Convoy 1 start on this)',
              ['play3', 'getL3', 'sched3', 'get_sched3', 'evLine3', 'complete'])
    return g


def keep_hq(path):
    return None


def convoy1(path):
    sl.add_vars(path, convoy_vars('ORE_Convoy1') + [
        ('AmbushHintLocation', 'struct:Vector', '(X=-2814,Y=1842,Z=52)', 'Ore|Places', True,
         'Ambush spot below the ridge, marked on the minimap.'),
        ('FirstCaptureLine', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor line on the first captured truck.'),
        ('TruckSubsystem', 'object:SurrenderingTruckSubsystem', None, 'Ore|State', False),
        # 2026-11-24 owner: a skippable cinematic shows the first convoy setting off; the Advisor talks over it.
        ('ConvoyApproachLine1', 'object:SoundBase', None, 'Ore|Lines|Approach', True,
         'Advisor during the convoy-approach cinematic (played in order, queued; empty = none).'),
        ('ConvoyApproachLine2', 'object:SoundBase', None, 'Ore|Lines|Approach', True),
        ('ConvoyApproachLine3', 'object:SoundBase', None, 'Ore|Lines|Approach', True),
        ('ApproachRevealRadius', 'float', '4500', 'Ore|Approach', True,
         'Fog lifted around each cinematic shot (cm) for the cinematic plus a few seconds.'),
        ('ApproachRoadPan', 'float', '2500', 'Ore|Approach', True,
         'Shot 1 pans this far west along the road from the column (cm).'),
        ('ApproachShot1Seconds', 'float', '8', 'Ore|Approach', True),
        ('ApproachShot2Seconds', 'float', '8.5', 'Ore|Approach', True,
         'Lengthened from 7 so the three approach lines (17.8 s) fit the tour (8 + 8.5 + 1.75 s return).')])
    sl.remove_functions(path, ['ShowCaptureFieldManual'])
    sl.compile_bp(path)
    sl.syn(path, 'ShowAmbushHint', [
        {'op': 'call', 'func': 'AddCustomMiniMapIcon',
         'args': {'IconId': 'ORE_AmbushHint', 'IconType': 'Warning', 'WorldLocation': '$AmbushHintLocation'}}])
    sl.syn(path, 'RemoveAmbushHint', [
        {'op': 'call', 'func': 'RemoveCustomMiniMapIcon', 'args': {'IconId': 'ORE_AmbushHint'}}])
    sl.syn(path, 'PlayConvoyApproachCinematic', [
        {'op': 'call', 'func': 'GetColumnCentre', 'target': '$Convoy', 'as': 'Centre'},
        {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': 'ShotColumn', 'fields': {
            'FocusStart': '$Centre',
            'FocusEnd': '=$Centre + MakeVector(0, 0 - $ApproachRoadPan, 0)',
            'YawStart': -25, 'YawEnd': -5, 'Seconds': '$ApproachShot1Seconds', 'BlendInSeconds': 1.5,
            'RevealRadius': '$ApproachRevealRadius', 'Title': 'Fuel convoy', 'Subtitle': 'Leaving the convoy yard'}},
        {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': 'ShotRoad', 'fields': {
            'FocusStart': '=($Centre + $AmbushHintLocation) * 0.5', 'FocusEnd': '$AmbushHintLocation',
            'YawStart': -5, 'YawEnd': 10, 'Seconds': '$ApproachShot2Seconds', 'BlendInSeconds': 2.0,
            'RevealRadius': '$ApproachRevealRadius', 'Title': 'The valley road', 'Subtitle': 'Below our ridge'}},
        {'op': 'make_array', 'as': 'Shots', 'items': ['$ShotColumn', '$ShotRoad']},
        {'op': 'make_struct', 'struct': 'MissionCameraTourSettings', 'as': 'Settings',
         'fields': {'bShowLetterbox': True, 'ReturnSeconds': 1.75, 'bAllowSkipping': True}},
        {'op': 'call', 'func': 'PlayCameraTour', 'args': {'Shots': '$Shots', 'Settings': '$Settings', 'ID': 1}},
        {'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$ConvoyApproachLine1'}},
        {'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$ConvoyApproachLine2'}},
        {'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$ConvoyApproachLine3'}}],
        inputs=[{'name': 'Convoy', 'type': 'object:Convoy'}])
    sl.compile_bp(path)
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('hint', 'fn.Self.ShowAmbushHint', None)]
          + start_convoy_nodes()[1:] + [('bindStarted', 'ev.DispatcherBind.EnemyConvoyRoute.OnConvoyStarted', None),
                                        ('bindCap', 'ev.DispatcherBind.EnemyConvoyRoute.OnConvoyTruckCaptured', None),
                                        ('listenCd', 'fn.Self.ListenForScuttleCountdown', None)])
    g.getters('start', [('getId', var('RouteId'), 'RouteId', 'RouteId')])
    g.wire('start.Route', 'setRoute.ConvoyRoute')
    bind_route_finished(g, 'bind')
    sx, sy = g.pos['bindStarted']
    g._place('bindStarted_route', var('ConvoyRoute'), sx - 20, sy + 150)
    bind_named(g, 'bindStarted', 'bindStarted_route.ConvoyRoute', 'OnFirstConvoyStarted', extra_y=65)
    bx, by = g.pos['bindCap']
    g._place('bindCap_route', var('ConvoyRoute'), bx - 20, by + 150)
    bind_named(g, 'bindCap', 'bindCap_route.ConvoyRoute', 'OnTruckCaptured', extra_y=65)
    g.end_row()
    g.row([('evConvoyStarted', 'ev.CustomEvent', {'custom_name': 'OnFirstConvoyStarted'}),
           ('onceStart', 'k2.DoOnce', None), ('cinematic', 'fn.Self.PlayConvoyApproachCinematic', None)])
    g.nodes[-3]['params'] = [{'name': 'Route', 'type': 'object:EnemyConvoyRoute'},
                             {'name': 'Convoy', 'type': 'object:Convoy'}]
    g.connections = [c for c in g.connections if c['to'] not in ('onceStart.execute', 'cinematic.execute')]
    g.wire('evConvoyStarted.then', 'onceStart.Execute')
    g.wire('onceStart.Completed', 'cinematic.execute')
    g.wire('evConvoyStarted.Convoy', 'cinematic.Convoy')
    g.end_row()
    g.row([('evTourDone', 'ev.BP_OnCameraTourFinished', None), ('play', 'fn.Self.PlayAdvisorLine', None)])
    g.getters('play', [('get_play', var('StartLine'), 'Line', 'StartLine')])
    g.end_row()
    g.row([('evListenCd', 'ev.CustomEvent', {'custom_name': 'ListenForScuttleCountdown'}),
           ('getSub', 'fn.Self.GetTruckSubsystem', None), ('setSub', 'var.set.TruckSubsystem', None),
           ('bindCd', 'ev.DispatcherBind.SurrenderingTruckSubsystem.OnScuttleCountdownStarted', None)])
    g.wire('getSub.Subsystem', 'setSub.TruckSubsystem')
    cx, cy = g.pos['bindCd']
    g._place('bindCd_sub', var('TruckSubsystem'), cx - 20, cy + 150)
    bind_named(g, 'bindCd', 'bindCd_sub.TruckSubsystem', 'OnFirstScuttleCountdown', extra_y=65)
    g.end_row()
    g.row([('evCd', 'ev.CustomEvent', {'custom_name': 'OnFirstScuttleCountdown'}), ('onceCd', 'k2.DoOnce', None),
           ('manual', 'fn.AsyncShowCapturingTrucksTakeover.ShowCapturingTrucksTakeover', None)])
    g.nodes[-3]['params'] = [{'name': 'Truck', 'type': 'object:SurrenderingCargoTruck'}]
    g.connections = [c for c in g.connections if c['to'] not in ('onceCd.execute', 'manual.execute')]
    g.wire('evCd.then', 'onceCd.Execute')
    g.wire('onceCd.Completed', 'manual.execute')
    g.end_row()
    g.row([('evCap', 'ev.CustomEvent', {'custom_name': 'OnTruckCaptured'}), ('once', 'k2.DoOnce', None),
           ('playCap', 'fn.Self.PlayAdvisorLine', None)])
    g.nodes[-3]['params'] = [{'name': 'Route', 'type': 'object:EnemyConvoyRoute'},
                             {'name': 'Convoy', 'type': 'object:Convoy'}, {'name': 'Truck', 'type': 'object:Actor'}]
    g.connections = [c for c in g.connections if c['to'] not in ('once.execute', 'playCap.execute')]
    g.wire('evCap.then', 'once.Execute')
    g.wire('once.Completed', 'playCap.execute')
    g.getters('playCap', [('getCapLine', var('FirstCaptureLine'), 'Line', 'FirstCaptureLine')])
    g.end_row()
    convoy_end_row(g)
    g.row([('evDone', 'ev.BP_OnMissionComplete', None), ('unhint', 'fn.Self.RemoveAmbushHint', None)])
    g.end_row()
    g.comment('The guided fuel run: mark the ambush spot at once (owner 2026-11-24: the player walks there during '
              'the 80 s before the convoy), start convoy 1 and listen for its end, for the column setting off, for '
              'captured trucks and for the first scuttle countdown',
              ['evStart', 'hint', 'start', 'getId', 'setRoute', 'bind', 'bind_route', 'bind_mk', 'bindStarted',
               'bindStarted_route', 'bindStarted_mk', 'listen', 'bindCap', 'bindCap_route', 'bindCap_mk', 'listenCd'])
    g.comment('The column has spawned (once): skippable cinematic of it setting off (fog lifted there), the Advisor '
              'talks over it', ['evConvoyStarted', 'onceStart', 'cinematic'])
    g.comment('Back to the player: Advisor, set your ambush at the marked spot',
              ['evTourDone', 'play', 'get_play'])
    g.comment('Listen for any convoy truck starting its scuttle countdown (its escort is gone)',
              ['evListenCd', 'getSub', 'setSub', 'bindCd', 'bindCd_sub', 'bindCd_mk'])
    g.comment('The first countdown (once): the Capturing trucks field manual explains the 12 s, the 20 m squad and the '
              '60 % rules', ['evCd', 'onceCd', 'manual'])
    g.comment('The first truck captured (once): the Advisor line', ['evCap', 'once', 'playCap', 'getCapLine'])
    g.comment('Stage done: remove the ambush mark', ['evDone', 'unhint'])
    return g


def simple_convoy(route_id, note):
    def build(path):
        sl.add_vars(path, convoy_vars(route_id))
        sl.compile_bp(path)
        add_intro_tour(path, short_of(path))
        g = Graph()
        g.row([('evStart', 'ev.BP_OnMissionStart', None)] + start_convoy_nodes() + [INTRO_BIND])
        wire_start_convoy(g)
        convoy_intro_bind(g)
        g.end_row()
        convoy_intro_rows(g)
        convoy_end_row(g)
        g.comment(note, ['evStart', 'play', 'get_play', 'start', 'getId', 'setRoute', 'bind', 'bind_route', 'bind_mk', 'listen']
                  + INTRO_BIND_IDS)
        return g
    return build


def convoy3(path):
    sl.add_vars(path, convoy_vars('ORE_Convoy3'))
    sl.compile_bp(path)
    add_intro_tour(path, short_of(path))
    sl.syn(path, 'NotifyBattery', [
        {'op': 'call', 'func': 'GetOreBattery', 'as': 'Battery'},
        {'op': 'is_valid', 'value': '$Battery.Battery', 'then': [
            {'op': 'call', 'func': 'NotifyAmmoTruckArrived', 'target': '$Battery.Battery', 'args': {'Truck': '$Truck'}}]}],
        inputs=[{'name': 'Truck', 'type': 'object:Actor'}])
    sl.compile_bp(path)
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None)] + start_convoy_nodes()
          + [('bindBranch', 'ev.DispatcherBind.EnemyConvoyRoute.OnBranchTruckArrived', None), INTRO_BIND])
    wire_start_convoy(g)
    convoy_intro_bind(g)
    bx, by = g.pos['bindBranch']
    g._place('bindBranch_route', var('ConvoyRoute'), bx - 20, by + 150)
    bind_named(g, 'bindBranch', 'bindBranch_route.ConvoyRoute', 'OnAmmoTruckAtBattery', extra_y=65)
    g.end_row()
    g.row([('evAmmo', 'ev.CustomEvent', {'custom_name': 'OnAmmoTruckAtBattery'}),
           ('notify', 'fn.Self.NotifyBattery', None)])
    g.nodes[-2]['params'] = [{'name': 'Route', 'type': 'object:EnemyConvoyRoute'}, {'name': 'Truck', 'type': 'object:Actor'},
                             {'name': 'Cargo', 'type': 'enum:EConvoyCargoType'}, {'name': 'ConvoyIndex', 'type': 'int'}]
    g.wire('evAmmo.Truck', 'notify.Truck')
    g.end_row()
    convoy_intro_rows(g)
    convoy_end_row(g)
    g.comment('Convoy 3 splits: its ammo trucks turn off to the battery (route Branch settings). Start it, wait for its '
              'end, and tell the battery about every ammo truck that arrives',
              ['evStart', 'play', 'get_play', 'start', 'getId', 'setRoute', 'bind', 'bind_route', 'bind_mk', 'listen',
               'bindBranch', 'bindBranch_route', 'bindBranch_mk'] + INTRO_BIND_IDS)
    g.comment('An ammo truck reached the battery: about 3 min supplied (the battery also detects it itself; each truck '
              'counts once)', ['evAmmo', 'notify'])
    return g


def battery(path):
    sl.add_vars(path, [('Battery', 'object:EnemyArtilleryBattery', None, 'Ore|State', False),
                       ('DoneLine', 'object:SoundBase', None, 'Ore|Lines', True),
                       ('CompleteDelaySeconds', 'int', '3', 'Ore|Timing', True)])
    sl.compile_bp(path)
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('find', 'fn.Self.GetOreBattery', None),
           ('setBattery', 'var.set.Battery', None), ('valid', 'k2.IsValid', None),
           ('bind', 'ev.DispatcherBind.EnemyArtilleryBattery.OnBatteryDestroyed', {'no_chain': True})])
    g.wire('find.Battery', 'setBattery.Battery')
    g.connections = [c for c in g.connections if c['to'] != 'valid.execute']
    g.wire('setBattery.then', 'valid.exec')
    g.getters('valid', [('getB1', var('Battery'), 'InputObject', 'Battery')])
    g.wire('valid.Is Valid', 'bind.execute')
    bx, by = g.pos['bind']
    g._place('getB2', var('Battery'), bx - 20, by + 150)
    bind_named(g, 'bind', 'getB2.Battery', 'OnBatteryDestroyed', extra_y=65)
    g.end_row()
    g.row([('evDead', 'ev.CustomEvent', {'custom_name': 'OnBatteryDestroyed'}), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnBatteryLineDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.nodes[-5]['params'] = [{'name': 'DestroyedBattery', 'type': 'object:EnemyArtilleryBattery'}]
    g.getters('play', [('getLine', var('DoneLine'), 'Line', 'DoneLine')])
    schedule_wait(g, 'sched', 'evDone', 'OnBatteryLineDone', 'CompleteDelaySeconds')
    g.end_row()
    g.comment('Find the battery (BatteryId) and listen for every pit dead or captured',
              ['evStart', 'find', 'setBattery', 'valid', 'getB1', 'bind', 'getB2', 'bind_mk'])
    g.comment('Battery silenced: line, wait, complete', ['evDead', 'play', 'getLine', 'sched', 'get_sched', 'evDone',
                                                       'complete'])
    return g


def timed_convoy(route_id, note, extra_vars=(), warning_fn=None):
    """Wait GapSeconds after the stage starts, warning line, then the convoy (convoys 4-6)."""
    def build(path):
        sl.add_vars(path, convoy_vars(route_id) + [
            ('GapSeconds', 'int', '240', 'Ore|Timing', True, 'Seconds from the previous convoy to this one.')]
            + list(extra_vars))
        sl.compile_bp(path)
        add_intro_tour(path, short_of(path))
        if warning_fn:
            warning_fn(path)
            sl.compile_bp(path)
        g = Graph()
        g.row([('evStart', 'ev.BP_OnMissionStart', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
               ('evGap', 'ev.CustomEvent', {'custom_name': 'OnGapOver', 'no_chain': True})]
              + ([('warn', 'fn.Self.PlayConvoyWarning', None)] if warning_fn else [])
              + start_convoy_nodes('play' if not warning_fn else 'playStart') + [INTRO_BIND])
        schedule_wait(g, 'sched', 'evGap', 'OnGapOver', 'GapSeconds')
        wire_start_convoy(g, 'play' if not warning_fn else 'playStart')
        convoy_intro_bind(g)
        g.end_row()
        convoy_intro_rows(g)
        convoy_end_row(g)
        ids = ['evStart', 'sched', 'get_sched', 'evGap', 'start', 'getId', 'setRoute', 'bind', 'bind_route', 'bind_mk', 'listen']
        ids += ['warn', 'playStart', 'get_playStart'] if warning_fn else ['play', 'get_play']
        g.comment(note, ids + INTRO_BIND_IDS)
        return g
    return build


def convoy5_warning(path):
    sl.syn(path, 'PlayConvoyWarning', [
        {'op': 'call', 'func': 'GetHasMissionClassCompleted', 'args': {'MissionClass': '$RadioOutpostStage'},
         'as': 'bMarked'},
        {'op': 'branch', 'condition': '$bMarked.ReturnValue',
         'then': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$SealedCargoLine'}}],
         'else': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$UnmarkedLine'}}]}])


def counterattack(note, escape_bonus=False):
    """Soviet counterattack stage (the C++ parent runs the waves). Light one: owner pick Q5, one unit stronger when
    convoy 2 escaped (an extra T-70 that joins the attack)."""
    def build(path):
        sl.add_vars(path, [('WarningLine', 'object:SoundBase', None, 'Ore|Lines', True,
                            'Advisor warning as the counterattack starts.')])
        if escape_bonus:
            sl.add_vars(path, [
                ('EscapeBonusRouteId', 'name', 'ORE_Convoy2', 'Ore|Escape bonus', True,
                 'If this route had a convoy escape, one extra unit joins the counterattack.'),
                ('EscapeBonusSpawn', 'struct:Vector', '(X=2010,Y=22500,Z=50)', 'Ore|Escape bonus', True),
                ('EscapeBonusDelaySeconds', 'float', '45', 'Ore|Escape bonus', True,
                 'Matches SecondsUntilSovietsArrive of the light counterattack settings.')])
        sl.compile_bp(path)
        if escape_bonus:
            sl.syn(path, 'AddEscapeBonusUnit', [
                {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
                {'op': 'call', 'func': 'EnemyConvoyRoute.FindEnemyConvoyRoute',
                 'args': {'WorldContextObject': '$Manager', 'RouteIdToFind': '$EscapeBonusRouteId'}, 'as': 'Route'},
                {'op': 'is_valid', 'value': '$Route', 'then': [
                    {'op': 'call', 'func': 'GetEscapedConvoyCount', 'target': '$Route', 'as': 'Escaped'},
                    {'op': 'branch', 'condition': '=$Escaped > 0', 'then': [
                        {'op': 'make_struct', 'struct': 'TrainingOption', 'as': 'Tank',
                         'fields': {'UnitType': 'UNType_Tank', 'SubtypeValue': 69}},
                        {'op': 'make_array', 'as': 'Options', 'items': ['$Tank']},
                        {'op': 'make_array', 'as': 'Spawns', 'items': ['$EscapeBonusSpawn']},
                        {'op': 'make_array', 'as': 'Waypoints', 'items': ['=MakeVector(-2800, 1842, 52)',
                                                                         '=MakeVector(-21000, 8000, 1800)']},
                        {'op': 'call', 'func': 'CreateSingleAttackMoveDifficultyConditional',
                         'args': {'MinimalDifficulty': 'NewToRTS', 'SpawnLocations': '$Spawns',
                                  'TrainingOptions': '$Options', 'WayPoints': '$Waypoints',
                                  'TimeTillWave': '$EscapeBonusDelaySeconds'}}]}]}])
            sl.compile_bp(path)
        add_intro_tour(path, short_of(path))
        g = Graph()
        g.row([('evStart', 'ev.BP_OnMissionStart', None)]
              + ([('bonus', 'fn.Self.AddEscapeBonusUnit', None)] if escape_bonus else [])
              + [('introStart', 'fn.Self.StartIntroWhenQuiet', None)])
        g.end_row()
        g.row([('evTourDone', 'ev.BP_OnCameraTourFinished', None), ('play', 'fn.MissionBase.PlayPortrait', None)])
        g.getters('play', [('getLine', var('WarningLine'), 'VoiceLine', 'WarningLine')])
        g.default('play', 'PortraitType', 'GermanAnnouncer')
        g.end_row()
        intro_start_row(g, convoy=False)
        intro_lines_row(g)
        g.comment(note + '; skippable intro tour of where they come from, the intercepted radio lines over it',
                  ['evStart', 'introStart'] + (['bonus'] if escape_bonus else []))
        g.comment('Back to the player: the Advisor warning', ['evTourDone', 'play', 'getLine'])
        return g
    return build


def tracked_targets(tag, prefix, note, extra=None):
    """Complete when every actor tagged `tag` is destroyed: line, wait, complete."""
    def build(path):
        sl.add_vars(path, [
            ('TargetTag', 'name', tag, 'Ore|Targets', True, 'Every actor with this tag must be destroyed.'),
            ('ProgressPrefix', 'string', prefix, 'Ore|Targets', True),
            ('DoneLine', 'object:SoundBase', None, 'Ore|Lines', True),
            ('CompleteDelaySeconds', 'int', '3', 'Ore|Timing', True)] + (extra.get('vars', []) if extra else []))
        sl.compile_bp(path)
        sl.set_defaults(path, {'complete_mission_when_tracking_done': 'False'})
        if extra and extra.get('functions'):
            extra['functions'](path)
            sl.compile_bp(path)
        g = Graph()
        g.row([('evStart', 'ev.BP_OnMissionStart', None), ('track', 'fn.Self.TrackTaggedTargets', None)])
        g.getters('track', [('getTag', var('TargetTag'), 'Tag', 'TargetTag'),
                            ('getPrefix', var('ProgressPrefix'), 'ProgressPrefix', 'ProgressPrefix')])
        g.end_row()
        done_nodes = [('evMax', 'ev.BP_OnTrackingReachedMaxCount', None)]
        done_nodes += extra['on_done'] if extra and extra.get('on_done') else []
        done_nodes += [('play', 'fn.Self.PlayAdvisorLine',
                        {'chain_from_pin': extra['play_after_pin']} if extra and extra.get('play_after_pin') else None),
                       ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
                       ('evDone', 'ev.CustomEvent', {'custom_name': 'OnTargetsDone', 'no_chain': True}),
                       ('complete', 'fn.MissionBase.OnMissionComplete', None)]
        g.row(done_nodes)
        g.getters('play', [('getLine', var('DoneLine'), 'Line', 'DoneLine')])
        schedule_wait(g, 'sched', 'evDone', 'OnTargetsDone', 'CompleteDelaySeconds')
        g.end_row()
        g.comment(f'Track every actor tagged {tag} on the minimap (with none tagged the plate stays up)',
                  ['evStart', 'track', 'getTag', 'getPrefix'])
        g.comment(note, [n[0] for n in done_nodes] + ['getLine', 'get_sched'])
        return g
    return build


def recover_cases(path):
    sl.add_vars(path, [('ObjectiveLine', 'object:SoundBase', None, 'Ore|Lines', True),
                       ('RecoveredLine', 'object:SoundBase', None, 'Ore|Lines', True),
                       ('CompleteDelaySeconds', 'int', '4', 'Ore|Timing', True),
                       ('TruckSubsystem', 'object:SurrenderingTruckSubsystem', None, 'Ore|State', False)])
    sl.compile_bp(path)
    add_intro_tour(path, short_of(path))
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('introStart', 'fn.Self.StartIntroWhenQuiet', None),
           ('getSub', 'fn.Self.GetTruckSubsystem', None), ('setSub', 'var.set.TruckSubsystem', None),
           ('bindDelivered', 'ev.DispatcherBind.SurrenderingTruckSubsystem.OnCasesDelivered', None),
           ('bindCache', 'ev.DispatcherBind.SurrenderingTruckSubsystem.OnSealedCacheScavenged', None)])
    g.wire('getSub.Subsystem', 'setSub.TruckSubsystem')
    for bind_id, event in (('bindDelivered', 'OnCasesDeliveredEvent'), ('bindCache', 'OnSealedCacheScavengedEvent')):
        bx, by = g.pos[bind_id]
        g._place(f'{bind_id}_sub', var('TruckSubsystem'), bx - 20, by + 150)
        bind_named(g, bind_id, f'{bind_id}_sub.TruckSubsystem', event, extra_y=65)
    g.end_row()
    g.row([('evTourDone', 'ev.BP_OnCameraTourFinished', None), ('play', 'fn.Self.PlayAdvisorLine', None)])
    g.getters('play', [('getLine', var('ObjectiveLine'), 'Line', 'ObjectiveLine')])
    g.end_row()
    intro_start_row(g, convoy=False)
    intro_lines_row(g)
    g.row([('evDelivered', 'ev.CustomEvent', {'custom_name': 'OnCasesDeliveredEvent'}),
           ('callRecovered1', 'fn.Self.OnCasesRecovered', None)])
    g.nodes[-2]['params'] = [{'name': 'CaseTruck', 'type': 'object:SurrenderingCaseTruck'}]
    dx, dy = g.pos['callRecovered1']
    g.row([('evCache', 'ev.CustomEvent', {'custom_name': 'OnSealedCacheScavengedEvent'}),
           ('callRecovered2', 'fn.Self.OnCasesRecovered', None)], x=dx + 400, y=dy)
    g.nodes[-2]['params'] = [{'name': 'SealedCache', 'type': 'object:Actor'}]
    g.end_row()
    g.row([('evRecovered', 'ev.CustomEvent', {'custom_name': 'OnCasesRecovered'}), ('once', 'k2.DoOnce', None),
           ('play2', 'fn.Self.PlayAdvisorLine', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnRecoveredLineDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.connections = [c for c in g.connections if c['to'] != 'once.execute' and c['from'] != 'once.then']
    g.wire('evRecovered.then', 'once.Execute')
    g.wire('once.Completed', 'play2.execute')
    g.getters('play2', [('getLine2', var('RecoveredLine'), 'Line', 'RecoveredLine')])
    schedule_wait(g, 'sched', 'evDone', 'OnRecoveredLineDone', 'CompleteDelaySeconds')
    g.end_row()
    g.comment('Recover the Berlin cases: skippable intro tour (valley road, adit) with the Advisor, then listen for the '
              'case truck reaching the adit or a sealed cache of the cases being scavenged',
              ['evStart', 'introStart', 'getSub', 'setSub', 'bindDelivered', 'bindDelivered_sub',
               'bindDelivered_mk', 'bindCache', 'bindCache_sub', 'bindCache_mk'])
    g.comment('Back to the player: the objective line', ['evTourDone', 'play', 'getLine'])
    g.comment('Either way the cases are ours', ['evDelivered', 'callRecovered1', 'evCache', 'callRecovered2'])
    g.comment('Once: line, wait, complete (the Ending stage waits for this)',
              ['evRecovered', 'once', 'play2', 'getLine2', 'sched', 'get_sched', 'evDone', 'complete'])
    return g


def destroy_base_b(path):
    """Tracked targets plus: Base B receives escaped cargo (escaped ammo goes on to the battery), and when it falls it
    drops the escaped Berlin cases as a sealed cache."""
    sl.add_vars(path, [
        ('TargetTag', 'name', 'ORE_BaseB_Target', 'Ore|Targets', True, 'Every actor with this tag must be destroyed.'),
        ('ProgressPrefix', 'string', 'Base B: ', 'Ore|Targets', True),
        ('DoneLine', 'object:SoundBase', None, 'Ore|Lines', True),
        ('CompleteDelaySeconds', 'int', '3', 'Ore|Timing', True),
        ('EscapedCasesTransform', 'struct:Transform', '(Translation=(X=12700,Y=-6000,Z=1600))', 'Ore|Places', True,
         'Where the sealed cache with escaped cases appears when Base B falls.'),
        ('PressureDirector', 'object:ConvoyPressureDirector', None, 'Ore|State', False)])
    sl.compile_bp(path)
    sl.set_defaults(path, {'complete_mission_when_tracking_done': 'False',
                           'EscapedCasesTransform': 'unreal.Transform(location=unreal.Vector(12700, -6000, 1600))'})
    sl.syn(path, 'DropEscapedCases', [
        {'op': 'call', 'func': 'GetTruckSubsystem', 'as': 'Sub'},
        {'op': 'is_valid', 'value': '$Sub.Subsystem', 'then': [
            {'op': 'call', 'func': 'GetHaveCasesEscaped', 'target': '$Sub.Subsystem', 'as': 'Escaped'},
            {'op': 'branch', 'condition': '$Escaped.ReturnValue', 'then': [
                {'op': 'call', 'func': 'SpawnEscapedCasesSealedCache', 'target': '$Sub.Subsystem',
                 'args': {'SpawnTransform': '$EscapedCasesTransform'}}]}]}])
    sl.syn(path, 'ForwardAmmoToBattery', [
        {'op': 'call', 'func': 'GetOreBattery', 'as': 'Battery'},
        {'op': 'is_valid', 'value': '$Battery.Battery', 'then': [
            {'op': 'call', 'func': 'AddSuppliedSeconds', 'target': '$Battery.Battery', 'args': {'Seconds': '$Seconds'}}]}],
        inputs=[{'name': 'Seconds', 'type': 'float'}])
    sl.compile_bp(path)
    add_intro_tour(path, short_of(path))
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('track', 'fn.Self.TrackTaggedTargets', None),
           ('findDir', 'fn.GameplayStatics.GetActorOfClass', None), ('setDir', 'var.set.PressureDirector', None),
           ('bindAmmo', 'ev.DispatcherBind.ConvoyPressureDirector.OnEscapedAmmoSeconds', None),
           ('introStart', 'fn.Self.StartIntroWhenQuiet', None)])
    g.getters('track', [('getTag', var('TargetTag'), 'Tag', 'TargetTag'),
                        ('getPrefix', var('ProgressPrefix'), 'ProgressPrefix', 'ProgressPrefix')])
    g.default('findDir', 'ActorClass', '/Script/RTS_Survival.ConvoyPressureDirector')
    g.getters('findDir', [('mgrDir', 'fn.MissionBase.GetMissionManagerChecked', 'WorldContextObject', 'ReturnValue')])
    g.wire('findDir.ReturnValue', 'setDir.PressureDirector')
    bx, by = g.pos['bindAmmo']
    g._place('bindAmmo_dir', var('PressureDirector'), bx - 20, by + 150)
    bind_named(g, 'bindAmmo', 'bindAmmo_dir.PressureDirector', 'OnEscapedAmmo', extra_y=65)
    g.end_row()
    g.row([('evAmmo', 'ev.CustomEvent', {'custom_name': 'OnEscapedAmmo'}), ('forward', 'fn.Self.ForwardAmmoToBattery', None)])
    g.nodes[-2]['params'] = [{'name': 'Seconds', 'type': 'float'}]
    g.wire('evAmmo.Seconds', 'forward.Seconds')
    g.end_row()
    intro_start_row(g, convoy=False)
    intro_lines_row(g)
    g.row([('evMax', 'ev.BP_OnTrackingReachedMaxCount', None), ('drop', 'fn.Self.DropEscapedCases', None),
           ('play', 'fn.Self.PlayAdvisorLine', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnTargetsDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('play', [('getLine', var('DoneLine'), 'Line', 'DoneLine')])
    schedule_wait(g, 'sched', 'evDone', 'OnTargetsDone', 'CompleteDelaySeconds')
    g.end_row()
    g.comment('Track every actor tagged ORE_BaseB_Target; find the convoy pressure director and listen for escaped '
              'ammo', ['evStart', 'track', 'getTag', 'getPrefix', 'findDir', 'mgrDir', 'setDir', 'bindAmmo',
                       'bindAmmo_dir', 'bindAmmo_mk'])
    g.comment('Skippable intro tour of the knoll with the Advisor (once no voice line plays)', ['introStart'])
    g.comment('Escaped ammo trucks: the battery stays supplied longer', ['evAmmo', 'forward'])
    g.comment('Base B destroyed: escaped cases drop as a sealed cache, line, wait, complete (Ending starts on this)',
              ['evMax', 'drop', 'play', 'getLine', 'sched', 'get_sched', 'evDone', 'complete'])
    return g


def ending(path):
    sl.add_vars(path, [('RecoverCasesStage', 'class:MissionBase', None, 'Ore|Stages', True),
                       ('BerlinLine', 'object:SoundBase', None, 'Ore|Lines', True,
                        'Advisor: the cases tie to the Forge survey at the Berlin crater (not recorded yet).'),
                       ('BerlinLineSeconds', 'int', '12', 'Ore|Timing', True)])
    sl.compile_bp(path)
    sl.syn(path, 'IsCasesStageEnding', [
        {'op': 'call', 'func': 'KismetMathLibrary.EqualEqual_ClassClass',
         'args': {'A': '$EndedMissionClass', 'B': '$RecoverCasesStage'}, 'as': 'IsCases'},
        {'op': 'return', 'values': {'bCasesRecovered': '=$bCompleted && $IsCases.ReturnValue'}}],
        inputs=[{'name': 'EndedMissionClass', 'type': 'class:MissionBase'}, {'name': 'bCompleted', 'type': 'bool'}],
        outputs=[{'name': 'bCasesRecovered', 'type': 'bool'}])
    sl.compile_bp(path)
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('br', 'k2.Branch', None),
           ('callEnd', 'fn.Self.OnReadyToEnd', None)])
    g.connections = [c for c in g.connections if c['to'] != 'callEnd.execute']
    g.wire('br.then', 'callEnd.execute')
    g.getters('br', [('hasCases', 'fn.MissionBase.GetHasMissionClassCompleted', 'Condition', 'ReturnValue')], dx=-40)
    hx, hy = g.pos['hasCases']
    g._place('getCasesStage', var('RecoverCasesStage'), hx - 230, hy + 40)
    g.wire('getCasesStage.RecoverCasesStage', 'hasCases.MissionClass')
    g.end_row()
    g.row([('evOther', 'ev.BP_OnOtherMissionEnded', None), ('isCases', 'fn.Self.IsCasesStageEnding', None),
           ('br2', 'k2.Branch', None), ('callEnd2', 'fn.Self.OnReadyToEnd', None)])
    g.wire('evOther.EndedMissionClass', 'isCases.EndedMissionClass')
    g.wire('evOther.bCompleted', 'isCases.bCompleted')
    g.wire('isCases.bCasesRecovered', 'br2.Condition')
    g.connections = [c for c in g.connections if c['to'] != 'callEnd2.execute']
    g.wire('br2.then', 'callEnd2.execute')
    g.end_row()
    g.row([('evEnd', 'ev.CustomEvent', {'custom_name': 'OnReadyToEnd'}), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evLineDone', 'ev.CustomEvent', {'custom_name': 'OnBerlinLineDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None), ('victory', 'fn.MissionManager.TriggerVictory', None)])
    g.getters('play', [('getLine', var('BerlinLine'), 'Line', 'BerlinLine')])
    schedule_wait(g, 'sched', 'evLineDone', 'OnBerlinLineDone', 'BerlinLineSeconds')
    g.getters('victory', [('getMgr', 'fn.MissionBase.GetMissionManagerChecked', 'self', 'ReturnValue')])
    g.default('victory', 'bShowBackToMenu', True)
    g.end_row()
    g.comment('Base B has fallen. Cases already at the adit? Then end now', ['evStart', 'br', 'callEnd', 'hasCases',
                                                                           'getCasesStage'])
    g.comment('Otherwise end when RecoverCases completes', ['evOther', 'isCases', 'br2', 'callEnd2'])
    g.comment('The ending: Advisor Berlin line, wait, complete, victory and debrief',
              ['evEnd', 'play', 'getLine', 'sched', 'get_sched', 'evLineDone', 'complete', 'victory', 'getMgr'])
    return g


def kv3_drop(path):
    """2026-11-24 owner: after convoy 3, Soviet cargo planes (can be shot down) land KV-3 heavy tanks at the west end of
    our ridge (placed AEnemyAirDrop ORE_KV3Drop; its DA_Ore_KV3AirDrop holds the delay and the count per difficulty:
    1 / 2 / 3 / 4). Each landed tank attack-moves along the bench to the base; the first landing gets a skippable
    cinematic with the Advisor; the stage completes when every landed KV-3 is destroyed (or none landed)."""
    sl.add_vars(path, [
        ('AirDropId', 'name', 'ORE_KV3Drop', 'Ore|KV3', True, 'AirDropId of the placed AEnemyAirDrop.'),
        ('AirDrop', 'object:EnemyAirDrop', None, 'Ore|State', False),
        ('HQAttackLocation', 'struct:Vector', '(X=-21000,Y=8000,Z=1800)', 'Ore|KV3', True,
         'The landed KV-3s attack-move here (ORE_HQ_AttackMarker, the base).'),
        ('KV3Line', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor over the landing cinematic.'),
        ('ShotDownLine', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor when every plane was shot down.'),
        ('DoneLine', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor when the last KV-3 is destroyed.'),
        ('LandedTitle', 'text', 'Destroy the KV-3s', 'Ore|KV3', True),
        ('LandedDescription', 'text', 'Prototype heavy tanks landed on the west end of our ridge. They are heading for '
         'the base.', 'Ore|KV3', True),
        ('CinematicDelaySeconds', 'int', '3', 'Ore|Timing', True, 'From the first KV-3 on the ground to the cinematic.'),
        ('CompleteDelaySeconds', 'int', '3', 'Ore|Timing', True),
        ('KV3RevealRadius', 'float', '4500', 'Ore|KV3', True, 'Fog lifted along the cinematic (cm).'),
        ('KV3FacingRotation', 'struct:Rotator', '(Pitch=0,Yaw=90,Roll=0)', 'Ore|KV3', True,
         'Facing when they arrive at the base (east along the bench).')])
    if 'LandedTanks' not in sl.own_var_names(path):
        sl.log('vars LandedTanks', ue.a('add_variable', blueprint_path=path, items=[{
            'variable_name': 'LandedTanks', 'variable_type': 'object:Actor', 'is_array': True, 'category': 'Ore|State',
            'instance_editable': False}]))
    sl.compile_bp(path)
    sl.set_defaults(path, {'complete_mission_when_tracking_done': 'False'})
    sl.syn(path, 'FindKV3AirDrop', [
        {'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Manager'},
        {'op': 'call', 'func': 'EnemyAirDrop.FindEnemyAirDrop',
         'args': {'WorldContextObject': '$Manager', 'AirDropIdToFind': '$AirDropId'}, 'as': 'Found'},
        {'op': 'set_var', 'var': 'AirDrop', 'value': '$Found'}])
    sl.syn(path, 'StartKV3AirDrop', [
        {'op': 'is_valid', 'value': '$AirDrop', 'then': [{'op': 'call', 'func': 'StartAirDrop', 'target': '$AirDrop'}]}])
    sl.syn(path, 'OrderKV3Attack', [
        {'op': 'array_add', 'array': '$LandedTanks', 'item': '$Tank'},
        {'op': 'call', 'func': 'Commands.AttackMoveToLocation', 'target': '$Tank',
         'args': {'Location': '$HQAttackLocation', 'bSetUnitToIdle': True,
                  'FinishedMovementRotation': '$KV3FacingRotation'}}],
        inputs=[{'name': 'Tank', 'type': 'object:Actor'}])
    sl.syn(path, 'PlayKV3Cinematic', [
        {'op': 'call', 'func': 'Actor.K2_GetActorLocation', 'target': '$AirDrop', 'as': 'Landing'},
        {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': 'ShotLanding', 'fields': {
            'FocusStart': '$Landing', 'FocusEnd': '$Landing', 'YawStart': -30, 'YawEnd': 0, 'Seconds': 6,
            'BlendInSeconds': 1.5, 'RevealRadius': '$KV3RevealRadius', 'Title': 'KV-3 heavy tanks',
            'Subtitle': 'West end of our ridge'}},
        {'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': 'ShotBench', 'fields': {
            'FocusStart': '=($Landing + $HQAttackLocation) * 0.5', 'FocusEnd': '$HQAttackLocation', 'YawStart': 0,
            'YawEnd': 15, 'Seconds': 5, 'BlendInSeconds': 1.5, 'RevealRadius': '$KV3RevealRadius',
            'Title': 'Along the bench', 'Subtitle': 'Straight for the base'}},
        {'op': 'make_array', 'as': 'Shots', 'items': ['$ShotLanding', '$ShotBench']},
        {'op': 'make_struct', 'struct': 'MissionCameraTourSettings', 'as': 'Settings',
         'fields': {'bShowLetterbox': True, 'ReturnSeconds': 1.75, 'bAllowSkipping': True}},
        {'op': 'call', 'func': 'PlayCameraTour', 'args': {'Shots': '$Shots', 'Settings': '$Settings', 'ID': 2}},
        {'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$KV3Line'}},
        {'op': 'call', 'func': 'SetMissionObjectiveText',
         'args': {'Title': '$LandedTitle', 'Description': '$LandedDescription'}}])
    sl.syn(path, 'TrackLandedTanks', [
        {'op': 'array_length', 'array': '$LandedTanks', 'as': 'Count'},
        {'op': 'branch', 'condition': '=$Count > 0', 'then': [
            {'op': 'call', 'func': 'TrackActorsWithMiniMapProgress',
             'args': {'TrackingType': 'TrackOnActorsDestroyed', 'ActorsToTrack': '$LandedTanks',
                      'ValidityCheckInterval': 2, 'Prefix': 'KV-3: ', 'Partitive': '/', 'Postfix': '',
                      'RichCountType': 'Text_SubTitle'}},
            {'op': 'return', 'values': {'bHasTanks': True}}],
         'else': [{'op': 'return', 'values': {'bHasTanks': False}}]}],
        outputs=[{'name': 'bHasTanks', 'type': 'bool'}])
    sl.compile_bp(path)

    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('find', 'fn.Self.FindKV3AirDrop', None),
           ('bindLanded', 'ev.DispatcherBind.EnemyAirDrop.OnUnitLanded', None),
           ('bindFinished', 'ev.DispatcherBind.EnemyAirDrop.OnDropFinished', None),
           ('startDrop', 'fn.Self.StartKV3AirDrop', None)])
    for bind_id, event in (('bindLanded', 'OnKV3Landed'), ('bindFinished', 'OnKV3DropFinished')):
        bx, by = g.pos[bind_id]
        g._place(f'{bind_id}_drop', var('AirDrop'), bx - 20, by + 150)
        bind_named(g, bind_id, f'{bind_id}_drop.AirDrop', event, extra_y=65)
    g.end_row()
    g.row([('evLanded', 'ev.CustomEvent', {'custom_name': 'OnKV3Landed'}), ('order', 'fn.Self.OrderKV3Attack', None),
           ('once', 'k2.DoOnce', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evShow', 'ev.CustomEvent', {'custom_name': 'OnShowKV3s', 'no_chain': True}),
           ('cinematic', 'fn.Self.PlayKV3Cinematic', None)])
    g.nodes[-6]['params'] = [{'name': 'AirDrop', 'type': 'object:EnemyAirDrop'}, {'name': 'Unit', 'type': 'object:Actor'}]
    g.wire('evLanded.Unit', 'order.Tank')
    g.connections = [c for c in g.connections if c['to'] != 'once.execute' and c['from'] != 'once.then']
    g.wire('order.then', 'once.Execute')
    g.wire('once.Completed', 'sched.execute')
    schedule_wait(g, 'sched', 'evShow', 'OnShowKV3s', 'CinematicDelaySeconds')
    g.end_row()
    g.row([('evFinished', 'ev.CustomEvent', {'custom_name': 'OnKV3DropFinished'}),
           ('track', 'fn.Self.TrackLandedTanks', None), ('br', 'k2.Branch', None),
           ('playShot', 'fn.Self.PlayAdvisorLine', None), ('completeShot', 'fn.MissionBase.OnMissionComplete', None)])
    g.nodes[-5]['params'] = [{'name': 'AirDrop', 'type': 'object:EnemyAirDrop'}, {'name': 'DropNumber', 'type': 'int'}]
    g.wire('track.bHasTanks', 'br.Condition')
    g.connections = [c for c in g.connections if c['to'] != 'playShot.execute']
    g.wire('br.else', 'playShot.execute')
    g.getters('playShot', [('getShotLine', var('ShotDownLine'), 'Line', 'ShotDownLine')])
    g.end_row()
    g.row([('evMax', 'ev.BP_OnTrackingReachedMaxCount', None), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('schedDone', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnKV3sDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('play', [('getLine', var('DoneLine'), 'Line', 'DoneLine')])
    schedule_wait(g, 'schedDone', 'evDone', 'OnKV3sDone', 'CompleteDelaySeconds')
    g.end_row()
    g.comment('Find the KV-3 air drop (ORE_KV3Drop), listen for each landed unit and for the drop being over, start it '
              '(its DA_Ore_KV3AirDrop delay runs from here)',
              ['evStart', 'find', 'bindLanded', 'bindLanded_drop', 'bindLanded_mk', 'bindFinished', 'bindFinished_drop',
               'bindFinished_mk', 'startDrop'])
    g.comment('Each landed KV-3 attack-moves to the base; the first one (once, after a short wait): the skippable '
              'cinematic with the Advisor, and the plate turns into Destroy the KV-3s',
              ['evLanded', 'order', 'once', 'sched', 'get_sched', 'evShow', 'cinematic'])
    g.comment('Every plane delivered or was shot down: track the landed KV-3s; none landed: line and complete',
              ['evFinished', 'track', 'br', 'playShot', 'getShotLine', 'completeShot'])
    g.comment('The last KV-3 is destroyed: line, wait, complete', ['evMax', 'play', 'getLine', 'schedDone',
                                                                  'get_schedDone', 'evDone', 'complete'])
    return g


# name: (parent, builder, plate title, plate description, objective type, extra CDO defaults)
STAGES = {
    'Opening': (ORE, opening, 'Set up the base',
                'The HQ, your first vehicles, three harvesters and the base trucks are flown in. Deploy them where you '
                'want.', 'MAJOR', {}),
    'KeepHQ': (DONT_LOSE_HQ, keep_hq, 'Keep the HQ alive', 'If the HQ falls, the ridge falls with it.', 'MAJOR',
               {'start_as_collapsed_widget': 'True'}),
    'Convoy1_FuelRun': (ORE, convoy1, 'Ambush the fuel convoy',
                        'Kill the escort, then bring a squad within 20 m of a truck before its crew scuttles it.',
                        'MAJOR', {}),
    'Convoy2_Weighbridge': (ORE, simple_convoy('ORE_Convoy2', 'Convoy 2 stops 60 s at the weighbridge (route '
                                               'settings); start it and wait for its end'),
                            'Strike at the weighbridge', 'The convoy stops at the weighbridge. Hit it there.',
                            'MAJOR', {}),
    'LightCounterattack': (COUNTERATTACK, counterattack('Light counterattack: infantry in trucks and light armour '
                                                        '(settings asset); the warning line; one T-70 more when '
                                                        'convoy 2 escaped', escape_bonus=True),
                           'Hold the ridge', 'Soviet trucks and light armour are coming for the ridge.', 'MAJOR', {}),
    'Convoy3_AmmoSplit': (ORE, convoy3,
                          'Stop the ammo convoy', 'Ammo trucks turn off to the battery. Stop them or the shells fall.',
                          'MAJOR', {}),
    'Convoy4_Hunters': (ORE, timed_convoy('ORE_Convoy4', 'Wait, then convoy 4: its infantry dismount at the ambush '
                                          'zone and climb to the ridge'),
                        'Raid the convoys', 'Another column is on the road.', 'MAJOR', {}),
    'Convoy5_Cases': (ORE, timed_convoy('ORE_Convoy5', 'Wait, warning (sealed cargo if the outpost marked it), then '
                                        'convoy 5 with the armoured case truck',
                                        extra_vars=[('RadioOutpostStage', 'class:MissionBase', None, 'Ore|Stages', True),
                                                    ('SealedCargoLine', 'object:SoundBase', None, 'Ore|Lines', True),
                                                    ('UnmarkedLine', 'object:SoundBase', None, 'Ore|Lines', True)],
                                        warning_fn=convoy5_warning),
                      'Raid the convoys', 'Another column is on the road.', 'MAJOR', {}),
    'HeavyCounterattack': (COUNTERATTACK, counterattack('Heavy counterattack: medium and heavy armour with hazmat '
                                                        'engineers (settings asset); the warning line'),
                           'Hold the ridge', 'Heavy armour and hazmat engineers are coming.', 'MAJOR', {}),
    'Convoy6_Heavy': (ORE, timed_convoy('ORE_Convoy6', 'Wait, then convoy 6: the heavy column that reinforces Base B'),
                      'Stop the heavy column', 'A heavy column is heading for the knoll.', 'MAJOR', {}),
    'RadioOutpost': (ORE, tracked_targets('ORE_RadioOutpost', 'Radio hut: ',
                                          'Radio hut destroyed: the manifest takeover, then the line, wait, complete. '
                                          'Convoy 5 is now marked',
                                          extra={'on_done': [('manifest', 'fn.AsyncShowOreManifestTakeover.'
                                                              'ShowOreManifestTakeover', None)],
                                                 'play_after_pin': 'OnClosed'}),
                     'Take the radio outpost', 'A small Soviet radio post in the north foothills. Crates are '
                     'stacked there.', 'BONUS', {}),
    'RecoverCases': (ORE, recover_cases, 'Recover the Berlin cases',
                     'Capture the armoured case truck, or scavenge the sealed cache, and bring the cases to the adit.',
                     'MAJOR', {}),
    'DestroyBaseB': (ORE, destroy_base_b,
                     'Destroy Base B', 'Take the Wismut knoll: guns, radio mast and depot.', 'MAJOR', {}),
    'BaseA': (ORE, tracked_targets('ORE_BaseA_Target', 'Convoy yard: ', 'Yard destroyed: line, wait, complete'),
              'Raid the convoy yard', 'The yard at the east end is lightly held. Take what is stored there.',
              'BONUS', {}),
    'Battery': (ORE, battery,
                'Silence the battery', 'The guns in the north-east reach the ridge whenever they have shells.',
                'BONUS', {}),
    'KV3Drop': (ORE, kv3_drop, 'Watch the skies', 'Soviet transport planes have been seen over the border.', 'MAJOR',
                {}),
    'Ending': (ORE, ending, 'Secure the cases', 'Base B has fallen. Get the Berlin cases to the adit.', 'MAJOR', {}),
}


def plate_defaults(title, description, objective):
    state = ('unreal.MissionWidgetState(mission_title=unreal.Text(%r), mission_description=unreal.Text(%r), '
             'objective_type=unreal.MissionObjectiveType.%s)' % (title, description, objective))
    return {'mission_widget_state': state}


def delete_if_exists(path):
    ue.py(f"import unreal\nif unreal.EditorAssetLibrary.does_asset_exist('{path}'):\n"
          f"    print(unreal.EditorAssetLibrary.delete_asset('{path}'))")


def build_stage(short):
    parent, builder, title, description, objective, extra = STAGES[short]
    name = 'Mission_Ore_' + short
    path = ue.BASE + name
    print(f'== {name}', flush=True)
    sl.create_stage(name, parent)  # an existing stage is rebuilt in place (the map references it)
    if parent == ORE:
        sl.remove_own_vars(path, ['ConvoyRoute'])  # lives on Mission_Ore_Base since the lookout wiring
    graph = builder(path)
    if graph is not None:
        result = sl.build_event_graph(path, graph)
        if not (isinstance(result, dict) and result.get('ok')):
            print('  retry after a compile (custom events called by name need a compiled class)', flush=True)
            sl.compile_bp(path)
            sl.build_event_graph(path, graph)
    sl.compile_bp(path)
    defaults = plate_defaults(title, description, objective)
    defaults.update(extra)
    sl.set_defaults(path, defaults)
    sl.compile_bp(path)
    sl.save(path)


MISSION_LINES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OreMountains/Mission/'
LINE_SLOTS = {
    'Base': {'FuelWarningLine': 'Lookout_Fuel', 'AmmoWarningLine': 'Lookout_Ammo', 'PartsWarningLine': 'Lookout_Parts',
             'ConcreteWarningLine': 'Lookout_Concrete'},
    'Convoy1_FuelRun': {'StartLine': 'Convoy1_Start', 'FirstCaptureLine': 'Convoy1_FirstCapture',
                        # Approach cinematic (2026-11-24); recorded by a separate lines session.
                        'ConvoyApproachLine1': 'Convoy1Approach_01', 'ConvoyApproachLine2': 'Convoy1Approach_02',
                        'ConvoyApproachLine3': 'Convoy1Approach_03'},
    'Convoy2_Weighbridge': {'StartLine': 'Convoy2_Start'},
    'Convoy3_AmmoSplit': {'StartLine': 'Convoy3_Start'},
    'Convoy4_Hunters': {'StartLine': 'Convoy4_Warning'},
    'Convoy5_Cases': {'SealedCargoLine': 'Convoy5_SealedCargo', 'UnmarkedLine': 'Convoy5_Unmarked'},
    'Convoy6_Heavy': {'StartLine': 'Convoy6_Warning'},
    'LightCounterattack': {'WarningLine': 'LightCounterattack_Warning'},
    'HeavyCounterattack': {'WarningLine': 'HeavyCounterattack_Warning'},
    'RadioOutpost': {'DoneLine': 'RadioOutpost_Taken'},
    'RecoverCases': {'ObjectiveLine': 'RecoverCases_Objective', 'RecoveredLine': 'RecoverCases_Recovered'},
    'BaseA': {'DoneLine': 'BaseA_Raided'},
    'DestroyBaseB': {'DoneLine': 'BaseB_Destroyed'},
    'Battery': {'DoneLine': 'Battery_Silenced'},
    'Ending': {'BerlinLine': 'Ending_Berlin'},
    'KV3Drop': {'KV3Line': 'KV3Landed'},
}
GENERIC_CONVOY_LINES = {'StoppedLine': 'Convoy_Stopped', 'EscapedLine': 'Convoy_Escaped'}


def assign_lines():
    """Advisor lines (take a until the owner picks) on the stage class defaults. The map instances get them too, by
    run_wire_ore_stages.py --lines, because instances placed earlier keep old values as overrides."""
    for short, slots in LINE_SLOTS.items():
        values = dict(slots)
        if short.startswith('Convoy'):
            values.update(GENERIC_CONVOY_LINES)
        path = ue.BASE + 'Mission_Ore_' + short
        sl.set_defaults(path, {var_name: "unreal.load_asset('%sOreMountains_%s')" % (MISSION_LINES, line)
                               for var_name, line in values.items()})
        sl.compile_bp(path)
        sl.save(path)


def link_stage_classes():
    """Class variables that point at other stages (set once every stage exists)."""
    links = {'Convoy5_Cases': {'RadioOutpostStage': 'RadioOutpost'}, 'Ending': {'RecoverCasesStage': 'RecoverCases'}}
    for short, values in links.items():
        path = ue.BASE + 'Mission_Ore_' + short
        sl.set_defaults(path, {var_name: "unreal.load_class(None, '%sMission_Ore_%s.Mission_Ore_%s_C')"
                               % (ue.BASE, target, target) for var_name, target in values.items()})
        sl.compile_bp(path)
        sl.save(path)


def main():
    wanted = sys.argv[1:]
    if not wanted or 'Base' in wanted:
        build_base()
    for short in STAGES:
        if not wanted or short in wanted:
            build_stage(short)
    if not wanted or 'Links' in wanted:
        link_stage_classes()
    if not wanted or 'Lines' in wanted:
        assign_lines()


if __name__ == '__main__':
    main()
