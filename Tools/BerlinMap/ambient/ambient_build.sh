#!/bin/sh
# Rebuilds every asset of the Berlin ambient motion and the placement file. Safe to rerun; nothing opens Ger_Berlin.
#   ambient_build.sh            textures, meshes, Unreal assets, placement file
#   ambient_build.sh probe      the same, and first re-reads the ruin kit FBX files for windows / perches / spans
#                               (only needed when the kit meshes changed)
# Needs: no UnrealBuildTool running; the open editor must not have the Ambient assets loaded (a save would fail).
HERE=D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ambient
MODEL=D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinAmbient
BLENDER="D:/Blender/BlenderSoftware/5/Blender5.0.exe"
python $HERE/ambient_textures_gen.py || exit 1
"$BLENDER" -b --factory-startup --python $MODEL/generate_berlin_ambient.py 2>&1 | grep -E "AMBIENT_MESHES|Error|Traceback" | cut -c1-200
if [ "$1" = "probe" ]; then
  "$BLENDER" -b --factory-startup --python $MODEL/kit_attach_probe.py 2>&1 | grep -E "ATTACH|Error|Traceback"
fi
sh $HERE/run_headless.sh $HERE/ambient_dressing_build.py dressing | grep -E "exit=|Error|NOT|DONE"
sh $HERE/run_headless.sh $HERE/ambient_litter_build.py litter | grep -E "exit=|Error|NOT|RESULT|DONE"
python $HERE/ambient_gen.py
