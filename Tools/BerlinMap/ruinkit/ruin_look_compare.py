"""Before | after sheets of two look-test runs (ruin_look_test.py), plus the two colour tables side by side.
usage: python ruin_look_compare.py <before folder name> <after folder name>
       -> Saved/Berlin/ruinkit_look/compare_<before>_<after>/<picture>.jpg and colours.txt
"""
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path('D:/UE5Projects/RTS_Survival/Saved/Berlin/ruinkit_look')


def main(before, after):
    out = ROOT / ('compare_%s_%s' % (before, after))
    out.mkdir(exist_ok=True)
    for path in sorted((ROOT / before).glob('*.png')):
        other = ROOT / after / path.name
        if path.name.startswith('_') or not other.exists():
            continue
        a, b = Image.open(path).convert('RGB'), Image.open(other).convert('RGB')
        sheet = Image.new('RGB', (a.width + b.width + 8, a.height), (0, 0, 0))
        sheet.paste(a, (0, 0))
        sheet.paste(b, (a.width + 8, 0))
        draw = ImageDraw.Draw(sheet)
        draw.text((10, 8), before, fill=(255, 255, 0))
        draw.text((a.width + 18, 8), after, fill=(255, 255, 0))
        sheet.save(out / (path.stem + '.jpg'), quality=92)
        diff = np.abs(np.asarray(a).astype(np.float32) - np.asarray(b).astype(np.float32)).mean() / 255
        print('%-24s mean abs difference %.4f' % (path.stem, diff))
    rows = []
    ca = json.loads((ROOT / before / 'colours.json').read_text())['spots']
    cb = json.loads((ROOT / after / 'colours.json').read_text())['spots']
    rows.append('%-26s %-34s %-34s' % ('spot (display sRGB)', before + ': rgb lum sat', after + ': rgb lum sat'))
    for name in sorted(ca):
        if name in cb:
            fmt = lambda c: '%s %.3f %.3f' % (c['rgb'], c['lum'], c['sat'])
            rows.append('%-26s %-34s %-34s' % (name, fmt(ca[name]), fmt(cb[name])))
    (out / 'colours.txt').write_text('\n'.join(rows) + '\n')
    print('\n'.join(rows))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
