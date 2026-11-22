"""Concept plans for the Forge survey station "alive" layer on Ger_Berlin (design round v01, 2026-11-22).

Draws 2D plans (no editor, no GPU) from the map layout JSON and the station's collision boxes:
    0  context: station, crater, Gate, Reichstag, lanes, patrol tracks, trigger ring, runner route
    1-5 the five "alive" options (A-E), each a close plan of the station with what moves, what sounds,
        which units, and a "what is new" footer
Output: D:/UE5Projects/RTS_Survival/Docs/Concepts/BerlinForgeStation/v01/ (one PNG per sheet + _all.png + _all_small.jpg)
Design doc: Source/Docs/Campaign/BerlinForgeStation.md

World frame as in the Unreal top view: +X north (up), +Y east (right), centimetres in the JSON, metres on the plots.
Station transform as placed by berlin_map_build.custom_poi_list(): pivot = feature forge_station, yaw = towards the crater
centre; Unreal local Y = -Blender Y (the generator's numbers are Blender metres).

Run:  python forge_alive_plans.py
"""
import json
import math
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, Circle, FancyArrowPatch, Wedge
from PIL import Image

LAYOUT_FILE = "D:/UE5Projects/RTS_Survival/Saved/Berlin/terrain/Ger_Berlin_layout.json"
OUT = "D:/UE5Projects/RTS_Survival/Docs/Concepts/BerlinForgeStation/v01"
RENDER = ("D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinForgeStation/Previews/"
          "Unreal_rts_a.png")

# Station parts in Blender metres (generator UCX_BOXES), name, (lo), (hi), role
PARTS = [
    ("gate W pier", (-4.9, -7.4), (-1.0, -4.4), "shell"),
    ("gate E pier", (7.5, -7.4), (9.6, -4.4), "shell"),
    ("cryo block", (-14.2, -10.4), (-4.9, -1.6), "target"),
    ("command bunker", (-13.6, 1.6), (-5.0, 8.8), "target"),
    ("crew hut", (-14.4, 11.0), (-6.0, 14.4), "shell"),
    ("sampling rig", (8.4, -9.6), (14.6, -2.2), "target"),
    ("rcv 1", (8.5, 1.6), (11.1, 8.0), "shell"),
    ("rcv 2", (11.5, 1.6), (14.1, 8.0), "shell"),
    ("sample bunker", (8.4, 10.4), (14.4, 14.2), "shell"),
]
ARCH_WALL = ((-1.0, -7.4), (7.5, -4.4))          # arch head over the lane (no collision)
LANE = ((-0.5, -15.0), (7.0, 15.0))
STACK = (-9.6, -6.0)                              # cooling stack centre (Blender)
DISH = (-9.0, 5.0)                                # command bunker roof dish / sensor horns
CONSOLE = (-1.9, 5.2)
RIG = (12.0, -5.9)
SAMPLE_ROOF = (11.4, 12.3)

INK = "#1d1d1f"
GLASS = "#3a4048"
RIM = "#6c727a"
TARGET = "#c8372d"
SHELL = "#8a7d72"
UNIT = "#2f6fb0"
CREW = "#d08a1e"
VEH = "#3c8c4a"
FX = "#7a4fb5"
SND = "#b0306a"
ALARM = "#e0a400"


def load_layout():
    with open(LAYOUT_FILE, "r", encoding="utf-8") as handle:
        return json.load(handle)


L = load_layout()
FEAT = {f["id"]: f for f in L["features"]}
REG = {r["id"]: r for r in L["regions"]}
PATHS = {p["id"]: p for p in L["paths"]}
PIVOT = FEAT["forge_station"]["points"][0]
CRATER_C = REG["crater"]["centre"]
YAW = math.atan2(CRATER_C[1] - PIVOT[1], CRATER_C[0] - PIVOT[0])
REICHSTAG_FORECOURT = (3600.0, -15500.0)    # owner 2026-11-22: by the ruin, south (portico) side; exact point to set
                                            # (flow 4.2 says (800, -15500), which the owner says is wrong)
GATE = tuple(L["gate_at"])


def to_world_cm(bx, by):
    """Blender metres (station frame) -> world cm. Unreal local = (bx, -by) * 100, then yaw about the pivot."""
    ux, uy = bx * 100.0, -by * 100.0
    c, s = math.cos(YAW), math.sin(YAW)
    return PIVOT[0] + ux * c - uy * s, PIVOT[1] + ux * s + uy * c


def to_world_m(bx, by):
    wx, wy = to_world_cm(bx, by)
    return wx / 100.0, wy / 100.0


def plot_xy(wx_m, wy_m):
    """Plot coords: horizontal = world Y (east right), vertical = world X (north up)."""
    return wy_m, wx_m


def P(bx, by):
    return plot_xy(*to_world_m(bx, by))


def Pw(wx_cm, wy_cm):
    return plot_xy(wx_cm / 100.0, wy_cm / 100.0)


def box_poly(lo, hi):
    return [P(lo[0], lo[1]), P(hi[0], lo[1]), P(hi[0], hi[1]), P(lo[0], hi[1])]


def centre(lo, hi):
    return P((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2)


def crater_rim_points():
    return [Pw(x, y) for x, y in REG["crater"]["points"]]


def rim_arc_near(station_m, radius_m, count, spread_deg):
    """Points on the crater rim polygon closest to the station, spread along the rim."""
    pts = REG["crater"]["points"]
    sx, sy = station_m
    dists = [math.hypot(p[0] / 100 - sx, p[1] / 100 - sy) for p in pts]
    i0 = dists.index(min(dists))
    step = max(1, int(len(pts) * spread_deg / 360.0 / max(count - 1, 1)))
    out = []
    for k in range(count):
        idx = (i0 + (k - count // 2) * step) % len(pts)
        out.append(Pw(*pts[idx]))
    return out


def base_axes(ax, half=48, title="", context=False):
    ax.set_facecolor("#efe9df")
    cx, cy = plot_xy(PIVOT[0] / 100, PIVOT[1] / 100)
    ccx, ccy = plot_xy(CRATER_C[0] / 100, CRATER_C[1] / 100)
    mx, my = (cx * 0.62 + ccx * 0.38), (cy * 0.62 + ccy * 0.38)
    ax.set_xlim(mx - half, mx + half)
    ax.set_ylim(my - half, my + half)
    ax.set_aspect("equal")
    ax.tick_params(labelsize=7, colors="#777")
    ax.set_title(title, fontsize=13, loc="left", color=INK, fontweight="bold")
    rim = crater_rim_points()
    ax.add_patch(Polygon(rim, closed=True, facecolor=GLASS, edgecolor=RIM, lw=2.5, alpha=0.85, zorder=1))
    ax.text(ccx, ccy, "blast crater\n(glass)", color="#cfd6de", ha="center", va="center", fontsize=9, zorder=2)
    return mx, my


def draw_station(ax, highlight_targets=False, alpha=1.0):
    lane = box_poly(*LANE)
    ax.add_patch(Polygon(lane, closed=True, facecolor="#d8cdbd", edgecolor="#b5a894", lw=0.8, ls="--", zorder=3))
    ax.add_patch(Polygon(box_poly(*ARCH_WALL), closed=True, facecolor="#9b8f84", edgecolor=INK, lw=0.6,
                         hatch="////", alpha=0.6 * alpha, zorder=4))
    for name, lo, hi, role in PARTS:
        col = TARGET if (highlight_targets and role == "target") else SHELL
        ax.add_patch(Polygon(box_poly(lo, hi), closed=True, facecolor=col, edgecolor=INK, lw=0.9, alpha=alpha,
                             zorder=5))
        x, y = centre(lo, hi)
        ax.text(x, y, name, ha="center", va="center", fontsize=6.3, color="white", zorder=6,
                fontweight="bold" if role == "target" else "normal")
    ax.text(*P(3.2, -8.8), "gate arch", fontsize=6, ha="center", va="center", color=INK, zorder=6)
    ax.plot(*P(*CONSOLE), marker="s", ms=5, color="#444", zorder=6)
    ax.text(*P(CONSOLE[0] + 0.2, CONSOLE[1] + 2.2), "console +\nbeacon", fontsize=5.5, ha="center", color=INK, zorder=6)


def edge_arrow(ax, mx, my, half, target_plot, label, color=INK, label_shift=(0.0, 0.0)):
    dx, dy = target_plot[0] - mx, target_plot[1] - my
    ux, uy = dx / math.hypot(dx, dy), dy / math.hypot(dx, dy)
    station = plot_xy(PIVOT[0] / 100, PIVOT[1] / 100)
    d = math.hypot(target_plot[0] - station[0], target_plot[1] - station[1])   # straight line from the station pivot
    r = half * 0.86
    sx, sy = mx + ux * (r - 9), my + uy * (r - 9)
    ex, ey = mx + ux * r, my + uy * r
    ax.add_patch(FancyArrowPatch((sx, sy), (ex, ey), arrowstyle="-|>", mutation_scale=14, color=color, lw=2,
                                 zorder=9))
    ax.text(sx - ux * 2.5 + label_shift[0], sy - uy * 2.5 + label_shift[1], f"{label}\n{d:.0f} m", fontsize=7, ha="center", va="center", color=color,
            zorder=9, bbox=dict(boxstyle="round,pad=0.2", fc="white", ec=color, lw=0.6, alpha=0.9))


def common_arrows(ax, mx, my, half):
    edge_arrow(ax, mx, my, half, Pw(*REICHSTAG_FORECOURT), "Reichstag (runners)", TARGET, (0.0, 7.0))
    edge_arrow(ax, mx, my, half, Pw(*GATE), "Brandenburg Gate", INK, (0.0, -7.0))
    start = PATHS["central_lane"]["points"][4]          # (3250, 5556): where the central lane comes in
    edge_arrow(ax, mx, my, half, Pw(*start), "player (central lane)", UNIT)
    ax.annotate("N", xy=(mx - half + 4, my + half - 4), xytext=(mx - half + 4, my + half - 11),
                arrowprops=dict(arrowstyle="-|>", color=INK), ha="center", fontsize=9, color=INK, zorder=9)


def squad(ax, xy, label, color=UNIT, marker="o", size=9):
    ax.plot(*xy, marker=marker, ms=size, color=color, markeredgecolor="white", mew=1.2, zorder=8)
    ax.text(xy[0] + 1.6, xy[1] + 1.2, label, fontsize=6.3, color=color, zorder=8, fontweight="bold")


def fx(ax, xy, label, color=FX, r=2.6):
    ax.add_patch(Circle(xy, r, facecolor=color, alpha=0.25, edgecolor=color, lw=1, zorder=7))
    ax.text(xy[0], xy[1] - r - 1.6, label, fontsize=6, color=color, ha="center", zorder=8)


def sound(ax, xy, label):
    ax.text(xy[0], xy[1], "))", fontsize=11, color=SND, ha="center", va="center", zorder=8, fontweight="bold")
    ax.text(xy[0], xy[1] + 2.0, label, fontsize=5.8, color=SND, ha="center", zorder=8)


def footer(fig, lines):
    fig.text(0.02, 0.015, "\n".join(lines), fontsize=8.2, va="bottom", ha="left", color=INK, family="DejaVu Sans")


def legend_box(ax):
    items = [(TARGET, "counted target"), (SHELL, "station shell"), (UNIT, "Forge guards (real units)"),
             (CREW, "hazmat crew (real units)"), (VEH, "vehicles"), (FX, "VFX / motion"), (SND, "sound")]
    for i, (c, t) in enumerate(items):
        ax.text(1.01, 0.98 - i * 0.045, "■ " + t, transform=ax.transAxes, fontsize=7, color=c, va="top")


def new_figure(title, half=48):
    fig = plt.figure(figsize=(11, 10.2), dpi=110)
    ax = fig.add_axes([0.05, 0.17, 0.72, 0.77])
    mx, my = base_axes(ax, half, title)
    return fig, ax, mx, my, half


def guards_static(ax):
    squad(ax, P(3.2, -11.0), "Mosin_Soviet (arch post)")
    squad(ax, P(-3.0, 9.0), "PTRS_Soviet (console)")
    squad(ax, P(16.5, -4.0), "HazmatBazooka (rig)", marker="D")


# ------------------------------------------------------------------ sheets
def sheet_context():
    fig = plt.figure(figsize=(11, 10.2), dpi=110)
    ax = fig.add_axes([0.05, 0.17, 0.72, 0.77])
    half = 132
    ax.set_facecolor("#efe9df")
    cx, cy = plot_xy(PIVOT[0] / 100, PIVOT[1] / 100)
    mx, my = cx - 70, cy + 30
    ax.set_xlim(mx - half, mx + half)
    ax.set_ylim(my - half, my + half)
    ax.set_aspect("equal")
    ax.tick_params(labelsize=7, colors="#777")
    ax.set_title("0  Context: trigger ring, runner route, targets", fontsize=13, loc="left", fontweight="bold")
    ax.add_patch(Polygon(crater_rim_points(), closed=True, facecolor=GLASS, edgecolor=RIM, lw=2, alpha=0.85))
    ax.text(*plot_xy(CRATER_C[0] / 100, CRATER_C[1] / 100), "crater", color="#cfd6de", ha="center", fontsize=9)
    for pid, col, lw, ls in (("central_lane", "#9d8f7d", 5, "-"), ("north_lane", "#9d8f7d", 5, "-"),
                             ("south_lane", "#9d8f7d", 5, "-"), ("patrol_shield", "#3d6fa8", 1.6, "--"),
                             ("patrol_alex", "#3d6fa8", 1.6, "--")):
        pts = [Pw(*p) for p in PATHS[pid]["points"]]
        ax.plot([p[0] for p in pts], [p[1] for p in pts], color=col, lw=lw, ls=ls, alpha=0.8, zorder=2)
        lx, ly = pts[min(2, len(pts) - 1)]
    ax.text(*Pw(5200, 9000), "patrol_alex", fontsize=7, color="#3d6fa8")
    ax.text(*Pw(5600, -8800), "patrol_shield", fontsize=7, color="#3d6fa8", rotation=0)
    draw_station(ax, highlight_targets=True)
    # trigger ring (sight-range proxy)
    # ring size: outermost real-unit post (option B stakes) + guard reach (attack range + AddedAggroRange, estimate)
    stake_dist = max(math.dist(p, (cx, cy)) for p in rim_arc_near(to_world_m(12, 0), 0, 5, 95))
    reach_m = 55.0
    ring_r = round(stake_dist + reach_m + 10.0, -1)
    ax.add_patch(Circle((cx, cy), ring_r, fill=False, ec=ALARM, lw=2.2, ls="-", zorder=6))
    ax.text(cx - ring_r * 0.62, cy - ring_r * 0.97, f"trigger ring r {ring_r:.0f} m (with B; about 80 m without)\n"
            "(plate + Berlin_ForgeStationSighted)", fontsize=7.5, color="#8a6500", zorder=7,
            bbox=dict(boxstyle="round,pad=0.2", fc="white", ec=ALARM, lw=0.5, alpha=0.9))
    # estimated reach of the outermost posts
    ax.add_patch(Circle((cx, cy), stake_dist + reach_m, fill=False, ec=TARGET, lw=0.8, ls=":", zorder=6))
    # runner route: rim road -> Brandenburg Gate -> Reichstag forecourt (the alarm trigger, flow 4.2)
    route = [Pw(*PIVOT), Pw(3250, -3333), Pw(*REICHSTAG_FORECOURT)]   # along patrol_shield
    ax.plot([p[0] for p in route], [p[1] for p in route], color=TARGET, lw=3, zorder=7)
    length_m = sum(math.dist(route[i], route[i + 1]) for i in range(len(route) - 1))
    ax.add_patch(FancyArrowPatch(route[-2], route[-1], arrowstyle="-|>", mutation_scale=18, color=TARGET, lw=3,
                                 zorder=7))
    ax.text(*Pw(5600, -6000), f"runner route {length_m:.0f} m (NW road, patrol_shield)\nabout "
            f"{length_m / 4.0:.0f}-{length_m / 3.0:.0f} s at 3-4 m/s", fontsize=8, color=TARGET, zorder=8,
            bbox=dict(boxstyle="round,pad=0.25", fc="white", ec=TARGET, lw=0.6))
    print("stake max dist m", round(stake_dist, 1), "ring", ring_r, "route m", round(length_m, 1))
    rf = Pw(*REICHSTAG_FORECOURT)
    ax.plot(*rf, marker="*", ms=16, color=TARGET, zorder=8)
    ax.text(rf[0] + 3, rf[1] + 3, "Reichstag forecourt\n(alarm trigger)", fontsize=8, color=TARGET)
    rt = Pw(*L["reichstag_at"])
    ax.add_patch(Circle(rt, 45, facecolor="#b9a99a", alpha=0.5, ec=INK, lw=0.6, zorder=1))
    ax.text(rt[0], rt[1] + 8, "Reichstag ruin\n(Shield base)", fontsize=8, ha="center")
    g = Pw(*GATE)
    ax.add_patch(Polygon([(g[0] - 13, g[1] - 32), (g[0] + 13, g[1] - 32), (g[0] + 13, g[1] + 32),
                          (g[0] - 13, g[1] + 32)], closed=True, facecolor="#b9a99a", ec=INK, lw=0.6, zorder=3))
    ax.text(g[0], g[1] + 36, "Brandenburg Gate", fontsize=8, ha="center")
    e6 = Pw(-8000, -2900)
    ax.plot(*e6, marker="h", ms=12, color="#c0392b", alpha=0.6)
    ax.text(e6[0] + 3, e6[1] - 6, "E6 Radixite field\n(far rim)", fontsize=7)
    ax.annotate("N", xy=(mx - half + 8, my + half - 8), xytext=(mx - half + 8, my + half - 22),
                arrowprops=dict(arrowstyle="-|>", color=INK), ha="center", fontsize=10)
    ax.text(1.01, 0.98, "■ counted target (3)", transform=ax.transAxes, fontsize=8, color=TARGET, va="top")
    ax.text(1.01, 0.93, "■ station shell", transform=ax.transAxes, fontsize=8, color=SHELL, va="top")
    ax.text(1.01, 0.88, "— trigger ring", transform=ax.transAxes, fontsize=8, color=ALARM, va="top")
    ax.text(1.01, 0.83, "··· guard aggro reach", transform=ax.transAxes, fontsize=8, color=TARGET, va="top")
    ax.text(1.01, 0.78, "-- patrol tracks", transform=ax.transAxes, fontsize=8, color="#3d6fa8", va="top")
    if os.path.exists(RENDER):
        ins = fig.add_axes([0.78, 0.30, 0.21, 0.26])
        ins.imshow(Image.open(RENDER))
        ins.set_axis_off()
        ins.set_title("SM_Berlin_ForgeStation (Unreal)", fontsize=7)
    footer(fig, [
        "The central lane enters from the east and leaves along the rim through the gate arch towards the Gate. "
        "Both patrol tracks end at the station.",
        "Runners flee north-west along patrol_shield to the Reichstag forecourt, away from a player who arrives "
        "from the east: block that road near the station.",
        "Size the ring from the outermost post plus the measured reach, so the plate comes before the fight. Targets: rig, cryo block, command bunker.",
    ])
    return fig


def sheet_A():
    fig, ax, mx, my, half = new_figure("A  Kulisse: the station runs (lights, steam, sound)")
    draw_station(ax)
    fx(ax, P(*STACK), "stack steam (NS_RadixiteSmoke1)", r=3.4)
    fx(ax, P(*DISH), "dish turns (6 rpm)")
    fx(ax, P(*CONSOLE), "beacon pulse (shard mast)", color=ALARM, r=2.0)
    fx(ax, P(*RIG), "drill string turns + core sways", r=2.6)
    fx(ax, P(9.8, 4.8), "frost vapour", r=2.0)
    for lx, ly in ((-4.5, -11.5), (8.0, 11.0), (-2.0, 12.5), (15.5, -10.5)):
        ax.plot(*P(lx, ly), marker="*", ms=10, color=ALARM, zorder=8)
    ax.text(*P(-2.0, 15.5), "floodlights (4)", fontsize=6, color="#8a6500", ha="center")
    sound(ax, P(-9.6, -1.0), "cooling hum + fan")
    sound(ax, P(14.0, -9.5), "drill loop")
    sound(ax, P(-7.0, 8.5), "radio static + Geiger")
    sound(ax, P(-3.0, 19.0), "megaphone (Directorate set)")
    guards_static(ax)
    squad(ax, P(19.0, 9.0), "HazmatEngineers (idle)", color=CREW, marker="D")
    common_arrows(ax, mx, my, half)
    legend_box(ax)
    footer(fig, [
        "What moves: steam, beacon, floodlights, the dish and the drill string. Units stand on AEnemyStrongpoint posts.",
        "Existing: NS_RadixiteSmoke1, NS_BerlinAtmo_CraterVapour, S_ConstructionDrill, Crane_Idle, TurnOnRadio_*, "
        "LogoGeiger, Rus_Bxp_Megaphone_Broadcast, strongpoint.",
        "New: BP_Berlin_ForgeStation (lights, Niagara, audio, two rotating parts split from the mesh). No C++. "
        "Cost: low.  Runner risk: low (compact posts).",
    ])
    return fig


def sheet_B():
    fig, ax, mx, my, half = new_figure("B  Probennahme: hazmat crews sample the rim")
    draw_station(ax)
    stakes = rim_arc_near(to_world_m(12, 0), 0, 5, 95)
    xs = [p[0] for p in stakes]
    ys = [p[1] for p in stakes]
    ax.plot(xs + [P(*SAMPLE_ROOF)[0]], ys + [P(*SAMPLE_ROOF)[1]], color=CREW, lw=1.6, ls="--", zorder=7)
    for i, s in enumerate(stakes):
        ax.plot(*s, marker="^", ms=9, color=CREW, markeredgecolor=INK, zorder=8)
        ax.text(s[0] + 1.2, s[1] - 2.6, f"stake {i + 1}", fontsize=6, color=CREW)
    squad(ax, stakes[1], "HazmatEngineers A: kneel + work 20-30 s", color=CREW, marker="D")
    squad(ax, stakes[3], "HazmatEngineers B (Hard+)", color=CREW, marker="D")
    ax.text(*P(13.0, 16.5), "carry cores to the\nsample bunker", fontsize=6.3, color=CREW, ha="center")
    fx(ax, P(*STACK), "stack steam", r=3.0)
    fx(ax, P(*CONSOLE), "beacon", color=ALARM, r=1.8)
    sound(ax, P(20.0, -14.0), "Geiger clicks at the stakes")
    guards_static(ax)
    squad(ax, P(22.0, 5.0), "Mosin_Soviet escort (follows crew)")
    common_arrows(ax, mx, my, half)
    legend_box(ax)
    footer(fig, [
        "Crews walk a circuit of 5 sampling stakes on the rim; at each one they kneel and work (Scavenge_infMontage), "
        "then carry the core back.",
        "Existing: Rus_Squad_HazmatEngineers, Scavenge_infMontage, Rus_Squad_Mosin_Soviet, strongpoint, Geiger sound. "
        "Base dressing of A included.",
        "New: a work-circuit behaviour (strongpoint post option 'work montage + next post', C++), stake + canister "
        "props. Cost: medium.  Runner risk: medium (crew spread 40 m along the rim).",
    ])
    return fig


def sheet_C():
    fig, ax, mx, my, half = new_figure("C  Sedierung: the cryo-sedation cycle (first look at Forge cryo tech)")
    draw_station(ax)
    rig = P(*RIG)
    for k, r in enumerate((4.5, 8.0, 11.5)):
        ax.add_patch(Circle(rig, r, fill=False, ec=FX, lw=1.4 - k * 0.3, ls="-" if k == 0 else "--", zorder=7))
    ax.text(rig[0] + 18, rig[1] + 22, "every 75 s:\n1 samples pulse brighter (2 s)\n2 four lances vent frost (6 s)"
            "\n3 core and samples dim, frosted\n4 slow thaw over 60 s", fontsize=6.6, color=FX, zorder=8,
            bbox=dict(boxstyle="round,pad=0.25", fc="white", ec=FX, lw=0.6))
    fx(ax, P(-9.0, -3.0), "sedation rack frosts (cryo block roof)", r=2.4)
    fx(ax, P(*SAMPLE_ROOF), "core rack frosts", r=2.0)
    fx(ax, P(4.0, -6.0), "cryo main vents at the arch", r=1.8)
    sound(ax, P(16.5, -12.5), "compressor rise + hiss")
    sound(ax, P(-11.0, -12.5), "pump thump")
    rim_pts = rim_arc_near(to_world_m(12, -6), 0, 3, 40)
    for p in rim_pts:
        ax.plot(*p, marker="h", ms=9, color="#e9e2ff", markeredgecolor=FX, zorder=8)
    ax.text(rim_pts[0][0] - 4, rim_pts[0][1] - 4, "frosted rim crystals\n(decals)", fontsize=6, color=FX)
    squad(ax, P(17.0, -1.0), "HazmatEngineers step back during the vent", color=CREW, marker="D")
    guards_static(ax)
    common_arrows(ax, mx, my, half)
    legend_box(ax)
    footer(fig, [
        "A visible, readable cycle at the rig: the sample pulses, the lances vent frost, the crystal goes dull. "
        "Seeds Act 3 (Radixite reacts; the Forge 'sedates' it) without saying so.",
        "Existing: NS_ConstructionSmokeRound_* / NS_SmokeBurst* (white), NS_BerlinAtmo_CraterVapour, MI_Crystal1 "
        "(as a parent), decal kit. Base dressing of A included.",
        "New: a timeline in the station BP, one crystal MI with a pulse/frost parameter (not MI_Crystal1 itself), "
        "2 sounds (hiss, compressor). No C++. Cost: low-medium.  Runner risk: none added.",
    ])
    return fig


def sheet_D():
    fig, ax, mx, my, half = new_figure("D  Pendel: Forge traffic comes and goes")
    draw_station(ax)
    arch_in = P(3.2, -24.0)
    dwell = P(3.2, 2.0)
    ax.add_patch(FancyArrowPatch(arch_in, dwell, arrowstyle="-|>", mutation_scale=16, color=VEH, lw=2.4,
                                 connectionstyle="arc3,rad=0.0", zorder=8))
    ax.add_patch(FancyArrowPatch(P(5.5, 2.0), P(5.5, -24.0), arrowstyle="-|>", mutation_scale=16, color=VEH, lw=1.4,
                                 ls="--", zorder=8))
    ax.add_patch(Polygon(box_poly((1.6, -1.5), (4.8, 6.0)), closed=True, facecolor=VEH, edgecolor=INK, lw=0.8,
                         zorder=8))
    ax.text(*P(-1.0, 2.5), "SovietTanker_Forge\ndwells 60 s, hose\nto cryo main", fontsize=6.3, color=VEH,
            ha="center", zorder=9)
    ax.text(*P(3.2, -27.5), "from / to Reichstag\nevery 4-5 min", fontsize=6.5, color=VEH, ha="center")
    ax.add_patch(Polygon(box_poly((-3.2, 10.5), (-0.6, 15.0)), closed=True, facecolor=VEH, edgecolor=INK, lw=0.6,
                         alpha=0.7, zorder=8))
    ax.text(*P(-1.9, 17.0), "parked BA-11_Soviet\n(engine idles, exhaust)", fontsize=6, color=VEH, ha="center")
    pe = Pw(*PATHS["patrol_alex"]["points"][-2])
    ax.text(mx + half * 0.15, my + half * 0.55, "patrol_alex and patrol_shield already end here:\n"
            "patrol vehicles stop 30 s at the arch, then turn back", fontsize=6.8, color="#3d6fa8",
            bbox=dict(boxstyle="round,pad=0.25", fc="white", ec="#3d6fa8", lw=0.6))
    sound(ax, P(8.5, 0.0), "truck idle + pump")
    guards_static(ax)
    common_arrows(ax, mx, my, half)
    legend_box(ax)
    footer(fig, [
        "A Forge fuel tanker shuttles from the Reichstag, parks under the arch 60 s to top up the cryo main, and drives "
        "back. It is a runner: attacked, it flees.",
        "Existing: Rus_ArmoredCar_SovietTanker_Forge, Rus_ArmoredCar_BA11_Soviet, the two patrol tracks, NS_ExhaustTank. "
        "Convoys are not used (cargo, capture, 'Escaped').",
        "New: a dwell-and-return shuttle (mission BP timer with move orders, or a dwell time per point on "
        "AEnemyConvoyRoute). Cost: medium.  Runner risk: high (it drives to the Reichstag).",
    ])
    return fig


def sheet_E():
    fig, ax, mx, my, half = new_figure("E  Sucher: a sensor sweep over the rim (moving detection zone)")
    draw_station(ax)
    rig = P(*RIG)
    w = Wedge(rig, 38, -100, -40, facecolor=ALARM, alpha=0.22, edgecolor=ALARM, lw=1, zorder=6)
    ax.add_patch(w)
    w2 = Wedge(rig, 38, -40, 60, fill=False, edgecolor=ALARM, lw=0.8, ls="--", zorder=6)
    ax.add_patch(w2)
    ax.text(rig[0] + 18, rig[1] - 22, "searchlight on the rig deck\nsweeps 160° in 20 s;\nits ground spot (r 6 m) "
            "is a\ndetection zone: alarm if a\nplayer unit stands in it", fontsize=6.6, color="#8a6500",
            bbox=dict(boxstyle="round,pad=0.25", fc="white", ec=ALARM, lw=0.6))
    fx(ax, P(*DISH), "dish tracks the beam", r=2.2)
    sound(ax, P(17.0, -13.0), "motor whine")
    guards_static(ax)
    common_arrows(ax, mx, my, half)
    legend_box(ax)
    footer(fig, [
        "The 'sensor' idea as gameplay: a beam the player has to time. Reads best at dusk/night; Ger_Berlin is lit "
        "for day-time, so the beam needs a strong ground decal.",
        "Existing: spot light, decal kit. Base dressing of A included.",
        "New: a moving detection zone (detection zones themselves are new, flow 4.1) and a rotating light rig. "
        "Cost: medium-high.  Runner risk: none, but a 4th way to raise the alarm.",
    ])
    return fig


def main():
    os.makedirs(OUT, exist_ok=True)
    sheets = [("0_context", sheet_context), ("1_A_Kulisse", sheet_A), ("2_B_Probennahme", sheet_B),
              ("3_C_Sedierung", sheet_C), ("4_D_Pendel", sheet_D), ("5_E_Sucher", sheet_E)]
    paths = []
    for name, make in sheets:
        fig = make()
        path = f"{OUT}/ForgeStation_v01_{name}.png"
        fig.savefig(path, dpi=110, facecolor="white")
        plt.close(fig)
        paths.append(path)
        print("wrote", path)
    imgs = [Image.open(p).convert("RGB") for p in paths]
    w, h = imgs[0].size
    sheet = Image.new("RGB", (w * 3, h * 2), "white")
    for i, im in enumerate(imgs):
        sheet.paste(im.resize((w, h)), ((i % 3) * w, (i // 3) * h))
    sheet.save(f"{OUT}/ForgeStation_v01_all.png")
    small = sheet.resize((sheet.width // 3, sheet.height // 3), Image.LANCZOS)
    small.save(f"{OUT}/ForgeStation_v01_all_small.jpg", quality=88)
    print("pivot", PIVOT, "yaw deg", round(math.degrees(YAW), 1))
    for name, lo, hi, role in PARTS:
        c = to_world_cm((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2)
        print(f"{name:16s} {role:6s} world ({c[0]:.0f}, {c[1]:.0f})")
    print("rig", tuple(round(v) for v in to_world_cm(*RIG)), "console", tuple(round(v) for v in to_world_cm(*CONSOLE)))


if __name__ == "__main__":
    main()
