import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "common"))
import df_parts as df  # noqa: E402

TITLE = "F  Peilstelle: loop hut inside a small Adcock square"
SUBTITLE = df.SUB + "site 15 x 15 m, masts 7 m, loop top 6.9 m, flag 7.5 m"
NOTES = [
    "Both aerials the brief names: four 7 m steel masts on a 12 m square plus the loop hut of A at the back.",
    "Masts carry red-white bands (fortress radio paint); feeders run on low posts to the hut.",
    "Generator outside the square on the right; flag at the front of the square, holder sign beside it.",
    "Reads as 'radio station' from any zoom; the square marks the site, its front half stays an open yard.",
    "Busiest of the six: more parts to model, but one mesh serves every station (no Loop / Adcock split).",
]
VIEW_ZOOM = df.VIEW_ZOOM
CAMERA_TARGET = (0, 2.0, 3.0)
VIEW_OVERRIDES = df.view_overrides(3.5)
SCALE_REFS = not df.ICON


def build(k):
    df.setup(k)
    df.yard(k, 5.5, (0, 0.5))
    for i, (x, y) in enumerate([(-6, -3.5), (6, -3.5), (6, 8.5), (-6, 8.5)]):
        k.cyl("adcock_mast", 0.12, 7.0, (x, y, 0), r_top=0.07, color="frame", segments=10, cat="mast")
        for b in range(3):
            k.cyl("mast_band", 0.13, 0.6, (x, y, 1.6 + b * 2.0), color="mark_enemy", segments=10, cat="mast")
        k.box("mast_foot", (0.6, 0.6, 0.25), (x, y, 0), color="concrete")
        k.beam(df.rot((x, y), (-0.7, 0), 45 + i * 90) + (6.5,), df.rot((x, y), (0.7, 0), 45 + i * 90) + (6.5,),
               w=0.08, color="aerial", name="crossarm")
        for g in range(3):
            gx, gy = df.rot((x, y), (3.0, 0), 45 + i * 90 + 180 + (g - 1) * 70)
            k.beam((x, y, 5.8), (gx, gy, 0.1), w=0.03, color="cable", name="guy")
        df.feeder(k, (x * 0.9, y * 0.9 + 0.25), (0.5 + (1.0 if x > 0 else -1.0) * 1.6, 4.2), z=0.6, posts=3)
    k.label("Adcock mast (x4)", "aerial", (6, -3.5, 8.0))
    df.hut(k, 0.5, 5.6, 4.2, 3.4)
    df.loop_aerial(k, (0.5, 5.6, 3.9), 1.0)
    df.lamp(k, -1.6, 3.8, 2.4)
    df.generator(k, 9.6, 4.0, yaw=-90)
    df.flag(k, -2.8, -1.6)
    df.holder_sign(k, 2.8, -2.6, yaw=-20)
    df.progress_bar(k, 9.0, 0, 2.0)
