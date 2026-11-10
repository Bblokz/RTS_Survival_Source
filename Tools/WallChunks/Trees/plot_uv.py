import json, sys
from PIL import Image, ImageDraw
tris = json.load(open('Source/tree1_uv.json'))
img = Image.open('Source/T_RTS_DeadTree_1_D.png').convert('RGB')
W, H = img.size
over = img.copy()
d = ImageDraw.Draw(over)
import math
for t in tris:
    zs = [p[2] for p in t['p']]
    rs = [math.hypot(p[0], p[1]) for p in t['p']]
    col = (255, 0, 0) if max(zs) < 3.0 and max(rs) < 1.2 else (0, 255, 255)
    pts = [((u % 1.0) * W if 0 <= u <= 1 else u * W, (1 - v) * H) for u, v in t['uv']]
    d.polygon(pts, outline=col)
over.save('Source/tree1_uv_overlay.png')
us = [u for t in tris for u, v in t['uv']]; vs = [v for t in tris for u, v in t['uv']]
print('u range', min(us), max(us), 'v range', min(vs), max(vs))
