"""Placement generator for the Ger_Berlin ambient motion (plain Python, no Unreal):

    python Source/Tools/BerlinMap/ambient/ambient_gen.py

Reads   Saved/Berlin/terrain/Ger_Berlin_layout.json   lanes and roads ("paths"), the height file
        Saved/Berlin/terrain/Ger_Berlin_dress.json    the ruins (piece, x, y, z, yaw)
        BerlinAmbient/kit_attach_table.json           windows, perches and wire spans per kit piece (kit_attach_probe.py)
        Config/DefaultGame.ini                        WindYawDegrees of the RTS Ambient Effects settings (35 by default)
Writes  Saved/Berlin/terrain/Ger_Berlin_ambient.json, placed by ambient_place.py (step_ambient in berlin_map_build.py).

  litter      one Niagara component per lane stretch: [x, y, z, yaw, width scale]; local +X runs down the lane with
              the wind, lanes across the wind get fewer stretches
  dressing    per ruin (index into the dress file's "ruins"): curtains, shutters, cables, lamps and wire spans as
              transforms RELATIVE TO THE RUIN, so they are attached to it and go when it dies
  crow_groups perched crows per ruin, relative to the ruin: [x, y, z, yaw, scale, phase seconds, rate]
  crow_flocks circling groups (the existing BP_BirdFlock_Crow) over the densest ruin fields
  stamp       count and hash of the ruins this was made for; ambient_place.py refuses a dress file that differs

Its own random stream: rerunning gives the same file, and nothing else in the dress generator shifts.
"""
import array
import hashlib
import json
import math
import os
import random
import re

ROOT = "D:/UE5Projects/RTS_Survival/"
LAYOUT_FILE = ROOT + "Saved/Berlin/terrain/Ger_Berlin_layout.json"
DRESS_FILE = ROOT + "Saved/Berlin/terrain/Ger_Berlin_dress.json"
OUT_FILE = ROOT + "Saved/Berlin/terrain/Ger_Berlin_ambient.json"
ATTACH_FILE = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinAmbient/kit_attach_table.json"
GAME_INI = ROOT + "Config/DefaultGame.ini"
SEED = 19450502

# ---- tuning --------------------------------------------------------------------------------------------------------------
STRETCH_SPACING = 3600.0        # cm between litter components along a lane (the emitter box is 3000 long, particles travel on)
LITTER_KEEP_ALONG = 0.92        # share of stretches kept on a lane that runs with the wind ...
LITTER_KEEP_ACROSS = 0.40       # ... and on one that runs across it
LITTER_PATH_KINDS = ("lane", "road")
CURTAIN_CHANCE = 0.075           # per window
SHUTTER_CHANCE = 0.02
CABLE_CHANCE = 0.04             # per perch with a free wall face
LAMP_CHANCE = 0.008
SPAN_CHANCE = 0.25              # per ruin whose piece has a span
MAX_PER_RUIN = {"E1": 6, "E2": 6, "E3": 6}      # other pieces: 3
CROW_GROUP_CHANCE = 0.055       # per ruin at least CROW_MIN_HEIGHT high with 4+ perches
CROW_MIN_HEIGHT = 500.0
CROW_GROUP_SIZE = (3, 6)
FLOCKS = 3
FLOCK_SPACING = 30000.0         # cm between two circling groups
HEROES = ("E1", "E2", "E3")


def wind_yaw():
    try:
        text = open(GAME_INI, "r", encoding="utf-8", errors="ignore").read()
        section = text.split("[/Script/RTS_Survival.RTSAmbientFxSettings]")[1].split("\n[")[0]
        return float(re.search(r"WindYawDegrees=([-0-9.]+)", section).group(1))
    except Exception:  # noqa: BLE001
        return 35.0


class Ground:
    """Landscape height from the generator's height file, as berlin_map_build.ground_z reads it."""

    def __init__(self, layout):
        self.lay = layout["landscape"]
        with open(layout["files"]["height"], "rb") as handle:
            self.heights = array.array("H", handle.read())

    def z(self, x, y):
        lay = self.lay
        vx, vy = lay["verts_x"], lay["verts_y"]
        fx = min(max((x - lay["corner"][0]) / lay["quad_cm"], 0.0), vx - 1.001)
        fy = min(max((y - lay["corner"][1]) / lay["quad_cm"], 0.0), vy - 1.001)
        ix, iy = int(fx), int(fy)
        tx, ty = fx - ix, fy - iy

        def h(i, j):
            return (self.heights[j * vx + i] - 32768.0) * 100.0 / 128.0

        return (h(ix, iy) * (1 - tx) * (1 - ty) + h(ix + 1, iy) * tx * (1 - ty) + h(ix, iy + 1) * (1 - tx) * ty + h(ix + 1, iy + 1) * tx * ty)


def ruin_stamp(ruins):
    text = ";".join("%s,%.0f,%.0f,%.1f" % (r["piece"], r["x"], r["y"], r["yaw"]) for r in ruins)
    return {"ruins": len(ruins), "hash": hashlib.md5(text.encode()).hexdigest()}


# ---- litter --------------------------------------------------------------------------------------------------------------
def litter(layout, ground, rng):
    wind = math.radians(wind_yaw())
    bounds = layout["bounds"]
    items = []
    for path in layout["paths"]:
        if path["kind"] not in LITTER_PATH_KINDS:
            continue
        points = path["points"]
        carried = STRETCH_SPACING * rng.uniform(0.2, 0.8)
        for (ax, ay), (bx, by) in zip(points[:-1], points[1:]):
            length = math.hypot(bx - ax, by - ay)
            if length < 1.0:
                continue
            heading = math.atan2(by - ay, bx - ax)
            along = math.cos(heading - wind)
            if along < 0.0:                     # the litter runs down the lane in the direction the wind pushes it
                heading += math.pi
            keep = LITTER_KEEP_ACROSS + (LITTER_KEEP_ALONG - LITTER_KEEP_ACROSS) * abs(along)
            at = carried
            while at < length:
                x, y = ax + (bx - ax) * at / length, ay + (by - ay) * at / length
                at += STRETCH_SPACING
                inside = bounds["xmin"] + 1500 < x < bounds["xmax"] - 1500 and bounds["ymin"] + 1500 < y < bounds["ymax"] - 1500
                if inside and rng.random() < keep:
                    yaw = math.degrees(heading) + rng.uniform(-6.0, 6.0)
                    items.append([round(x, 1), round(y, 1), round(ground.z(x, y), 1), round((yaw + 180.0) % 360.0 - 180.0, 2),
                                  round(max(path.get("width", 800.0), 500.0) / 700.0, 2)])
            carried = at - length
    return items


# ---- dressing on the ruins ---------------------------------------------------------------------------------------------
def unique_windows(windows):
    """A window seen from both faces of a wall is one opening: keep the first (the +X street face comes first)."""
    kept = []
    for window in windows:
        px, py, pz = window["pos"]
        twin = any(abs(pz - k["pos"][2]) < 30 and math.hypot(px - k["pos"][0], py - k["pos"][1]) < 110
                   and abs(abs(window["yaw"] - k["yaw"]) - 180.0) < 1.0 for k in kept)
        if not twin:
            kept.append(window)
    return kept


def face(perch, rng, min_drop):
    """A free wall face beside a perch: (outward yaw, drop, distance from the perch to the face)."""
    faces = [(float(yaw), drop, edge) for yaw, (drop, edge) in perch["drops"].items() if drop >= min_drop]
    return rng.choice(faces) if faces else None


def dressing_for(index, ruin, attach, rng):
    piece_id = ruin["piece"].split("_")[0]
    budget = MAX_PER_RUIN.get(piece_id, 3)
    items = []

    def add(mesh, pos, yaw, roll=0.0, scale=(1.0, 1.0, 1.0)):
        if len(items) < budget:
            items.append([index, mesh, round(pos[0], 1), round(pos[1], 1), round(pos[2], 1), round(yaw, 1), 0.0, round(roll, 2),
                          round(scale[0], 3), round(scale[1], 3), round(scale[2], 3)])

    spans = list(attach["spans"])
    rng.shuffle(spans)
    for span in spans[: 2 if piece_id in HEROES else 1]:
        if rng.random() < SPAN_CHANCE:
            (ax, ay, az), (bx, by, bz) = span["a"], span["b"]
            if rng.random() < 0.5:
                ax, ay, az, bx, by, bz = bx, by, bz, ax, ay, az
            flat = math.hypot(bx - ax, by - ay)
            mesh, mesh_length = ("SM_BerlinAmb_WireSpan_A", 600.0) if flat < 800.0 else ("SM_BerlinAmb_WireSpan_B", 1000.0)
            # the span mesh runs down local -Y in Unreal: yaw puts -Y on the line a -> b, roll lifts the far end by dz
            yaw = math.degrees(math.atan2(by - ay, bx - ax)) + 90.0
            roll = math.degrees(math.atan2(bz - az, flat))
            add(mesh, (ax, ay, az + 4.0), yaw, roll, (1.0, math.hypot(flat, bz - az) / mesh_length, 1.0))
    windows = unique_windows(attach["windows"])
    rng.shuffle(windows)
    for window in windows:
        roll_dice = rng.random()
        px, py, pz = window["pos"]
        yaw = window["yaw"]
        if roll_dice < CURTAIN_CHANCE:
            wide = window["width"] >= 100.0 and rng.random() < 0.7
            mesh, mesh_width, mesh_height = ("SM_BerlinAmb_Curtain_A", 112.0, 170.0) if wide else ("SM_BerlinAmb_Curtain_B", 58.0, 205.0)
            scale_y = (window["width"] * (0.92 if wide else 0.5)) / mesh_width
            scale_z = min(window["height"] * rng.uniform(0.7, 0.95) / mesh_height, 1.15)
            add(mesh, (px, py, pz - 4.0), yaw, 0.0, (1.0, scale_y, scale_z))
        elif roll_dice < CURTAIN_CHANCE + SHUTTER_CHANCE:
            # hinge at one top corner of the opening; local +Y of the window frame is yaw + 90
            side = math.radians(yaw + 90.0)
            half = window["width"] / 2.0
            add("SM_BerlinAmb_Shutter", (px + math.cos(side) * half, py + math.sin(side) * half, pz - 12.0), yaw)
    perches = list(attach["perches"])
    rng.shuffle(perches)
    for perch in perches:
        roll_dice = rng.random()
        px, py, pz = perch["pos"]
        if roll_dice < CABLE_CHANCE:
            long_cable = rng.random() < 0.6
            found = face(perch, rng, 330.0 if long_cable else 220.0)
            if found is None:
                continue
            yaw, _drop, edge = found
            out = math.radians(yaw)
            mesh = "SM_BerlinAmb_CableDangle_A" if long_cable else "SM_BerlinAmb_CableDangle_B"
            add(mesh, (px + math.cos(out) * edge, py + math.sin(out) * edge, pz + 2.0), yaw, 0.0, (1.0, 1.0, rng.uniform(0.85, 1.1)))
        elif roll_dice < CABLE_CHANCE + LAMP_CHANCE:
            found = face(perch, rng, 300.0)
            if found is None:
                continue
            yaw, _drop, edge = found
            out = math.radians(yaw)
            reach = edge + 45.0                 # the bracket arm is 45 cm long and ends at the pivot
            add("SM_BerlinAmb_Lamp", (px + math.cos(out) * reach, py + math.sin(out) * reach, pz - 55.0), yaw)
    return items


def crow_group(index, attach, rng):
    perches = [p for p in attach["perches"] if p["pos"][2] >= CROW_MIN_HEIGHT * 0.8]
    if len(perches) < 4:
        return None
    first = rng.choice(perches)
    near = sorted(perches, key=lambda p: math.dist(p["pos"], first["pos"]))
    want = rng.randint(*CROW_GROUP_SIZE)
    birds = []
    for perch in near:
        if len(birds) >= want:
            break
        px, py, pz = perch["pos"]
        yaws = [float(y) for y in perch["drops"]]
        look = rng.choice(yaws) + rng.uniform(-35.0, 35.0)
        spots = [(0.0, 0.0)]
        # a wall top with a drop on both sides runs across those two faces: a second bird fits beside the first
        if any(abs(abs(a - b) - 180.0) < 1.0 for a in yaws for b in yaws) and rng.random() < 0.55:
            along = math.radians(yaws[0] + 90.0)
            gap = rng.uniform(32.0, 55.0)
            spots = [(-math.cos(along) * gap / 2, -math.sin(along) * gap / 2), (math.cos(along) * gap / 2, math.sin(along) * gap / 2)]
        for dx, dy in spots:
            if len(birds) < want:
                birds.append([round(px + dx, 1), round(py + dy, 1), round(pz + 1.0, 1), round(look + rng.uniform(-20, 20), 1),
                              round(rng.uniform(1.0, 1.22), 2), round(rng.uniform(0.0, 40.0), 2), round(rng.uniform(0.8, 1.25), 2)])
    return {"ruin": index, "birds": birds} if len(birds) >= 3 else None


def crow_flocks(ruins, ground, rng):
    """Circling groups over the places with the most tall ruins, well apart from each other."""
    tall = [r for r in ruins if r["piece"].split("_")[0] in ("E1", "E2", "E3", "B3", "B4", "B5", "B7")]
    scored = sorted(tall, key=lambda r: -sum(1 for o in tall if math.hypot(o["x"] - r["x"], o["y"] - r["y"]) < 6000.0))
    flocks = []
    for ruin in scored:
        if all(math.hypot(ruin["x"] - f["x"], ruin["y"] - f["y"]) >= FLOCK_SPACING for f in flocks):
            flocks.append({"x": round(ruin["x"], 0), "y": round(ruin["y"], 0), "z": round(ground.z(ruin["x"], ruin["y"]), 0),
                           "yaw": round(rng.uniform(0.0, 360.0), 0), "semi_x": round(rng.uniform(6000.0, 8500.0), 0),
                           "semi_y": round(rng.uniform(3200.0, 4500.0), 0), "height": 2500.0, "count": rng.randint(6, 8),
                           "seed": rng.randint(1, 9999), "clockwise": rng.random() < 0.5})
        if len(flocks) >= FLOCKS:
            break
    return flocks


def main():
    rng = random.Random(SEED)
    with open(LAYOUT_FILE, "r") as handle:
        layout = json.load(handle)
    with open(DRESS_FILE, "r") as handle:
        ruins = json.load(handle)["ruins"]
    with open(ATTACH_FILE, "r") as handle:
        table = json.load(handle)
    ground = Ground(layout)
    out = {"stamp": ruin_stamp(ruins), "wind_yaw": wind_yaw(), "litter": litter(layout, ground, rng), "dressing": [],
           "crow_groups": [], "crow_flocks": crow_flocks(ruins, ground, rng)}
    for index, ruin in enumerate(ruins):
        attach = table.get(ruin["piece"])
        if attach is None:
            continue
        ruin_rng = random.Random(SEED * 31 + index)         # one stream per ruin: a changed chance does not shift the others
        out["dressing"] += dressing_for(index, ruin, attach, ruin_rng)
        if attach["bounds"][5] >= CROW_MIN_HEIGHT and ruin_rng.random() < CROW_GROUP_CHANCE:
            group = crow_group(index, attach, ruin_rng)
            if group is not None:
                out["crow_groups"].append(group)
    with open(OUT_FILE, "w") as handle:
        json.dump(out, handle)
    counts = {}
    for item in out["dressing"]:
        counts[item[1]] = counts.get(item[1], 0) + 1
    print("ambient: wind yaw %.0f, litter stretches %d, dressing %d on %d ruins %s" % (
        out["wind_yaw"], len(out["litter"]), len(out["dressing"]), len({i[0] for i in out["dressing"]}), dict(sorted(counts.items()))))
    print("ambient: crow groups %d with %d birds, circling flocks %d -> %s" % (
        len(out["crow_groups"]), sum(len(g["birds"]) for g in out["crow_groups"]), len(out["crow_flocks"]), OUT_FILE))


if __name__ == "__main__":
    main()
