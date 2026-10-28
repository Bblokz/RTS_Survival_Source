"""Applies the editor-only layout the landscape builders cannot set from Python: comment frame sizes (recorded in each
comment's desc as "size=WxH", see ls_graph.comment()) and the master's output node position.

Both are editor state that only URTSMaterialGraphLibrary (Utils/MaterialGraph) can set. When the editor the builders
ran in lacks that library, run this in an editor or commandlet that has it (DebugGame build), after the shared editor
saved the assets, then reload them in the shared editor:
  UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script="<this folder>/ls_editor_layout.py"
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ls_graph as G
import ls_master_build

ASSETS = [
    "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle",
    G.FUNCTION_DIR + "/MF_RTS_LS_GroundBreakup",
    G.FUNCTION_DIR + "/MF_RTS_LS_TankTrails",
    G.FUNCTION_DIR + "/MF_RTS_LS_FieldFurrows",
    G.FUNCTION_DIR + "/MF_RTS_LS_WetRuts",
    G.FUNCTION_DIR + "/MF_RTS_LS_GrassWeights",
]


def apply(asset_paths=ASSETS):
    results = {}
    for path in asset_paths:
        asset = unreal.load_asset(path)
        sized = [G.set_comment_size(e) for e in unreal.ObjectIterator(unreal.MaterialExpressionComment)
                 if e.get_outer() == asset]
        if isinstance(asset, unreal.MaterialFunction):
            G.mel.update_material_function(asset)
        else:
            sized.append(ls_master_build.place_output(asset))
        results[path.split("/")[-1]] = (sum(sized), len(sized),
                                        unreal.EditorAssetLibrary.save_loaded_asset(asset, False))
    return results


if __name__ == "__main__":
    for name, (sized, total, saved) in apply().items():
        print("EDITORLAYOUT %s laid out %d of %d saved %s" % (name, sized, total, saved))
