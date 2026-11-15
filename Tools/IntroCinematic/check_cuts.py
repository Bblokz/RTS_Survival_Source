"""Looks for junk frames at the cuts of a rendered intro cinematic: the first frames of every shot are compared with
the frame a little later in the same shot. A first frame that differs a lot from a later one, while the later ones
agree with each other, is a pop-in (level not shown yet, camera or grade one frame late).

    python check_cuts.py <frames dir> <fps>      -> prints suspects, writes Saved/IntroCinematic/cuts.jpg
"""
import json
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageStat

TIMELINE = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/timeline.json"
OUT = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/cuts.jpg"
SMALL = (320, 180)


def load(folder, number):
    path = os.path.join(folder, "f.%05d.jpeg" % number)
    if not os.path.exists(path):
        return None
    return Image.open(path).convert("RGB").resize(SMALL)


def difference(a, b):
    """Mean absolute difference with the top-middle (portrait, subtitles) and lower-left (cards) left out."""
    diff = ImageChops.difference(a, b)
    draw = ImageDraw.Draw(diff)
    draw.rectangle([100, 0, 220, 60], fill=(0, 0, 0))
    draw.rectangle([0, 130, 130, 180], fill=(0, 0, 0))
    return sum(ImageStat.Stat(diff).mean) / 3.0


def main():
    folder, fps = sys.argv[1], float(sys.argv[2])
    with open(TIMELINE, "r", encoding="utf-8") as handle:
        shots = json.load(handle)["shots"]
    rows = []
    suspects = 0
    for shot in shots:
        first = int(round(shot["start"] * fps))
        frames = [load(folder, first + k) for k in (0, 1, 2, 6, 10)]
        if any(f is None for f in frames):
            continue
        settle = difference(frames[3], frames[4])
        worst = max(difference(frames[k], frames[3]) for k in (0, 1, 2))
        junk = worst > max(12.0, settle * 4.0)
        suspects += junk
        print("%-5s frame %5d  first-vs-settled %5.1f  settled %5.1f  %s" % (shot["id"], first, worst, settle, "SUSPECT" if junk else ""))
        rows.append((shot["id"], frames, junk))
    sheet = Image.new("RGB", (SMALL[0] * 5, SMALL[1] * len(rows)))
    draw = ImageDraw.Draw(sheet)
    for r, (shot_id, frames, junk) in enumerate(rows):
        for c, frame in enumerate(frames):
            sheet.paste(frame, (c * SMALL[0], r * SMALL[1]))
        draw.text((4, r * SMALL[1] + 4), "%s%s" % (shot_id, "  SUSPECT" if junk else ""), fill=(255, 60, 60) if junk else (255, 255, 0))
    sheet.save(OUT, quality=85)
    print("suspects", suspects, "of", len(rows), "->", OUT)


if __name__ == "__main__":
    main()
