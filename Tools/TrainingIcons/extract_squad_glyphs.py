"""Lifts the blue squad-role glyphs off the live squad training icons (GerSquads/<iconed> vs GerSquads/NoIcon/<plain>)
with the same least-opaque matte as UnitPortraits/engrave_icons.py, so new icons carry pixel-identical glyphs.

Usage: python extract_squad_glyphs.py   -> Saved/TrainingIcons/glyphs/<name>.png (256x256 RGBA, glyph in place)
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "UnitPortraits"))
from engrave_icons import load_rgb, matte  # noqa: E402

SQUADS = Path("D:/UE_Assets/Icons/TrainingUI/GerSquads")
OUT = Path("D:/UE5Projects/RTS_Survival/Saved/TrainingIcons/glyphs")
# glyph name: (iconed picture, plain picture)
SOURCES = {
    "squad_rocket": ("T_Ger_Portrait_PanzerGrenadier.png", "NoIcon/T_Ger_Portrait_PanzerGrenadier.png"),
    "squad_wrench": ("T_Ger_Portrait_Scavengers.PNG", "NoIcon/T_Ger_Portrait_Scavengers.PNG"),
    "squad_skull": ("T_Ger_Portrait_SturmKommando.PNG", "NoIcon/T_Ger_Portrait_SturmKommando.PNG"),
    "squad_knife": ("T_Ger_Portrait_Vulture.PNG", "NoIcon/T_Ger_Portrait_Vulture_NoIcon.PNG"),
    "squad_bullets": ("Thumb_Portrait_ResistanceInfantry_Portrait.PNG", "NoIcon/Thumb_Portrait_ResistanceInfantry_Portrait.PNG"),
    "squad_star": ("T_Ger_Portrait_LightBringer.PNG", "NoIcon/T_Ger_Portrait_LightBringer.PNG"),
    "squad_flame": ("T_Ger_Portrait_FlameTrooper.PNG", "NoIcon/T_Ger_Portrait_FlameTrooper.PNG"),
    "squad_circle": ("T_Ger_Portrait_FemaleSniper.PNG", "NoIcon/T_Ger_Portrait_FemaleSniper.PNG"),
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (iconed_name, plain_name) in SOURCES.items():
        iconed_path, plain_path = SQUADS / iconed_name, SQUADS / plain_name
        if not iconed_path.exists() or not plain_path.exists():
            print("SKIP", name, iconed_path.exists(), plain_path.exists())
            continue
        iconed, plain = load_rgb(iconed_path), load_rgb(plain_path)
        region = np.ones(iconed.shape[:2], dtype=bool)
        rgba = matte(iconed, plain, region)
        alpha = rgba[..., 3]
        ys, xs = np.nonzero(alpha > 8)
        print(name, "pixels", len(xs), "box", (xs.min(), ys.min(), xs.max(), ys.max()) if len(xs) else None)
        Image.fromarray(rgba, "RGBA").save(OUT / f"{name}.png")


if __name__ == "__main__":
    main()
