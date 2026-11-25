"""One headless run: import the radio outpost meshes and materials, then build the hut Blueprint.
  sh run_ue_py.sh ue_radio_build.py build -nullrhi
"""
import sys

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/OreRadioOutpost'
for name in ('ue_radio_import.py', 'ue_radio_bp.py'):
    with open(HERE + '/' + name) as f:
        exec(compile(f.read(), name, 'exec'), {'__name__': '__main__'})
