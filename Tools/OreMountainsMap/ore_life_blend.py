"""Ore life pass, landscape blending (offline, numpy): soften the hardest weightmap edges without new layers.

    python ore_life_blend.py      reads  Saved/OreMountains/life/weights/<Layer>.raw   (exported from the live map by
                                         ore_life_export_weights.py: uint8, row = Y, 757 x 757)
                                  writes Saved/OreMountains/life/weights_blend/<Layer>.raw + blend_report.json/png

Where any layer's weight changes faster than EDGE_PER_M per metre (a full switch in about 2-3 m, which reads as a hard
cut-out line from the RTS camera), the weights are blended toward a Gaussian blur (sigma SIGMA_M) with a feathered
mask, then renormalised so every vertex keeps its original total. Roads, paving, the asphalt layer itself and the
ditch lips keep their crisp edges. Zero runtime cost: only the weightmap texels change.
"""
import json
import os

import numpy as np

LIFE = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/life/"
MASKS = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/Ore_masks.npz"
EDGE_PER_M = 0.30
SIGMA_M = 1.8
KEEP_CRISP = ("road", "pavement", "ditch_lip", "viaduct_deck", "pads")
SKIP_LAYERS = ("Asphalt",)


def gaussian(a, sigma):
    r = int(3 * sigma + 1)
    k = np.exp(-0.5 * (np.arange(-r, r + 1) / sigma) ** 2)
    k /= k.sum()
    out = np.apply_along_axis(lambda v: np.convolve(np.pad(v, r, mode="edge"), k, "valid"), 0, a)
    return np.apply_along_axis(lambda v: np.convolve(np.pad(v, r, mode="edge"), k, "valid"), 1, out)


def main():
    region = json.load(open(LIFE + "weights/region.json"))
    names = region["layers"]
    n = region["region"][2] + 1
    W = {nm: np.fromfile(LIFE + "weights/%s.raw" % nm, dtype=np.uint8).reshape(n, n).astype(float) / 255.0
         for nm in names}
    total = sum(W.values())
    hard = np.zeros((n, n))
    for nm in names:
        if nm in SKIP_LAYERS:
            continue
        gy, gx = np.gradient(W[nm])
        hard = np.maximum(hard, np.hypot(gx, gy))
    edge = (hard > EDGE_PER_M).astype(float)
    feather = np.clip(gaussian(edge, 2.0) * 3.0, 0.0, 1.0)   # widen + soften the mask (~3 m)
    masks = np.load(MASKS)
    crisp = np.zeros((n, n))
    for key in KEEP_CRISP:
        if key in masks.files:
            crisp = np.maximum(crisp, masks[key].astype(float))
    crisp = np.clip(gaussian(crisp, 1.0) * 2.0, 0.0, 1.0)
    crisp = np.maximum(crisp, np.clip(W.get("Asphalt", 0) * 4.0, 0.0, 1.0))
    blend = feather * (1.0 - crisp)
    out = {}
    for nm in names:
        out[nm] = W[nm] * (1.0 - blend) + gaussian(W[nm], SIGMA_M) * blend
    new_total = sum(out.values())
    scale = np.where(new_total > 1e-6, total / np.maximum(new_total, 1e-6), 0.0)
    os.makedirs(LIFE + "weights_blend", exist_ok=True)
    report = {"edge_texels": int(edge.sum()), "blended_texels": int((blend > 0.05).sum()),
              "sigma_m": SIGMA_M, "edge_per_m": EDGE_PER_M, "layers": {}}
    for nm in names:
        v = np.clip(np.round(out[nm] * scale * 255.0), 0, 255).astype(np.uint8)
        v.tofile(LIFE + "weights_blend/%s.raw" % nm)
        diff = np.abs(v.astype(int) - np.round(W[nm] * 255).astype(int))
        report["layers"][nm] = {"changed_texels": int((diff > 2).sum()), "max_change": int(diff.max())}
        gy, gx = np.gradient(v.astype(float) / 255.0)
        report["layers"][nm]["hard_after"] = int((np.hypot(gx, gy) > EDGE_PER_M).sum())
    json.dump(report, open(LIFE + "weights_blend/blend_report.json", "w"), indent=1)
    try:
        from PIL import Image
        img = (np.clip(blend, 0, 1) * 255).astype(np.uint8)
        Image.fromarray(img.T[::-1]).save(LIFE + "weights_blend/blend_mask.png")
    except ImportError:
        pass
    print(json.dumps(report, indent=1))


if __name__ == "__main__":
    main()
