"""Lays the impact capture pictures of each system out as one sheet: a row per camera, a column per moment.
Usage: python contact_sheet.py <capture folder> [cell width]
Writes <capture folder>/<system>_sheet.png next to the system folders."""
import os
import sys
from PIL import Image, ImageDraw

root = sys.argv[1]
cell_width = int(sys.argv[2]) if len(sys.argv) > 2 else 320
for system in sorted(os.listdir(root)):
    folder = os.path.join(root, system)
    if not os.path.isdir(folder):
        continue
    pictures = {}
    for name in os.listdir(folder):
        if not name.endswith('.png'):
            continue
        stem = name[len(system) + 1:-4]
        view, moment = stem.split('_', 1)
        pictures.setdefault(view, {})[moment] = os.path.join(folder, name)
    if not pictures:
        continue
    views = sorted(pictures)
    moments = sorted({m for v in pictures.values() for m in v})
    cell_height = cell_width * 9 // 16
    label_height = 16
    sheet = Image.new('RGB', (cell_width * len(moments), (cell_height + label_height) * len(views)), (20, 20, 20))
    draw = ImageDraw.Draw(sheet)
    for row, view in enumerate(views):
        for column, moment in enumerate(moments):
            path = pictures[view].get(moment)
            x = column * cell_width
            y = row * (cell_height + label_height)
            draw.text((x + 4, y + 2), f'{view} {moment}', fill=(230, 230, 230))
            if path:
                sheet.paste(Image.open(path).convert('RGB').resize((cell_width, cell_height)), (x, y + label_height))
    out = os.path.join(root, f'{system}_sheet.png')
    sheet.save(out)
    print(out)
