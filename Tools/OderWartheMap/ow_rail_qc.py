"""Offline quality check of the Ger_OderWarthe rail yard and its two tracks to the east edge (no editor).

    python Source/Tools/OderWartheMap/ow_rail_qc.py            # rail items, the bed formula over the written paint
    python Source/Tools/OderWartheMap/ow_rail_qc.py --paint    # + the nine layers rebuilt in memory (ow_terrain_gen
                                                               #   Gen, ~30 s, nothing written), checked and drawn

Runs the dressing generator in memory against the generated terrain files (Saved/OderWarthe/terrain: heights, masks,
layout; nothing is written there) and checks the rail items exactly as the map build will read them (rounded values,
not the ideal curves):
    joints        consecutive panels and every turnout toe / heel: 3D gap, plan heading and grade change
    seating       every panel against OW_height.raw (bilinear, as ow_map_build.ground_z): rails visible, sleepers not
                  buried, no ballast wall standing proud, the skirt never above the ground (floating)
    clearances    water, corridors (dams, causeways, ford, boardwalks, ramps), Werk hills and 70 m discs, resources,
                  stations, guards, outposts, tunnel mouths; other dressing on the track
    edge          S and N run to the landscape edge
    paint         the ballast bed (ow_rail.bed_weight) under every panel, soft edges
    vegetation    no tree, shrub, rock or crater left on the bed
Writes Saved/OderWarthe/rail_qc/rail_qc.txt and rail_qc.png (layout over heights and paint) + rail_qc_yard.png.
"""
import math
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ow_rail  # noqa: E402
import ow_dress_gen as dg  # noqa: E402

OUT = os.path.join(dg.ROOT, "Saved", "OderWarthe", "rail_qc")
LAY = dg.LAY
CORNER = LAY["corner"]
VX, VY = LAY["verts_x"], LAY["verts_y"]

# mesh cross-section (SM_RTS_Railway_Track_01, measured from its source FBX): (local y, local z)
RAIL_TOP = [(-92.5, 59.0), (92.5, 59.0)]
SLEEPER_END = [(-170.0, 33.0), (170.0, 33.0)]
RIM = [(-375.0, 17.0), (400.0, 17.0)]
SKIRT = [(-375.0, -93.0), (400.0, -93.0)]

LIMITS = {"joint_gap_cm": 2.0, "joint_heading_deg": 0.5, "joint_grade_deg": 0.5, "rail_clear_min_cm": 20.0,
          "sleeper_buried_max_cm": 5.0, "rim_proud_max_cm": 20.0, "water_ground_min_cm": 30.0, "water_dist_min_cm": 800.0,
          "road_dist_min_cm": 500.0, "werk_centre_min_cm": 7000.0, "resource_min_cm": 800.0, "station_min_cm": 2600.0,
          "enemy_min_cm": 800.0, "tunnel_min_cm": 3000.0, "edge_tol_cm": 100.0, "paint_under_min": 0.95}


# ---- terrain --------------------------------------------------------------------------------------------------------
def load_heights():
    raw = np.fromfile(LAYOUT_FILES["height"], dtype="<u2").astype(np.float64)
    return ((raw - 32768.0) * 100.0 / 128.0).reshape(VY, VX)      # [iy, ix]


LAYOUT_FILES = dg.LAYOUT["files"]
HRAW = None


def ground(x, y):
    """ow_map_build.ground_z: bilinear on OW_height.raw, clamped at the landscape edge."""
    fx = min(max((x - CORNER[0]) / LAY["quad_cm"], 0.0), VX - 1.001)
    fy = min(max((y - CORNER[1]) / LAY["quad_cm"], 0.0), VY - 1.001)
    ix, iy = int(math.floor(fx)), int(math.floor(fy))
    tx, ty = fx - ix, fy - iy
    H = HRAW
    return (H[iy, ix] * (1 - tx) * (1 - ty) + H[iy, ix + 1] * tx * (1 - ty) + H[iy + 1, ix] * (1 - tx) * ty +
            H[iy + 1, ix + 1] * tx * ty)


def mask(name, x, y):
    return dg.M[name][dg.cell(x, y)]


# ---- panels as built ------------------------------------------------------------------------------------------------
class Rigid:
    def __init__(self, item):
        x, y, z, yaw, scale, pitch, roll = item
        self.t = np.array([x, y, z], float)
        self.yaw, self.pitch = math.radians(yaw), math.radians(pitch)

    def world(self, lx, ly, lz):
        # UE Rotator(roll 0, pitch, yaw): positive pitch lifts +X toward +Z, then yaw turns +X toward +Y
        cp, sp, cy, sy = math.cos(self.pitch), math.sin(self.pitch), math.cos(self.yaw), math.sin(self.yaw)
        px, pz = lx * cp - lz * sp, lx * sp + lz * cp
        return self.t + np.array([px * cy - ly * sy, px * sy + ly * cy, pz])

    def ends(self):
        d = np.array([math.cos(self.pitch) * math.cos(self.yaw), math.cos(self.pitch) * math.sin(self.yaw), math.sin(self.pitch)])
        return self.world(0, 0, 0), d, self.world(ow_rail.PANEL, 0, 0), d


class Bent:
    def __init__(self, seg):
        v = np.asarray(seg, float)
        self.p0, self.t0, self.p1, self.t1 = v[0:3], v[3:6], v[6:9], v[9:12]

    def pos(self, t):
        h00, h10, h01, h11 = 2 * t ** 3 - 3 * t ** 2 + 1, t ** 3 - 2 * t ** 2 + t, -2 * t ** 3 + 3 * t ** 2, t ** 3 - t ** 2
        return h00 * self.p0 + h10 * self.t0 + h01 * self.p1 + h11 * self.t1

    def deriv(self, t):
        a, b, c, d = 6 * t ** 2 - 6 * t, 3 * t ** 2 - 4 * t + 1, -6 * t ** 2 + 6 * t, 3 * t ** 2 - 2 * t
        return a * self.p0 + b * self.t0 + c * self.p1 + d * self.t1

    def world(self, lx, ly, lz):
        # USplineMeshComponent: t from the mesh's X bounds (0..1000), X axis along the curve, Y = up x X, Z = X x Y
        t = lx / ow_rail.PANEL
        xa = self.deriv(t)
        xa = xa / np.linalg.norm(xa)
        ya = np.cross([0.0, 0.0, 1.0], xa)
        ya = ya / np.linalg.norm(ya)
        za = np.cross(xa, ya)
        return self.pos(t) + ly * ya + lz * za

    def ends(self):
        d0, d1 = self.deriv(0.0), self.deriv(1.0)
        return self.pos(0.0), d0 / np.linalg.norm(d0), self.pos(1.0), d1 / np.linalg.norm(d1)


def built_tracks():
    """{track: [panel objects in order]} from the emitted INST group and RAILS segments, ordered like ow_rail.panels."""
    rigid = list(dg.INST[dg.RAIL_GROUP]["items"])
    out = {}
    k = 0
    for name, items in ow_rail.panels(dg.RAIL_BED).items():
        bent = list(dg.RAILS["tracks"].get(name, []))
        seq = []
        for p in items:
            if p["kind"] == "rigid":
                seq.append(("rigid", Rigid(rigid[k])))
                k += 1
            else:
                seq.append(("spline", Bent(bent.pop(0))))
        out[name] = seq
    assert k == len(rigid), "rigid panel count mismatch"
    return out


def heading_deg(d):
    return math.degrees(math.atan2(d[1], d[0]))


def grade_deg(d):
    return math.degrees(math.atan2(d[2], math.hypot(d[0], d[1])))


def adiff(a, b):
    return abs((a - b + 180.0) % 360.0 - 180.0)


# ---- checks ---------------------------------------------------------------------------------------------------------
def check_joints(tracks, lines, fails):
    worst = {"gap": 0.0, "head": 0.0, "grade": 0.0}
    lines.append("JOINTS (consecutive panels; limits gap %.1f cm, heading %.1f deg, grade %.1f deg)" % (
        LIMITS["joint_gap_cm"], LIMITS["joint_heading_deg"], LIMITS["joint_grade_deg"]))
    for name, seq in tracks.items():
        g_max = h_max = gr_max = 0.0
        for (_, a), (_, b) in zip(seq[:-1], seq[1:]):
            _, _, e, de = a.ends()
            s, ds, _, _ = b.ends()
            gap = float(np.linalg.norm(e - s))
            hd = adiff(heading_deg(de), heading_deg(ds))
            gd = abs(grade_deg(de) - grade_deg(ds))
            g_max, h_max, gr_max = max(g_max, gap), max(h_max, hd), max(gr_max, gd)
        kinds = [k for k, _ in seq]
        lines.append("  %-7s %3d panels (%d rigid, %d spline): max gap %.2f cm, max heading step %.3f deg, max grade step %.3f deg" % (
            name, len(seq), kinds.count("rigid"), kinds.count("spline"), g_max, h_max, gr_max))
        worst["gap"], worst["head"], worst["grade"] = max(worst["gap"], g_max), max(worst["head"], h_max), max(worst["grade"], gr_max)
        if g_max > LIMITS["joint_gap_cm"] or h_max > LIMITS["joint_heading_deg"] or gr_max > LIMITS["joint_grade_deg"]:
            fails.append("joints on %s" % name)
    return worst


def nearest_joint(seq, p, which):
    best = (1e18, None)
    for _, panel in seq:
        s, ds, e, de = panel.ends()
        for q, d in ((s, ds), (e, de)):
            dd = float(np.linalg.norm(q[:2] - p[:2]))
            if dd < best[0]:
                best = (dd, (q, d))
    return best[1]


def check_turnouts(tracks, lines, fails):
    lines.append("TURNOUTS (leg toe on the through track, leg heel on the track it joins; the leg sits %.0f cm lower)" % -ow_rail.BRANCH_DIP)
    for t in ow_rail.network()["turnouts"]:
        leg = tracks[t["leg"]]
        toe_end = leg[0][1].ends()[:2] if t["id"] != "T3" else leg[-1][1].ends()[2:]
        q, d = nearest_joint(tracks[t["through"]], toe_end[0], "toe")
        gap = float(np.linalg.norm(toe_end[0] - q))
        plan = float(np.linalg.norm(toe_end[0][:2] - q[:2]))
        hd = adiff(heading_deg(toe_end[1]), heading_deg(d))
        heel = leg[-1][1].ends()[2:] if t["id"] != "T3" else leg[0][1].ends()[:2]
        q2, d2 = nearest_joint(tracks[t["joins"]], heel[0], "heel")
        gap2 = float(np.linalg.norm(heel[0] - q2))
        hd2 = adiff(heading_deg(heel[1]), heading_deg(d2))
        lines.append("  %s at (%.0f, %.0f): toe on %s gap %.2f cm (plan %.2f), heading %.3f deg; heel on %s gap %.2f cm, heading %.3f deg" % (
            t["id"], toe_end[0][0], toe_end[0][1], t["through"], gap, plan, hd, t["joins"], gap2, hd2))
        if gap > LIMITS["joint_gap_cm"] or gap2 > LIMITS["joint_gap_cm"] or hd > LIMITS["joint_heading_deg"] or hd2 > LIMITS["joint_heading_deg"]:
            fails.append("turnout %s" % t["id"])


def check_seating(tracks, lines, fails):
    lines.append("SEATING against OW_height.raw (every panel sampled every 1.25 m, both rails, sleeper ends, ballast rims, skirt)")
    stats = {"rail_min": 1e9, "sleeper_min": 1e9, "rim_max": -1e9, "float_max": -1e9, "panels": 0}
    worst_rows = []
    for name, seq in tracks.items():
        for idx, (_, panel) in enumerate(seq):
            r_min = s_min = 1e9
            rim_max = fl_max = -1e9
            for lx in np.arange(0.0, ow_rail.PANEL + 1.0, 125.0):
                for ly, lz in RAIL_TOP:
                    p = panel.world(lx, ly, lz)
                    r_min = min(r_min, p[2] - ground(p[0], p[1]))
                for ly, lz in SLEEPER_END:
                    p = panel.world(lx, ly, lz)
                    s_min = min(s_min, p[2] - ground(p[0], p[1]))
                for ly, lz in RIM:
                    p = panel.world(lx, ly, lz)
                    rim_max = max(rim_max, p[2] - ground(p[0], p[1]))
                for ly, lz in SKIRT:
                    p = panel.world(lx, ly, lz)
                    fl_max = max(fl_max, p[2] - ground(p[0], p[1]))
            stats["rail_min"] = min(stats["rail_min"], r_min)
            stats["sleeper_min"] = min(stats["sleeper_min"], s_min)
            stats["rim_max"] = max(stats["rim_max"], rim_max)
            stats["float_max"] = max(stats["float_max"], fl_max)
            stats["panels"] += 1
            bad = (r_min < LIMITS["rail_clear_min_cm"] or s_min < -LIMITS["sleeper_buried_max_cm"] or
                   rim_max > LIMITS["rim_proud_max_cm"] or fl_max > 0.0)
            if bad:
                p = panel.world(500.0, 0.0, 0.0)
                worst_rows.append("    %s #%d at (%.0f, %.0f): rail top %+.1f, sleeper end %+.1f, rim %+.1f, skirt bottom %+.1f cm" % (
                    name, idx, p[0], p[1], r_min, s_min, rim_max, fl_max))
    lines.append("  %d panels: rail top over ground min %+.1f cm (limit >= %.0f); sleeper ends over ground min %+.1f cm "
                 "(limit >= -%.0f); ballast rim over ground max %+.1f cm (limit <= %.0f); skirt bottom over ground max %+.1f cm "
                 "(limit <= 0: never floating)" % (stats["panels"], stats["rail_min"], LIMITS["rail_clear_min_cm"], stats["sleeper_min"],
                                                   LIMITS["sleeper_buried_max_cm"], stats["rim_max"], LIMITS["rim_proud_max_cm"], stats["float_max"]))
    if worst_rows:
        fails.append("seating (%d panels)" % len(worst_rows))
        lines.extend(worst_rows[:30])
    return stats


def footprint_points(seq, step=125.0):
    pts = []
    for _, panel in seq:
        for lx in np.arange(0.0, ow_rail.PANEL + 1.0, step):
            for ly in (-400.0, 0.0, 400.0):
                pts.append(panel.world(lx, ly, 0.0)[:2])
    return np.array(pts)


def water_cells():
    wy, wx = np.nonzero(dg.M["water"])
    return np.c_[wx * LAY["quad_cm"] + CORNER[0], wy * LAY["quad_cm"] + CORNER[1]]


def min_dist(pts, targets):
    best = 1e18
    for start in range(0, len(pts), 256):
        chunk = pts[start:start + 256]
        lo, hi = chunk.min(axis=0) - 5000.0, chunk.max(axis=0) + 5000.0
        sel = (targets[:, 0] >= lo[0]) & (targets[:, 0] <= hi[0]) & (targets[:, 1] >= lo[1]) & (targets[:, 1] <= hi[1])
        if not sel.any():
            continue
        t = targets[sel]
        d = np.hypot(chunk[:, None, 0] - t[None, :, 0], chunk[:, None, 1] - t[None, :, 1])
        best = min(best, float(d.min()))
    return best


def check_clearances(tracks, lines, fails):
    lines.append("CLEARANCES (track footprint = centre line and both 4 m ballast edges, every 1.25 m)")
    wc = water_cells()
    feats = {f["id"]: f for f in dg.LAYOUT["features"]}
    for name, seq in tracks.items():
        fp = footprint_points(seq)
        cl = np.array([panel.world(lx, 0.0, 0.0)[:2] for _, panel in seq for lx in np.arange(0.0, ow_rail.PANEL + 1.0, 125.0)])
        g_min = min(ground(x, y) for x, y in fp)
        wet = sum(bool(mask("water", x, y)) for x, y in fp)
        d_water = min_dist(fp, wc)
        road = min(float(mask("road_dist", x, y)) for x, y in fp)
        werk_cell = sum(bool(mask("werk_" + w, x, y)) for x, y in fp for w in dg.WERKS)
        d_werk = min(math.hypot(x - w["centre"][0], y - w["centre"][1]) for x, y in cl for w in dg.WERKS.values())
        d_tunnel = min(math.hypot(x - w["tunnel"][0], y - w["tunnel"][1]) for x, y in cl for w in dg.WERKS.values())
        lines.append("  %-7s ground min %.0f cm, footprint cells in water %d, nearest water %.1f m, nearest corridor edge %.1f m, "
                     "werk hill cells %d, nearest Werk centre %.1f m, nearest tunnel mouth %.1f m" % (
                         name, g_min, wet, d_water / 100.0, road / 100.0, werk_cell, d_werk / 100.0, d_tunnel / 100.0))
        if g_min < LIMITS["water_ground_min_cm"] or wet or d_water < LIMITS["water_dist_min_cm"]:
            fails.append("water near %s" % name)
        if road < LIMITS["road_dist_min_cm"]:
            fails.append("corridor near %s" % name)
        if werk_cell or d_werk < LIMITS["werk_centre_min_cm"]:
            fails.append("Werk near %s" % name)
        if d_tunnel < LIMITS["tunnel_min_cm"]:
            fails.append("tunnel mouth near %s" % name)
        near = []
        for f in dg.LAYOUT["features"]:
            kind = f["kind"]
            lim = {"resource": LIMITS["resource_min_cm"], "capture": LIMITS["station_min_cm"], "enemy_group": LIMITS["enemy_min_cm"]}.get(kind)
            if lim is None or f["id"].startswith("tunnel_"):
                continue
            for k, q in enumerate(f["points"]):
                d = min(math.hypot(x - q[0], y - q[1]) for x, y in cl)
                if d < lim + 2000.0:
                    near.append("%s[%d] %.1f m" % (f["id"], k, d / 100.0))
                if d < lim:
                    fails.append("%s too close to %s" % (f["id"], name))
        if near:
            lines.append("          near sites: " + ", ".join(near))
    _ = feats


def check_other_dressing(tracks, lines, fails):
    """Anything the dressing put within 4.5 m of a track centre line that is not rail (nature is checked separately)."""
    items = [(b[0], b[2], b[3]) for b in dg.BPS if not b[6].startswith("Trees/")] + [(m[0], m[2], m[3]) for m in dg.MESHES]
    for group, g in dg.INST.items():
        if group == dg.RAIL_GROUP or group.startswith(dg.NATURE_PREFIXES):
            continue
        items += [(group, it[0], it[1]) for it in g["items"]]
    if not items:
        return
    d = ow_rail.distance(np.array([i[1] for i in items]), np.array([i[2] for i in items]), 1000.0)
    on = [(lab, dd) for (lab, _, _), dd in zip(items, d) if dd < 450.0 and lab != "RailYard_Ramp"]
    lines.append("OTHER DRESSING within 4.5 m of a track centre line: %d %s" % (len(on), ["%s %.1f m" % (l, v / 100.0) for l, v in on][:12]))
    if on:
        fails.append("dressing on the track")


def check_stretch(tracks, lines, fails):
    """Bent panels squeeze or stretch the 10 m mesh (sleeper spacing): report the range per track."""
    parts = []
    worst = 0.0
    for name, seq in tracks.items():
        lens = [float(np.sum([np.linalg.norm(panel.pos(t1) - panel.pos(t0)) for t0, t1 in
                              zip(np.linspace(0, 1, 21)[:-1], np.linspace(0, 1, 21)[1:])])) for kind, panel in seq if kind == "spline"]
        if lens:
            parts.append("%s %.1f..%.1f %%" % (name, (min(lens) / ow_rail.PANEL - 1.0) * 100.0, (max(lens) / ow_rail.PANEL - 1.0) * 100.0))
            worst = max(worst, max(abs(v / ow_rail.PANEL - 1.0) for v in lens))
    lines.append("BENT PANELS length against the 10 m mesh (limit +-5 %%): %s" % ", ".join(parts))
    if worst > 0.05:
        fails.append("bent panels stretched %.0f %%" % (worst * 100.0))


def check_ramp(tracks, lines, fails):
    """The loading ramp (SM_Berlin_RailRamp; Blender report: pivot = footprint centre at ground level, track line at local
    X -260 along +Y, side platform 4.2 m wide from X -70 to +350 with props to +503, slope from Y -375 down to -1125,
    buffer face at Y +775, end deck 8 m across the track end to Y +1075): ground under the structure and clearance from
    every track but S (the platform face is 1.9 m off its own track)."""
    rx, ry, ryaw = ow_rail.ramp_transform()
    r = math.radians(ryaw)

    def w(lx, ly):
        return rx + lx * math.cos(r) - ly * math.sin(r), ry + lx * math.sin(r) + ly * math.cos(r)

    parts = [w(lx, ly) for lx in np.linspace(-70.0, 503.0, 6) for ly in np.linspace(-1125.0, 775.0, 12)]
    parts += [w(lx, ly) for lx in np.linspace(-460.0, 350.0, 6) for ly in (775.0, 925.0, 1075.0)]
    pts = np.array(parts)
    g0 = ground(rx, ry)
    rel = np.array([g0 - ground(x, y) for x, y in pts])               # > 0: the structure stands over the ground here
    wet = sum(bool(mask("water", x, y)) for x, y in pts)
    d_other = 1e9
    for n, t in ow_rail.network()["tracks"].items():
        if n == "S":
            continue
        c = t["path"].sample(100.0)
        d_other = min(d_other, float(np.hypot(pts[:, None, 0] - c[None, :, 0], pts[:, None, 1] - c[None, :, 1]).min()))
    buf = ow_rail.network()["buffer"][0]
    tx, ty = w(ow_rail.RAMP_TRACK_X, ow_rail.RAMP_BUFFER_Y)
    lines.append("RAMP SM_Berlin_RailRamp at (%.0f, %.0f) yaw %.1f: buffer face on S's end within %.1f cm; under the structure "
                 "the ground is up to %.0f cm below its base (floating) and up to %.0f cm above it (buried); water cells %d; "
                 "nearest other track centre line %.1f m (limit 1.9); built for rail top +24 cm over the ground, here +%.0f" % (
                     rx, ry, ryaw, math.hypot(tx - buf[0], ty - buf[1]), rel.max(), -rel.min(), wet, d_other / 100.0,
                     59.0 + ow_rail.SINK))
    if wet or rel.max() > 15.0 or d_other < 190.0:
        fails.append("ramp footprint")


def check_generated(lines):
    """Are the generated files (layout rail_line, OW_dress.json rail items) from the current ow_rail.py? The other checks
    rebuild everything in memory and cannot see a stale file."""
    import json
    notes = []
    line_now = ow_rail.rail_line()
    line_file = dg.LAYOUT.get("rail_line", [])
    d0 = math.hypot(line_now[0][0] - line_file[0][0], line_now[0][1] - line_file[0][1]) if line_file else 1e9
    if d0 > 1.0 or len(line_now) != len(line_file):
        notes.append("layout rail_line[0] %.0f cm off the current code (%d / %d points)" % (d0, len(line_file), len(line_now)))
    try:
        with open(os.path.join(dg.TERRAIN, "OW_dress.json")) as handle:
            dress = json.load(handle)
    except Exception as error:
        dress = {}
        notes.append("OW_dress.json unreadable (%r)" % (error,))
    items_file = dress.get("inst", {}).get(dg.RAIL_GROUP, {}).get("items", [])
    items_now = dg.INST[dg.RAIL_GROUP]["items"]
    if len(items_file) != len(items_now):
        notes.append("OW_dress.json has %d rigid rail panels, the code %d" % (len(items_file), len(items_now)))
    else:
        dev = max([0.0] + [max(abs(a - b) for a, b in zip(f, n)) for f, n in zip(items_file, items_now)])
        if dev > 0.05:
            notes.append("OW_dress.json rigid rail panels differ from the code by up to %.1f" % dev)
    segs_file = {k: len(v) for k, v in dress.get("rails", {}).get("tracks", {}).items()}
    segs_now = {k: len(v) for k, v in dg.RAILS["tracks"].items()}
    if segs_file != segs_now:
        notes.append("OW_dress.json bent panels %s, the code %s" % (segs_file, segs_now))
    lines.append("GENERATED FILES vs current code: %s" % ("in step" if not notes else
                                                          "STALE (rerun ow_terrain_gen.py + ow_dress_gen.py): " + "; ".join(notes)))
    return not notes


def check_edge(tracks, lines, fails):
    lines.append("EDGE (landscape edge y = %.0f, play edge y = %.0f)" % (ow_rail.LANDSCAPE_EDGE_Y, dg.B["ymax"]))
    for name in ("S", "N"):
        _, _, e, d = tracks[name][-1][1].ends()
        lines.append("  %s ends at (%.1f, %.1f), heading %.2f deg: %.1f cm from the landscape edge, %.1f m past the play edge" % (
            name, e[0], e[1], heading_deg(d), ow_rail.LANDSCAPE_EDGE_Y - e[1], (e[1] - dg.B["ymax"]) / 100.0))
        if abs(ow_rail.LANDSCAPE_EDGE_Y - e[1]) > LIMITS["edge_tol_cm"] or abs(e[0]) > LAY["size_x"] / 2.0:
            fails.append("%s does not reach the edge" % name)
        length = sum(1 for _ in tracks[name])
        lines.append("    %d panels, continuous from %s" % (length, "the buffer stop" if name == "S" else "turnout T1"))


def paint_grid():
    """The ballast bed weight on the terrain generator's work grid (row north..south), as build_layers paints it."""
    import ow_terrain_gen as tg
    V = tg.V
    X = (tg.SIZE / 2.0 - np.arange(V) * tg.QUAD_CM)[:, None] * np.ones((1, V))
    Y = np.ones((V, 1)) * (-tg.SIZE / 2.0 + np.arange(V) * tg.QUAD_CM)[None, :]
    n2 = tg.noise(102, 7)
    d = ow_rail.distance(X, Y, 2500.0)
    Zw = HRAW[::1, :].T[::-1, :]                                   # [iy, ix] -> work frame [row, col]
    bed = ow_rail.bed_weight(d, n2) * tg.smoothstep(30.0, 90.0, Zw)
    yard = np.asarray(ow_rail.yard_point(), float)
    apron = tg.smoothstep(1300.0, 600.0, d + 150.0 * n2) * tg.smoothstep(9000.0, 6000.0, np.hypot(X - yard[0], Y - yard[1]))
    apron = apron * (1.0 - bed) * tg.smoothstep(30.0, 90.0, Zw)
    return X, Y, d, bed, apron


def check_paint(tracks, lines, fails, bed_w):
    X, Y, d, bed, apron = bed_w

    def at_w(A, x, y):
        r = int(round((X[0, 0] - x) / 100.0))
        c = int(round((y - Y[0, 0]) / 100.0))
        return A[min(max(r, 0), A.shape[0] - 1), min(max(c, 0), A.shape[1] - 1)]

    under = []
    for name, seq in tracks.items():
        for _, panel in seq:
            for lx in np.arange(0.0, ow_rail.PANEL + 1.0, 250.0):
                for ly in (-375.0, 0.0, 400.0):
                    p = panel.world(lx, ly, 0.0)
                    under.append(at_w(bed, p[0], p[1]))
    under = np.array(under)
    band = (bed > 0.1) & (bed < 0.9)
    widths = d[band]
    lines.append("PAINT ballast bed: under the panels min weight %.2f, mean %.3f (limit >= %.2f); soft edge 0.9->0.1 between "
                 "%.1f and %.1f m from the centre line (median %.1f m); bed cells %d, yard apron cells %d" % (
                     under.min(), under.mean(), LIMITS["paint_under_min"], np.percentile(widths, 5) / 100.0,
                     np.percentile(widths, 95) / 100.0, np.median(widths) / 100.0, int((bed > 0.5).sum()), int((apron > 0.3).sum())))
    if under.min() < LIMITS["paint_under_min"]:
        fails.append("ballast paint thin under a panel")


def check_vegetation(lines, fails, removed, ran_full):
    left = []
    for group, g in dg.INST.items():
        if group.startswith(dg.NATURE_PREFIXES) and g["items"]:
            d = ow_rail.distance(np.array([i[0] for i in g["items"]]), np.array([i[1] for i in g["items"]]), 1000.0)
            left += [group for v in d if v < 500.0]
    trees = [b for b in dg.BPS if b[6].startswith("Trees/")]
    if trees:
        d = ow_rail.distance(np.array([b[2] for b in trees]), np.array([b[3] for b in trees]), 1000.0)
        left += ["Pine" for v in d if v < 700.0]
    lines.append("VEGETATION on the bed: removed %d by clear_rail_bed, left %d %s%s" % (
        removed, len(left), sorted(set(left)), "" if ran_full else " (nature not run: partial check)"))
    if left:
        fails.append("vegetation on the bed")


# ---- picture --------------------------------------------------------------------------------------------------------
def picture(tracks, bed_w, path, window=None, dpi=110, layers=None):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    X, Y, d, bed, apron = bed_w
    colours = {"PripPeat": (72, 60, 46), "PripMoss": (96, 110, 60), "PripMud": (44, 40, 38), "PripGrass": (170, 156, 104),
               "PripSand": (206, 192, 150), "PripGravel": (150, 146, 138), "PripEarth": (120, 96, 72),
               "PripAsh": (170, 170, 174), "PripAlgae": (120, 70, 150)}
    rgb = np.zeros((VY, VX, 3))
    for name, col in colours.items():
        if layers is not None:
            w = layers[name].astype(float) / 255.0
        else:
            w = np.fromfile(LAYOUT_FILES["layers"][name], dtype=np.uint8).reshape(VY, VX).astype(float) / 255.0
        rgb += w[:, :, None] * np.array(col)[None, None, :]
    if layers is None:
        to_grid = lambda a: a[::-1, :].T                             # work frame -> [iy, ix]
        b, ap = to_grid(bed), to_grid(apron)
        # the new bed over the written paint (the generator multiplies the other layers by 1 - 0.8 bed - 0.5 apron)
        rgb = rgb * (1.0 - 0.8 * b - 0.5 * ap)[:, :, None] + (0.8 * b)[:, :, None] * np.array((58, 52, 48)) + \
            (0.5 * ap)[:, :, None] * np.array((112, 92, 72))
    wet = HRAW < 0.0
    rgb[wet] = rgb[wet] * 0.35 + np.array((70, 50, 110)) * 0.65
    gx, gy = np.gradient(HRAW, axis=1) / 100.0, np.gradient(HRAW, axis=0) / 100.0
    rgb = np.clip(rgb * np.clip(0.85 + 0.9 * (gx + gy) / 1.4, 0.3, 1.5)[:, :, None], 0, 255) / 255.0
    # plot with +Y (east) to the right and +X (north) up: image rows = x
    img = np.transpose(rgb, (1, 0, 2))
    ext = [CORNER[1], CORNER[1] + LAY["size_y"], CORNER[0], CORNER[0] + LAY["size_x"]]
    fig, ax = plt.subplots(figsize=(16, 9) if window else (14, 14))
    ax.imshow(img, origin="lower", extent=ext, interpolation="nearest")
    grid = np.linspace(CORNER[0], CORNER[0] + LAY["size_x"], VX)
    ax.contour(grid, grid, dg.M["road"].T.astype(float), [0.5], colors="#d03030", linewidths=1.0)
    ax.contour(grid, grid, (HRAW < 0.0).T.astype(float), [0.5], colors="#3050ff", linewidths=0.8)
    for fid, w in dg.WERKS.items():
        c = w["centre"]
        ax.add_patch(plt.Circle((c[1], c[0]), ow_rail_qc_werk_r, fill=False, color="m", lw=1.0, ls="--"))
        ax.text(c[1], c[0], fid, color="m", fontsize=9, ha="center")
    for f in dg.LAYOUT["features"]:
        if f["kind"] in ("resource", "capture", "enemy_group") and not f["id"].startswith(("tunnel_", "garr_")):
            for q in f["points"]:
                ax.plot(q[1], q[0], "^" if f["kind"] == "resource" else "o", color="yellow" if f["kind"] == "resource" else "red", ms=6, mec="k")
                if window:
                    ax.text(q[1] + 150, q[0] + 150, f["id"], fontsize=7, color="w")
    style = {"S": "#ffa020", "N": "#20d0ff", "N_T1": "#ff40ff", "X_T2T3": "#ff40ff"}
    for name, seq in tracks.items():
        for kind, panel in seq:
            for ly in (-92.5, 92.5):
                line = np.array([panel.world(lx, ly, 0.0)[:2] for lx in np.linspace(0.0, ow_rail.PANEL, 9)])
                ax.plot(line[:, 1], line[:, 0], "-", color=style.get(name, "w"), lw=0.9 if window else 0.6)
            edge = np.array([panel.world(lx, ly, 0.0)[:2] for lx, ly in
                             ((0, -375), (ow_rail.PANEL, -375), (ow_rail.PANEL, 400), (0, 400), (0, -375))])
            ax.plot(edge[:, 1], edge[:, 0], "-", color="k" if kind == "rigid" else "#ff40ff", lw=0.3, alpha=0.6)
    for t in ow_rail.network()["turnouts"]:
        ax.plot(t["toe"][1], t["toe"][0], "s", color="lime", ms=6, mec="k")
        ax.text(t["toe"][1] + 200, t["toe"][0] - 600, t["id"], color="lime", fontsize=9, fontweight="bold")
    buf, _ = ow_rail.network()["buffer"]
    ax.plot(buf[1], buf[0], "X", color="w", ms=9, mec="k")
    rx, ry, ryaw = ow_rail.ramp_transform()
    ax.plot(ry, rx, "P", color="orange", ms=8, mec="k")
    for b_ in dg.BPS:
        if b_[0].startswith("RailYard_Wagon"):
            ax.plot(b_[3], b_[2], "s", color="#a0a0a0", ms=7, mec="k")
    ax.axvline(dg.B["ymax"], color="yellow", ls=":", lw=1)
    ax.axvline(ow_rail.LANDSCAPE_EDGE_Y, color="w", ls="-", lw=1)
    if window:
        ax.set_xlim(window[2], window[3])
        ax.set_ylim(window[0], window[1])
    ax.set_xlabel("Y east (cm)")
    ax.set_ylabel("X north (cm)")
    ax.set_title("Ger_OderWarthe rail: S orange, N cyan, turnout legs magenta (spline panels outlined magenta), "
                 "turnouts green, buffer X, ramp +, containers grey; dashed = Werk 70 m", fontsize=9)
    ax.set_aspect("equal")
    fig.tight_layout()
    fig.savefig(path, dpi=dpi)
    plt.close(fig)


ow_rail_qc_werk_r = 7000.0


def paint_layers_in_memory():
    """The nine paint layers exactly as ow_terrain_gen would write them now ([iy, ix] uint8), built in memory; and how
    many height cells of that build equal OW_height.raw (the seating check reads the file)."""
    import ow_terrain_gen as tg
    gen = tg.Gen()
    gen.build_heights()
    gen.build_layers()
    z = np.clip(np.round(32768.0 + tg.Gen.to_grid(gen.Z) * 128.0 / 100.0), 0, 65535)
    raw = np.fromfile(LAYOUT_FILES["height"], dtype="<u2").astype(np.float64).reshape(VY, VX)
    return {n: tg.Gen.to_grid(gen.W[n]) for n in tg.LAYERS}, int((z == raw).sum())


def check_layers(tracks, lines, fails, layers, same):
    ballast = (layers["PripMud"].astype(float) + layers["PripGravel"] + layers["PripAsh"]) / 255.0
    under = []
    for name, seq in tracks.items():
        for _, panel in seq:
            for lx in np.arange(0.0, ow_rail.PANEL + 1.0, 250.0):
                for ly in (-375.0, 0.0, 400.0):
                    p = panel.world(lx, ly, 0.0)
                    under.append(ballast[dg.cell(p[0], p[1])])
    under = np.array(under)
    lines.append("PAINT layers rebuilt in memory (their heights equal OW_height.raw in %d of %d cells): ballast share "
                 "(mud + gravel + ash) under the panels min %.2f, mean %.2f (limit >= 0.60)" % (same, HRAW.size, under.min(), under.mean()))
    if under.min() < 0.6:
        fails.append("ballast layers thin under a panel")


def main():
    global HRAW
    HRAW = load_heights()
    os.makedirs(OUT, exist_ok=True)
    ran_full = True
    try:
        # the dressing in main()'s order, in memory (no file is written): the vegetation filter needs the scatter
        dg.ridge(); dg.werks(); dg.stations(); dg.outposts(); dg.resources(); dg.crossings(); dg.peat_fires()
        dg.nature(); dg.decals()
    except Exception as error:                      # another agent may be mid-edit on the Werk side: rail-only check
        print("full dressing run failed (%r): rail-only check" % (error,))
        ran_full = False
        dg.INST.clear(); dg.BPS.clear(); dg.MESHES.clear(); dg.DECALS.clear()
        dg.resources()
    removed = dg.clear_rail_bed()
    tracks = built_tracks()
    lines, fails = [], []
    lines.append("Ger_OderWarthe rail QC (offline, generated terrain files of %s)" % LAYOUT_FILES["height"])
    net = ow_rail.network()
    lines.append("design: yard heading %.1f deg, curve R %.0f m to %.1f deg, track spacing %.1f m, turnout legs %.0f m long "
                 "(R %.1f m, %.2f deg), panels sunk %.0f cm, legs %.0f cm lower; buffer stop at (%.0f, %.0f)" % (
                     ow_rail.YARD_HEADING, ow_rail.CURVE_R / 100.0, ow_rail.EAST_HEADING, ow_rail.SPACING / 100.0,
                     ow_rail.TURNOUT_ALONG / 100.0,
                     ow_rail.TURNOUT_ALONG / (2.0 * math.sin(2.0 * math.atan(ow_rail.SPACING / ow_rail.TURNOUT_ALONG))) / 100.0,
                     math.degrees(2.0 * math.atan(ow_rail.SPACING / ow_rail.TURNOUT_ALONG)), -ow_rail.SINK, -ow_rail.BRANCH_DIP,
                     net["buffer"][0][0], net["buffer"][0][1]))
    lines.append("tracks: " + ", ".join("%s %.1f m" % (n, t["path"].length / 100.0) for n, t in net["tracks"].items()))
    lines.append("")
    check_joints(tracks, lines, fails)
    check_stretch(tracks, lines, fails)
    check_turnouts(tracks, lines, fails)
    check_seating(tracks, lines, fails)
    check_clearances(tracks, lines, fails)
    check_other_dressing(tracks, lines, fails)
    check_edge(tracks, lines, fails)
    check_ramp(tracks, lines, fails)
    bed_w = paint_grid()
    check_paint(tracks, lines, fails, bed_w)
    layers = None
    if "--paint" in sys.argv:
        layers, same = paint_layers_in_memory()
        check_layers(tracks, lines, fails, layers, same)
    check_vegetation(lines, fails, removed, ran_full)
    lines.append("")
    fresh = check_generated(lines)
    lines.append("RESULT: %s" % ("PASS" if not fails else "FAIL: " + "; ".join(fails)))
    if not fresh:
        lines.append("        (the checks above are of the current code; the files on disk are older)")
    text = "\n".join(lines)
    with open(os.path.join(OUT, "rail_qc.txt"), "w") as handle:
        handle.write(text + "\n")
    print(text)
    picture(tracks, bed_w, os.path.join(OUT, "rail_qc.png"), window=(-6000.0, 16000.0, 9000.0, 45500.0), dpi=120, layers=layers)
    buf = ow_rail.network()["buffer"][0]
    picture(tracks, bed_w, os.path.join(OUT, "rail_qc_yard.png"),
            window=(buf[0] - 7000.0, buf[0] + 3000.0, buf[1] - 2000.0, buf[1] + 16000.0), dpi=130, layers=layers)
    print("wrote %s" % OUT)


if __name__ == "__main__":
    main()
