"""Create or rebuild the Frozen Lake stage Blueprints over UECP.

Usage: python build_fl_stages.py <port> [StageName ...]
Re-runnable: existing Blueprints keep their variables (missing ones are added), functions are regenerated whole and
the event graph is rebuilt from fl_stages.py. Saves only these assets.
"""
import copy
import json
import sys

from uecp_client import action, call
from fl_stages import STAGES, path_of

MGR = '=GetMissionManagerChecked()'


def fix_manager_refs(body):
    """'=GetMissionManagerChecked()' -> one GetMissionManagerChecked call bound as $Mgr at the top of the body."""
    text = json.dumps(body)
    if MGR not in text:
        return body
    body = json.loads(text.replace(json.dumps(MGR), json.dumps('$Mgr')))
    return [{'op': 'call', 'func': 'GetMissionManagerChecked', 'as': 'Mgr'}] + body


def ok(result):
    return isinstance(result, dict) and result.get('ok', True) and not result.get('error')


def log(*parts):
    print(*parts, flush=True)


def ensure_blueprint(port, stage):
    path = path_of(stage)
    skel = action(port, 'get_blueprint_skeleton', {'blueprint_path': path})
    if ok(skel) and skel.get('blueprint_path'):
        return skel
    folder, name = path.rsplit('/', 1)
    created = call(port, 'create_asset', {'asset_type': 'Blueprint', 'name': name, 'save_path': folder,
                                          'parent_class': stage['parent']})
    log('  create', stage['name'], 'ok' if ok(created) else created)
    return action(port, 'get_blueprint_skeleton', {'blueprint_path': path})


def add_variables(port, stage, skel):
    have = {v['name'] for v in skel.get('variables', [])}
    items = [v for v in stage.get('vars', []) + stage.get('array_vars', []) if v['variable_name'] not in have]
    if items:
        result = action(port, 'add_variable', {'blueprint_path': path_of(stage), 'items': items})
        log('  vars added', result.get('added'), 'failed', result.get('failed'), '' if ok(result) else result)


def compile_bp(port, stage):
    result = action(port, 'compile_blueprint', {'blueprint_path': path_of(stage)})
    errors = result.get('error_count') if isinstance(result, dict) else result
    if errors:
        log('  COMPILE ERRORS', json.dumps(result)[:1500])
    return errors


def build_functions(port, stage):
    funcs = stage['functions']
    order = ['PlayLine'] + [n for n in funcs if n != 'PlayLine']
    failed = []
    for name in order:
        if name not in funcs:
            continue
        inputs, outputs, body = funcs[name]
        args = {'blueprint_path': path_of(stage), 'function': name, 'body': fix_manager_refs(copy.deepcopy(body)),
                'overwrite': True}
        if inputs:
            args['inputs'] = inputs
        if outputs:
            args['outputs'] = outputs
        result = action(port, 'synthesize_graph', args)
        build = result.get('build', {}) if isinstance(result, dict) else {}
        bad = [k for k in ('nodes', 'connections', 'defaults') if build.get(k, {}).get('failed')]
        if not ok(result) or bad:
            failed.append(name)
            log('  FUNCTION', name, json.dumps(result)[:1200])
        if name == 'PlayLine':
            compile_bp(port, stage)
    log('  functions', len(order) - len(failed), 'ok', ('failed: ' + ', '.join(failed)) if failed else '')
    return failed


def build_event_graph(port, stage):
    import re
    import fl_graph
    fl_graph.SELF_SUBTITLE = 'Target is ' + re.sub(r'_', ' ', re.sub(r'(?<=[a-z])(?=[A-Z])', ' ', stage['name']))
    g = stage['graph']()
    result = action(port, 'build_blueprint_graph', {
        'blueprint_path': path_of(stage), 'graph_name': 'EventGraph', 'clear_before_build': True,
        'clear_force': True, 'nodes': g.nodes, 'connections': g.connections, 'defaults': g.defaults,
        'comments': g.comments})
    summary = {k: result.get(k) for k in ('ok', 'nodes', 'connections', 'defaults')} if isinstance(result, dict) \
        else str(result)[:300]
    log('  event graph', json.dumps(summary)[:600])
    if not ok(result):
        log('   ', json.dumps(result)[:1500])


def set_variable_defaults(port, stage):
    for name, value in stage.get('cdo', {}).items():
        result = action(port, 'set_blueprint_variable_default', {'blueprint_path': path_of(stage),
                                                                 'variable_name': name, 'default_value': value})
        log('  default', name, 'ok' if ok(result) else json.dumps(result)[:400])


PY_CDO = r'''
import unreal
path = "{path}"
bp = unreal.load_asset(path)
cls = unreal.load_class(None, path + "." + path.rsplit("/", 1)[1] + "_C")
cdo = unreal.get_default_object(cls)
state = cdo.get_editor_property("mission_widget_state")
state.import_text('(MissionTitle=INVTEXT("{title}"),MissionDescription=INVTEXT("{description}"),'
                  'TextSpeed=TS_None,NextButtonType=Btn_Next,ObjectiveType={objective})')
cdo.set_editor_property("mission_widget_state", state)
for key, value in {bools!r}.items():
    cdo.set_editor_property(key, value)
cdo.modify()
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
check = unreal.get_default_object(cls).get_editor_property("mission_widget_state").export_text()
open(unreal.Paths.project_saved_dir() + "FrozenLake/cdo_" + path.rsplit("/", 1)[1] + ".txt", "w").write(
    str(saved) + "\n" + check)
'''


def set_plate_and_save(port, stage):
    p = stage['plate']
    code = PY_CDO.format(path=path_of(stage), title=p['title'], description=p['description'],
                         objective=p['objective'], bools=stage.get('cdo_bool', {}))
    result = action(port, 'execute_python', {'code': code, 'description': 'Set FrozenLake stage plate and save'})
    if not ok(result):
        log('  CDO/SAVE', json.dumps(result)[:800])
    try:
        text = open(r'D:\UE5Projects\RTS_Survival\Saved\FrozenLake\cdo_' + stage['name'] + '.txt').read()
        log('  saved/plate', text.replace('\n', ' | ')[:300])
    except OSError as error:
        log('  no CDO report', error)


def build(port, stage):
    log('==', stage['name'])
    skel = ensure_blueprint(port, stage)
    add_variables(port, stage, skel)
    compile_bp(port, stage)
    failed = build_functions(port, stage)
    build_event_graph(port, stage)
    set_variable_defaults(port, stage)
    errors = compile_bp(port, stage)
    set_plate_and_save(port, stage)
    return failed, errors


def main():
    port = int(sys.argv[1])
    wanted = set(sys.argv[2:])
    report = {}
    for stage in STAGES:
        if wanted and stage['name'] not in wanted:
            continue
        report[stage['name']] = build(port, stage)
    log('\nSUMMARY')
    for name, (failed, errors) in report.items():
        log(f'  {name}: functions failed {failed or "none"}, compile errors {errors or 0}')


if __name__ == '__main__':
    main()
