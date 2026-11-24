"""Shots 3.3 / 3.4: the Berlin strike, in 3D, from the Berlin mission kits (ruin kit, Reichstag, flak tower, the
Brandenburg Gate) on a small set of its own (Ger_Berlin is never touched: this strike has to knock the city down).

Two levels at the same origin, both built here from one city plan:
- Intro_Skyline (3.3, 3.4a): the city standing, January 1945 (bomb-damaged shells; the kits have no intact houses).
- Intro_SkylineRuin (3.4b): the same plots flattened: heaps, stubs and footprints. The buildings the camera sees fall
  in 3.4b as spawnables (collapse_actors), knocked over and sunk into the ground by the blast front as it runs out from
  ground zero; the far city behind ground zero is gone under the white-out at the 3.4a / 3.4b cut.
The detonation is the game's own nuke (NS_Nuke_1, as in Operation Svarog, 6.2), spawned at the start of 3.4b.

Plan coordinates are local to the set origin (cm): the camera looks down the boulevard along +X to the Gate.
"""
import math
import random

RUIN = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/"
RT = "/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin/Mesh/"
GATE = "/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/SM_BrandenburgGate_Ruin"
FLAK = "/Game/RTS_Survival/Blueprints/Environment/Berlin/FlakTower/SM_BF_FlakTower_LightDamage"
BREWERY = "/Game/RTS_Survival/Blueprints/Environment/Berlin/BreweryEnclave/Meshes/SM_Berlin_BreweryHall"
NUKE = "/Game/BigExplosions/Niagara/NS_Nuke_1"
# Collapse dust: the game's own building-destruction smoke and stone debris burst (the explosion pack's NS_Ground_*
# are fiery blasts, far too big here; the smoke pillar drew thin wisps like dead trees).
DUST = ("/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke_Large",
        "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke_Large",
        "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke")
DEBRIS = "/Game/RTS_Survival/ImpactSfxVfx/4SuperHeavyCalibre/Stone/NS_RTSDebrisExplosion_Huge"

LEVEL = "Intro_Skyline"
GZ = (86000.0, -5000.0)          # ground zero (local cm)
GATE_AT = (20000.0, 0.0)
CAMERA_X = -7600.0               # 3.3 starts here, 16 m up, looking down the boulevard
FRONT_SPEED = 18000.0            # blast front, cm/s (slowed for the eye: it reaches the camera ~5.3 s after the flash)
FRONT_DELAY = 0.15               # s after the start of 3.4b
NUKE_SCALE = 16.0

# Standing shells: mesh -> (height cm, width cm across the plot, kind of fall)
# 'facade' = thin wall that topples; 'block' = deep shell that sinks into itself with a smaller lean.
SHELLS = {
    RUIN + "Meshes/SM_BerlinRuin_E1_CornerTower": (2080, 2000, "block"),
    RUIN + "Meshes/SM_BerlinRuin_E2_Courtyard": (1604, 2600, "block"),
    RUIN + "Meshes/SM_BerlinRuin_E3_CoreRow": (1837, 3000, "block"),
    RUIN + "Meshes/SM_BerlinRuin_B3_Facade4St": (1554, 1300, "facade"),
    RUIN + "Meshes/SM_BerlinRuin_B4_Firewall": (1675, 1300, "facade"),
    RUIN + "Meshes/SM_BerlinRuin_B7_WallCore": (1145, 1600, "block"),
    RUIN + "Meshes/SM_BerlinRuin_B5_CornerL": (1110, 850, "facade"),
    RUIN + "Meshes/SM_BerlinRuin_B2_Facade2St_A": (786, 1300, "facade"),
    RUIN + "Meshes/SM_BerlinRuin_E7_HalfHouse_A": (796, 860, "block"),
    RUIN + "Meshes/SM_BerlinRuin_E7_HalfHouse_B": (800, 860, "block"),
    BREWERY: (2005, 2500, "block"),
}
STREET_FRONT = [RUIN + "Meshes/SM_BerlinRuin_B3_Facade4St", RUIN + "Meshes/SM_BerlinRuin_B4_Firewall",
                RUIN + "Meshes/SM_BerlinRuin_E3_CoreRow", RUIN + "Meshes/SM_BerlinRuin_B3_Facade4St",
                RUIN + "Meshes/SM_BerlinRuin_B2_Facade2St_A", RUIN + "Meshes/SM_BerlinRuin_B7_WallCore"]
BACK_ROW = [RUIN + "Meshes/SM_BerlinRuin_E1_CornerTower", RUIN + "Meshes/SM_BerlinRuin_E2_Courtyard",
            RUIN + "Meshes/SM_BerlinRuin_E3_CoreRow", RUIN + "Meshes/SM_BerlinRuin_B7_WallCore", BREWERY,
            RUIN + "Meshes/SM_BerlinRuin_E7_HalfHouse_A", RUIN + "Meshes/SM_BerlinRuin_E7_HalfHouse_B",
            RUIN + "Meshes/SM_BerlinRuin_B5_CornerL"]
# What a plot is after the strike (ruin level) and under a standing shell (both levels).
HEAPS = [RUIN + "Footprints/SM_BerlinFoot_CollapseHeap_L", RUIN + "Footprints/SM_BerlinFoot_CollapseHeap_S",
         RUIN + "Cover/Meshes/SM_BerlinCoverHeap_Concrete_A", RUIN + "Cover/Meshes/SM_BerlinCoverHeap_Brick",
         RUIN + "Cover/Meshes/SM_BerlinCoverHeap_Block"]
STUBS = [RUIN + "Meshes/SM_BerlinRuin_E4_StubRing_A", RUIN + "Meshes/SM_BerlinRuin_E4_StubRing_B",
         RUIN + "Meshes/SM_BerlinRuin_B6_Stub_A", RUIN + "Meshes/SM_BerlinRuin_B6_Stub_B",
         RUIN + "Meshes/SM_BerlinRuin_B8_DebrisBed", RUIN + "Meshes/SM_BerlinRuin_E6_HeapFin_A"]
FOOTPRINTS = [RUIN + "Footprints/SM_BerlinFoot_SlabPlot_A", RUIN + "Footprints/SM_BerlinFoot_SlabPlot_B",
              RUIN + "Footprints/SM_BerlinFoot_Shell_A", RUIN + "Footprints/SM_BerlinFoot_Shell_B",
              RUIN + "Footprints/SM_BerlinFoot_StubHeap", RUIN + "Footprints/SM_BerlinFoot_HalfHouse_A"]
STREET_LITTER = [RUIN + "Cover/Meshes/SM_BerlinCover_04_ToppledSlab", RUIN + "Cover/Meshes/SM_BerlinCover_05_FallenChimney",
                 RUIN + "Cover/Meshes/SM_BerlinCover_03_HeapChunk", RUIN + "Cover/Meshes/SM_BerlinCover_10_MasonryBlock",
                 RUIN + "Cover/Meshes/SM_BerlinCoverHeap_Concrete_B", RUIN + "Cover/Meshes/SM_BerlinCoverHeap_Concrete_C"]


def _in_view(x, y, margin=4000.0):
    """Roughly inside the cone the camera sweeps in 3.3 - 3.4b (looking +X from CAMERA_X, ~25 degrees each side)."""
    return abs(y) < (x - CAMERA_X) * 0.47 + margin


def city():
    """The city plan: one dict per building plot. fall=True: the shell is a spawnable in 3.4b that the blast knocks down
    (near enough to watch); False: far city, standing level only (its plot is a stub or heap in the ruin level)."""
    rng = random.Random(19450116)
    plots = []

    def add(mesh, x, y, yaw, scale, fall):
        if math.hypot(x - GZ[0], y - GZ[1]) < 4200.0:       # the hypocentre stays open
            return
        if abs(x - GATE_AT[0]) < 4800.0 and abs(y - GATE_AT[1]) < 7500.0:   # Pariser Platz and the Gate
            return
        height, width, kind = SHELLS[mesh]
        plots.append({"label": "Berlin_%03d" % len(plots), "mesh": mesh, "x": x, "y": y, "yaw": yaw, "scale": scale,
                      "height": height * scale, "width": width * scale, "kind": kind, "fall": fall,
                      "heap": HEAPS[rng.randrange(len(HEAPS))], "stub": STUBS[rng.randrange(len(STUBS))],
                      "foot": FOOTPRINTS[rng.randrange(len(FOOTPRINTS))], "heap_yaw": rng.uniform(0, 360),
                      "lean": rng.uniform(0.75, 1.15), "seed": rng.random()})

    # The boulevard (60 m wide) from behind the camera to the square before the Gate: street fronts on both sides,
    # long side along the street (yaw 90), then a second and third row behind.
    for side in (-1, 1):
        x = CAMERA_X - 2200.0
        while x < GATE_AT[0] - 5000.0:
            mesh = STREET_FRONT[rng.randrange(len(STREET_FRONT))]
            scale = rng.uniform(1.15, 1.35)
            width = SHELLS[mesh][1] * scale
            add(mesh, x + width / 2.0, side * (3300.0 + rng.uniform(0, 250)), 90.0 + (180.0 if side > 0 else 0.0), scale, True)
            x += width + rng.uniform(60, 420)
        for row, (y0, y1, step) in enumerate(((6200, 8400, 3600), (10500, 15500, 5200))):
            x = CAMERA_X + rng.uniform(-1500, 1500)
            while x < GATE_AT[0] - 5000.0:
                mesh = BACK_ROW[rng.randrange(len(BACK_ROW))]
                add(mesh, x, side * rng.uniform(y0, y1), rng.choice((0, 90, 180, 270)) + rng.uniform(-6, 6),
                    rng.uniform(1.1, 1.4), True)
                x += step * rng.uniform(0.85, 1.25)
    # Beyond the Gate: the city to the horizon, blocks on a loose grid inside the view.
    x = 26000.0
    while x < 170000.0:
        y = -60000.0
        while y < 60000.0:
            px, py = x + rng.uniform(-1500, 1500), y + rng.uniform(-1500, 1500)
            if _in_view(px, py) and not (abs(py) < 2600 and px < 60000):    # the avenue on beyond the Gate stays open
                mesh = BACK_ROW[rng.randrange(len(BACK_ROW))] if rng.random() < 0.7 else STREET_FRONT[rng.randrange(len(STREET_FRONT))]
                near = px < GZ[0] - 12000.0 and abs(py) < 22000.0
                add(mesh, px, py, rng.choice((0, 90, 180, 270)) + rng.uniform(-8, 8), rng.uniform(1.1, 1.45), near)
            y += rng.uniform(4600, 6400)
        x += rng.uniform(5200, 7200)
    return plots


# Landmarks: (label, mesh, x, y, yaw, scale, fall). The Gate stands through it (3.5 stands inside its ruin).
LANDMARKS = [
    ("Berlin_Gate", GATE, GATE_AT[0], GATE_AT[1], 90.0, 1.0, False),
    ("Berlin_RT_Dome", RT + "SM_RT_Dome", 36000.0, -17000.0, 0.0, 1.0, True),
    ("Berlin_RT_Portico", RT + "SM_RT_Portico", 33400.0, -17000.0, -90.0, 1.0, True),
    ("Berlin_RT_TowerNW", RT + "SM_RT_Tower_Tall", 33000.0, -21000.0, 0.0, 1.0, True),
    ("Berlin_RT_TowerSW", RT + "SM_RT_Tower_Mid", 33000.0, -13000.0, 0.0, 1.0, True),
    ("Berlin_RT_TowerNE", RT + "SM_RT_Tower_Mid", 39500.0, -21000.0, 0.0, 1.0, True),
    ("Berlin_RT_TowerSE", RT + "SM_RT_Tower_Low", 39500.0, -13000.0, 0.0, 1.0, True),
    ("Berlin_FlakTower", FLAK, 62000.0, 24000.0, 20.0, 1.0, False),
]
LANDMARK_HEIGHT = {"SM_RT_Dome": 3660, "SM_RT_Portico": 2400, "SM_RT_Tower_Tall": 3915, "SM_RT_Tower_Mid": 2650,
                   "SM_RT_Tower_Low": 1782}


def landmark_plots():
    plots = []
    for label, mesh, x, y, yaw, scale, fall in LANDMARKS:
        height = LANDMARK_HEIGHT.get(mesh.rsplit("/", 1)[-1], 2500) * scale
        plots.append({"label": label, "mesh": mesh, "x": x, "y": y, "yaw": yaw, "scale": scale, "height": height,
                      "width": 2000.0, "kind": "block", "fall": fall, "heap": RT + "SM_RT_Rubble_L", "stub": RT + "SM_RT_Rubble_Ramp",
                      "foot": None, "heap_yaw": (x * 7.0) % 360.0, "lean": 0.6, "seed": (y * 0.37) % 1.0})
    return plots


# --- the sets ------------------------------------------------------------------------------------------------------
def build_level(base, sets2, ruined):
    """Builds Intro_Skyline (standing) or Intro_SkylineRuin (ruined). The level must be the editor's current world
    (standalone) or a streamed sublevel of the cinematic map (base.Set handles both)."""
    import unreal
    name = "Intro_SkylineRuin" if ruined else "Intro_Skyline"
    s = base.Set(name)
    M = base.M
    rng = random.Random(1945 if not ruined else 1946)
    s.box("Berlin_Ground", (60000, 0, -50), (600000, 600000, 100), M + "MI_Intro_Ground_Berlin")
    # Sky: a dawn backdrop far behind the cloud (the old one stood in the cloud's way), haze, a low sun from behind
    # the camera's left so the street fronts read, and a cool sky light.
    s.mesh("Berlin_Backdrop", base.PLANE, (420000, 0, 90000), rotation=(0, -90, 90), scale=(7000, 2600, 1),
           materials=M + "MI_Intro_Sky_Dawn", shadow=False)
    sets2.sun(s, "Berlin_Sun", -11, 38, 3.4, (1.0, 0.82, 0.62, 1.0))
    sets2.sky_light(s, "Berlin_Sky", 0.75, (0.6, 0.64, 0.74, 1.0))
    sets2.fog(s, "Berlin_Fog", 0.0032, (0.62, 0.56, 0.48, 1.0), 0.006)
    for plot in city() + landmark_plots():
        x, y = plot["x"], plot["y"]
        if plot["foot"] and plot["width"] < 2600:
            s.mesh(plot["label"] + "_Foot", plot["foot"], (x, y, -10), rotation=(0, plot["yaw"], 0),
                   scale=(plot["width"] / 1600.0,) * 2 + (0.8,), shadow=False)
        if plot["label"] in ("Berlin_Gate", "Berlin_FlakTower"):
            s.mesh(plot["label"], plot["mesh"], (x, y, 0), rotation=(0, plot["yaw"], 0), scale=(plot["scale"],) * 3)
            continue
        if not ruined:
            s.mesh(plot["label"], plot["mesh"], (x, y, 0), rotation=(0, plot["yaw"], 0), scale=(plot["scale"],) * 3)
        else:
            # what is left: a heap where it fell (pushed away from ground zero) and, far off, a stub or two
            dx, dy = x - GZ[0], y - GZ[1]
            d = math.hypot(dx, dy) or 1.0
            push = plot["height"] * 0.18
            size = max(0.9, plot["width"] / 1300.0)
            s.mesh(plot["label"] + "_Heap", plot["heap"], (x + dx / d * push, y + dy / d * push, -20),
                   rotation=(0, plot["heap_yaw"], 0), scale=(size, size, size * 0.9), shadow=True)
            if plot["seed"] < 0.45:
                s.mesh(plot["label"] + "_Stub", plot["stub"], (x, y, -10), rotation=(0, plot["yaw"], 0),
                       scale=(plot["scale"],) * 3)
    # Litter on the boulevard and the square: the city had been bombed for two years already.
    for k in range(34):
        x = rng.uniform(CAMERA_X + 1500, GATE_AT[0] - 2500)
        y = rng.uniform(-2600, 2600)
        mesh = STREET_LITTER[rng.randrange(len(STREET_LITTER))]
        s.mesh("Berlin_Litter_%02d" % k, mesh, (x, y, -8), rotation=(0, rng.uniform(0, 360), 0),
               scale=(rng.uniform(0.9, 1.6),) * 3, shadow=True)
    sets2.fixed_exposure(s, "Berlin_Exposure", EXPOSURE)
    base.finish_level(name)


EXPOSURE = 1.0


# --- the shots -----------------------------------------------------------------------------------------------------
def _axes_from_rotator(pitch, yaw, roll):
    cp, sp = math.cos(math.radians(pitch)), math.sin(math.radians(pitch))
    cy, sy = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    cr, sr = math.cos(math.radians(roll)), math.sin(math.radians(roll))
    x = (cp * cy, cp * sy, sp)
    y = (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp)
    z = (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp)
    return x, y, z


def _rotator_from_axes(x, y, z):
    """UE FMatrix::Rotator for a matrix with rows x, y, z."""
    pitch = math.degrees(math.atan2(x[2], math.hypot(x[0], x[1])))
    yaw = math.degrees(math.atan2(x[1], x[0]))
    sy_axis = (-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0.0)
    roll = math.degrees(math.atan2(sum(a * b for a, b in zip(z, sy_axis)), sum(a * b for a, b in zip(y, sy_axis))))
    return pitch, yaw, roll


def _rotate(v, axis, angle):
    """Rodrigues: v turned about the unit axis by angle (radians)."""
    c, s = math.cos(angle), math.sin(angle)
    cross = (axis[1] * v[2] - axis[2] * v[1], axis[2] * v[0] - axis[0] * v[2], axis[0] * v[1] - axis[1] * v[0])
    dot = sum(a * b for a, b in zip(axis, v))
    return tuple(v[i] * c + cross[i] * s + axis[i] * dot * (1 - c) for i in range(3))


def _tilted(yaw, direction, angle_deg):
    """Rotator of a building with this yaw, leaned angle_deg away along the horizontal unit direction."""
    axis = (-direction[1], direction[0], 0.0)           # up x direction: the top moves along +direction
    axes = _axes_from_rotator(0.0, yaw, 0.0)
    turned = [_rotate(a, axis, math.radians(angle_deg)) for a in axes]
    return _rotator_from_axes(*turned)


def _unwrap(previous, angles):
    if previous is None:
        return angles
    return tuple(a + 360.0 * round((p - a) / 360.0) for p, a in zip(previous, angles))


def arrival(x, y):
    """Seconds after the start of 3.4b at which the blast front reaches a plan point."""
    return FRONT_DELAY + math.hypot(x - GZ[0], y - GZ[1]) / FRONT_SPEED


def collapse_actors(at, seconds):
    """Spawnables of 3.4b: every shell near enough to watch falls when the front reaches it (a jolt, then it leans
    away from ground zero and sinks into its own dust), with a dust burst at its foot."""
    actors = []
    for plot in city() + landmark_plots():
        if not plot["fall"]:
            continue
        x, y = plot["x"], plot["y"]
        dx, dy = x - GZ[0], y - GZ[1]
        d = math.hypot(dx, dy) or 1.0
        direction = (dx / d, dy / d)
        t_hit = arrival(x, y)
        height = plot["height"]
        fall_time = 1.1 + height / 1500.0 * 0.55
        lean = (62.0 if plot["kind"] == "facade" else 24.0) * plot["lean"]
        keys = [(0.0, at(LEVEL, x, y, 0), (0.0, plot["yaw"], 0.0), plot["scale"])]
        previous = None
        steps = 14
        for k in range(steps + 1):
            tau = fall_time * k / steps
            t = t_hit + tau
            if t > seconds:
                break
            f = tau / fall_time
            jolt = min(tau / 0.18, 1.0)
            shove = 45.0 * jolt + height * 0.22 * f * f
            sink = height * 1.08 * f ** 1.7
            angle = 3.0 * jolt + lean * f * f
            rotation = _unwrap(previous, _tilted(plot["yaw"], direction, angle))
            previous = rotation
            keys.append((t / seconds, at(LEVEL, x + direction[0] * shove, y + direction[1] * shove, -sink), rotation, plot["scale"]))
        if keys[-1][0] < 1.0:
            keys.append((1.0,) + keys[-1][1:])
        actors.append({"kind": "mesh", "asset": plot["mesh"], "label": plot["label"], "keys": keys})
        # dust where it comes down (bigger buildings, bigger cloud); only where the camera can see it
        if DUST_SCALE > 0 and t_hit + 0.05 < seconds and (plot["width"] >= 1500 or plot["seed"] < 0.5):
            size = max(plot["width"], height) / 1400.0
            size *= min(max(math.hypot(x - CAMERA_X, y) / 40000.0, 0.35), 1.0)    # near the camera: smaller
            actors.append({"kind": "niagara", "asset": DUST[int(plot["seed"] * 3) % 3], "label": plot["label"] + "_Dust",
                           "spawn": ((t_hit + 0.05) / seconds, 1.0),
                           "keys": [(0.0, at(LEVEL, x, y, 0), (0, 0, 0), DUST_SCALE * size)]})
            if plot["seed"] < 0.4:
                actors.append({"kind": "niagara", "asset": DEBRIS, "label": plot["label"] + "_Debris",
                               "spawn": ((t_hit + 0.02) / seconds, 1.0),
                               "keys": [(0.0, at(LEVEL, x, y, height * 0.4), (0, 0, 0), DEBRIS_SCALE * size)]})
    return actors


DUST_SCALE = 2.0
DEBRIS_SCALE = 2.0


def nuke_actors(at, seconds):
    """The detonation (the game's NS_Nuke_1) and the fireball's light on the city, stepping down as it cools."""
    actors = [{"kind": "niagara", "asset": NUKE, "label": "Berlin_Nuke",
               "keys": [(0.0, at(LEVEL, GZ[0], GZ[1], 0), (0, 0, 0), NUKE_SCALE)]}]
    for k, (intensity, off) in enumerate(((2.0e5, 0.10), (8.0e4, 0.28), (2.5e4, 0.6), (8.0e3, 1.0))):
        actors.append({"kind": "point", "label": "Berlin_Fireball_%d" % k, "intensity": intensity,
                       "color": (1.0, 0.7, 0.42, 1.0), "radius": 260000.0, "spawn": (0.0, off),
                       "keys": [(0.0, at(LEVEL, GZ[0], GZ[1], 9000), (0, 0, 0))]})
    return actors


def camera_hit_time():
    return arrival(CAMERA_X + 400.0, 0.0)


def strike_camera(at, seconds, start, end):
    """3.4b camera: drifts on, tilts up after the rising cloud; the blast hits it at camera_hit_time() and shakes it."""
    rng = random.Random(1945)
    hit = camera_hit_time()
    keys = []
    count = int(seconds / 0.1)
    for k in range(count + 1):
        t = seconds * k / count
        f = t / seconds
        ease = f * f * (3 - 2 * f)
        lift = 1.0 - (1.0 - f) ** 2.2          # the tilt-up leads: it follows the cap as it climbs
        position = tuple(start[0][i] + (end[0][i] - start[0][i]) * ease for i in range(3))
        rotation = [start[1][i] + (end[1][i] - start[1][i]) * lift for i in range(3)]
        if t >= hit:
            amp = 1.6 * math.exp(-(t - hit) * 1.4) + 0.25
            rotation = [rotation[0] + rng.uniform(-1, 1) * amp, rotation[1] + rng.uniform(-1, 1) * amp * 0.8,
                        rotation[2] + rng.uniform(-1, 1) * amp * 0.6]
            position = (position[0] - 120.0 * min((t - hit) / 0.4, 1.0), position[1], position[2] + rng.uniform(-1, 1) * amp * 8.0)
        keys.append((f, at(LEVEL, *position), tuple(rotation)))
    return keys


def smoke_actors(at):
    """3.3 / 3.4a: the city still smoulders from the air raids (smoke columns far off)."""
    return [{"kind": "niagara", "asset": "NS_SmokePillar1", "label": "Berlin_Smoke_%d" % k,
             "keys": [(0.0, at(LEVEL, x, y, 0), (0, 0, 0), scale)]}
            for k, (x, y, scale) in enumerate(((52000, 15000, 5.0), (74000, -26000, 6.0), (118000, 11000, 7.0), (36000, -30000, 4.0)))]
