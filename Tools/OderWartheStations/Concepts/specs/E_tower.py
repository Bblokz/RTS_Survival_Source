import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "common"))
import df_parts as df  # noqa: E402

TITLE = "E  Peilturm: forest watchtower with the loop on its cabin"
SUBTITLE = df.SUB + "site 12 x 11 m, tower 9 m, loop top 12.8 m, flag 7.5 m"
NOTES = [
    "A timber fire-watch tower (the line runs through dead pine forest) re-used as the DF post: cabin at 7.2 m.",
    "Loop aerial on the cabin roof turns slowly; the tallest landmark of the six, seen over the trees.",
    "Generator shed at the tower foot, cable up the tower leg; ladder on the camera side.",
    "Flag front-left on its plinth, holder sign on the yard edge; the yard in front of the tower is open.",
    "Most readable over forest; the capture bar must sit high (14 m) and the tower hides units behind it.",
]
VIEW_ZOOM = df.VIEW_ZOOM
CAMERA_TARGET = (0, 2.0, 5.0)
VIEW_OVERRIDES = df.view_overrides(6.0)
SCALE_REFS = not df.ICON


def build(k):
    df.setup(k)
    df.yard(k, 5.5)
    tx, ty = 1.5, 5.0
    base, top, h = 1.7, 1.05, 7.2
    corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    for sx, sy in corners:
        k.box("tower_foot", (0.7, 0.7, 0.3), (tx + sx * base, ty + sy * base, 0), color="concrete")
        k.beam((tx + sx * base, ty + sy * base, 0.3), (tx + sx * top, ty + sy * top, h), w=0.22,
               color="timber", name="tower_leg")
    for z0, z1 in ((0.6, 3.6), (3.6, 6.6)):
        f0 = base + (top - base) * z0 / h
        f1 = base + (top - base) * z1 / h
        for (ax, ay), (bx, by) in zip(corners, corners[1:] + corners[:1]):
            k.beam((tx + ax * f0, ty + ay * f0, z0), (tx + bx * f1, ty + by * f1, z1), w=0.1, color="timber_dark",
                   name="brace")
            k.beam((tx + ax * f1, ty + ay * f1, z1), (tx + bx * f1, ty + by * f1, z1), w=0.12, color="timber_dark",
                   name="ring")
    k.box("cabin_floor", (2.9, 2.9, 0.2), (tx, ty, h), color="timber_dark")
    k.box("cabin", (2.5, 2.5, 2.0), (tx, ty, h + 0.2), color="timber")
    k.box("cabin_window", (2.0, 0.08, 0.6), (tx, ty - 1.27, h + 1.2), color="slot")
    k.prism("cabin_roof", [(-1.6, 0), (1.6, 0), (0, 0.8)], 3.0, (tx, ty, h + 2.2), color="steel")
    k.label("Watchtower cabin (receiver)", "hut", (tx - 1.5, ty, h + 1.5))
    for i in range(10):
        k.box("ladder_rung", (0.6, 0.06, 0.06), (tx + 0.4, ty - base + 0.1 + i * 0.065, 0.4 + i * 0.7),
              color="timber_dark")
    df.loop_aerial(k, (tx, ty, h + 2.9), 0.9)
    df.lamp(k, tx - 1.0, ty - 1.3, h + 1.6)
    df.hut(k, -3.0, 6.0, 2.6, 2.2, h=2.0, roof="flat", door=False, label="Generator shed")
    df.generator(k, -3.0, 3.4, yaw=0, shelter=False, label="Generator")
    df.flag(k, -4.4, -1.0)
    df.holder_sign(k, 3.2, -3.0, yaw=-20)
    df.progress_bar(k, 14.0, 0, 1.5)
