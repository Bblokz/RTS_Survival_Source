"""The five SM_Desert_01 start locations plus the land between them (see survival_landforms.py for the operations).

Heights are world Z; the desert floor lies around 600. Start positions are local (x, y) in cm around the deploy centre:
+x points at the map centre, +y to the left when looking at it. WORLD features use world (x, y). The build disc (radius
8400) is forced flat, so every main pad outline keeps at least ~9400 from the centre; metal fields need flat ground to
about r 14600, so main pads reach past r 14800 on most sides. Lower tiers are partial shelves that dip into the desert
and pinch out; no drivable step is under ~500 cm. Ridge tops within 12500 are what the hill-top scan finds; ramps with
gate=True lead up from the desert. `covers` on a platform names the gates whose foot it can see; the generator checks it.
"""
import math
import random


def blob(cx, cy, length, width, angle_deg, seed, spikes=9):
    """Irregular elongated outline for rock remnants; the long axis follows the map's strike."""
    rng = random.Random(seed)
    a = math.radians(angle_deg)
    out = []
    for k in range(spikes):
        t = 2.0 * math.pi * k / spikes
        r = rng.uniform(0.72, 1.18)
        lx, ly = length * 0.5 * r * math.cos(t), width * 0.5 * r * math.sin(t)
        out.append((cx + lx * math.cos(a) - ly * math.sin(a), cy + lx * math.sin(a) + ly * math.cos(a)))
    return out


def knoll(name, points, crest, at, covers, reach, reach_from, radius=950.0, reach_full=False):
    """A guard post: a short rock hump on a rim, a gun platform on it, and a road up from the pad behind it, so the gun
    sits within cannon range of the ramp it watches and is reached from inside."""
    widths = [300.0] + [860.0] * (len(points) - 2) + [300.0]
    crests = [crest - 330.0] + [crest] * (len(points) - 2) + [crest - 330.0]
    return [
        dict(op="ridge", name=name, out_side=1, points=points, crest=crests, half_width=widths),
        dict(op="platform", name=name + "_top", at=at, radius=radius, covers=covers),
        dict(op="ramp", name=name + "_road", z_from=reach_from, z_to="ground", half_width=620.0, side=380.0, points=reach,
             full=True),
    ]


DUNE_RIDGE = dict(name="Dune Ridge", ops=[
    # One long sand-rock spine across the front, broken by the pass, sand banked against it. The base stands on one big
    # table; a shelf hangs off the rear left and sinks away into the desert, and a rear knoll watches the back roads.
    # Dunes banked against the lee of the spine and marching off it: the name of the place, readable from the base.
    dict(op="dune", name="drift_front_w", height=430.0, lee_side=-1,
         points=[(15000, -22500), (17200, -15000), (16200, -8500), (17600, -3200)]),
    dict(op="dune", name="drift_front_w2", height=300.0, lee_side=-1,
         points=[(20500, -19500), (22300, -12500), (21500, -6000)]),
    dict(op="dune", name="drift_front_e", height=400.0, lee_side=-1,
         points=[(17000, 7000), (18300, 13000), (16400, 19000), (15500, 25000)]),
    dict(op="dune", name="drift_front_e2", height=280.0, lee_side=-1,
         points=[(22500, 9500), (23500, 16000), (21500, 22000)]),
    dict(op="dune", name="drift_back", height=320.0, lee_side=1,
         points=[(-26500, -3000), (-27000, 6000), (-24500, 14000)]),
    dict(op="pad", name="back_shelf", z=900.0, dip=((-17000, -9000), (-11500, 13500), 610.0), outline=[
        (-6000, -15500), (-14000, -14500), (-19500, -8000), (-20000, -1000), (-17500, 6500), (-14500, 13500), (-10000, 14500),
        (-9000, 4000), (-9500, -6000)]),
    dict(op="pad", name="main", z=1420.0, build=True, outline=[
        (10600, -6500), (10800, 1500), (10400, 8500), (7500, 14000), (5500, 19000), (4200, 23000), (1800, 18500), (-1800, 15400), (-6500, 16500), (-10300, 13200),
        (-14500, 13500), (-11000, 9000), (-10300, 4000), (-13500, -1500), (-15500, -7500), (-11000, -14000), (-3000, -15500),
        (4000, -14800), (8800, -11000)]),
    dict(op="ridge", name="spine_west", out_side=-1,
         points=[(5500, -24500), (9000, -19500), (12800, -14500), (12300, -8000), (11900, -2200)],
         crest=[900, 1690, 2070, 2130, 1910], half_width=[320, 560, 800, 760, 980]),
    dict(op="ridge", name="spine_east", out_side=-1,
         points=[(11800, 3200), (12700, 9000), (11300, 14500), (13500, 20000), (10000, 24000)],
         crest=[1910, 2110, 2150, 1750, 900], half_width=[980, 760, 800, 520, 320]),
    dict(op="platform", name="pass_west", at=(12300, -2400), covers=["front_pass"]),
    dict(op="platform", name="pass_east", at=(12300, 3500), covers=["front_pass"]),
    dict(op="platform", name="spine_west_top", at=(12400, -9000), radius=1000.0),
    dict(op="platform", name="spine_east_top", at=(12500, 9000), radius=1000.0),
    dict(op="ramp", name="front_pass", gate=True, z_from="ground", z_to=1420.0, half_width=820.0, side=600.0,
         points=[(19600, 6600), (16400, 3400), (13900, 900), (11400, 300), (9300, 1000)]),
    *knoll("rear_knoll", [(-12900, -2300), (-14200, -5300), (-14500, -8300)], 1780.0, (-14200, -5400), ["back_road"],
           [(-8700, -5900), (-10800, -5700), (-12600, -5500), (-13600, -5400)], 1420.0),
    *knoll("flank_knoll", [(-400, -13300), (500, -14500), (2900, -14700)], 1770.0, (500, -14400), ["flank_road"],
           [(1600, -10600), (1400, -12300), (900, -13500), (600, -14000)], 1420.0),
    dict(op="ramp", name="back_road", gate=True, z_from="ground", z_to=900.0, half_width=760.0, side=560.0,
         points=[(-25500, -8000), (-22600, -6000), (-19500, -5200), (-16600, -4300)]),
    dict(op="ramp", name="flank_road", gate=True, z_from="ground", z_to=1420.0, half_width=780.0, side=560.0,
         points=[(1500, -25000), (800, -22000), (-1800, -19500), (-3000, -17000), (-2200, -14000)]),
    dict(op="ramp", name="shelf_to_main", z_from=900.0, z_to=1420.0, half_width=760.0, side=460.0,
         points=[(-15000, 4300), (-12800, 3200), (-10600, 1800), (-8600, 800)]),
    # A bay bitten into the north-west rim.
    dict(op="pad", name="bay", z=680.0, outline=[(-19500, 15500), (-13000, 12000), (-10800, 8200), (-12300, 6400), (-17500, 8500)]),
    dict(op="ramp", name="crest_west", z_from=1420.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(4500, -9500), (7000, -8300), (9300, -5800), (11400, -3400)]),
    dict(op="ramp", name="crest_east", z_from=1420.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(8300, 13200), (9800, 10500), (10600, 7400), (11300, 4800)]),
])

SALT_FLATS = dict(name="Salt Flats", no_fields=[(15500, -12500, 3600), (15800, -6200, 3600), (15800, 0, 3600), (15800, 6200, 3600), (15000, 12000, 3200)], ops=[
    # One long low bluff on the shore of a white salt pan, backed by a stony hogback; a shelf on the north side sinks away.
    dict(op="pad", name="salt_pan", z=455.0, profile="soft", scarp=1400.0, fray=0.8, paint="salt", paint_edge=500.0, outline=[
        (9800, -19500), (15500, -21500), (22000, -17500), (25500, -9500), (24000, -1000), (22500, 7500), (17000, 12500),
        (12300, 10500), (11200, 3000), (12000, -5000), (11000, -12500)]),
    dict(op="pad", name="north_shelf", z=680.0, dip=((-14000, 15000), (6500, 24000), 590.0), outline=[
        (-12500, 14500), (-16500, 19500), (-12000, 24500), (-4000, 26500), (4000, 26000), (6500, 21000), (2000, 18000), (-6000, 16000)]),
    dict(op="pad", name="main", z=1120.0, build=True, outline=[
        (9700, -9000), (10000, -1000), (10300, 6000), (10000, 12800), (5500, 18000), (-1500, 19500), (-7000, 16500), (-11200, 10500),
        (-10000, 2000), (-10800, -6500), (-8500, -13000), (-7500, -17500), (-1500, -17300), (5500, -15800)]),
    # A spur running out along the pan shore, and a bay on the south side.
    dict(op="pad", name="shore_spur", z=1120.0, steep=0.65, outline=[(8000, 11500), (11800, 12800), (15200, 13800), (16000, 15800), (12400, 16600), (9000, 16300), (6500, 15000)]),
    dict(op="pad", name="south_bay", z=640.0, outline=[(-15500, -21500), (-11500, -18500), (-9800, -14600), (-11800, -12900), (-16000, -15500)]),
    dict(op="ridge", name="hogback", out_side=1,
         points=[(-22000, -15500), (-15500, -11000), (-12400, -5500), (-11300, 1000), (-12500, 7500), (-17500, 13500), (-23500, 18500)],
         crest=[860, 1520, 1640, 1690, 1650, 1520, 850], half_width=[300, 620, 760, 1000, 760, 600, 300]),
    dict(op="ridge", name="spit", out_side=1, points=[(3500, -24500), (6500, -18500), (8600, -13500), (8000, -9700)],
         crest=[640, 1380, 1500, 1520], half_width=[260, 520, 660, 960]),
    dict(op="platform", name="hogback_top", at=(-11300, 1000), covers=[]),
    dict(op="platform", name="hogback_south", at=(-15000, -10500), radius=1000.0, covers=[]),
    dict(op="platform", name="spit_top", at=(7800, -9600), covers=[]),
    *knoll("shore_knoll", [(9800, 3400), (10400, 4500), (10000, 7000)], 1560.0, (10300, 4300), ["pan_grade"],
           [(6300, 6100), (8100, 5700), (9400, 4900), (9900, 4600)], 1120.0),
    *knoll("north_bluff", [(-1800, 18300), (-3500, 18700), (-5300, 17300)], 1540.0, (-3500, 18600), ["north_track"],
           [(-1400, 14600), (-2400, 16300), (-3000, 17500), (-3400, 18100)], 1120.0),
    *knoll("south_bluff", [(1800, -15900), (-400, -15300), (-400, -13800)], 1560.0, (-300, -15300), ["south_track"],
           [(1700, -10800), (1100, -12800), (300, -14200), (0, -14800)], 1120.0),
    dict(op="ramp", name="pan_grade", gate=True, z_from="ground", z_to=1120.0, half_width=1100.0, side=620.0,
         points=[(16800, 7000), (14400, 3400), (12300, 1700), (9400, 1000)]),
    dict(op="ramp", name="north_track", gate=True, z_from="ground", z_to=680.0, half_width=760.0, side=600.0,
         points=[(-6800, 32000), (-7000, 29000), (-6000, 25000), (-5000, 20600)]),
    dict(op="ramp", name="south_track", gate=True, z_from="ground", z_to=1120.0, half_width=780.0, side=600.0, full=True,  # round 5: graded all the way (it ran over the pad lobe)
         points=[(-3500, -26000), (-4600, -22500), (-3700, -19500), (-3300, -16000), (-2500, -13800)]),
    dict(op="ramp", name="shelf_to_main", z_from=680.0, z_to=1120.0, half_width=860.0, side=460.0,
         points=[(-8500, 21000), (-7800, 18900), (-6100, 16900), (-5900, 14900), (-4300, 13300)]),
    dict(op="ramp", name="crest_hogback", z_from=1120.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(-6400, 6600), (-8600, 4800), (-10200, 2800), (-11000, 1600)]),
    dict(op="ramp", name="crest_spit", z_from=1120.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(2600, -9700), (4600, -10300), (6300, -10300), (7500, -9800)]),
    dict(op="ramp", name="crest_hogback_s", z_from=1120.0, z_to="ground", half_width=620.0, side=380.0,
         points=[(-8200, -8000), (-10700, -7300), (-13000, -8000), (-14400, -9500)]),
])

SCORPION_BASIN = dict(name="Scorpion Basin", ops=[
    # A perched basin. From above it reads as the animal: the oval body holds the base, two unequal claws reach towards the
    # map centre round a walled forecourt, and the tail ridge whips out behind and curls back over itself.
    dict(op="pad", name="basin", z=1560.0, build=True, outline=[
        (21500, -6500), (22500, 0), (20500, 6500), (16000, 9500), (10000, 11500), (3500, 13500), (-4500, 13500), (-11800, 11000),
        (-16500, 7000), (-17500, 1500), (-17000, -6500), (-11800, -11000), (-4500, -13800), (3000, -13000), (9500, -11500),
        (16000, -9500)]),
    dict(op="ridge", name="claw_left", out_side=1,
         points=[(-14000, 11000), (-6000, 13400), (1500, 12200), (9000, 13500), (16000, 11500), (21500, 11200), (25000, 7500), (24000, 3400)],
         crest=[1740, 2220, 2260, 2200, 2180, 2120, 2000, 1820],
         half_width=[420, 760, 1000, 720, 760, 680, 760, 520]),
    dict(op="ridge", name="claw_right", out_side=-1,
         points=[(-13500, -11500), (-6500, -13800), (2000, -12300), (6000, -13300)],
         crest=[1720, 2190, 2270, 1780], half_width=[420, 760, 1000, 320]),
    dict(op="ridge", name="pincer_right", out_side=-1,
         points=[(12800, -12200), (14500, -11500), (19500, -9400), (23400, -6600), (23800, -3400)],
         crest=[1760, 2180, 2080, 1980, 1820], half_width=[320, 760, 660, 760, 520]),
    dict(op="pad", name="flank_bay", z=720.0, outline=[(6500, -18500), (7200, -13800), (8400, -11000), (10300, -10600), (11200, -13500), (12500, -18500)]),
    dict(op="ridge", name="claw_spur", out_side=1, points=[(9000, 13500), (10600, 17500), (14500, 21500), (13800, 25500)],
         crest=[2180, 1920, 1520, 980], half_width=[640, 520, 400, 260]),
    dict(op="ridge", name="tail", out_side=-1,
         points=[(-16700, 2300), (-19800, -500), (-22600, -5000), (-23200, -11000), (-20000, -15500), (-16000, -14600), (-15700, -11700)],
         crest=[2100, 1960, 1840, 1760, 1700, 1680, 1360], half_width=[720, 520, 440, 420, 380, 340, 240]),
    dict(op="platform", name="claw_left_top", at=(1700, 12000), covers=[]),
    dict(op="platform", name="claw_right_top", at=(2300, -12100), covers=[]),
    dict(op="platform", name="pincer_left", at=(16000, 11500), radius=1000.0, covers=[]),
    dict(op="platform", name="pincer_right", at=(14500, -11500), radius=1000.0, covers=[]),
    dict(op="platform", name="sting_left", at=(23900, 4000), radius=900.0, covers=["mouth"]),
    dict(op="platform", name="sting_right", at=(23500, -3900), radius=900.0, covers=["mouth"]),
    dict(op="platform", name="tail_root", at=(-17700, 2300), radius=1000.0, covers=["rear_gap"]),
    dict(op="ramp", name="mouth", gate=True, z_from="ground", z_to=1560.0, half_width=880.0, side=620.0,
         points=[(30800, -3800), (28000, -2500), (25000, -1200), (22600, 800), (20500, 1000)]),
    dict(op="ramp", name="rear_gap", gate=True, z_from="ground", z_to=1560.0, half_width=760.0, side=560.0,
         points=[(-22500, 12000), (-20500, 8700), (-18300, 5800), (-16000, 3900)]),
    dict(op="ramp", name="sting_left_road", z_from=1560.0, z_to="ground", half_width=620.0, side=380.0, full=True,
         points=[(18300, 6500), (20600, 6000), (22400, 4900), (23400, 4200)]),
    dict(op="ramp", name="sting_right_road", z_from=1560.0, z_to="ground", half_width=620.0, side=380.0, full=True,
         points=[(18000, -4600), (20400, -4300), (22200, -4000), (23000, -3900)]),
    dict(op="ramp", name="crest_left", z_from=1560.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(6800, 8400), (5000, 9500), (3200, 10800), (2000, 11700)]),
    dict(op="ramp", name="pincer_road", z_from=1560.0, z_to="ground", half_width=620.0, side=380.0, full=True,
         points=[(10900, -5900), (12600, -7900), (13800, -9700), (14300, -10800)]),
    dict(op="ramp", name="crest_right", z_from=1560.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(12000, -7700), (9200, -9300), (6200, -10600), (3600, -11700)]),
    dict(op="ramp", name="crest_tail", z_from=1560.0, z_to="ground", half_width=620.0, side=380.0,
         points=[(-12000, -1500), (-14300, 300), (-16000, 1500), (-17000, 2100)]),
])

RED_MESA = dict(name="Red Mesa", ops=[
    # The highest ground on the map. A blocky red table with walls up to 16 m, one bench on its map-centre flank, a long road
    # cut along the wall on the far side, and two detached buttes broken off the same rock.
    dict(op="pad", name="bench", z=1500.0, dip=((20000, -2000), (3000, -20000), 900.0), outline=[
        (15500, -19000), (21000, -10500), (21000, -2000), (17500, 2500), (13500, 0), (13000, -9000), (8500, -13500), (2500, -15000),
        (1500, -21000), (9000, -22500)]),
    dict(op="pad", name="butte_north", z=1980.0, outline=[(-1500, 21500), (2200, 20200), (4200, 22800), (2500, 26000), (-1500, 26800), (-3800, 24000)]),
    dict(op="pad", name="butte_east", z=2300.0, outline=[(18500, 13000), (22000, 12500), (24800, 15000), (25600, 18600), (22800, 21000), (19500, 18800), (17300, 15800)]),
    dict(op="pad", name="main", z=2260.0, build=True, outline=[
        (12500, -11000), (15500, -5500), (15000, 1500), (12500, 5500), (13200, 10000), (17000, 12300), (15200, 13900), (8000, 13800), (2000, 14200), (-4000, 15500),
        (-10500, 13500), (-15500, 10500), (-13300, 6800), (-14800, 2500), (-15500, -4000), (-12500, -9500), (-9500, -12500), (-3000, -15000),
        (4500, -15200), (9000, -14000)]),
    dict(op="ridge", name="rim_north", out_side=1,
         points=[(12800, 4400), (10600, 6700), (8700, 8400), (5500, 9900), (1400, 10500)],
         crest=[2300, 2680, 2780, 2720, 2300], half_width=[340, 700, 900, 640, 320]),
    dict(op="ridge", name="rim_south", out_side=-1,
         points=[(-1800, -11100), (-6000, -10500), (-8200, -9100), (-10800, -6800), (-11400, -3500)],
         crest=[2300, 2720, 2770, 2710, 2300], half_width=[320, 640, 900, 700, 340]),
    dict(op="ridge", name="east_knoll", out_side=-1, points=[(11000, -10500), (13300, -8400), (13900, -5900), (13500, -3900)],
         crest=[2300, 2640, 2680, 2300], half_width=[300, 820, 760, 300]),
    dict(op="platform", name="east_knoll_top", at=(13700, -6500), radius=1000.0, covers=[]),
    dict(op="platform", name="rim_north_top", at=(8800, 8300), covers=[]),
    dict(op="platform", name="rim_south_top", at=(-8200, -9100), covers=[]),
    *knoll("bench_knoll", [(17400, -9500), (19200, -9700), (20900, -9300)], 1870.0, (19200, -9500), ["bench_road"],
           [(15500, -6500), (16800, -8100), (17900, -8900), (18500, -9300)], "ground"),
    *knoll("road_guard", [(-13700, -1100), (-14700, -3500), (-14600, -6000)], 2620.0, (-14700, -3500), ["cliff_road"],
           [(-10300, -5000), (-12100, -4600), (-13500, -4000), (-14100, -3700)], 2260.0),
    dict(op="ramp", name="bench_road", gate=True, z_from="ground", z_to=1500.0, half_width=800.0, side=600.0,
         points=[(27500, -5000), (24500, -6600), (21600, -7000), (18700, -6200)]),
    dict(op="ramp", name="wall_road", z_from=1500.0, z_to=2260.0, half_width=780.0, side=480.0,
         points=[(18500, -2500), (16300, -1700), (14200, -1200), (12300, -1600), (10000, -1800)]),
    dict(op="ramp", name="cliff_road", gate=True, z_from="ground", z_to=2260.0, half_width=780.0, side=520.0,
         points=[(-17500, -21500), (-19000, -16000), (-18500, -9500), (-17700, -3500), (-15000, 500), (-11500, 1600)]),
    dict(op="ramp", name="crest_knoll", z_from=2260.0, z_to="ground", half_width=620.0, side=380.0,
         points=[(8400, -8900), (10200, -7900), (12000, -7000), (13000, -6700)]),
    dict(op="ramp", name="crest_north", z_from=2260.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(5900, 6500), (7300, 6700), (8300, 7300), (8800, 8100)]),
    dict(op="ramp", name="crest_south", z_from=2260.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(-11300, -800), (-11000, -3500), (-10000, -6200), (-8800, -8300)]),
])

DRY_WELLS = dict(name="Dry Wells", ops=[
    # A hillside falling to the wash: a big high table with one bold cliff, a low terrace crescent below it on the wash side
    # that sinks away northward, one short bench where the wells were dug, and a long rock wall closing the high side.
    dict(op="pad", name="low_terrace", z=1000.0, dip=((16000, -6000), (-7000, 22000), 620.0), outline=[
        (20500, -9500), (21500, 2000), (17000, 11000), (8500, 18500), (-1000, 22000), (-9500, 22500), (-7000, 17000), (5000, 10000),
        (11000, 1000), (11500, -9000)]),
    dict(op="pad", name="wells_bench", z=1230.0, profile="soft", scarp=2100.0, outline=[
        (13000, -7000), (17000, -6000), (17800, 1500), (14800, 6300), (11400, 5300)]),
    dict(op="pad", name="main", z=1680.0, build=True, outline=[
        (11000, -11000), (13500, -3000), (11500, 4500), (9500, 8500), (5000, 13200), (-2000, 15500), (-9000, 15500), (-15500, 12500), (-16500, 8200), (-12500, 6200), (-14500, 3000),
        (-13000, -2500), (-13800, -9000), (-7000, -14500), (3000, -15000)]),
    dict(op="ridge", name="back_wall", out_side=1,
         points=[(-23500, 12500), (-19000, 7300), (-13300, 3300), (-11300, -3000), (-8400, -9000), (-2600, -15000), (6000, -17500), (13000, -21000)],
         crest=[840, 1860, 2160, 2220, 2240, 2200, 1940, 900], half_width=[280, 520, 740, 1000, 1000, 720, 520, 280]),
    dict(op="ridge", name="south_nose", out_side=-1, points=[(5500, -12300), (8500, -11700), (11700, -9900), (13200, -7400)],
         crest=[1700, 2100, 2160, 1720], half_width=[300, 760, 900, 320]),
    *knoll("ford_knoll", [(16900, 3000), (18600, 3500), (20300, 3100)], 1460.0, (18600, 3500), ["wash_ford"],
           [(14700, 900), (16000, 1800), (17400, 2600), (18100, 3100)], "ground"),
    *knoll("road_knoll", [(-3200, 15000), (-1000, 15500), (1000, 15000), (2300, 13600)], 2040.0, (1000, 15000), ["long_road"],
           [(-3200, 9700), (-2700, 11700), (-1000, 13300), (300, 14500)], 1680.0),
    dict(op="platform", name="wall_west", at=(-11300, -3000), covers=[]),
    dict(op="platform", name="wall_south", at=(-8300, -9000), covers=["back_gate"]),
    dict(op="platform", name="nose_top", at=(9200, -11200), covers=["wash_ford"]),
    dict(op="platform", name="wall_east", at=(-2300, -15300), radius=1000.0, covers=["back_gate"]),
    dict(op="ramp", name="wash_ford", gate=True, z_from="ground", z_to=1000.0, half_width=840.0, side=620.0, full=True,  # round 5: graded through the ford knoll
         points=[(24000, 2800), (21900, 4800), (19800, 6000), (17500, 7000)]),
    dict(op="ramp", name="bench_to_main", z_from=1230.0, z_to=1680.0, half_width=820.0, side=440.0,
         points=[(14300, 3400), (12300, 2600), (10300, 2300), (9000, 2700)]),
    dict(op="ramp", name="long_road", gate=True, z_from="ground", z_to=1680.0, half_width=800.0, side=560.0,
         points=[(-500, 27000), (800, 23500), (2600, 20500), (4300, 17800), (4500, 15000), (3800, 12800), (2300, 11000), (1500, 9500)]),
    dict(op="ramp", name="back_gate", gate=True, z_from="ground", z_to=1680.0, half_width=760.0, side=560.0,
         points=[(-1500, -24500), (-1000, -21000), (-500, -18500), (500, -16000), (1200, -13000)]),
    dict(op="ramp", name="crest_wall", z_from=1680.0, z_to="ground", half_width=640.0, side=400.0,
         points=[(-5400, -5900), (-7400, -6200), (-9000, -6000), (-10300, -5000)]),
    dict(op="ramp", name="crest_nose", z_from=1680.0, z_to="ground", half_width=620.0, side=380.0,
         points=[(4300, -8700), (6300, -9300), (7900, -10400), (8700, -10900)]),
    dict(op="wash", name="well_track", half_width=250.0, depth=10.0, points=[
        (15600, 4000), (16300, 9900), (12900, 12800), (9000, 15300), (6000, 17600)]),
    dict(op="wash", name="well_drain", half_width=300.0, depth=45.0, points=[
        (16300, 9900), (19000, 12000), (21500, 11000), (23000, 8800)]),
    dict(op="well", at=(16300, 9900), radius=420.0),
    dict(op="well", at=(12900, 12800), radius=340.0),
    dict(op="well", at=(9000, 15300), radius=380.0),
    dict(op="well", at=(15600, 4000), radius=300.0),
])

LANDFORMS = [DUNE_RIDGE, SALT_FLATS, SCORPION_BASIN, RED_MESA, DRY_WELLS]

# The land between the starts, composited before them. One eroded formation striking WSW-ENE: the old table in the middle,
# its tail and broken remnants, all the same rock. Fairness comes from distances and routes, not symmetry. Water from the
# Dry Wells side drains into the low point east of the table: the oasis. The salt pan is its dried-out cousin.
OASIS_CENTRE = (15500.0, -3600.0)
OASIS_INFLOWS = [(20500.0, -4800.0), (17000.0, -500.0)]  # where the washes arrive
WORLD = dict(name="Between the starts", window=47000.0, centre=(0.0, 0.0), ops=[
    dict(op="wash", name="wash_south", half_width=760.0, depth=150.0, points=[
        (51000, -17500), (43500, -12000), (37000, -13500), (30500, -9000), (26000, -7300), (23000, -5900), (21000, -5200)]),
    dict(op="wash", name="wash_north", half_width=620.0, depth=120.0, points=[
        (24500, 29000), (21500, 21000), (25000, 12500), (21500, 6000), (18800, 2400), (17000, -500)]),
    dict(op="basin", name="oasis", z=250.0, water_z=372.0, slope=1700.0, rim=3000.0, fray=1.6, outline=[
        (10000, -4600), (11800, -7200), (14500, -6300), (17800, -7700), (20800, -6000), (20200, -3800), (21500, -1900), (18300, -1800),
        (17200, 500), (15000, -1200), (13300, -200), (11600, -2100)]),
    dict(op="pad", name="table", z=1720.0, outline=[
        (-9500, -4500), (-6000, -6500), (-1200, -4700), (2400, -5600), (6200, -2500), (5200, 800), (8600, 4200), (4200, 6200),
        (800, 4200), (-3000, 5600), (-7600, 3400), (-10600, -600)]),
    dict(op="ridge", name="table_tail", out_side=1,
         points=[(6000, 3000), (10500, 6300), (14300, 7600), (17400, 11000)], crest=[1620, 1400, 1180, 720],
         half_width=[520, 420, 360, 240]),
    dict(op="pad", name="table_shard", z=1480.0, outline=blob(-15500, -7500, 6200, 2800, 33.0, 5)),
    dict(op="ramp", name="table_north", z_from="ground", z_to=1720.0, half_width=800.0, side=560.0,
         points=[(-6000, 13000), (-5000, 10000), (-3600, 7400), (-2400, 4500), (-1800, 2600)]),
    dict(op="ramp", name="table_south", z_from="ground", z_to=1720.0, half_width=760.0, side=560.0,
         points=[(-1000, -16500), (1000, -13000), (1500, -10000), (800, -7000), (-200, -3500)]),
    dict(op="pad", name="remnant_nw", z=1560.0, outline=blob(-17500, 24000, 7600, 3000, 33.0, 11)),
    dict(op="pad", name="remnant_nw2", z=1120.0, outline=blob(-23200, 21000, 3200, 1900, 25.0, 12)),
    dict(op="pad", name="remnant_ne", z=1240.0, outline=blob(13500, 24800, 4800, 2400, 40.0, 13)),
    dict(op="pad", name="remnant_w", z=1680.0, outline=blob(-27500, -11000, 8200, 3200, 28.0, 14)),
    dict(op="pad", name="remnant_w2", z=1200.0, outline=blob(-21500, -8000, 3000, 1700, 38.0, 15)),
    dict(op="pad", name="remnant_e", z=1420.0, outline=blob(30000, -7500, 5400, 2500, 30.0, 16)),
])
OUTER_REMNANTS = [
    dict(name="outer", window=4500.0, centre=(cx, cy), ops=[dict(op="pad", name="outer", z=z, outline=blob(cx, cy, l, w, a, seed))])
    for seed, (cx, cy, z, l, w, a) in enumerate([
        (-27000, 62000, 1120, 4200, 1900, 30), (31000, 60000, 1000, 3000, 1500, 25), (66000, 34000, 1060, 3600, 1700, 35),
        (63000, -36000, 980, 2600, 1400, 20), (43000, -60000, 1200, 5000, 2000, 33), (-38000, -62000, 1040, 3400, 1800, 28),
        (-64000, -38000, 1000, 2800, 1300, 38), (-62000, 30000, 1100, 3800, 1700, 30)], 40)]

# Power lines: world corner points of long straight tangents. Poles stand exactly on the tangent, spans come from the
# ground (long over dips and washes, short before corners and on rises), corners are angle towers. Kinds pair their
# wires socket by socket: pole2 and pole3 mix, pole1 keeps a line to itself.
POWER_LINES = [
    dict(name="trunk", tangent_kind="pole3", corner_kind="pole2", corners=[
        (60500, 74000), (49000, 50500), (24500, 36000), (21300, 29800), (18500, 21500), (-3000, 17500), (-22000, 3500),
        (-29000, -6500), (-38500, -9000), (-55000, -14500), (-74000, -42000)]),
    dict(name="eastern", tangent_kind="pole3", corner_kind="pole2", branch_of="trunk", corners=[
        (18500, 21500), (24600, 9800), (27500, 3000), (28800, -600), (28500, -13000), (45000, -21000), (60000, -26500), (74000, -38000)]),
    dict(name="highline", tangent_kind="pole1", corner_kind="pole1", corners=[
        (-74000, 64000), (-48500, 49500), (-30000, 57500), (-12000, 74000)]),
]


# ----------------------------------------------------------------------------------------------------------------------
# Round 5 (owner: "the map is too large ... lets do the slide a little"). The five starts slide in from R 42 000 to
# START_RADIUS; the islands keep their stamps. A few outer arms are trimmed (pulled in beyond CORE_RADIUS by TRIMS) so the
# islands keep a 20 m gap, the oasis basin and shore stay clear and unchanged, and the table keeps its room.
# ----------------------------------------------------------------------------------------------------------------------
START_RADIUS = 40000.0
START_FIRST_ANGLE_DEG = 90.0
CORE_RADIUS = 14600.0
TRIMS = [
    # Factor on the part beyond CORE_RADIUS, per op name; "*" applies to unnamed ops (wells). Ops that build on each other
    # (a platform on a ridge, a road onto a bench) share one factor so they still meet.
    {"spine_west": 0.6, "spine_east": 0.6},                                    # Dune Ridge: spine ends
    {"spit": 0.6, "spit_top": 0.6, "crest_spit": 0.6, "salt_pan": 0.75, "north_shelf": 0.9},  # Salt Flats: spit, pan tip, shelf
    {"claw_spur": 0.45, "claw_left": 0.8, "sting_left": 0.8, "sting_left_road": 0.8,
     "pincer_right": 0.8, "sting_right": 0.8, "sting_right_road": 0.8},        # Scorpion Basin: claws and stings
    {"butte_east": 0.7, "bench": 0.85, "bench_road": 0.85, "wall_road": 0.85, "bench_knoll": 0.85, "bench_knoll_top": 0.85,
     "bench_knoll_road": 0.85},                                                # Red Mesa: east butte, the bench on the oasis side
    {"back_wall": 0.7, "wash_ford": 0.85, "low_terrace": 0.85, "wells_bench": 0.85, "bench_to_main": 0.85, "ford_knoll": 0.85,
     "ford_knoll_top": 0.85, "ford_knoll_road": 0.85, "well_track": 0.85, "well_drain": 0.85, "*": 0.85},  # Dry Wells: wall end, ford side
]
ORIGINAL_LANDFORMS = LANDFORMS


def squeeze(point, factor):
    """Pulls a local point in towards the deploy centre: the part beyond CORE_RADIUS is scaled by factor."""
    x, y = point[0], point[1]
    r = math.hypot(x, y)
    if r <= CORE_RADIUS or factor == 1.0:
        return (x, y) + tuple(point[2:])
    s = (CORE_RADIUS + (r - CORE_RADIUS) * factor) / r
    return (x * s, y * s) + tuple(point[2:])


def trim_landform(spec, trims):
    ops = []
    for op in spec["ops"]:
        op = dict(op)
        f = trims.get(op["name"], 1.0) if "name" in op else trims.get("*", 1.0)
        for key in ("points", "outline"):
            if key in op:
                op[key] = [squeeze(p, f) for p in op[key]]
        if "at" in op:
            op["at"] = squeeze(op["at"], f)
        if "dip" in op:
            a, b, z = op["dip"]
            op["dip"] = (squeeze(a, f), squeeze(b, f), z)
        ops.append(op)
    out = dict(spec)
    out["ops"] = ops
    if "no_fields" in spec:  # the start content planner's no-field discs follow the salt pan
        f = trims.get("salt_pan", 1.0)
        out["no_fields"] = [squeeze(p, f) for p in spec["no_fields"]]
    return out


LANDFORMS = [trim_landform(spec, t) for spec, t in zip(ORIGINAL_LANDFORMS, TRIMS)]

# Round 5 crop: the outer remnants fall outside the new map; the power lines keep their shape at the new scale.
OUTER_REMNANTS = []
# Corners stay where they were (the islands slid away from them); only the far ends are pulled onto the new ground.
POWER_LINES = [dict(line, corners=[(max(-73500.0, min(73500.0, x)), max(-73500.0, min(73500.0, y))) for x, y in line["corners"]])
               for line in POWER_LINES]

# Round 6: corridors widened through the narrow gaps between islands (centre lines follow the existing low passage,
# floor heights sampled from it). survival_frontier.apply_corridors sets the floor within 15 m and only lowers beyond.
GAP_CORRIDORS = {
    "S2-S3": [(-998, -19306, 496), (-1798, -20106, 585), (-2298, -20906, 639), (-2498, -21706, 686), (-2798, -22506, 790), (-3198, -23306, 861), (-3698, -24106, 906), (-3998, -24906, 905), (-3998, -25706, 948), (-3998, -26506, 889), (-3898, -27306, 819), (-3898, -28106, 760), (-3898, -28906, 642), (-3898, -29706, 541), (-4498, -30506, 625), (-4498, -31306, 705), (-4498, -32106, 715), (-4498, -32906, 724), (-4498, -33706, 722), (-4498, -34506, 715), (-4298, -35306, 715), (-3798, -36106, 720), (-3598, -36606, 722)],
    "S3-S4": [(22098, -6684, 368), (22898, -6184, 301), (23698, -6184, 328), (24498, -5984, 320), (25298, -5884, 370), (26098, -5884, 459), (26898, -5884, 486), (27698, -5884, 591), (28498, -5484, 771), (29298, -4784, 837), (30098, -4584, 834), (30898, -4584, 829), (31698, -4584, 809), (32498, -4984, 796), (33298, -5784, 799), (34098, -6584, 775), (34898, -7384, 726), (35698, -8184, 674), (36498, -8984, 677), (37298, -9784, 759), (38098, -10584, 770), (38898, -11384, 755), (39398, -12184, 700), (40098, -12984, 652), (40498, -13784, 726), (40498, -14584, 713), (40498, -14584, 713)],
    "S4-S0": [(15652, 19305, 563), (16452, 19605, 533), (17252, 19805, 533), (18052, 20505, 497), (18852, 21305, 462), (19652, 22105, 466), (20452, 22905, 501), (20752, 23705, 616), (20452, 24505, 697), (20452, 25305, 730), (20452, 26105, 766), (20452, 26905, 689), (20852, 27705, 673), (21652, 28505, 673), (22452, 29305, 732), (23252, 29505, 771), (24052, 29905, 777), (24752, 30605, 773)],
}

# Round 6/7: route centre lines (outer: nearest edge zone to an outward gate foot; inner: outer lane through a gap to an
# inward gate foot). The map carries a two-track spline on each; the generator paints a worn band under it.
ROUTE_LINES = {
    "S0_outer": [(-53000, 67000), (-51400, 66800), (-49800, 66400), (-48200, 66400), (-46600, 66000), (-45000, 65800), (-43400, 65600), (-41800, 65600), (-40400, 65000), (-38800, 64800), (-37200, 64800), (-35600, 64800), (-34000, 64800), (-32400, 64600), (-30800, 64600), (-29200, 64600), (-27600, 64400), (-26000, 64400), (-24400, 64000), (-22800, 64000), (-21200, 64000), (-19600, 64000), (-18000, 64000), (-16400, 64000), (-14800, 64000), (-13200, 64400), (-11600, 65000), (-10200, 65600), (-8600, 65600), (-8000, 65600)],
    "S1_outer": [(-65000, -29000), (-65000, -27400), (-65000, -25800), (-63800, -24600), (-62600, -23400), (-61400, -22200), (-60200, -21000), (-59000, -19800), (-57800, -18800), (-56600, -17600), (-55400, -16400), (-54200, -15400), (-52800, -14600), (-51600, -13600), (-50400, -12400), (-49200, -11400), (-49400, -11200)],
    "S2_outer": [(-31000, -60000), (-32400, -59200), (-33600, -58200), (-35000, -57600), (-36200, -56600), (-37400, -55400), (-38600, -54400), (-39800, -53200), (-41000, -52000), (-42200, -50800), (-43400, -49800), (-44600, -48600), (-45800, -47400), (-46400, -45800), (-46400, -44200), (-46400, -43600)],
    "S3_outer": [(55000, -60000), (55000, -58400), (55000, -56800), (55000, -55200), (55000, -53600), (55000, -52000), (54400, -50600), (54000, -49000), (53800, -47400), (53400, -45800), (53000, -44200), (53000, -42600), (53000, -41000), (53000, -39400), (52600, -37800), (52200, -36200), (51800, -34600), (51200, -33800)],
    "S4_outer": [(62000, 44000), (60400, 43800), (58800, 43800), (57200, 43800), (55600, 43800), (54000, 43800), (52400, 43800), (51000, 43000), (49400, 42600), (47800, 42200), (46200, 42200), (44800, 41600), (43600, 40400), (42200, 39800), (40600, 39600), (39400, 38400), (38000, 37800), (36600, 37000), (35000, 36800), (33400, 36800), (31800, 36200), (31800, 36200)],
    "S0_inner": [(26200, 34800), (25000, 33800), (23800, 32600), (23200, 31200), (22200, 30000), (21000, 28800), (20200, 27400), (20200, 25800), (20200, 24200), (19400, 22800), (18200, 21800), (17000, 20800), (15400, 20800), (13800, 21000), (12200, 21200), (10600, 21000), (9000, 21000), (7400, 21000), (6600, 20400)],
    "S1_inner": [(-28200, 41800), (-27000, 40600), (-26000, 39400), (-24800, 38200), (-23800, 37000), (-23200, 35400), (-22600, 33800), (-21400, 32600), (-20200, 31400), (-19000, 30200), (-17800, 29000), (-16600, 27800), (-15000, 27200), (-13400, 26600), (-12200, 25600), (-10600, 25600), (-9000, 25400), (-7600, 24800), (-6600, 23600), (-7600, 22400), (-9200, 21800), (-10400, 20600), (-12000, 20200), (-13600, 20000), (-15000, 19400), (-16600, 19400), (-18200, 19400), (-19600, 18400), (-19800, 16800), (-20000, 15200), (-20000, 13800)],
    "S2_inner": [(-38600, -16000), (-37800, -14600), (-36600, -13400), (-35200, -12600), (-33800, -11800), (-32200, -11800), (-32200, -10200), (-30800, -9200), (-29600, -8200), (-28200, -7400), (-26600, -6800), (-25000, -6600), (-23600, -6000), (-22000, -5800), (-20800, -4800), (-19600, -3800), (-18000, -3800), (-16400, -3800), (-14800, -3800), (-13400, -4400), (-12200, -5400), (-11000, -6400), (-9800, -7400), (-8600, -8600), (-7000, -9000), (-5400, -9000), (-3800, -9200), (-2400, -9600)],
    "S3_inner": [(-4800, -34400), (-4000, -33000), (-4000, -31400), (-4000, -29800), (-4000, -28200), (-4000, -26600), (-3600, -25000), (-2600, -23800), (-1600, -22600), (-1000, -21200), (0, -19800), (800, -18400), (1800, -17200), (3000, -16000), (4200, -14800), (5400, -13600), (6600, -12400), (7800, -11200), (9000, -10000), (10200, -9000), (11800, -9000), (12200, -9000)],
    "S4_inner": [(41200, -11600), (39800, -10800), (38200, -10600), (36600, -10600), (35000, -10000), (33600, -9000), (32600, -7800), (31800, -6400), (30600, -5200), (29200, -4400), (28000, -3200), (26600, -2600), (25200, -1800), (23800, -1000), (22400, -400), (21200, 800), (19600, 1000), (18400, 2200), (17400, 2800)],
}
