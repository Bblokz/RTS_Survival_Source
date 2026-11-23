import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "common"))
import df_parts as df  # noqa: E402

TITLE = "B  Adcock-Kreuz: four guyed masts round a buried goniometer"
SUBTITLE = df.SUB + "site 17 x 17 m, masts 10 m, flag 7.5 m"
NOTES = [
    "Four 10 m timber masts on a diamond (N/E/S/W, 8 m out), each with a crossarm and three guy wires.",
    "Feeders on low posts run from every mast to a concrete goniometer pit in the middle (0.8 m high, walk round it).",
    "Receiver hut outside the cross at the back-right, generator at the back-left; the yard between the masts is open.",
    "Flag front-left inside the cross: steel, 7.5 m, plinth, no guys; set apart from the timber masts by colour/height.",
    "Reads 'aerial array' from far away; widest footprint, the front mast stands in front of the yard.",
]
VIEW_ZOOM = df.VIEW_ZOOM
CAMERA_TARGET = (0, 1.5, 3.5)
VIEW_OVERRIDES = df.view_overrides(4.5)
SCALE_REFS = not df.ICON


def build(k):
    df.setup(k)
    df.yard(k, 6.5)
    masts = [(0, 8), (8, 0), (0, -8), (-8, 0)]
    for i, (x, y) in enumerate(masts):
        df.guyed_mast(k, x, y, 10.0, yaw0=30 + i * 90, guy_r=4.0)
        df.feeder(k, (x * 0.85, y * 0.85), (x * 0.2, y * 0.2), z=0.8, posts=3)
    k.label("Adcock mast (x4)", "aerial", (8, 0, 11.4))
    k.cyl("gonio_pit", 1.3, 0.8, (0, 0, 0), color="concrete", segments=20, round_top=0.1)
    k.cyl("gonio_lid", 0.8, 0.25, (0, 0, 0.8), color="steel", segments=16)
    k.label("Goniometer pit", "aerial", (0, 0, 1.4))
    df.hut(k, 6.2, 7.0, 3.6, 3.0, yaw=-45)
    df.lamp(k, 5.0, 5.6, 2.4)
    df.generator(k, -6.4, 6.8, yaw=45)
    df.flag(k, -3.2, -3.4)
    df.holder_sign(k, 4.0, -4.2, yaw=-30)
    df.progress_bar(k, 12.0, 0, 1.5)
