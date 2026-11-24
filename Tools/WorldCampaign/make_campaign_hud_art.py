"""Makes the campaign HUD's own art (sheet 14A, steel and cream) into UISkins/13_Frontstahl_Gunmetal/WorldCampaign.

  T_Campaign_BoltedPanel  half-size copy of Slice/T9_Theme01_T_Panel01_Dark: Slate draws box brush corners at texture
                          size, so this is one texel per UI unit (corner 37 units)
  T_Campaign_GlassDome    grey-scale glass dome for the commander ability button; the widget tints it per state
  T_Campaign_GlassBezel   gunmetal ring that sits around the dome

Run with plain python (needs Pillow), then import with run_import_campaign_hud_art.ps1.
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
SKIN = os.path.abspath(os.path.join(HERE, '..', '..', '..', 'UISkins', '13_Frontstahl_Gunmetal'))
OUT = os.path.join(SKIN, 'WorldCampaign')
SUPERSAMPLE = 4


def make_panel():
    source = Image.open(os.path.join(SKIN, 'Slice', 'T9_Theme01_T_Panel01_Dark.png')).convert('RGBA')
    source.resize((source.width // 2, source.height // 2), Image.LANCZOS).save(os.path.join(OUT, 'T_Campaign_BoltedPanel.png'))


def make_dome(size=148):
    big = size * SUPERSAMPLE
    image = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    pixels = image.load()
    radius = big / 2.0
    for y in range(big):
        for x in range(big):
            dx, dy = (x + 0.5 - radius) / radius, (y + 0.5 - radius) / radius
            distance = math.hypot(dx, dy)
            if distance > 1.0:
                continue
            # Light falls from above: bright crown, dark lower rim, a faint bounce at the very bottom.
            crown = max(0.0, 1.0 - math.hypot(dx, dy + 0.38) / 1.25)
            rim = distance ** 3
            bounce = max(0.0, dy - 0.55) * (1.0 - abs(dx)) * 0.55
            value = 0.30 + 0.62 * crown - 0.26 * rim + bounce
            grey = int(255 * min(1.0, max(0.0, value)))
            pixels[x, y] = (grey, grey, grey, 255)
    # Specular window on the upper half.
    shine = Image.new('L', (big, big), 0)
    ImageDraw.Draw(shine).ellipse((big * 0.20, big * 0.07, big * 0.80, big * 0.46), fill=150)
    shine = shine.filter(ImageFilter.GaussianBlur(big * 0.035))
    image = Image.composite(Image.new('RGBA', (big, big), (255, 255, 255, 255)), image, shine)
    mask = Image.new('L', (big, big), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, big - 1, big - 1), fill=255)
    image.putalpha(mask)
    image.resize((size, size), Image.LANCZOS).save(os.path.join(OUT, 'T_Campaign_GlassDome.png'))


def make_bezel(size=208, inner=150):
    big = size * SUPERSAMPLE
    image = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    pixels = image.load()
    radius = big / 2.0
    inner_radius = inner / float(size)
    for y in range(big):
        for x in range(big):
            dx, dy = (x + 0.5 - radius) / radius, (y + 0.5 - radius) / radius
            distance = math.hypot(dx, dy)
            if distance > 1.0 or distance < inner_radius:
                continue
            across = (distance - inner_radius) / (1.0 - inner_radius)
            # A rounded ring: lit on top outside and on the bottom inside, with dark edges.
            facing = -dy / max(distance, 1e-6)
            profile = math.sin(across * math.pi)
            value = 0.30 + 0.34 * profile + 0.26 * facing * (across - 0.45) * 2.0
            if across < 0.08 or across > 0.94:
                value = 0.06
            grey = min(1.0, max(0.0, value))
            pixels[x, y] = (int(grey * 236), int(grey * 244), int(grey * 250), 255)
    image.resize((size, size), Image.LANCZOS).save(os.path.join(OUT, 'T_Campaign_GlassBezel.png'))


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    make_panel()
    make_dome()
    make_bezel()
    print('wrote', sorted(os.listdir(OUT)))
