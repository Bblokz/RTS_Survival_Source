#!/usr/bin/env bash
# Render supply point concept specs with the Breakthrough concept tools and compose the sheets here.
# usage: run_round.sh <out_dir> "<sheet title>" <spec.py> [<spec.py> ...]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
TOOLS="/d/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/_ConceptTools"
BLENDER="/d/Blender/BlenderSoftware/5/Blender5.0.exe"
OUT="$1"; TITLE="$2"; shift 2
"$BLENDER" -b --factory-startup --python "$TOOLS/render_concepts.py" -- "$OUT" "$@" 2>&1 \
    | grep -E "CONCEPT_DONE|Error|Traceback|File \"" || true
python "$HERE/compose_sheets_sp.py" "$OUT" "$TITLE"
