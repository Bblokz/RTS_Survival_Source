"""Finishes the German building expansion (BXP) portraits from run_capture.sh pictures (ger_bxps.txt,
ger_bxps_backdrop.txt): crop -> 256 NoIcon portrait -> placard version (TacticalIconBackgrounds/placard_portraits.py with
plans/bxps.txt) -> manifest.json + contact sheet.

Picks file (bxp_picks.txt), one line per BXP:
  <EnumType>=<Short>|<capture folder>|<view>[|zoom=1.05][|dx=0][|dy=0.03][|box=cx,cy,side][|gain=1][|backdrop=...][|notes=...]
  view = FrontRight / Right / Front (idle picture). zoom > 1 crops tighter. box=cx,cy,side (capture pixels) replaces
  the logged crop box where the BXP's bounds are wrong (the 37mm flak turret actors report far too large bounds).
  capture=<entry name> picks another capture folder (phase 2 entries carry the view yaw, e.g. T_Ger_Bxp_X_Y130);
  image=<png> uses a finished picture instead of a capture (folder and view fields are then ignored, write -).
  dx/dy move the crop centre by a fraction of the crop side (dy > 0 moves the frame down, so the subject sits higher, clear of the bottom badges).

Usage: python bxp_portraits.py bxp_picks.txt [--only Short,Short]
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from crop_portraits import read_crop_boxes  # noqa: E402

OUT = Path("D:/UE_Assets/Icons/TrainingUI/Expansions/Ger")
NOICON = OUT / "NoIcon"
PLAN = HERE.parent / "TacticalIconBackgrounds" / "plans" / "bxps.txt"
ENGRAVER = HERE.parent / "TacticalIconBackgrounds" / "placard_portraits.py"
SIZE = 256
# A little tighter than the logged box and the BXP a little above centre, so the two bottom badges cover less of it.
DEFAULT_ZOOM = 1.05
DEFAULT_DY = 0.03


def read_picks(path):
    picks = []
    for line in Path(path).read_text(encoding="utf-8-sig").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        enum_type, spec = line.split("=", 1)
        fields = spec.split("|")
        pick = {"type": enum_type.strip(), "short": fields[0].strip(), "folder": Path(fields[1].strip()),
                "view": fields[2].strip(), "zoom": DEFAULT_ZOOM, "dx": 0.0, "dy": DEFAULT_DY, "backdrop": None, "notes": "",
                "gain": 1.0}
        for field in fields[3:]:
            key, value = field.split("=", 1)
            key = key.strip()
            if key == "box":
                pick[key] = tuple(float(v) for v in value.split(","))
            elif key in ("zoom", "dx", "dy", "gain"):
                pick[key] = float(value)
            else:
                pick[key] = value.strip()
        picks.append(pick)
    return picks


def crop(pick):
    if "image" in pick:
        # A finished picture used as is (e.g. the old HQ flak defense render, which still shows the gun and crew).
        source = Path(pick["image"])
        return Image.open(source).convert("RGB").resize((SIZE, SIZE), Image.LANCZOS), source
    unit = pick.get("capture") or f"T_Ger_Bxp_{pick['short']}"
    folder = pick["folder"] / unit
    name = f"{unit}_{pick['view']}_idle.png"
    source = folder / name
    image = Image.open(source).convert("RGB")
    min_x, min_y, max_x, max_y = read_crop_boxes(folder).get(name, (0, 0, image.width, image.height))
    if "box" in pick:
        centre_x, centre_y, box_side = pick["box"]
        min_x, min_y = centre_x - box_side * 0.5, centre_y - box_side * 0.5
        max_x, max_y = centre_x + box_side * 0.5, centre_y + box_side * 0.5
    side = min(max(max_x - min_x, max_y - min_y) / pick["zoom"], image.width, image.height)
    centre_x = (min_x + max_x) * 0.5 + pick["dx"] * side
    centre_y = (min_y + max_y) * 0.5 + pick["dy"] * side
    left = min(max(centre_x - side * 0.5, 0), image.width - side)
    top = min(max(centre_y - side * 0.5, 0), image.height - side)
    box = tuple(int(round(v)) for v in (left, top, left + side, top + side))
    picture = image.crop(box).resize((SIZE, SIZE), Image.LANCZOS)
    if pick["gain"] != 1.0:
        picture = ImageEnhance.Brightness(picture).enhance(pick["gain"])
    return picture, source


def write_sheet(entries, path, columns=6):
    cell, label_h, gap = 256, 22, 8
    rows = (len(entries) + columns // 2 - 1) // (columns // 2)
    sheet = Image.new("RGB", (columns * (cell + gap) + gap, rows * (cell + label_h + gap) + gap), (22, 23, 25))
    draw = ImageDraw.Draw(sheet)
    for index, (short, noicon, icon) in enumerate(entries):
        x = gap + (index % (columns // 2)) * 2 * (cell + gap)
        y = gap + (index // (columns // 2)) * (cell + label_h + gap)
        sheet.paste(Image.open(noicon).convert("RGB"), (x, y))
        if icon.exists():
            framed = Image.open(icon).convert("RGB")
            sheet.paste(framed, (x + cell + gap, y))
            small = framed.resize((75, 75), Image.LANCZOS)
            sheet.paste(small, (x + 2 * cell + gap - 75, y))
        draw.text((x + 2, y + cell + 3), short, fill=(235, 232, 220))
    sheet.save(path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("picks")
    parser.add_argument("--only", default="")
    args = parser.parse_args()
    only = {s for s in args.only.split(",") if s}
    NOICON.mkdir(parents=True, exist_ok=True)
    manifest_path = OUT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    picks = read_picks(args.picks)
    for pick in picks:
        if only and pick["short"] not in only:
            continue
        picture, source = crop(pick)
        noicon = NOICON / f"T_Ger_Bxp_{pick['short']}.png"
        picture.save(noicon)
        manifest[pick["type"]] = {
            "short": pick["short"],
            "noicon": noicon.as_posix(),
            "icon": (OUT / f"T_Ger_Bxp_{pick['short']}.png").as_posix(),
            "backdrop": pick["backdrop"],
            "source_capture": source.as_posix(),
            "notes": pick["notes"],
        }
        print(f"noicon {noicon}")
    result = subprocess.run([sys.executable, str(ENGRAVER), str(PLAN), "--out", str(OUT)], capture_output=True,
                            text=True)
    print(result.stdout[-3000:], result.stderr[-3000:])
    manifest_path.write_text(json.dumps(manifest, indent=1), encoding="utf-8")
    entries = [(v["short"], Path(v["noicon"]), Path(v["icon"])) for v in manifest.values()]
    write_sheet(entries, OUT / "contact_sheet.png")
    print(f"sheet {OUT / 'contact_sheet.png'} ({len(entries)} BXPs)")


if __name__ == "__main__":
    main()
