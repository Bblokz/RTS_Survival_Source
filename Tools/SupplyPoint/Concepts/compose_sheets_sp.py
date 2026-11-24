"""Compose labelled concept sheets from render_concepts.py output (system python + Pillow).

python compose_sheets.py <out_dir> [<sheet title>]
Writes sheet_<key>.png per design and overview.png (all RTS views side by side + colour key).
"""
import glob
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

FONT = 'C:/Windows/Fonts/segoeui.ttf'
FONT_B = 'C:/Windows/Fonts/segoeuib.ttf'
KIND_RGB = {
    'preview': (38, 190, 215), 'radixite': (240, 195, 25), 'metal': (120, 170, 230),
    'parts': (175, 90, 50), 'dropoff': (70, 170, 70), 'crane': (255, 115, 15),
    'radar': (215, 40, 40), 'antenna': (215, 40, 40), 'exit': (90, 90, 90), 'pit': (150, 70, 170), 'net': (120, 50, 35),
    'module': (225, 60, 145), 'melt': (235, 65, 15), 'sandbag': (190, 165, 115),
    'ammo': (200, 150, 40), 'issue': (60, 110, 160),
    'flag': (215, 30, 30), 'truck': (95, 112, 70), 'lane': (50, 66, 102), 'ui': (50, 190, 90),
}
KEY_TEXT = [('flag', 'Capture flag: raised (pale = lowered)'), ('truck', 'Opel Blitz supply truck (real mesh)'),
            ('lane', 'Truck drive-in / drive-out, bay marks'), ('ui', 'Floating progress bar (UI)'),
            ('crane', 'Crane (animated), dashed = its path'), ('radixite', 'Radixite stock / skip'),
            ('metal', 'Metal stock / plate bundle'), ('parts', 'Vehicle parts stock / pallet')]
_UNUSED_KEY_TEXT = [('preview', 'Training / build preview'), ('radar', 'Radar, antennas'),
            ('crane', 'Crane mount (animated attachment)'), ('radixite', 'Radixite store'),
            ('metal', 'Metal store'), ('parts', 'Vehicle parts store'), ('dropoff', 'Resource drop-off'),
            ('exit', 'Unit exit'), ('pit', 'Weapon pit / casemate'), ('module', 'Expansion module and dock'),
            ('melt', 'Furnace light, red-hot oxide'), ('sandbag', 'German sandbag wall'),
            ('ammo', 'Ammunition (real faction meshes)'), ('issue', 'Issue door (infantry draw kit)'),
            ('net', 'Camo net (Hinterhalt Rotbraun)')]
BG = (236, 237, 240)


def font(size, bold=False):
    return ImageFont.truetype(FONT_B if bold else FONT, size)


def draw_labels(img, view, size=19):
    d = ImageDraw.Draw(img, 'RGBA')
    f = font(size, True)
    placed = []
    for lab in sorted(view['labels'], key=lambda l: l['xy'][1]):
        x, y = lab['xy']
        if not (0 <= x <= img.width and 0 <= y <= img.height):  # off a zoomed view: don't drag it in
            continue
        col = KIND_RGB.get(lab['kind'], (40, 40, 40))
        tw = d.textlength(lab['text'], font=f)
        bx, by = x + 14, y - 34
        box = [bx, by, bx + tw + 12, by + size + 8]
        for _ in range(12):  # nudge down until it clears earlier labels
            if not any(box[0] < p[2] and box[2] > p[0] and box[1] < p[3] and box[3] > p[1] for p in placed):
                break
            by += size + 10
            box = [bx, by, bx + tw + 12, by + size + 8]
        if box[2] > img.width - 4:
            shift = box[2] - img.width + 4
            box = [box[0] - shift, box[1], box[2] - shift, box[3]]
        placed.append(box)
        d.line([x, y, box[0], (box[1] + box[3]) / 2], fill=col + (255,), width=2)
        d.ellipse([x - 5, y - 5, x + 5, y + 5], fill=col + (255,), outline=(20, 20, 20, 255))
        d.rectangle(box, fill=(255, 255, 255, 225), outline=col + (255,), width=2)
        d.text((box[0] + 6, box[1] + 2), lab['text'], font=f, fill=(25, 25, 25, 255))


def scale_bar(img, px_per_m, metres=10, label=None):
    d = ImageDraw.Draw(img)
    x0, y0 = 24, img.height - 30
    x1 = x0 + px_per_m * metres
    d.rectangle([x0, y0, x1, y0 + 8], fill=(30, 30, 30))
    d.rectangle([x0 + px_per_m * metres / 2, y0 + 1, x1 - 1, y0 + 7], fill=(250, 250, 250))
    d.text((x0, y0 - 24), label or f'{metres} m', font=font(17, True), fill=(30, 30, 30))


def caption(img, text):
    d = ImageDraw.Draw(img)
    d.text((14, 8), text, font=font(20, True), fill=(30, 30, 30))


def sheet(out_dir, info):
    v = info['views']
    main_key = info.get('main_view', 'rts')
    side_key = 'rear' if main_key == 'rts' else 'rts'
    names = {'rts': 'RTS view (front-right)', 'rear': 'Rear (back-left)'}
    rts = Image.open(os.path.join(out_dir, v[main_key]['file'])).convert('RGB')
    top = Image.open(os.path.join(out_dir, v['top']['file'])).convert('RGB')
    rear = Image.open(os.path.join(out_dir, v[side_key]['file'])).convert('RGB')

    dt = ImageDraw.Draw(top, 'RGBA')
    for x0, y0, x1, y1 in v['top']['grid']:
        dt.line([x0, y0, x1, y1], fill=(255, 255, 255, 90), width=1)
    draw_labels(rts, v[main_key])
    draw_labels(top, v['top'], 15)
    scale_bar(rts, v[main_key]['px_per_m'], 10, '10 m (across the screen)')
    scale_bar(top, v['top']['px_per_m'], 10, '10 m  (grid 5 m)')
    caption(rts, v[main_key].get('caption') or names[main_key])
    caption(top, v['top'].get('caption') or 'Top down')
    caption(rear, v[side_key].get('caption') or names[side_key])

    W, H = 2460, 1600
    out = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(out)
    d.text((20, 18), info['title'], font=font(44, True), fill=(25, 25, 25))
    d.text((22, 76), info.get('subtitle') or f"Overall footprint {info['footprint'][0]} x {info['footprint'][1]} m   |   "
                                             f"structure up to {info['height']} m, masts to {info['mast_height']} m   |   "
                                             f"tank + soldier for scale",
           font=font(22), fill=(70, 70, 70))
    out.paste(rts, (20, 120))
    out.paste(top, (1640, 120))  # 800 x 800
    out.paste(rear.resize((800, 550)), (1640, 1035))
    y = 1240
    for note in info['notes']:
        d.text((24, y), '\u2022  ' + note, font=font(23), fill=(30, 30, 30))
        y += 36
    out.save(os.path.join(out_dir, f"sheet_{info['key']}.png"))


def overview(out_dir, infos, title):
    cw, ch = 800, 550
    rows = (len(infos) + 3) // 3  # one extra cell for the colour key
    used = {lab['kind'] for info in infos for view in info['views'].values() for lab in view['labels']}
    key_text = [entry for entry in KEY_TEXT if entry[0] in used]
    out = Image.new('RGB', (3 * cw + 80, rows * (ch + 50) + 110), BG)
    d = ImageDraw.Draw(out)
    d.text((20, 16), title, font=font(40, True), fill=(25, 25, 25))
    for i, info in enumerate(infos + [None]):
        x = 20 + (i % 3) * (cw + 20)
        y = 90 + (i // 3) * (ch + 50)
        if info is None:
            d.rectangle([x, y + 40, x + cw, y + 40 + ch], fill=(250, 250, 250), outline=(200, 200, 200))
            d.text((x + 24, y + 60), 'Colour key', font=font(28, True), fill=(25, 25, 25))
            for j, (kind, text) in enumerate(key_text):
                yy = y + 105 + j * 44
                d.rectangle([x + 28, yy, x + 60, yy + 32], fill=KIND_RGB[kind], outline=(30, 30, 30))
                d.text((x + 76, yy - 2), text, font=font(24), fill=(30, 30, 30))
            d.text((x + 28, y + 40 + ch - 36), 'Brown = timber, grey = concrete footings, dark grey = steel',
                   font=font(20), fill=(80, 80, 80))
            continue
        main = info['views'][info.get('main_view', 'rts')]
        img = Image.open(os.path.join(out_dir, main['file'])).convert('RGB').resize((cw, ch))
        d.text((x, y + 2), info['title'], font=font(26, True), fill=(25, 25, 25))
        out.paste(img, (x, y + 40))
    out.save(os.path.join(out_dir, 'overview.png'))


def main():
    out_dir = sys.argv[1]
    title = sys.argv[2] if len(sys.argv) > 2 else 'Concepts'
    infos = []
    for path in sorted(glob.glob(os.path.join(out_dir, '*.json'))):
        with open(path) as f:
            infos.append(json.load(f))
    for info in infos:
        sheet(out_dir, info)
    overview(out_dir, infos, title)
    print('sheets:', [i['key'] for i in infos])


main()
