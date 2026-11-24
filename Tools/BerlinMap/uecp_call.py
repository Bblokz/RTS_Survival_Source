"""Minimal client for the editor's UECP bridge, for when the MCP tool connection of a session is gone.

    python uecp_call.py <script.py> ["description"]      runs the file through the bridge's execute_python action
    python uecp_call.py -c "print(1)" ["description"]

Reads the bearer token from %LOCALAPPDATA%/UECP/session_token.txt at call time (never printed). The server's HTTP
replies claim chunked encoding but are not framed, so this talks over a raw socket and reads until the peer closes
(Source/AGENTS.md, "Local Unreal Editor Connection Memory"). Prints the action's output text.
"""
import json
import os
import socket
import sys

HOST, PORT, PATH = "127.0.0.1", 9855, "/uecp/RTS_Survival/mcp"


def call(code, description, timeout=900.0):
    with open(os.path.join(os.environ["LOCALAPPDATA"], "UECP", "session_token.txt")) as handle:
        token = handle.read().strip()
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                       "params": {"name": "call_action",
                                  "arguments": {"action": "execute_python",
                                                "params": {"code": code, "description": description}}}}).encode("utf-8")
    head = ("POST %s HTTP/1.1\r\nHost: %s:%d\r\nAuthorization: Bearer %s\r\nContent-Type: application/json\r\n"
            "Accept: application/json, text/event-stream\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % (
                PATH, HOST, PORT, token, len(body))).encode("ascii")
    sock = socket.create_connection((HOST, PORT), timeout=30.0)
    sock.settimeout(timeout)
    sock.sendall(head + body)
    chunks = []
    while True:
        try:
            data = sock.recv(65536)
        except socket.timeout:
            break
        if not data:
            break
        chunks.append(data)
    sock.close()
    raw = b"".join(chunks).decode("utf-8", "replace")
    status = raw.split("\r\n", 1)[0]
    payload = raw.split("\r\n\r\n", 1)[1] if "\r\n\r\n" in raw else ""
    # server-sent events: the reply is the last event; its JSON is spread over many "data:" lines
    last = payload.split("event: message")[-1]
    lines = [line[5:] for line in last.splitlines() if line.startswith("data:")]
    text = " ".join(lines) if lines else payload[payload.find("{"):payload.rfind("}") + 1]
    try:
        reply = json.loads(text)
    except ValueError:
        return status, payload[:2000]
    result = reply.get("result", reply)
    out = []
    for item in result.get("content", []) if isinstance(result, dict) else []:
        if item.get("type") == "text":
            try:
                inner = json.loads(item["text"])
                out.append(inner.get("output_text") or json.dumps(inner)[:3000])
                if not inner.get("ok", True):
                    out.append("FAILED: " + str(inner.get("result"))[:3000])
            except ValueError:
                out.append(item["text"][:3000])
    return status, "\n".join(out) if out else json.dumps(reply)[:3000]


if __name__ == "__main__":
    args = sys.argv[1:]
    if args[0] == "-c":
        source, rest = args[1], args[2:]
    else:
        with open(args[0], encoding="utf-8") as handle:
            source = handle.read()
        rest = args[1:]
    status, text = call(source, rest[0] if rest else "Berlin map step")
    print(status)
    print(text)
