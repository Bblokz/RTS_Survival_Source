"""Creates the layer infos of the three Ore Mountains mineral layers (see ls_ore_layers_build.py). Headless only:

    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>"
"""
import sys

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
import ls_ore_layers_build

ls_ore_layers_build.layer_infos()
