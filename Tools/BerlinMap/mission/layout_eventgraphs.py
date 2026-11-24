"""Rebuild the Berlin stage EventGraphs with a compact, hand-defined layout (build_blueprint_graph, explicit x/y).

Rows read left to right. A scheduled wait's custom event sits right after its Schedule node on the same row, so the
red delegate wire is short and the timeline continues in one line. Getters sit just below the node they feed.
Usage: python layout_eventgraphs.py <port> [BlueprintName ...]
"""
import sys

from uecp_client import action

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/'
GAP = 50
GETTER_DY = 160
GETTER_STEP = 60
ROW = 330


def width(handle):
    if handle == 'ev.CustomEvent':
        return 230
    if handle.startswith('ev.'):
        return 300
    if handle == 'k2.Branch':
        return 170
    if handle.startswith('var.set.'):
        return 230
    if any(k in handle for k in ('ScheduleRepeated', 'RegisterCallbackOnNomadic', 'GetIsNomadicDeployed',
                                 'AddCustomArchiveItem', 'CancelScheduled')):
        return 340
    return 300


def height(handle):
    if handle == 'ev.BP_OnTriggerAreaCallback':
        return 140
    if handle == 'ev.CustomEvent':
        return 70
    if handle.startswith('ev.'):
        return 110
    if handle == 'k2.Branch':
        return 90
    if handle.startswith('var.set.'):
        return 80
    if 'ScheduleRepeated' in handle:
        return 300
    if any(k in handle for k in ('ScheduleSingle', 'RegisterCallbackOnNomadic', 'AddCustomArchiveItem')):
        return 170
    return 115


ROW_GAP = 120  # room for the next row's comment header


class Graph:
    def __init__(self):
        self.nodes, self.connections, self.defaults, self.comments = [], [], [], []
        self.pos = {}
        self.handles = {}
        self.cursor_y = 0
        self.bottom = 0

    def _place(self, node_id, handle, x, y, extra=None):
        node = {'id': node_id, 'handle': handle, 'x': int(x), 'y': int(y)}
        if extra and 'custom_name' in extra:
            node['custom_name'] = extra['custom_name']
        self.nodes.append(node)
        self.pos[node_id] = (x, y)
        self.handles[node_id] = handle
        self.bottom = max(self.bottom, y + height(handle))

    def row(self, items, x=0, y=None):
        """A left-to-right exec row at the cursor (or y). Consecutive nodes are exec-chained unless 'no_chain'."""
        y = self.cursor_y if y is None else y
        prev = None
        for node_id, handle, extra in items:
            extra = extra or {}
            self._place(node_id, handle, x, y, extra)
            if prev and not extra.get('no_chain'):
                self.connections.append({'from': f'{prev}.{extra.get("chain_from_pin", "then")}',
                                         'to': f'{node_id}.execute'})
            prev = node_id
            x += width(handle) + GAP
        return x

    def below(self, anchor, items, dx=0, gap=40):
        """A short exec row starting just below an existing node (branch arms)."""
        ax, _ = self.pos[anchor]
        return self.row(items, x=ax + dx, y=self.bottom + gap)

    def end_row(self):
        self.cursor_y = self.bottom + ROW_GAP

    def getters(self, consumer, items, dx=-20):
        """items: (id, handle, consumer pin, output pin). Stacked just below the consumer's real height."""
        cx, cy = self.pos[consumer]
        top = cy + height(self.handles[consumer]) + 25
        for index, (node_id, handle, to_pin, out_pin) in enumerate(items):
            self._place(node_id, handle, cx + dx, top + 55 * index)
            self.connections.append({'from': f'{node_id}.{out_pin}', 'to': f'{consumer}.{to_pin}'})

    def wire(self, src, dst):
        self.connections.append({'from': src, 'to': dst})

    def default(self, node_id, pin, value):
        self.defaults.append({'node_id': node_id, 'pin_name': pin, 'value': value})

    def comment(self, text, node_ids):
        self.comments.append({'text': text, 'node_ids': node_ids})


def var(name):
    return f'var.get.{name}'


def schedule_defaults(g, node_id):
    for pin, value in (('TotalCalls', 1), ('IntervalSeconds', 1), ('InitialDelaySeconds', 1),
                       ('bFireBeforeFirstInterval', False), ('bRepeatForever', True)):
        g.default(node_id, pin, value)


def arrival():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('intro', 'fn.Self.ShowBerlinLocationIntro', None)])
    g.end_row()
    g.row([('evIntroDone', 'ev.BP_OnLocationIntroFinished', None),
           ('playCrater', 'fn.Self.PlayAdvisorLine', None),
           ('schedCrater', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evCraterDone', 'ev.CustomEvent', {'custom_name': 'OnCraterLineDone', 'no_chain': True}),
           ('tour', 'fn.Self.PlayLeitturmTour', None)])
    g.getters('playCrater', [('getCrater', var('CraterLine'), 'Line', 'CraterLine')])
    g.getters('schedCrater', [('getCraterSecs', var('CraterLineSeconds'), 'DelaySeconds', 'CraterLineSeconds')])
    g.wire('evCraterDone.OutputDelegate', 'schedCrater.Callback')
    g.end_row()
    g.row([('evTourDone', 'ev.BP_OnCameraTourFinished', None),
           ('playQuiet', 'fn.Self.PlayAdvisorLine', None),
           ('callTryHQ', 'fn.Self.TryWatchPlayerHQ', None)])
    g.getters('playQuiet', [('getQuiet', var('StayQuietLine'), 'Line', 'StayQuietLine')])
    g.end_row()
    g.row([('evTryHQ', 'ev.CustomEvent', {'custom_name': 'TryWatchPlayerHQ'}),
           ('findHQ', 'fn.Self.FindPlayerHQ', None),
           ('brFound', 'k2.Branch', None),
           ('brDeployed', 'k2.Branch', None),
           ('completeNow', 'fn.MissionBase.OnMissionComplete', None)])
    g.wire('findHQ.bFound', 'brFound.Condition')
    row_y = g.pos['evTryHQ'][1]
    fx = g.pos['findHQ'][0]
    g._place('isDeployed', 'fn.MissionBase.GetIsNomadicDeployedAsBuilding', fx + 120, row_y + 150)
    g.wire('isDeployed.ReturnValue', 'brDeployed.Condition')
    g._place('getHQ1', var('PlayerHQ'), fx - 80, row_y + 200)
    g.wire('getHQ1.PlayerHQ', 'isDeployed.NomadicVehicle')
    cx = g.pos['completeNow'][0]
    g.row([('watchHQ', 'fn.MissionBase.RegisterCallbackOnNomadicVehicle', None)], x=cx, y=row_y + 150)
    g.wire('brDeployed.else', 'watchHQ.execute')
    g.getters('watchHQ', [('getHQ2', var('PlayerHQ'), 'NomadicVehicle', 'PlayerHQ')])
    g.default('watchHQ', 'bCallBackOnBuildingConversion', True)
    bx = g.pos['brDeployed'][0]
    g.row([('missingHQ', 'fn.Self.HandleMissingHQ', None),
           ('schedRetry', 'fn.MissionBase.ScheduleSingleCallback', None)], x=bx - 60, y=g.bottom + 50)
    g.wire('brFound.else', 'missingHQ.execute')
    g.wire('evTryHQ.OutputDelegate', 'schedRetry.Callback')
    g.getters('schedRetry', [('getRetry', var('HQSearchRetrySeconds'), 'DelaySeconds', 'HQSearchRetrySeconds')])
    g.end_row()
    g.row([('evDeployed', 'ev.BP_OnCallBackNomadicVehicle', None),
           ('completeDeployed', 'fn.MissionBase.OnMissionComplete', None)])
    g.end_row()
    g.comment('Briefing: location intro, crater line, wait, Zoo tower tour with the Leitturm line',
              ['evStart', 'intro', 'evIntroDone', 'playCrater', 'schedCrater', 'evCraterDone', 'tour',
               'getCrater', 'getCraterSecs'])
    g.comment('After the tour: stay-quiet line, then watch the HQ', ['evTourDone', 'playQuiet', 'callTryHQ', 'getQuiet'])
    g.comment('Find the HQ every second (a glider brings one after MaxHQSearchAttempts); complete once it is deployed',
              ['evTryHQ', 'findHQ', 'brFound', 'brDeployed', 'completeNow', 'isDeployed', 'getHQ1', 'watchHQ',
               'getHQ2', 'missingHQ', 'schedRetry', 'getRetry'])
    g.comment('The HQ deployed: the stage is complete', ['evDeployed', 'completeDeployed'])
    return g


def reach_leitturm():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('mkTrigger', 'fn.Self.CreateZooApproachTrigger', None)])
    g.end_row()
    g.row([('evTrigger', 'ev.BP_OnTriggerAreaCallback', None), ('br', 'k2.Branch', None),
           ('play', 'fn.Self.PlayAdvisorLine', None), ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('br', [('eq', 'fn.KismetMathLibrary.EqualEqual_IntInt', 'Condition', 'ReturnValue')], dx=-40)
    g.wire('evTrigger.TriggerID', 'eq.A')
    ex, ey = g.pos['eq']
    g._place('getId', var('ZooTriggerId'), ex - 190, ey + 40)
    g.wire('getId.ZooTriggerId', 'eq.B')
    g.getters('play', [('getLine', var('LeitturmReachedLine'), 'Line', 'LeitturmReachedLine')])
    g.end_row()
    g.comment('Make the Zoo approach circle', ['evStart', 'mkTrigger'])
    g.comment('A player unit enters it: line (if recorded), complete. HoldRelay starts on this.',
              ['evTrigger', 'br', 'eq', 'getId', 'play', 'getLine', 'complete'])
    return g


def hold_relay():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('mkTrigger', 'fn.Self.CreateBunkerTrigger', None)])
    g.end_row()
    g.row([('evTrigger', 'ev.BP_OnTriggerAreaCallback', None), ('should', 'fn.Self.ShouldStartTransfer', None),
           ('brStart', 'k2.Branch', None), ('setStarted', 'var.set.bTransferStarted', None),
           ('playStart', 'fn.Self.PlayAdvisorLine', None),
           ('schedTick', 'fn.MissionBase.ScheduleRepeatedCallback', None),
           ('setTask', 'var.set.TransferTaskId', None)])
    g.wire('evTrigger.OverlappingActor', 'should.OverlappingActor')
    g.wire('evTrigger.TriggerID', 'should.TriggerID')
    g.wire('should.bStart', 'brStart.Condition')
    g.default('setStarted', 'bTransferStarted', True)
    g.getters('playStart', [('getStartLine', var('TransferStartLine'), 'Line', 'TransferStartLine')])
    schedule_defaults(g, 'schedTick')
    g.wire('schedTick.ReturnValue', 'setTask.TransferTaskId')
    g.end_row()
    g.row([('evTick', 'ev.CustomEvent', {'custom_name': 'OnTransferTick'}),
           ('update', 'fn.Self.UpdateTransfer', None),
           ('brDone', 'k2.Branch', None),
           ('cancelTick', 'fn.MissionBase.CancelScheduledCallback', None),
           ('playSecured', 'fn.Self.PlayAdvisorLine', None),
           ('schedVictory', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evSecured', 'ev.CustomEvent', {'custom_name': 'OnDataSecured', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None),
           ('victory', 'fn.MissionManager.TriggerVictory', None)])
    g.wire('evTick.OutputDelegate', 'schedTick.Callback')
    g.wire('update.bTransferDone', 'brDone.Condition')
    g.getters('cancelTick', [('getTask', var('TransferTaskId'), 'TaskID', 'TransferTaskId')])
    g.getters('playSecured', [('getSecuredLine', var('DataSecuredLine'), 'Line', 'DataSecuredLine')])
    g.getters('schedVictory', [('getVicDelay', var('VictoryDelaySeconds'), 'DelaySeconds', 'VictoryDelaySeconds')])
    g.wire('evSecured.OutputDelegate', 'schedVictory.Callback')
    g.getters('victory', [('getMgr', 'fn.MissionBase.GetMissionManagerChecked', 'self', 'ReturnValue')])
    g.default('victory', 'bShowBackToMenu', True)
    g.end_row()
    g.comment('Make the bunker trigger', ['evStart', 'mkTrigger'])
    g.comment('First player infantry in the bunker starts the transfer; it ticks every second',
              ['evTrigger', 'should', 'brStart', 'setStarted', 'playStart', 'getStartLine', 'schedTick', 'setTask'])
    g.comment('Each tick: one more uncontested second; transfer full? stop ticking, line, wait, complete and win',
              ['evTick', 'update', 'brDone', 'cancelTick', 'getTask', 'playSecured', 'getSecuredLine', 'schedVictory',
               'getVicDelay', 'evSecured', 'complete', 'victory', 'getMgr'])
    return g


def stay_undetected():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('zones', 'fn.Self.CreateDetectionZoneTriggers', None),
           ('bunker', 'fn.Self.CreateRelayBunkerTrigger', None), ('fallback', 'fn.Self.StartEnemyFallback', None)])
    g.end_row()
    g.row([('evTrigger', 'ev.BP_OnTriggerAreaCallback', None), ('isAlarm', 'fn.Self.IsAlarmOverlap', None),
           ('br', 'k2.Branch', None), ('raise', 'fn.Self.RaiseAlarm', None)])
    g.wire('evTrigger.OverlappingActor', 'isAlarm.OverlappingActor')
    g.wire('evTrigger.TriggerID', 'isAlarm.TriggerID')
    g.wire('isAlarm.bRaisesAlarm', 'br.Condition')
    g.end_row()
    g.comment('Alarm sources: detection zones (ids 100+), the relay bunker (1), fleeing enemies at the Reichstag '
              'forecourt (2). Also starts the enemy fallback watch.', ['evStart', 'zones', 'bunker', 'fallback'])
    g.comment('Alarm = this stage FAILS (ResponseForce starts; RailYard and StopRunners fail)',
              ['evTrigger', 'isAlarm', 'br', 'raise'])
    return g


def response_force():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('alarmLine', 'fn.Self.PlayAlarmLine', None),
           ('sabotage', 'fn.Self.ApplyRailYardSabotage', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evHold', 'ev.CustomEvent', {'custom_name': 'OnHoldOver', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.wire('sabotage.HoldSeconds', 'sched.DelaySeconds')
    g.wire('evHold.OutputDelegate', 'sched.Callback')
    g.end_row()
    g.comment('Alarm line, hold (longer if the rail yard was sabotaged), complete: Counterattack starts on this',
              ['evStart', 'alarmLine', 'sabotage', 'sched', 'evHold', 'complete'])
    return g


def stop_runners():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('playFlee', 'fn.Self.PlayAdvisorLine', None),
           ('firstRefresh', 'fn.Self.RefreshRunners', None),
           ('schedRefresh', 'fn.MissionBase.ScheduleRepeatedCallback', None),
           ('setTask', 'var.set.RefreshTaskId', None)])
    g.getters('playFlee', [('getFleeLine', var('RunnerFleeingLine'), 'Line', 'RunnerFleeingLine')])
    schedule_defaults(g, 'schedRefresh')
    g.wire('schedRefresh.ReturnValue', 'setTask.RefreshTaskId')
    g.end_row()
    g.row([('evRefresh', 'ev.CustomEvent', {'custom_name': 'OnRefreshRunners'}),
           ('refresh', 'fn.Self.RefreshRunners', None), ('brStopped', 'k2.Branch', None),
           ('playStopped', 'fn.Self.PlayAdvisorLine', None)])
    g.wire('evRefresh.OutputDelegate', 'schedRefresh.Callback')
    g.wire('refresh.bAllRunnersJustStopped', 'brStopped.Condition')
    g.getters('playStopped', [('getStoppedLine', var('RunnersStoppedLine'), 'Line', 'RunnersStoppedLine')])
    g.end_row()
    g.row([('evOther', 'ev.BP_OnOtherMissionEnded', None), ('isAlarm', 'fn.Self.IsAlarmEnding', None),
           ('brAlarm', 'k2.Branch', None), ('cancel', 'fn.MissionBase.CancelScheduledCallback', None),
           ('fail', 'fn.MissionBase.OnMissionFailed', None)])
    g.wire('isAlarm.bIsAlarm', 'brAlarm.Condition')
    g.getters('cancel', [('getTask', var('RefreshTaskId'), 'TaskID', 'RefreshTaskId')])
    g.end_row()
    g.comment('First fleeing enemy: line, count the runners, then refresh every second',
              ['evStart', 'playFlee', 'getFleeLine', 'firstRefresh', 'schedRefresh', 'setTask'])
    g.comment('Each refresh: count, minimap markers, text; line when the last runner died',
              ['evRefresh', 'refresh', 'brStopped', 'playStopped', 'getStoppedLine'])
    g.comment('The alarm (StayUndetected failed): stop refreshing and fail',
              ['evOther', 'isAlarm', 'brAlarm', 'cancel', 'getTask', 'fail'])
    return g


def cellar_people():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('reveal', 'fn.Self.RevealDetectionZones', None), ('give', 'fn.Self.GiveBurrowerSquad', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnContactDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('play', [('getLine', var('CellarLine'), 'Line', 'CellarLine')])
    g.getters('sched', [('getDelay', var('CompleteDelaySeconds'), 'DelaySeconds', 'CompleteDelaySeconds')])
    g.wire('evDone.OutputDelegate', 'sched.Callback')
    g.end_row()
    g.comment('Contact with the Burrowers: line, detection zones on the minimap, one Burrower squad, complete',
              ['evStart', 'play', 'getLine', 'reveal', 'give', 'sched', 'getDelay', 'evDone', 'complete'])
    return g


def rail_yard():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('setup', 'fn.Self.SetUpSabotageTargets', None),
           ('text', 'fn.Self.UpdateSabotageText', None), ('isLate', 'fn.Self.IsAlarmEnding', None),
           ('brLate', 'k2.Branch', None), ('failLate', 'fn.MissionBase.OnMissionFailed', None)])
    g.wire('isLate.bIsAlarm', 'brLate.Condition')
    g.end_row()
    g.row([('evScav', 'ev.BP_OnCallBackScavengableObjectScavenged', None),
           ('mark', 'fn.Self.MarkTargetSabotaged', None), ('brAll', 'k2.Branch', None),
           ('play', 'fn.Self.PlayAdvisorLine', None), ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnSabotageDone', 'no_chain': True}),
           ('brNotDone', 'k2.Branch', None), ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.wire('mark.bAllSabotaged', 'brAll.Condition')
    g.getters('play', [('getLine', var('SabotagedLine'), 'Line', 'SabotagedLine')])
    g.getters('sched', [('getDelay', var('CompleteDelaySeconds'), 'DelaySeconds', 'CompleteDelaySeconds')])
    g.wire('evDone.OutputDelegate', 'sched.Callback')
    g.getters('brNotDone', [('not', 'fn.KismetMathLibrary.Not_PreBool', 'Condition', 'ReturnValue')], dx=-40)
    nx, ny = g.pos['not']
    g._place('isComplete', 'fn.MissionBase.GetIsMissionComplete', nx - 230, ny + 40)
    g.wire('isComplete.ReturnValue', 'not.A')
    g.end_row()
    g.row([('evOther', 'ev.BP_OnOtherMissionEnded', None), ('isAlarm', 'fn.Self.IsAlarmEnding', None),
           ('brAlarm', 'k2.Branch', None), ('fail', 'fn.MissionBase.OnMissionFailed', None)])
    g.wire('isAlarm.bIsAlarm', 'brAlarm.Condition')
    g.end_row()
    g.comment('Register the two targets; if the plate only appears after the alarm, fail at once',
              ['evStart', 'setup', 'text', 'isLate', 'brLate', 'failLate'])
    g.comment('Scavengers sabotage the ramp and the switch tower; both done: line, wait, complete',
              ['evScav', 'mark', 'brAll', 'play', 'getLine', 'sched', 'getDelay', 'evDone', 'brNotDone', 'not',
               'isComplete', 'complete'])
    g.comment('The alarm before both are done: the bonus fails', ['evOther', 'isAlarm', 'brAlarm', 'fail'])
    return g


def raid_reichstag():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('track', 'fn.Self.TrackFactoryTargets', None)])
    g.end_row()
    g.row([('evMax', 'ev.BP_OnTrackingReachedMaxCount', None), ('play', 'fn.Self.PlayAdvisorLine', None),
           ('sched', 'fn.MissionBase.ScheduleSingleCallback', None),
           ('evDone', 'ev.CustomEvent', {'custom_name': 'OnRaidDone', 'no_chain': True}),
           ('complete', 'fn.MissionBase.OnMissionComplete', None)])
    g.getters('play', [('getLine', var('RaidedLine'), 'Line', 'RaidedLine')])
    g.getters('sched', [('getDelay', var('CompleteDelaySeconds'), 'DelaySeconds', 'CompleteDelaySeconds')])
    g.wire('evDone.OutputDelegate', 'sched.Callback')
    g.end_row()
    g.comment('Track every actor tagged FactoryTag', ['evStart', 'track'])
    g.comment('All destroyed: line, wait, complete', ['evMax', 'play', 'getLine', 'sched', 'getDelay', 'evDone',
                                                      'complete'])
    return g


def demeter_pod():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('archive', 'fn.MissionBase.AddCustomArchiveItem', None),
           ('play', 'fn.Self.PlayAdvisorLine', None)])
    g.getters('archive', [('getEntry', var('DemeterArchive'), 'Content', 'DemeterArchive')])
    g.getters('play', [('getLine', var('DemeterLine'), 'Line', 'DemeterLine')])
    g.end_row()
    g.comment('Found the pod: Project Demeter archive entry and line (the text-only plate completes by itself)',
              ['evStart', 'archive', 'getEntry', 'play', 'getLine'])
    return g


GRAPHS = {
    'Mission_Berlin_Arrival': arrival, 'Mission_Berlin_ReachLeitturm': reach_leitturm,
    'Mission_Berlin_HoldRelay': hold_relay, 'Mission_Berlin_StayUndetected': stay_undetected,
    'Mission_Berlin_ResponseForce': response_force, 'Mission_Berlin_StopRunners': stop_runners,
    'Mission_Berlin_CellarPeople': cellar_people, 'Mission_Berlin_RailYard': rail_yard,
    'Mission_Berlin_RaidReichstag': raid_reichstag, 'Mission_Berlin_DemeterPod': demeter_pod,
}


def main():
    port = int(sys.argv[1])
    for name in sys.argv[2:] or list(GRAPHS):
        g = GRAPHS[name]()
        result = action(port, 'build_blueprint_graph', {
            'blueprint_path': BASE + name, 'graph_name': 'EventGraph', 'clear_before_build': True,
            'clear_force': True, 'nodes': g.nodes, 'connections': g.connections, 'defaults': g.defaults,
            'comments': g.comments})
        summary = {k: result.get(k) for k in ('ok', 'nodes', 'connections', 'defaults')} if isinstance(result, dict) \
            else str(result)[:300]
        print(name, summary)
        if isinstance(result, dict) and not result.get('ok'):
            print('   ', str(result)[:800])
        compiled = action(port, 'compile_blueprint', {'blueprint_path': BASE + name})
        print('   compile errors', compiled.get('error_count') if isinstance(compiled, dict) else compiled)


if __name__ == '__main__':
    main()
