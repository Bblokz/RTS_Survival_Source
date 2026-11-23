"""Compact event-graph layout for the Frozen Lake stage Blueprints (same rules as Berlin's layout_eventgraphs.py).

Rows read left to right. A scheduled wait's custom event sits right after its Schedule node on the same row, so the
delegate wire stays short. Getters sit just below the pin they feed. One comment box per flow.
"""

GAP = 60
# set per Blueprint before building its graph: 'Target is Mission FL 07 Haul Core'
SELF_SUBTITLE = ''
ROW_GAP = 120  # room for the next row's comment header


def _title(handle):
    """Display title of a node handle: fn.Self.ShowRevealCamera -> 'Show Reveal Camera'."""
    import re
    name = handle.rsplit('.', 1)[-1]
    return re.sub(r'(?<=[a-z0-9])(?=[A-Z])', ' ', name)


def width(handle):
    """Real node widths at zoom 1:1 (measured from render_graph_image), so the gaps between nodes stay even."""
    if handle == 'ev.CustomEvent':
        return 160
    if handle.startswith('ev.'):
        return max(220, int(7.5 * len(_title(handle))) + 70)
    if handle == 'k2.Branch':
        return 180
    if handle.startswith('var.set.'):
        return 230
    if handle == 'fn.Self.PlayLine':
        return max(290, int(6.6 * len(SELF_SUBTITLE)) + 70)
    if 'ScheduleSingle' in handle:
        return 240
    width_by_title = int(8 * len(_title(handle))) + 80
    if handle.startswith('fn.Self.'):
        # the subtitle "Target is <Blueprint name, spaced>" is often wider than the title
        return max(240, width_by_title, int(6.6 * len(SELF_SUBTITLE)) + 70)
    if handle.startswith('fn.MissionBase.'):
        return max(240, width_by_title, int(6.6 * len('Target is Mission Base')) + 70)
    return max(240, width_by_title)


def height(handle):
    if handle == 'ev.BP_OnTriggerAreaCallback':
        return 140
    if handle in ('ev.BP_OnAsyncSpawnComplete', 'ev.BP_OnOtherMissionEnded'):
        return 140
    if handle == 'ev.CustomEvent':
        return 70
    if handle.startswith('ev.'):
        return 110
    if handle == 'k2.Branch':
        return 90
    if handle.startswith('var.set.'):
        return 80
    if 'ScheduleSingle' in handle:
        return 170
    if handle == 'fn.Self.PlayLine':
        return 200
    return 115


def var(name):
    return f'var.get.{name}'


class Graph:
    def __init__(self):
        self.nodes, self.connections, self.defaults, self.comments = [], [], [], []
        self.pos, self.handles = {}, {}
        self.cursor_y = 0
        self.bottom = 0

    def place(self, node_id, handle, x, y, custom_name=None):
        node = {'id': node_id, 'handle': handle, 'x': int(x), 'y': int(y)}
        if custom_name:
            node['custom_name'] = custom_name
        self.nodes.append(node)
        self.pos[node_id] = (x, y)
        self.handles[node_id] = handle
        self.bottom = max(self.bottom, y + height(handle))

    def row(self, items, x=0, y=None):
        """items: (id, handle, opts). Consecutive nodes are exec-chained unless opts['no_chain'].
        opts: custom_name, no_chain, chain_from_pin (pin of the previous node, default 'then')."""
        y = self.cursor_y if y is None else y
        prev = None
        for node_id, handle, opts in items:
            opts = opts or {}
            self.place(node_id, handle, x, y, opts.get('custom_name'))
            if prev and not opts.get('no_chain'):
                self.connections.append({'from': f'{prev}.{opts.get("chain_from_pin", "then")}',
                                         'to': f'{node_id}.execute'})
            prev = node_id
            x += width(handle) + GAP
        return x

    def end_row(self):
        self.cursor_y = self.bottom + ROW_GAP

    def getters(self, consumer, items, dx=-20):
        """items: (id, handle, consumer pin, output pin), stacked just below the consumer's real height."""
        cx, cy = self.pos[consumer]
        top = cy + height(self.handles[consumer]) + 25
        for index, (node_id, handle, to_pin, out_pin) in enumerate(items):
            self.place(node_id, handle, cx + dx, top + 55 * index)
            self.connections.append({'from': f'{node_id}.{out_pin}', 'to': f'{consumer}.{to_pin}'})

    def wire(self, src, dst):
        self.connections.append({'from': src, 'to': dst})

    def default(self, node_id, pin, value):
        self.defaults.append({'node_id': node_id, 'pin_name': pin, 'value': value})

    def comment(self, text, node_ids):
        self.comments.append({'text': text, 'node_ids': node_ids})

    # ---- small recurring patterns -------------------------------------------------------------------------------

    def line(self, node_id, line_var, speaker):
        """A PlayLine(Line, Speaker) call that was already placed in a row: its Line getter goes below it."""
        self.getters(node_id, [(node_id + 'Get', var(line_var), 'Line', line_var)])
        self.default(node_id, 'Speaker', speaker)

    def wait(self, sched_id, event_id, seconds_var):
        """Wires a ScheduleSingleCallback to the custom event placed right after it, with its delay getter below."""
        self.wire(f'{event_id}.OutputDelegate', f'{sched_id}.Callback')
        self.getters(sched_id, [(sched_id + 'Secs', var(seconds_var), 'DelaySeconds', seconds_var)])

    def trigger_id_branch(self, event_id, branch_id, id_var):
        """Branch on TriggerID == <id_var>, with the compare node under the branch."""
        self.getters(branch_id, [(branch_id + 'Eq', 'fn.KismetMathLibrary.EqualEqual_IntInt', 'Condition',
                                  'ReturnValue')], dx=-40)
        self.wire(f'{event_id}.TriggerID', f'{branch_id}Eq.A')
        ex, ey = self.pos[branch_id + 'Eq']
        self.place(branch_id + 'Id', var(id_var), ex - 190, ey + 40)
        self.wire(f'{branch_id}Id.{id_var}', f'{branch_id}Eq.B')
        return [branch_id + 'Eq', branch_id + 'Id']
