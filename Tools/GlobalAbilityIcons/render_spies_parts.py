"""Renders the Spies Infiltration icon parts in a freshly started editor and closes it. Start the editor with:
    UnrealEditor.exe RTS_Survival.uproject /Engine/Maps/Templates/Template_Default -ExecutePythonScript=<this file>
A fresh editor needs its shaders and textures first, so the renders wait for a while after startup and run twice
(the first capture after an asset loads can catch unstreamed textures). Writes renders/spies_done.txt at the end.
"""
import time

exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/render_ability_parts.py").read())

SPIES_JOBS = ["Spy_Crouch", "Spy_Spot", "T3485_Ground", "T3476_Ground"]
# Seconds after startup for the first and the second pass, and when to close the editor.
SCHEDULE = [(60.0, "first"), (120.0, "second"), (130.0, "quit")]
_state = {"start": time.time(), "step": 0, "handle": None}


def _tick(delta_seconds):
    if _state["step"] >= len(SCHEDULE):
        return
    at, what = SCHEDULE[_state["step"]]
    if time.time() - _state["start"] < at:
        return
    _state["step"] += 1
    try:
        if what == "quit":
            with open(RENDER_DIR + "/spies_done.txt", "w") as done:
                done.write("ok")
            unreal.unregister_slate_post_tick_callback(_state["handle"])
            unreal.SystemLibrary.quit_editor()
            return
        unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
        run_jobs(SPIES_JOBS)
        unreal.log("[SpiesRender] %s pass done" % what)
    except Exception as error:
        unreal.log_error("[SpiesRender] %s failed: %s" % (what, error))


_state["handle"] = unreal.register_slate_post_tick_callback(_tick)
