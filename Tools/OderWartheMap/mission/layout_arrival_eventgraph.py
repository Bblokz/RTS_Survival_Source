"""Event graph of Mission_OW_Arrival (build_arrival_stage.py builds its functions), hand laid out like the Berlin
stages (BerlinMap/mission/layout_eventgraphs.py: rows left to right, getters under the pin they feed).

Usage: python layout_arrival_eventgraph.py <uecp port>
"""
import sys

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/mission')
from layout_eventgraphs import Graph, var  # noqa: E402
from uecp_client import action  # noqa: E402

BP = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OderWarthe/Mission_OW_Arrival'


def arrival():
    g = Graph()
    g.row([('evStart', 'ev.BP_OnMissionStart', None), ('escort', 'fn.Self.SpawnEscort', None),
           ('tour', 'fn.Self.PlayArrivalTour', None)])
    g.end_row()

    g.row([('evShot', 'ev.BP_OnCameraTourShotStarted', None), ('isColumn', 'fn.Self.IsColumnShot', None),
           ('brColumn', 'k2.Branch', None), ('releaseOnShot', 'fn.Self.ReleaseColumn', None)])
    g.wire('evShot.ShotIndex', 'isColumn.ShotIndex')
    g.wire('isColumn.bIsColumnShot', 'brColumn.Condition')
    g.end_row()

    g.row([('evTourDone', 'ev.BP_OnCameraTourFinished', None), ('setTourDone', 'var.set.bTourFinished', None),
           ('brReleased', 'k2.Branch', None), ('completeReleased', 'fn.MissionBase.OnMissionComplete', None)])
    g.default('setTourDone', 'bTourFinished', True)
    g.getters('brReleased', [('getReleased', var('bColumnReleased'), 'Condition', 'bColumnReleased')])
    bx, by = g.pos['brReleased']
    g.row([('brRetrying', 'k2.Branch', None), ('releaseAfterTour', 'fn.Self.ReleaseColumn', {'no_chain': True})],
          x=bx + 220, y=by + 190)
    g.wire('brReleased.else', 'brRetrying.execute')
    g.wire('brRetrying.else', 'releaseAfterTour.execute')
    g.getters('brRetrying', [('getRetrying', var('bReleaseRetryScheduled'), 'Condition', 'bReleaseRetryScheduled')])
    g.end_row()

    g.row([('evRelease', 'ev.CustomEvent', {'custom_name': 'ReleaseColumn'}),
           ('tryDrive', 'fn.Self.TryDriveColumnUp', None), ('brDone', 'k2.Branch', None),
           ('brTour', 'k2.Branch', None), ('completeDone', 'fn.MissionBase.OnMissionComplete', None)])
    g.wire('tryDrive.bDone', 'brDone.Condition')
    g.getters('brTour', [('getTourDone', var('bTourFinished'), 'Condition', 'bTourFinished')])
    dx, dy = g.pos['brDone']
    g.row([('setRetrying', 'var.set.bReleaseRetryScheduled', None),
           ('schedRetry', 'fn.MissionBase.ScheduleSingleCallback', None)], x=dx + 40, y=g.bottom + 50)
    g.wire('brDone.else', 'setRetrying.execute')
    g.default('setRetrying', 'bReleaseRetryScheduled', True)
    g.wire('evRelease.OutputDelegate', 'schedRetry.Callback')
    g.getters('schedRetry', [('getRetrySecs', var('ReleaseRetrySeconds'), 'DelaySeconds', 'ReleaseRetrySeconds')])
    g.end_row()

    g.comment('Briefing: two light armoured cars join the deck on the Berlin road, then the skippable arrival tour '
              '(title over the lakes, the bunker line, the five Werke, the column)',
              ['evStart', 'escort', 'tour'])
    g.comment('The last Werk shot starts: the column sets off up the road, so the column shot shows it climbing',
              ['evShot', 'isColumn', 'brColumn', 'releaseOnShot'])
    g.comment('Tour over (also after a skip): column already on its way? complete. Otherwise send it now (unless a '
              'retry for the deck is already running)',
              ['evTourDone', 'setTourDone', 'brReleased', 'completeReleased', 'getReleased', 'brRetrying',
               'releaseAfterTour', 'getRetrying'])
    g.comment('Drive the deck and escort up (waits for the deck to spawn); complete once the tour is over too',
              ['evRelease', 'tryDrive', 'brDone', 'brTour', 'completeDone', 'getTourDone', 'setRetrying',
               'schedRetry', 'getRetrySecs'])
    return g


def main():
    port = int(sys.argv[1])
    # A call to a custom event resolves only once the event exists in a compiled class: build it alone first.
    stub = Graph()
    stub.row([('evRelease', 'ev.CustomEvent', {'custom_name': 'ReleaseColumn'})])
    first = action(port, 'build_blueprint_graph', {
        'blueprint_path': BP, 'graph_name': 'EventGraph', 'clear_before_build': True, 'clear_force': True,
        'nodes': stub.nodes, 'connections': [], 'defaults': [], 'comments': []})
    print('stub', first.get('ok') if isinstance(first, dict) else str(first)[:200])
    action(port, 'compile_blueprint', {'blueprint_path': BP})
    g = arrival()
    result = action(port, 'build_blueprint_graph', {
        'blueprint_path': BP, 'graph_name': 'EventGraph', 'clear_before_build': True, 'clear_force': True,
        'nodes': g.nodes, 'connections': g.connections, 'defaults': g.defaults, 'comments': g.comments})
    summary = {k: result.get(k) for k in ('ok', 'nodes', 'connections', 'defaults')} if isinstance(result, dict) \
        else str(result)[:300]
    print('EventGraph', summary)
    if isinstance(result, dict) and not result.get('ok'):
        print('   ', str(result)[:1500])
    compiled = action(port, 'compile_blueprint', {'blueprint_path': BP})
    print('compile errors', compiled.get('error_count') if isinstance(compiled, dict) else compiled,
          str(compiled.get('errors', ''))[:800] if isinstance(compiled, dict) else '')


if __name__ == '__main__':
    main()
