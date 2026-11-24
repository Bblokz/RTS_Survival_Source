"""Build the Ore Mountains (mission 2) stage Blueprints in our own editor over UECP.

Design: Source/Docs/Campaign/OreMountainsMission.md, "Mission structure" (Missions[] stage plan).
Rules (owner): one UMissionBase Blueprint per stage; the flow in the event graph; fixed steps in Blueprint functions;
waits through ScheduleSingleCallback; compact, readable graphs.

Usage (editor on the Ore map, UECP direct port in ORE_UECP_PORT, default 30000):
    python build_ore_stages.py              build every stage (existing stage assets are deleted and rebuilt)
    python build_ore_stages.py Opening ...  only these stages (names without the Mission_Ore_ prefix)

Shared pieces:
    Mission_Ore_Base   parent of the UMissionBase stages: PlayAdvisorLine, StartConvoyRoute, TrackTaggedTargets
    BPML_OreMission    macro library: SwitchConvoyEnd (Outcome -> Stopped / Escaped)
"""
import json
import sys

import ue
import stage_lib as sl
from stage_lib import Graph, var

ORE = sl.ORE_BASE_CLASS
DONT_LOSE_HQ = '/Script/RTS_Survival.DontLoseHQMission'
COUNTERATTACK = '/Script/RTS_Survival.SovietCounterattackMission'
SWITCH_END = 'mc.BPML_OreMission.SwitchConvoyEnd'
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
        {'op': 'call', 'func': 'EnemyArtilleryBattery.FindEnemyArtilleryBattery',
         'args': {'WorldContextObject': '$self', 'BatteryIdToFind': '$BatteryId'}, 'as': 'Found'},
        {'op': 'return', 'values': {'Battery': '$Found'}}],
        outputs=[{'name': 'Battery', 'type': 'object:EnemyArtilleryBattery'}])
    sl.syn(path, 'GetTruckSubsystem', [
        {'op': 'call', 'func': 'SurrenderingTruckSubsystem.Get', 'args': {'WorldContextObject': '$self'}, 'as': 'Found'},
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
           ('loop', 'k2.ForEachLoop', None),
           ('bind', 'ev.DispatcherBind.LookoutTowerCaptureActor.OnConvoyWarning', {'no_chain': True})])
    g.getters('loop', [('towers', 'fn.LookoutTowerCaptureActor.GetAllLookoutTowers', 'Array', 'ReturnValue')], dx=-60)
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
    g._place('getRoute', var('ConvoyRoute'), sx - 210, sy + 60)
    g.wire('evWarn.Route', 'same.A')
    g.wire('getRoute.ConvoyRoute', 'same.B')
    g.wire('evWarn.CargoType', 'warn.CargoType')
    g.end_row()
    g.comment("Convoy stages call this at their start: bind every lookout tower's convoy warning",
              ['evListen', 'loop', 'towers', 'bind', 'bind_mk'])
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
    """OnConvoyFinished -> SwitchConvoyEnd -> stopped / escaped line -> complete (both arms meet at one node)."""
    g.row([('evFin', 'ev.CustomEvent', {'custom_name': 'OnConvoyFinished'}),
           ('sw', SWITCH_END, {'no_chain': True})])
    g.nodes[-2]['params'] = CONVOY_FINISHED_PARAMS
    g.wire('evFin.then', 'sw.In')
    g.wire('evFin.Outcome', 'sw.Outcome')
    sx, sy = g.pos['sw']
    g.row([('playStopped', 'fn.Self.PlayAdvisorLine', None)], x=sx + 280, y=sy - 70)
    g.row([('playEscaped', 'fn.Self.PlayAdvisorLine', None)], x=sx + 280, y=sy + 135)
    g._place('getStopped', var('StoppedLine'), sx + 260, sy + 78)
    g._place('getEscaped', var('EscapedLine'), sx + 260, sy + 283)
    g.wire('getStopped.StoppedLine', 'playStopped.Line')
    g.wire('getEscaped.EscapedLine', 'playEscaped.Line')
    g.wire('sw.Stopped', 'playStopped.execute')
    g.wire('sw.Escaped', 'playEscaped.execute')
    g.row([('complete', 'fn.MissionBase.OnMissionComplete', None)], x=sx + 640, y=sy + 40)
    g.wire('playStopped.then', 'complete.execute')
    g.wire('playEscaped.then', 'complete.execute')
    g.end_row()
    g.comment('The convoy is done, stopped (destroyed or captured) or escaped: a line for either, then complete. '
              'The stage always completes, so the next stage starts either way.',
              ['evFin', 'sw', 'playStopped', 'playEscaped', 'getStopped', 'getEscaped', 'complete'])


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


# ----------------------------------------------------------------------------------------------- stages

def opening(path):
    sl.add_vars(path, [
        ('LocationText', 'text', 'Erzgebirge', 'Ore|Intro', True),
        ('DateText', 'text', '1976', 'Ore|Intro', True, 'Placeholder date; the owner decides.'),
        ('SubtitleText', 'text', 'Ore Mountains, the Czech border', 'Ore|Intro', True),
        ('WeighbridgeLocation', 'struct:Vector', '(X=133,Y=7705,Z=33)', 'Ore|Places', True),
        ('BaseBLocation', 'struct:Vector', '(X=12700,Y=-6000,Z=1600)', 'Ore|Places', True),
        ('BatteryLocation', 'struct:Vector', '(X=18760,Y=13400,Z=950)', 'Ore|Places', True),
        ('AditLocation', 'struct:Vector', '(X=-21938,Y=8196,Z=1820)', 'Ore|Places', True),
        ('BriefingLine1', 'object:SoundBase', sound('OreMountains_Briefing_01_Weighbridge'), 'Ore|Lines', True),
        ('BriefingLine2', 'object:SoundBase', sound('OreMountains_Briefing_02_DiggingIn'), 'Ore|Lines', True),
        ('BriefingLine3', 'object:SoundBase', sound('OreMountains_Briefing_03_SpecialConvoy'), 'Ore|Lines', True),
        ('Line1Seconds', 'int', '11', 'Ore|Timing', True, 'Wait after briefing line 1 (its length, 10.5 s).'),
        ('Line2Seconds', 'int', '22', 'Ore|Timing', True, 'Wait after briefing line 2 (21.3 s).'),
        ('Line3Seconds', 'int', '12', 'Ore|Timing', True, 'Wait after briefing line 3 (11.1 s).'),
        ('StartForceLocation', 'struct:Vector', '(X=-21000,Y=6400,Z=1850)', 'Ore|Places', True,
         'Centre of the start force on the bench, west of the adit drop-off.'),
        ('StartForceRotation', 'struct:Rotator', '(Pitch=0,Yaw=0,Roll=0)', 'Ore|Places', True)])
    sl.compile_bp(path)
    sl.syn(path, 'ShowOreLocationIntro', [
        {'op': 'call', 'func': 'ShowLocationIntro',
         'args': {'Location': '$LocationText', 'Date': '$DateText', 'DriftAxis': 'WorldY', 'DriftDistance': -1500,
                  'DurationSeconds': 7, 'bPanCamera': False, 'ID': 0, 'LocationSubtitle': '$SubtitleText'}}])
    shots = []
    for index, (place, title, subtitle, yaw) in enumerate((
            ('WeighbridgeLocation', 'The weighbridge', 'Every convoy is weighed here', -20),
            ('BaseBLocation', 'The Wismut knoll', 'Their guns watch the road', -40),
            ('BatteryLocation', 'The battery', 'Out of range, until the shells come', -10),
            ('AditLocation', 'Our adit', 'Captured trucks unload here', -30))):
        shots.append({'op': 'make_struct', 'struct': 'MissionCameraTourShot', 'as': f'Shot{index}', 'fields': {
            'FocusStart': f'${place}', 'FocusEnd': f'${place}', 'YawStart': yaw - 15, 'YawEnd': yaw + 15,
            'Seconds': 5, 'BlendInSeconds': 1.5, 'Title': title, 'Subtitle': subtitle}})
    sl.syn(path, 'PlayValleyTour', shots + [
        {'op': 'make_array', 'as': 'Shots', 'items': ['$Shot0', '$Shot1', '$Shot2', '$Shot3']},
        {'op': 'make_struct', 'struct': 'MissionCameraTourSettings', 'as': 'Settings',
         'fields': {'bShowLetterbox': True, 'ReturnSeconds': 1.75}},
        {'op': 'call', 'func': 'PlayCameraTour', 'args': {'Shots': '$Shots', 'Settings': '$Settings', 'ID': 1}}])
    offsets = ((0, 0), (0, 900), (700, 0), (700, 900))
    spawns = ('SpawnPlayerLightMediumTank_PZIII_J_Or_PZIV_F1', 'SpawnPlayerLightMediumTank_PZIII_J_Or_PZIV_F1',
              'SpawnPlayerGerMediumTankDestroyer_StugIII_OrMarder', 'SpawnPlayerGerArmoredCar250_251Mortar')
    body = []
    for (dx, dy), func in zip(offsets, spawns):
        body.append({'op': 'call', 'func': func, 'args': {
            'SpawnLocation': f'=$StartForceLocation + MakeVector({dx}, {dy}, 0)', 'SpawnRotation': '$StartForceRotation'}})
    sl.syn(path, 'SpawnStartForce', body)
    sl.compile_bp(path)

    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('intro', 'fn.Self.ShowOreLocationIntro', None)])
    g.end_row()
    g.row([('evIntro', 'ev.BP_OnLocationIntroFinished', None), ('tour', 'fn.Self.PlayValleyTour', None)])
    g.end_row()
    g.row([('evTour', 'ev.BP_OnCameraTourFinished', None), ('play1', 'fn.Self.PlayAdvisorLine', None),
           ('sched1', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evLine1', 'ev.CustomEvent', {'custom_name': 'OnLine1Done', 'no_chain': True}),
           ('play2', 'fn.Self.PlayAdvisorLine', None), ('sched2', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evLine2', 'ev.CustomEvent', {'custom_name': 'OnLine2Done', 'no_chain': True})])
    g.getters('play1', [('getL1', var('BriefingLine1'), 'Line', 'BriefingLine1')])
    schedule_wait(g, 'sched1', 'evLine1', 'OnLine1Done', 'Line1Seconds')
    g.getters('play2', [('getL2', var('BriefingLine2'), 'Line', 'BriefingLine2')])
    schedule_wait(g, 'sched2', 'evLine2', 'OnLine2Done', 'Line2Seconds')
    row3_bottom = g.bottom
    g.end_row()
    ex, _ = g.pos['evLine2']
    g.row([('play3', 'fn.Self.PlayAdvisorLine', None), ('sched3', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evLine3', 'ev.CustomEvent', {'custom_name': 'OnBriefingDone', 'no_chain': True}),
           ('spawn', 'fn.Self.SpawnStartForce', None), ('complete', 'fn.MissionBase.OnMissionComplete', None)],
          x=ex, y=row3_bottom + 60)
    g.wire('evLine2.then', 'play3.execute')
    g.getters('play3', [('getL3', var('BriefingLine3'), 'Line', 'BriefingLine3')])
    schedule_wait(g, 'sched3', 'evLine3', 'OnBriefingDone', 'Line3Seconds')
    g.end_row()
    g.comment('Location intro (Erzgebirge)', ['evStart', 'intro'])
    g.comment('Camera tour of the valley: weighbridge, knoll, battery, our adit', ['evIntro', 'tour'])
    g.comment('Briefing 1 (weighbridge patrol), wait, briefing 2 (Shield digs in), wait',
              ['evTour', 'play1', 'getL1', 'sched1', 'get_sched1', 'evLine1', 'play2', 'getL2', 'sched2', 'get_sched2',
               'evLine2'])
    g.comment('Briefing 3 (the special convoy), wait, the start force drives out of the adits; complete '
              '(KeepHQ, Convoy 1, the bases and the outpost start on this)',
              ['play3', 'getL3', 'sched3', 'get_sched3', 'evLine3', 'spawn', 'complete'])
    return g


def keep_hq(path):
    return None


def convoy1(path):
    sl.add_vars(path, convoy_vars('ORE_Convoy1') + [
        ('AmbushHintLocation', 'struct:Vector', '(X=-2814,Y=1842,Z=52)', 'Ore|Places', True,
         'Ambush spot below the ridge, marked on the minimap.'),
        ('FirstCaptureLine', 'object:SoundBase', None, 'Ore|Lines', True, 'Advisor line on the first captured truck.')])
    sl.compile_bp(path)
    sl.syn(path, 'ShowAmbushHint', [
        {'op': 'call', 'func': 'AddCustomMiniMapIcon',
         'args': {'IconId': 'ORE_AmbushHint', 'IconType': 'Warning', 'WorldLocation': '$AmbushHintLocation'}}])
    sl.syn(path, 'RemoveAmbushHint', [
        {'op': 'call', 'func': 'RemoveCustomMiniMapIcon', 'args': {'IconId': 'ORE_AmbushHint'}}])
    sl.syn(path, 'ShowCaptureFieldManual', [
        {'op': 'print', 'value': 'Ore: the Capturing trucks field manual is not built yet (needs its takeover widget).'}])
    sl.compile_bp(path)
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('hint', 'fn.Self.ShowAmbushHint', None)]
          + start_convoy_nodes() + [('bindCap', 'ev.DispatcherBind.EnemyConvoyRoute.OnConvoyTruckCaptured', None)])
    wire_start_convoy(g)
    bx, by = g.pos['bindCap']
    g._place('bindCap_route', var('ConvoyRoute'), bx - 20, by + 150)
    g.wire('bindCap_route.ConvoyRoute', 'bindCap.self')
    g._place('bindCap_mk', 'k2.CreateEvent', bx - 20, by + 215)
    g.nodes[-1]['function'] = 'OnTruckCaptured'
    g.wire('bindCap_mk.OutputDelegate', 'bindCap.Delegate')
    g.bottom = max(g.bottom, by + 330)
    g.end_row()
    g.row([('evCap', 'ev.CustomEvent', {'custom_name': 'OnTruckCaptured'}), ('once', 'k2.DoOnce', None),
           ('manual', 'fn.Self.ShowCaptureFieldManual', None), ('playCap', 'fn.Self.PlayAdvisorLine', None)])
    g.nodes[-4]['params'] = [{'name': 'Route', 'type': 'object:EnemyConvoyRoute'},
                             {'name': 'Convoy', 'type': 'object:Convoy'}, {'name': 'Truck', 'type': 'object:Actor'}]
    g.connections = [c for c in g.connections if c['to'] != 'once.execute']
    g.wire('evCap.then', 'once.Execute')
    g.connections = [c for c in g.connections if c['from'] != 'once.then']
    g.wire('once.Completed', 'manual.execute')
    g.getters('playCap', [('getCapLine', var('FirstCaptureLine'), 'Line', 'FirstCaptureLine')])
    g.end_row()
    convoy_end_row(g)
    g.row([('evDone', 'ev.BP_OnMissionComplete', None), ('unhint', 'fn.Self.RemoveAmbushHint', None)])
    g.end_row()
    g.comment('The guided fuel run: mark the ambush spot, Advisor line, start convoy 1 and listen for its end and '
              'for captured trucks', ['evStart', 'hint', 'play', 'get_play', 'start', 'getId', 'setRoute', 'bind',
                                      'bind_route', 'bind_mk', 'bindCap', 'bindCap_route', 'bindCap_mk'])
    g.comment('The first truck captured (once): the Capturing trucks field manual (slot, not built yet), then the '
              'Advisor line', ['evCap', 'once', 'manual', 'playCap', 'getCapLine'])
    g.comment('Stage done: remove the ambush mark', ['evDone', 'unhint'])
    return g


def simple_convoy(route_id, note):
    def build(path):
        sl.add_vars(path, convoy_vars(route_id))
        sl.compile_bp(path)
        g = Graph()
        g.row([('evStart', 'ev.BP_OnMissionStart', None)] + start_convoy_nodes())
        wire_start_convoy(g)
        g.end_row()
        convoy_end_row(g)
        g.comment(note, ['evStart', 'play', 'get_play', 'start', 'getId', 'setRoute', 'bind', 'bind_route', 'bind_mk', 'listen'])
        return g
    return build


def convoy3(path):
    sl.add_vars(path, convoy_vars('ORE_Convoy3'))
    sl.compile_bp(path)
    sl.syn(path, 'NotifyBattery', [
        {'op': 'call', 'func': 'GetOreBattery', 'as': 'Battery'},
        {'op': 'is_valid', 'value': '$Battery.Battery', 'then': [
            {'op': 'call', 'func': 'NotifyAmmoTruckArrived', 'target': '$Battery.Battery', 'args': {'Truck': '$Truck'}}]}],
        inputs=[{'name': 'Truck', 'type': 'object:Actor'}])
    sl.compile_bp(path)
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None)] + start_convoy_nodes()
          + [('bindBranch', 'ev.DispatcherBind.EnemyConvoyRoute.OnBranchTruckArrived', None)])
    wire_start_convoy(g)
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
    convoy_end_row(g)
    g.comment('Convoy 3 splits: its ammo trucks turn off to the battery (route Branch settings). Start it, wait for its '
              'end, and tell the battery about every ammo truck that arrives',
              ['evStart', 'play', 'get_play', 'start', 'getId', 'setRoute', 'bind', 'bind_route', 'bind_mk', 'listen',
               'bindBranch', 'bindBranch_route', 'bindBranch_mk'])
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
        if warning_fn:
            warning_fn(path)
            sl.compile_bp(path)
        g = Graph()
        g.row([('evStart', 'ev.BP_OnMissionStart', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
               ('evGap', 'ev.CustomEvent', {'custom_name': 'OnGapOver', 'no_chain': True})]
              + ([('warn', 'fn.Self.PlayConvoyWarning', None)] if warning_fn else [])
              + start_convoy_nodes('play' if not warning_fn else 'playStart'))
        schedule_wait(g, 'sched', 'evGap', 'OnGapOver', 'GapSeconds')
        wire_start_convoy(g, 'play' if not warning_fn else 'playStart')
        g.end_row()
        convoy_end_row(g)
        ids = ['evStart', 'sched', 'get_sched', 'evGap', 'start', 'getId', 'setRoute', 'bind', 'bind_route', 'bind_mk', 'listen']
        ids += ['warn', 'playStart', 'get_playStart'] if warning_fn else ['play', 'get_play']
        g.comment(note, ids)
        return g
    return build


def convoy5_warning(path):
    sl.syn(path, 'PlayConvoyWarning', [
        {'op': 'call', 'func': 'GetHasMissionClassCompleted', 'args': {'MissionClass': '$RadioOutpostStage'},
         'as': 'bMarked'},
        {'op': 'branch', 'condition': '$bMarked.ReturnValue',
         'then': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$SealedCargoLine'}}],
         'else': [{'op': 'call', 'func': 'PlayAdvisorLine', 'args': {'Line': '$UnmarkedLine'}}]}])


def counterattack(note):
    def build(path):
        sl.add_vars(path, [('WarningLine', 'object:SoundBase', None, 'Ore|Lines', True,
                            'Advisor warning as the counterattack starts.')])
        sl.compile_bp(path)
        g = Graph()
        g.row([('evStart', 'ev.BP_OnMissionStart', None), ('play', 'fn.MissionBase.PlayPortrait', None)])
        g.getters('play', [('getLine', var('WarningLine'), 'VoiceLine', 'WarningLine')])
        g.default('play', 'PortraitType', 'GermanAnnouncer')
        g.end_row()
        g.comment(note, ['evStart', 'play', 'getLine'])
        return g
    return build


def tracked_targets(tag, prefix, note, extra=None):
    """Complete when every actor tagged `tag` is destroyed: line, wait, complete."""
    def build(path):
        sl.add_vars(path, [
            ('TargetTag', 'name', tag, 'Ore|Targets', True, 'Every actor with this tag must be destroyed.'),
            ('ProgressPrefix', 'string', prefix, 'Ore|Targets', True),
            ('DoneLine', 'object:SoundBase', None, 'Ore|Lines', True),
            ('CompleteDelaySeconds', 'int', '3', 'Ore|Timing', True)] + (extra['vars'] if extra else []))
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
        done_nodes += [('play', 'fn.Self.PlayAdvisorLine', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
                       ('evDone', 'ev.CustomEvent', {'custom_name': 'OnTargetsDone', 'no_chain': True}),
                       ('complete', 'fn.MissionBase.OnMissionComplete', None)]
        g.row(done_nodes)
        g.getters('play', [('getLine', var('DoneLine'), 'Line', 'DoneLine')])
        schedule_wait(g, 'sched', 'evDone', 'OnTargetsDone', 'CompleteDelaySeconds')
        g.end_row()
        g.comment(f'Track every actor tagged {tag} on the minimap (none placed yet: the plate stays up)',
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
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('getSub', 'fn.Self.GetTruckSubsystem', None), ('setSub', 'var.set.TruckSubsystem', None),
           ('bindDelivered', 'ev.DispatcherBind.SurrenderingTruckSubsystem.OnCasesDelivered', None),
           ('bindCache', 'ev.DispatcherBind.SurrenderingTruckSubsystem.OnSealedCacheScavenged', None)])
    g.getters('play', [('getLine', var('ObjectiveLine'), 'Line', 'ObjectiveLine')])
    g.wire('getSub.Subsystem', 'setSub.TruckSubsystem')
    for bind_id, event in (('bindDelivered', 'OnCasesDeliveredEvent'), ('bindCache', 'OnSealedCacheScavengedEvent')):
        bx, by = g.pos[bind_id]
        g._place(f'{bind_id}_sub', var('TruckSubsystem'), bx - 20, by + 150)
        bind_named(g, bind_id, f'{bind_id}_sub.TruckSubsystem', event, extra_y=65)
    g.end_row()
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
    g.comment('Recover the Berlin cases: objective line, then listen for the case truck reaching the adit or a sealed '
              'cache of the cases being scavenged', ['evStart', 'play', 'getLine', 'getSub', 'setSub', 'bindDelivered',
                                                     'bindDelivered_sub', 'bindDelivered_mk', 'bindCache',
                                                     'bindCache_sub', 'bindCache_mk'])
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
    sl.set_defaults(path, {'complete_mission_when_tracking_done': 'False'})
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
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('track', 'fn.Self.TrackTaggedTargets', None),
           ('findDir', 'fn.GameplayStatics.GetActorOfClass', None), ('cast', 'k2.Cast To ConvoyPressureDirector', None),
           ('setDir', 'var.set.PressureDirector', None),
           ('bindAmmo', 'ev.DispatcherBind.ConvoyPressureDirector.OnEscapedAmmoSeconds', None)])
    g.getters('track', [('getTag', var('TargetTag'), 'Tag', 'TargetTag'),
                        ('getPrefix', var('ProgressPrefix'), 'ProgressPrefix', 'ProgressPrefix')])
    g.default('findDir', 'ActorClass', '/Script/RTS_Survival.ConvoyPressureDirector')
    g.wire('findDir.ReturnValue', 'cast.Object')
    g.wire('cast.AsConvoyPressureDirector', 'setDir.PressureDirector')
    bx, by = g.pos['bindAmmo']
    g._place('bindAmmo_dir', var('PressureDirector'), bx - 20, by + 150)
    bind_named(g, 'bindAmmo', 'bindAmmo_dir.PressureDirector', 'OnEscapedAmmo', extra_y=65)
    g.end_row()
    g.row([('evAmmo', 'ev.CustomEvent', {'custom_name': 'OnEscapedAmmo'}), ('forward', 'fn.Self.ForwardAmmoToBattery', None)])
    g.nodes[-2]['params'] = [{'name': 'Seconds', 'type': 'float'}]
    g.wire('evAmmo.Seconds', 'forward.Seconds')
    g.end_row()
    g.row([('evMax', 'ev.BP_OnTrackingReachedMaxCount', None), ('drop', 'fn.Self.DropEscapedCases', None),
           ('play', 'fn.Self.PlayAdvisorLine', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnTargetsDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('play', [('getLine', var('DoneLine'), 'Line', 'DoneLine')])
    schedule_wait(g, 'sched', 'evDone', 'OnTargetsDone', 'CompleteDelaySeconds')
    g.end_row()
    g.comment('Track every actor tagged ORE_BaseB_Target; find the convoy pressure director and listen for escaped '
              'ammo', ['evStart', 'track', 'getTag', 'getPrefix', 'findDir', 'cast', 'setDir', 'bindAmmo',
                       'bindAmmo_dir', 'bindAmmo_mk'])
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


# name: (parent, builder, plate title, plate description, objective type, extra CDO defaults)
STAGES = {
    'Opening': (ORE, opening, 'Survey the valley', 'Our scouts know the road. Listen to the briefing.', 'MAJOR', {}),
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
                                                        '(settings asset); the warning line'),
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
    'RadioOutpost': (ORE, tracked_targets('ORE_RadioOutpost', 'Outpost guards: ',
                                          'Guards dead: the manifest (takeover not built yet; line), wait, complete. '
                                          'Convoy 5 is now marked'),
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
    delete_if_exists(path)
    sl.create_stage(name, parent)
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


if __name__ == '__main__':
    main()
