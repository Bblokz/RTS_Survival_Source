"""File-driven command loop for the Ger_Berlin floating-gap sculpt (throwaway full editor, no UECP endpoint).

Same loop as berlin_editor_server.py, but with its OWN command folder (Saved/Berlin/gaps/cmd) so it never
picks up another session's requests. Launch from PowerShell (Git Bash rewrites /Game/... arguments):

    UnrealEditor-Win64-DebugGame.exe <uproject> /Game/RTS_Survival/Maps/GerMissions/Ger_Berlin
        -ini:EditorPerProjectUserSettings:[/Script/UnrealEd.EditorLoadingSavingSettings]:bAutoSaveEnable=False
        -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/skirt/gaps_editor_server.py"
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -abslog=<log>

gaps_cmd.py is the client. A request can set `_defer = callable`: the loop calls it every tick until it returns True.
"""
import contextlib
import glob
import io
import os
import traceback

import unreal

CMD_DIR = "D:/UE5Projects/RTS_Survival/Saved/Berlin/gaps/cmd"
os.makedirs(CMD_DIR, exist_ok=True)
for stale in glob.glob(os.path.join(CMD_DIR, "req_*.py")) + glob.glob(os.path.join(CMD_DIR, "res_*.txt")):
    os.remove(stale)
STATE = {"ns": {"unreal": unreal, "__name__": "gaps_cmd"}, "pending": None, "busy": False}


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
unreal.log_warning("SKIRT_SERVER ready, watching " + CMD_DIR)
with open(os.path.join(CMD_DIR, "server_ready.txt"), "w") as ready:
    ready.write(str(os.getpid()))
