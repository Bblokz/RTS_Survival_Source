"""Contact sheet from rendered frames: python contact_sheet.py <frames dir> <out.jpg> <frame numbers...> [--cols N]"""
import os
import sys

from PIL import Image, ImageDraw


def main():
    args = sys.argv[1:]
    cols = 3
    if "--cols" in args:
        k = args.index("--cols")
        cols = int(args[k + 1])
        del args[k:k + 2]
    folder, out = args[0], args[1]
    numbers = [int(a) for a in args[2:]]
    w, h = 640, 360
    rows = (len(numbers) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * w, rows * h))
    draw = ImageDraw.Draw(sheet)
    for i, number in enumerate(numbers):
        path = os.path.join(folder, "f.%05d.jpeg" % number)
        if not os.path.exists(path):
            continue
        sheet.paste(Image.open(path).convert("RGB").resize((w, h), Image.LANCZOS), ((i % cols) * w, (i // cols) * h))
        draw.text(((i % cols) * w + 6, (i // cols) * h + h - 16), "f%d  %.2fs" % (number, number / 30.0), fill=(255, 255, 0))
    sheet.save(out, quality=90)
    print("sheet", out, len(numbers))


if __name__ == "__main__":
    main()
