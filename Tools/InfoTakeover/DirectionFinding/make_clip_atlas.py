"""Packs the tactical-map frames of capture_clip.ps1 into T_DirectionFinding_ClipAtlas for UW_DirectionFindingTakeover.

The map is square and the clip cell is 16:9, so each frame's map square (CROP, in 1920 x 1080 frame pixels) is scaled to
the cell height and centred on the tactical map's own dark background. Every STEP-th recorded frame from FIRST is kept
(24 fps recording / 2 = the clip's 12 fps), FRAMES in all, COLUMNS wide. A small tag names what just happened; the event
frames come from the DF_CLIP log lines (DirectionFindingClipCapture.cpp).
Writes Saved/DirectionFindingClip/T_DirectionFinding_ClipAtlas.png, clip_atlas.json and clip_preview.gif.
The grid must match the constants in W_DirectionFindingTakeover.cpp (8 x 12, 96 frames).

    python make_clip_atlas.py
"""
import json
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..', 'Saved', 'DirectionFindingClip')
CELL_W, CELL_H = 480, 270
COLUMNS = 8
FIRST, STEP, FRAMES = 12, 2, 96
# The map with its frame and grid letters in the 1920 x 1080 recording.
CROP = (505, 84, 915, 912)
BACKGROUND = (14, 19, 24)
# Recorded frame ranges and their tags (events at 24 / 72 / 132 / 180, see the DF_CLIP lines).
LABELS = [(24, 71, 'DF 1 HELD: A BEARING'), (72, 131, 'ATTACK ORDER: NARROWER'),
          (132, 179, 'DF 2 HELD: CROSS BEARING'), (180, 999, 'ATTACK ORDER: NARROWER')]
FONT = 'C:/Windows/Fonts/arialbd.ttf'
FONT_SIZE = 13
PLATE = (12, 12, 12, 200)
BAR = (200, 32, 28, 255)
TEXT = (240, 236, 228, 255)


def label_for(frame):
    for first, last, text in LABELS:
        if first <= frame <= last:
            return text
    return ''


def make_cell(path, frame):
    x, y, w, h = CROP
    source = Image.open(path).convert('RGB').crop((x, y, x + w, y + h))
    side = CELL_H
    square = source.resize((round(w * side / h), side), Image.LANCZOS)
    cell = Image.new('RGBA', (CELL_W, CELL_H), BACKGROUND + (255,))
    cell.paste(square, ((CELL_W - square.width) // 2, 0))
    text = label_for(frame)
    if text:
        draw = ImageDraw.Draw(cell)
        font = ImageFont.truetype(FONT, FONT_SIZE)
        box = draw.textbbox((0, 0), text, font=font)
        width, height = box[2] - box[0], box[3] - box[1]
        draw.rectangle((8, 8, 8 + 4 + 8 + width + 8, 8 + height + 10), fill=PLATE)
        draw.rectangle((8, 8, 12, 8 + height + 10), fill=BAR)
        draw.text((20, 8 + 5 - box[1]), text, font=font, fill=TEXT)
    return cell.convert('RGB')


def main():
    kept = [FIRST + i * STEP for i in range(FRAMES)]
    rows = (len(kept) + COLUMNS - 1) // COLUMNS
    atlas = Image.new('RGB', (CELL_W * COLUMNS, CELL_H * rows), BACKGROUND)
    cells = []
    for index, frame in enumerate(kept):
        path = os.path.join(ROOT, 'Frames', 'frame_%04d.png' % frame)
        cell = make_cell(path, frame)
        cells.append(cell)
        atlas.paste(cell, ((index % COLUMNS) * CELL_W, (index // COLUMNS) * CELL_H))
    atlas_path = os.path.join(ROOT, 'T_DirectionFinding_ClipAtlas.png')
    atlas.save(atlas_path)
    cells[0].save(os.path.join(ROOT, 'clip_preview.gif'), save_all=True, append_images=cells[1:], duration=83, loop=0)
    info = {'columns': COLUMNS, 'rows': rows, 'frame_count': len(kept), 'first': FIRST, 'step': STEP,
            'cell': [CELL_W, CELL_H], 'crop': CROP}
    with open(os.path.join(ROOT, 'clip_atlas.json'), 'w') as handle:
        json.dump(info, handle, indent=1)
    print(atlas_path, atlas.size, info)


if __name__ == '__main__':
    main()
