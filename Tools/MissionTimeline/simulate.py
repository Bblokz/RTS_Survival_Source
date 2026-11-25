"""Estimate a mission's timeline from its exported model (export_model.py) without a play test.

Usage:
    python simulate.py <MapName> [--scenario passive] [--set key=value ...] [--until 3600]

Reads Saved/MissionTimeline/<MapName>/model.json and writes timeline.json + timeline.txt next to it.

How it works: the stage Blueprints are interpreted (events, calls, branches, variables, waits, macros), driven by a
clock. What the engine does on its own is modelled from the exported settings: location intros, camera tours, the
voice-line queue, cargo-plane deliveries, convoy routes (pace, stops, branches, dismounts, ambush zones), Soviet
counterattack timers, and stage triggers (class completed, player near). What the player does comes from the scenario:
'passive' = the player stops nothing and destroys nothing, so every convoy escapes and kill objectives never finish.
Every call the simulator cannot time is listed under "unmodelled" (counted as 0 s), so the weak spots show.
"""
import argparse
import heapq
import json
import math
import os
import re
import sys

SAVED = r'D:\UE5Projects\RTS_Survival\Saved\MissionTimeline'

# Assumptions (override with --set key=value). Keep each one visible in the output.
RULES = {
    'difficulty': 'normal',              # counterattack settings field and convoy escort tier column
    'air_delivery_seconds': 35.0,        # cargo plane: spawn at the map edge, fly in, lower the load (estimate)
    'location_intro_min_seconds': 7.0,   # ShowLocationIntro uses 7 when the duration is 0 or less
    'takeover_read_seconds': 12.0,       # info takeovers (field manuals) until the player closes them
    'tour_min_shot_seconds': 0.5,
    'tour_max_screen_wait_seconds': 45.0,
    'convoy_default_speed_cms': 1000.0,  # when a truck spec has no calibrated speed
    'convoy_exit_radius_cms': 1500.0,
    'player_unit_speed_cms': 600.0,      # for 'near' triggers reached by moving start units (not used: passive)
    'mark_route_distance_cms': 8000.0,   # a minimap mark this close to a route counts as a spot the convoy passes
}


# ------------------------------------------------------------------------------------------------ small helpers

def norm(path):
    """"/Script/Engine.X'/Game/A.B'" -> "/Game/A.B"; other strings unchanged."""
    if isinstance(path, str) and "'" in path:
        parts = path.split("'")
        if len(parts) >= 2 and parts[1]:
            return parts[1]
    return path


def low(name):
    return name.lower() if isinstance(name, str) else name


def mmss(seconds):
    seconds = max(0, int(round(seconds)))
    return f'{seconds // 60:2d}:{seconds % 60:02d}'


def vec(value):
    if isinstance(value, dict):
        return (float(value.get('x', 0)), float(value.get('y', 0)), float(value.get('z', 0)))
    if isinstance(value, (list, tuple)) and len(value) == 3:
        return tuple(float(v) for v in value)
    if isinstance(value, str):
        numbers = re.findall(r'-?\d+(?:\.\d+)?(?:e-?\d+)?', value)
        if len(numbers) >= 3:
            return tuple(float(n) for n in numbers[:3])
    return (0.0, 0.0, 0.0)


def dist2d(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


ENUM_INDEX = {}   # enum value name -> index, filled from the model's "enums"


def byte_value(value):
    """Byte/enum pins: literals hold the index ("1"), events hand over the name ("Escaped")."""
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (int, float)):
        return int(value)
    text = str(value).rsplit('::', 1)[-1]
    if re.fullmatch(r'-?\d+', text):
        return int(text)
    return ENUM_INDEX.get(text, text)


def get_ci(mapping, key, default=None):
    if not isinstance(mapping, dict):
        return default
    if key in mapping:
        return mapping[key]
    wanted = low(key)
    for k, v in mapping.items():
        if low(k) == wanted:
            return v
    return default


# ------------------------------------------------------------------------------------------------ runtime objects

class WorldObject:
    """Anything a graph can hold a reference to: routes, convoys, the battery, subsystems, actors."""

    def __init__(self, kind, name, location=(0.0, 0.0, 0.0), data=None):
        self.kind, self.name, self.location, self.data = kind, name, location, data or {}
        self.subscribers = {}

    def subscribe(self, dispatcher, mission, event_name):
        self.subscribers.setdefault(low(dispatcher), []).append((mission, event_name))

    def __repr__(self):
        return f'<{self.kind} {self.name}>'


class Convoy(WorldObject):
    def __init__(self, route, index, spawn_time):
        super().__init__('Convoy', f'{route.name}#{index}', route.points[0] if route.points else (0, 0, 0))
        self.route, self.index, self.spawn_time = route, index, spawn_time


class Route(WorldObject):
    def __init__(self, actor, sim):
        props = actor['properties']
        super().__init__('EnemyConvoyRoute', get_ci(props, 'routeId'), vec(actor['location']), props)
        self.sim = sim
        self.label = actor['label']
        yaw = math.radians(float(actor.get('yaw', 0.0)))
        self.points = [self.to_world(vec(p), yaw) for p in get_ci(props, 'routePoints', [])]
        self.branch_points = [self.to_world(vec(p), yaw) for p in get_ci(props, 'branchPoints', [])]
        self.stop_point = self.to_world(vec(get_ci(props, 'stopPoint')), yaw)
        self.dismount_point = self.to_world(vec(get_ci(props, 'dismountPoint')), yaw)
        self.zone_points = [self.to_world(vec(p), yaw) for p in get_ci(props, 'ambushZonePoints', [])]
        self.started = False
        self.convoy_count = 0

    def to_world(self, relative, yaw):
        x, y, z = relative
        cos, sin = math.cos(yaw), math.sin(yaw)
        return (self.location[0] + x * cos - y * sin, self.location[1] + x * sin + y * cos, self.location[2] + z)

    # -- geometry along the route
    def cumulative(self):
        out, total = [0.0], 0.0
        for a, b in zip(self.points, self.points[1:]):
            total += dist2d(a, b)
            out.append(total)
        return out

    def distance_of(self, point):
        """Distance along the route of the route vertex nearest to point (route points are ~25 m apart)."""
        cumulative = self.cumulative()
        best = min(range(len(self.points)), key=lambda i: dist2d(self.points[i], point))
        return cumulative[best], dist2d(self.points[best], point)

    def pace_cms(self, definition):
        speed = get_ci(definition, 'speed', {})
        fraction = float(get_ci(speed, 'truckSpeedFraction', 0.5))
        speeds = [float(get_ci(t, 'calibratedCruiseSpeedCms', 0)) or RULES['convoy_default_speed_cms']
                  for t in get_ci(definition, 'trucks', [])]
        return fraction * (min(speeds) if speeds else RULES['convoy_default_speed_cms'])


# ------------------------------------------------------------------------------------------------ model access

class Model:
    def __init__(self, data):
        self.data = data
        self.assets = {norm(k): v for k, v in data.get('assets', {}).items()}
        for names in data.get('enums', {}).values():
            for index, name in enumerate(names):
                ENUM_INDEX.setdefault(name.rsplit('::', 1)[-1], index)
        self.blueprints = {}
        self.bp_by_class = {}
        for path, bp in data.get('blueprints', {}).items():
            bp['path'] = path
            bp['graph_by_name'] = {}
            for graph in bp['graphs']:
                graph['by_id'] = {n['id']: n for n in graph['nodes']}
                for node in graph['nodes']:
                    node['graph'] = graph
                    node['pin_by_name'] = {p['name']: p for p in node['pins']}
                graph['bp'] = bp
                bp['graph_by_name'][graph['name']] = graph
            self.blueprints[path] = bp
            self.bp_by_class[norm(bp.get('generated_class', ''))] = bp

    def chain(self, class_path):
        """Blueprints from the mission's own class up through its Blueprint parents."""
        out, current = [], self.bp_by_class.get(norm(class_path))
        while current is not None and current not in out:
            out.append(current)
            current = self.bp_by_class.get(norm(current.get('parent_class', '')))
        return out

    def native_parent(self, class_path):
        chain = self.chain(class_path)
        return norm(chain[-1].get('parent_class', '')) if chain else ''

    def sound_seconds(self, ref):
        asset = self.assets.get(norm(ref)) if isinstance(ref, str) else None
        return float(asset['duration']) if asset and 'duration' in asset else None

    def asset_props(self, ref):
        asset = self.assets.get(norm(ref)) if isinstance(ref, str) else None
        return asset.get('properties', {}) if asset else {}


def member_name(node, key):
    ref = node['settings'].get(key) or {}
    return ref.get('memberName') if isinstance(ref, dict) else None


def event_name_of(node):
    if node['class'] == 'K2Node_CustomEvent':
        return node['settings'].get('customFunctionName')
    if node['class'] == 'K2Node_Event':
        return member_name(node, 'eventReference')
    return None


def is_pure(node):
    return not any(p['category'] == 'exec' for p in node['pins'])


# ------------------------------------------------------------------------------------------------ missions

class Mission(WorldObject):
    def __init__(self, entry, model):
        super().__init__('Mission', entry['class'][:-2] if entry['class'].endswith('_C') else entry['class'])
        self.entry = entry
        self.class_path = norm(entry['class_path'])
        self.chain = model.chain(self.class_path)
        self.native = model.native_parent(self.class_path) or norm(entry['class_path'])
        self.vars = {low(k): v for k, v in entry.get('properties', {}).items()}
        self.trigger = entry.get('trigger')
        self.state = 'waiting'
        self.started_at = self.completed_at = None
        widget = get_ci(entry.get('properties', {}), 'missionWidgetState', {})
        self.title = get_ci(widget, 'missionTitle', '') or self.name
        self.objective = str(get_ci(widget, 'objectiveType', 'Major'))
        self.once = {}
        # The mission's own camera tour (PlayCameraTour): None, or (screen start, end of shots, end).
        self.tour = None

    def find_event(self, name):
        for bp in self.chain:
            for graph in bp['graphs']:
                if graph['kind'] != 'event':
                    continue
                for node in graph['nodes']:
                    if event_name_of(node) == name:
                        return node
        return None

    def find_function_graph(self, name):
        for bp in self.chain:
            graph = bp['graph_by_name'].get(name)
            if graph is not None and graph['kind'] == 'function':
                return graph
        return None


class Frame:
    def __init__(self, mission, params=None):
        self.mission = mission
        self.params = dict(params or {})
        self.cache = {}


# ------------------------------------------------------------------------------------------------ the simulator

class Simulator:
    def __init__(self, model, scenario='passive'):
        self.model, self.scenario = model, scenario
        self.now = 0.0
        self.queue, self.seq = [], 0
        self.timeline, self.unmodelled = [], {}
        self.voice_free = 0.0
        self.voice_queue = []      # (end, stage) of every queued line
        self.screen_free = 0.0
        self.victory = None
        self.convoys = []          # one record per spawned convoy: spawn, leave, milestones (for the chart)
        self.marks = []            # minimap marks with location: reaction windows against passing convoys
        self.manager = WorldObject('MissionManager', 'MissionManager')
        self.controller = WorldObject('PlayerController', 'PlayerController')
        self.truck_subsystem = WorldObject('SurrenderingTruckSubsystem', 'SurrenderingTruckSubsystem')
        self.enemy = WorldObject('EnemyController', 'EnemyController')
        self.player_positions = []
        self.actors = []
        self.routes = {}
        for actor in model.data.get('actors', []):
            chain = actor.get('class_chain', [actor['class']])
            obj = WorldObject(actor['class'], actor['label'], vec(actor['location']), actor)
            obj.chain = chain
            obj.tags = actor.get('tags', [])
            if 'EnemyConvoyRoute' in chain and 'properties' in actor:
                obj = Route(actor, self)
                obj.chain, obj.tags = chain, actor.get('tags', [])
                self.routes[obj.name] = obj
            self.actors.append(obj)
        starts = [a for a in self.actors if 'PlayerStart' in a.kind or 'PLayerStart' in a.kind]
        self.player_start = starts[0].location if starts else None
        self.missions = [Mission(entry, model) for entry in model.data.get('missions', [])]

    # -- bookkeeping
    def log(self, lane, text, start=None, end=None, **extra):
        item = {'t': round(self.now if start is None else start, 2), 'lane': lane, 'text': text}
        if end is not None:
            item['end'] = round(end, 2)
        item.update(extra)
        self.timeline.append(item)

    def unknown(self, what, mission):
        key = f'{what} ({mission.name})'
        self.unmodelled[key] = self.unmodelled.get(key, 0) + 1

    def at(self, time, callback, label=''):
        self.seq += 1
        heapq.heappush(self.queue, (time, self.seq, label, callback))

    def run(self, until):
        for mission in self.missions:
            if not mission.trigger:
                self.at(0.0, lambda m=mission: self.start_mission(m), 'start ' + mission.name)
        while self.queue and self.victory is None:
            time, _, _, callback = heapq.heappop(self.queue)
            if time > until:
                break
            self.now = max(self.now, time)
            callback()
        self.timeline.sort(key=lambda i: (i['t'], i['lane']))

    # -- missions
    def start_mission(self, mission):
        if mission.state != 'waiting':
            return
        delay = float(mission.vars.get('loadtostartdelay', 0) or 0)
        if delay > 0:
            mission.state = 'loading'
            self.at(self.now + delay, lambda: self._begin(mission))
            return
        self._begin(mission)

    def _begin(self, mission):
        mission.state = 'active'
        mission.started_at = self.now
        self.log('objective', f'+ {mission.title}', kind=mission.objective, stage=mission.name)
        native = mission.native.rsplit('.', 1)[-1]
        if native == 'SovietCounterattackMission':
            self.native_counterattack(mission)
        self.run_event(mission, 'BP_OnMissionStart', {})

    def complete_mission(self, mission, completed=True):
        if mission.state in ('complete', 'failed'):
            return
        mission.state = 'complete' if completed else 'failed'
        mission.completed_at = self.now
        self.log('objective', f'{"done" if completed else "FAILED"}: {mission.title}', kind=mission.objective,
                 stage=mission.name)
        self.run_event(mission, 'BP_OnMissionComplete' if completed else 'BP_OnMissionFailed', {})
        for other in self.missions:
            if other is not mission and other.state == 'active':
                self.run_event(other, 'BP_OnOtherMissionEnded',
                               {'EndedMissionClass': mission.class_path, 'bCompleted': completed})
        self.check_class_triggers(mission, completed)

    def check_class_triggers(self, ended, completed):
        for mission in self.missions:
            trigger = mission.trigger
            if mission.state != 'waiting' or not trigger:
                continue
            if trigger.get('_class') != 'MissionClassCompletedMissionTrigger' and \
                    'MissionClassCompleted' not in trigger.get('_class', ''):
                continue
            watched = [norm(get_ci(trigger, 'missionClassToWatch', ''))]
            watched += [norm(c) for c in get_ci(trigger, 'alsoWatchMissionClasses', []) or []]
            if ended.class_path not in watched:
                continue
            if completed or get_ci(trigger, 'bAlsoTriggerWhenFailed', False):
                self.start_mission(mission)

    def check_near_triggers(self, position, what):
        self.player_positions.append(position)
        for mission in self.missions:
            trigger = mission.trigger
            if mission.state != 'waiting' or not trigger or 'UnitsNear' not in trigger.get('_class', ''):
                continue
            centre = vec(get_ci(trigger, 'centreLocation'))
            if dist2d(centre, position) <= float(get_ci(trigger, 'radius', 0)):
                self.log('trigger', f'{what} is near {mission.title}')
                self.start_mission(mission)

    # -- native mission classes
    def native_counterattack(self, mission):
        settings = self.model.asset_props(mission.vars.get('settings'))
        difficulty = get_ci(settings, RULES['difficulty'], {})
        prepare = float(get_ci(difficulty, 'secondsUntilSovietsArrive', 180))
        waves = int(get_ci(difficulty, 'waveCount', 4))
        between = float(get_ci(difficulty, 'secondsBetweenWaves', 90))
        start = self.now
        self.log('attack', f'{mission.title}: prepare {prepare:.0f} s, {waves} waves every {between:.0f} s',
                 start=start, end=start + prepare + waves * between)
        for wave in range(waves):
            self.log('attack', f'wave {wave + 1}/{waves} arrives', start=start + prepare + wave * between)
        # Survive timer over -> the remaining Soviets must still die; passive assumes that happens at once.
        self.at(start + prepare + waves * between, lambda: self.complete_mission(mission))

    # -- events and execution
    def run_event(self, mission, name, params):
        node = mission.find_event(name)
        if node is None:
            return
        frame = Frame(mission, params)
        for pin in node['pins']:
            if pin['dir'] == 'out' and pin['category'] not in ('exec', 'delegate'):
                frame.cache[(node['id'], pin['name'])] = params.get(pin['name'], frame.params.get(pin['name']))
        self.follow(frame, node, 'then')

    def fire(self, obj, dispatcher, params):
        for mission, event in list(obj.subscribers.get(low(dispatcher), [])):
            if mission.state == 'active':
                self.run_event(mission, event, params)

    def follow(self, frame, node, out_pin):
        pin = node['pin_by_name'].get(out_pin)
        if pin is None:
            return None
        for link in pin['links']:
            target = node['graph']['by_id'].get(link['node'])
            if target is not None:
                result = self.execute(frame, target, link['pin'])
                if result is not None:
                    return result
        return None

    def execute(self, frame, node, in_pin):
        cls = node['class']
        if cls == 'K2Node_FunctionResult':
            return {p['name']: self.eval_in(frame, node, p['name']) for p in node['pins']
                    if p['dir'] == 'in' and p['category'] != 'exec'}
        if cls == 'K2Node_Tunnel':
            return {'__tunnel__': in_pin}
        if cls == 'K2Node_IfThenElse':
            return self.follow(frame, node, 'then' if self.eval_in(frame, node, 'Condition') else 'else')
        if cls in ('K2Node_SwitchInteger', 'K2Node_SwitchEnum', 'K2Node_SwitchName', 'K2Node_SwitchString'):
            value = self.eval_in(frame, node, 'Selection')
            key = str(int(value)) if isinstance(value, (int, float)) and not isinstance(value, bool) else str(value)
            key = key.rsplit('::', 1)[-1]
            if key not in node['pin_by_name'] and cls == 'K2Node_SwitchEnum':
                names = [p['name'] for p in node['pins'] if p['dir'] == 'out' and p['category'] == 'exec']
                key = next((n for n in names if byte_value(n) == byte_value(key)), key)
            return self.follow(frame, node, key if key in node['pin_by_name'] else 'Default')
        if cls == 'K2Node_ExecutionSequence':
            for pin in node['pins']:
                if pin['dir'] == 'out' and pin['category'] == 'exec':
                    self.follow(frame, node, pin['name'])
            return None
        if cls == 'K2Node_VariableSet':
            name = member_name(node, 'variableReference')
            value_pin = next((p['name'] for p in node['pins'] if p['dir'] == 'in' and p['category'] != 'exec'
                              and p['name'] not in ('self',)), None)
            frame.mission.vars[low(name)] = self.eval_in(frame, node, value_pin) if value_pin else None
            return self.follow(frame, node, 'then')
        if cls == 'K2Node_AddDelegate':
            return self.exec_add_delegate(frame, node)
        if cls == 'K2Node_DynamicCast':
            value = self.eval_in(frame, node, 'Object')
            for pin in node['pins']:
                if pin['dir'] == 'out' and pin['category'] == 'object':
                    frame.cache[(node['id'], pin['name'])] = value
            frame.cache[(node['id'], 'bSuccess')] = value is not None
            return self.follow(frame, node, 'then' if value is not None else 'CastFailed')
        if cls == 'K2Node_MacroInstance':
            return self.exec_macro(frame, node, in_pin)
        if cls == 'K2Node_AsyncAction':
            return self.exec_async_action(frame, node)
        if cls in ('K2Node_CallFunction', 'K2Node_CallArrayFunction', 'K2Node_CallParentFunction', 'K2Node_Message'):
            outputs = self.call(frame, node)
            for name, value in (outputs or {}).items():
                frame.cache[(node['id'], name)] = value
            return self.follow(frame, node, 'then')
        if cls == 'K2Node_Knot':
            return self.follow(frame, node, node['pins'][-1]['name'])
        self.unknown('node ' + cls, frame.mission)
        return self.follow(frame, node, 'then')

    # -- data
    def eval_in(self, frame, node, pin_name):
        pin = node['pin_by_name'].get(pin_name)
        if pin is None:
            return None
        if pin['links']:
            link = pin['links'][0]
            source = node['graph']['by_id'].get(link['node'])
            return self.eval_out(frame, source, link['pin']) if source else None
        return self.literal(pin)

    def literal(self, pin):
        category, default = pin['category'], pin.get('default')
        if pin.get('default_object'):
            return pin['default_object']
        if pin.get('default_text'):
            return pin['default_text']
        if default is None:
            return {'int': 0, 'int64': 0, 'real': 0.0, 'float': 0.0, 'double': 0.0, 'bool': False}.get(category)
        if category in ('int', 'int64', 'byte') and re.fullmatch(r'-?\d+', default):
            return int(default)
        if category in ('real', 'float', 'double'):
            return float(default)
        if category == 'bool':
            return default.lower() == 'true'
        if category == 'struct' and pin.get('sub_object') in ('Vector', 'Rotator'):
            return vec(default)
        return default

    def eval_out(self, frame, node, pin_name):
        cls = node['class']
        key = (node['id'], pin_name)
        if key in frame.cache:
            return frame.cache[key]
        if cls == 'K2Node_VariableGet':
            return frame.mission.vars.get(low(member_name(node, 'variableReference')))
        if cls == 'K2Node_Self':
            return frame.mission
        if cls in ('K2Node_Event', 'K2Node_CustomEvent', 'K2Node_FunctionEntry', 'K2Node_Tunnel'):
            return frame.params.get(pin_name)
        if cls == 'K2Node_VariableSet':
            return frame.mission.vars.get(low(member_name(node, 'variableReference')))
        if cls == 'K2Node_MakeStruct':
            return {p['name']: self.eval_in(frame, node, p['name']) for p in node['pins'] if p['dir'] == 'in'}
        if cls == 'K2Node_BreakStruct':
            value = self.eval_in(frame, node, node['pins'][0]['name'])
            return get_ci(value, pin_name) if isinstance(value, dict) else None
        if cls == 'K2Node_MakeArray':
            return [self.eval_in(frame, node, p['name']) for p in node['pins'] if p['dir'] == 'in']
        if cls == 'K2Node_Knot':
            return self.eval_in(frame, node, node['pins'][0]['name'])
        if cls == 'K2Node_MacroInstance' and is_pure(node):
            return self.eval_pure_macro(frame, node, pin_name)
        if cls in ('K2Node_CallFunction', 'K2Node_CallArrayFunction') and is_pure(node):
            outputs = self.call(frame, node) or {}
            return outputs.get(pin_name, outputs.get('ReturnValue'))
        return None

    # -- macros
    def macro_graph(self, node):
        bp = self.model.blueprints.get(norm(node.get('macro_blueprint', '')))
        return bp['graph_by_name'].get(node.get('macro_graph')) if bp else None

    def exec_macro(self, frame, node, in_pin):
        name = node.get('macro_graph')
        key = (frame.mission.name, node['id'])
        if name == 'DoOnce':
            if in_pin == 'Reset':
                frame.mission.once[key] = False
                return None
            if frame.mission.once.get(key, bool(self.eval_in(frame, node, 'Start Closed'))):
                return None
            frame.mission.once[key] = True
            return self.follow(frame, node, 'Completed')
        if name == 'IsValid':
            value = self.eval_in(frame, node, 'InputObject')
            return self.follow(frame, node, 'Is Valid' if value not in (None, '', 'None') else 'Is Not Valid')
        if name in ('ForEachLoop', 'ForEachLoopWithBreak'):
            items = self.eval_in(frame, node, 'Array') or []
            for index, item in enumerate(items):
                frame.cache[(node['id'], 'Array Element')] = item
                frame.cache[(node['id'], 'Array Index')] = index
                self.follow(frame, node, 'LoopBody')
            return self.follow(frame, node, 'Completed')
        self.unknown('macro ' + str(name), frame.mission)
        return None

    def eval_pure_macro(self, frame, node, pin_name):
        graph = self.macro_graph(node)
        if graph is None:
            self.unknown('pure macro ' + str(node.get('macro_graph')), frame.mission)
            return None
        inner = Frame(frame.mission, {p['name']: self.eval_in(frame, node, p['name'])
                                      for p in node['pins'] if p['dir'] == 'in'})
        for tunnel in graph['nodes']:
            if tunnel['class'] == 'K2Node_Tunnel' and pin_name in tunnel['pin_by_name'] \
                    and tunnel['pin_by_name'][pin_name]['dir'] == 'in':
                return self.eval_in(inner, tunnel, pin_name)
        return None

    # -- delegates
    def delegate_target(self, frame, node, pin_name):
        """The custom event (by name) a delegate pin is wired to: a Create Event or a custom event's own pin."""
        pin = node['pin_by_name'].get(pin_name)
        if pin is None or not pin['links']:
            return None
        source = node['graph']['by_id'].get(pin['links'][0]['node'])
        if source is None:
            return None
        if source['class'] == 'K2Node_CreateDelegate':
            return source['settings'].get('selectedFunctionName')
        return event_name_of(source)

    def exec_add_delegate(self, frame, node):
        dispatcher = member_name(node, 'delegateReference')
        target = self.eval_in(frame, node, 'self')
        event = self.delegate_target(frame, node, 'Delegate')
        if isinstance(target, WorldObject) and event:
            target.subscribe(dispatcher, frame.mission, event)
        elif target is None:
            self.unknown(f'bind {dispatcher} on a missing object', frame.mission)
        return self.follow(frame, node, 'then')

    def exec_async_action(self, frame, node):
        name = node['settings'].get('proxyFactoryFunctionName')
        if 'Takeover' in str(name):
            start = max(self.now, self.screen_free)
            end = start + RULES['takeover_read_seconds']
            self.screen_free = end
            self.log('cinematic', f'takeover {name}', start=start, end=end)
            frame.cache[(node['id'], 'bWasShown')] = True
            self.follow(frame, node, 'then')
            snapshot = Frame(frame.mission, frame.params)
            snapshot.cache = dict(frame.cache)
            self.at(end, lambda: self.follow(snapshot, node, 'OnClosed'))
            return None
        self.unknown('async ' + str(name), frame.mission)
        return self.follow(frame, node, 'then')

    # -- calls
    def call(self, frame, node):
        name = member_name(node, 'functionReference')
        ref = node['settings'].get('functionReference') or {}
        self_context = ref.get('bSelfContext') or norm(ref.get('memberParent', 'None')) in ('None', '')
        target = self.eval_in(frame, node, 'self') if 'self' in node['pin_by_name'] else None
        mission = target if isinstance(target, Mission) else frame.mission
        if self_context or isinstance(target, Mission):
            graph = mission.find_function_graph(name)
            if graph is not None:
                return self.call_graph(frame, node, mission, graph)
            if mission.find_event(name) is not None:
                params = {p['name']: self.eval_in(frame, node, p['name']) for p in node['pins']
                          if p['dir'] == 'in' and p['category'] not in ('exec',) and p['name'] != 'self'}
                self.run_event(mission, name, params)
                return {}
        if str(name).startswith('GetPlayer') and 'RTSBlueprintFunctionLibrary' in str(ref.get('memberParent', '')):
            # Faction picks (e.g. Pz III J or Pz IV F1): the name is enough for the timeline.
            return {'ReturnValue': name[len('GetPlayer'):]}
        if str(name).startswith('SpawnPlayer'):
            return self.spawn_player_units(_Args(self, frame, node), frame.mission, name[len('SpawnPlayer'):])
        handler = getattr(self, 'fn_' + name, None)
        if handler is not None:
            args = _Args(self, frame, node)
            return handler(args, mission if isinstance(target, Mission) else frame.mission, target) or {}
        pure = PURE.get(name)
        if pure is not None:
            args = _Args(self, frame, node)
            try:
                return {'ReturnValue': pure(args)}
            except Exception:  # noqa: BLE001  (bad literal etc.: report, never stop the run)
                self.unknown('math ' + name, frame.mission)
                return {}
        self.unknown('call ' + str(name), frame.mission)
        return {}

    def call_graph(self, frame, node, mission, graph):
        params = {p['name']: self.eval_in(frame, node, p['name']) for p in node['pins']
                  if p['dir'] == 'in' and p['category'] != 'exec' and p['name'] != 'self'}
        inner = Frame(mission, params)
        entry = next((n for n in graph['nodes'] if n['class'] == 'K2Node_FunctionEntry'), None)
        if entry is None:
            return {}
        result = self.follow(inner, entry, 'then')
        if result is None:
            # A function without a reached return node: evaluate the result node's inputs anyway (pure outputs).
            ret = next((n for n in graph['nodes'] if n['class'] == 'K2Node_FunctionResult'), None)
            result = {p['name']: self.eval_in(inner, ret, p['name']) for p in ret['pins']
                      if p['dir'] == 'in' and p['category'] != 'exec'} if ret else {}
        return result

    # ---------------------------------------------------------------------------------------- native handlers
    # Each fn_<Name>(args, mission, target) returns the call's outputs; time passes only through scheduled events.

    def schedule_delegate(self, args, mission, pin, time):
        event = self.delegate_target(args.frame, args.node, pin)
        if event:
            self.at(time, lambda: self.run_event(mission, event, {}), f'{mission.name}.{event}')
        else:
            self.unknown('scheduled callback without an event', mission)

    def fn_ScheduleSingleCallback(self, args, mission, target):
        self.schedule_delegate(args, mission, 'Callback', self.now + float(args.get('DelaySeconds') or 0))
        return {'ReturnValue': self.seq}

    def fn_ScheduleRepeatedCallback(self, args, mission, target):
        total = int(args.get('TotalCalls') or 0)
        interval = float(args.get('IntervalSeconds') or 1)
        initial = float(args.get('InitialDelaySeconds') or 0)
        first = bool(args.get('bFireBeforeFirstInterval'))
        times = []
        if first:
            times.append(self.now)
        start = self.now + (initial if initial > 0 else interval)
        while len(times) < total:
            times.append(start + (len(times) - (1 if first else 0)) * interval)
        for time in times:
            self.schedule_delegate(args, mission, 'Callback', time)
        return {'ReturnValue': self.seq}

    def fn_ShowLocationIntro(self, args, mission, target):
        duration = float(args.get('DurationSeconds') or 0)
        duration = duration if duration > 0 else RULES['location_intro_min_seconds']
        start = max(self.now, self.screen_free)
        end = start + duration
        self.screen_free = end
        text = f"location intro: {args.get('Location')} {args.get('Date')}" + \
               (' (panning)' if args.get('bPanCamera') else '')
        self.log('cinematic', text, start=start, end=end, queued=round(start - self.now, 2), stage=mission.name)
        identifier = args.get('ID') or 0
        self.at(end, lambda: self.run_event(mission, 'BP_OnLocationIntroFinished', {'ID': identifier, 'bWasShown': True}))
        return {'ReturnValue': True}

    def fn_PlayCameraTour(self, args, mission, target):
        shots = args.get('Shots') or []
        settings = args.get('Settings') or {}
        identifier = args.get('ID') or 0
        if mission.tour and self.now < mission.tour[2]:
            self.unknown('second camera tour of one mission (refused in game)', mission)
            return {'ReturnValue': False}
        shot_seconds = sum(max(RULES['tour_min_shot_seconds'], float(get_ci(s, 'Seconds', 5) or 5)) for s in shots)
        seconds = shot_seconds + float(get_ci(settings, 'ReturnSeconds', 1.75) or 0)
        start = max(self.now, self.screen_free)
        titles = ', '.join(str(get_ci(s, 'Title', '')) for s in shots if get_ci(s, 'Title'))
        if start - self.now > RULES['tour_max_screen_wait_seconds']:
            # The session gives up after 45 s waiting for the screen (SkippedScreenBusy): never shown.
            end = self.now + RULES['tour_max_screen_wait_seconds']
            mission.tour = (end, end, end)
            self.log('cinematic', f'camera tour SKIPPED (screen busy 45 s): {titles}', start=end, end=end,
                     queued=round(end - self.now, 2), stage=mission.name)
            self.at(end, lambda: self.run_event(mission, 'BP_OnCameraTourFinished', {'ID': identifier,
                                                                                    'bWasShown': False}))
            return {'ReturnValue': True}
        end = start + seconds
        self.screen_free = end
        mission.tour = (start, start + shot_seconds, end)
        skip = ' (skippable)' if get_ci(settings, 'bAllowSkipping') else ''
        self.log('cinematic', f'camera tour, {len(shots)} shots: {titles}{skip}', start=start, end=end,
                 queued=round(start - self.now, 2), stage=mission.name)
        self.at(end, lambda: self.run_event(mission, 'BP_OnCameraTourFinished', {'ID': identifier, 'bWasShown': True}))
        return {'ReturnValue': True}

    def fn_GetPlayerPortraitManager(self, args, mission, target):
        return {'ReturnValue': 'PortraitManager'}

    def fn_GetIsPortraitPlaying(self, args, mission, target):
        return {'ReturnValue': self.voice_free > self.now + 1e-6}

    def fn_GetCameraTourShotIndex(self, args, mission, target):
        tour = mission.tour
        return {'ReturnValue': 0 if tour and tour[0] <= self.now < tour[1] else -1}

    def fn_GetIsCameraTourPlaying(self, args, mission, target):
        tour = mission.tour
        return {'ReturnValue': bool(tour and self.now < tour[2])}

    def fn_PlayPortrait(self, args, mission, target):
        line = args.get('VoiceLine')
        seconds = self.model.sound_seconds(line)
        if not line or line == 'None':
            return {}
        if seconds is None:
            self.unknown('voice line without a duration', mission)
            seconds = 0.0
        start = max(self.now, self.voice_free)
        behind = sorted({stage for end, stage in self.voice_queue if end > self.now and stage != mission.name})
        self.voice_free = start + seconds
        self.voice_queue.append((start + seconds, mission.name))
        speaker = str(args.get('PortraitType') or '')
        self.log('voice', f'{norm(line).rsplit(".", 1)[-1]} ({speaker})', start=start, end=start + seconds,
                 queued=round(start - self.now, 2), stage=mission.name, behind=behind)
        return {}

    def fn_OnMissionComplete(self, args, mission, target):
        self.complete_mission(mission)
        return {}

    def fn_TriggerVictory(self, args, mission, target):
        self.log('objective', 'VICTORY')
        self.victory = self.now
        return {}

    def fn_GetMissionManagerChecked(self, args, mission, target):
        return {'ReturnValue': self.manager}

    def fn_GetEnemyControllerChecked(self, args, mission, target):
        return {'ReturnValue': self.enemy}

    def fn_SetEnemyWaveSupply(self, args, mission, target):
        self.log('setup', f"enemy wave supply = {args.get('NewSupply')}")
        return {}

    def fn_GetPlayerController(self, args, mission, target):
        return {'ReturnValue': self.controller}

    def fn_MissionOnHqSpawned(self, args, mission, target):
        self.log('setup', 'HQ registered: profile start (resources, HQ cards)')
        return {}

    def deliver(self, mission, what, location, event, params):
        land = self.now + RULES['air_delivery_seconds']
        self.log('delivery', f'cargo plane with {what} sets off', end=land, stage=mission.name)
        def landed():
            self.log('delivery', f'{what} set down')
            self.check_near_triggers(location, what)
            self.run_event(mission, event, params)
        self.at(land, landed)

    def fn_DeliverPlayerNomadicByCargoPlane(self, args, mission, target):
        subtype = str(args.get('NomadicSubtype'))
        built = 'built on landing' if args.get('bBuildOnLanding') else 'truck only'
        truck = WorldObject('NomadicVehicle', subtype, vec(args.get('BuildLocation')))
        self.deliver(mission, f'{subtype} ({built})', truck.location, 'BP_OnAirDeliveredNomadicLanded',
                     {'NomadicVehicle': truck, 'NomadicSubtype': subtype, 'ID': args.get('ID') or 0})
        return {}

    def fn_DeliverPlayerVehicleByCargoPlane(self, args, mission, target):
        subtype = str(args.get('VehicleSubtype'))
        vehicle = WorldObject('TankMaster', subtype, vec(args.get('DropLocation')))
        self.deliver(mission, subtype, vehicle.location, 'BP_OnAirDeliveredVehicleLanded',
                     {'Vehicle': vehicle, 'VehicleSubtype': subtype, 'ID': args.get('ID') or 0})
        return {}

    def fn_DeliverPlayerTrainingOptionByCargoPlane(self, args, mission, target):
        vehicle = args.get('Vehicle')
        what = vehicle if isinstance(vehicle, str) else str(get_ci(vehicle, 'SubtypeValue', 'vehicle'))
        tank = WorldObject('TankMaster', what, vec(args.get('DropLocation')))
        self.deliver(mission, what, tank.location, 'BP_OnAirDeliveredVehicleLanded',
                     {'Vehicle': tank, 'VehicleSubtype': what, 'ID': args.get('ID') or 0})
        return {}

    def fn_DeliverPlayerHQByMammutGlider(self, args, mission, target):
        hq = WorldObject('NomadicVehicle', 'Nomadic_GerHq', vec(args.get('BuildLocation')))
        self.deliver(mission, 'HQ by glider', hq.location, 'BP_OnAirDeliveredNomadicLanded',
                     {'NomadicVehicle': hq, 'NomadicSubtype': 'Nomadic_GerHq', 'ID': args.get('ID') or 0})
        return {}

    def spawn_player_units(self, args, mission, what):
        location = vec(args.get('SpawnLocation'))
        self.log('setup', f'spawns {what}')
        self.check_near_triggers(location, what)
        return {}

    def fn_AsyncSpawnActorAtLocation(self, args, mission, target):
        return self.spawn_player_units(args, mission, f"unit {get_ci(args.get('TrainingOption'), 'UnitType', '')}")

    def fn_AddCustomMiniMapIcon(self, args, mission, target):
        mark = {'id': str(args.get('IconId')), 't': self.now, 'location': vec(args.get('WorldLocation')),
                'stage': mission.name}
        self.marks.append(mark)
        self.log('hint', f"minimap mark {mark['id']}", stage=mission.name)
        for convoy in self.convoys:
            self.check_mark(mark, convoy)
        return {}

    def check_mark(self, mark, convoy):
        """When a convoy on the road reaches a minimap mark: the player's reaction window from the mark."""
        route = convoy['route_obj']
        distance, off = route.distance_of(mark['location'])
        if off > RULES['mark_route_distance_cms'] or convoy['leave'] < mark['t']:
            return
        reach = convoy['time_at'](distance)
        if mark.get('removed') is not None and mark['removed'] < reach:
            return
        if reach > self.now:
            # The mark may still be removed before the convoy gets there: decide when it does.
            self.at(reach, lambda: self.record_mark_reach(mark, convoy, reach))
            return
        self.record_mark_reach(mark, convoy, reach)

    def record_mark_reach(self, mark, convoy, reach):
        if mark.get('removed') is not None and mark['removed'] < reach:
            return
        route = convoy['route_obj']
        window = reach - mark['t']
        convoy['milestones'].append({'t': round(reach, 2), 'what': f"reaches mark {mark['id']}"})
        self.log('reaction', f"{route.name} reaches mark {mark['id']} {window:.0f} s after it appeared",
                 start=max(reach, mark['t']), window=round(window, 2), mark=mark['id'], route=route.name,
                 mark_t=round(mark['t'], 2))

    def fn_RemoveCustomMiniMapIcon(self, args, mission, target):
        for mark in self.marks:
            if mark['id'] == str(args.get('IconId')) and 'removed' not in mark:
                mark['removed'] = self.now
        self.log('hint', f"minimap mark {args.get('IconId')} removed")
        return {}

    def fn_TrackActorsWithMiniMapProgress(self, args, mission, target):
        count = len(args.get('ActorsToTrack') or [])
        self.log('objective', f'{mission.title}: tracks {count} targets (player has to destroy them)')
        return {}

    def fn_GetAllActorsWithTag(self, args, mission, target):
        tag = str(args.get('Tag'))
        return {'OutActors': [a for a in self.actors if tag in a.tags]}

    def fn_GetActorOfClass(self, args, mission, target):
        wanted = norm(str(args.get('ActorClass'))).rsplit('.', 1)[-1].replace('_C', '')
        found = next((a for a in self.actors if wanted in getattr(a, 'chain', [a.kind])), None)
        return {'ReturnValue': found}

    def fn_GetAllLookoutTowers(self, args, mission, target):
        return {'ReturnValue': [a for a in self.actors if 'LookoutTowerCaptureActor' in getattr(a, 'chain', [])]}

    def fn_FindEnemyConvoyRoute(self, args, mission, target):
        route = self.routes.get(str(args.get('RouteIdToFind')))
        if route is None:
            self.unknown(f"route {args.get('RouteIdToFind')} not in the map", mission)
        return {'ReturnValue': route}

    def fn_FindEnemyArtilleryBattery(self, args, mission, target):
        wanted = str(args.get('BatteryIdToFind'))
        found = next((a for a in self.actors if a.kind == 'EnemyArtilleryBattery'
                      and str(get_ci(a.data.get('properties', {}), 'batteryId')) == wanted), None)
        return {'ReturnValue': found}

    def fn_GetSurrenderingTruckSubsystem(self, args, mission, target):
        return {'ReturnValue': self.truck_subsystem}

    def fn_GetHasMissionClassCompleted(self, args, mission, target):
        wanted = norm(str(args.get('MissionClass')))
        return {'ReturnValue': any(m.class_path == wanted and m.state == 'complete' for m in self.missions)}

    def fn_GetColumnCentre(self, args, mission, target):
        return {'ReturnValue': target.location if isinstance(target, WorldObject) else (0.0, 0.0, 0.0)}

    def fn_GetEscapedConvoyCount(self, args, mission, target):
        return {'ReturnValue': target.data.get('escaped', 0) if isinstance(target, Route) else 0}

    def fn_GetHaveCasesEscaped(self, args, mission, target):
        return {'ReturnValue': bool(self.truck_subsystem.data.get('cases_escaped'))}

    def fn_NotifyAmmoTruckArrived(self, args, mission, target):
        self.log('attack', 'ammo truck reaches the battery: battery supplied')
        return {}

    def fn_AddSuppliedSeconds(self, args, mission, target):
        self.log('attack', f"battery supplied {args.get('Seconds')} s longer")
        return {}

    def fn_CreateSingleAttackMoveDifficultyConditional(self, args, mission, target):
        self.log('attack', f"extra attack wave in {args.get('TimeTillWave')} s")
        return {}

    def fn_SpawnEscapedCasesSealedCache(self, args, mission, target):
        self.log('objective', 'escaped cases drop as a sealed cache')
        return {}

    def fn_StartRoute(self, args, mission, target):
        if not isinstance(target, Route):
            self.unknown('StartRoute on a missing route', mission)
            return {}
        self.start_route(target)
        return {}

    # ---------------------------------------------------------------------------------------- air drops
    def fn_FindEnemyAirDrop(self, args, mission, target):
        wanted = str(args.get('AirDropIdToFind'))
        found = next((a for a in self.actors if 'EnemyAirDrop' in getattr(a, 'chain', [])
                      and str(get_ci(a.data.get('properties', {}), 'airDropId')) == wanted), None)
        if found is None:
            self.unknown(f'air drop {wanted} not in the map', mission)
        return {'ReturnValue': found}

    def air_drop_vehicles(self, drop):
        """Vehicle count and delay of a placed air drop (its settings asset wins, as in AEnemyAirDrop::BeginPlay)."""
        props = drop.data.get('properties', {})
        settings = self.model.asset_props(get_ci(props, 'settingsAsset'))
        difficulty = RULES['difficulty']
        if settings:
            field = {'newtorts': 'countNewToRTS', 'normal': 'countNormal', 'hard': 'countHard',
                     'brutal': 'countBrutal', 'ironman': 'countBrutal'}[difficulty]
            options = get_ci(settings, 'vehicleOptions', [])
            what = str(get_ci(options[0], 'vehicleSubtype', 'vehicle')) if options else 'vehicle'
            return int(get_ci(settings, field, 0)), float(get_ci(settings, 'delaySeconds', 0)), what
        units = get_ci(props, 'units', {})
        lists = ['baseUnits'] + {'newtorts': [], 'normal': ['extraUnitsNormal'],
                                 'hard': ['extraUnitsNormal', 'extraUnitsHard'],
                                 'brutal': ['extraUnitsNormal', 'extraUnitsHard', 'extraUnitsBrutal'],
                                 'ironman': ['extraUnitsNormal', 'extraUnitsHard', 'extraUnitsBrutal']}[difficulty]
        count = sum(int(get_ci(e, 'count', 1)) for name in lists for e in get_ci(units, name, []) or [])
        delay = float(get_ci(get_ci(props, 'timing', {}), 'firstDropDelay', 10))
        return count, delay, 'units'

    def fn_StartAirDrop(self, args, mission, target):
        if not isinstance(target, WorldObject):
            self.unknown('StartAirDrop on a missing air drop', mission)
            return {}
        count, delay, what = self.air_drop_vehicles(target)
        depart = self.now + delay
        land = depart + RULES['air_delivery_seconds']
        self.log('attack', f'{target.name}: {count} cargo plane(s) with {what} set off (can be shot down)',
                 start=depart, end=land)

        def landed():
            for index in range(count):
                unit = WorldObject('Tank', f'{what}#{index + 1}', target.location)
                self.log('attack', f'{target.name}: {what} #{index + 1} lands')
                self.fire(target, 'OnUnitLanded', {'AirDrop': target, 'Unit': unit})
            self.fire(target, 'OnDropFinished', {'AirDrop': target, 'DropNumber': 1})
        self.at(land, landed)
        return {}

    def fn_SetMissionObjectiveText(self, args, mission, target):
        mission.title = str(args.get('Title') or mission.title)
        self.log('objective', f'~ {mission.title} (plate retitled)', kind=mission.objective, stage=mission.name)
        return {}

    def fn_K2_GetActorLocation(self, args, mission, target):
        return {'ReturnValue': target.location if isinstance(target, WorldObject) else (0.0, 0.0, 0.0)}

    def fn_AttackMoveToLocation(self, args, mission, target):
        self.log('attack', f'{getattr(target, "name", "unit")} attack-moves to {tuple(round(v) for v in vec(args.get("Location")))}')
        return {}

    def fn_Array_Add(self, args, mission, target):
        array = args.get('TargetArray')
        if isinstance(array, list):
            array.append(args.get('NewItem'))
        return {'ReturnValue': len(array) - 1 if isinstance(array, list) else 0}

    # ---------------------------------------------------------------------------------------- convoys
    def start_route(self, route):
        if route.started:
            return
        route.started = True
        timing = get_ci(route.data, 'timing', {})
        delay = float(get_ci(timing, 'firstConvoyDelay', 30))
        self.at(self.now + delay, lambda: self.spawn_convoy(route))

    def spawn_convoy(self, route):
        definition = self.model.asset_props(get_ci(route.data, 'definition'))
        name = get_ci(definition, 'displayName', route.name)
        convoy = Convoy(route, route.convoy_count, self.now)
        route.convoy_count += 1
        trucks = sum(int(get_ci(t, 'count', 1)) for t in get_ci(definition, 'trucks', []))
        tiers = get_ci(definition, 'guardTiers', [])
        escort = get_ci(route.data, 'escort', {})
        tier_index = int(get_ci(escort, 'guardTier' + {'normal': 'Normal', 'hard': 'Hard', 'brutal': 'Brutal',
                                                       'newtorts': 'NewToRTS'}[RULES['difficulty']], 0))
        tier = tiers[min(tier_index, len(tiers) - 1)] if tiers else {}
        escorts = sum(int(get_ci(e, 'count', 1)) for e in get_ci(tier, 'escorts', []))
        pace = route.pace_cms(definition)
        self.log('convoy', f'{route.name} spawns: {name}, {trucks} trucks, escort "{get_ci(tier, "tierName", "?")}" '
                           f'({escorts}), {pace / 100:.1f} m/s', route=route.name)
        self.fire(route, 'OnConvoyStarted', {'Route': route, 'Convoy': convoy})
        self.drive(route, convoy, definition, pace)

    def drive(self, route, convoy, definition, pace):
        """Schedule the column's milestones along the route (passive: nobody attacks it)."""
        speed = get_ci(definition, 'speed', {})
        forming = float(get_ci(speed, 'formingSeconds', 0))
        cumulative = route.cumulative()
        total = cumulative[-1] if cumulative else 0.0
        milestones = []
        stop = get_ci(route.data, 'stop', {})
        if get_ci(stop, 'bEnabled'):
            distance, off = route.distance_of(route.stop_point)
            milestones.append((distance, 'stop', float(get_ci(stop, 'stopSeconds', 0))))
        hunters = get_ci(route.data, 'hunters', {})
        if get_ci(hunters, 'bEnabled'):
            distance, _ = route.distance_of(route.dismount_point)
            milestones.append((distance, 'dismount', float(get_ci(hunters, 'dismountHaltSeconds', 0))))
        branch = get_ci(route.data, 'branch', {})
        if get_ci(branch, 'bEnabled') and route.branch_points:
            distance, _ = route.distance_of(route.branch_points[0])
            milestones.append((distance, 'branch', 0.0))
        for index, point in enumerate(route.zone_points):
            distance, _ = route.distance_of(point)
            milestones.append((distance, f'ambush zone {index + 1}', 0.0))
        milestones.sort()
        ambush = get_ci(route.data, 'ambush', {})
        boost = float(get_ci(ambush, 'speedMultiplier', 1.0)) if get_ci(ambush, 'radioPostActor', 'None') != 'None' \
            else 1.0
        zone_radius = float(get_ci(ambush, 'zoneRadius', 0))
        zones = [route.distance_of(p)[0] for p in route.zone_points]

        def travel(a, b):
            """Seconds from distance a to b; inside an ambush zone the column runs at the radio-post boost."""
            seconds, step = 0.0, 100.0
            position = a
            while position < b - 1e-6:
                nxt = min(b, position + step)
                in_zone = any(abs((position + nxt) / 2 - z) <= zone_radius for z in zones)
                seconds += (nxt - position) / (pace * (boost if in_zone else 1.0))
                position = nxt
            return seconds

        start_time = self.now + forming
        holds = [(distance, hold) for distance, _, hold in milestones if hold > 0]

        def time_at(distance):
            return start_time + travel(0.0, distance) + sum(h for d, h in holds if d < distance)

        record = {'route': route.name, 'route_obj': route, 'name': get_ci(definition, 'displayName', route.name),
                  'spawn': round(self.now, 2), 'leave': None, 'pace_ms': round(pace / 100.0, 2),
                  'milestones': [], 'time_at': time_at}
        time = start_time
        position = 0.0
        for distance, what, hold in milestones:
            time += travel(position, distance)
            position = distance
            record['milestones'].append({'t': round(time, 2), 'what': what, 'hold': hold})
            if what == 'stop':
                self.log('convoy', f'{route.name} stops at the stop point for {hold:.0f} s', start=time,
                         end=time + hold, route=route.name)
            elif what == 'dismount':
                self.log('convoy', f'{route.name} hunters dismount and attack', start=time, route=route.name)
            elif what == 'branch':
                self.branch(route, convoy, definition, pace, time)
            else:
                self.log('convoy', f'{route.name} enters {what}', start=time, route=route.name)
            time += hold
        time += travel(position, total)
        route.data['escaped'] = route.data.get('escaped', 0) + 1
        record['leave'] = round(time, 2)
        self.convoys.append(record)
        for mark in self.marks:
            self.check_mark(mark, record)

        def finished():
            self.log('convoy', f'{route.name} leaves the map (escaped)', route=route.name)
            if self.model.asset_props(get_ci(route.data, 'definition')) and \
                    any('CaseTruck' in str(get_ci(t, 'truckClass', '')) for t in get_ci(definition, 'trucks', [])):
                self.truck_subsystem.data['cases_escaped'] = True
            self.fire(route, 'OnConvoyFinished', {'Route': route, 'Convoy': convoy, 'Outcome': 'Escaped'})
        self.at(time, finished)

    def branch(self, route, convoy, definition, pace, time):
        branch = get_ci(route.data, 'branch', {})
        cargo = get_ci(branch, 'cargoType', 'Ammo')
        length = sum(dist2d(a, b) for a, b in zip(route.branch_points, route.branch_points[1:]))
        speed = pace * float(get_ci(branch, 'speedMultiplier', 1.0))
        arrive = time + length / speed
        self.log('convoy', f'{route.name}: {cargo} trucks turn off ({length / 100:.0f} m to go)', start=time,
                 route=route.name)
        trucks = [t for t in get_ci(definition, 'trucks', []) if get_ci(t, 'bOverrideCargoType')
                  and get_ci(t, 'cargoTypeOverride') == cargo]
        count = sum(int(get_ci(t, 'count', 1)) for t in trucks) or 1

        def arrived():
            self.log('convoy', f'{route.name}: {count} {cargo} truck(s) arrive at the branch end', route=route.name)
            for index in range(count):
                self.fire(route, 'OnBranchTruckArrived', {'Route': route, 'Truck': WorldObject('Truck', f'{cargo}{index}'),
                                                          'Cargo': cargo, 'ConvoyIndex': convoy.index})
        self.at(arrive, arrived)


class _Args:
    """Lazy access to a call node's input pins by name."""

    def __init__(self, sim, frame, node):
        self.sim, self.frame, self.node = sim, frame, node

    def get(self, name, default=None):
        if name not in self.node['pin_by_name']:
            for pin_name in self.node['pin_by_name']:
                if low(pin_name) == low(name):
                    name = pin_name
                    break
            else:
                return default
        value = self.sim.eval_in(self.frame, self.node, name)
        return default if value is None else value


def _v(args, name):
    value = args.get(name)
    return vec(value) if not isinstance(value, tuple) else value


def _num(args, name):
    value = args.get(name, 0)
    return float(value) if not isinstance(value, bool) else float(value)


PURE = {
    'Add_IntInt': lambda a: int(_num(a, 'A') + _num(a, 'B')),
    'Subtract_IntInt': lambda a: int(_num(a, 'A') - _num(a, 'B')),
    'Multiply_IntInt': lambda a: int(_num(a, 'A') * _num(a, 'B')),
    'Add_DoubleDouble': lambda a: _num(a, 'A') + _num(a, 'B'),
    'Subtract_DoubleDouble': lambda a: _num(a, 'A') - _num(a, 'B'),
    'Multiply_DoubleDouble': lambda a: _num(a, 'A') * _num(a, 'B'),
    'Divide_DoubleDouble': lambda a: _num(a, 'A') / (_num(a, 'B') or 1.0),
    'Less_DoubleDouble': lambda a: _num(a, 'A') < _num(a, 'B'),
    'Greater_DoubleDouble': lambda a: _num(a, 'A') > _num(a, 'B'),
    'LessEqual_DoubleDouble': lambda a: _num(a, 'A') <= _num(a, 'B'),
    'GreaterEqual_DoubleDouble': lambda a: _num(a, 'A') >= _num(a, 'B'),
    'EqualEqual_DoubleDouble': lambda a: abs(_num(a, 'A') - _num(a, 'B')) < 1e-4,
    'Less_IntInt': lambda a: _num(a, 'A') < _num(a, 'B'),
    'Greater_IntInt': lambda a: _num(a, 'A') > _num(a, 'B'),
    'GreaterEqual_IntInt': lambda a: _num(a, 'A') >= _num(a, 'B'),
    'EqualEqual_IntInt': lambda a: _num(a, 'A') == _num(a, 'B'),
    'Conv_IntToDouble': lambda a: _num(a, 'InInt'),
    'Conv_IntToFloat': lambda a: _num(a, 'InInt'),
    'BooleanAND': lambda a: bool(a.get('A')) and bool(a.get('B')),
    'BooleanOR': lambda a: bool(a.get('A')) or bool(a.get('B')),
    'Not_PreBool': lambda a: not bool(a.get('A')),
    'NotEqual_ByteByte': lambda a: byte_value(a.get('A')) != byte_value(a.get('B')),
    'EqualEqual_ByteByte': lambda a: byte_value(a.get('A')) == byte_value(a.get('B')),
    'EqualEqual_ObjectObject': lambda a: a.get('A') is a.get('B'),
    'EqualEqual_ClassClass': lambda a: norm(str(a.get('A'))) == norm(str(a.get('B'))),
    'MakeVector': lambda a: (_num(a, 'X'), _num(a, 'Y'), _num(a, 'Z')),
    'Add_VectorVector': lambda a: tuple(x + y for x, y in zip(_v(a, 'A'), _v(a, 'B'))),
    'Subtract_VectorVector': lambda a: tuple(x - y for x, y in zip(_v(a, 'A'), _v(a, 'B'))),
    'Multiply_VectorFloat': lambda a: tuple(x * _num(a, 'B') for x in _v(a, 'A')),
    'Array_Length': lambda a: len(a.get('TargetArray') or []),
}


# ------------------------------------------------------------------------------------------------ output

LANE_ORDER = ['objective', 'cinematic', 'voice', 'delivery', 'convoy', 'attack', 'hint', 'trigger', 'setup']


def describe_trigger(sim, mission):
    """What starts a stage, for the stage-flow graph: {'kind': start|after|near|other, ...}."""
    trigger = mission.trigger
    if not trigger:
        return {'kind': 'start'}
    by_class = {m.class_path: m.name for m in sim.missions}
    cls = trigger.get('_class', '')
    if 'MissionClassCompleted' in cls:
        watched = [norm(get_ci(trigger, 'missionClassToWatch', ''))]
        watched += [norm(c) for c in get_ci(trigger, 'alsoWatchMissionClasses', []) or []]
        return {'kind': 'after', 'stages': [by_class.get(w, w) for w in watched],
                'or_failed': bool(get_ci(trigger, 'bAlsoTriggerWhenFailed', False))}
    if 'UnitsNear' in cls:
        centre = vec(get_ci(trigger, 'centreLocation'))
        return {'kind': 'near', 'centre': centre, 'radius': float(get_ci(trigger, 'radius', 0))}
    return {'kind': 'other', 'class': cls}


def write_outputs(sim, folder, map_name):
    lines = [f'Mission timeline estimate: {map_name} (scenario {sim.scenario}, difficulty {RULES["difficulty"]})',
             'Assumptions: ' + ', '.join(f'{k}={v}' for k, v in RULES.items()), '']
    for item in sim.timeline:
        span = f' -> {mmss(item["end"])}' if 'end' in item and item['end'] > item['t'] + 0.5 else ''
        lines.append(f'{mmss(item["t"])}{span:9s}  {item["lane"]:9s}  {item["text"]}')
    lines += ['', 'Stages:']
    for mission in sim.missions:
        started = mmss(mission.started_at) if mission.started_at is not None else '  -  '
        done = mmss(mission.completed_at) if mission.completed_at is not None else '  -  '
        lines.append(f'  {mission.name:38s} {mission.objective:6s} start {started}  done {done}  "{mission.title}"')
    lines += ['', 'Unmodelled (counted as 0 s):'] + [f'  {count}x {what}' for what, count in
                                                      sorted(sim.unmodelled.items())]
    data = {'map': map_name, 'scenario': sim.scenario, 'rules': dict(RULES), 'events': sim.timeline,
            'end': round(max([i.get('end', i['t']) for i in sim.timeline] + [0]), 2),
            'stages': [{'stage': m.name, 'title': m.title, 'objective': m.objective, 'start': m.started_at,
                        'done': m.completed_at, 'trigger': describe_trigger(sim, m)} for m in sim.missions],
            'convoys': [{k: v for k, v in c.items() if k not in ('route_obj', 'time_at')} for c in sim.convoys],
            'marks': [{k: v for k, v in m.items()} for m in sim.marks],
            'unmodelled': sim.unmodelled}
    import report  # noqa: E402  (sibling module)
    warnings = report.pacing_checks(data)
    data['warnings'] = warnings
    lines += ['', 'Pacing warnings:'] + [f'  {mmss(w["t"])}  {w["kind"]:10s} {w["text"]}' for w in warnings]
    text = '\n'.join(lines)
    with open(os.path.join(folder, 'timeline.txt'), 'w', encoding='utf-8') as handle:
        handle.write(text)
    with open(os.path.join(folder, 'timeline.json'), 'w', encoding='utf-8') as handle:
        json.dump(data, handle, indent=1, default=str)
    with open(os.path.join(folder, 'timeline.html'), 'w', encoding='utf-8') as handle:
        handle.write(report.render_html(data))
    return text


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('map')
    parser.add_argument('--scenario', default='passive', choices=['passive'])
    parser.add_argument('--set', action='append', default=[], help='override an assumption: key=value')
    parser.add_argument('--until', type=float, default=3600.0)
    parser.add_argument('--pace', action='append', default=[], help='override a pacing threshold: key=value')
    arguments = parser.parse_args()
    import report  # noqa: E402  (sibling module)
    for item in arguments.pace:
        key, value = item.split('=', 1)
        report.PACING[key] = type(report.PACING.get(key, 0.0))(value)
    for item in arguments.set:
        key, value = item.split('=', 1)
        RULES[key] = type(RULES.get(key, ''))(value) if key in RULES and not isinstance(RULES[key], str) else value
    folder = os.path.join(SAVED, arguments.map)
    with open(os.path.join(folder, 'model.json'), encoding='utf-8') as handle:
        model = Model(json.load(handle))
    sim = Simulator(model, arguments.scenario)
    sim.run(arguments.until)
    print(write_outputs(sim, folder, arguments.map))


if __name__ == '__main__':
    sys.setrecursionlimit(5000)
    main()
