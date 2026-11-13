"""Pictures of the hero ruin height maps of atmosphere_host_probe.py, with the picked fire spot (plain python, Pillow).
    python Source/Tools/BerlinMap/atmosphere/atmosphere_host_picture.py   ->  Saved/Berlin/atmosphere/probe/host_<piece>.png
Grey = height (white 12 m and up), blue tint = hit something that is not the ruin (ground, rubble skirt), red dot = fire.
Local +X is up, local +Y is right.
"""
import glob
import json

from PIL import Image, ImageDraw

for path in glob.glob("D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/probe/host_*.json"):
    data = json.load(open(path))
    n, scale = data["n"], 8
    image = Image.new("RGB", (n * scale, n * scale), (40, 0, 40))
    draw = ImageDraw.Draw(image)
    for i, j, h, own in data["cells"]:
        v = max(0, min(255, int(h / 1200.0 * 235) + 20))
        colour = (v, v, v) if own else (int(v * 0.5), int(v * 0.6), min(255, v + 60))
        x, y = j * scale, (n - 1 - i) * scale
        draw.rectangle([x, y, x + scale - 1, y + scale - 1], fill=colour)
    if data["best"]:
        i = (data["best"][0] + data["half"]) / data["step"]
        j = (data["best"][1] + data["half"]) / data["step"]
        x, y = j * scale, (n - 1 - i) * scale
        draw.ellipse([x - 6, y - 6, x + 6, y + 6], outline=(255, 0, 0), width=3)
    image.save(path.replace(".json", ".png"))
    print(path.replace(".json", ".png"))
