#!/usr/bin/env bash
# One look-dev round for the howitzer impacts: build from the recipe, capture at SCALE, lay out comparison sheets.
# Usage: iterate.sh <round name> [extra capture switches...]   e.g. SCALE=2.6 iterate.sh v2
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
ROUND="$1"; shift || true
H=/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Howitzer
OUT="D:/UE5Projects/RTS_Survival/Saved/ImpactVfx/$ROUND"
"$HERE/build_systems.sh" | grep "built \|RESULT\|shared\|Error"
rm -rf "$OUT"
"$HERE/run_capture.sh" "$H/NS_HowitzerImpact_Regular+$H/NS_HowitzerImpact_Sand+$H/NS_HowitzerImpact_Stone" "$OUT" \
  -ImpactVfxMoments=0.1,0.3,0.6,1,1.6,2.5,4,6 "$@" | grep "max particles\|could not"
for VIEW in Game Side GameFar; do
  python "$HERE/compare_sheet.py" "$OUT" "$VIEW" 00.10,00.30,00.60,01.00,01.60,02.50,04.00,06.00 \
    "D:/UE5Projects/RTS_Survival/Saved/ImpactVfx/sheets/${ROUND}_${VIEW}.png" 240
done
