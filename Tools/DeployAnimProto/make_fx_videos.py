# python make_fx_videos.py <fx capture dir> <out dir>
# One full-frame 30 fps MP4 per deploy-effect setup (title + progress stamped on) and one labelled sheet with the three
# setups at the same moments. PROTOTYPE tool.
import glob
import os
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
TITLES = {'DustSettle': '1  Dust Settle  - landing dust, heavy dust on containers and mast',
          'FieldEngineers': '2  Field Engineers  - diesel exhaust, sparks on metal, welding, light dust',
          'SmokeScreen': '3  Smoke Screen  - smoke cloud covers the start, heavy pieces kick dust'}
FPS, DEPLOY_FRAMES = 30, 360
try:
    font = ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf', 26)
    small = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 22)
except OSError:
    font = small = ImageFont.load_default()
sheet_frames = [60, 120, 170, 230, 290, 360, 420]
rows = []
for setup, title in TITLES.items():
    frames = sorted(glob.glob(os.path.join(src, setup + '_*.png')))
    if not frames:
        print('no frames for', setup)
        continue
    tmp = os.path.join(out, 'tmp_' + setup)
    os.makedirs(tmp, exist_ok=True)
    row = []
    for i, f in enumerate(frames):
        im = Image.open(f).convert('RGB')
        d = ImageDraw.Draw(im)
        d.rectangle((0, 0, im.width, 44), fill=(0, 0, 0))
        d.text((14, 8), title, font=font, fill=(255, 230, 160))
        p = min(i / DEPLOY_FRAMES, 1.0)
        d.text((im.width - 250, 10), 'deploy %3d%%  %4.1f s' % (p * 100, i / FPS), font=small, fill=(255, 255, 120))
        im.save(os.path.join(tmp, 'f_%04d.png' % i))
        if i in sheet_frames:
            row.append(im.resize((480, 270)))
    rows.append(row)
    mp4 = os.path.join(out, 'deploy_fx_%s.mp4' % setup)
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', os.path.join(tmp, 'f_%04d.png'),
                    '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '21', mp4], check=True)
    print('wrote', mp4, len(frames), 'frames')
if rows:
    cols = max(len(r) for r in rows)
    sheet = Image.new('RGB', (480 * cols, 270 * len(rows)), 'black')
    for y, row in enumerate(rows):
        for x, im in enumerate(row):
            sheet.paste(im, (x * 480, y * 270))
    sheet.save(os.path.join(out, 'deploy_fx_sheet.png'))
    print('wrote sheet')
