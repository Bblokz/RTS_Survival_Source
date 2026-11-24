"""UECP client for this session's own editor (direct port from %LOCALAPPDATA%/UECP/instances/<pid>.json).
CLI: python u.py py <file.py> [more files concatenated]   |  python u.py act <action> '<json params>'  |  python u.py tools <query>
Env UECP_PID picks the instance file; default = newest instance whose pid is alive."""
import glob
import json
import os
import socket
import subprocess
import sys

HOST = '127.0.0.1'


def _instance():
    pid = os.environ.get('UECP_PID')
    files = glob.glob(os.path.join(os.environ['LOCALAPPDATA'], 'UECP', 'instances', '*.json'))
    best = None
    for f in files:
        data = json.load(open(f))
        if pid and str(data['pid']) != pid:
            continue
        alive = subprocess.run(['tasklist', '/FI', f'PID eq {data["pid"]}'], capture_output=True, text=True).stdout
        if str(data['pid']) not in alive:
            continue
        if best is None or data['started_at'] > best['started_at']:
            best = data
    if best is None:
        raise SystemExit('no live UECP instance')
    return best['mcp_http_port']


def call(name, arguments, timeout=1800):
    port = _instance()
    token = open(os.path.join(os.environ['LOCALAPPDATA'], 'UECP', 'session_token.txt')).read().strip()
    body = json.dumps({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                       'params': {'name': name, 'arguments': arguments}}).encode('utf-8')
    head = (f'POST /mcp HTTP/1.1\r\nHost: {HOST}:{port}\r\nAuthorization: Bearer {token}\r\n'
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
    candidates = []
    for event in payload.replace('\r\n', '\n').split('\n\n'):
        lines = [line[5:] for line in event.split('\n') if line.startswith('data:')]
        if lines:
            candidates.append('\n'.join(lines))
    candidates = candidates or [payload]
    msg = None
    for text in candidates:
        start, end = text.find('{'), text.rfind('}')
        try:
            parsed = json.loads(text[start:end + 1], strict=False)
        except ValueError:
            continue
        if isinstance(parsed, dict) and ('result' in parsed or 'error' in parsed):
            msg = parsed
    if msg is None:
        return payload
    result = msg.get('result', msg)
    texts = [c.get('text', '') for c in result.get('content', [])] if isinstance(result, dict) else []
    return '\n'.join(texts) if texts else json.dumps(result)


def act(action, **params):
    text = call('call_action', {'action': action, 'params': params})
    try:
        return json.loads(text, strict=False)
    except ValueError:
        return {'raw': text}


def py(code, description='sd bxp round 2'):
    out = act('execute_python', code=code, description=description)
    if isinstance(out, dict):
        text = out.get('output_text') or ''
        if not out.get('ok', True):
            text += '\nERROR: ' + str(out.get('error')) + ' | ' + str(out.get('result'))
        return text or json.dumps(out)[:3000]
    return str(out)


if __name__ == '__main__':
    mode = sys.argv[1]
    if mode == 'py':
        code = '\n'.join(open(f, encoding='utf-8').read() for f in sys.argv[2:])
        print(py(code))
    elif mode == 'act':
        print(json.dumps(act(sys.argv[2], **(json.loads(sys.argv[3]) if len(sys.argv) > 3 else {})), indent=1)[:20000])
    elif mode == 'tools':
        print(call('search_tools', {'query': sys.argv[2]})[:8000])
