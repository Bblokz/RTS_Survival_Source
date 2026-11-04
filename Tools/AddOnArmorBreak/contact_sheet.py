"""Lays out the capture pictures of an add-on armour break run as sheets, one per hit, with a row per camera and a
column per moment. Usage: python contact_sheet.py <capture folder> [cell width]
Pictures are named <Camera>_<Hit>_t<+seconds>.png (and <Camera>_intact.png / <Camera>_final.png)."""
import os
import re
import sys

from PIL import Image, ImageDraw

PICTURE = re.compile(r'^(?P<camera>[A-Za-z]+)_(?P<label>.+)\.png$')


def main():
    folder = sys.argv[1]
    cell_width = int(sys.argv[2]) if len(sys.argv) > 2 else 400
    cell_height = cell_width * 9 // 16
    label_height = 18
    by_hit = {}
    for name in os.listdir(folder):
        match = PICTURE.match(name)
        if not match:
            continue
        label = match.group('label')
        hit, _, moment = label.partition('_t')
        moment = moment or hit
        by_hit.setdefault(hit if '_t' in label else 'overview', {}).setdefault(match.group('camera'), {})[moment] = name
    for hit, cameras in sorted(by_hit.items()):
        camera_names = sorted(cameras)
        moments = sorted({m for c in cameras.values() for m in c}, key=lambda m: float(m) if re.match(r'^[+-]?[0-9.]+$', m) else -99)
        sheet = Image.new('RGB', (cell_width * len(moments), (cell_height + label_height) * len(camera_names)), (18, 18, 18))
        draw = ImageDraw.Draw(sheet)
        for row, camera in enumerate(camera_names):
            for column, moment in enumerate(moments):
                x = column * cell_width
                y = row * (cell_height + label_height)
                draw.text((x + 4, y + 3), f'{camera}  {hit} {moment}', fill=(235, 235, 235))
                name = cameras[camera].get(moment)
                if name:
                    picture = Image.open(os.path.join(folder, name)).convert('RGB').resize((cell_width, cell_height))
                    sheet.paste(picture, (x, y + label_height))
        out = os.path.join(folder, f'sheet_{hit}.png')
        sheet.save(out)
        print(out)


main()
