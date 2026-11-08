"""Side-by-side before/after sheet + diff numbers for mesh_reduce.py captures.
Usage: python compare_shots.py <before tag> <after tag> <mesh name> [...]  -> Saved/PerfAudit/MeshShots/cmp_<mesh>.png"""
import sys
from PIL import Image, ImageChops, ImageStat

D = "D:/UE5Projects/RTS_Survival/Saved/PerfAudit/MeshShots/"
a_tag, b_tag = sys.argv[1], sys.argv[2]
for name in sys.argv[3:]:
    tiles, report = [], []
    for view in ("close", "rts"):
        a = Image.open(D + "%s_%s_%s.png" % (name, a_tag, view)).convert("RGB")
        b = Image.open(D + "%s_%s_%s.png" % (name, b_tag, view)).convert("RGB")
        diff = ImageChops.difference(a, b)
        mean = sum(ImageStat.Stat(diff).mean) / 3.0
        changed = sum(1 for p in diff.convert("L").getdata() if p > 24) / float(a.size[0] * a.size[1])
        report.append("%s mean=%.2f changed>24=%.3f%%" % (view, mean, changed * 100))
        amp = diff.point(lambda v: min(255, v * 4))
        if view == "rts":  # crop the centre so the tiny RTS view is visible
            box = (384, 384, 640, 640)
            a, b, amp = (im.crop(box).resize((512, 512), Image.NEAREST) for im in (a, b, amp))
        else:
            a, b, amp = (im.resize((512, 512)) for im in (a, b, amp))
        tiles.append((a, b, amp))
    sheet = Image.new("RGB", (512 * 3, 512 * len(tiles)))
    for r, row in enumerate(tiles):
        for c, im in enumerate(row):
            sheet.paste(im, (c * 512, r * 512))
    sheet.save(D + "cmp_%s.png" % name)
    print(name, " | ".join(report))
