"""File-driven command loop for a throwaway full editor used to build the rail yard D1 (BerlinRailYard.md) (copy of
berlin_editor_server.py with its own command folder Saved/BerlinRailYard/cmd, so it never meets the Berlin map tools).
Launch (the editor stays open; -ExecutePythonScript would quit it before any tick):
    UnrealEditor.exe <uproject> -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/forge_editor_server.py"
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>

Every tick it looks for Saved/BerlinRailYard/cmd/req_<id>.py, runs it in one persistent namespace and writes
res_<id>.txt (stdout + traceback, last line "OK" or "FAIL"). berlin_cmd.py is the client.
A request can set `_defer = callable` in the namespace: the loop then calls it every tick until it returns True
before it writes the result (for waits on navmesh or shader compiles).
"""
import contextlib
import glob
import io
import os
import traceback

import unreal

CMD_DIR = "D:/UE5Projects/RTS_Survival/Saved/BerlinRailYard/cmd"
os.makedirs(CMD_DIR, exist_ok=True)
STATE = {"ns": {"unreal": unreal, "__name__": "railyard_cmd"}, "pending": None, "busy": False}


def _finish(req_id, text, ok):
    with open(os.path.join(CMD_DIR, "res_%s.tmp" % req_id), "w", encoding="utf-8") as handle:
        handle.write(text + ("\nOK" if ok else "\nFAIL"))
    os.replace(os.path.join(CMD_DIR, "res_%s.tmp" % req_id), os.path.join(CMD_DIR, "res_%s.txt" % req_id))


def _tick(delta):
    if STATE["busy"]:
        return
    STATE["busy"] = True
    try:
        pending = STATE["pending"]
        if pending is not None:
            req_id, buffer, defer = pending
            try:
                with contextlib.redirect_stdout(buffer):
                    done = defer()
            except Exception:
                buffer.write(traceback.format_exc())
                STATE["pending"] = None
                _finish(req_id, buffer.getvalue(), False)
                return
            if done:
                STATE["pending"] = None
                _finish(req_id, buffer.getvalue(), True)
            return
        requests = sorted(glob.glob(os.path.join(CMD_DIR, "req_*.py")))
        if not requests:
            return
        path = requests[0]
        req_id = os.path.basename(path)[4:-3]
        with open(path, "r", encoding="utf-8") as handle:
            source = handle.read()
        os.remove(path)
        buffer = io.StringIO()
        namespace = STATE["ns"]
        namespace.pop("_defer", None)
        ok = True
        try:
            with contextlib.redirect_stdout(buffer):
                exec(compile(source, path, "exec"), namespace)
        except Exception:
            buffer.write(traceback.format_exc())
            ok = False
        defer = namespace.pop("_defer", None)
        if ok and callable(defer):
            STATE["pending"] = (req_id, buffer, defer)
            return
        _finish(req_id, buffer.getvalue(), ok)
    finally:
        STATE["busy"] = False


HANDLE = unreal.register_slate_post_tick_callback(_tick)
unreal.log_warning("RAILYARD_SERVER ready, watching " + CMD_DIR)
with open(os.path.join(CMD_DIR, "server_ready.txt"), "w") as ready:
    ready.write(str(os.getpid()))
