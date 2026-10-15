"""Turns the sand dirt-column flipbooks into stone-dust ones: grey (luminance), brighter, alpha kept, half size.
Usage: python make_stone_flipbooks.py <folder with T_Expl_39.png and T_Expl_40.png>
Writes T_HowitzerStoneDust_1.png / _2.png next to them for import_stone_flipbooks.py."""
import os
import sys
from PIL import Image

BRIGHTNESS = 1.55
COOL_TINT = (0.97, 1.0, 1.04)
OUTPUT_SIZE = 2048
folder = sys.argv[1]
for index, source in enumerate(['T_Expl_39.png', 'T_Expl_40.png'], start=1):
    image = Image.open(os.path.join(folder, source)).convert('RGBA')
    red, green, blue, alpha = image.split()
    luminance = Image.merge('RGB', (red, green, blue)).convert('L')
    channels = [luminance.point(lambda value, tint=tint: min(255, int(value * BRIGHTNESS * tint))) for tint in COOL_TINT]
    stone = Image.merge('RGBA', (*channels, alpha)).resize((OUTPUT_SIZE, OUTPUT_SIZE), Image.LANCZOS)
    out = os.path.join(folder, f'T_HowitzerStoneDust_{index}.png')
    stone.save(out)
    print(out)
