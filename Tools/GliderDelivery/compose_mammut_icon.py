"""Global ability icon for the Mammut E-100 delivery, in the style of compose_ability_icons.py (system Python).

python compose_mammut_icon.py   (after render_icon_cutout.py wrote Saved/GlobalAbilityIcons/renders/Mammut_Tow_*)
Writes Saved/GlobalAbilityIcons/out/T_GA_GerMammutE100Drop.png.
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'GlobalAbilityIcons'))
import compose_ability_icons as cai  # noqa: E402

RENDERS = cai.RENDERS


def mask_from_color():
    """Blender writes coverage into the colour PNG's alpha; the composer wants alpha = 1 - coverage in a mask PNG."""
    color = Image.open(RENDERS + 'Mammut_Tow_color.png')
    if color.mode != 'RGBA':
        return
    alpha = color.split()[3]
    Image.merge('RGBA', (alpha, alpha, alpha, alpha.point(lambda v: 255 - v))).save(RENDERS + 'Mammut_Tow_mask.png')
    color.convert('RGB').save(RENDERS + 'Mammut_Tow_color.png')


def mammut_icon():
    cut, pts = cai.load_cutout('Mammut_Tow')
    left, top, right, bottom = cai.bbox(cut)
    # Fit the pair across the card above the hotkey label, centred left-right.
    scale = (cai.SIZE * 0.9) / (right - left)
    centre = ((left + right) / 2, (top + bottom) / 2)
    at = (cai.SIZE / 2 + (pts['origin'][0] - centre[0]) * scale,
          cai.SIZE * 0.42 + (pts['origin'][1] - centre[1]) * scale)
    img = cai.background('ger', 322)
    layer, _ = cai.place(cut, pts, 'origin', at, scale, gain=1.15)
    img = cai.over(img, layer)
    return cai.finish(img)


if __name__ == '__main__':
    mask_from_color()
    os.makedirs(cai.OUT, exist_ok=True)
    mammut_icon().save(cai.OUT + 'T_GA_GerMammutE100Drop.png')
    print('ICON', cai.OUT + 'T_GA_GerMammutE100Drop.png')
