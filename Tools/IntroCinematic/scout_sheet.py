"""Contact sheet of scout stills: python scout_sheet.py <tag> [cols]  ->  Saved/IntroCinematic/scout/<tag>.jpg"""
import glob
import os
import sys

from PIL import Image, ImageDraw

OUT = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/scout"


def main():
    tag = sys.argv[1]
    cols = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    files = sorted(glob.glob("%s/%s/*.png" % (OUT, tag)), key=os.path.getmtime)
    w, h = 640, 360
    rows = (len(files) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * h))
    draw = ImageDraw.Draw(sheet)
    for i, path in enumerate(files):
        sheet.paste(Image.open(path).convert("RGB").resize((w, h), Image.LANCZOS), ((i % cols) * w, (i // cols) * h))
        draw.text(((i % cols) * w + 6, (i // cols) * h + 4), os.path.basename(path)[:-4], fill=(255, 255, 0))
    sheet.save("%s/%s.jpg" % (OUT, tag), quality=88)
    print("sheet", "%s/%s.jpg" % (OUT, tag), len(files))


if __name__ == "__main__":
    main()
