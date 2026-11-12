# python make_sheets.py <captures_dir>
# Turns the headless scrub captures into contact sheets, side-by-side MP4/GIF clips and a progress-1 vs original
# difference number per prototype. PROTOTYPE tool.
import os
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw, ImageStat

d = sys.argv[1]
steps = 90
for key in ('wpo', 'vat'):
    picks = [0, 9, 18, 27, 36, 45, 54, 63, 72, 81, 90]
    tiles = []
    for i in picks:
        im = Image.open(os.path.join(d, '%s_%03d.png' % (key, i))).convert('RGB').resize((480, 300))
        ImageDraw.Draw(im).text((8, 6), 'progress %.1f' % (i / steps), fill=(255, 255, 0))
        tiles.append(im)
    sheet = Image.new('RGB', (480 * 4, 300 * 3), 'black')
    for k, im in enumerate(tiles):
        sheet.paste(im, ((k % 4) * 480, (k // 4) * 300))
    sheet.save(os.path.join(d, 'sheet_%s.png' % key))
    final = Image.open(os.path.join(d, '%s_%03d.png' % (key, steps))).convert('RGB')
    original = Image.open(os.path.join(d, 'original.png')).convert('RGB')
    diff = ImageChops.difference(final, original)
    print(key, 'progress 1 vs original: mean abs diff per channel', [round(v, 2) for v in ImageStat.Stat(diff).mean])

# side-by-side frames: prototype 1 left, prototype 2 right; forward then back (pack up)
os.makedirs(os.path.join(d, 'side'), exist_ok=True)
order = list(range(steps + 1)) + [steps] * 12 + list(range(steps, -1, -1)) + [0] * 6
for n, i in enumerate(order):
    a = Image.open(os.path.join(d, 'wpo_%03d.png' % i)).convert('RGB').resize((800, 500))
    b = Image.open(os.path.join(d, 'vat_%03d.png' % i)).convert('RGB').resize((800, 500))
    frame = Image.new('RGB', (1600, 540), 'black')
    frame.paste(a, (0, 40))
    frame.paste(b, (800, 40))
    dr = ImageDraw.Draw(frame)
    dr.text((10, 12), 'Prototype 1: procedural WPO', fill=(255, 230, 160))
    dr.text((810, 12), 'Prototype 2: baked VAT', fill=(255, 230, 160))
    dr.text((1480, 12), 'progress %.2f' % (i / steps), fill=(255, 255, 0))
    frame.save(os.path.join(d, 'side', 'f_%04d.png' % n))
mp4 = os.path.join(d, 'deploy_prototypes_side_by_side.mp4')
subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', '15', '-i', os.path.join(d, 'side', 'f_%04d.png'),
                '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '20', mp4], check=True)
print('wrote', mp4)
