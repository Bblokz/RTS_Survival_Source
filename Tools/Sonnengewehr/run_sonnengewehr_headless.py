"""One headless run for the Sonnengewehr: builds the materials, then authors the data asset template.
Hold Saved/AgentLocks/debuggame_build.lockdir (the run locks the DebugGame DLL) and da_globalabilities.lockdir.
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended
  -nullrhi -NoLiveCoding
Pass SONNENGEWEHR_SKIP_DA=1 in the environment to build only the materials.
"""
import os
import sys

import unreal

HERE = r"D:/UE5Projects/RTS_Survival/Source/Tools/Sonnengewehr"
if HERE not in sys.path:
    sys.path.append(HERE)

import build_sonnengewehr_assets  # noqa: E402
import author_sonnengewehr_template  # noqa: E402

PREFIX = "[Sonnengewehr] "
materials_ok = build_sonnengewehr_assets.run()
unreal.log(PREFIX + "materials_ok=%s" % materials_ok)
# A material another process holds open fails to save but keeps its last version on disk; the template script
# checks that every material exists, which is all it needs.
if os.environ.get("SONNENGEWEHR_SKIP_DA") != "1":
    for line in author_sonnengewehr_template.run():
        unreal.log(PREFIX + line)
