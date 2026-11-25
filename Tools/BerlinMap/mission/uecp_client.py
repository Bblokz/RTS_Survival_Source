"""Tiny UECP client for one editor instance (direct port). call(port, tool, args) -> parsed JSON (or text)."""
import json
import os
import socket


def _post(port, body):
    token = open(os.path.expandvars(r'%LOCALAPPDATA%\UECP\session_token.txt'), encoding='utf-8').read().strip()
    request = (f'POST /mcp HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\nContent-Type: application/json\r\n'
               f'Accept: application/json, text/event-stream\r\nAuthorization: Bearer {token}\r\n'
               f'Content-Length: {len(body)}\r\nConnection: close\r\n\r\n').encode('utf-8') + body
    # UECP_HOST=::1 picks an editor that is the only one on the IPv6 loopback when two share a port.
    sock = socket.create_connection((os.environ.get('UECP_HOST', '127.0.0.1'), port), timeout=900)
    sock.sendall(request)
    chunks = []
    while True:
        data = sock.recv(65536)
        if not data:
            break
        chunks.append(data)
    sock.close()
    raw = b''.join(chunks).decode('utf-8', errors='replace')
    return raw.split('\r\n\r\n', 1)[1] if '\r\n\r\n' in raw else raw


def _pick_response(payload):
    # SSE or plain JSON, possibly several objects; return the one carrying the request id.
    text = '\n'.join(line[5:] if line.startswith('data:') else line for line in payload.splitlines())
    decoder = json.JSONDecoder()
    index, found = 0, None
    while index < len(text):
        start = text.find('{', index)
        if start < 0:
            break
        try:
            message, end = decoder.raw_decode(text, start)
        except json.JSONDecodeError:
            index = start + 1
            continue
        if isinstance(message, dict) and 'id' in message:
            found = message
        index = end
    if found is None:
        raise RuntimeError('no response event: ' + payload[:500])
    return found


def call(port, tool, args):
    body = json.dumps({'jsonrpc': '2.0', 'id': 1, 'method': 'tools/call',
                       'params': {'name': tool, 'arguments': args}}).encode('utf-8')
    message = _pick_response(_post(port, body))
    texts = [c.get('text', '') for c in message.get('result', {}).get('content', []) if c.get('type') == 'text']
    text = '\n'.join(texts)
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


def action(port, name, params):
    return call(port, 'call_action', {'action': name, 'params': params})
