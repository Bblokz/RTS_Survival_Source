"""Shot 2.4 (owner, 2026-11-21): the timeline advances from 1944 to 1945 while the Red Army, with its own V2s, rolls
west from the Soviet border into western Europe, told on the map table like a wartime briefing.

- 2.4a: the date line at the table's edge: a red pencil arrow runs from the 1944 tick to the 1945 tick.
- 2.4b: low over Poland, the miniatures (T-34 and IS-2 scale models on wargame bases, a green V2 on its stand)
  are pushed west behind a red arrow that grows ahead of them.
- 2.4c: wide over Europe, both thrusts sweep west past Germany (north along the coast, south over Vienna) to the
  Low Countries and France; Berlin is left alone (it stands until 3.3).

The arrows are flattened red bars that grow along their route (scale and position keyed) with a squashed cone as
the head (seen from above a cone is a triangle). Table-local cm: +X north, +Y east; west is yaw -90.
"""
import math

import intro_sets as sets

O = sets.ORIGINS["Intro_Bunker"]
TOP = sets.TABLE_TOP_Z
SURFACE = TOP + 0.4 + 0.5           # the countries' paper over the sea sheet
CUBE = "/Engine/BasicShapes/Cube"
CONE = "/Engine/BasicShapes/Cone"
RED = "/Game/RTS_Survival/Cinematics/Intro1976/Materials/MI_Intro_RedLine"
BASE = "/Game/RTS_Survival/Cinematics/Intro1976/Materials/MI_Intro_DarkWood"
V2 = "/Game/RTS_Survival/Meshes/Weapons/V2/SM_V2Rocket"
V2_GREEN = "/Game/RTS_Survival/Cinematics/V2Scene/V2materials/MI_V2_RussianGreen"
PENCIL_1945 = "/Game/RTS_Survival/Cinematics/Intro1976/Materials/MI_Intro_Pencil1945"
T34 = "Rus_Vehicle_T3476_Soviet"
IS2 = "Rus_Vehicle_IS-2_Soviet"
TANK_SCALE = 0.005                 # a T-34 is about 3.4 cm long on the table
V2_SCALE = 0.0019                  # about 4.8 cm tall

# Routes (lat, lon), east to west. North: Belarus, Warsaw, Pomerania, the coast past Hamburg, the Netherlands.
# South: Ukraine, Hungary, Vienna, Bavaria, Alsace.
NORTH = ((54.0, 28.8), (52.3, 22.6), (53.1, 17.2), (53.7, 12.6), (53.3, 8.6), (52.2, 4.6))
SOUTH = ((48.4, 29.2), (47.4, 23.6), (47.6, 18.8), (48.2, 15.2), (48.3, 10.6), (48.5, 6.0))


def world(x, y, z):
    return (O[0] + x, O[1] + y, O[2] + z)


def route(points):
    return [sets.map_local(*p) for p in points]


def _length(poly):
    return [math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(poly, poly[1:])]


def point_at(poly, f):
    """Point and heading (yaw, degrees) at fraction f of the polyline's length."""
    lengths = _length(poly)
    total = sum(lengths)
    d = max(0.0, min(1.0, f)) * total
    for (a, b), length in zip(zip(poly, poly[1:]), lengths):
        if d <= length:
            t = d / length if length else 0.0
            yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t), yaw
        d -= length
    a, b = poly[-2], poly[-1]
    return b, math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def arrow(label, poly, grow, width=0.9, steps=24, start=0.0):
    """A red map arrow growing along poly: grow = [(shot fraction, route fraction)] (monotonic). The shaft is one bar
    per route leg (lengthened by half a width at each end so the bends close); the head rides the tip."""
    lengths = _length(poly)
    total = sum(lengths)
    starts = [sum(lengths[:k]) for k in range(len(lengths))]

    def tip_at(fraction):
        for (f0, g0), (f1, g1) in zip(grow, grow[1:]):
            if f0 <= fraction <= f1:
                return g0 + (g1 - g0) * (fraction - f0) / max(f1 - f0, 1e-6)
        return grow[0][1] if fraction < grow[0][0] else grow[-1][1]

    actors = []
    for leg, (a, b) in enumerate(zip(poly, poly[1:])):
        yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
        ux, uy = (b[0] - a[0]) / lengths[leg], (b[1] - a[1]) / lengths[leg]
        keys = []
        for k in range(steps + 1):
            f = k / float(steps)
            covered = tip_at(f) * total - starts[leg]
            shown = max(0.0, min(lengths[leg], covered))
            # a leg that has not started is a speck under its start (scale 0 is avoided)
            extent = shown + (width if shown > 0.0 else 0.0)
            centre = (a[0] + ux * (shown / 2.0), a[1] + uy * (shown / 2.0))
            keys.append((f, world(centre[0], centre[1], SURFACE + 0.08), (0, yaw, 0),
                         (max(extent, 0.01) / 100.0, (width if shown > 0.0 else 0.01) / 100.0, 0.12 / 100.0)))
        actors.append({"kind": "mesh", "asset": CUBE, "label": "%s_Leg%d" % (label, leg), "mats": RED, "shadow": False,
                       "keys": keys})
    keys = []
    for k in range(steps + 1):
        f = k / float(steps)
        (x, y), yaw = point_at(poly, tip_at(f))
        head = 2.6 * width
        cx, cy = x + math.cos(math.radians(yaw)) * head * 0.35, y + math.sin(math.radians(yaw)) * head * 0.35
        keys.append((f, world(cx, cy, SURFACE + 0.1), (-90.0, yaw, 0.0), (0.18 / 100.0, head * 1.05 / 100.0, head / 100.0)))
    actors.append({"kind": "mesh", "asset": CONE, "label": label + "_Head", "mats": RED, "shadow": False, "keys": keys})
    return actors


def _piece_keys(poly, along, behind, side, lift=0.0):
    """Keys of a piece riding the route 'behind' cm behind the tip fraction along(f), 'side' cm to its right."""
    total = sum(_length(poly))
    keys = []
    for f, g in along:
        (x, y), yaw = point_at(poly, max(0.0, g - behind / total))
        r = math.radians(yaw)
        keys.append((f, world(x - math.sin(r) * side, y + math.cos(r) * side, SURFACE + lift), (0.0, yaw, 0.0)))
    return keys


def army(label, poly, along, v2=True):
    """A small army on wargame bases: T-34, IS-2, T-34, and a green V2 on its stand at the rear."""
    actors = []
    for index, (asset, behind, side) in enumerate(((T34, 3.2, -1.6), (IS2, 5.0, 1.8), (T34, 8.6, -1.2))):
        keys = _piece_keys(poly, along, behind, side, 0.35)
        actors.append({"kind": "class", "asset": asset, "label": "%s_Tank%d" % (label, index), "kit_scale": TANK_SCALE,
                       "keys": keys})
        actors.append({"kind": "mesh", "asset": CUBE, "label": "%s_Base%d" % (label, index), "mats": BASE,
                       "keys": [(k[0], (k[1][0], k[1][1], k[1][2] - 0.175), k[2], (4.0 / 100.0, 2.6 / 100.0, 0.35 / 100.0))
                                for k in keys]})
    if v2:
        keys = _piece_keys(poly, along, 12.5, 0.4, 0.35)
        actors.append({"kind": "mesh", "asset": V2, "label": label + "_V2", "mats": V2_GREEN,
                       "keys": [(k[0], k[1], k[2], V2_SCALE) for k in keys]})
        actors.append({"kind": "mesh", "asset": CUBE, "label": label + "_V2Base", "mats": BASE,
                       "keys": [(k[0], (k[1][0], k[1][1], k[1][2] - 0.175), k[2], (2.4 / 100.0, 2.4 / 100.0, 0.35 / 100.0))
                                for k in keys]})
    return actors


# --- 2.4a: the date line ------------------------------------------------------------------------------------------
# The strip (intro_sets 'Map_DateLine'): centre x -64, 20 cm deep, 320 cm along Y; the ruled line lies 7.5 cm in from
# its map-side edge (x about -61.5), the empty band between line and edge is x -61 .. -54. Year ticks every 9.27 cm
# from 1944 at y -148.3.
YEAR_1944 = -148.3
YEAR_STEP = (3946.0 - 150.0) / 4096.0 * 320.0 / 32.0


def date_arrow():
    """The arrow over the line, then '1945' written in red pencil under the 1945 tick (MPC Pencil, shot 2.4a)."""
    poly = [(-57.6, YEAR_1944), (-57.6, YEAR_1944 + YEAR_STEP - 0.9)]
    return arrow("DateArrow", poly, [(0.0, 0.0), (0.18, 0.0), (0.7, 1.0), (1.0, 1.0)], width=0.55) + [
        {"kind": "mesh", "asset": "/Engine/BasicShapes/Plane", "label": "Pencil1945", "mats": PENCIL_1945, "shadow": False,
         # right of the 1945 tick: the big '1944' under its own tick reaches past the 1945 tick
         "keys": [(0.0, world(-66.0, YEAR_1944 + YEAR_STEP + 4.5, TOP + 0.56), (0, 90, 0), (0.13, 0.065, 1.0))]}]


DATE_PENCIL = {"Pencil": [(0.0, 0.0), (0.7, 0.0), (0.92, 1.0), (1.0, 1.0)]}


def thrusts_2_4b():
    """Low over Poland: the north army pushed from the Vistula toward the Oder; its arrow grows ahead of it."""
    poly = route(NORTH)
    along = [(f, 0.25 + 0.12 * f) for f in (0.0, 0.25, 0.5, 0.75, 1.0)]
    return arrow("NorthArrow", poly, [(0.0, 0.33), (1.0, 0.47)]) + army("North", poly, along)


def thrusts_2_4c():
    """Wide over Europe: both thrusts sweep west to the Low Countries and France."""
    actors = []
    for label, points, (g0, g1) in (("North", NORTH, (0.42, 1.0)), ("South", SOUTH, (0.30, 1.0))):
        poly = route(points)
        actors += arrow(label + "Arrow", poly, [(0.0, g0), (0.85, g1), (1.0, g1)], width=1.6, steps=30)
        along = [(f, (g0 - 0.08) + (g1 - 0.13 - (g0 - 0.08)) * min(1.0, f / 0.85)) for f in (0.0, 0.2, 0.4, 0.6, 0.85, 1.0)]
        actors += army(label, poly, along)
    return actors


def camera_point(lat, lon, back=0.0, side=0.0, up=0.0):
    x, y = sets.map_local(lat, lon)
    return world(x - back, y + side, TOP + up)


def map_mpc():
    """1944: nothing burnt or stained on the map yet."""
    return {"Rings36": [(0.0, 0.0)], "Rings42": [(0.0, 0.0)], "Rings62": [(0.0, 0.0)], "Stain": [(0.0, 0.0)],
            "StainOpacity": [(0.0, 0.0)], "Pencil": [(0.0, 0.0)]}
