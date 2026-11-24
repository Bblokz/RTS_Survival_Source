"""Bakes T_Intro_MouldRelief_ORM.png (R = vessels, G = nodules) over the mats' UV domain (mould_field), so the
engine material's vessel colour and glow sit on the vessels build_mould_stages.py raised out of the geometry.

Row 0 of the image is V = V0 (Unreal flips V on FBX import; M_Intro_PurpleDeath samples ((U - U0) / U_SPAN,
((1 - UV.y) - V0) / V_SPAN)). Run with plain python (numpy + PIL):  python bake_mould_relief.py
"""
import os

import numpy as np
from PIL import Image

import mould_field as MF

OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', '..',
                                    'Saved', 'IntroCinematic', 'import', 'Sets'))
W, H = 4096, 2048


def main():
    u = MF.U0 + MF.U_SPAN * (np.arange(W) + 0.5) / W
    v = MF.V0 + MF.V_SPAN * (np.arange(H) + 0.5) / H
    U, V = np.meshgrid(u, v)
    s, t = U - 0.5, V - 0.5                       # UV = field coordinate + 0.5 (the offsets are in both)
    img = np.zeros((H, W, 3), np.uint8)
    img[..., 0] = np.clip(MF.vessels(s, t) * 255.0 + 0.5, 0, 255).astype(np.uint8)
    img[..., 1] = np.clip(MF.nodules(s, t) * 255.0 + 0.5, 0, 255).astype(np.uint8)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, 'T_Intro_MouldRelief_ORM.png')
    Image.fromarray(img).save(path)
    print('RELIEF', path, 'vessel cover %.1f%%' % (100.0 * (img[..., 0] > 64).mean()))


main()
