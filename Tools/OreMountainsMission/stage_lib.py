"""Helpers to create the Ore Mountains stage Blueprints over UECP (our own editor, direct port; see ue.py).

Graph is Berlin's hand-layout helper (Source/Tools/BerlinMap/mission/layout_eventgraphs.py): rows read left to
right, a scheduled wait's custom event sits right after its Schedule node, getters sit under the pin they feed.
"""
import json

import ue
import layout_eventgraphs as _layout
from layout_eventgraphs import Graph, var  # noqa: F401  (re-exported for the stage scripts)

_berlin_height = _layout.height
# Real heights (px) of Ore nodes taller than the Berlin estimate, so the next row and its comment never overlap.
_TALL = {'IsCasesStageEnding': 165, 'TriggerVictory': 140, 'GetHasMissionClassCompleted': 110,
         'DispatcherBind': 120, 'PlayPortrait': 185, 'ScheduleSingle': 170, 'PlayAdvisorLine': 135, 'TrackTaggedTargets': 140}


def _ore_height(handle):
    for key, value in _TALL.items():
        if key in handle:
            return max(value, _berlin_height(handle))
    return _berlin_height(handle)


_layout.height = _ore_height

ORE_BASE_CLASS = ue.BASE + 'Mission_Ore_Base.Mission_Ore_Base_C'
MACROS = 'mc.BPML_OreMission.'


def log(tag, result, limit=600):
    text = json.dumps(result) if not isinstance(result, str) else result
    ok = result.get('ok') if isinstance(result, dict) else None
    print(f'  {tag}: ok={ok} {text[:limit] if ok is not True else ""}', flush=True)
    return result


def create_stage(name, parent_class):
    path = ue.BASE + name
    exists = ue.py(f"import unreal\nprint(unreal.EditorAssetLibrary.does_asset_exist('{path}'))")
    if 'True' in json.dumps(exists):
        print(f'{name}: exists')
        return path
    log('create', ue.a('create_asset', asset_type='blueprint', name=name, save_path=ue.BASE.rstrip('/'),
                       parent_class=parent_class))
    return path


def own_var_names(path):
    skeleton = ue.a('get_blueprint_skeleton', blueprint_path=path)
    return {v.get('name') for v in skeleton.get('variables', [])} if isinstance(skeleton, dict) else set()


def remove_own_vars(path, names):
    for name in names:
        if name in own_var_names(path):
            log(f'remove var {name}', ue.a('delete_variable', blueprint_path=path, variable_name=name))


def add_vars(path, variables):
    """variables: (name, type, default or None, category, instance_editable[, tooltip]); existing ones are kept."""
    have = own_var_names(path)
    items = []
    for v in variables:
        if v[0] in have:
            continue
        name, vtype, default, category, editable = v[:5]
        item = {'variable_name': name, 'variable_type': vtype, 'category': category, 'instance_editable': editable}
        if default is not None:
            item['default_value'] = default
        if len(v) > 5:
            item['tooltip'] = v[5]
        items.append(item)
    if items:
        log('vars', ue.a('add_variable', blueprint_path=path, items=items))


def syn(path, fn, body, inputs=None, outputs=None, pure=False):
    result = ue.a('synthesize_graph', blueprint_path=path, function=fn, body=body, inputs=inputs or [],
                  outputs=outputs or [], overwrite=True)
    log(f'fn {fn}', result)
    if pure:
        log(f'pure {fn}', ue.a('set_function_pure', blueprint_path=path, function_name=fn, is_pure=True))
    return result


def compile_bp(path):
    result = ue.a('compile_blueprint', blueprint_path=path)
    errors = result.get('error_count') if isinstance(result, dict) else result
    print(f'  compile errors={errors}', '' if not errors else json.dumps(result)[:1500], flush=True)
    return errors


def build_event_graph(path, g):
    result = ue.a('build_blueprint_graph', blueprint_path=path, graph_name='EventGraph', clear_before_build=True,
                  clear_force=True, nodes=g.nodes, connections=g.connections, defaults=g.defaults,
                  comments=g.comments)
    if isinstance(result, dict) and not result.get('ok'):
        print('  eventgraph FAILED', json.dumps(result)[:2500], flush=True)
    else:
        print('  eventgraph ok', {k: result.get(k) for k in ('nodes', 'connections', 'defaults')}, flush=True)
    return result


def set_defaults(path, values):
    """values: {property: python literal string} on the CDO, set after a compile."""
    lines = ['import unreal', f"bp = unreal.load_asset('{path}')",
             'cdo = unreal.get_default_object(bp.generated_class())']
    for prop, literal in values.items():
        lines.append(f"cdo.set_editor_property('{prop}', {literal})")
    lines.append("print('defaults set')")
    log('defaults', ue.py('\n'.join(lines)))


def save(path):
    log('save', ue.py(f"import unreal\nprint(unreal.EditorAssetLibrary.save_asset('{path}', only_if_is_dirty=False))"))


def ensure_vars(path, variables):
    """add_vars for the variables the Blueprint does not have yet (re-runnable on an existing Blueprint)."""
    skeleton = ue.a('get_blueprint_skeleton', blueprint_path=path)
    have = {v.get('name') for v in skeleton.get('variables', [])} if isinstance(skeleton, dict) else set()
    add_vars(path, [v for v in variables if v[0] not in have])


_berlin_end_row = Graph.end_row


def _end_row_with_param_events(self):
    """Custom events with parameters grow ~30 px per pin; make the row's bottom cover them before the next row."""
    for node in self.nodes:
        params = node.get('params')
        if params and node['id'] in self.pos:
            self.bottom = max(self.bottom, self.pos[node["id"]][1] + 90 + 40 * len(params))
    _berlin_end_row(self)


Graph.end_row = _end_row_with_param_events


def remove_functions(path, names):
    skeleton = ue.a('get_blueprint_skeleton', blueprint_path=path)
    have = {f.get('name') for f in skeleton.get('functions', [])} if isinstance(skeleton, dict) else set()
    for name in names:
        if name in have:
            log(f'remove function {name}', ue.a('delete_function', blueprint_path=path, function_name=name))
