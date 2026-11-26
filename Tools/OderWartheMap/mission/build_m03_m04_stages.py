"""Builds the Oder-Warthe stages M03 and M04 (OderWartheMissionFlow.md 10.3) over UECP and wires them after M00.

Mission_OW_ListeningPost (M03, plate "Take the listening post"): when DF 1 (the pump house) is held, the Direction
Finding field manual opens; once it is closed the Advisor says A2 ("Now we listen.") and the stage completes.
Mission_OW_FirstAttack (M04, plate "Hold the ridge"): the first wave is sent by AWerkWaveDirector as soon as DF 1 is
held, or after the difficulty's grace time at the latest. When its units spawn the Advisor warns (A11) and, if DF 1
listens, adds A3 ("When they attack, they talk."). When the wave is wiped out, a small supply drop arrives and the
stage completes. Later waves on the timer come with M05.

Usage: python build_m03_m04_stages.py <uecp port>     (then ow_cmd.py Saved/OderWarthe/c_m03_m04.py for the wiring)
"""
import sys

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/mission')
import layout_eventgraphs as layout  # noqa: E402
from layout_eventgraphs import Graph, var  # noqa: E402
from uecp_client import action  # noqa: E402

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OderWarthe/'
LINES = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OderWarthe/'
MANAGER = {'op': 'call', 'func': 'Self.GetMissionManagerChecked'}

_berlin_height = layout.height
_TALL = {'DispatcherBind': 120, 'PlayPortrait': 185, 'ScheduleSingle': 170, 'ShowDirectionFindingTakeover': 120}


def _height(handle):
    for key, value in _TALL.items():
        if key in handle:
            return max(value, _berlin_height(handle))
    return _berlin_height(handle)


layout.height = _height


def line(name):
    return LINES + name + '.' + name


# ------------------------------------------------------------------------------------------------ shared functions

FIND_DF = {
    'function': 'FindDirectionFinding',
    'outputs': [{'name': 'bFound', 'type': 'bool'}],
    'body': [
        {'op': 'call', 'func': 'DirectionFindingManager.FindDirectionFindingManager',
         'args': {'WorldContextObject': MANAGER}, 'as': 'Found'},
        {'op': 'set_var', 'var': 'DFManager', 'value': '$Found'},
        {'op': 'is_valid', 'value': '$Found', 'then': [{'op': 'return', 'values': {'bFound': True}}]},
        {'op': 'return', 'values': {'bFound': False}},
    ],
}

IS_DF1_TAKEN = {
    'function': 'IsDF1Taken',
    'inputs': [{'name': 'StationIndex', 'type': 'int'}, {'name': 'bHeldByPlayer', 'type': 'bool'}],
    'outputs': [{'name': 'bTaken', 'type': 'bool'}],
    'body': [
        {'op': 'branch', 'condition': '$bHeldByPlayer', 'then': [
            {'op': 'return', 'values': {'bTaken': '=$StationIndex == 0'}}]},
        {'op': 'return', 'values': {'bTaken': False}},
    ],
}

IS_DF1_HELD_NOW = {
    'function': 'IsDF1HeldNow',
    'outputs': [{'name': 'bHeld', 'type': 'bool'}],
    'body': [
        {'op': 'is_valid', 'value': '$DFManager', 'then': [
            {'op': 'call', 'func': 'DirectionFindingManager.GetIsStationHeldByPlayer', 'target': '$DFManager',
             'args': {'StationIndex': 0}, 'as': 'bHeldNow'},
            {'op': 'return', 'values': {'bHeld': '$bHeldNow'}}]},
        {'op': 'return', 'values': {'bHeld': False}},
    ],
}

# ------------------------------------------------------------------------------------------------ M03

M03 = {
    'name': 'Mission_OW_ListeningPost',
    'variables': [
        {'variable_name': 'DFManager', 'variable_type': 'object:DirectionFindingManager', 'category': 'OW|State'},
        {'variable_name': 'A2Line', 'variable_type': 'object:SoundBase', 'instance_editable': True,
         'category': 'OW|Lines', 'tooltip': 'Advisor once the field manual is closed: "Now we listen."'},
    ],
    'defaults': {'A2Line': line('OderWarthe_A2_NowWeListen')},
    'functions': [FIND_DF, IS_DF1_TAKEN, IS_DF1_HELD_NOW],
}

# ------------------------------------------------------------------------------------------------ M04

FIND_PIECES = {
    'function': 'FindWavePieces',
    'outputs': [{'name': 'bFound', 'type': 'bool'}],
    'body': [
        {'op': 'call', 'func': 'Self.FindDirectionFinding', 'as': 'bFoundDF'},
        {'op': 'call', 'func': 'WerkWaveDirector.FindWerkWaveDirector', 'args': {'WorldContextObject': MANAGER},
         'as': 'Director'},
        {'op': 'set_var', 'var': 'WaveDirector', 'value': '$Director'},
        {'op': 'is_valid', 'value': '$Director', 'then': [{'op': 'return', 'values': {'bFound': '$bFoundDF'}}]},
        {'op': 'return', 'values': {'bFound': False}},
    ],
}

GRACE = {
    'function': 'GetGraceSeconds',
    'outputs': [{'name': 'Seconds', 'type': 'int'}],
    'body': [
        {'op': 'call', 'func': 'WerkWaveDirector.GetFirstWaveGraceSeconds', 'target': '$WaveDirector', 'as': 'Grace'},
        {'op': 'return', 'values': {'Seconds': '=FTrunc($Grace)'}},
    ],
}

SEND_FIRST = {
    'function': 'SendFirstWave',
    'body': [
        {'op': 'branch', 'condition': '$bFirstWaveSent', 'then': [{'op': 'return'}]},
        {'op': 'set_var', 'var': 'bFirstWaveSent', 'value': True},
        {'op': 'call', 'func': 'WerkWaveDirector.LaunchWave', 'target': '$WaveDirector'},
    ],
}

IS_FIRST = {
    'function': 'IsFirstWave',
    'inputs': [{'name': 'WaveNumber', 'type': 'int'}],
    'outputs': [{'name': 'bFirst', 'type': 'bool'}],
    'body': [{'op': 'return', 'values': {'bFirst': '=$WaveNumber == 1'}}],
}

WARN = {
    'function': 'PlayFirstWaveWarning',
    'body': [
        {'op': 'call', 'func': 'MissionBase.PlayPortrait',
         'args': {'PortraitType': 'GermanAnnouncer', 'VoiceLine': '$WarnLine'}},
        {'op': 'call', 'func': 'Self.IsDF1HeldNow', 'as': 'bListening'},
        # Only a held station heard the attack order, so only then "Hear that?" (queued behind the warning).
        {'op': 'branch', 'condition': '$bListening', 'then': [
            {'op': 'call', 'func': 'MissionBase.PlayPortrait',
             'args': {'PortraitType': 'GermanAnnouncer', 'VoiceLine': '$HearThatLine'}}]},
    ],
}

DROP = {
    'function': 'GiveFirstWaveDrop',
    'body': [
        {'op': 'call', 'func': 'MissionBase.GivePlayerResources',
         'args': {'Resource': 'Resource_Radixite', 'Amount': '$DropRadixite'}},
        {'op': 'call', 'func': 'MissionBase.GivePlayerResources',
         'args': {'Resource': 'Resource_Metal', 'Amount': '$DropMetal'}},
    ],
}

M04 = {
    'name': 'Mission_OW_FirstAttack',
    'variables': [
        {'variable_name': 'DFManager', 'variable_type': 'object:DirectionFindingManager', 'category': 'OW|State'},
        {'variable_name': 'WaveDirector', 'variable_type': 'object:WerkWaveDirector', 'category': 'OW|State'},
        {'variable_name': 'bFirstWaveSent', 'variable_type': 'bool', 'default_value': 'false', 'category': 'OW|State'},
        {'variable_name': 'WarnLine', 'variable_type': 'object:SoundBase', 'instance_editable': True,
         'category': 'OW|Lines', 'tooltip': 'Advisor when the first wave leaves its tunnel mouth (A11).'},
        {'variable_name': 'HearThatLine', 'variable_type': 'object:SoundBase', 'instance_editable': True,
         'category': 'OW|Lines', 'tooltip': 'Advisor after the warning while DF 1 listens (A3).'},
        {'variable_name': 'DropRadixite', 'variable_type': 'int', 'default_value': '300', 'instance_editable': True,
         'category': 'OW|Reward', 'tooltip': 'Small supply drop when the first wave is beaten (placeholder).'},
        {'variable_name': 'DropMetal', 'variable_type': 'int', 'default_value': '200', 'instance_editable': True,
         'category': 'OW|Reward', 'tooltip': 'Small supply drop when the first wave is beaten (placeholder).'},
    ],
    'defaults': {'WarnLine': line('OderWarthe_A11_WaveLeaving'), 'HearThatLine': line('OderWarthe_A3_HearThat')},
    'functions': [FIND_DF, IS_DF1_TAKEN, IS_DF1_HELD_NOW, FIND_PIECES, GRACE, SEND_FIRST, IS_FIRST, WARN, DROP],
}


# ------------------------------------------------------------------------------------------------ event graphs

def bind(g, bind_id, var_name, event_name):
    """Target getter and the Create Event naming event_name, stacked under the Bind node."""
    bx, by = g.pos[bind_id]
    g._place(f'{bind_id}_target', var(var_name), bx - 20, by + 150)
    g.wire(f'{bind_id}_target.{var_name}', f'{bind_id}.self')
    g._place(f'{bind_id}_mk', 'k2.CreateEvent', bx - 20, by + 215)
    g.nodes[-1]['function'] = event_name
    g.wire(f'{bind_id}_mk.OutputDelegate', f'{bind_id}.Delegate')
    g.bottom = max(g.bottom, by + 300)


def custom_event(node_id, name, params):
    return node_id, 'ev.CustomEvent', {'custom_name': name, 'params': params}


def apply_event_params(g):
    """Graph._place keeps only custom_name; put the custom events' parameters onto their nodes."""
    for node in g.nodes:
        params = EVENT_PARAMS.get(node.get('custom_name'))
        if node['handle'] == 'ev.CustomEvent' and params:
            node['params'] = params


WAVE_PARAMS = [{'name': 'WaveNumber', 'type': 'int'}, {'name': 'WerkIndex', 'type': 'int'}]
EVENT_PARAMS = {'OnStationChanged': [{'name': 'StationIndex', 'type': 'int'}, {'name': 'bHeldByPlayer', 'type': 'bool'}],
                'OnWaveLaunched': WAVE_PARAMS, 'OnWaveDefeated': WAVE_PARAMS}


def m03_graph():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('find', 'fn.Self.FindDirectionFinding', None),
           ('brFound', 'k2.Branch', None),
           ('bindStation', 'ev.DispatcherBind.DirectionFindingManager.OnStationChanged', None),
           ('heldNow', 'fn.Self.IsDF1HeldNow', None), ('brHeld', 'k2.Branch', None),
           ('takenNow', 'fn.Self.HandleDF1Taken', None)])
    g.wire('find.bFound', 'brFound.Condition')
    g.wire('heldNow.bHeld', 'brHeld.Condition')
    bind(g, 'bindStation', 'DFManager', 'OnStationChanged')
    g.end_row()
    g.row([custom_event('evChanged', 'OnStationChanged', [{'name': 'StationIndex', 'type': 'int'},
                                                          {'name': 'bHeldByPlayer', 'type': 'bool'}]),
           ('isTaken', 'fn.Self.IsDF1Taken', None), ('brTaken', 'k2.Branch', None),
           ('taken', 'fn.Self.HandleDF1Taken', None)])
    g.wire('evChanged.StationIndex', 'isTaken.StationIndex')
    g.wire('evChanged.bHeldByPlayer', 'isTaken.bHeldByPlayer')
    g.wire('isTaken.bTaken', 'brTaken.Condition')
    g.end_row()
    g.row([custom_event('evTaken', 'HandleDF1Taken', []), ('once', 'k2.DoOnce', None),
           ('manual', 'fn.AsyncShowDirectionFindingTakeover.ShowDirectionFindingTakeover', None),
           ('play', 'fn.MissionBase.PlayPortrait', None), ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.connections = [c for c in g.connections if c['to'] not in ('once.execute', 'manual.execute', 'play.execute')]
    g.wire('evTaken.then', 'once.Execute')
    g.wire('once.Completed', 'manual.execute')
    g.wire('manual.OnClosed', 'play.execute')
    g.default('play', 'PortraitType', 'GermanAnnouncer')
    g.getters('play', [('getA2', var('A2Line'), 'VoiceLine', 'A2Line')])
    g.end_row()
    g.comment('Watch DF 1 (the pump house): the manager tells every capture and loss; DF 1 may already be held',
              ['evStart', 'find', 'brFound', 'bindStation', 'bindStation_target', 'bindStation_mk', 'heldNow',
               'brHeld', 'takenNow'])
    g.comment('A station changed hands: DF 1 taken by the player?', ['evChanged', 'isTaken', 'brTaken', 'taken'])
    g.comment('Once: the Direction Finding field manual, then "Now we listen.", then the stage is done',
              ['evTaken', 'once', 'manual', 'play', 'getA2', 'complete'])
    apply_event_params(g)
    return g


def m04_graph():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('find', 'fn.Self.FindWavePieces', None),
           ('brFound', 'k2.Branch', None),
           ('bindLaunch', 'ev.DispatcherBind.WerkWaveDirector.OnWaveLaunched', None),
           ('bindDefeat', 'ev.DispatcherBind.WerkWaveDirector.OnWaveDefeated', None),
           ('bindStation', 'ev.DispatcherBind.DirectionFindingManager.OnStationChanged', None),
           ('grace', 'fn.Self.GetGraceSeconds', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           custom_event('evGrace', 'OnGraceOver', []),
           ('sendGrace', 'fn.Self.SendFirstWave', None)])
    g.wire('find.bFound', 'brFound.Condition')
    bind(g, 'bindLaunch', 'WaveDirector', 'OnWaveLaunched')
    bind(g, 'bindDefeat', 'WaveDirector', 'OnWaveDefeated')
    bind(g, 'bindStation', 'DFManager', 'OnStationChanged')
    g.wire('grace.Seconds', 'sched.DelaySeconds')
    g.wire('evGrace.OutputDelegate', 'sched.Callback')
    # The grace event starts its own exec line (it is not chained after the Schedule node).
    g.connections = [c for c in g.connections if c['to'] != 'evGrace.execute']
    hx, hy = g.pos['sched']
    g.row([('heldNow', 'fn.Self.IsDF1HeldNow', None), ('brHeld', 'k2.Branch', None),
           ('sendNow', 'fn.Self.SendFirstWave', None)], x=hx, y=g.bottom + 40)
    g.wire('sched.then', 'heldNow.execute')
    g.wire('heldNow.bHeld', 'brHeld.Condition')
    g.end_row()
    g.row([custom_event('evChanged', 'OnStationChanged', [{'name': 'StationIndex', 'type': 'int'},
                                                          {'name': 'bHeldByPlayer', 'type': 'bool'}]),
           ('isTaken', 'fn.Self.IsDF1Taken', None), ('brTaken', 'k2.Branch', None),
           ('sendTaken', 'fn.Self.SendFirstWave', None)])
    g.wire('evChanged.StationIndex', 'isTaken.StationIndex')
    g.wire('evChanged.bHeldByPlayer', 'isTaken.bHeldByPlayer')
    g.wire('isTaken.bTaken', 'brTaken.Condition')
    g.end_row()
    wave_params = [{'name': 'WaveNumber', 'type': 'int'}, {'name': 'WerkIndex', 'type': 'int'}]
    g.row([custom_event('evLaunched', 'OnWaveLaunched', wave_params), ('isFirstL', 'fn.Self.IsFirstWave', None),
           ('brFirstL', 'k2.Branch', None), ('warn', 'fn.Self.PlayFirstWaveWarning', None)])
    g.wire('evLaunched.WaveNumber', 'isFirstL.WaveNumber')
    g.wire('isFirstL.bFirst', 'brFirstL.Condition')
    g.end_row()
    g.row([custom_event('evDefeated', 'OnWaveDefeated', wave_params), ('isFirstD', 'fn.Self.IsFirstWave', None),
           ('brFirstD', 'k2.Branch', None), ('drop', 'fn.Self.GiveFirstWaveDrop', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.wire('evDefeated.WaveNumber', 'isFirstD.WaveNumber')
    g.wire('isFirstD.bFirst', 'brFirstD.Condition')
    g.end_row()
    g.comment('Find the wave director and the listening posts, listen to waves and stations; the first wave goes '
              'after the grace time at the latest, or at once if DF 1 is already held',
              ['evStart', 'find', 'brFound', 'bindLaunch', 'bindLaunch_target', 'bindLaunch_mk', 'bindDefeat',
               'bindDefeat_target', 'bindDefeat_mk', 'bindStation', 'bindStation_target', 'bindStation_mk', 'grace',
               'sched', 'evGrace', 'sendGrace', 'heldNow', 'brHeld', 'sendNow'])
    g.comment('DF 1 taken: the first wave goes now', ['evChanged', 'isTaken', 'brTaken', 'sendTaken'])
    g.comment('The first wave left its tunnel mouth: the Advisor warns (and "Hear that?" while DF 1 listens)',
              ['evLaunched', 'isFirstL', 'brFirstL', 'warn'])
    g.comment('The first wave is wiped out: small supply drop, stage done',
              ['evDefeated', 'isFirstD', 'brFirstD', 'drop', 'complete'])
    apply_event_params(g)
    return g


# ------------------------------------------------------------------------------------------------ build

def build_stage(port, spec, graph):
    bp = BASE + spec['name']
    r = action(port, 'create_asset', {'asset_type': 'blueprint', 'name': spec['name'], 'save_path': BASE[:-1],
                                      'parent_class': '/Script/RTS_Survival.MissionBase'})
    print(spec['name'], 'create', r.get('ok'), str(r.get('error', ''))[:160])
    for item in spec['variables']:
        r = action(port, 'add_variable', dict(item, blueprint_path=bp))
        print('  var', item['variable_name'], r.get('ok'), str(r.get('error', ''))[:160])
    for name, value in spec['defaults'].items():
        r = action(port, 'set_blueprint_variable_default', {'blueprint_path': bp, 'variable_name': name,
                                                            'default_value': value})
        print('  default', name, r.get('ok'), str(r.get('error', ''))[:160])
    for function in spec['functions']:
        r = action(port, 'synthesize_graph', dict(function, blueprint=bp))
        print('  fn', function['function'], r.get('ok'),
              str(r.get('error', ''))[:500].encode('ascii', 'replace').decode())
        # Later functions call earlier ones (Self.FindDirectionFinding): compile so they resolve.
        action(port, 'compile_blueprint', {'blueprint_path': bp})
    # Custom events called from the graph (Self.HandleDF1Taken) must exist in a compiled class first.
    stubs = [n for n in graph.nodes if n['handle'] == 'ev.CustomEvent']
    action(port, 'build_blueprint_graph', {'blueprint_path': bp, 'graph_name': 'EventGraph', 'clear_before_build': True,
                                           'clear_force': True, 'nodes': stubs, 'connections': [], 'defaults': [],
                                           'comments': []})
    action(port, 'compile_blueprint', {'blueprint_path': bp})
    r = action(port, 'build_blueprint_graph', {
        'blueprint_path': bp, 'graph_name': 'EventGraph', 'clear_before_build': True, 'clear_force': True,
        'nodes': graph.nodes, 'connections': graph.connections, 'defaults': graph.defaults,
        'comments': graph.comments})
    print('  EventGraph', {k: r.get(k) for k in ('ok', 'nodes', 'connections', 'defaults')} if isinstance(r, dict)
          else str(r)[:300])
    if isinstance(r, dict) and not r.get('ok'):
        print('   ', str(r)[:2000].encode('ascii', 'replace').decode())
    c = action(port, 'compile_blueprint', {'blueprint_path': bp})
    print('  compile', c.get('error_count'), str(c.get('errors', ''))[:900])


def main():
    port = int(sys.argv[1])
    only = sys.argv[2:]
    for spec, graph in ((M03, m03_graph), (M04, m04_graph)):
        if only and spec['name'] not in only:
            continue
        build_stage(port, spec, graph())


if __name__ == '__main__':
    main()
