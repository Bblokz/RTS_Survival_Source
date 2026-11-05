"""One headless DebugGame-Cmd run for the Ju 87 G: author_ju87g_blueprint.py, then author_ju87g_tank_busters.py.
Hold Saved/AgentLocks/debuggame_build.lockdir and da_globalabilities.lockdir; see run_ju87g_authoring.ps1."""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
for script in ("author_ju87g_blueprint.py", "author_ju87g_tank_busters.py"):
    path = os.path.join(HERE, script)
    with open(path, encoding="utf-8") as handle:
        exec(compile(handle.read(), path, "exec"), {"__name__": "__main__", "__file__": path})
