"""Rebuilds the Mammut materials and their textures without reimporting the meshes (DebugGame-Cmd).

python make_textures.py <Export/Textures> first, then
UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
"""
MAMMUT_MATERIALS_ONLY = True
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/GliderDelivery/import_mammut_model.py', encoding='utf8').read())
