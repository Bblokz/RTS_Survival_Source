"""Launch ue_import_berlin_lines.py in a commandlet, reusing new_voice_lines.py's launcher (DLL choice, dialog closer)."""
import json, os, sys
TOOLS = os.path.dirname(os.path.abspath(__file__))
VC = os.path.normpath(os.path.join(TOOLS, "..", "..", "..", "..", "VoiceLines", "VoiceChanger"))
sys.path.insert(0, VC)
import new_voice_lines as nvl
nvl.IMPORT_SCRIPT = os.path.join(TOOLS, "ue_import_berlin_lines.py").replace("\\", "/")
nvl.REPORT = os.path.join(TOOLS, "ue_import_berlin_lines_report.json")
nvl.LOG = os.path.join(TOOLS, "ue_import_berlin_lines.log")
print(json.dumps(nvl.run_unreal_import(), indent=1))
