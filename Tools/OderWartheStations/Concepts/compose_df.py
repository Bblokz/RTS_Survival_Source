"""Compose DF station concept sheets (system python + Pillow).

python compose_df.py <out_dir> [<sheet title>]
<out_dir> holds the render passes enemy/, player/, icon_enemy/, icon_player/ from run_round.sh.
Writes sheet_<key>.png per design and overview.png.
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

FONT = 'C:/Windows/Fonts/segoeui.ttf'
FONT_B = 'C:/Windows/Fonts/segoeuib.ttf'
KIND_RGB = {
    'flag': (215, 30, 30), 'held': (150, 40, 40), 'aerial': (40, 130, 220), 'hut': (120, 85, 50),
    'gen': (220, 158, 30), 'ui': (50, 190, 90),
}
KEY_TEXT = [('flag', 'Capture flag (colour = holder, pale = lowered)'), ('held', 'Holder sign / mark (swaps)'),
            ('aerial', 'Direction-finding aerials'), ('hut', 'Receiver hut / room'),
            ('gen', 'Generator'), ('ui', 'Capture bar (UI)')]
BG = (236, 237, 240)
MINIMAP_BG = (46, 54, 44)


def font(size, bold=False):
    return ImageFont.truetype(FONT_B if bold else FONT, size)


def draw_labels(img, view, size=19):
    d = ImageDraw.Draw(img, 'RGBA')
    f = font(size, True)
    placed = []
    for lab in sorted(view['labels'], key=lambda l: l['xy'][1]):
        x, y = lab['xy']
        if not (0 <= x <= img.width and 0 <= y <= img.height):
            continue
        col = KIND_RGB.get(lab['kind'], (40, 40, 40))
        tw = d.textlength(lab['text'], font=f)
        bx, by = x + 14, y - 34
        box = [bx, by, bx + tw + 12, by + size + 8]
        for _ in range(12):
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


def caption(img, text, size=20):
    d = ImageDraw.Draw(img)
    tw = d.textlength(text, font=font(size, True))
    d.rectangle([8, 6, 22 + tw, 12 + size + 6], fill=(255, 255, 255))
    d.text((14, 8), text, font=font(size, True), fill=(30, 30, 30))


def icon_master(out_dir, state, key, size=128):
    """Crop the transparent front elevation to its content and fit it in a square (the minimap icon source)."""
    img = Image.open(os.path.join(out_dir, 'icon_' + state, key + '_rts.png')).convert('RGBA')
    box = img.getchannel('A').point(lambda a: 255 if a > 20 else 0).getbbox()
    img = img.crop(box)
    scale = (size - 6) / max(img.width, img.height)
    img = img.resize((max(1, round(img.width * scale)), max(1, round(img.height * scale))), Image.LANCZOS)
    sq = Image.new('RGBA', (size, size), (0, 0, 0, 0))
    sq.alpha_composite(img, ((size - img.width) // 2, size - img.height - 3))
    return sq


def icon_tile(icon, px, pad=10):
    """Icon at px size on a minimap-coloured tile."""
    tile = Image.new('RGBA', (px + 2 * pad, px + 2 * pad), MINIMAP_BG + (255,))
    tile.alpha_composite(icon.resize((px, px), Image.LANCZOS), (pad, pad))
    return tile.convert('RGB')


def icon_panel(out_dir, key, w, h):
    panel = Image.new('RGB', (w, h), (250, 250, 250))
    d = ImageDraw.Draw(panel)
    d.text((14, 8), 'Minimap icon mock (front elevation, upright like the supply flags)', font=font(19, True),
           fill=(30, 30, 30))
    x = 14
    for state, name in (('enemy', 'Enemy-held'), ('player', 'Player-held')):
        icon = icon_master(out_dir, state, key)
        big = icon_tile(icon, 128)
        small = icon_tile(icon, 36, pad=6)
        panel.paste(big, (x, 44))
        panel.paste(small, (x + big.width + 12, 44 + big.height - small.height))
        d.text((x, 44 + big.height + 6), f'{name}: 128 px / 36 px', font=font(16), fill=(50, 50, 50))
        x += big.width + small.width + 60
    return panel


def load(out_dir, state, key):
    with open(os.path.join(out_dir, state, key + '.json')) as f:
        return json.load(f)


def sheet(out_dir, key):
    info = load(out_dir, 'enemy', key)
    pinfo = load(out_dir, 'player', key)
    v, pv = info['views'], pinfo['views']
    rts = Image.open(os.path.join(out_dir, 'enemy', v['rts']['file'])).convert('RGB')
    prts = Image.open(os.path.join(out_dir, 'player', pv['rts']['file'])).convert('RGB')
    top = Image.open(os.path.join(out_dir, 'enemy', v['top']['file'])).convert('RGB')
    dt = ImageDraw.Draw(top, 'RGBA')
    for x0, y0, x1, y1 in v['top']['grid']:
        dt.line([x0, y0, x1, y1], fill=(255, 255, 255, 90), width=1)
    draw_labels(rts, v['rts'])
    draw_labels(top, v['top'], 15)
    scale_bar(rts, v['rts']['px_per_m'], 10, '10 m (across the screen)')
    scale_bar(top, v['top']['px_per_m'], 10, '10 m  (grid 5 m)')
    caption(rts, 'Enemy-held (RTS view): red flag up, red star sign, lamp off')
    prts = prts.resize((800, 550), Image.LANCZOS)
    caption(prts, 'Player-held: faction flag up, Balkenkreuz sign, lamp lit')
    top = top.resize((540, 540), Image.LANCZOS)
    caption(top, 'Top down', 17)

    W, H = 2460, 1600
    out = Image.new('RGB', (W, H), BG)
    d = ImageDraw.Draw(out)
    d.text((20, 18), info['title'], font=font(44, True), fill=(25, 25, 25))
    d.text((22, 76), info.get('subtitle') or '', font=font(22), fill=(70, 70, 70))
    out.paste(rts, (20, 120))
    out.paste(prts, (1640, 120))
    out.paste(top, (1640, 690))
    out.paste(icon_panel(out_dir, key, 800, 330), (1640, 1250))
    y = 1240
    for note in info['notes']:
        d.text((24, y), '\u2022  ' + note, font=font(23), fill=(30, 30, 30))
        y += 36
    out.save(os.path.join(out_dir, f'sheet_{key}.png'))
    return info


def overview(out_dir, keys, title):
    cw, ch = 800, 550
    rows = (len(keys) + 2) // 3
    out = Image.new('RGB', (3 * cw + 80, rows * (ch + 50) + 110 + 90), BG)
    d = ImageDraw.Draw(out)
    d.text((20, 16), title, font=font(40, True), fill=(25, 25, 25))
    for i, key in enumerate(keys):
        info = load(out_dir, 'enemy', key)
        x = 20 + (i % 3) * (cw + 20)
        y = 90 + (i // 3) * (ch + 50)
        d.text((x, y), info['title'], font=font(22, True), fill=(25, 25, 25))
        rts = Image.open(os.path.join(out_dir, 'enemy', info['views']['rts']['file'])).convert('RGB')
        cell = rts.resize((cw, ch), Image.LANCZOS)
        prts = Image.open(os.path.join(out_dir, 'player', key + '_rts.png')).convert('RGB').resize((280, 192),
                                                                                                 Image.LANCZOS)
        cell.paste(prts, (cw - 284, ch - 196))
        ImageDraw.Draw(cell).rectangle([cw - 285, ch - 197, cw - 3, ch - 3], outline=(30, 30, 30), width=2)
        ImageDraw.Draw(cell).text((cw - 280, ch - 222), 'player-held', font=font(17, True), fill=(30, 30, 30))
        tx = 6
        for state in ('enemy', 'player'):
            tile = icon_tile(icon_master(out_dir, state, key), 64, pad=5)
            cell.paste(tile, (tx, 6))
            tx += tile.width + 6
        out.paste(cell, (x, y + 36))
    yk = 90 + rows * (ch + 50) + 10
    xk = 24
    for kind, text in KEY_TEXT:
        d.rectangle([xk, yk, xk + 28, yk + 28], fill=KIND_RGB[kind], outline=(30, 30, 30))
        d.text((xk + 38, yk - 2), text, font=font(21), fill=(30, 30, 30))
        xk += 48 + d.textlength(text, font=font(21))
    d.text((24, yk + 42), 'Top-left tiles of each cell: minimap icon mock, enemy-held / player-held. Inset: the same '
                          'station player-held. Panzer IV + soldier for scale.', font=font(20), fill=(60, 60, 60))
    out.save(os.path.join(out_dir, 'overview.png'))


def main():
    out_dir = sys.argv[1]
    title = sys.argv[2] if len(sys.argv) > 2 else 'Direction-finder station concepts'
    keys = sorted(os.path.splitext(f)[0] for f in os.listdir(os.path.join(out_dir, 'enemy')) if f.endswith('.json'))
    for key in keys:
        sheet(out_dir, key)
    overview(out_dir, keys, title)
    print('COMPOSED', len(keys), out_dir)


main()
