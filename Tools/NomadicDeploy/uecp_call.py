"""Minimal UECP bridge client (raw HTTP; the bridge's chunked replies break normal HTTP clients).
Usage:
  python uecp_call.py py <file.py> [--var NAME=VALUE ...]   run a python file in the editor (execute_python);
                                                             each --var becomes a python global set before the file
  python uecp_call.py uecp <file.py>                         same through execute_uecp_script (uecp.* available)
  python uecp_call.py tool <action> '<json params>'          call any bridge action through call_action
  --port N picks another editor instance (default broker 9855). Prints the tool's text output.
The token is read from %LOCALAPPDATA%/UECP/session_token.txt at call time and never printed.
"""
import json
import os
import socket
import sys

HOST = '127.0.0.1'


def call(name, arguments, port=9855, timeout=900):
    token = open(os.path.join(os.environ['LOCALAPPDATA'], 'UECP', 'session_token.txt')).read().strip()
    body = json.dumps({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                       'params': {'name': name, 'arguments': arguments}}).encode('utf-8')
    head = (f'POST /uecp/RTS_Survival/mcp HTTP/1.1\r\nHost: {HOST}:{port}\r\nAuthorization: Bearer {token}\r\n'
            f'Content-Type: application/json\r\nAccept: application/json, text/event-stream\r\n'
            f'Content-Length: {len(body)}\r\nConnection: close\r\n\r\n').encode('utf-8')
    with socket.create_connection((HOST, port), timeout=timeout) as s:
        s.sendall(head + body)
        chunks = []
        while True:
            data = s.recv(65536)
            if not data:
                break
            chunks.append(data)
    raw = b''.join(chunks).decode('utf-8', 'replace')
    payload = raw.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in raw else raw
    msg = None
    # SSE: events are separated by blank lines and one event may span several data: lines
    candidates = []
    for event in payload.replace('\r\n', '\n').split('\n\n'):
        data_lines = [line[5:] for line in event.split('\n') if line.startswith('data:')]
        if data_lines:
            candidates.append('\n'.join(data_lines))
    candidates = candidates or [payload]
    for text in candidates:
        start, end = text.find('{'), text.rfind('}')
        try:
            parsed = json.loads(text[start:end + 1])
        except ValueError:
            continue
        if isinstance(parsed, dict) and ('result' in parsed or 'error' in parsed):
            msg = parsed
    if msg is None:
        # unframed body: the result object is the last JSON object that has an id
        for line in payload.splitlines():
            start = line.find('{')
            try:
                parsed = json.loads(line[start:line.rfind('}') + 1])
            except ValueError:
                continue
            if isinstance(parsed, dict) and 'id' in parsed:
                msg = parsed
    if msg is None:
        return payload
    result = msg.get('result', msg)
    texts = [c.get('text', '') for c in result.get('content', [])] if isinstance(result, dict) else []
    return '\n'.join(texts) if texts else json.dumps(result)


def unwrap(text):
    try:
        j = json.loads(text)
    except ValueError:
        return text
    if isinstance(j, dict):
        out = j.get('output_text') or ''
        err = ''
        if not j.get('ok', True):  # keep the traceback (in 'result') next to the generic error message
            err = ' | '.join(str(x) for x in (j.get('error'), j.get('result')) if x and x != 'None')
        return (out + ('\nERROR: ' + str(err) if err else '')).strip() or json.dumps(j)[:4000]
    return text


def with_editor_lock(fn):
    """Command-line calls take the shared editor lock (ue_pipeline.py holds it itself and calls call() directly)."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import locks
    locks.acquire('editor.lock', 'uecp_call ' + ' '.join(sys.argv[1:3]))
    try:
        return fn()
    finally:
        locks.release('editor.lock')


def main(argv):
    if os.environ.get('NOMADIC_LOCK_HELD') != '1':
        os.environ['NOMADIC_LOCK_HELD'] = '1'
        return with_editor_lock(lambda: main(argv))
    port = 9855
    if '--port' in argv:
        i = argv.index('--port')
        port = int(argv[i + 1])
        del argv[i:i + 2]
    variables = {}
    while '--var' in argv:
        i = argv.index('--var')
        k, v = argv[i + 1].split('=', 1)
        variables[k] = v
        del argv[i:i + 2]
    mode = argv[1]
    if mode in ('py', 'uecp'):
        path = os.path.abspath(argv[2]).replace('\\', '/')
        prelude = ''.join(f'{k} = {json.dumps(v)}\n' for k, v in variables.items())
        code = prelude + f"exec(open({json.dumps(path)}, encoding='utf-8').read(), globals())\n"
        action = 'execute_python' if mode == 'py' else 'execute_uecp_script'
        print(unwrap(call('call_action', {'action': action, 'params': {'code': code, 'description': os.path.basename(path)}},
                          port)))
    else:
        params = json.loads(argv[3]) if len(argv) > 3 else {}
        print(unwrap(call('call_action', {'action': argv[2], 'params': params}, port)))


if __name__ == '__main__':
    main(sys.argv)
