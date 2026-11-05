"""Makes T_EliteScavenger_Diffuse.png: the scavenger base colour with the coat and trouser panels (white in
T_Inf_PosApo_Mask) dyed a dark oxblood red and the rest a little darker, so Elite Scavengers read apart from the
regular scavengers at game distance. Plain Python (PIL + numpy), run before build_elite_scavengers.py.

  python make_elite_scavenger_texture.py
"""
import os

import numpy as np
from PIL import Image

SRC = "D:/Blender/BlenderProjects/BlenderTanks/Infantry/PostApoSoldier/post-apocalypse-character/textures"
OUT_DIR = "D:/Blender/BlenderProjects/BlenderTanks/Infantry/PostApoSoldier/EliteScavenger"
OUT = OUT_DIR + "/T_EliteScavenger_Diffuse.png"
DYE = np.array([0.40, 0.17, 0.13])   # dark oxblood, multiplied with the cloth's own luminance
DYE_GAIN = 1.45                      # a little darker than the original cloth
DYE_STRENGTH = 0.75
REST_DARKEN = 0.82


def main():
    base = np.asarray(Image.open(SRC + "/initialShadingGroup_Base_color.jpg").convert("RGB"), dtype=np.float32) / 255.0
    mask = np.asarray(Image.open(SRC + "/T_Inf_PosApo_Mask.png").convert("L").resize(
        (base.shape[1], base.shape[0]), Image.BILINEAR), dtype=np.float32) / 255.0
    lum = base @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    dyed = np.clip(lum[..., None] * DYE[None, None, :] * DYE_GAIN, 0.0, 1.0)
    cloth = base * (1.0 - DYE_STRENGTH) + dyed * DYE_STRENGTH
    rest = base * REST_DARKEN
    m = mask[..., None]
    out = cloth * m + rest * (1.0 - m)
    os.makedirs(OUT_DIR, exist_ok=True)
    Image.fromarray((np.clip(out, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)).save(OUT)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
