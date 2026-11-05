"""Renders the showcase map's camera presets headlessly and writes contact sheets.

    python sc_shoot.py [filter] [--scale 0.25] [--out D:/.../Saved/Showcase/<name>] [--frames 48]

Runs the DebugGame editor binary as a windowed -game (the offscreen still path needs a real game viewport; a small
window opens for about a minute plus ~5 s per shot). Needs the RTS_SurvivalEditor DebugGame build with
Source/RTS_Survival/Showcase. Output: one PNG per camera (<Stage>__<Shot>.png) and contact_<Stage>.jpg sheets.
"""
import argparse
import datetime
import os
import subprocess
import sys
import time

PROJECT = "D:/UE5Projects/RTS_Survival"
EDITOR = "D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Win64-DebugGame.exe"
MAP = "/Game/RTS_Survival/Maps/Showcase/RTS_Showcase"


def contact_sheets(folder, thumb_height=360):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("PIL missing: no contact sheets")
        return []
    stages = {}
    for name in sorted(os.listdir(folder)):
        if name.endswith(".png") and "__" in name:
            stages.setdefault(name.split("__")[0], []).append(name)
    sheets = []
    for stage, files in stages.items():
        thumbs = []
        for name in files:
            image = Image.open(os.path.join(folder, name)).convert("RGB")
            width = int(image.width * thumb_height / image.height)
            thumb = image.resize((width, thumb_height), Image.LANCZOS)
            ImageDraw.Draw(thumb).text((8, 6), name[:-4], fill=(255, 255, 0))
            thumbs.append(thumb)
        sheet = Image.new("RGB", (sum(t.width for t in thumbs) + 8 * (len(thumbs) + 1), thumb_height + 16), (20, 20, 20))
        x = 8
        for thumb in thumbs:
            sheet.paste(thumb, (x, 8))
            x += thumb.width + 8
        path = os.path.join(folder, "contact_%s.jpg" % stage)
        sheet.save(path, quality=88)
        sheets.append(path)
    return sheets


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("filter", nargs="?", default="all")
    parser.add_argument("--scale", type=float, default=1.0)
    parser.add_argument("--frames", type=int, default=48)
    parser.add_argument("--out", default=None)
    parser.add_argument("--timeout", type=int, default=3600)
    args = parser.parse_args()
    out = args.out or "%s/Saved/Showcase/%s" % (PROJECT, datetime.datetime.now().strftime("%Y%m%d_%H%M%S"))
    os.makedirs(out, exist_ok=True)
    command = [EDITOR, PROJECT + "/RTS_Survival.uproject", MAP, "-game", "-windowed", "-ResX=1280", "-ResY=720",
               "-unattended", "-NoLiveCoding", "-NoSound", "-ShowcaseShots=" + args.filter,
               "-ShowcaseScale=%g" % args.scale, "-ShowcaseFrames=%d" % args.frames, "-ShowcaseDir=" + out,
               "-log=Showcase_Shots.log"]
    start = time.time()
    process = subprocess.Popen(command)
    try:
        process.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        print("timed out")
    print("render took %.0f s, exit %s" % (time.time() - start, process.returncode))
    for sheet in contact_sheets(out):
        print("sheet", sheet)
    print("out", out)


if __name__ == "__main__":
    sys.exit(main())
