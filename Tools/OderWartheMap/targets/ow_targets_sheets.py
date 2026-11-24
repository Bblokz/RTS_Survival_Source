"""Compose the concept sheets of the Werk targets from the Blender renders (system Python + PIL).

    python ow_targets_sheets.py
Reads <OUT>/renders/<KEY>_<state>.png, writes <OUT>/Targets_<Group>.png. Texts come from ow_targets_info.py.
"""
import os
import textwrap

from PIL import Image, ImageDraw, ImageFont

from ow_targets_info import GROUPS

OUT = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Targets/Concepts/'
IMG_W, IMG_H = 620, 420
GAP = 24
CELL_W = IMG_W * 2 + 12
TEXT_H = 120
HEAD_H = 46


def font(size, bold=False):
    return ImageFont.truetype('arialbd.ttf' if bold else 'arial.ttf', size)


def cell(draw, sheet, x, y, letter, key, name, desc):
    draw.text((x, y), 'Option %s  -  %s' % (letter, name), fill=(20, 20, 20), font=font(30, True))
    y += HEAD_H
    for k, state in enumerate(('intact', 'destroyed')):
        path = OUT + 'renders/%s_%s.png' % (key, state)
        px = x + k * (IMG_W + 12)
        if os.path.exists(path):
            sheet.paste(Image.open(path).convert('RGB'), (px, y))
            draw.rectangle((px, y, px + 150, y + 30), fill=(255, 255, 255))
            draw.text((px + 8, y + 3), state.capitalize(), fill=(20, 20, 20), font=font(22, True))
        else:
            draw.rectangle((px, y, px + IMG_W, y + IMG_H), outline=(150, 150, 150), width=2)
            if k == 0:
                draw.text((px + 30, y + IMG_H / 2 - 20), 'No extra mesh', fill=(90, 90, 90), font=font(34, True))
    y += IMG_H + 8
    for i, line in enumerate(textwrap.wrap(desc, 118)):
        draw.text((x, y + i * 26), line, fill=(30, 30, 30), font=font(21))


def sheet(group):
    g = GROUPS[group]
    rows = (len(g['options']) + 1) // 2
    intro = textwrap.wrap(g['intro'], 210)
    top = 90 + 30 * len(intro) + 20
    W = 2 * CELL_W + 3 * GAP
    H = top + rows * (HEAD_H + IMG_H + TEXT_H + GAP) + GAP
    img = Image.new('RGB', (W, H), (246, 244, 238))
    d = ImageDraw.Draw(img)
    d.text((GAP, 24), g['title'], fill=(10, 10, 10), font=font(44, True))
    for i, line in enumerate(intro):
        d.text((GAP, 90 + 30 * i), line, fill=(40, 40, 40), font=font(24))
    for n, (key, (name, desc)) in enumerate(g['options'].items()):
        cx = GAP + (n % 2) * (CELL_W + GAP)
        cy = top + (n // 2) * (HEAD_H + IMG_H + TEXT_H + GAP)
        cell(d, img, cx, cy, key[-1], key, name, desc)
    path = OUT + 'Targets_%s.png' % group.capitalize()
    img.save(path)
    print(path, img.size)


if __name__ == '__main__':
    for grp in GROUPS:
        sheet(grp)
