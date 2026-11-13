#!/usr/bin/env bash
# One capture pass for the landscape critic rounds: ore_ls_round.sh <tag> [paint|full]
# Needs the throwaway editor with ore_editor_server.py running on Ger_2ndOreMountains.
set -u
cd /d/UE5Projects/RTS_Survival
TAG="$1"
REBUILD="${2:-}"
{ echo "TAG = '$TAG'"; echo "REBUILD = '$REBUILD'"; cat Source/Tools/OreMountainsMap/ore_ls_shots.py; } > Saved/OreMountains/_ls_run.py
python Source/Tools/OreMountainsMap/ore_cmd.py Saved/OreMountains/_ls_run.py --timeout 3000 | grep -v "^ok " | cut -c1-240
python - "$TAG" <<'EOF'
import glob, sys, os
from PIL import Image, ImageDraw, ImageEnhance
tag = sys.argv[1]
d = "Saved/OreMountains/critic/%s/" % tag
files = sorted(f for f in glob.glob(d + "[0-9][0-9]_*.png"))
w, h, cols = 800, 450, 4
for part in range(0, len(files), 8):
    sub = files[part:part + 8]
    rows = (len(sub) + cols - 1) // cols
    im = Image.new("RGB", (cols * w, rows * h))
    dr = ImageDraw.Draw(im)
    for i, f in enumerate(sub):
        im.paste(Image.open(f).convert("RGB").resize((w, h)), ((i % cols) * w, (i // cols) * h))
        dr.text(((i % cols) * w + 6, (i // cols) * h + 4), os.path.basename(f)[:-4], fill=(255, 255, 0))
    im.save(d + "sheet_%d.jpg" % (part // 8 + 1), quality=88)
if os.path.exists(d + "top.png"):
    ImageEnhance.Brightness(Image.open(d + "top.png").convert("RGB")).enhance(1.9).resize((1600, 1600)).save(d + "top_b.jpg", quality=88)
print("sheets", len(files), "views in", d)
EOF
