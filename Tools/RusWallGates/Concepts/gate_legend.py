"""Paint a Soviet-gate legend over the (Breakthrough) colour-key cell of overview.png.

python gate_legend.py <out_dir>
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROWS = [
    ((143, 105, 94), 'Wall concrete (SM_Wall_14 look)'),
    ((107, 77, 69), 'Pillars, towers, jambs (darker concrete)'),
    ((61, 79, 56), 'Soviet green steel: leaves, guides, beams'),
    ((70, 74, 79), 'Bare steel: rails, hinges, chains, hedgehogs'),
    ((184, 18, 13), 'Flag / star red'),
    ((247, 247, 247), 'White dashes and arrows: open / close travel'),
]
NOTES = [
    'Wall: 22.6 m x 5 m x 0.2 m slab (Rus_Wall_Concrete_Long).',
    'Clearance target: E-100 = 4.72 m wide, 3.90 m tall.',
    'Greybox only: pick one, then it gets refined.',
]


def main():
    out_dir = sys.argv[1]
    path = os.path.join(out_dir, 'overview.png')
    img = Image.open(path).convert('RGB')
    d = ImageDraw.Draw(img)
    cw, ch = 800, 550
    x, y = 20 + 2 * (cw + 20), 90 + (ch + 50)  # sixth cell (row 2, column 3)
    d.rectangle([x, y + 40, x + cw, y + 40 + ch], fill=(250, 250, 250), outline=(200, 200, 200))
    bold = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 28)
    reg = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 24)
    small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 21)
    d.text((x + 24, y + 60), 'Colour key', font=bold, fill=(25, 25, 25))
    for i, (rgb, text) in enumerate(ROWS):
        yy = y + 108 + i * 46
        d.rectangle([x + 28, yy, x + 60, yy + 32], fill=rgb, outline=(30, 30, 30))
        d.text((x + 76, yy - 2), text, font=reg, fill=(30, 30, 30))
    for i, text in enumerate(NOTES):
        d.text((x + 28, y + 40 + ch - 120 + i * 34), text, font=small, fill=(80, 80, 80))
    img.save(path)
    print('legend painted:', path)


main()
