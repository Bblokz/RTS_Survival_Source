"""One headless DebugGame-Cmd pass: builds the sFH 18 gun + squad + spawner row, then re-authors GA_GerSFH18Drop.

  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file>
Hold the debuggame_build, async_spawner and da_globalabilities lock dirs (Saved/AgentLocks) while it runs.
Set the environment variable SFH18_ONLY to "squad" or "drop" to run one half, and SFH18_STEPS (comma list of
gun,squad,spawner) to limit the squad half.
"""
import os

HERE = "D:/UE5Projects/RTS_Survival/Source/Tools/SFH18Squad"
ONLY = os.environ.get("SFH18_ONLY", "")


def run_file(name, extra=None):
    scope = {"__name__": "__main__", "__file__": HERE + "/" + name}
    scope.update(extra or {})
    with open(HERE + "/" + name, encoding="utf-8") as handle:
        exec(compile(handle.read(), HERE + "/" + name, "exec"), scope)


STEPS = tuple(s for s in os.environ.get("SFH18_STEPS", "gun,squad,spawner").split(",") if s)
if ONLY in ("", "squad"):
    run_file("build_sfh18_squad.py", {"SFH18_STEPS": STEPS})
if ONLY in ("", "drop"):
    run_file("author_sfh18_drop.py")
