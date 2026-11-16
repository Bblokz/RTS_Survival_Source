"""The shots of the 1976 intro cinematic, section by section, as data for intro_shots.build_film.

Source: Source/Docs/Campaign/IntroCinematicScript.md. The narration rows come from intro_narration.py.
Rules checked against the doc's "may and may not learn" table while writing this:
- the Purple Death is only shown on people and places (trench, bog, map): no crops, farms or silos anywhere;
- strikes are scattered (SVAROG_STRIKES), never a line; the map stain is cut before it nears the Urals and the Ural
  Shield is shown as the real wall, never as a line on the map;
- sections 5 (purple) and 8 (orange) share no shot, set or transition; no crystal stands near a rocket;
- no leader's face or name in section 7.

Fields of a shot: id, levels, vo (narration rows), pre/post/gap/min (seconds), cut_vo (fraction of the voice after
which the shot ends: the line runs on over the next shot), grade ('archive' or 'present'), fov, cam keys
(fraction, position, rotation or {'at': target}), cam_snap, actors, mpc, cues, fade_in/fade_out.
"""
import importlib

import intro_sets as sets

importlib.reload(sets)

import intro_sets2 as sets2

O = sets.ORIGINS
M = sets.M
TOP = sets.TABLE_TOP_Z


def at(name, x, y, z):
    return (O[name][0] + x, O[name][1] + y, O[name][2] + z)


def on_map(lat, lon, back=0.0, side=0.0, up=0.0):
    """A point over the map table: 'back' cm toward the officer (south), 'up' cm above the table top."""
    x, y = sets.map_local(lat, lon)
    return at("Intro_Bunker", x - back, y + side, TOP + up)


def card(line1, line2, at_seconds=0.6, dur=5.5):
    return {"type": "Card", "at": at_seconds, "dur": dur, "l1": line1, "l2": line2}


def stamp(word, pos, angle, after_vo=0, at_seconds=-0.6, dur=2.8):
    return {"type": "Stamp", "after_vo": after_vo, "at": at_seconds, "dur": dur, "l1": word, "pos": pos, "angle": angle}


def clock(seconds):
    """Burn-ring clock: runs in real seconds over the shot."""
    return [(0.0, 0.0), (1.0, lambda duration: duration)]


SOLDIER_RUS = "SK_RussianSoldier"
SOLDIER_SCAV = "SK_PostApoSoldier"
T34 = "Rus_Vehicle_T3476_Soviet"
PANTHER = "Ger_Vehicle_PantherG"
V2 = "/Game/RTS_Survival/Meshes/Weapons/V2/SM_V2Rocket"
GREEN = "/Game/RTS_Survival/Cinematics/V2Scene/V2materials/MI_V2_RussianGreen"
CUBE, SPHERE, CYL = sets.CUBE, sets.SPHERE, sets.CYLINDER
NUKE = "NS_Nuke_1"
DUST = "NS_FallingDust_01"


def PD_MAT(mesh):
    """The Purple Death material instance prefix of a trench mould mesh (intro_purple.build_instances)."""
    return sets.M + "MI_Intro_PD_" + mesh.replace("SM_Intro_Mould_", "")


MOULD_STAGES = 44


def mould_actors():
    """Every growth stage of every trench mould mesh (SM_..._S<n>); each stage's instance shows it only in its window
    of the MPC Mould clock (intro_purple.STAGE_LO), so the growth needs no reveal mask."""
    return [{"kind": "mesh", "asset": "%s_S%d" % (mesh, stage), "label": "Growth_%d_S%d" % (index, stage),
             "mats": "%s_S%d" % (PD_MAT(mesh), stage), "keys": [(0.0, at("Intro_Trench", *offset), (0, 0, 0))]}
            for index, (mesh, offset) in enumerate(sets2.TRENCH_MOULD) for stage in range(MOULD_STAGES)]

NOTICE = ("ARCHIVAL NOTICE\\n\\nThirty years of static, ash and, well, the nukes,\\nleft huge blackouts in our history."
          "\\n\\nWhat follows is compiled from the limited data available.")

# Old intro map sets (streamed level RTS_IntroCinematic).
OLD = "RTS_IntroCinematic"
PAD = (-9000.0, -14000.0)         # open concrete away from the old film's own rockets
OLD_Z = 150.0                     # ground height of the old map's plain (its ground meshes do not answer traces)
RUS_PAD = (-38800.0, -11800.0)
HANGAR_Y = 27600.0

# Ger_2ndOreMountains.
ORE = "Ger_2ndOreMountains"
WALL_X = -34000.0

# SM_Desert_01.
DESERT = "SM_Desert_01"
CRATER = (-1200.0, 900.0)
OUTPOST = (1500.0, 25000.0)
DUNE = (-25000.0, 0.0)


def rocket_rise(x, y, base, heights, start=0.25):
    """Keys of a rocket standing, then climbing: heights at even steps after 'start'."""
    keys = [(0.0, (x, y, base), (0, 0, 0)), (start, (x, y, base), (0, 0, 0))]
    step = (1.0 - start) / len(heights)
    for index, height in enumerate(heights):
        keys.append((start + step * (index + 1), (x, y, base + height), (0, 0, 0)))
    return keys


def column(start, end, count, spacing, yaw, lateral=0.0):
    """Tanks in line ahead driving from start to end (x, y); returns actor specs snapped to the ground."""
    import math
    dx, dy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    specs = []
    for index in range(count):
        back = index * spacing
        side = lateral * (index % 2)
        a = (start[0] - dx * back - dy * side, start[1] - dy * back + dx * side, 0.0)
        e = (end[0] - dx * back - dy * side, end[1] - dy * back + dx * side, 0.0)
        steps = 12
        specs.append({"kind": "class", "asset": T34, "label": "T34_%d" % index, "snap": 5.0,
                      "keys": [(k / float(steps), (a[0] + (e[0] - a[0]) * k / steps, a[1] + (e[1] - a[1]) * k / steps, 0.0), (0, yaw, 0))
                               for k in range(steps + 1)]})
    return specs


# Ger_2ndOreMountains valley road (spline ORE_Road_Valley), sampled every 4 m: (distance along spline, x, y).
ORE_ROAD = [(44800, 290, -7036), (45200, 369, -7428), (45600, 442, -7822), (46000, 513, -8215), (46400, 585, -8609),
            (46800, 662, -9001), (47200, 748, -9392), (47600, 842, -9781), (48000, 940, -10168), (48400, 1040, -10556),
            (48800, 1140, -10943), (49200, 1237, -11331), (49600, 1328, -11721), (50000, 1411, -12112),
            (50400, 1486, -12505), (50800, 1557, -12898), (51200, 1630, -13292), (51600, 1709, -13684),
            (52000, 1800, -14073), (52400, 1908, -14458), (52800, 2039, -14836), (53200, 2184, -15209),
            (53600, 2332, -15581), (54000, 2472, -15956), (54400, 2595, -16336), (54800, 2686, -16725),
            (55200, 2736, -17122), (55600, 2749, -17522), (56000, 2738, -17922), (56400, 2719, -18321),
            (56800, 2703, -18721)]


def road_at(distance, lateral=0.0):
    """Point on the valley road and the heading toward lower distances (the column drives toward the arches);
    lateral cm to the right of that heading."""
    import math
    for a, c in zip(ORE_ROAD[:-1], ORE_ROAD[1:]):
        if a[0] <= distance <= c[0]:
            f = (distance - a[0]) / float(c[0] - a[0])
            x, y = a[1] + (c[1] - a[1]) * f, a[2] + (c[2] - a[2]) * f
            yaw = math.degrees(math.atan2(a[2] - c[2], a[1] - c[1]))
            r = math.radians(yaw)
            return (x - math.sin(r) * lateral, y + math.cos(r) * lateral, 0.0), yaw
    raise ValueError("distance off the sampled road: %s" % distance)


def road_column(tail_start, travel, count, spacing, lateral, drive):
    """Tanks in line ahead on the valley road, driving away from the camera; keys every eighth of the shot follow
    the road's curve. Seated on the ground under their tracks (snap) with turning tracks (drive)."""
    specs = []
    for index in range(count):
        start = tail_start - (count - 1 - index) * spacing
        keys = []
        for k in range(9):
            position, yaw = road_at(start - travel * k / 8.0, lateral)
            keys.append((k / 8.0, position, (0, yaw, 0)))
        specs.append({"kind": "class", "asset": T34, "label": "T34_%d" % index, "snap": 0.0, "drive": drive, "keys": keys})
    return specs


US_SITES = ("LosAlamos", "OakRidge", "Hanford")


def tags_curl(curl_at):
    """The paper tags on the US pins curl once the burn has taken them: the kit's curled tag spawns where the flat
    tag lies, for the whole shot; the two materials hand over on the same burn clock (MI_Intro_Tag_* VanishAt 0.545,
    MI_Intro_TagCurled_* -0.545). curl_at: per site, fraction of the shot (>= 1: not curled within it, left out)."""
    actors = []
    for index, site in enumerate(US_SITES):
        f = curl_at[index]
        if f >= 1.0:
            continue
        x, y = sets.map_local(*sets.PLACES[site])
        where = at("Intro_Bunker", x - 9.0, y + 3.0, sets.TABLE_TOP_Z + 0.4 + 0.5 + 0.5)
        actors.append({"kind": "mesh", "asset": "SM_Intro_PaperTag_Curled", "label": "TagCurled_" + site,
                       "mats": sets.M + "MI_Intro_TagCurled_" + site, "shadow": False,
                       "spawn": (0.0, 1.0), "keys": [(0.0, where, (0, 96 + index * 5, 0), 1.8)]})
    return actors


def shots():
    germany = (51.0, 10.5)
    europe = (51.5, 20.0)
    usa = (40.5, -101.0)
    return [
        # ---- Section 0: cold open ---------------------------------------------------------------------------------
        {"id": "0.1", "levels": ["Intro_Bunker"], "grade": "present", "min": 10.4, "fov": 50,
         "cam": [(0.0, at("Intro_Bunker", -330, -290, 170), (-17, 40, 0)), (1.0, at("Intro_Bunker", -322, -282, 169), (-17, 40, 0))],
         "cues": [{"type": "Notice", "at": 1.2, "dur": 9.2, "param": 2.4, "l1": NOTICE}]},
        # ---- Section 1: the briefing (present grade) ---------------------------------------------------------------
        {"id": "1.1", "levels": ["Intro_Bunker"], "grade": "present", "vo": ["1_1"], "pre": 2.4, "post": 1.2, "fov": 50,
         "cam": [(0.0, at("Intro_Bunker", -322, -282, 169), (-17, 40, 0)), (1.0, at("Intro_Bunker", -205, -205, 142), (-19, 58, 0))],
         "cues": [card("1976", "RESISTANCE COMMAND BUNKER", 0.5, 6.0)]},
        {"id": "1.2", "levels": ["Intro_Bunker"], "grade": "present", "vo": ["1_2"], "pre": 0.7, "post": 1.6, "fov": 55,
         "cam": [(0.0, at("Intro_Bunker", -150, -40, 215), (-52, 0, 0)), (0.55, at("Intro_Bunker", -110, -120, 175), (-68, 0, 0)),
                 (1.0, at("Intro_Bunker", -74, -148, 150), (-82, 0, 0))],
         "mpc": {"Archive": [(0.0, 0.0), (0.68, 0.0), (1.0, 1.0)]}},
        # ---- Section 2: October 1944, the stolen blueprints --------------------------------------------------------
        {"id": "2.1", "levels": [OLD], "vo": ["2_1"], "pre": 0.8, "post": 0.8, "fov": 45,
         "cam": [(0.0, (PAD[0] - 3300, PAD[1] + 1500, OLD_Z + 190), {"at": (PAD[0], PAD[1], OLD_Z + 1400)}),
                 (0.45, (PAD[0] - 3300, PAD[1] + 1500, OLD_Z + 190), {"at": (PAD[0], PAD[1], OLD_Z + 2600)}),
                 (1.0, (PAD[0] - 3300, PAD[1] + 1500, OLD_Z + 190), {"at": (PAD[0], PAD[1], OLD_Z + 15500)})],
         "actors": [
             {"kind": "mesh", "asset": CUBE, "label": "LaunchTable", "mats": M + "MI_Intro_Steel",
              "keys": [(0.0, (PAD[0], PAD[1], OLD_Z + 40), (0, 0, 0), (4.2, 4.2, 0.8))]},
             {"kind": "mesh", "asset": V2, "label": "V2",
              "keys": rocket_rise(PAD[0], PAD[1], OLD_Z + 150, [350, 1500, 3800, 7600, 13000, 20000], 0.3)},
             {"kind": "niagara", "asset": "NS_RTS_Smoking_V2", "label": "PadSmoke",
              "keys": [(0.0, (PAD[0], PAD[1], OLD_Z + 60), (0, 0, 0), 2.5)]},
             {"kind": "point", "label": "EngineGlow", "intensity": 5000.0, "color": (1.0, 0.6, 0.25, 1.0), "radius": 9000.0,
              "spawn": (0.27, 1.0),
              "keys": rocket_rise(PAD[0], PAD[1], OLD_Z + 60, [350, 1500, 3800, 7600, 13000, 20000], 0.3)},
             {"kind": "niagara", "asset": "NS_Rocket_Engine_1", "label": "Flame", "spawn": (0.27, 1.0), "_scale": (2.2, 2.2, 2.2),
              "keys": [(k[0], k[1], (90, 0, 0)) for k in rocket_rise(PAD[0], PAD[1], OLD_Z + 150, [350, 1500, 3800, 7600, 13000, 20000], 0.3)]},
             {"kind": "niagara", "asset": "NS_SmokePillar1", "label": "PadCloud", "spawn": (0.27, 1.0),
              "keys": [(0.0, (PAD[0], PAD[1], OLD_Z + 40), (0, 0, 0), 5.0)]}]},
        {"id": "2.2", "levels": ["Intro_Draft"], "vo": ["2_2"], "pre": 1.6, "post": 2.4, "fov": 50,
         "cam": [(0.0, at("Intro_Draft", 132, 30, 300), {"at": at("Intro_Draft", 218, -2, 98)}),
                 (1.0, at("Intro_Draft", 146, 22, 282), {"at": at("Intro_Draft", 218, -6, 98)})],
         "actors": [
             # Paper bounce: the torch itself never lights the man who holds it.
             {"kind": "point", "label": "PaperBounce", "intensity": 0.45, "color": (1.0, 0.9, 0.74, 1.0), "radius": 160.0,
              "keys": [(0.0, at("Intro_Draft", 196, -62, 118), (0, 0, 0), 1.0)]},
             {"kind": "spot", "label": "Torch", "intensity": 160.0, "color": (1.0, 0.92, 0.78, 1.0), "radius": 900.0,
              "cone": 17.0, "shadows": True,
              "keys": [(0.0, at("Intro_Draft", 130, -80, 170), {"at": at("Intro_Draft", 214, -55, 98)}),
                       (0.5, at("Intro_Draft", 132, -70, 170), {"at": at("Intro_Draft", 214, 40, 98)}),
                       (1.0, at("Intro_Draft", 134, -64, 170), {"at": at("Intro_Draft", 214, 0, 98)})]}],
         "mpc": {"Flash": [(0.0, 0.0), (0.40, 0.0), (0.41, 0.55), (0.43, 0.0), (0.60, 0.0), (0.61, 0.55), (0.63, 0.0),
                           (0.78, 0.0), (0.79, 0.55), (0.81, 0.0)]},
         "cues": [card("OCTOBER 1944", "HEERESVERSUCHSANSTALT PEENEMÜNDE", 0.4, 5.6),
                  stamp("CONFIRMED", (0.66, 0.60), -8.0, 0, 0.3, 2.6)]},
        {"id": "2.3", "levels": [OLD], "vo": ["2_3"], "pre": 0.7, "post": 1.3, "fov": 60,
         "cam": [(0.0, (-2541, HANGAR_Y - 2700, 360), (3, 90, 0)), (1.0, (-2541, HANGAR_Y - 1500, 330), (4, 90, 0))]},
        # ---- Section 3: January 1945, Berlin ----------------------------------------------------------------------
        {"id": "3.1", "handheld": 0.12, "levels": [OLD], "vo": ["3_1"], "pre": 0.6, "post": 0.6, "fov": 50,
         "cam": [(0.0, (-34700, -9300, 640), {"at": (RUS_PAD[0], RUS_PAD[1], 1700)}),
                 (0.5, (-34700, -9300, 640), {"at": (RUS_PAD[0], RUS_PAD[1], 2400)}),
                 (1.0, (-34700, -9300, 640), {"at": (RUS_PAD[0], RUS_PAD[1], 11000)})],
         "actors": [
             {"kind": "mesh", "asset": V2, "label": "SovietRocket", "mats": GREEN,
              "keys": rocket_rise(RUS_PAD[0], RUS_PAD[1], 640, [250, 1100, 2900, 5800, 9800, 15000], 0.4)},
             {"kind": "point", "label": "EngineGlow", "intensity": 5000.0, "color": (1.0, 0.6, 0.25, 1.0), "radius": 9000.0,
              "spawn": (0.38, 1.0),
              "keys": rocket_rise(RUS_PAD[0], RUS_PAD[1], 560, [250, 1100, 2900, 5800, 9800, 15000], 0.4)},
             {"kind": "niagara", "asset": "NS_Rocket_Engine_1", "label": "Flame", "spawn": (0.38, 1.0), "_scale": (2.2, 2.2, 2.2),
              "keys": [(k[0], k[1], (90, 0, 0)) for k in rocket_rise(RUS_PAD[0], RUS_PAD[1], 640, [250, 1100, 2900, 5800, 9800, 15000], 0.4)]},
             {"kind": "niagara", "asset": "NS_SmokePillar1", "label": "PadCloud", "spawn": (0.38, 1.0),
              "keys": [(0.0, (RUS_PAD[0], RUS_PAD[1], 560), (0, 0, 0), 5.0)]},
             {"kind": "skel", "asset": SOLDIER_RUS, "label": "Crew_A", "anim": "Anim_TW_StandingIdle",
              "keys": [(0.0, (-35250, -9420, 556), (0, 215, 0))]},
             {"kind": "skel", "asset": SOLDIER_RUS, "label": "Crew_B", "anim": "AN_Infantry_SpotHowitzer_Standing",
              "keys": [(0.0, (-35330, -9650, 556), (0, 205, 0))]}]},
        {"id": "3.2", "levels": ["Intro_Space"], "vo": ["3_2"], "pre": 0.5, "post": 0.9, "fov": 42,
         "cam": [(0.0, at("Intro_Space", -3100, 1500, 2500), {"at": at("Intro_Space", 100, 0, 1500)}),
                 (1.0, at("Intro_Space", -2300, 2100, 3300), {"at": at("Intro_Space", 100, 0, 1750)})],
         "actors": [
             {"kind": "mesh", "asset": V2, "label": "Warhead", "mats": GREEN,
              "keys": [(0.0, at("Intro_Space", 0, 0, 0), (12, 0, 0)), (1.0, at("Intro_Space", 0, 0, 60), (12, 26, 0))]},
             {"kind": "point", "label": "RimLight", "intensity": 60.0, "color": (0.6, 0.75, 1.0, 1.0), "radius": 3000.0,
              "keys": [(0.0, at("Intro_Space", -700, 900, 2600), (0, 0, 0))]},
             # Hard sunlight on the hull: the planet is so bright that the sun alone leaves the rocket a black bar.
             {"kind": "point", "label": "HullKey", "intensity": 900.0, "color": (1.0, 0.95, 0.86, 1.0), "radius": 6000.0,
              "keys": [(0.0, at("Intro_Space", -1500, 1500, 1900), (0, 0, 0))]}]},
        {"id": "3.3", "levels": ["Intro_Skyline"], "min": 6.0, "fov": 28,
         "cam": [(0.0, at("Intro_Skyline", -5200, 620, 400), (2.4, 0, 0)), (1.0, at("Intro_Skyline", 600, 500, 430), (2.5, 0, 0))],
         "cues": [card("JANUARY 1945", "BERLIN", 0.8, 4.6)]},
        {"id": "3.4a", "levels": ["Intro_Skyline"], "min": 1.4, "fov": 28,
         "cam": [(0.0, at("Intro_Skyline", 600, 500, 430), (2.5, 0, 0)), (1.0, at("Intro_Skyline", 1950, 472, 437), (2.6, 0, 0))],
         "mpc": {"Flash": [(0.0, 0.0), (0.3, 0.0), (0.42, 1.0), (1.0, 1.0)]}},
        {"id": "3.4b", "levels": ["Intro_SkylineRuin"], "min": 8.0, "fov": 28,
         "cam": [(0.0, at("Intro_Skyline", 1950, 472, 437), (2.6, 0, 0)), (0.45, at("Intro_Skyline", 2300, 300, 470), (4.4, -3.0, 0)),
                 (1.0, at("Intro_Skyline", 2500, 100, 520), (7.4, -5.5, 0))],
         "actors": [
             {"kind": "mesh", "asset": SPHERE, "label": "Cloud_%d" % index, "mats": M + "MI_Intro_Silhouette",
              "keys": [(0.0, at("Intro_Skyline", 50000, -8200 + dy * 0.6, z0), (0, 0, 0), (sx * 0.6, sx * 0.6, sz * 0.6)),
                       (1.0, at("Intro_Skyline", 50000, -8200 + dy, z1), (0, 0, 0), (sx, sx, sz))]}
             for index, (dy, z0, z1, sx, sz) in enumerate((
                 # cap: a broad core and lumps round its edge and top
                 (0, 4200, 7000, 84.0, 30.0), (-3300, 3900, 6500, 44.0, 24.0), (3300, 3900, 6500, 44.0, 24.0),
                 (-1700, 4700, 7900, 46.0, 26.0), (1600, 4800, 8100, 42.0, 24.0), (0, 5100, 8700, 36.0, 22.0),
                 (-4600, 3600, 5900, 26.0, 16.0), (4500, 3600, 5900, 26.0, 16.0),
                 # skirt under the cap, stem in three swelling parts, the dust foot
                 (0, 3000, 5100, 40.0, 9.0), (0, 2300, 4000, 20.0, 22.0), (300, 1500, 2600, 23.0, 24.0), (-200, 700, 1200, 28.0, 22.0),
                 (0, 0, 0, 110.0, 13.0), (-5200, 0, 0, 56.0, 9.0), (5200, 0, 0, 56.0, 9.0)))],
         "mpc": {"Flash": [(0.0, 1.0), (0.12, 1.0), (0.42, 0.0)]}},
        {"id": "3.5", "levels": ["Ger_Berlin", "Intro_BerlinSky"], "vo": ["3_5"], "pre": 1.6, "post": 2.2, "fov": 52,
         "cam": [(0.0, (72, -5200, 250), (5, -90, 0)), (1.0, (72, -5950, 235), (6, -90, 0))],
         "actors": [{"kind": "niagara", "asset": "P_Rain_Converted", "label": "Sleet",
                     "keys": [(0.0, (72, -5900, 700), (0, 0, 0), 2.0), (1.0, (72, -6650, 700), (0, 0, 0), 2.0)]}],
         # Berlin has no purple (doc): the grade drains it here (a scrap of the map's street litter read as purple).
         "mpc": {"Rain": [(0.0, 0.7)], "PurpleKeep": [(0.0, 0.0)]},
         "cues": [stamp("CONFIRMED", (0.80, 0.30), -7.0, 0, 0.2, 2.4)]},
        {"id": "3.6", "levels": ["Intro_Bunker"], "vo": ["3_6"], "pre": 1.0, "min": 8.2, "fov": 38,
         "cam": [(0.0, on_map(*germany, back=27, up=30), (-48, 0, 0)), (1.0, on_map(*germany, back=23, up=26), (-49, 0, 0))],
         "mpc": {"Rings36": clock(1.0),
                 "Darken": [(0.0, 0.0), (0.16, 0.0), (0.18, 0.5), (0.22, 0.0), (0.34, 0.0), (0.36, 0.45), (0.40, 0.0),
                            (0.53, 0.0), (0.55, 0.5), (0.59, 0.0), (0.72, 0.0), (0.74, 0.4), (0.78, 0.0)]}},
        # ---- Section 4: May 1945, the Manhattan strike (map table) -------------------------------------------------
        {"id": "4.1", "levels": ["Intro_Bunker"], "vo": ["4_1"], "pre": 0.5, "post": 0.5, "fov": 50,
         "cam": [(0.0, on_map(52.0, 12.0, back=5, side=52, up=46), (-40, -90, 0)),
                 (1.0, on_map(44.0, -62.0, back=5, side=52, up=46), (-40, -90, 0))],
         # The German fires burn down behind the camera's first frames (their clock runs on from the end of 3.6).
         "mpc_run_on": ["Rings36"]},
        {"id": "4.2", "levels": ["Intro_Bunker"], "vo": ["4_2"], "pre": 0.5, "post": 1.6, "fov": 55,
         "cam": [(0.0, on_map(*usa, back=62, up=66), (-46, 0, 0)), (1.0, on_map(*usa, back=54, up=58), (-46, 0, 0))],
         "mpc": {"Rings42": clock(1.0), "Rings36": [(0.0, 60.0)], "Darken": [(0.0, 0.0), (0.12, 0.3), (1.0, 0.3)]},
         "actors": tags_curl((4.8 / 8.33, 6.5 / 8.33, 8.2 / 8.33)),
         "cues": [card("MAY 1945", "MANHATTAN PROJECT SITES, USA", 0.4, 5.6), stamp("CONFIRMED", (0.76, 0.70), -9.0, 0, -0.2, 2.6)]},
        {"id": "4.3", "levels": ["Intro_Bunker"], "vo": ["4_3"], "pre": 0.5, "post": 1.2, "fov": 85,
         "mpc_run_on": ["Rings42"],     # the US fires keep burning down through the wide shot
         "actors": tags_curl((0.0, 0.0, 0.0)),
         "cam": [(0.0, at("Intro_Bunker", -205, -10, 228), (-44, 0, 0)), (0.3, at("Intro_Bunker", -200, 0, 226), (-44, 0, 0)),
                 (1.0, on_map(50.2, 20.5, back=30, up=32), (-46, 0, 0))]},
        # ---- Section 5: August 1945, the Purple Death --------------------------------------------------------------
        {"id": "5.1", "levels": ["Intro_Clouds"], "vo": ["5_1"], "pre": 0.8, "post": 0.8, "fov": 55,
         "cam": [(0.0, at("Intro_Clouds", 0, 0, 0), (5, -3, 0)), (1.0, at("Intro_Clouds", 120, 30, -60), (-9, 4, 0))],
         "actors": [
             {"kind": "mesh", "asset": "SM_Intro_Canister", "fallback": True, "label": "Canister", "mats": None,
              "keys": [(0.0, at("Intro_Clouds", 560, -210, 330), (-52, 20, 0)), (1.0, at("Intro_Clouds", 500, 150, -250), (-70, 60, 150))]},
             # A cold key on the canister's flank, so the steel reads against the cloud.
             {"kind": "point", "label": "CanisterKey", "intensity": 22.0, "color": (0.75, 0.8, 1.0, 1.0), "radius": 900.0,
              "keys": [(0.0, at("Intro_Clouds", 330, -330, 400), (0, 0, 0), 1.0), (1.0, at("Intro_Clouds", 300, 20, -150), (0, 0, 0), 1.0)]}],
         "cues": [card("AUGUST 1945", "LOCATION UNKNOWN", 0.5, 5.4)]},
        {"id": "5.2", "handheld": 0.12, "levels": ["Intro_Trench"], "vo": ["5_2"], "pre": 0.8, "post": 1.0, "fov": 48,
         "cam": [(0.0, at("Intro_Trench", 14, 470, 132), (-9, -90, 0)), (1.0, at("Intro_Trench", 8, 330, 122), (-11, -91, 0))],
         "actors": [
             {"kind": "niagara", "asset": DUST, "label": "Dust", "keys": [(0.0, at("Intro_Trench", 0, -100, 330), (0, 0, 0), 1.2)]},
             {"kind": "point", "label": "PurpleHaze", "intensity": 0.35, "color": (0.62, 0.2, 0.95, 1.0), "radius": 1300.0,
              "keys": [(0.0, at("Intro_Trench", 0, -300, 210), (0, 0, 0))]}] +
             # The first of it: small, already there when we arrive (its first growth stage, MPC Mould).
             mould_actors(),
         "mpc": {"Mould": [(0.0, 0.14), (1.0, 0.26)]}},
        {"id": "5.3", "levels": ["Intro_Trench"], "vo": ["5_3"], "pre": 0.6, "post": 1.4, "fov": 55,
         "cam": [(0.0, at("Intro_Trench", -52, 96, 118), {"at": at("Intro_Trench", 100, -150, 62)}),
                 (1.0, at("Intro_Trench", -30, 30, 104), {"at": at("Intro_Trench", 100, -160, 66)})],
         "actors": mould_actors() + [
             {"kind": "point", "label": "PurpleHaze", "intensity": 0.7, "color": (0.62, 0.2, 0.95, 1.0), "radius": 1300.0,
              "keys": [(0.0, at("Intro_Trench", 0, -300, 210), (0, 0, 0))]},
             # A soft cold fill from the camera side: the low sun is behind the mound, and its camera-facing half fell to a
             # flat black blob whose folds and vessels could not read.
             {"kind": "point", "label": "MouldFill", "intensity": 4.0, "color": (0.72, 0.66, 0.86, 1.0), "radius": 650.0,
              "keys": [(0.0, at("Intro_Trench", -10, 20, 150), (0, 0, 0)), (1.0, at("Intro_Trench", 0, -40, 140), (0, 0, 0))]}],
         # Time-lapse: it takes the trench (the growth front crawls outward, the veins pump).
         # it keeps growing to the cut (a held last stretch read as the growth having stopped)
         "mpc": {"Mould": [(0.0, 0.26), (0.12, 0.31), (1.0, 1.0)]}},
        {"id": "5.4", "levels": ["Intro_Bunker"], "vo": ["5_4"], "pre": 0.6, "post": 0.2, "fov": 55,
         # framed west of centre: the stain's eastern edge stays out of frame (it must never read as where it stops)
         "cam": [(0.0, on_map(50.5, 7.5, back=70, up=80), (-50, 0, 0)), (1.0, on_map(50.5, 7.5, back=62, up=72), (-50, 0, 0))],
         "mpc": {"Stain": [(0.0, 0.02), (1.0, 0.9)], "StainOpacity": [(0.0, 1.0)], "Pencil": [(0.0, 0.0), (0.45, 0.0), (0.6, 1.0)],
                 "Rings36": [(0.0, 60.0)], "Rings42": [(0.0, 60.0)], "Rings62": [(0.0, 0.0)]}},
        {"id": "5.5", "levels": ["Ger_PripyatMe410"], "min": 6.5, "fov": 58, "cam_snap": 230.0,
         "cam": [(0.0, (28050, 24800, 0), (-6, 198, 0)), (1.0, (27650, 25400, 0), (-6, 205, 0))],
         "actors": [{"kind": "niagara", "asset": "P_Rain_Converted", "label": "Rain", "snap": 500.0,
                     "keys": [(0.0, (27500, 24650, 0), (0, 0, 0), 2.5), (1.0, (27100, 25250, 0), (0, 0, 0), 2.5)]}],
         "mpc": {"Stain": [(0.0, 0.82), (1.0, 1.6)], "Rain": [(0.0, 1.0)]}},
        # ---- Section 6: 1946-1947, Operation Svarog and the Ural Shield --------------------------------------------
        # Behind the column on the valley road, looking down it to the ruined arches; a wreck lies at the roadside.
        # The tanks follow the road's curve, sit on the ground under their tracks and their tracks turn.
        {"id": "6.1", "handheld": 0.22, "levels": [ORE], "vo": ["6_1"], "pre": 1.4, "min": 7.6, "fov": 40, "cam_snap": 165.0,
         # Pitch at most -4 at this lens: something high in the map's sky draws as a dark square above -4 degrees.
         "cam": [(0.0, (2790, -17560, 0), (-4, 101, 0)), (1.0, (2782, -17300, 0), (-4, 103, 0))],
         # (No falling-dust Niagara here: in this view it draws as a dark square over the sky.)
         "actors": road_column(54250, 3350, 4, 2000, 150.0, 1.5) + [
             # Artillery beyond the ridge: three distant flashes light the haze behind the arches.
             {"kind": "point", "label": "Artillery_%d" % k, "intensity": 60000.0, "color": (1.0, 0.72, 0.42, 1.0),
              "radius": 30000.0, "snap": 900.0, "spawn": (at_, at_ + 0.035), "keys": [(0.0, position, (0, 0, 0))]}
             for k, (at_, position) in enumerate(((0.27, (-2500, -4000, 0)), (0.53, (3500, -1500, 0)), (0.8, (-6000, -6500, 0))))],
         # Ash on the wind (drawn by the grade) and the flashes' glow on the whole frame.
         "mpc": {"Sand": [(0.0, 0.32)],
                 "Flash": [(0.0, 0.0), (0.27, 0.0), (0.28, 0.07), (0.31, 0.0), (0.53, 0.0), (0.54, 0.06), (0.57, 0.0),
                           (0.8, 0.0), (0.81, 0.08), (0.84, 0.0)]},
         "cues": [card("1946 - 1947", "OPERATION SVAROG", 0.5, 5.6)]},
        {"id": "6.2", "levels": [ORE], "vo": ["6_2"], "pre": 0.5, "post": 0.8, "fov": 62, "cam_snap": 620.0,
         "cam": [(0.0, (-11900, 2500, 0), (-4, -138, 0)), (0.3, (-11900, 2500, 0), (-4, -128, 0)), (0.45, (-11900, 2500, 0), (-3, -92, 0)),
                 (0.7, (-11900, 2500, 0), (-3, -84, 0)), (0.82, (-11900, 2500, 0), (-3, -58, 0)), (1.0, (-11900, 2500, 0), (-3, -54, 0))],
         "actors": [
             {"kind": "class", "asset": T34, "label": "T34_Halt_A", "snap": 5.0, "keys": [(0.0, (-12500, 1300, 0), (0, -80, 0))]},
             {"kind": "class", "asset": T34, "label": "T34_Halt_B", "snap": 5.0, "keys": [(0.0, (-11300, 1000, 0), (0, -105, 0))]},
             # Scattered on different horizons, different distances: no line.
             {"kind": "niagara", "asset": NUKE, "label": "Nuke_A", "spawn": (0.08, 1.0), "keys": [(0.0, (-52000, -42000, 300), (0, 0, 0), 7.0)]},
             {"kind": "niagara", "asset": NUKE, "label": "Nuke_B", "spawn": (0.36, 1.0), "keys": [(0.0, (-8000, -90000, 300), (0, 0, 0), 12.0)]},
             {"kind": "niagara", "asset": NUKE, "label": "Nuke_C", "spawn": (0.62, 1.0), "keys": [(0.0, (14000, -38000, 300), (0, 0, 0), 5.0)]}],
         "mpc": {"Flash": [(0.0, 0.0), (0.08, 0.0), (0.09, 0.4), (0.12, 0.0), (0.36, 0.0), (0.37, 0.3), (0.40, 0.0),
                           (0.62, 0.0), (0.63, 0.4), (0.66, 0.0)]}},
        {"id": "6.2b", "levels": ["Intro_Bunker"], "vo": ["6_2b"], "pre": 0.5, "post": 1.0, "fov": 55,
         "cam": [(0.0, on_map(50.5, 22.0, back=66, up=80), (-50, 0, 0)), (1.0, on_map(50.5, 23.0, back=58, up=72), (-50, 0, 0))],
         "mpc": {"Rings62": clock(1.0), "Rings36": [(0.0, 60.0)],
                 "Darken": [(0.0, 0.0), (0.2, 0.0), (0.22, 0.4), (0.26, 0.0), (0.5, 0.0), (0.52, 0.4), (0.56, 0.0)]}},
        {"id": "6.3", "levels": ["Intro_Ural"], "vo": ["6_3"], "pre": 2.2, "min": 12.5, "fov": 50,
         "cam": [(0.0, at("Intro_Ural", -9800, -1500, 190), {"at": at("Intro_Ural", -900, 0, 1300)}),
                 (0.66, at("Intro_Ural", -8300, -1000, 260), {"at": at("Intro_Ural", -900, 0, 1500)}),
                 (1.0, at("Intro_Ural", -8900, -1300, 1500), {"at": at("Intro_Ural", -900, 0, 1900)})],
         "actors": [
             {"kind": "class", "asset": T34, "label": "T34_%d" % index, "drive": 2.0, "snap": 0.0, "trace_from": 600.0,
              "contact": True,
              "keys": [(0.0, at("Intro_Ural", -7600 - index * 1200, 0, 0), (0, 0, 0)),
                       (0.62, at("Intro_Ural", 700 - index * 1200, 0, 0), (0, 0, 0)),
                       (1.0, at("Intro_Ural", 4200 - index * 1200, 0, 0), (0, 0, 0))]} for index in range(4)] + [
             # The iron leaves hang on their hinge pivots (kit README): closed at yaw 0, they swing outward (-X).
             {"kind": "mesh", "asset": "SM_UralShield_GateLeaf_L", "label": "Leaf_L",
              "keys": [(0.0, at("Intro_Ural", -500, -795, -40), (0, 0, 0)), (0.10, at("Intro_Ural", -500, -795, -40), (0, 0, 0)),
                       (0.30, at("Intro_Ural", -500, -795, -40), (0, 85, 0)), (0.72, at("Intro_Ural", -500, -795, -40), (0, 85, 0)),
                       (0.92, at("Intro_Ural", -500, -795, -40), (0, 0, 0)), (1.0, at("Intro_Ural", -500, -795, -40), (0, 0, 0))]},
             {"kind": "mesh", "asset": "SM_UralShield_GateLeaf_R", "label": "Leaf_R",
              "keys": [(0.0, at("Intro_Ural", -500, 795, -40), (0, 0, 0)), (0.10, at("Intro_Ural", -500, 795, -40), (0, 0, 0)),
                       (0.30, at("Intro_Ural", -500, 795, -40), (0, -85, 0)), (0.72, at("Intro_Ural", -500, 795, -40), (0, -85, 0)),
                       (0.92, at("Intro_Ural", -500, 795, -40), (0, 0, 0)), (1.0, at("Intro_Ural", -500, 795, -40), (0, 0, 0))]}],
         "cues": [{"type": "Card", "at": 0.8, "dur": 5.0, "l1": "THE URAL SHIELD", "l2": ""}]},
        {"id": "6.4", "mpc": {"Sand": [(0.0, 0.22), (1.0, 0.4)]}, "levels": ["Intro_Ural"], "vo": ["6_4"], "pre": 0.6, "post": 1.2, "fov": 50,
         "cam": [(0.0, at("Intro_Ural", -26000, -52000, 9000), {"at": at("Intro_Ural", 0, 60000, 2500)}),
                 (1.0, at("Intro_Ural", -47000, -84000, 27000), {"at": at("Intro_Ural", 0, 80000, 2500)})],
         "actors": [
             {"kind": "mesh", "asset": "SM_UralShield_GateLeaf_L", "label": "Leaf_L", "keys": [(0.0, at("Intro_Ural", -500, -795, -40), (0, 0, 0))]},
             {"kind": "mesh", "asset": "SM_UralShield_GateLeaf_R", "label": "Leaf_R", "keys": [(0.0, at("Intro_Ural", -500, 795, -40), (0, 0, 0))]}]},
        # ---- Section 7: 1948-1975, the Directorate -----------------------------------------------------------------
        {"id": "7.1", "levels": ["Intro_Hall"], "vo": ["7_1"], "pre": 0.8, "post": 0.8, "fov": 55,
         "cam": [(0.0, at("Intro_Hall", -1750, -260, 190), {"at": at("Intro_Hall", 1440, 0, 900)}),
                 (1.0, at("Intro_Hall", -1250, -180, 230), {"at": at("Intro_Hall", 1440, 0, 960)})],
         "actors": [
             # The old portrait goes down, seen from behind: only the back of the frame.
             {"kind": "mesh", "asset": "SM_Intro_PictureFrame", "label": "OldPortrait",
              "keys": [(0.0, at("Intro_Hall", 900, 0, 1330), (0, 90, 0), 2.4), (0.5, at("Intro_Hall", 900, 0, 760), (0, 90, 4), 2.4),
                       (0.62, at("Intro_Hall", 900, 0, 560), (0, 90, 9), 2.4), (1.0, at("Intro_Hall", 900, 0, 560), (0, 90, 9), 2.4)]},
             {"kind": "spot", "label": "Key", "intensity": 190.0, "color": (1.0, 0.86, 0.7, 1.0), "radius": 4000.0, "cone": 38.0,
              "keys": [(0.0, at("Intro_Hall", 100, 0, 1500), {"at": at("Intro_Hall", 1440, 0, 900)})]}] + [
             # The banner's pivot is its pole: scaling Z from almost nothing unrolls it downward.
             {"kind": "mesh", "asset": "SM_Intro_Banner", "label": "Banner_%d" % index, "mats": [emblem, None],
              "keys": [(0.0, at("Intro_Hall", 1440, y, 1500), (0, 90, 0), (1.5, 1.5, 0.03)),
                       (0.45 + 0.1 * index, at("Intro_Hall", 1440, y, 1500), (0, 90, 0), (1.5, 1.5, 0.03)),
                       (0.8 + 0.08 * index, at("Intro_Hall", 1440, y, 1500), (0, 90, 0), (1.5, 1.5, 1.5)),
                       (1.0, at("Intro_Hall", 1440, y, 1500), (0, 90, 0), (1.5, 1.5, 1.5))]}
             for index, (y, emblem) in enumerate(((-560, "MI_Emblem_DirectorOfShield"), (0, "MI_Emblem_DirectorOfForge"),
                                                  (560, "MI_Emblem_DirectorOfHarvest")))],
         "cues": [card("1948 - 1975", "REPORTS INCOMPLETE", 0.5, 5.6)]},
        {"id": "7.2", "levels": ["Intro_Hall"], "vo": ["7_2"], "pre": 0.5, "post": 1.4, "fov": 60,
         "cam": [(0.0, at("Intro_Hall", -760, 240, 420), {"at": at("Intro_Hall", 1440, 0, 980)}),
                 (1.0, at("Intro_Hall", -520, 120, 520), {"at": at("Intro_Hall", 1440, 0, 980)})],
         "actors": [
             {"kind": "mesh", "asset": "SM_Intro_Banner", "label": "Banner_%d" % index, "mats": [emblem, None],
              "keys": [(0.0, at("Intro_Hall", 1440, y, 1500), (0, 90, 0), (1.5, 1.5, 1.5))]}
             for index, (y, emblem) in enumerate(((-560, "MI_Emblem_DirectorOfShield"), (0, "MI_Emblem_DirectorOfForge"),
                                                  (560, "MI_Emblem_DirectorOfHarvest")))] + [
             {"kind": "spot", "label": "Key", "intensity": 110.0, "color": (1.0, 0.86, 0.7, 1.0), "radius": 4000.0, "cone": 38.0,
              "keys": [(0.0, at("Intro_Hall", 100, 0, 1500), {"at": at("Intro_Hall", 1440, 0, 900)})]}] + [
             {"kind": "spot", "label": "Light_%d" % index, "intensity": 210.0, "color": (1.0, 0.9, 0.78, 1.0), "radius": 3000.0,
              "cone": 22.0, "spawn": (0.12 + 0.24 * index, 1.0),
              "keys": [(0.0, at("Intro_Hall", 300, y, 1350), {"at": at("Intro_Hall", 1440, y, 950)})]} for index, y in enumerate((-560, 0, 560))]},
        # ---- Section 8: Radixite and the desert --------------------------------------------------------------------
        {"id": "8.1", "levels": ["Intro_Crater"], "vo": ["8_1"], "pre": 0.8, "post": 0.8, "fov": 42,
         "cam": [(0.0, at("Intro_Crater", -4000, -2250, 1950), {"at": at("Intro_Crater", 300, 150, 0)}),
                 (0.55, at("Intro_Crater", -2950, -1660, 1700), {"at": at("Intro_Crater", 150, 80, 0)}),
                 (1.0, at("Intro_Crater", -1650, -930, 560), {"at": at("Intro_Crater", 0, 0, 40)})],
         "actors": [
             {"kind": "point", "label": "GroundGlow", "intensity": 14.0, "color": (1.0, 0.42, 0.08, 1.0), "radius": 2600.0,
              "keys": [(0.0, at("Intro_Crater", 0, 0, 160), (0, 0, 0))]}],
         "mpc": {"Crystal": [(0.0, 0.0), (0.25, 0.02), (1.0, 0.55)], "Darken": [(0.0, 0.3)]}},
        {"id": "8.2", "levels": ["Intro_Crater"], "vo": ["8_2"], "pre": 0.6, "post": 1.6, "fov": 50,
         "cam": [(0.0, at("Intro_Crater", -1380, -780, 430), {"at": at("Intro_Crater", 0, 0, 50)}),
                 (1.0, at("Intro_Crater", -640, -520, 170), {"at": at("Intro_Crater", 0, 0, 110)})],
         "actors": [
             {"kind": "mesh", "asset": mesh, "label": "Crystal_%d" % index,
              "keys": [(0.0, at("Intro_Crater", dx, dy, -12), (0, yaw, 0), 0.04), (0.2 + 0.08 * index, at("Intro_Crater", dx, dy, -12), (0, yaw, 0), 0.12),
                       (1.0, at("Intro_Crater", dx, dy, -12), (0, yaw, 0), size)]}
             for index, (mesh, dx, dy, yaw, size) in enumerate((("SM_Crystals1", 0, 0, 0, 1.5), ("SM_Crystals3", 250, 110, 80, 1.1),
                                                                ("SM_Crystals5", -230, 170, 200, 1.2), ("SM_Crystal1", 110, -250, 30, 1.6),
                                                                ("SM_Scattered_Crystals3", -160, -190, 140, 1.3)))] + [
             {"kind": "point", "label": "CrystalGlow", "intensity": 30.0, "color": (1.0, 0.42, 0.08, 1.0), "radius": 2200.0,
              "keys": [(0.0, at("Intro_Crater", 0, 0, 120), (0, 0, 0))]}],
         "mpc": {"Crystal": [(0.0, 0.55), (1.0, 0.8)], "Darken": [(0.0, 0.3), (1.0, 0.2)]},
         "cues": [{"type": "Label", "at": 2.0, "dur": 4.2, "l1": "RADIXITE", "pos": (0.60, 0.40)}]},
        {"id": "8.3", "levels": ["Intro_Crater"], "vo": ["8_3"], "pre": 0.5, "post": 1.0, "fov": 40,
         "cam": [(0.0, at("Intro_Crater", -330, 420, 110), {"at": at("Intro_Crater", 0, 0, 110)}),
                 (1.0, at("Intro_Crater", -300, 380, 110), {"at": at("Intro_Crater", 0, 0, 110)})],
         "actors": [
             {"kind": "mesh", "asset": "SM_Crystals1", "label": "Crystal", "keys": [(0.0, at("Intro_Crater", 0, 0, -12), (0, 0, 0), 1.5)]},
             {"kind": "mesh", "asset": "SM_Crystal1", "label": "Crystal_B", "keys": [(0.0, at("Intro_Crater", 110, -250, -12), (0, 30, 0), 1.6)]},
             {"kind": "mesh", "asset": "SM_Crystals5", "label": "Crystal_C", "keys": [(0.0, at("Intro_Crater", -230, 170, -12), (0, 200, 0), 1.2)]},
             {"kind": "point", "label": "Glow", "intensity": 30.0, "color": (1.0, 0.42, 0.08, 1.0), "radius": 2200.0,
              "keys": [(0.0, at("Intro_Crater", 0, 0, 120), (0, 0, 0))]},
             {"kind": "point", "label": "Flare", "intensity": 500.0, "color": (1.0, 0.5, 0.12, 1.0), "radius": 2600.0,
              "spawn": (0.55, 0.72), "keys": [(0.0, at("Intro_Crater", 0, 0, 120), (0, 0, 0))]}],
         "mpc": {"Crystal": [(0.0, 0.8), (0.5, 0.8), (0.6, 1.6), (0.75, 0.8)], "Darken": [(0.0, 0.2)]}},
        {"id": "8.4a", "levels": [DESERT], "vo": ["8_4"], "pre": 0.5, "cut_vo": 0.62, "fov": 58, "cam_snap": 380.0,
         "cam": [(0.0, (24000, -2000, 0), (-8, -140, 0)), (1.0, (23700, -2250, 0), (-9, -138, 0))],
         "move_level_actors": [
             {"level": "SM_Desert_01", "label": "SMD_OasisWater",
              "keys": [(0.0, (15700, -3600, 372), (0, 0, 0), (139.0, 114.0, 1.0)), (0.15, (15700, -3600, 372), (0, 0, 0), (139.0, 114.0, 1.0)),
                       (1.0, (15700, -3600, 150), (0, 0, 0), (139.0, 114.0, 1.0))]}]},
        {"id": "8.4b", "levels": [DESERT], "min": 6.6, "fov": 62, "cam_snap": 330.0,
         "cam": [(0.0, (0, 0, 0), (-3, 45, 0)), (1.0, (300, 250, 0), (-3, 40, 0))],
         "actors": [{"kind": "niagara", "asset": "NS_AmbientDustDevils", "label": "DustDevils", "snap": 0.0,
                     "keys": [(0.0, (3500, 3500, 0), (0, 0, 0))]},
                    {"kind": "niagara", "asset": "NS_SandStorm_B", "label": "Sand", "snap": 200.0,
                     "keys": [(0.0, (2600, 2400, 0), (0, 225, 0), 1.0)]}],
         "mpc": {"Heat": [(0.0, 0.8)], "Sand": [(0.0, 0.25), (1.0, 0.7)]}},
        # ---- Section 9: 1976, what survived the fire ---------------------------------------------------------------
        {"id": "9.1", "levels": [OLD], "vo": ["9_1a", "9_1b"], "pre": 0.8, "post": 1.2, "gap": 1.3, "fov": 55,
         "cam": [(0.0, (-3300, HANGAR_Y - 900, 300), (5, 66, 0)), (1.0, (-2900, HANGAR_Y - 700, 280), (6, 70, 0))],
         "cues": [stamp("UNKNOWN", (0.72, 0.62), -8.0, 0, -1.0, 2.1)]},
        {"id": "9.2", "levels": ["Intro_Bunker"], "vo": ["9_2"], "pre": 2.6, "min": 7.2, "fov": 55,
         "cam": [(0.0, at("Intro_Bunker", -74, 148, 150), (-82, 0, 0)), (1.0, at("Intro_Bunker", -215, 60, 232), (-42, 0, 0))],
         "mpc": {"Archive": [(0.0, 1.0), (0.06, 1.0), (0.3, 0.0)], "Flash": [(0.0, 0.0), (0.05, 0.0), (0.08, 0.14), (0.16, 0.0)],
                 # 1976: every strike on the map is long burnt out (no flames, no smoke).
                 "Rings36": [(0.0, 60.0)], "Rings42": [(0.0, 60.0)], "Rings62": [(0.0, 60.0)]},
         "actors": tags_curl((0.0, 0.0, 0.0)),
         "cues": [card("1976", "THIRTY YEARS LATER", 1.4, 5.0)]},
        {"id": "9.3a", "handheld": 0.14, "levels": [DESERT], "grade": "present", "vo": ["9_3"], "pre": 0.4, "cut_vo": 0.75, "fov": 45, "cam_snap": 150.0,
         "cam": [(0.0, (DUNE[0] + 6950, -2950, 0), {"at": (DUNE[0] + 6050, -3550, 0), "snap": 110.0}),
                 (1.0, (DUNE[0] + 6800, -2850, 0), {"at": (DUNE[0] + 6050, -3550, 0), "snap": 110.0})],
         "actors": [
             {"kind": "class", "asset": "BP_Env_DestroyedT34", "label": "Wreck", "snap": 0.0, "keys": [(0.0, (DUNE[0] + 6000, -3600, 0), (0, 40, 0))]},
             {"kind": "skel", "asset": SOLDIER_SCAV, "label": "Scav_A", "anim": "AN_Infantry_MortarCrew_Crouched", "snap": 0.0,
              "keys": [(0.0, (DUNE[0] + 6200, -3330, 0), (0, 235, 0))]},
             {"kind": "skel", "asset": SOLDIER_SCAV, "label": "Scav_B", "anim": "Anim_TW_StandingIdle", "snap": 0.0,
              "keys": [(0.0, (DUNE[0] + 6420, -3700, 0), (0, 175, 0))]},
             {"kind": "niagara", "asset": "NS_Sparks_Welding", "label": "Sparks", "snap": 110.0, "keys": [(0.0, (DUNE[0] + 6120, -3400, 0), (0, 0, 0))]}],
         "mpc": {"Heat": [(0.0, 1.0)]}},
        {"id": "9.3b", "levels": [DESERT], "grade": "present", "min": 5.2, "fov": 50, "cam_snap": 260.0,
         "cam": [(0.0, (2500, 2750, 0), {"at": (0, 0, 0), "snap": 480.0}), (1.0, (2950, 2300, 0), {"at": (0, 0, 0), "snap": 560.0})],
         "actors": [{"kind": "mesh", "asset": "SM_HQ_Deploy", "label": "NomadicHQ", "snap": 0.0, "keys": [(0.0, (0, 0, 0), (0, 30, 0))]}],
         # The truck unfolds into the HQ inside the shot (the mesh's baked deploy animation).
         "cues": [{"type": "MaterialRamp", "at": 0.2, "dur": 4.6, "l1": "SM_HQ_Deploy", "l2": "NomadicDeployProgress"}],
         "mpc": {"Heat": [(0.0, 1.0)]}},
        {"id": "9.3c", "levels": [DESERT], "grade": "present", "min": 3.8, "fov": 55, "cam_snap": 220.0,
         "cam": [(0.0, (8000, -25000, 0), (10, 47, 0)), (1.0, (8000, -25000, 0), (9, -52, 0))],
         "actors": [
             {"kind": "mesh", "asset": "SM_Mammut_Body", "label": "Mammut", "snap": 1900.0, "mats": M + "MI_Intro_Steel",
              "keys": [(0.0, (16000, -15000, 0), (0, -120, 0)), (1.0, (11500, -31000, -700), (0, -120, 0))]},
             {"kind": "mesh", "asset": "SM_Mammut_Nose", "label": "MammutNose", "snap": 1900.0, "mats": M + "MI_Intro_Steel",
              "keys": [(0.0, (16000, -15000, 0), (0, -120, 0)), (1.0, (11500, -31000, -700), (0, -120, 0))]}],
         "mpc": {"Heat": [(0.0, 1.0)]}},
        {"id": "9.4", "levels": [DESERT], "grade": "present", "vo": ["9_4"], "pre": 1.0, "post": 0.8, "fov": 42, "cam_snap": 640.0,
         "cam": [(0.0, (OUTPOST[0] + 1300, OUTPOST[1] - 560, 0), {"at": (OUTPOST[0] - 5000, OUTPOST[1] + 1100, 0), "snap": 250.0}),
                 (1.0, (OUTPOST[0] + 900, OUTPOST[1] - 400, 0), {"at": (OUTPOST[0] - 5000, OUTPOST[1] + 1100, 0), "snap": 230.0})],
         "actors": [
             # The wall of an outpost seen from inside: a firing step behind it, the sentry on the step, his back to us.
             {"kind": "mesh", "asset": "SM_Wall_14", "label": "Wall_%d" % index, "snap": 0.0,
              "keys": [(0.0, (OUTPOST[0], OUTPOST[1] + (index - 1) * 2260, 0), (0, 90, 0))]} for index in range(3)] + [
             {"kind": "mesh", "asset": CUBE, "label": "FiringStep", "mats": M + "MI_Intro_DarkWood", "snap": 0.0,
              "keys": [(0.0, (OUTPOST[0] + 190, OUTPOST[1], 392), (0, 0, 0), (3.2, 30.0, 0.16))]}] + [
             {"kind": "mesh", "asset": CUBE, "label": "StepPost_%d" % index, "mats": M + "MI_Intro_DarkWood", "snap": 0.0,
              "keys": [(0.0, (OUTPOST[0] + 320, OUTPOST[1] + y, 192), (0, 0, 0), (0.16, 0.16, 3.9))]} for index, y in enumerate((-1200, -400, 400, 1200))] + [
             {"kind": "mesh", "asset": "SM_Rus_GuardMgTower", "label": "Tower", "snap": 0.0,
              "keys": [(0.0, (OUTPOST[0] + 420, OUTPOST[1] + 1750, 0), (0, 180, 0))]},
             {"kind": "skel", "asset": SOLDIER_RUS, "label": "Sentry", "anim": "AN_Infantry_SpotHowitzer_Standing", "snap": 0.0,
              "keys": [(0.0, (OUTPOST[0] + 150, OUTPOST[1] + 60, 401), (0, 86, 0))]},
             {"kind": "skel", "asset": SOLDIER_RUS, "label": "Sentry_B", "anim": "Anim_TW_StandingIdle", "snap": 0.0,
              "keys": [(0.0, (OUTPOST[0] + 170, OUTPOST[1] + 880, 401), (0, 104, 0))]},
             # Sand on the wind over the parapet.
             {"kind": "niagara", "asset": "NS_SandStorm_B", "label": "Sand", "snap": 200.0,
              "keys": [(0.0, (OUTPOST[0] - 2500, OUTPOST[1] + 400, 0), (0, 60, 0), 1.0)]}],
         "mpc": {"Darken": [(0.0, 0.12)], "Heat": [(0.0, 0.8)], "Sand": [(0.0, 0.45)]}},
        {"id": "9.5", "levels": [DESERT], "grade": "present", "vo": ["9_5"], "pre": 0.5, "post": 0.9, "handheld": 0.2, "fov": 40, "cam_snap": 130.0,
         "cam": [(0.0, (DUNE[0] - 2300, DUNE[1] - 500, 0), {"at": (DUNE[0] + 2200, DUNE[1], 0), "snap": 300.0}),
                 (1.0, (DUNE[0] - 2300, DUNE[1] - 500, 0), {"at": (DUNE[0] + 1800, DUNE[1], 0), "snap": 260.0})],
         "actors": [
             {"kind": "class", "asset": PANTHER, "label": "Panther_%d" % index, "snap": 5.0,
              "keys": [(0.0, (DUNE[0] + 1700 + index * 520, DUNE[1] + offset, 0), (0, 180, 0)),
                       (1.0, (DUNE[0] - 900 + index * 520, DUNE[1] + offset, 0), (0, 180, 0))]}
             for index, offset in enumerate((0, -1100, 1000, -2100))] + [
             {"kind": "niagara", "asset": "NS_SandStorm_B", "label": "Sand", "snap": 150.0,
              "keys": [(0.0, (DUNE[0] + 1500, DUNE[1], 0), (0, 180, 0), 1.0)]}],
         "mpc": {"Heat": [(0.0, 1.0)], "Sand": [(0.0, 0.5)]}},
        {"id": "9.6", "handheld": 0.18, "levels": [DESERT], "grade": "present", "vo": ["9_6"], "pre": 0.3, "post": 0.5, "fov": 35, "cam_snap": 170.0,
         "cam": [(0.0, (DUNE[0] - 300, DUNE[1] - 1900, 0), {"at": (DUNE[0] + 900, DUNE[1], 0), "snap": 220.0}),
                 (1.0, (DUNE[0] - 350, DUNE[1] - 1850, 0), {"at": (DUNE[0] + 800, DUNE[1], 0), "snap": 220.0})],
         "actors": [
             {"kind": "class", "asset": PANTHER, "label": "Panther_Lead", "snap": 5.0,
              "keys": [(0.0, (DUNE[0] + 1400, DUNE[1], 0), (0, 180, 0)), (0.62, (DUNE[0] + 900, DUNE[1], 0), (0, 180, 0)),
                       (1.0, (DUNE[0] + 900, DUNE[1], 0), (0, 180, 0))]},
             # The shot itself: blast and light at the muzzle a few frames before the picture burns out.
             {"kind": "niagara", "asset": "NS_LaunchSmoke_Tank_SplashGroundSandy", "label": "GroundBlast", "snap": 20.0, "spawn": (0.78, 1.0),
              "keys": [(0.0, (DUNE[0] + 600, DUNE[1], 0), (0, 180, 0), 1.0)]},
             {"kind": "point", "label": "MuzzleFlash", "intensity": 180.0, "color": (1.0, 0.75, 0.4, 1.0), "radius": 6000.0, "snap": 260.0,
              "spawn": (0.78, 1.0), "keys": [(0.0, (DUNE[0] + 300, DUNE[1], 0), (0, 0, 0))]}],
         "mpc": {"Heat": [(0.0, 1.0)], "Sand": [(0.0, 0.5)], "Flash": [(0.0, 0.0), (0.88, 0.0), (0.90, 1.0), (0.915, 1.0), (0.93, 0.0)],
                 # Darken above 1.25 is full black in the grade: the picture never comes back after the flash.
                 "Darken": [(0.0, 0.0), (0.915, 0.0), (0.93, 1.3), (1.0, 1.3)]}},
        {"id": "9.7", "levels": [OLD], "sequence": "/Game/RTS_Survival/Cinematics/LS_RTS_Logo", "min": 15.4, "grade": "present", "fade_out": 0.6},
    ]


def global_cues(timeline):
    """Cues that are not tied to one shot: the portrait (from the first line to the last) and the hidden gameplay
    actors of the streamed maps."""
    by_id = {placed["id"]: placed for placed in timeline}
    first_voice = by_id["1.1"]["vo"][0]["start"]
    last = by_id["9.6"]
    # The portrait's fade-out lies after the last shot's black, not over its flash.
    cues = [{"type": "Portrait", "start": first_voice - 0.6, "end": last["start"] + last["duration"]}]
    # (part of a class or mesh name, part of the level name or "" for every streamed level)
    rules = [("SM_DeadWillow", "Ger_PripyatMe410")]     # leaf cards that draw as grey squares in this light
    rules += [(part, "") for part in ("Ger_Vehicle_", "Rus_Vehicle_", "Squad_", "BP_Mission", "Ger_Inf_", "Rus_Inf_", "RTS_BP_Nav",
                                     "PlayerStart", "StartLocation",
                                     "BP_Scene_HideTank", "BP_TestFow", "ScavObj_", "Note")]
    # The old intro map: nothing orange near a rocket (doc table), and none of its own staging.
    rules += [(part, "RTS_IntroCinematic") for part in ("Radixite", "Crystal", "Rus_Turret_", "RTS_BP_EmbTurret", "Env_Dest_",
                                                         "Ger_Weapon_")]
    # Ger_Berlin: the pod with the "Demeter" label must never be in frame.
    rules += [("DemeterPod", "Ger_Berlin")]
    # Ger_2ndOreMountains: resource pickups draw as glassy bubbles beside the road (6.1).
    rules += [("Resource_", "Ger_2ndOreMountains")]
    # ... and its parked civilian cars read as a van driving in the 6.1 tank column.
    rules += [("BP_Cover_OldCar", "Ger_2ndOreMountains")]
    for part, level in rules:
        cues.append({"type": "HideActors", "start": 0.0, "end": 0.0, "l1": part, "l2": level})
    return cues
