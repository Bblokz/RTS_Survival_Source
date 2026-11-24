"""Headless wrapper for forge_station_import.py:
    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -NoSound
        -DisablePlugins=Fab,BpGeneratorUltimate
Writes Saved/Berlin/forge_style/import_result.json.
"""
import json
import traceback

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/forge_style/import_result.json'
ns = {}
try:
    exec(open(HERE + 'forge_station_import.py', encoding='utf-8').read(), ns)
    result = ns['run']()
except Exception as exc:  # report, never hang
    result = {'error': repr(exc), 'trace': traceback.format_exc()}
with open(OUT, 'w', encoding='utf-8') as f:
    json.dump(result, f, indent=1)
print('FORGE_STATION_JOB', 'error' in result)
