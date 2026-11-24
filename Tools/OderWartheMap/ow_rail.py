"""Rail geometry for Ger_OderWarthe: the metal_c rail yard, its turnouts and the double track off the east edge.

One source for both generators (2026-11-17, user: "implement railroad switches and quality control the railroad
placements; two rails go out of that area to the edge of the map"):
    ow_terrain_gen.py   ballast paint (bed_weight), the rail_near mask and the layout's rail_line
    ow_dress_gen.py     the track panels (panels()) and the loading ramp at the buffer stop

Pure plan geometry (cm, +X north, +Y east, headings in degrees from +X toward +Y = UE yaw); heights are not decided
here: panels() takes a bed height sampler, so the track always sits on whatever heights the terrain generator wrote.

The track mesh SM_RTS_Railway_Track_01 is a 10 m panel: pivot at the panel START, x 0..1000, rails at y +-92.5, rail top
+59, sleeper tops +33..36, its own ballast 7.75 m wide (y -375..400) with a vertical skirt to -93 (measured from the
source FBX). Two tracks 4.5 m apart overlapped their ballast (the old yard), so parallel tracks here are 6 m apart and
every panel is sunk 22 cm (SINK): the mesh ballast shows only as a low crown between the rails, its square ends and
skirts stay under the ground, and the landscape paint is the ballast bed round it.

Layout: track S runs from the buffer stop (loading ramp) at the west end of the yard to the east edge; track N leaves S
at turnout T1, runs 6 m north of it through the yard and the neck and also reaches the edge; one crossover (turnouts
T2/T3) joins S to N in the neck. Straights are rigid panels (exact 10 m multiples, pitched along the bed); the turnout
legs and the curve are spline-mesh panels (one 10 m mesh bent per segment), so every joint shares position and heading.
"""
import math

import numpy as np

TRACK_MESH = "/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01"
# Berlin loading ramp + buffer stop (Source/Tools/BerlinMap/poi/rail_yard_target_import.py): local frame: track centre
# line at X = -260 along +Y, buffer stop face at Y = +775, side platform from X = -70 outward (+X), ground at z 0
RAMP_MESH = "/Game/RTS_Survival/Blueprints/Environment/Berlin/RailYardTarget/SM_Berlin_RailRamp"
RAMP_TRACK_X, RAMP_BUFFER_Y = -260.0, 775.0

PANEL = 1000.0                  # mesh length along +X
SPACING = 600.0                 # parallel track centres (the old 450 overlapped the 7.75 m mesh ballast)
SINK = -22.0                    # panel pivot under the bed: rail top 37 cm over the ground, ballast crown +6 cm
BRANCH_DIP = -1.0               # turnout legs 1 cm under the through track: no z-fighting where their sleepers overlap
TURNOUT_ALONG = 6000.0          # a turnout leg reaches the next track (6 m over) in 60 m: R 152 m, 11.4 degrees; each
                                # of its two arcs is 30.2 m = 3 panels stretched under 1 % (50 m legs squeezed them 16 %)
LANDSCAPE_EDGE_Y = 44100.0      # the east landscape edge (882 m landscape, centred)
BED_HALF = 480.0                # ballast paint at full weight out to here (the mesh ballast is 3.75-4 m a side)
BED_FADE = 300.0                # soft edge of the paint
BED_BLUR = 10                   # the track bed: dry ground box-blurred twice over 21 m (vertical kinks under 0.3 degree)
BED_LAND = 120.0                # ... averaging only ground over 1.2 m (no shore, no lake)

# design: the yard sits north of the metal_c resources (the west third of the site is inside W2's 70 m), the yard
# straight runs ESE through the neck between the s4 hill, the north shore and the Lubow dam end, then one 300 m curve
# turns it to almost due east and it runs past the play edge to the landscape edge
YARD_HEADING = 108.0
EAST_HEADING = 91.0
CURVE_R = 30000.0               # radius of S through the curve (N, on the inside, 294 m)
YARD_PANELS = 19                # S panels from the buffer stop to the curve (190 m: the yard and the neck)
T1_AT = 2000.0                  # turnout T1 toe on S (the lead to the ramp is 20 m: the ramp's platform and slope)
CROSS_AT = 10000.0              # crossover toe on S (T2); it lands on N at T3, 60 m on (both on 10 m joints)
BUFFER_ANCHOR = (7440.0, 12900.0)   # the buffer stop slides along YARD_HEADING from here so S ends at the landscape edge
                                    # (the ramp then stands on level ground; 10 m further west its end deck floated 37 cm)


# ---- plan elements --------------------------------------------------------------------------------------------------
def _dir(h):
    return np.array([math.cos(h), math.sin(h)])


def _left(h):
    """Unit normal to the left of heading h (radians): +90 degrees counter to the yaw direction (north of an east run)."""
    return np.array([math.sin(h), -math.cos(h)])


class Element:
    """A straight (k = 0) or circular arc of plan length L from p0 at heading h0 (radians). k > 0 turns left (the
    heading decreases)."""

    def __init__(self, p0, h0, k, length):
        self.p0, self.h0, self.k, self.L = np.asarray(p0, float), float(h0), float(k), float(length)

    def at(self, s):
        h = self.h0 - self.k * s
        if abs(self.k) < 1e-12:
            p = self.p0 + s * _dir(self.h0)
        else:
            p = self.p0 + (_left(self.h0) - _left(h)) / self.k
        return p, h

    def end(self):
        return self.at(self.L)

    def offset(self, d):
        """The parallel element d cm to the left (concentric arc for a curve)."""
        p0 = self.p0 + d * _left(self.h0)
        if abs(self.k) < 1e-12:
            return Element(p0, self.h0, 0.0, self.L)
        f = 1.0 - self.k * d
        return Element(p0, self.h0, self.k / f, self.L * f)


class Path:
    def __init__(self, elements):
        self.el = list(elements)

    @property
    def length(self):
        return sum(e.L for e in self.el)

    def at(self, s):
        for e in self.el:
            if s <= e.L + 1e-6:
                return e.at(min(max(s, 0.0), e.L))
            s -= e.L
        return self.el[-1].end()

    def end(self):
        return self.el[-1].end()

    def split_at(self, s):
        """(elements before s, elements after s): the element holding s is cut there."""
        before, after = [], []
        for e in self.el:
            if s >= e.L - 1e-6:
                before.append(e)
                s -= e.L
            elif s <= 1e-6:
                after.append(e)
            else:
                before.append(Element(e.p0, e.h0, e.k, s))
                p, h = e.at(s)
                after.append(Element(p, h, e.k, e.L - s))
                s = 0.0
        return before, after

    def sample(self, step=100.0):
        n = max(2, int(math.ceil(self.length / step)) + 1)
        return np.array([self.at(s)[0] for s in np.linspace(0.0, self.length, n)])


def s_curve(p0, h0, side, d=SPACING, along=TURNOUT_ALONG):
    """A turnout leg: from p0 / h0 it curves to the given side (+1 left, -1 right) and back, ending parallel d cm over
    and `along` cm further on. Two arcs of radius R and angle theta, tan(theta / 2) = d / along."""
    theta = 2.0 * math.atan(d / along)
    R = along / (2.0 * math.sin(theta))
    a = Element(p0, h0, side / R, R * theta)
    p1, h1 = a.end()
    b = Element(p1, h1, -side / R, R * theta)
    return [a, b]


# ---- the network ----------------------------------------------------------------------------------------------------
def _s_track(buffer_pt, n_east=None):
    h1, h2 = math.radians(YARD_HEADING), math.radians(EAST_HEADING)
    yard = Element(buffer_pt, h1, 0.0, YARD_PANELS * PANEL)
    p, h = yard.end()
    curve = Element(p, h, 1.0 / CURVE_R, CURVE_R * (h1 - h2))
    p, h = curve.end()
    if n_east is None:
        n_east = int(math.floor((LANDSCAPE_EDGE_Y - p[1]) / math.sin(h) / PANEL))
    east = Element(p, h, 0.0, n_east * PANEL)
    return [yard, curve, east]


def _buffer_point():
    """Slide the buffer stop along the yard heading so S's last panel ends on the landscape edge (the east run keeps
    its whole number of panels; the slide is under one panel)."""
    anchor = np.asarray(BUFFER_ANCHOR, float)
    els = _s_track(anchor)
    n_east = int(round(els[-1].L / PANEL))
    short = LANDSCAPE_EDGE_Y - els[-1].end()[0][1]                 # 0..10 m short of the edge
    slide = short / math.sin(math.radians(YARD_HEADING))
    buf = anchor + slide * _dir(math.radians(YARD_HEADING))
    return buf, n_east


_NET = {}


def network():
    """{"tracks": {name: {"path": Path, "branch": bool}}, "turnouts": [...], "buffer": (point, heading deg)}."""
    if _NET:
        return _NET
    buf, n_east = _buffer_point()
    s_path = Path(_s_track(buf, n_east))
    # N: turnout T1 off S at T1_AT, then S's own elements offset 6 m to the left from the end of the leg
    p, h = s_path.at(T1_AT)
    leg = s_curve(p, h, +1)
    _, rest = s_path.split_at(T1_AT + TURNOUT_ALONG)
    n_path = Path([e.offset(SPACING) for e in rest])
    # crossover: a leg from S at CROSS_AT to N, TURNOUT_ALONG on (it sits in the yard straight)
    p, h = s_path.at(CROSS_AT)
    cross = s_curve(p, h, +1)
    _NET.update({
        "tracks": {"S": {"path": s_path, "branch": False}, "N": {"path": n_path, "branch": False},
                   "N_T1": {"path": Path(leg), "branch": True}, "X_T2T3": {"path": Path(cross), "branch": True}},
        "turnouts": [
            {"id": "T1", "toe": s_path.at(T1_AT)[0], "through": "S", "leg": "N_T1", "joins": "N",
             "heel": n_path.at(0.0)[0]},
            {"id": "T2", "toe": s_path.at(CROSS_AT)[0], "through": "S", "leg": "X_T2T3", "joins": "N",
             "heel": Path(cross).end()[0]},
            {"id": "T3", "toe": Path(cross).end()[0], "through": "N", "leg": "X_T2T3", "joins": "S",
             "heel": s_path.at(CROSS_AT)[0]},
        ],
        "buffer": (buf, YARD_HEADING + 180.0),
    })
    return _NET


def yard_point():
    """Middle of the yard (centre of the worn cinder apron in the paint). The crater barrage in ow_dress_gen.decals()
    aims at the layout's rail_line[0], the buffer stop end of S."""
    return network()["tracks"]["S"]["path"].at(T1_AT + TURNOUT_ALONG)[0]


def rail_line():
    """S as a plain polyline every 10 m (layout key rail_line: [0] is the buffer stop end)."""
    return [[round(float(a), 1), round(float(b), 1)] for a, b in network()["tracks"]["S"]["path"].sample(PANEL)]


def centre_samples(step=50.0):
    return [t["path"].sample(step) for t in network()["tracks"].values()]


# ---- distance and paint on a grid -----------------------------------------------------------------------------------
def distance(X, Y, reach=3000.0):
    """Distance (cm) from every point of the X / Y arrays to the nearest track centre line; cells further than reach
    get reach. Each 50 cm sample pair is a segment; work in bounding-box windows so the full 883 x 883 grid is cheap."""
    X = np.asarray(X, float)
    Y = np.asarray(Y, float)
    d = np.full(X.shape, reach)
    for pts in centre_samples(50.0):
        for start in range(0, len(pts) - 1, 40):
            seg = pts[start:start + 41]
            lo, hi = seg.min(axis=0) - reach, seg.max(axis=0) + reach
            sel = (X >= lo[0]) & (X <= hi[0]) & (Y >= lo[1]) & (Y <= hi[1])
            if not sel.any():
                continue
            xs, ys = X[sel], Y[sel]
            best = d[sel]
            for (ax, ay), (bx, by) in zip(seg[:-1], seg[1:]):
                vx, vy = bx - ax, by - ay
                t = np.clip(((xs - ax) * vx + (ys - ay) * vy) / (vx * vx + vy * vy), 0.0, 1.0)
                best = np.minimum(best, np.hypot(xs - ax - t * vx, ys - ay - t * vy))
            d[sel] = best
    return d


def bed_weight(dist, wobble):
    """Ballast paint weight from the distance to the nearest track: full over the mesh ballast, then a 3 m soft edge
    broken up by the noise wobble (-1..1), so the bed never ends in a ruled line. Past a buffer stop the distance grows
    radially: the bed ends in a soft round cap, not a rectangle."""
    return _smooth(BED_HALF + BED_FADE, BED_HALF, dist + 90.0 * wobble)


def _smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


# ---- panels ---------------------------------------------------------------------------------------------------------
def _box(A, radius):
    for axis in (0, 1):
        pad = [(0, 0), (0, 0)]
        pad[axis] = (radius + 1, radius)
        p = np.pad(A, pad, mode="edge")
        c = np.cumsum(p, axis=axis)
        n = A.shape[axis]
        A = (np.take(c, np.arange(2 * radius + 1, 2 * radius + 1 + n), axis=axis) -
             np.take(c, np.arange(0, n), axis=axis)) / (2 * radius + 1)
    return A


def bed_sampler(height, corner=(-44100.0, -44100.0), quad=100.0, radius=None, land=None):
    """A smoothed copy of the height grid ([iy, ix], cm, as OW_masks.npz "height") and a bilinear sampler on it. The bed
    follows the ground's swells but not its 1 m lumps; every track reads the same field, so joints and turnouts share
    one height. Only dry land (over `land` cm) is averaged: a nearby shore or lake would pull the bed under the ground."""
    radius = BED_BLUR if radius is None else radius
    land = BED_LAND if land is None else land
    H = np.asarray(height, float)
    w = (H > land).astype(float)
    num, den = H * w, w
    for _ in range(2):
        num, den = _box(num, radius), _box(den, radius)
    H = np.where(den > 1e-3, num / np.maximum(den, 1e-3), H)
    vy, vx = H.shape

    def sample(x, y):
        fx = min(max((x - corner[0]) / quad, 0.0), vx - 1.001)
        fy = min(max((y - corner[1]) / quad, 0.0), vy - 1.001)
        ix, iy = int(math.floor(fx)), int(math.floor(fy))
        tx, ty = fx - ix, fy - iy
        return float(H[iy, ix] * (1 - tx) * (1 - ty) + H[iy, ix + 1] * tx * (1 - ty) +
                     H[iy + 1, ix] * (1 - tx) * ty + H[iy + 1, ix + 1] * tx * ty)

    return sample


def panels(bed):
    """Every track panel. bed(x, y) -> bed height (cm). Returns {track: [panel, ...]} in order along the track; a panel:
    kind "rigid" (straight 10 m mesh: start x, y, z, yaw, pitch) or "spline" (bent mesh: start / end position and
    tangent, world space). Straights whose length is a whole number of panels are rigid, everything else is spline."""
    out = {}
    for name, t in network()["tracks"].items():
        dip = BRANCH_DIP if t["branch"] else 0.0

        def z_at(p, dip=dip):
            return bed(p[0], p[1]) + SINK + dip

        items = []
        for e in t["path"].el:
            n_rigid = e.L / PANEL
            if abs(e.k) < 1e-12 and abs(n_rigid - round(n_rigid)) < 1e-3:
                for i in range(int(round(n_rigid))):
                    p0, h = e.at(i * PANEL)
                    p1, _ = e.at((i + 1) * PANEL)
                    z0, z1 = z_at(p0), z_at(p1)
                    items.append({"kind": "rigid", "p0": p0, "z0": z0, "yaw": math.degrees(h) % 360.0,
                                  "pitch": math.degrees(math.atan2(z1 - z0, PANEL)), "p1": p1, "z1": z1})
                continue
            n = max(1, int(round(e.L / PANEL)))
            ds = e.L / n
            for i in range(n):
                p0, h0 = e.at(i * ds)
                p1, h1 = e.at((i + 1) * ds)
                z0, z1 = z_at(p0), z_at(p1)
                # Hermite tangents of a circular arc: magnitude 4 R tan(phi / 4) (= the chord for a straight); the
                # vertical part from the bed slope at each end, so a bent panel meets its neighbours level with them
                phi = abs(e.k) * ds
                mag = ds if phi < 1e-9 else 4.0 * math.tan(phi / 4.0) / abs(e.k)
                g0 = (z_at(e.at(min(i * ds + 100.0, e.L))[0]) - z_at(e.at(max(i * ds - 100.0, 0.0))[0])) / \
                    (min(i * ds + 100.0, e.L) - max(i * ds - 100.0, 0.0))
                g1 = (z_at(e.at(min((i + 1) * ds + 100.0, e.L))[0]) - z_at(e.at(max((i + 1) * ds - 100.0, 0.0))[0])) / \
                    (min((i + 1) * ds + 100.0, e.L) - max((i + 1) * ds - 100.0, 0.0))
                items.append({"kind": "spline", "p0": p0, "z0": z0, "p1": p1, "z1": z1,
                              "t0": (mag * math.cos(h0), mag * math.sin(h0), mag * g0),
                              "t1": (mag * math.cos(h1), mag * math.sin(h1), mag * g1),
                              "yaw": math.degrees(h0) % 360.0, "yaw1": math.degrees(h1) % 360.0, "len": ds})
        out[name] = items
    return out


def ramp_transform():
    """(x, y, yaw) of the loading ramp actor: its track line on S, its buffer stop face at S's west end. The ramp's +X
    (its side platform) is then to the left of the run toward the buffer: the south side of the lead, toward the metal
    piles (N's turnout leg leaves to the north)."""
    buf, heading = network()["buffer"]
    yaw = heading - 90.0                                           # the ramp's +Y (toward its buffer) = west along S
    r = math.radians(yaw)
    lx, ly = RAMP_TRACK_X, RAMP_BUFFER_Y
    ox = lx * math.cos(r) - ly * math.sin(r)
    oy = lx * math.sin(r) + ly * math.cos(r)
    return buf[0] - ox, buf[1] - oy, yaw % 360.0
