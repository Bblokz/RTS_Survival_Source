"""Puts several captured systems side by side: a row per system, a column per moment, one camera.
Usage: python compare_sheet.py <capture folder> <view> <moments comma list> <out.png> [cell width] [system ...]"""
import os
import sys
from PIL import Image, ImageDraw

root, view, moments, out = sys.argv[1], sys.argv[2], sys.argv[3].split(','), sys.argv[4]
cell_width = int(sys.argv[5]) if len(sys.argv) > 5 else 400
systems = sys.argv[6:] or sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d)))
cell_height = cell_width * 9 // 16
label = 16
sheet = Image.new('RGB', (cell_width * len(moments), (cell_height + label) * len(systems)), (20, 20, 20))
draw = ImageDraw.Draw(sheet)
for row, system in enumerate(systems):
    for column, moment in enumerate(moments):
        x, y = column * cell_width, row * (cell_height + label)
        draw.text((x + 4, y + 2), f'{system} {view} t{moment}', fill=(230, 230, 230))
        path = os.path.join(root, system, f'{system}_{view}_t{moment}.png')
        if os.path.exists(path):
            sheet.paste(Image.open(path).convert('RGB').resize((cell_width, cell_height)), (x, y + label))
sheet.save(out)
print(out)
