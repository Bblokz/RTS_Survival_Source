#!/usr/bin/env bash
# Pictures the Panzer III M with each team colour test shader and colour, offscreen (the open editor is untouched).
# Uses the unit portrait capture (Docs/UI/UnitPortraits.md) with only the idle picture, a steep RTS-camera view and
# fixed exposure so every picture is lit the same.
# Usage: run_team_color_capture.sh [list file] [output folder]
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
LIST="${1:-$HERE/pz3m_team_colors.txt}"
OUT="${2:-D:/UE5Projects/RTS_Survival/Saved/TeamColorTest/captures}"
EXPOSURE_BIAS="${EXPOSURE_BIAS:-2.0}" "$HERE/../UnitPortraits/run_capture.sh" "$LIST" "$OUT" -UnitPortraitShots=0 -UnitPortraitRtsView -UnitPortraitFixedExposure
