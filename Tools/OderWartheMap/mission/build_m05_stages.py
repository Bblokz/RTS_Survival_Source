"""Builds the Oder-Warthe stage M05 and its two bonus objectives over UECP (OderWartheMissionFlow.md 10.3 and 14).

Mission_OW_SecondBearing (M05, plate "Cross the Packlitz dam"): the Advisor names the next post (A12), the wave director
starts its timed waves, and the stage completes once DF 2 (the forester's hill) is held.
Mission_OW_BonusAllPosts (bonus "Take all five listening posts"): intro A13; completes when all five stations are held
(the hunt then fixes the command post on its own).
Mission_OW_BonusTunnelMouth (bonus "Collapse a tunnel mouth"): intro A14; completes when the director reports a collapsed
tunnel mouth (that Werk then sends no more waves).
All three start when M04 completes (ow_arrival_editor.wire_later_stages). The radio-truck bonus (A15 recorded) is later.

Usage: python build_m05_stages.py <uecp port> [StageName ...]
"""
import sys

import build_m03_m04_stages as m34
from build_m03_m04_stages import FIND_DF, FIND_PIECES, Graph, bind, build_stage, custom_event, line, var

m34.EVENT_PARAMS['OnTunnelMouthCollapsed'] = [{'name': 'WerkIndex', 'type': 'int'}]

IS_DF2_TAKEN = {
    'function': 'IsDF2Taken',
    'inputs': [{'name': 'StationIndex', 'type': 'int'}, {'name': 'bHeldByPlayer', 'type': 'bool'}],
    'outputs': [{'name': 'bTaken', 'type': 'bool'}],
    'body': [
        {'op': 'branch', 'condition': '$bHeldByPlayer', 'then': [
            {'op': 'return', 'values': {'bTaken': '=$StationIndex == 1'}}]},
        {'op': 'return', 'values': {'bTaken': False}},
    ],
}

IS_DF2_HELD_NOW = {
    'function': 'IsDF2HeldNow',
    'outputs': [{'name': 'bHeld', 'type': 'bool'}],
    'body': [
        {'op': 'is_valid', 'value': '$DFManager', 'then': [
            {'op': 'call', 'func': 'DirectionFindingManager.GetIsStationHeldByPlayer', 'target': '$DFManager',
             'args': {'StationIndex': 1}, 'as': 'bHeldNow'},
            {'op': 'return', 'values': {'bHeld': '$bHeldNow'}}]},
        {'op': 'return', 'values': {'bHeld': False}},
    ],
}

START_WAVES = {
    'function': 'StartTimedWaves',
    'body': [{'op': 'call', 'func': 'WerkWaveDirector.StartTimedWaves', 'target': '$WaveDirector'}],
}

ALL_POSTS_HELD = {
    'function': 'AreAllPostsHeld',
    'outputs': [{'name': 'bAllHeld', 'type': 'bool'}],
    'body': [
        {'op': 'is_valid', 'value': '$DFManager', 'then': [
            {'op': 'call', 'func': 'DirectionFindingManager.GetHeldStationCount', 'target': '$DFManager',
             'as': 'Held'},
            {'op': 'return', 'values': {'bAllHeld': '=$Held >= $StationsToHold'}}]},
        {'op': 'return', 'values': {'bAllHeld': False}},
    ],
}

FIND_DIRECTOR = {
    'function': 'FindWaveDirector',
    'outputs': [{'name': 'bFound', 'type': 'bool'}],
    'body': [
        {'op': 'call', 'func': 'WerkWaveDirector.FindWerkWaveDirector', 'args': {'WorldContextObject': m34.MANAGER},
         'as': 'Director'},
        {'op': 'set_var', 'var': 'WaveDirector', 'value': '$Director'},
        {'op': 'is_valid', 'value': '$Director', 'then': [{'op': 'return', 'values': {'bFound': True}}]},
        {'op': 'return', 'values': {'bFound': False}},
    ],
}

ANY_COLLAPSED = {
    'function': 'IsAnyTunnelMouthCollapsed',
    'outputs': [{'name': 'bCollapsed', 'type': 'bool'}],
    'body': [
        {'op': 'call', 'func': 'WerkWaveDirector.GetCollapsedTunnelMouthCount', 'target': '$WaveDirector',
         'as': 'Collapsed'},
        {'op': 'return', 'values': {'bCollapsed': '=$Collapsed > 0'}},
    ],
}

DF_VAR = {'variable_name': 'DFManager', 'variable_type': 'object:DirectionFindingManager', 'category': 'OW|State'}
DIRECTOR_VAR = {'variable_name': 'WaveDirector', 'variable_type': 'object:WerkWaveDirector', 'category': 'OW|State'}


def intro_var(tooltip):
    return {'variable_name': 'IntroLine', 'variable_type': 'object:SoundBase', 'instance_editable': True,
            'category': 'OW|Lines', 'tooltip': tooltip}


M05 = {
    'name': 'Mission_OW_SecondBearing',
    'variables': [DF_VAR, DIRECTOR_VAR, intro_var('Advisor: the next post is across the Packlitz dam (A12).')],
    'defaults': {'IntroLine': line('OderWarthe_A12_PacklitzDam')},
    'functions': [FIND_DF, FIND_PIECES, IS_DF2_TAKEN, IS_DF2_HELD_NOW, START_WAVES],
}

BONUS_POSTS = {
    'name': 'Mission_OW_BonusAllPosts',
    'variables': [DF_VAR, intro_var('Advisor: hold all five posts and he has nowhere to hide (A13).'),
                  {'variable_name': 'StationsToHold', 'variable_type': 'int', 'default_value': '5',
                   'instance_editable': True, 'category': 'OW|Bonus'}],
    'defaults': {'IntroLine': line('OderWarthe_A13_AllFivePosts')},
    'functions': [FIND_DF, ALL_POSTS_HELD],
}

BONUS_TUNNEL = {
    'name': 'Mission_OW_BonusTunnelMouth',
    'variables': [DIRECTOR_VAR, intro_var('Advisor: collapse a tunnel mouth with heavy guns (A14).')],
    'defaults': {'IntroLine': line('OderWarthe_A14_TunnelMouths')},
    'functions': [FIND_DIRECTOR, ANY_COLLAPSED],
}


def intro_row(g, find_handle, extra):
    """Start: intro line on the Advisor portrait, find the pieces, then `extra` (exec-chained) when found."""
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('intro', 'fn.MissionBase.PlayPortrait', None),
           ('find', find_handle, None), ('brFound', 'k2.Branch', None)] + extra)
    g.default('intro', 'PortraitType', 'GermanAnnouncer')
    g.getters('intro', [('getIntro', var('IntroLine'), 'VoiceLine', 'IntroLine')])
    g.wire('find.bFound', 'brFound.Condition')


def done_row(g):
    g.row([custom_event('evDone', 'HandleObjectiveDone', []), ('once', 'k2.DoOnce', None),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.connections = [c for c in g.connections if c['to'] not in ('once.execute', 'complete.execute')]
    g.wire('evDone.then', 'once.Execute')
    g.wire('once.Completed', 'complete.execute')
    g.end_row()


def m05_graph():
    g = Graph()
    intro_row(g, 'fn.Self.FindWavePieces', [
        ('waves', 'fn.Self.StartTimedWaves', None),
        ('bindStation', 'ev.DispatcherBind.DirectionFindingManager.OnStationChanged', None),
        ('heldNow', 'fn.Self.IsDF2HeldNow', None), ('brHeld', 'k2.Branch', None),
        ('doneNow', 'fn.Self.HandleObjectiveDone', None)])
    g.wire('heldNow.bHeld', 'brHeld.Condition')
    bind(g, 'bindStation', 'DFManager', 'OnStationChanged')
    g.end_row()
    g.row([custom_event('evChanged', 'OnStationChanged', []), ('isTaken', 'fn.Self.IsDF2Taken', None),
           ('brTaken', 'k2.Branch', None), ('done', 'fn.Self.HandleObjectiveDone', None)])
    g.wire('evChanged.StationIndex', 'isTaken.StationIndex')
    g.wire('evChanged.bHeldByPlayer', 'isTaken.bHeldByPlayer')
    g.wire('isTaken.bTaken', 'brTaken.Condition')
    g.end_row()
    done_row(g)
    g.comment('The next post: Advisor line (A12), timed waves start, watch DF 2 (it may already be held)',
              ['evStart', 'intro', 'getIntro', 'find', 'brFound', 'waves', 'bindStation', 'bindStation_target',
               'bindStation_mk', 'heldNow', 'brHeld', 'doneNow'])
    g.comment('A station changed hands: DF 2 taken by the player?', ['evChanged', 'isTaken', 'brTaken', 'done'])
    g.comment('DF 2 held: the stage is done', ['evDone', 'once', 'complete'])
    m34.apply_event_params(g)
    return g


def bonus_posts_graph():
    g = Graph()
    intro_row(g, 'fn.Self.FindDirectionFinding', [
        ('bindStation', 'ev.DispatcherBind.DirectionFindingManager.OnStationChanged', None),
        ('heldNow', 'fn.Self.AreAllPostsHeld', None), ('brHeld', 'k2.Branch', None),
        ('doneNow', 'fn.Self.HandleObjectiveDone', None)])
    g.wire('heldNow.bAllHeld', 'brHeld.Condition')
    bind(g, 'bindStation', 'DFManager', 'OnStationChanged')
    g.end_row()
    g.row([custom_event('evChanged', 'OnStationChanged', []), ('allHeld', 'fn.Self.AreAllPostsHeld', None),
           ('brAll', 'k2.Branch', None), ('done', 'fn.Self.HandleObjectiveDone', None)])
    g.wire('allHeld.bAllHeld', 'brAll.Condition')
    g.end_row()
    done_row(g)
    g.comment('Bonus intro (A13), then watch the stations', ['evStart', 'intro', 'getIntro', 'find', 'brFound',
                                                              'bindStation', 'bindStation_target', 'bindStation_mk',
                                                              'heldNow', 'brHeld', 'doneNow'])
    g.comment('A station changed hands: all five held?', ['evChanged', 'allHeld', 'brAll', 'done'])
    g.comment('All five held: bonus done', ['evDone', 'once', 'complete'])
    m34.apply_event_params(g)
    return g


def bonus_tunnel_graph():
    g = Graph()
    intro_row(g, 'fn.Self.FindWaveDirector', [
        ('bindCollapse', 'ev.DispatcherBind.WerkWaveDirector.OnTunnelMouthCollapsed', None),
        ('anyNow', 'fn.Self.IsAnyTunnelMouthCollapsed', None), ('brAny', 'k2.Branch', None),
        ('doneNow', 'fn.Self.HandleObjectiveDone', None)])
    g.wire('anyNow.bCollapsed', 'brAny.Condition')
    bind(g, 'bindCollapse', 'WaveDirector', 'OnTunnelMouthCollapsed')
    g.end_row()
    g.row([custom_event('evCollapsed', 'OnTunnelMouthCollapsed', []), ('done', 'fn.Self.HandleObjectiveDone', None)])
    g.end_row()
    done_row(g)
    g.comment('Bonus intro (A14), then wait for a collapsed tunnel mouth (one may already be down)',
              ['evStart', 'intro', 'getIntro', 'find', 'brFound', 'bindCollapse', 'bindCollapse_target',
               'bindCollapse_mk', 'anyNow', 'brAny', 'doneNow'])
    g.comment('A tunnel mouth collapsed: bonus done', ['evCollapsed', 'done'])
    g.comment('Done once', ['evDone', 'once', 'complete'])
    m34.apply_event_params(g)
    return g


STAGES = ((M05, m05_graph), (BONUS_POSTS, bonus_posts_graph), (BONUS_TUNNEL, bonus_tunnel_graph))


def main():
    port = int(sys.argv[1])
    only = sys.argv[2:]
    for spec, graph in STAGES:
        if not only or spec['name'] in only:
            build_stage(port, spec, graph())


if __name__ == '__main__':
    main()
