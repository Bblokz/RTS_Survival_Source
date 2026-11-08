#!/usr/bin/env bash
# Perf matrix for the add-on armour break comparison: every technique at each scale, REPEATS times, techniques
# interleaved so drift in the machine's load hits all of them alike. Waits for other headless Unreal runs first
# (they share the GPU and CPU; any UnrealEditor*-Cmd binary, DebugGame included). A run during which another one
# started is marked with a CONCURRENT line in the matrix file; its numbers include that load. Summary lines go to
# Saved/AddOnArmorBreak/matrix_<stamp>.txt; analyse with matrix_report.py.
# Usage: run_matrix.sh   (PROJECT=<uproject> for an isolated copy; REPEATS=2; SCALES="25:10 100:30";
#   TECHNIQUE_LIST="None Niagara BakedMask"; VIEW_TAG=Close prefixes the run tags; arguments go on to run_test.sh,
#   e.g. -AddOnArmorBreakViewDistance=1800 -AddOnArmorBreakProfileGpu)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPEATS="${REPEATS:-2}"
SCALES="${SCALES:-25:10 100:30}"
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="D:/UE5Projects/RTS_Survival/Saved/AddOnArmorBreak/matrix_${STAMP}.txt"
read -r -a TECHNIQUES <<< "${TECHNIQUE_LIST:-None Niagara GeometryCollection}"
TECHNIQUE_COUNT=${#TECHNIQUES[@]}
wait_for_other_runs() {
  while powershell -NoProfile -Command "if (Get-Process UnrealEditor*-Cmd -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"; do
    echo "waiting for other UnrealEditor*-Cmd runs..."; sleep 30
  done
}
# Lists headless Unreal runs still alive after ours ended: they started while it was measuring.
report_concurrent_runs() {
  powershell -NoProfile -Command "Get-Process UnrealEditor*-Cmd -ErrorAction SilentlyContinue | ForEach-Object { 'CONCURRENT tag=$1 ' + \$_.ProcessName + ' started ' + \$_.StartTime.ToString('HH:mm:ss') }"
}
for REPEAT in $(seq 1 "$REPEATS"); do
  for SCALE in $SCALES; do
    VEHICLES="${SCALE%%:*}"; HITS="${SCALE##*:}"
    for OFFSET in $(seq 0 $(( TECHNIQUE_COUNT - 1 ))); do
      TECHNIQUE="${TECHNIQUES[$(( (OFFSET + REPEAT - 1) % TECHNIQUE_COUNT ))]}"
      TAG="Perf${VIEW_TAG:-}_${TECHNIQUE}_v${VEHICLES}_h${HITS}_r${REPEAT}"
      wait_for_other_runs
      echo "== $TAG"
      TECHNIQUE="$TECHNIQUE" VEHICLES="$VEHICLES" HITS="$HITS" TAG="$TAG" SEED="$REPEAT" \
        "$HERE/run_test.sh" Perf "$@" | grep "RTS_ADDON_ARMOR_PERF" | sed 's/.*RTS_ADDON_ARMOR_PERF //' >> "$OUT"
      report_concurrent_runs "$TAG" | tee -a "$OUT"
    done
  done
done
echo "matrix done: $OUT"
