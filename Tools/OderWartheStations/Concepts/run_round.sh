#!/usr/bin/env bash
# Render the DF station concept specs (enemy-held, player-held, icon elevations) and compose the sheets.
# usage: run_round.sh <out_dir> "<sheet title>" <spec.py> [<spec.py> ...]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
TOOLS="/d/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/_ConceptTools"
BLENDER="/d/Blender/BlenderSoftware/5/Blender5.0.exe"
OUT="$1"; TITLE="$2"; shift 2
for pass in enemy player icon_enemy icon_player; do
    state="${pass#icon_}"
    icon=0; [[ "$pass" == icon_* ]] && icon=1
    DF_STATE="$state" DF_ICON="$icon" "$BLENDER" -b --factory-startup --python "$TOOLS/render_concepts.py" -- \
        "$OUT/$pass" "$@" 2>&1 | grep -E "CONCEPT_DONE|Error|Traceback|File \"" || true
done
python "$HERE/compose_df.py" "$OUT" "$TITLE"
