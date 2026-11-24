import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "common"))
import df_parts as df  # noqa: E402

TITLE = "C  Werk-Peilstand: 1930s pillbox with a loop on its steel cupola"
SUBTITLE = df.SUB + "site 14 x 12 m, loop top 6.7 m, flag 7.5 m"
NOTES = [
    "An old Festungsfront MG pillbox (concrete, 6 x 5 m) re-used as the receiver room; the line's own architecture.",
    "Steel observation cupola on the roof; the loop shaft comes up through it and turns (same loop as A).",
    "Generator in a sandbagged revetment on the right; cable in a shallow trench to the pillbox door.",
    "Flag front-left on its plinth, holder mark on the pillbox face (stencilled red star / painted Balkenkreuz).",
    "Matches the Werke and dragon's teeth around it; reads 'bunker' first and 'radio' second.",
]
VIEW_ZOOM = df.VIEW_ZOOM
CAMERA_TARGET = (0, 1.5, 3.0)
VIEW_OVERRIDES = df.view_overrides(3.5)
SCALE_REFS = not df.ICON


def build(k):
    df.setup(k)
    df.yard(k, 5.5)
    k.box("pillbox", (6.0, 5.0, 2.4), (0, 5.4, 0), color="concrete", round_top=0.5, segments=4, taper=0.25)
    k.box("pillbox_door", (1.0, 0.1, 1.8), (-1.6, 2.88, 0), color="interior")
    k.box("embrasure", (1.4, 0.1, 0.35), (1.4, 2.9, 1.25), color="slot")
    k.label("Fortress pillbox (receiver room)", "hut", (-1.8, 5.4, 3.4))
    k.dome("cupola", (0.6, 5.6, 2.35), 1.15, 0.85)
    k.cyl("cupola_ring", 1.25, 0.12, (0.6, 5.6, 2.3), color="steel", segments=24)
    df.loop_aerial(k, (0.6, 5.6, 3.1), 1.4)
    df.whip(k, -2.2, 6.8, 2.3, 3.0)
    df.lamp(k, -0.6, 2.95, 2.0)
    if df.PLAYER:
        k.box("face_cross", (0.8, 0.06, 0.2), (2.0, 2.92, 1.9), color="mark_player")
        k.box("face_cross", (0.2, 0.06, 0.8), (2.0, 2.92, 1.6), color="mark_player")
    else:
        df.star(k, (2.0, 2.9, 1.98), 0.38, color="mark_enemy", depth=0.06)
    for i in range(7):
        a = -60 + i * 25
        x, y = df.rot((5.8, 1.0), (1.7, 0), a + 180)
        k.box("sandbag", (0.9, 0.55, 0.75), (x, y, 0), color="sandbag", yaw=a + 90, round_top=0.15, segments=2)
    df.generator(k, 5.8, 1.0, yaw=-90, shelter=False)
    k.box("cable_trench", (4.0, 0.5, 0.06), (2.4, 2.4, 0), color="interior", yaw=-15, cat="mark")
    df.flag(k, -4.2, -0.6)
    df.holder_sign(k, 3.2, -3.4, yaw=-20)
    df.progress_bar(k, 9.0, 0, 1.5)
