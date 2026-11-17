"""Nine Werk (bunker position) concepts for Ger_OderWarthe, as sketch data.

Local frame in metres: +X = the Werk's front (the attacker's side), +Y = right, origin = the Werk centre.
The tunnel mouth stays where it is on the map: (-50, 0), opening to the back.
Each concept returns a heightfield over the 180 x 180 m sketch area, paint masks, placed objects and notes.
"""
import math
import numpy as np
from matplotlib.path import Path

EXT = 90.0
STEP = 0.5
AX = np.arange(-EXT, EXT + STEP / 2, STEP)
X, Y = np.meshgrid(AX, AX, indexing="xy")          # X[j, i] = AX[i] (front), Y[j, i] = AX[j] (right)
N = AX.size
PORTAL = (-50.0, 0.0)


# ----------------------------------------------------------------------------------------------------------- helpers
def smoothstep(a, b, v):
    t = np.clip((v - a) / (b - a), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def blur(a, r_m):
    """Box blur three times (about a gaussian) with radius r_m metres."""
    r = max(1, int(round(r_m / STEP)))
    out = a.astype(float)
    for _ in range(3):
        for axis in (0, 1):
            pad = np.pad(out, [(r, r) if k == axis else (0, 0) for k in (0, 1)], mode="edge")
            c = np.cumsum(pad, axis=axis)
            c = np.concatenate([np.zeros_like(np.take(c, [0], axis=axis)), c], axis=axis)
            hi = np.take(c, np.arange(2 * r + 1, c.shape[axis]), axis=axis)
            lo = np.take(c, np.arange(0, c.shape[axis] - 2 * r - 1), axis=axis)
            out = (hi - lo) / (2 * r + 1)
    return out


def seg_field(pts):
    """Distance to a polyline, the arc length of the nearest point, and the signed side (+ = left of travel)."""
    pts = np.asarray(pts, float)
    best = np.full(X.shape, 1e9)
    s_at = np.zeros(X.shape)
    side = np.zeros(X.shape)
    s0 = 0.0
    for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
        dx, dy = bx - ax, by - ay
        L = math.hypot(dx, dy)
        t = np.clip(((X - ax) * dx + (Y - ay) * dy) / (L * L), 0.0, 1.0)
        px, py = ax + t * dx, ay + t * dy
        d = np.hypot(X - px, Y - py)
        cross = dx * (Y - ay) - dy * (X - ax)
        m = d < best
        best = np.where(m, d, best)
        s_at = np.where(m, s0 + t * L, s_at)
        side = np.where(m, np.sign(cross), side)
        s0 += L
    return best, s_at, side, s0


def poly_sdf(poly):
    """Signed distance to a closed polygon, negative inside."""
    poly = list(poly)
    d, _, _, _ = seg_field(poly + [poly[0]])
    inside = Path(poly).contains_points(np.c_[X.ravel(), Y.ravel()]).reshape(X.shape)
    return np.where(inside, -d, d)


def rect(cx, cy, sx, sy, yaw=0.0):
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    out = []
    for ux, uy in ((sx, sy), (-sx, sy), (-sx, -sy), (sx, -sy)):
        ux, uy = ux / 2.0, uy / 2.0
        out.append((cx + ux * c - uy * s, cy + ux * s + uy * c))
    return out


def octagon(cx, cy, sx, sy, cut):
    hx, hy = sx / 2.0, sy / 2.0
    return [(cx + hx, cy - hy + cut), (cx + hx, cy + hy - cut), (cx + hx - cut, cy + hy), (cx - hx + cut, cy + hy),
            (cx - hx, cy + hy - cut), (cx - hx, cy - hy + cut), (cx - hx + cut, cy - hy), (cx + hx - cut, cy - hy)]


def mesa(sdf, height, slope_deg, round_m=1.5):
    """Flat top inside the outline, falling at slope_deg outside it."""
    run = height / math.tan(math.radians(slope_deg))
    z = height * np.clip(1.0 - np.maximum(sdf, 0.0) / run, 0.0, 1.0)
    return blur(z, round_m) if round_m else z


def relief(amp=0.35):
    return amp * (np.sin(X / 7.3) * np.cos(Y / 5.9) + 0.6 * np.sin((X + 1.7 * Y) / 4.1))


def arc_pts(cx, cy, r, a0, a1, n=40):
    return [(cx + r * math.cos(math.radians(a)), cy + r * math.sin(math.radians(a))) for a in np.linspace(a0, a1, n)]


class Werk:
    def __init__(self, key, title, name):
        self.key, self.title, self.name = key, title, name
        self.H = np.zeros(X.shape)
        self.paint = {"earth": np.zeros(X.shape), "gravel": np.zeros(X.shape), "mud": np.zeros(X.shape),
                      "sod": np.zeros(X.shape)}
        self.objs = []
        self.water = []                 # (polygon, z)
        self.hq = []                    # (x, y, label)
        self.notes = {}
        self.section = [(-90.0, 0.0), (90.0, 0.0)]
        self.walls = []                 # polylines of vertical faces, for the plan and the nav note
        self.hero = (0.0, 0.0, 0.0, 0.0)      # close-up view target: x, y, z, facing yaw
        self.protect = np.zeros(X.shape, bool)   # ground shaped for a bunker or kit piece: no relief over it
        self.seated = np.zeros(X.shape, bool)    # ground a bunker sits on: benches leave it alone

    # ----------------------------------------------------------------------------------------- ground
    def ground(self, x, y):
        i = (x + EXT) / STEP
        j = (y + EXT) / STEP
        i0, j0 = int(np.clip(math.floor(i), 0, N - 2)), int(np.clip(math.floor(j), 0, N - 2))
        fi, fj = i - i0, j - j0
        h = self.H
        return float((h[j0, i0] * (1 - fi) + h[j0, i0 + 1] * fi) * (1 - fj) + (h[j0 + 1, i0] * (1 - fi) + h[j0 + 1, i0 + 1] * fi) * fj)

    def ground_min(self, poly):
        return min(self.ground(x, y) for x, y in poly)

    # ----------------------------------------------------------------------------------------- objects
    def box(self, cx, cy, z0, sx, sy, sz, yaw=0.0, m="concrete"):
        self.objs.append({"t": "box", "c": [cx, cy], "z": z0, "s": [sx, sy, sz], "yaw": yaw, "m": m})

    def mod(self, kind, fx, fy, yaw, base=None, **p):
        """A German bunker module (see render.py MODULES). Origin: the front face centre at ground level
        (leitstand: the centre). Also adds footprint boxes for the plan and the section (skipped by the renderer)."""
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        z0 = (self.ground(fx + 2.0 * c, fy + 2.0 * s) if base is None else base) - 0.25
        self.objs.append({"t": "mod", "k": kind, "c": [fx, fy], "z": z0, "yaw": yaw, "p": p})
        W, D, H = p["W"], p["D"], p["H"]
        if kind == "m272":
            fp = [(-D / 2 + 0.9, 0.0, D + 1.8, W + 0.8, H)]
        elif kind == "regelbau":
            fp = [(-D / 2 + 0.2, 0.0, D + 0.4, W + 0.6, H + 0.9), (0.9, -p.get("pier", 1) * (W / 2 - 1.1), 3.4, 2.2, H + 1.7)]
        elif kind == "blockhaus":
            fp = [(-D / 2 + 0.3, 0.0, D + 0.6, W, H)]
        elif kind in ("leitstand", "tobruk"):
            fp = [(0.0, 0.0, D + 0.6, W, H)]
        elif kind == "peilturm":
            fp = [(-D / 2 + 0.45, 0.0, D + 0.9, W, H)]
            if p.get("annex"):
                fp.append((-D * 0.3 + 0.5, -W / 2 - 3.0, D * 0.6 + 1.0, 6.5, 0.32 * H))
        else:
            fp = [(-D / 2 + 0.4, 0.0, D + 0.8, W, H)]
        for lx, ly, sx, sy, sz in fp:
            self.objs.append({"t": "box", "c": [fx + lx * c - ly * s, fy + lx * s + ly * c], "z": z0, "s": [sx, sy, sz],
                              "yaw": yaw, "m": "concrete", "fp": True})
            self.seat(fx + lx * c - ly * s, fy + lx * s + ly * c, sx, sy, yaw, z0)
        if kind not in ("leitstand", "tobruk"):
            self.walls.append([(fx - s * W / 2, fy + c * W / 2), (fx + s * W / 2, fy - c * W / 2)])
            self.carve(fx, fy, yaw, W, z0, p)
        # door approaches: the ground in front of every door stays at the sill (the mesh's sill is 0.25 m over z0)
        if kind == "eingang":
            self.approach(fx, fy, yaw, 0.0, -1.2, z0 + 0.25)
        elif kind == "regelbau" and p.get("door"):
            self.approach(fx, fy, yaw, 0.0, -p.get("pier", 1) * W * 0.12, z0 + 0.25)
        elif kind == "leitstand":          # its door is in the back wall, through the earth bank
            self.approach(fx, fy, yaw + 180.0, D / 2.0, 0.0, z0 + 0.25, length=3.5, inside=1.6)
        return z0

    def approach(self, fx, fy, yaw, face, ly0, sill, length=2.5, half=1.3, inside=0.6):
        """Level ground at the sill in front of a door (face = the door wall's distance along yaw from (fx, fy)), with
        45 degree sides, so no door is buried."""
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        lx = (X - fx) * c + (Y - fy) * s - face
        ly = -(X - fx) * s + (Y - fy) * c - ly0
        dx = np.maximum(np.maximum(-inside - lx, lx - length), 0.0)
        dy = np.maximum(np.abs(ly) - half, 0.0)
        cap = sill - 0.08 + np.hypot(dx, dy)
        front = lx > -inside                   # only in front of the door wall: the bunker body covers the rest
        self.H = np.where(front & (cap < self.H), cap, self.H)
        self.protect |= front & (cap < 99.0) & (np.hypot(dx, dy) < 1.5)

    def seat(self, cx, cy, sx, sy, yaw, z0, margin=1.3):
        """No hollow under a bunker: inside its footprint (plus a margin) the ground comes up to 0.2 m over the concrete's
        buried foot (which runs 0.4 m under z0), so no landscape edge can show a gap at the wall."""
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        lx = (X - cx) * c + (Y - cy) * s
        ly = -(X - cx) * s + (Y - cy) * c
        inside = (np.abs(lx) < sx / 2 + margin) & (np.abs(ly) < sy / 2 + margin)
        self.H = np.where(inside, np.maximum(self.H, z0 - 0.2), self.H)
        self.protect |= (np.abs(lx) < sx / 2 + 2.0) & (np.abs(ly) < sy / 2 + 2.0)
        self.seated |= inside

    def bench(self, x, y, r, fade):
        """A level bench for a kit piece with a wide base (cupola apron, mast guys, a tooth): flat within r, blended out by
        fade (wide enough that the blend stays a gentle slope, or the tank navmesh breaks up on the glacis)."""
        level = self.ground(x, y)
        # the blend only as wide as the ground needs: about 15 degrees from the bench edge to the ground round it
        i0, i1 = int(max((x - r - 1.0 + EXT) / STEP, 0)), int(min((x + r + 1.0 + EXT) / STEP + 2, N))
        j0, j1 = int(max((y - r - 1.0 + EXT) / STEP, 0)), int(min((y + r + 1.0 + EXT) / STEP + 2, N))
        ring = np.hypot(X[j0:j1, i0:i1] - x, Y[j0:j1, i0:i1] - y) > r - 0.5
        dh = float(np.abs(self.H[j0:j1, i0:i1][ring] - level).max()) if ring.any() else 0.0
        fade = min(fade, r + max(1.5, dh / math.tan(math.radians(15.0))))
        i0, i1 = int(max((x - fade + EXT) / STEP, 0)), int(min((x + fade + EXT) / STEP + 2, N))
        j0, j1 = int(max((y - fade + EXT) / STEP, 0)), int(min((y + fade + EXT) / STEP + 2, N))
        d = np.hypot(X[j0:j1, i0:i1] - x, Y[j0:j1, i0:i1] - y)
        wgt = smoothstep(fade, r, d) * ~self.seated[j0:j1, i0:i1]
        self.H[j0:j1, i0:i1] = self.H[j0:j1, i0:i1] * (1.0 - wgt) + level * wgt
        self.protect[j0:j1, i0:i1] |= d < fade

    def carve(self, fx, fy, yaw, W, z0, p):
        """Keep the landscape out of a module's openings: nothing above the base in the 2.6 m behind its face
        (embrasures, doors and slots are up to 1.6 m deep), and for a 667 a sloped notch beside its side embrasure."""
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        lx = (X - fx) * c + (Y - fy) * s
        ly = -(X - fx) * s + (Y - fy) * c
        strip = (lx > -2.6) & (lx < 0.3) & (np.abs(ly) < W / 2 + 0.3)
        self.H = np.where(strip, np.minimum(self.H, z0 - 0.1), self.H)
        if p.get("side_emb"):
            sg = p.get("pier", 1)
            out = sg * ly - W / 2                                  # metres out from the embrasure's side wall
            d = np.maximum(np.maximum(np.abs(lx + 3.2) - 2.2, 0.0), np.maximum(-out, 0.0))
            zone = (out > -0.3) & (out < 16.0) & (lx > -14.0) & (lx < 6.0)
            self.protect |= zone
            # inside the body along the embrasure's side wall the ground stays low too: the 1 m landscape grid would
            # otherwise smear the earth cover over the roof down across the embrasure
            inner = (out > -2.6) & (out <= -0.3) & (lx > -p["D"] - 0.5) & (lx < 1.0)
            self.H = np.where(inner, np.minimum(self.H, z0 - 0.1), self.H)
            cap = z0 + np.maximum(np.abs(lx + 3.2) - 3.6, 0.0) * 0.58 + np.maximum(out - 7.0, 0.0) * 0.58
            self.H = np.where(zone, np.minimum(self.H, cap), self.H)

    def ear(self, a, b, h0, h1, base=None, th=0.9):
        """A wing wall (Fluegelmauer) from a (h0 high) to b (h1 high), sloping down into the ground."""
        z0 = (min(self.ground(*a), self.ground(*b)) if base is None else base) - 0.3
        self.objs.append({"t": "wedge", "a": list(a), "b": list(b), "z": z0, "h0": h0, "h1": h1, "th": th})
        self.walls.append([tuple(a), tuple(b)])

    def bank(self, cx, cy, yaw, W, D, top, slope=32.0, front_open=True, centred=False, cover_from=None, reach=None):
        """Bank earth against a structure up to absolute height top (the Atlantikwall dune look): sides and back,
        or all round. The footprint itself is left alone."""
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        lx = (X - cx) * c + (Y - cy) * s
        ly = -(X - cx) * s + (Y - cy) * c
        x0, x1 = (-D / 2, D / 2) if centred else (-D, 0.0)
        dx = np.maximum(np.maximum(x0 - lx, lx - x1), 0.0)
        dy = np.maximum(np.abs(ly) - W / 2, 0.0)
        dist = np.hypot(dx, dy)
        raise_ = top - dist * math.tan(math.radians(slope))
        m = (dist > 0.2)
        if reach is not None:
            m &= dist < reach
        if front_open:
            m &= lx < x1 - 0.5
        if cover_from is not None:
            # earth heaped over the roof behind cover_from (local x); the front lip stays bare concrete
            inside = (dist <= 0.2) & (lx < cover_from)
            ramp = top - np.maximum(lx - (cover_from - 2.0), 0.0) * 0.6
            m = m | inside
            raise_ = np.where(inside, ramp, raise_)
            self.paint["sod"] += 0.8 * inside
        self.H = np.where(m, np.maximum(self.H, raise_), self.H)
        self.paint["sod"] += 0.5 * ((dist > 0.2) & (dist < 6.0) & (raise_ > self.H - 0.01))

    def wall(self, pts, height, thick=1.2, base=None, m="concrete", top=None):
        """Straight wall segments along a polyline (a revetment or anti-tank wall)."""
        for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
            L = math.hypot(bx - ax, by - ay)
            yaw = math.degrees(math.atan2(by - ay, bx - ax))
            cx, cy = (ax + bx) / 2, (ay + by) / 2
            z0 = (min(self.ground(ax, ay), self.ground(bx, by)) if base is None else base) - 0.4
            hz = (top - z0) if top is not None else height
            self.box(cx, cy, z0, L + thick * 0.5, thick, hz, yaw, m)
        self.walls.append(list(pts))

    def kit(self, k, x, y, yaw=0.0, dz=0.0, scale=1.0):
        self.objs.append({"t": "kit", "k": k, "c": [x, y], "z": self.ground(x, y) + dz, "yaw": yaw, "s": scale})

    def cupola(self, x, y):
        self.bench(x, y, 5.6, 11.6)
        self.kit("cupola", x, y, 0.0, -0.4)

    def mast(self, x, y, yaw=0.0):
        self.bench(x, y, 6.6, 12.6)
        self.kit("mast", x, y, yaw)

    def portal(self, yaw=180.0, x=PORTAL[0], y=PORTAL[1], dz=0.0):
        self.kit("portal", x, y, yaw, dz)

    def teeth(self, pts, rows=4, spacing=2.6, row_gap=2.2, gaps=()):
        """Four staggered rows along a polyline (rows go outward = to the right of travel), minus the vehicle gaps
        (arc-length ranges)."""
        pts = np.asarray(pts, float)
        seglen = np.hypot(np.diff(pts[:, 0]), np.diff(pts[:, 1]))
        total = seglen.sum()
        k = 0
        for r in range(rows):
            s = (spacing / 2.0 if r % 2 else 0.0)
            while s < total:
                if not any(a <= s <= b for a, b in gaps):
                    i = int(np.searchsorted(np.cumsum(seglen), s, side="right"))
                    i = min(i, len(seglen) - 1)
                    s_in = s - (np.cumsum(seglen)[i] - seglen[i])
                    (ax, ay), (bx, by) = pts[i], pts[i + 1]
                    ux, uy = (bx - ax) / seglen[i], (by - ay) / seglen[i]
                    nx, ny = uy, -ux                                   # right of travel
                    px = ax + ux * s_in + nx * (r * row_gap)
                    py = ay + uy * s_in + ny * (r * row_gap)
                    self.bench(px, py, 0.75, 2.2)            # a level pad: a tooth on a slope would hover or sink
                    self.kit("toothA" if k % 3 else "toothB", px, py, (k * 37) % 90, -0.15)
                    k += 1
                s += spacing
        self.teeth_line = [list(map(float, p)) for p in pts]
        self.teeth_gaps = list(gaps)

    def scale_units(self, x, y, yaw=180.0):
        """A Panzer IV sized tank and a 6-man squad beside it, for scale."""
        self.objs.append({"t": "tank", "c": [x, y], "z": self.ground(x, y), "yaw": yaw})
        for k in range(6):
            px, py = x + 2.0 + (k % 3) * 1.4, y + 6.0 + (k // 3) * 1.6
            self.objs.append({"t": "man", "c": [px, py], "z": self.ground(px, py)})

    def finish(self):
        keep = 1.0 - np.clip(blur(self.protect.astype(float), 1.0) * 2.0, 0.0, 1.0)
        self.H = self.H + relief() * smoothstep(0.0, 1.5, self.H) * keep
        return self


def hill_union(*zs):
    out = zs[0]
    for z in zs[1:]:
        out = np.maximum(out, z)
    return out


def notch(H, poly, floor, slope_deg=45.0):
    """Cut a forecourt into a hill: inside poly the ground drops to floor, the cut sides at slope_deg."""
    sdf = poly_sdf(poly)
    cap = floor + np.maximum(sdf, 0.0) * math.tan(math.radians(slope_deg))
    return np.minimum(H, np.maximum(cap, floor))


# ----------------------------------------------------------------------------------------------------------- concepts
# German bunker modules used below (built in render.py):
#   m272      Longues-type gun casemate: stepped plinth, rounded "loaf" body, a dark gun slot under a roof slab that
#             cantilevers forward (Schartendecke).
#   regelbau  Regelbau casemate: hard-edged body, thick roof, a stepped flank pier projecting forward, a round MG
#             embrasure, steel door, a wing wall sloping into the earth on the other side.
#   blockhaus Monolith (Eperlecques/Watten): sheer board-form walls in projecting panels, small square ports.
#   leitstand Fire-control post: pillowed plinth, a dark observation slit band, an overhanging pillow tier, flat roof
#             plate, rangefinder block at the back.
def c1_walruecken():
    w = Werk("1", "Walrücken", "Whaleback ridge with three Longues-type casemates and a Leitstand")
    crest = [(6.0, -58.0), (13.0, -30.0), (15.0, 0.0), (13.0, 30.0), (6.0, 58.0)]
    d, s, side, L = seg_field(crest)
    front = side <= 0
    hc = 11.5 * smoothstep(0.0, 22.0, s) * smoothstep(0.0, 22.0, L - s)
    fall = np.where(front, np.tan(math.radians(31.0)), np.tan(math.radians(17.0)))
    w.H = blur(np.clip(hc - np.maximum(d - 3.0, 0.0) * fall, 0.0, None), 1.5)
    bays = [(-30.0, 22.0, 13.0, 6.5), (0.0, 25.0, 17.0, 7.5), (30.0, 22.0, 13.0, 6.5)]
    for yc, fx, wid, hh in bays:
        w.H = notch(w.H, rect(fx + 5.0, yc, 10.0, wid + 5.0), 0.3, 42.0)
        w.paint["gravel"] += (poly_sdf(rect(fx + 5.0, yc, 10.0, wid + 5.0)) < 1.0)
    w.paint["sod"] += smoothstep(4.0, 9.0, w.H)
    w.bank(2.0, 0.0, 0.0, 10.0, 9.0, w.ground(2.0, 0.0) + 2.6, 30.0, front_open=False, centred=True)
    for yc, fx, wid, hh in bays:
        w.mod("m272", fx, yc, 0.0, base=0.3, W=wid, D=12.0, H=hh, n=2 if yc == 0 else 1, door=(yc == 0))
        for sg in (-1.0, 1.0):
            w.ear((fx + 0.4, yc + sg * (wid / 2 + 1.0)), (fx + 7.5, yc + sg * (wid / 2 + 3.0)), 4.0, 0.6, base=0.3)
    w.mod("leitstand", 2.0, 0.0, 0.0, base=w.ground(2.0, 0.0) - 0.6, W=10.0, D=9.0, H=7.5)
    for yc in (-46.0, -16.0, 16.0, 46.0):
        w.cupola(6.0 + 9.0 * (1 - (abs(yc) / 58.0) ** 2) - 1.0, yc)
    w.teeth([(36.0, -66.0), (44.0, -36.0), (46.0, 0.0), (44.0, 36.0), (36.0, 66.0)], gaps=[(78.0, 88.0)])
    w.kit("mast", -16.0, 22.0)
    w.portal()
    w.paint["gravel"] += (seg_field([(-50.0, 0.0), (-30.0, 8.0), (-8.0, 4.0)])[0] < 2.5)
    w.hq = [(25.0, 0.0, "command entrance (centre casemate door)"), (-16.0, 22.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (25.0, 0.0, 0.3, 0.0)
    w.scale_units(58.0, -22.0)
    w.notes = {
        "form": "One long ridge across the front, 116 x 46 m, 11.5 m high: 31 degrees to the front, 17 to the back. Reads as a natural esker with three concrete faces cut into it.",
        "bunker": "Three Longues-type gun casemates (rounded body, gun slot under a cantilevered roof slab, stepped plinth) in forecourts with sloping wing walls; a two-tier Leitstand with its slit band on the crest, earth banked round it; four cupolas.",
        "block buried": "Yes: every casemate body sits inside the ridge, only the faces and roof lips show.",
        "nav": "Top reached from the back slope (17 deg) and the ridge ends; earth between the casemates is 31 deg (climbable); faces and wing walls need blockers.",
        "per site": "Wide across, shallow front to back: the crest polyline can bend to follow each neck.",
    }
    return w.finish()


def c2_sichel():
    w = Werk("2", "Sichel", "Crescent rampart: Regelbau casemates at the apex and the horns, Leitstand in the court")
    crest = arc_pts(-34.0, 0.0, 44.0, -72.0, 72.0, 60)
    d, s, side, L = seg_field(crest)
    r = np.hypot(X + 34.0, Y)
    outer = r >= 44.0
    hc = 10.0 * (0.62 + 0.38 * smoothstep(0.0, 40.0, s) * smoothstep(0.0, 40.0, L - s))
    endfade = smoothstep(-4.0, 6.0, np.minimum(s, L - s))
    fall = np.where(outer, np.tan(math.radians(29.0)), np.tan(math.radians(24.0)))
    w.H = blur(np.clip(hc - np.maximum(d - 2.5, 0.0) * fall, 0.0, None) * endfade, 1.4)
    court = (r < 30.0) & (X < 0.0)
    w.paint["gravel"] += 0.55 * blur(court.astype(float), 3.0)
    w.paint["sod"] += smoothstep(4.0, 8.0, w.H)
    w.H = notch(w.H, rect(16.0, 0.0, 9.0, 24.0), 0.3, 45.0)
    horns = []
    for sg in (-1.0, 1.0):
        a = math.radians(72.0 * sg)
        hx, hy = -34.0 + 44.0 * math.cos(a), 44.0 * math.sin(a)
        yaw = 65.0 * sg
        fx, fy = hx + 6.0 * math.cos(math.radians(yaw)), hy + 6.0 * math.sin(math.radians(yaw))
        horns.append((fx, fy, yaw, sg))
    w.bank(-28.0, -14.0, 0.0, 9.0, 9.0, 2.2, 30.0, front_open=False, centred=True)
    w.mod("regelbau", 12.5, 0.0, 0.0, base=0.3, W=18.0, D=11.0, H=6.0, pier=-1, ear=True, door=True, n=2)
    for fx, fy, yaw, sg in horns:
        w.mod("regelbau", fx, fy, yaw, base=0.3, W=10.0, D=8.0, H=5.0, pier=-sg, ear=True, door=False, n=1)
    w.mod("leitstand", -28.0, -14.0, 0.0, base=0.0, W=9.0, D=9.0, H=7.0)
    for a in (-48.0, -22.0, 22.0, 48.0):
        w.cupola(-34.0 + 43.0 * math.cos(math.radians(a)), 43.0 * math.sin(math.radians(a)))
    w.teeth(arc_pts(-34.0, 0.0, 70.0, -62.0, 62.0, 50), gaps=[(98.0, 108.0)])
    w.kit("mast", -36.0, 14.0)
    w.portal()
    w.hq = [(12.5, 0.0, "command entrance (apex casemate)"), (-36.0, 14.0, "transmitter (in the court)"), (-50.0, 0.0, "rail entrance")]
    w.hero = (12.5, 0.0, 0.3, 0.0, 9.0, -7.0, 1.6, 4.0)
    w.scale_units(48.0, 30.0)
    w.notes = {
        "form": "A crescent rampart, 10 m high, horns sweeping back round an open court; 88 x 96 m. 29 deg outside, 24 deg inside.",
        "bunker": "Apex Regelbau casemate (18 m face, stepped flank pier, round MG ports, steel door, sloping wing wall); two smaller Regelbau flanking casemates at the horn tips fire along the teeth. A Leitstand stands half-buried in the court. Four cupolas on the crest.",
        "block buried": "Yes, the casemate bodies sit in the rampart.",
        "nav": "Court open to the back: waves leave the tunnel into a sheltered yard. Crest reached from the court (24 deg) and the horn ends.",
        "per site": "The arc radius and horn length can vary per site without changing the kit.",
    }
    return w.finish()


def c3_terrassen():
    w = Werk("3", "Terrassenwerk", "Stepped concrete terraces, Regelbau casemates in the lowest wall, a big Leitstand on top")
    levels = [(octagon(-2.0, 0.0, 66.0, 60.0, 14.0), 4.0), (octagon(-6.0, 0.0, 48.0, 44.0, 10.0), 4.0),
              (octagon(-10.0, 0.0, 30.0, 28.0, 6.0), 4.0)]
    z = np.zeros(X.shape)
    for poly, h in levels:
        sdf = poly_sdf(poly)
        back = smoothstep(-6.0, -22.0, X)
        run = 0.05 + back * h / math.tan(math.radians(26.0))
        z += h * np.clip(1.0 - np.maximum(sdf, 0.0) / run, 0.0, 1.0)
    w.H = z
    w.paint["gravel"] += smoothstep(-0.5, 0.5, -poly_sdf(octagon(-10.0, 0.0, 30.0, 28.0, 6.0)))
    w.paint["sod"] += 0.6 * (z > 3.5)
    for (poly, h), top in zip(levels, (4.0, 8.0, 12.0)):
        ring = poly + [poly[0]]
        for a, b in zip(ring[:-1], ring[1:]):
            if (a[0] + b[0]) / 2.0 > -6.0:
                w.wall([a, b], 0.0, thick=1.4, base=top - h, top=top + 0.6)
    w.mod("leitstand", -8.0, 0.0, 0.0, base=12.0, W=15.0, D=14.0, H=10.0)
    for sg in (-1.0, 1.0):
        w.mod("regelbau", 31.8, sg * 13.0, 0.0, base=0.0, W=9.0, D=7.0, H=3.4, pier=-sg, ear=False, door=False, n=1)
    for x, y in ((-20.0, -10.0), (-20.0, 10.0), (14.0, -14.0), (14.0, 14.0), (27.0, 0.0), (27.0, 22.0), (27.0, -22.0)):
        w.cupola(x, y)
    w.teeth([(30.0, -46.0), (46.0, -24.0), (46.0, 24.0), (30.0, 46.0)], gaps=[(62.0, 71.0)])
    w.kit("mast", -26.0, 0.0, 0.0)
    w.portal()
    w.paint["gravel"] += (seg_field([(-50.0, 0.0), (-38.0, 12.0)])[0] < 2.5)
    w.hq = [(-0.5, 0.0, "command entrance (Leitstand door)"), (-26.0, 0.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (31.8, 0.0, 0.0, 0.0)
    w.scale_units(60.0, -10.0)
    w.notes = {
        "form": "An octagonal stepped mound, 66 x 60 m: three 4 m terraces faced with concrete at the front and sides, earth slopes (26 deg) at the back; 12 m top.",
        "bunker": "A large Leitstand (pillowed plinth, slit band, overhanging tier, rangefinder block) on the top, 22 m over the ground; two Regelbau casemates set in the lowest revetment; seven cupolas on the terraces.",
        "block buried": "The Leitstand stands proud on purpose; the terraces bury the rest.",
        "nav": "Revetments are walls (blockers along each); access only up the back slopes. Each terrace is a fighting ledge.",
        "per site": "Rigid geometry: best as an identical stamp.",
    }
    return w.finish()


def c4_stirnwand():
    w = Werk("4", "Stirnwand", "Wedge hill ending in a 16 m Eperlecques-style monolith face over a ditch")
    poly = [(22.0, -28.0), (22.0, 28.0), (-34.0, 20.0), (-34.0, -20.0)]
    sdf = poly_sdf(poly)
    ramp = 12.0 * smoothstep(-36.0, 20.0, X)
    side = np.clip(1.0 - np.maximum(sdf, 0.0) / (ramp / math.tan(math.radians(30.0)) + 1e-3), 0.0, 1.0)
    z = np.where(X > 22.0, 0.0, ramp * side)
    w.H = blur(z, 0.8)
    ditch = poly_sdf(rect(28.5, 0.0, 8.0, 76.0))
    w.H = w.H - 3.2 * smoothstep(1.5, -1.0, ditch)
    w.paint["mud"] += smoothstep(1.0, -1.0, ditch)
    w.paint["gravel"] += smoothstep(-20.0, 10.0, X) * (sdf < -2.0) * 0.5
    w.mod("blockhaus", 24.3, 0.0, 0.0, base=-3.0, W=60.0, D=12.0, H=16.4, rows=2, cols=7, panels=7, door=True, lip=1.4, stepped=True)
    w.wall([(33.0, -38.0), (33.0, 38.0)], 0.0, thick=1.0, base=-3.2, top=0.3)
    for x, y in ((12.0, -18.0), (12.0, 0.0), (12.0, 18.0), (-4.0, -9.0), (-4.0, 9.0)):
        w.cupola(x, y)
    w.teeth([(38.5, -44.0), (40.5, 0.0), (38.5, 44.0)], gaps=[(70.0, 80.0)])
    w.kit("mast", -24.0, -8.0)
    w.portal()
    w.hq = [(24.5, 0.0, "command entrance (gate on the ditch floor)"), (-24.0, -8.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (24.3, 0.0, -3.0, 0.0, 26.0, -22.0, 3.0, 9.0)
    w.scale_units(56.0, 26.0)
    w.notes = {
        "form": "A wedge, 56 m wide, rising from the back (12 deg) to the top of a sheer concrete face; 30 deg earth sides. A 3 m anti-tank ditch with a concrete counterscarp at the foot.",
        "bunker": "One monolith front in the Eperlecques style: 60 m of board-form concrete in projecting panels, 16 m from the ditch floor, two rows of small square ports and an armoured gate at the bottom. Five cupolas on the top.",
        "block buried": "Yes: the whole block is the hill.",
        "nav": "The face and ditch walls are blockers: the top is reached only up the back ramp, so attackers must go round. The ditch floor is a dead end (or a gate lane in the assault).",
        "per site": "Identical stamp; the face is the most intimidating thing from the gameplay camera.",
    }
    return w.finish()


def c5_torwerk():
    w = Werk("5", "Torwerk", "Twin knolls round a cutting; a monolith gatehouse; Longues-type casemates")
    zs = []
    for sg in (-1.0, 1.0):
        poly = [(22.0, sg * 8.0), (22.0, sg * 30.0), (6.0, sg * 38.0), (-34.0, sg * 34.0), (-40.0, sg * 8.0)]
        zs.append(mesa(poly_sdf(poly if sg > 0 else poly[::-1]), 10.0, 28.0, 1.2))
    z = hill_union(*zs)
    cut = poly_sdf(rect(-12.0, 0.0, 70.0, 16.0))
    z = np.where(cut < 0.0, -3.0 * smoothstep(22.0, -38.0, X), z)
    for sg in (-1.0, 1.0):
        z = notch(z, rect(30.0, sg * 21.0, 12.0, 19.0), 0.3, 45.0)
    w.H = z
    w.paint["gravel"] += (cut < 0.5)
    w.paint["sod"] += smoothstep(5.0, 9.0, z)
    for sg in (-1.0, 1.0):
        w.wall([(20.0, sg * 8.0), (-40.0, sg * 8.0)], 0.0, thick=1.4, base=-3.4, top=10.6)
        for k in range(4):
            xk = 14.0 - k * 14.0
            w.box(xk, sg * 7.2, 3.0 - 3.0 * float(smoothstep(22.0, -38.0, np.array(xk))), 1.6, 0.6, 0.8, 0.0, "void")
        w.mod("m272", 25.0, sg * 21.0, 0.0, base=0.3, W=14.0, D=12.0, H=7.0, n=1, door=False)
        w.ear((25.4, sg * 29.0), (31.5, sg * 31.0), 3.6, 0.6, base=0.3)
        w.mod("blockhaus", 25.0, sg * 11.0, 0.0, base=0.0, W=6.0, D=7.0, H=13.0, rows=3, cols=1, panels=1, door=False)
        for x in (10.0, -18.0):
            w.cupola(x, sg * 22.0)
    w.box(22.5, 0.0, 10.5, 5.0, 17.0, 2.0, 0.0, "concrete")
    w.portal(yaw=0.0, x=-46.0, y=0.0, dz=-0.2)
    w.teeth([(42.0, -48.0), (44.0, 0.0), (42.0, 48.0)], gaps=[(43.0, 53.0)])
    w.kit("mast", -26.0, 22.0)
    w.hq = [(-46.0, 0.0, "rail entrance (end of the cutting)"), (25.0, -21.0, "command entrance (left casemate)"), (-26.0, 22.0, "transmitter")]
    w.hero = (25.0, 0.0, 0.0, 0.0, 18.0, -1.0, 1.8, 6.0)
    w.scale_units(60.0, -30.0)
    w.notes = {
        "form": "Two angular knolls, 10 m high, 70 x 30 m each, either side of a 16 m cutting that falls 3 m to the tunnel mouth.",
        "bunker": "A gatehouse of two 13 m monolith towers (small square ports, Eperlecques style) joined by a roof slab over the cutting mouth; two Longues-type casemates in the knoll noses; four cupolas; firing ports in both cutting walls.",
        "block buried": "Yes, in each knoll; the gate towers stand proud.",
        "nav": "The tunnel opens INTO the cutting (portal turned to face the front): waves pour out through the gate. The cutting walls are blockers; knoll tops from the back slopes (28 deg).",
        "per site": "Identical stamp; the gap in the teeth lines up with the gate, so tanks must enter the kill zone.",
    }
    return w.finish()


def c6_ringwerk():
    w = Werk("6", "Ringwerk", "Horseshoe rampart round a sunken court with a Leitstand keep")
    crest = arc_pts(0.0, 0.0, 32.0, -138.0, 138.0, 80)
    d, s, side, L = seg_field(crest)
    r = np.hypot(X, Y)
    outer = r >= 32.0
    fall = np.where(outer, np.tan(math.radians(30.0)), np.tan(math.radians(34.0)))
    endfade = smoothstep(-6.0, 8.0, np.minimum(s, L - s))
    z = np.clip(9.0 - np.maximum(d - 2.0, 0.0) * fall, 0.0, None) * endfade
    court = smoothstep(22.0, 18.0, r) * (1.0 - smoothstep(-20.0, -30.0, X))
    w.H = blur(z - 1.8 * court, 1.2)
    w.paint["gravel"] += court
    w.paint["sod"] += smoothstep(4.0, 8.0, w.H)
    w.bank(2.0, 0.0, 0.0, 17.0, 16.0, 0.6, 30.0, front_open=False, centred=True)
    zk = w.mod("leitstand", 2.0, 0.0, 0.0, base=-1.8, W=17.0, D=16.0, H=9.5)
    w.objs.append({"t": "kit", "k": "cupola", "c": [6.0, 0.0], "z": zk + 0.88 * 9.5, "yaw": 0.0, "s": 1.0})
    for a in (-100.0, -50.0, 0.0, 50.0, 100.0):
        w.cupola(31.0 * math.cos(math.radians(a)), 31.0 * math.sin(math.radians(a)))
    for sg in (-1.0, 1.0):
        a = math.radians(138.0 * sg)
        hx, hy = 32.0 * math.cos(a), 32.0 * math.sin(a)
        w.mod("regelbau", hx - 4.0, hy + sg * 3.0, 180.0 + 40.0 * sg, base=0.3, W=10.0, D=8.0, H=5.0, pier=sg, ear=True, door=True, n=1)
    w.teeth(arc_pts(0.0, 0.0, 52.0, -112.0, 112.0, 70), gaps=[(56.0, 66.0)])
    w.kit("mast", -14.0, 12.0)
    w.portal()
    w.hq = [(-6.0, 0.0, "command entrance (keep door)"), (-14.0, 12.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (-7.0, 0.0, -1.8, 180.0, 18.0, 9.0, 4.0, 4.5)
    w.scale_units(64.0, 20.0)
    w.notes = {
        "form": "A horseshoe rampart, 9 m high, 86 m across, open to the back; inside, a court sunk 1.8 m. 30 deg outside, 34 deg inside.",
        "bunker": "A big Leitstand keep in the court (pillowed plinth, slit band, overhanging tier, rangefinder block, a cupola on the roof), earth banked to its plinth; five cupolas on the ring; two Regelbau gate casemates at the horn ends guard the opening.",
        "block buried": "No: the keep is shown, the ring shelters it.",
        "nav": "Vehicles only get in round the back through the opening by the tunnel; infantry can climb the ring (30 deg).",
        "per site": "Identical stamp; the ring reads clearly as a fort from the camera.",
    }
    return w.finish()


def c7_felsnase():
    w = Werk("7", "Felsnase", "Crag-and-tail: a high nose with a two-storey Longues-type casemate, a tail back to the tunnel")
    hx = np.clip((X + 46.0) / 58.0, 0.0, 1.0)
    height = 13.5 * np.sqrt(hx)
    half_w = 9.0 + 9.0 * hx
    d = np.maximum(np.abs(Y) - half_w * 0.55, 0.0)
    side = np.clip(1.0 - d / (height / math.tan(math.radians(30.0)) + 1e-3), 0.0, 1.0)
    z = height * side * (X <= 14.0)
    nose = mesa(poly_sdf([(14.0, -18.0), (14.0, 18.0), (2.0, 21.0), (2.0, -21.0)]), 13.5, 34.0, 1.0)
    w.H = blur(np.maximum(z, nose), 1.0)
    w.H = np.where(X > 13.0, np.minimum(w.H, 0.3 + np.maximum(np.abs(Y) - 15.0, 0.0) * 0.7 + np.maximum(13.0 - X, 0) * 9), w.H)
    w.paint["sod"] += smoothstep(5.0, 10.0, w.H)
    w.paint["gravel"] += poly_sdf(rect(20.0, 0.0, 10.0, 30.0)) < 0
    w.mod("m272", 15.0, 0.0, 0.0, base=0.3, W=28.0, D=14.0, H=11.0, n=3, door=True, storeys=2)
    for sg in (-1.0, 1.0):
        w.ear((15.4, sg * 14.6), (22.0, sg * 18.0), 5.0, 0.8, base=0.3)
    for x, y in ((4.0, -9.0), (4.0, 9.0), (-8.0, 0.0), (-22.0, -3.0), (-34.0, 2.0)):
        w.cupola(x, y)
    w.teeth(arc_pts(6.0, 0.0, 38.0, -84.0, 84.0, 60), gaps=[(20.0, 30.0)])
    w.kit("mast", -30.0, 16.0)
    w.portal()
    w.hq = [(16.0, 0.0, "command entrance (main gate)"), (-30.0, 16.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (15.0, 0.0, 0.3, 0.0)
    w.scale_units(52.0, 26.0)
    w.notes = {
        "form": "A glacial crag-and-tail: a 13.5 m blunt nose at the front, the tail tapering 60 m back to the tunnel mouth; 36 m wide at the nose, 18 m at the tail.",
        "bunker": "A two-storey Longues-type casemate in the nose: 28 m rounded front, a lower row of stepped ports and the main gate, the gun slot under a cantilevered roof slab; wing walls; five cupolas along the spine.",
        "block buried": "Yes: the casemate body is the nose.",
        "nav": "Spine reached from the tail (gentle); the nose face is a wall (blocker). Narrow: leaves the neck sides free for the wave lanes.",
        "per site": "The narrowest concept (36 m): fits the tight necks best; the tail can bend toward each tunnel.",
    }
    return w.finish()


def c8_werkgruppe():
    w = Werk("8", "Werkgruppe", "Four angular hillocks, each a different combat block, linked by trenches")
    hills = [
        ([(22.0, -14.0), (22.0, 14.0), (8.0, 20.0), (-10.0, 16.0), (-12.0, -12.0), (4.0, -20.0)], 10.0, 30.0),
        ([(2.0, 26.0), (-2.0, 44.0), (-20.0, 46.0), (-26.0, 30.0), (-12.0, 22.0)], 7.0, 30.0),
        ([(0.0, -26.0), (-8.0, -22.0), (-30.0, -38.0), (-24.0, -46.0), (-4.0, -42.0)], 7.0, 30.0),
        ([(-26.0, 0.0), (-28.0, 12.0), (-40.0, 12.0), (-42.0, -2.0), (-34.0, -6.0)], 5.0, 28.0),
    ]
    z = hill_union(*[mesa(poly_sdf(p), h, sl, 1.2) for p, h, sl in hills])
    trench_d = np.full(X.shape, 1e9)
    for line in ([(-2.0, 16.0), (-8.0, 24.0)], [(-6.0, -16.0), (-12.0, -28.0)], [(-12.0, 2.0), (-28.0, 4.0)],
                 [(-24.0, 14.0), (-28.0, 26.0)], [(-30.0, -6.0), (-20.0, -30.0)]):
        trench_d = np.minimum(trench_d, seg_field(line)[0])
    w.H = z - 2.0 * smoothstep(1.6, 0.8, trench_d)
    w.paint["mud"] += smoothstep(1.8, 1.0, trench_d)
    w.paint["gravel"] += smoothstep(10.0, 6.0, np.hypot(X + 18.0, Y - 4.0))
    w.paint["sod"] += smoothstep(4.0, 8.0, z)
    w.H = notch(w.H, rect(27.0, 0.0, 10.0, 22.0), 0.3, 45.0)
    w.mod("m272", 22.0, 0.0, 0.0, base=0.3, W=17.0, D=12.0, H=7.5, n=2, door=True)
    for sg in (-1.0, 1.0):
        w.ear((22.4, sg * 9.5), (29.0, sg * 11.5), 4.0, 0.6, base=0.3)
    w.mod("regelbau", 2.0, 38.0, 55.0, W=10.0, D=8.0, H=5.0, pier=-1, ear=True, door=False, n=1)
    w.mod("regelbau", -2.0, -40.0, -60.0, W=10.0, D=8.0, H=5.0, pier=1, ear=True, door=False, n=1)
    w.mod("leitstand", -35.0, 3.0, 0.0, base=w.ground(-35.0, 3.0) - 0.5, W=8.0, D=8.0, H=6.5)
    for x, y in ((8.0, -8.0), (8.0, 8.0), (-12.0, 36.0), (-16.0, -38.0)):
        w.cupola(x, y)
    w.teeth([(-24.0, -60.0), (20.0, -52.0), (44.0, -20.0), (46.0, 14.0), (26.0, 50.0), (-22.0, 62.0)], gaps=[(70.0, 80.0)])
    w.kit("mast", -18.0, 4.0)
    w.portal()
    w.hq = [(22.0, 0.0, "command entrance (main casemate)"), (-18.0, 4.0, "transmitter (yard)"), (-50.0, 0.0, "rail entrance")]
    w.hero = (22.0, 0.0, 0.3, 0.0, 19.0, -9.0, 3.0, 4.0)
    w.scale_units(62.0, -6.0)
    w.notes = {
        "form": "Not one hill but four: a 10 m main hill and three 5-7 m outworks, all angular, 74 x 108 m overall, linked by 2 m deep trenches round a central yard.",
        "bunker": "Longues-type gun casemate in the main hill, two Regelbau flanking casemates on the outworks, a small Leitstand on the rear hillock; four cupolas.",
        "block buried": "Yes, each block in its own hillock.",
        "nav": "Lanes between the hillocks are open (tanks can enter the yard through the gaps; infantry use the trenches). All slopes climbable (28-30 deg).",
        "per site": "Hillock positions can shift per site to fit each neck while the kit stays the same.",
    }
    return w.finish()


def c9_sperrlinie():
    w = Werk("9", "Turmlinie", "A long dyke with three monolith bunker towers behind a wet ditch")
    poly = [(10.0, -62.0), (10.0, 62.0), (-12.0, 58.0), (-12.0, -58.0)]
    sdf = poly_sdf(poly)
    z = mesa(sdf, 8.0, 26.0, 1.0)
    z = np.where((X > 0.0) & (sdf > 0), 8.0 * np.clip(1.0 - np.maximum(sdf, 0.0) / (8.0 / math.tan(math.radians(36.0))), 0, 1), z)
    ditch = poly_sdf(rect(25.0, 0.0, 9.0, 132.0))
    z = z - 3.0 * smoothstep(1.5, -1.0, ditch)
    cause = poly_sdf(rect(25.0, 30.0, 12.0, 9.0))
    z = np.where(cause < 0.0, np.maximum(z, 0.4), z)
    w.H = notch(blur(z, 0.6), rect(15.5, 30.0, 8.0, 13.0), 0.3, 45.0)
    w.water.append((rect(25.0, -16.0, 8.0, 101.0), -1.2))
    w.water.append((rect(25.0, 48.0, 8.0, 27.0), -1.2))
    w.paint["mud"] += smoothstep(2.0, -0.5, ditch)
    w.paint["gravel"] += (cause < 0.5) + (seg_field([(25.0, 30.0), (10.0, 30.0)])[0] < 3.0)
    w.paint["sod"] += smoothstep(3.0, 7.0, w.H)
    for y in (-42.0, 0.0, 42.0):
        zt = w.mod("blockhaus", 5.5, y, 0.0, base=7.5, W=13.0, D=13.0, H=8.5, rows=2, cols=2, door=False, sides=True, chamfer=3.0, lip=1.2)
        w.objs.append({"t": "kit", "k": "cupola", "c": [-1.0, y], "z": zt + 9.8, "yaw": 0.0, "s": 1.0})
    w.mod("regelbau", 11.5, 30.0, 0.0, base=0.3, W=9.0, D=8.0, H=5.0, pier=1, ear=True, door=True, n=1)
    for y in (-21.0, 21.0, -56.0, 56.0):
        w.cupola(0.0, y)
    w.teeth([(34.0, -70.0), (36.0, 0.0), (34.0, 70.0)], gaps=[(95.0, 106.0)])
    w.kit("mast", -24.0, -12.0)
    w.portal()
    w.hq = [(11.5, 30.0, "command entrance (causeway gate)"), (-24.0, -12.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (11.5, 30.0, 0.3, 0.0, 22.0, -7.0, 2.5, 6.0)
    w.scale_units(52.0, -26.0)
    w.notes = {
        "form": "A straight dyke, 124 x 22 m, 8 m high (36 deg front, 26 deg back) behind a 9 m wet ditch; one earth causeway crosses the ditch to a gate casemate.",
        "bunker": "Three monolith towers (board-form panels, small square ports on every side, a cupola on each roof) stand 8.5 m proud of the dyke, 16 m over the ground; a Regelbau gate casemate at the causeway; four more cupolas along the dyke.",
        "block buried": "The towers stand proud on purpose; their lower half is in the dyke.",
        "nav": "The ditch is a wall for tanks except at the causeway; the teeth gap lines up with the causeway. Dyke top from the back (26 deg).",
        "per site": "Longest concept (140 m across): needs a site that is wide across its front.",
    }
    return w.finish()


CONCEPTS = [c1_walruecken, c2_sichel, c3_terrassen, c4_stirnwand, c5_torwerk, c6_ringwerk, c7_felsnase,
            c8_werkgruppe, c9_sperrlinie]
