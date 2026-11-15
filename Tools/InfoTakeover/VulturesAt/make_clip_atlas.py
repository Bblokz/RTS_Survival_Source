"""Packs the recorded clip frames of a clip takeover into the flipbook atlas UW_ClipInfoTakeover plays.

--clip names the takeover (VulturesAt by default, RattleFeed for the Rattle-Feed takeover).
Reads Saved/InfoTakeover/<clip>/Frames/Frame_<n>.png (one per 1/24 s game tick, from capture_clip.sh), keeps every
STEP-th frame, scales each to FRAME_W x FRAME_H and lays them out COLUMNS wide, left to right, top to bottom.
Writes Saved/InfoTakeover/<clip>/T_<clip>_ClipAtlas.png plus clip_atlas.json (columns, rows, frame count),
a contact sheet of every 8th kept frame and clip_preview.gif for a quick look.

Usage:
  python make_clip_atlas.py [--clip RattleFeed] --start 40 [--step 2] [--frames 64] [--crop x,y,w,h]
  python make_clip_atlas.py --clip RattleFeed --segments 40-110,200-300 [--labels "40-81:NORMAL FIRE,..."]
--segments joins recorded frame ranges (inclusive) with a hard cut, in place of --start/--frames. A "Name:" prefix
reads that range from Frames<Name> (a second capture camera), e.g. --segments 0-48,Target:88-240. A "/N" suffix
keeps every N-th frame of that range in place of --step, to speed a slow part up: --segments 0-296/8,Wide:318-460.
--labels stamps a small tag on the recorded frames of each range (first-last:TEXT, comma separated).
The json says what was written; the asset script copies the grid into the widget's class defaults.
"""
import argparse
import glob
import json
import os

from PIL import Image, ImageDraw, ImageFont

SAVED_ROOT = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'Saved', 'InfoTakeover')
FRAME_W, FRAME_H = 480, 270
COLUMNS = 8
CLIP_FPS = 12
# Tag look, in pixels of the 1280 x 720 recording: white caps on a dark plate with a signal-red bar, top left.
LABEL_FONT = 'C:/Windows/Fonts/arialbd.ttf'
LABEL_FONT_SIZE = 40
LABEL_MARGIN = 28
LABEL_PADDING = (18, 10)
LABEL_BAR_WIDTH = 8
LABEL_PLATE = (12, 12, 12, 190)
LABEL_BAR = (200, 32, 28, 255)
LABEL_TEXT = (240, 236, 228, 255)


def parse_ranges(text):
    """@return (camera, first, last, step or None) per comma separated range; "Target:88-240/4" reads Frames<Camera>."""
    ranges = []
    for part in text.split(','):
        camera, span = part.split(':', 1) if ':' in part else ('', part)
        span, step = span.split('/') if '/' in span else (span, None)
        first, last = span.split('-')
        ranges.append((camera, int(first), int(last), int(step) if step else None))
    return ranges


def parse_labels(text):
    labels = []
    for part in [p for p in text.split(',') if p.strip()]:
        span, label = part.split(':', 1)
        first, last = span.split('-')
        labels.append((int(first), int(last), label.strip()))
    return labels


def stamp_label(frame, text):
    font = ImageFont.truetype(LABEL_FONT, LABEL_FONT_SIZE)
    overlay = Image.new('RGBA', frame.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    box = draw.textbbox((0, 0), text, font=font)
    width = box[2] - box[0] + 2 * LABEL_PADDING[0] + LABEL_BAR_WIDTH
    height = box[3] - box[1] + 2 * LABEL_PADDING[1]
    left, top = LABEL_MARGIN, LABEL_MARGIN
    draw.rectangle((left, top, left + width, top + height), fill=LABEL_PLATE)
    draw.rectangle((left, top, left + LABEL_BAR_WIDTH, top + height), fill=LABEL_BAR)
    draw.text((left + LABEL_BAR_WIDTH + LABEL_PADDING[0] - box[0], top + LABEL_PADDING[1] - box[1]), text,
              font=font, fill=LABEL_TEXT)
    return Image.alpha_composite(frame.convert('RGBA'), overlay).convert('RGB')


def camera_paths(saved, camera):
    return sorted(glob.glob(os.path.join(saved, 'Frames' + camera, 'Frame_*.png')))


def pick_frames(args, saved):
    """@return (recorded frame index, path) pairs in clip order."""
    paths = camera_paths(saved, '')
    if args.segments:
        picked = []
        for camera, first, last, step in parse_ranges(args.segments):
            segment_paths = camera_paths(saved, camera)
            picked += [(index, segment_paths[index])
                       for index in range(first, min(last, len(segment_paths) - 1) + 1, step or args.step)]
        return picked
    indexed = list(enumerate(paths))
    picked = indexed[args.start::args.step][:args.frames]
    if len(picked) < args.frames:
        print('only %d frames available from %d (wanted %d)' % (len(picked), args.start, args.frames))
    return picked


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--clip', default='VulturesAt', help='takeover folder and atlas name')
    parser.add_argument('--start', type=int, default=0, help='first recorded frame used')
    parser.add_argument('--step', type=int, default=2, help='24 fps recording / step = clip fps')
    parser.add_argument('--frames', type=int, default=64)
    parser.add_argument('--segments', default='', help='first-last recorded frame ranges joined with a hard cut')
    parser.add_argument('--labels', default='', help='first-last:TEXT tags stamped on recorded frames')
    parser.add_argument('--crop', default='', help='x,y,w,h crop of the 1280x720 frames before scaling (16:9)')
    args = parser.parse_args()

    saved = os.path.normpath(os.path.join(SAVED_ROOT, args.clip))
    picked = pick_frames(args, saved)
    labels = parse_labels(args.labels)
    crop = tuple(int(v) for v in args.crop.split(',')) if args.crop else None
    rows = (len(picked) + COLUMNS - 1) // COLUMNS
    atlas = Image.new('RGB', (COLUMNS * FRAME_W, rows * FRAME_H))
    cells = []
    for index, (recorded, path) in enumerate(picked):
        frame = Image.open(path).convert('RGB')
        if crop:
            frame = frame.crop((crop[0], crop[1], crop[0] + crop[2], crop[1] + crop[3]))
        for first, last, text in labels:
            if first <= recorded <= last:
                frame = stamp_label(frame, text)
        frame = frame.resize((FRAME_W, FRAME_H), Image.LANCZOS)
        cells.append(frame)
        atlas.paste(frame, ((index % COLUMNS) * FRAME_W, (index // COLUMNS) * FRAME_H))
    atlas_path = os.path.join(saved, 'T_%s_ClipAtlas.png' % args.clip)
    atlas.save(atlas_path)
    with open(os.path.join(saved, 'clip_atlas.json'), 'w') as handle:
        json.dump({'columns': COLUMNS, 'rows': rows, 'frames': len(picked), 'start': args.start, 'step': args.step,
                   'segments': args.segments, 'labels': args.labels, 'crop': args.crop,
                   'frame_size': [FRAME_W, FRAME_H]}, handle, indent=2)

    sheet_cells = cells[::8]
    sheet = Image.new('RGB', (4 * FRAME_W, ((len(sheet_cells) + 3) // 4) * FRAME_H))
    for index, cell in enumerate(sheet_cells):
        sheet.paste(cell, ((index % 4) * FRAME_W, (index // 4) * FRAME_H))
    sheet.save(os.path.join(saved, 'clip_contact_sheet.png'))
    cells[0].save(os.path.join(saved, 'clip_preview.gif'), save_all=True, append_images=cells[1:],
                  duration=int(1000 / CLIP_FPS), loop=0)
    print('atlas %dx%d, %d frames (%d x %d) -> %s' % (atlas.width, atlas.height, len(picked), COLUMNS, rows,
                                                      atlas_path))


if __name__ == '__main__':
    main()
