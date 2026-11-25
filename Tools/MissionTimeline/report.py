"""Pacing checks and the HTML report for a simulated mission timeline (called by simulate.py).

pacing_checks(data) -> list of warnings ({t, end, kind, text}); thresholds in PACING (simulate.py --pace key=value).
render_html(data)   -> one self-contained page: summary + warnings, the lane chart (time left to right), the
                       stage-flow graph (what starts what, placed at its start time) and the full event list.
"""
import html
import math

PACING = {
    'burst_window_s': 30.0,     # this many seconds ...
    'burst_count': 3,           # ... holding this many new objective plates is a burst
    'quiet_gap_s': 90.0,        # longer than this without anything for the player to see, hear or fight is quiet
    'instant_s': 10.0,          # a one-off event (plate, mark, wave) keeps the player busy this long
    'voice_queue_s': 8.0,       # a voice line that waits this long behind others plays late
    'reaction_window_s': 60.0,  # a convoy reaching a minimap mark sooner than this after it appeared is too tight
    'screen_wait_s': 5.0,       # a cinematic that waits this long for the screen starts late
}

LANE_COLOURS = {
    'major': '#3b7dd8', 'bonus': '#2f9e5b', 'cinematic': '#8e5bd0', 'voice': '#c98a1b', 'delivery': '#4a9bb5',
    'convoy': '#b0583a', 'attack': '#c0392b', 'warning': '#e0453a', 'hold': '#7a3a26',
}


def mmss(seconds):
    seconds = max(0, int(round(seconds or 0)))
    return f'{seconds // 60}:{seconds % 60:02d}'


# ------------------------------------------------------------------------------------------------ pacing

def _activity(data):
    """Intervals during which the player has something to see, hear or fight."""
    spans = []
    for event in data['events']:
        if event['lane'] in ('setup', 'trigger'):
            continue
        start = event['t']
        end = event.get('end', start + PACING['instant_s'])
        spans.append((start, max(end, start + PACING['instant_s'] if 'end' not in event else end)))
    for convoy in data['convoys']:
        spans.append((convoy['spawn'], convoy['leave'] or convoy['spawn']))
    return sorted(spans)


def pacing_checks(data):
    warnings = []
    plates = sorted(e['t'] for e in data['events'] if e['lane'] == 'objective' and e['text'].startswith('+'))
    index = 0
    while index < len(plates):
        window = [t for t in plates if plates[index] <= t <= plates[index] + PACING['burst_window_s']]
        if len(window) >= PACING['burst_count']:
            titles = [e['text'][2:] for e in data['events'] if e['lane'] == 'objective' and e['text'].startswith('+')
                      and plates[index] <= e['t'] <= window[-1]]
            warnings.append({'t': plates[index], 'end': window[-1] + 1, 'kind': 'burst',
                             'text': f'{len(window)} new objectives within {window[-1] - plates[index]:.0f} s: '
                                     + '; '.join(titles)})
            index += len(window)
        else:
            index += 1

    covered_until = 0.0
    for start, end in _activity(data):
        if start - covered_until > PACING['quiet_gap_s']:
            warnings.append({'t': covered_until, 'end': start, 'kind': 'quiet',
                             'text': f'{start - covered_until:.0f} s with nothing to see, hear or fight '
                                     f'({mmss(covered_until)}-{mmss(start)})'})
        covered_until = max(covered_until, end)
    if data['end'] - covered_until > PACING['quiet_gap_s']:
        warnings.append({'t': covered_until, 'end': data['end'], 'kind': 'quiet',
                         'text': f'{data["end"] - covered_until:.0f} s quiet at the end'})

    for event in data['events']:
        queued = event.get('queued', 0) or 0
        if event['lane'] == 'voice' and queued > PACING['voice_queue_s'] and event.get('behind'):
            # Lines a stage queues back to back on purpose are fine; waiting behind another stage's lines is not.
            behind = ', '.join(s.replace('Mission_', '') for s in event['behind'])
            warnings.append({'t': event['t'] - queued, 'end': event['t'], 'kind': 'voice queue',
                             'text': f'{event["text"]} waits {queued:.0f} s behind lines of {behind}'})
        if event['lane'] == 'cinematic' and queued > PACING['screen_wait_s']:
            warnings.append({'t': event['t'] - queued, 'end': event['t'], 'kind': 'screen wait',
                             'text': f'{event["text"]} waits {queued:.0f} s for the screen'})
        if event['lane'] == 'reaction' and event.get('window') is not None:
            window = event['window']
            if window < PACING['reaction_window_s']:
                warnings.append({'t': event.get('mark_t', event['t']), 'end': event['t'], 'kind': 'reaction',
                                 'text': f'{event["route"]} reaches mark {event["mark"]} only {window:.0f} s after '
                                         f'the mark appears (need {PACING["reaction_window_s"]:.0f} s)'})
    return sorted(warnings, key=lambda w: w['t'])


# ------------------------------------------------------------------------------------------------ html

PX_PER_S = 1.2
LEFT = 170
ROW = 22
BAR = 14


def _x(seconds):
    return LEFT + seconds * PX_PER_S


def _esc(text):
    return html.escape(str(text), quote=True)


def _pack(items):
    """Greedy row packing: items (start, end, payload) -> list of (row, start, end, payload)."""
    rows, out = [], []
    for start, end, payload in sorted(items, key=lambda i: i[0]):
        for index, free_at in enumerate(rows):
            if start >= free_at:
                rows[index] = end
                out.append((index, start, end, payload))
                break
        else:
            rows.append(end)
            out.append((len(rows) - 1, start, end, payload))
    return out, max(1, len(rows))


class _Svg:
    def __init__(self):
        self.parts, self.y = [], 30

    def text(self, x, y, text, cls='', anchor='start'):
        self.parts.append(f'<text x="{x:.1f}" y="{y:.1f}" class="{cls}" text-anchor="{anchor}">{_esc(text)}</text>')

    def bar(self, start, end, y, colour, tip, label='', open_end=False, cls='bar', label_mode='inside'):
        """label_mode: 'inside' (may run past the bar: own row), 'fit' (only when it fits), 'after' (right of it)."""
        x1, x2 = _x(start), _x(max(end, start + 1))
        if label and label_mode == 'fit' and len(str(label)) * 6.0 + 8 > x2 - x1:
            label = ''
        dash = ' stroke-dasharray="4 3"' if open_end else ''
        self.parts.append(f'<g class="{cls}"><title>{_esc(tip)}</title>'
                          f'<rect x="{x1:.1f}" y="{y:.1f}" width="{x2 - x1:.1f}" height="{BAR}" rx="3" '
                          f'fill="{colour}" fill-opacity="{0.35 if open_end else 0.85}" stroke="{colour}"{dash}/>')
        if label:
            lx = x2 + 5 if label_mode == 'after' else x1 + 4
            self.parts.append(f'<text x="{lx:.1f}" y="{y + BAR - 3.5:.1f}" class="barlabel">{_esc(label)}</text>')
        self.parts.append('</g>')

    def tick(self, t, y, colour, tip, height=BAR + 6):
        x = _x(t)
        self.parts.append(f'<g><title>{_esc(tip)}</title><line x1="{x:.1f}" x2="{x:.1f}" y1="{y - 3:.1f}" '
                          f'y2="{y + height - 3:.1f}" stroke="{colour}" stroke-width="2.5"/>'
                          f'<circle cx="{x:.1f}" cy="{y - 3:.1f}" r="3" fill="{colour}"/></g>')

    def lane(self, title, rows):
        top = self.y
        height = rows * ROW + 10
        self.parts.append(f'<rect x="0" y="{top - 4:.1f}" width="100%" height="{height:.1f}" class="lanebg"/>')
        self.text(10, top + 12, title, 'lanetitle')
        self.y += height + 6
        return top


def _chart(data):
    end = max(data['end'], 60.0)
    svg = _Svg()
    width = _x(end) + 120

    # objectives: one row per stage that started
    stages = [s for s in data['stages'] if s['start'] is not None]
    top = svg.lane('Objectives', len(stages))
    for row, stage in enumerate(sorted(stages, key=lambda s: s['start'])):
        colour = LANE_COLOURS['bonus'] if 'bonus' in stage['objective'].lower() else LANE_COLOURS['major']
        done = stage['done']
        tip = f"{stage['title']} ({stage['stage']}) {mmss(stage['start'])} - " + (mmss(done) if done is not None
                                                                                   else 'not done (player)')
        svg.bar(stage['start'], done if done is not None else end, top + row * ROW, colour, tip,
                f"{stage['title']}", open_end=done is None)

    def spans(lane, colour, title, label_fn=lambda e: e['text']):
        items = [(e['t'], e.get('end', e['t'] + 2), e) for e in data['events'] if e['lane'] == lane]
        packed, rows = _pack(items)
        lane_top = svg.lane(title, rows)
        for row, start, stop, event in packed:
            svg.bar(start, stop, lane_top + row * ROW, colour, f"{mmss(start)}-{mmss(stop)} {event['text']}",
                    label_fn(event), label_mode='fit')

    spans('cinematic', LANE_COLOURS['cinematic'], 'Cinematics')
    spans('voice', LANE_COLOURS['voice'], 'Voice lines', lambda e: e['text'].split(' (')[0].replace('OreMountains_', ''))
    deliveries = [e for e in data['events'] if e['lane'] == 'delivery' and 'end' in e]
    packed, rows = _pack([(e['t'], e['end'], e) for e in deliveries])
    lane_top = svg.lane('Air deliveries', rows)
    for row, start, stop, event in packed:
        what = event['text'].replace('cargo plane with ', '').replace(' sets off', '')
        svg.bar(start, stop, lane_top + row * ROW, LANE_COLOURS['delivery'], f'{mmss(start)}-{mmss(stop)} {what}',
                what.replace('Nomadic_Ger', '').replace('Tank_', ''))

    convoys = data['convoys']
    lane_top = svg.lane('Convoys', len(convoys))
    for row, convoy in enumerate(convoys):
        y = lane_top + row * ROW
        leave = convoy['leave'] or end
        svg.bar(convoy['spawn'], leave, y, LANE_COLOURS['convoy'],
                f"{convoy['route']} {convoy['name']}: {mmss(convoy['spawn'])}-{mmss(leave)}, {convoy['pace_ms']} m/s",
                f"{convoy['route'].replace('ORE_', '')} {convoy['name']}", label_mode='after')
        for milestone in convoy['milestones']:
            if milestone.get('hold'):
                svg.bar(milestone['t'], milestone['t'] + milestone['hold'], y, LANE_COLOURS['hold'],
                        f"{milestone['what']} {mmss(milestone['t'])} for {milestone['hold']:.0f} s", cls='hold')
            svg.tick(milestone['t'], y, '#f2c14e', f"{mmss(milestone['t'])} {convoy['route']}: {milestone['what']}")

    attacks = [e for e in data['events'] if e['lane'] == 'attack']
    spans_attack = [e for e in attacks if 'end' in e]
    lane_top = svg.lane('Attacks', max(1, len(spans_attack)) + 1)
    for row, event in enumerate(spans_attack):
        svg.bar(event['t'], event['end'], lane_top + row * ROW, LANE_COLOURS['attack'],
                f"{mmss(event['t'])}-{mmss(event['end'])} {event['text']}", event['text'], label_mode='fit')
    for event in attacks:
        if 'end' not in event:
            svg.tick(event['t'], lane_top + len(spans_attack) * ROW, LANE_COLOURS['attack'],
                     f"{mmss(event['t'])} {event['text']}")

    warnings = data.get('warnings', [])
    packed, rows = _pack([(w['t'], max(w['end'], w['t'] + 4) + (len(w['kind']) * 6 + 12) / PX_PER_S, w)
                          for w in warnings])
    lane_top = svg.lane('Pacing warnings', rows)
    for row, start, stop, warning in packed:
        svg.bar(start, stop, lane_top + row * ROW, LANE_COLOURS['warning'],
                f"{mmss(start)} {warning['kind']}: {warning['text']}", warning['kind'], cls='warnbar',
                label_mode='after')

    # minute grid on top of the lanes
    grid = []
    for minute in range(0, int(math.ceil(end / 60)) + 1):
        x = _x(minute * 60)
        grid.append(f'<line x1="{x:.1f}" x2="{x:.1f}" y1="18" y2="{svg.y:.1f}" class="grid{" major" if minute % 5 == 0 else ""}"/>')
        grid.append(f'<text x="{x:.1f}" y="13" class="gridlabel" text-anchor="middle">{minute}:00</text>')
    return (f'<svg width="{width:.0f}" height="{svg.y + 10:.0f}" class="chart">' + ''.join(grid) + ''.join(svg.parts)
            + '</svg>')


def _stage_graph(data):
    """Layered flow: each stage one column right of the stages that start it; rows in start-time order."""
    box_w, box_h, col_gap, row_gap = 210, 48, 64, 12
    stages = {s['stage']: s for s in data['stages']}
    depth = {}

    def depth_of(name, seen=()):
        if name in depth:
            return depth[name]
        trigger = stages[name]['trigger']
        sources = []
        if trigger['kind'] == 'after':
            sources = [n for n in trigger.get('stages', []) if n in stages and n not in seen]
        if sources:
            depth[name] = 1 + max(depth_of(n, seen + (name,)) for n in sources)
        else:
            depth[name] = 1 if trigger['kind'] == 'near' else 0
        return depth[name]

    for name in stages:
        depth_of(name)
    columns = {}
    for name, stage in stages.items():
        columns.setdefault(depth[name], []).append(stage)
    pos = {}
    for column, members in columns.items():
        members.sort(key=lambda s: (s['start'] is None, s['start'] or 0))
        for row, stage in enumerate(members):
            pos[stage['stage']] = (40 + column * (box_w + col_gap), 30 + row * (box_h + row_gap))
    width = 80 + (max(columns) + 1) * (box_w + col_gap)
    height = 50 + max(len(m) for m in columns.values()) * (box_h + row_gap)
    edges, parts = [], []
    for stage in stages.values():
        trigger = stage['trigger']
        tx, ty = pos[stage['stage']]
        if trigger['kind'] == 'after':
            for index, source in enumerate(trigger['stages']):
                if source not in pos:
                    continue
                sx, sy = pos[source]
                x1, y1, x2, y2 = sx + box_w, sy + box_h / 2, tx, ty + box_h / 2
                bend = max(24.0, (x2 - x1) / 2)
                dash = ' stroke-dasharray="5 4"' if index > 0 else ''
                edges.append(f'<path d="M{x1:.1f},{y1:.1f} C{x1 + bend:.1f},{y1:.1f} {x2 - bend:.1f},{y2:.1f} '
                             f'{x2 - 2:.1f},{y2:.1f}" class="edge"{dash} marker-end="url(#arrow)">'
                             f'<title>{_esc(source)} completes: {_esc(stage["stage"])} starts</title></path>')
        elif trigger['kind'] == 'near':
            edges.append(f'<text x="{tx - 6:.1f}" y="{ty + box_h / 2 + 4:.1f}" class="gridlabel" '
                         f'text-anchor="end">player within {trigger["radius"] / 100:.0f} m</text>')
    for name, stage in stages.items():
        x, y = pos[name]
        colour = LANE_COLOURS['bonus'] if 'bonus' in stage['objective'].lower() else LANE_COLOURS['major']
        if stage['done'] is not None:
            when = f"{mmss(stage['start'])} - {mmss(stage['done'])}"
        elif stage['start'] is not None:
            when = f"{mmss(stage['start'])} - open"
        else:
            when = 'not started (player)'
        faded = ' faded' if stage['start'] is None else ''
        parts.append(f'<g class="stagebox{faded}"><title>{_esc(name)}</title>'
                     f'<rect x="{x:.1f}" y="{y:.1f}" width="{box_w}" height="{box_h}" rx="6" fill="{colour}" '
                     f'fill-opacity="0.16" stroke="{colour}"/>'
                     f'<text x="{x + 9:.1f}" y="{y + 19:.1f}" class="stagetitle">{_esc(stage["title"][:30])}</text>'
                     f'<text x="{x + 9:.1f}" y="{y + 36:.1f}" class="stagesub">{_esc(when)} - '
                     f'{_esc(name.replace("Mission_", "")[:22])}</text></g>')
    defs = ('<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" '
            'orient="auto-start-reverse"><path d="M0,0 L10,5 L0,10 z" class="arrowhead"/></marker></defs>')
    return f'<svg width="{width:.0f}" height="{height:.0f}" class="chart">{defs}' + ''.join(edges + parts) + '</svg>'


CSS = """
:root{--bg:#f6f5f2;--panel:#ffffff;--ink:#1d1f23;--muted:#6b6f76;--line:#e2e0db;--lane:#00000008;--warn:#e0453a}
@media (prefers-color-scheme:dark){:root{--bg:#141518;--panel:#1c1e22;--ink:#e8e6e1;--muted:#9a9ea6;--line:#2c2f35;--lane:#ffffff06}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.45 system-ui,Segoe UI,sans-serif}
main{max-width:1400px;margin:0 auto;padding:24px 16px 60px}h1{font-size:22px;margin:0 0 4px}h2{font-size:16px;margin:28px 0 10px}
.sub{color:var(--muted);font-size:13px}.cards{display:flex;flex-wrap:wrap;gap:10px;margin:16px 0}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px 14px;min-width:120px}
.card b{display:block;font-size:20px}.card span{color:var(--muted);font-size:12px}
.scroll{overflow-x:auto;background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:8px}
table{border-collapse:collapse;width:100%;background:var(--panel);border:1px solid var(--line);border-radius:10px;overflow:hidden}
td,th{padding:6px 10px;border-bottom:1px solid var(--line);text-align:left;vertical-align:top;font-size:13px}
th{color:var(--muted);font-weight:600}td.t{font-variant-numeric:tabular-nums;white-space:nowrap;color:var(--muted)}
.kind{display:inline-block;padding:1px 8px;border-radius:99px;background:#e0453a22;color:var(--warn);font-size:12px;font-weight:600}
details{margin-top:10px}summary{cursor:pointer;color:var(--muted)}
svg text{fill:var(--ink)}.lanetitle{font-weight:600;font-size:12px}.barlabel{font-size:10.5px;fill:var(--ink)}
.gridlabel{font-size:10.5px;fill:var(--muted)}.grid{stroke:var(--line);stroke-width:1}.grid.major{stroke-width:2}
.lanebg{fill:var(--lane)}.stagetitle{font-size:12.5px;font-weight:600}.stagesub{font-size:11px;fill:var(--muted)}
.edge{fill:none;stroke:var(--muted);stroke-width:1.6}.arrowhead{fill:var(--muted)}.faded{opacity:.55}
.rules{color:var(--muted);font-size:12px}
"""


def render_html(data):
    warnings = data.get('warnings', [])
    stages = data['stages']
    started = [s for s in stages if s['start'] is not None]
    cards = [(mmss(data['end']), 'timeline length'), (len(started), f'of {len(stages)} stages start'),
             (len(data['convoys']), 'convoys'), (len([e for e in data['events'] if e['lane'] == 'voice']),
                                                  'voice lines'),
             (len(warnings), 'pacing warnings'), (sum(data['unmodelled'].values()), 'unmodelled calls')]
    rows = ''.join(f'<tr><td class="t">{mmss(w["t"])}</td><td><span class="kind">{_esc(w["kind"])}</span></td>'
                   f'<td>{_esc(w["text"])}</td></tr>' for w in warnings) or \
        '<tr><td colspan="3">No pacing warnings.</td></tr>'
    events = ''.join(f'<tr><td class="t">{mmss(e["t"])}{" - " + mmss(e["end"]) if "end" in e and e["end"] > e["t"] + 0.5 else ""}'
                     f'</td><td>{_esc(e["lane"])}</td><td>{_esc(e["text"])}</td></tr>' for e in data['events'])
    unmodelled = ''.join(f'<li>{count}x {_esc(what)}</li>' for what, count in sorted(data['unmodelled'].items())) \
        or '<li>none</li>'
    rules = ', '.join(f'{k}={v}' for k, v in data['rules'].items())
    pacing = ', '.join(f'{k}={v}' for k, v in PACING.items())
    return f"""<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Mission Timeline {_esc(data['map'])}</title>
<style>{CSS}</style></head><body><main>
<h1>Mission timeline: {_esc(data['map'])}</h1>
<div class="sub">Estimated from the mission Blueprints and the placed actors, not from a play test. Scenario
<b>{_esc(data['scenario'])}</b> (the player stops nothing and destroys nothing).</div>
<div class="cards">{''.join(f'<div class="card"><b>{_esc(v)}</b><span>{_esc(k)}</span></div>' for v, k in cards)}</div>
<h2>Pacing warnings</h2><table><tr><th>When</th><th>Kind</th><th>What</th></tr>{rows}</table>
<h2>Timeline</h2><div class="sub">Hover a bar for details. Dashed objective bars are not finished in this scenario.</div>
<div class="scroll">{_chart(data)}</div>
<h2>Stage flow</h2><div class="sub">What starts what: each stage sits one column right of the stage whose
completion starts it (dashed arrow: a second watched stage). Times are from this scenario; faded = not started.</div><div class="scroll">{_stage_graph(data)}</div>
<details><summary>All events ({len(data['events'])})</summary><table><tr><th>When</th><th>Lane</th><th>What</th></tr>
{events}</table></details>
<details><summary>Assumptions and unmodelled calls</summary><p class="rules">Simulation: {_esc(rules)}</p>
<p class="rules">Pacing thresholds: {_esc(pacing)}</p><ul class="rules">{unmodelled}</ul></details>
</main></body></html>"""
