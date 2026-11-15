"""Contact sheet per section from a rendered preview: one row per shot, four frames across the shot.

    python section_sheets.py <frames dir> <frames per second of the render> [out dir]

Reads Saved/IntroCinematic/timeline.json; writes <out dir>/section_<n>.jpg (default Saved/IntroCinematic/sheets).
"""
import json
import os
import sys

from PIL import Image, ImageDraw

TIMELINE = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/timeline.json"
FRACTIONS = (0.12, 0.4, 0.68, 0.94)
W, H = 640, 360


def main():
    frames, fps = sys.argv[1], float(sys.argv[2])
    out = sys.argv[3] if len(sys.argv) > 3 else "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/sheets"
    os.makedirs(out, exist_ok=True)
    with open(TIMELINE, "r", encoding="utf-8") as handle:
        timeline = json.load(handle)
    sections = {}
    for shot in timeline["shots"]:
        sections.setdefault(shot["id"].split(".")[0], []).append(shot)
    for section, shots in sorted(sections.items()):
        sheet = Image.new("RGB", (len(FRACTIONS) * W, len(shots) * H))
        draw = ImageDraw.Draw(sheet)
        for row, shot in enumerate(shots):
            for column, fraction in enumerate(FRACTIONS):
                seconds = shot["start"] + shot["duration"] * fraction
                path = os.path.join(frames, "f.%05d.jpeg" % int(seconds * fps))
                if os.path.exists(path):
                    sheet.paste(Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS), (column * W, row * H))
                draw.rectangle([column * W, row * H + H - 18, column * W + 150, row * H + H], fill=(0, 0, 0))
                draw.text((column * W + 5, row * H + H - 15), "shot %s  %.1f s" % (shot["id"], seconds), fill=(255, 255, 0))
        path = os.path.join(out, "section_%s.jpg" % section)
        sheet.save(path, quality=90)
        print(path, len(shots), "shots")


if __name__ == "__main__":
    main()
