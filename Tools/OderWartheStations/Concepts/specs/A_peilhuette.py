import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "common"))
import df_parts as df  # noqa: E402

TITLE = "A  Peilhuette: plank hut with a rotating loop on the roof"
SUBTITLE = df.SUB + "site 13 x 11 m, loop top 6.9 m, flag 7.5 m"
NOTES = [
    "The classic small DF post: receiver hut at the back of a gravel yard, loop aerial on a shaft through the roof.",
    "The loop turns slowly (enemy: steady sweep; player: hunting back and forth). Sense whip on the gable.",
    "Generator under a lean-to beside the hut, fuel drums; cable on two posts into the hut.",
    "Flag on a concrete plinth front-left of the yard; holder sign front-right. The yard centre stays open.",
    "Smallest and cheapest; reads as 'radio hut', but the loop is only 2 m across from the RTS camera.",
]
VIEW_ZOOM = df.VIEW_ZOOM
CAMERA_TARGET = (0, 1.5, 3.0)
VIEW_OVERRIDES = df.view_overrides(3.5)
SCALE_REFS = not df.ICON


def build(k):
    df.setup(k)
    df.yard(k, 5.5)
    df.hut(k, 0.5, 5.2, 4.2, 3.4)
    df.loop_aerial(k, (0.5, 5.2, 3.9), 1.0)
    df.whip(k, 2.6, 5.2, 3.0, 2.8)
    df.lamp(k, -1.6, 3.4, 2.4)
    df.generator(k, 5.6, 4.6, yaw=-90)
    df.feeder(k, (5.0, 3.0), (2.6, 3.9), z=2.0, posts=2)
    df.flag(k, -4.0, -0.5)
    df.holder_sign(k, 3.4, -2.6, yaw=-20)
    df.crates(k, -2.4, 6.6)
    df.progress_bar(k, 9.0, 0, 1.5)
