"""Wire the Ore stages into the open Ger_2ndOreMountains over UECP (our editor) and save the level.
Usage: python run_wire_ore_stages.py [--dry]"""
import os
import sys

import ue

here = os.path.dirname(os.path.abspath(__file__))
code = open(os.path.join(here, 'wire_ore_stages.py'), encoding='utf-8').read()
dry = '--dry' in sys.argv
code += '''
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
print('MAP', world.get_name())
if world.get_name() == 'Ger_2ndOreMountains':
    if not %s:
        print('CHANGES', wire(world))
        print('SAVED', unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level())
    for line in describe(world):
        print(line)
''' % dry
result = ue.py(code)
print(result.get('output_text') or result)
