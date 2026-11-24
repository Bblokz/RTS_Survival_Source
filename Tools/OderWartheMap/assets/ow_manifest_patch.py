"""Adds kit-level notes (previews, status, known issues) to Saved/OderWarthe/assets/ow_kit.json without UE.
Rows themselves are written only by ow_kit_import.py from the UE readback. Plain python: python ow_manifest_patch.py
"""
import json
import os
import time

MANIFEST = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/assets/ow_kit.json'
PREV = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/assets/previews/'

d = json.load(open(MANIFEST))
d['previews'] = sorted(PREV + f for f in os.listdir(PREV) if f.endswith('.png'))
d['status'] = 'all requested assets imported and verified headless (bounds, triangles, slots, UCX count, Nanite off)'
d['known_issues'] = [
    'UE look is unverified (headless -nullrhi import): judge the materials in the editor/game; previews are Blender '
    'Cycles renders with the same textures and tints.',
    'SM_OW_DragonTooth_A/_B are the first (pilot) import: same geometry as the current FBX, but their LightMapUV '
    'channel may be junk (only matters for baked static lighting). A later re-import failed because another editor '
    'had them loaded (Windows file lock); re-run ow_kit_import.py with meshes [teeth] when no editor has them open.',
    'Tunnel portal interior stays at z 0 (no real descending ramp) so the flat landscape cannot poke through; the '
    'descent is faked by a ceiling dip and a black void within ~4 m of the mouth.',
    'Footprints with earth: SM_OW_WerkBlock ~21.5 x 21.6 m, SM_OW_TunnelPortal 22 x 22 m (berm slopes), cupola mound '
    'r 5.2 m; all earth skirts dip 20-40 cm below z 0 to blend into the landscape.',
    'Blender +Y = UE -Y: asymmetric details (ajar door leaf, Loop station mast) are mirrored relative to the Blender '
    'scripts; the bounds in this file are UE space.',
]
d['updated'] = time.strftime('%Y-%m-%d %H:%M:%S')
tmp = MANIFEST + '.tmp'
with open(tmp, 'w') as f:
    json.dump(d, f, indent=1)
os.replace(tmp, MANIFEST)
print('patched', len(d['assets']), 'rows,', len(d['previews']), 'previews')
