import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "common"))
import df_parts as df  # noqa: E402

TITLE = "D  Peilring: a ring of twelve short masts round the hut"
SUBTITLE = df.SUB + "ring 18 m across, masts 4.5 m, flag 7.5 m"
NOTES = [
    "A small circular array (late-war Wullenweber idea): twelve 4.5 m masts on a 9 m ring, joined by a top wire.",
    "The ring outlines the station from any zoom; units walk in through the gaps between masts.",
    "Receiver hut at the back of the ring, generator outside on the right, feeders from the ring to the hut.",
    "Flag front-left inside the ring: the only thing taller than the masts, so it reads at once.",
    "Most unusual silhouette; the ring is a 'capture zone' look without UI, but 12 masts cost more draws.",
]
VIEW_ZOOM = df.VIEW_ZOOM
CAMERA_TARGET = (0, 1.0, 3.0)
VIEW_OVERRIDES = df.view_overrides(3.5)
SCALE_REFS = not df.ICON


def build(k):
    df.setup(k)
    df.yard(k, 7.5)
    tops = []
    for i in range(12):
        a = 15 + i * 30
        x, y = df.rot((0, 0), (9.0, 0), a)
        k.cyl("ring_mast", 0.12, 4.5, (x, y, 0), r_top=0.07, color="timber", segments=8, cat="mast")
        k.box("ring_foot", (0.5, 0.5, 0.2), (x, y, 0), color="concrete")
        k.beam((x, y, 4.1), df.rot((0, 0), (9.6, 0), a) + (4.1,), w=0.08, color="aerial", name="ring_dipole")
        tops.append((x, y, 4.4))
    for a, b in zip(tops, tops[1:] + tops[:1]):
        k.beam(a, b, w=0.04, color="cable", name="ring_wire")
    k.label("Ring array (12 masts)", "aerial", (9.0, 0, 5.4))
    df.hut(k, 0.5, 4.6, 3.8, 3.0)
    df.whip(k, 2.2, 4.6, 3.0, 2.5)
    df.lamp(k, -1.4, 3.0, 2.4)
    for a in (75, 105, 135):
        x, y = df.rot((0, 0), (8.6, 0), a)
        df.feeder(k, (x, y), (0.5, 6.0), z=0.8, posts=2)
    df.generator(k, 11.5, 3.5, yaw=-90)
    df.flag(k, -3.4, -2.2)
    df.holder_sign(k, 3.2, -4.0, yaw=-20)
    df.progress_bar(k, 10.0, 0, 1.5)
