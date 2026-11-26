"""Ore Mountains life pass, step 1 (offline, numpy): plan the blending decals and the VAT animals.

    python ore_life_gen.py            writes Saved/OreMountains/life/life_plan.json and a preview PNG

Owner goal (2026-11-26): more detail without sacrificing performance; decals; better blending between parts of the
landscape; VAT animals that fit. The landscape material is at the 64-SRV limit (memory: landscape-srv-limit), so
blending is done with decals and with smoothing the existing weightmaps (ore_life_place.py), never new layers.

Inputs (all written by the map tools): Saved/OreMountains/terrain/Ore_layout.json, Ore_height.raw, Ore_layer_*.raw,
Ore_masks.npz, Ore_rockworks.json, Ore_crush_rocks.json, Ore_dress.json, and the read-only survey of the saved map
(Saved/OreMountains/life/survey.json from ore_life_survey.py) for the sites and buildings placed by the mission tools.

Decal entry: [label, material key, x, y, yaw, depth, half_y, half_z]; the placement projects straight down
(pitch -90) and snaps to the ground. Budget: about 300 decals, all small, with the default fade screen size so they
cull when the camera zooms out.
"""
import json
import math
import os

import numpy as np

T = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/terrain/"
OUT_DIR = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/life/"
RNG = np.random.default_rng(1976)
HEIGHT_CM = 100.0 / 128.0      # Ore_height.raw: uint16, 128 units per metre around 32768
MESH_SIDE_DEPTH_CAP = 180.0     # decals that touch rocks, cliffs or buildings: at most 1.8 m up their sides

MATERIALS = {
    "scree_fan": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones/MI_CliffStoneDecal_ScreeFan",
    "rubble_strip": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones/MI_CliffStoneDecal_RubbleStrip",
    "fine_gravel": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones/MI_CliffStoneDecal_FineGravel",
    "chip_scatter": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones/MI_CliffStoneDecal_ChipScatter",
    "half_slab": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/CliffStones/MI_CliffStoneDecal_HalfBuriedSlab",
    "dust_spill": "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Decals/MI_PathDecal_DustSpill",
    "mud_churn": "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Decals/MI_PathDecal_MudChurn",
    "rut_fade": "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Decals/MI_PathDecal_RutFade",
    "gravel": "/Game/Megascans/Decals/Gravel_vcedbbxh/MI_Gravel_vcedbbxh_1K",
    "arid_debris": "/Game/Megascans/Decals/Arid_Debris_vbzebfrh/MI_Arid_Debris_vbzebfrh_2K",
    "bones": "/Game/Megascans/Decals/Scattered_Bones_uk1ceb1h/MI_Scattered_Bones_uk1ceb1h_4K",
    "damp_patch": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/MI_OreCliffDecal_DampPatch",
    "lichen": "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/MI_OreCliffDecal_LichenCrust",
}
ANIMALS = {
    "crow": "/Game/RTS_Survival/Blueprints/Environment/Animated/Birds/DesertFlocks/BP_BirdFlock_Crow",
    "raven": "/Game/RTS_Survival/Blueprints/Environment/Animated/Birds/DesertFlocks/BP_BirdFlock_InkRaven",
    "goat": "/Game/RTS_Survival/Blueprints/Environment/Animated/FarmAnimals/Goat/BP_Wanderer_Goat",
    "chicken": "/Game/RTS_Survival/Blueprints/Environment/Animated/FarmAnimals/Chicken/BP_Wanderer_Chicken",
}


def load():
    lay = json.load(open(T + "Ore_layout.json"))
    L = lay["landscape"]
    n = L["verts_x"]
    h = np.fromfile(T + "Ore_height.raw", dtype=np.uint16).reshape(n, n).astype(float)
    layers = {}
    for name in lay["layers"]:
        path = T + "Ore_layer_%s.raw" % name
        raw = np.fromfile(path, dtype=np.uint8 if os.path.getsize(path) == n * n else np.uint16)
        layers[name] = raw.reshape(n, n).astype(float) / (255.0 if raw.dtype == np.uint8 else 65535.0)
    masks = dict(np.load(T + "Ore_masks.npz"))
    return lay, L, h, layers, masks


class Grid:
    """Texel <-> world. The terrain tools write every raw / mask as [iy, ix] (ore_terrain_gen.to_grid): row i runs
    along +Y, column j along +X."""

    def __init__(self, L):
        self.cx, self.cy = L["corner"]
        self.q = L["quad_cm"]
        self.n = L["verts_x"]

    def world(self, i, j):
        return self.cx + j * self.q, self.cy + i * self.q

    def texel(self, x, y):
        return int(round((y - self.cy) / self.q)), int(round((x - self.cx) / self.q))

    def inside_play(self, x, y, margin=1500.0):
        return abs(x) < 28475 - margin and abs(y) < 28475 - margin


def downslope_yaw(h, g, x, y):
    i, j = g.texel(x, y)
    i = min(max(i, 1), g.n - 2)
    j = min(max(j, 1), g.n - 2)
    dy = h[i + 1, j] - h[i - 1, j]
    dx = h[i, j + 1] - h[i, j - 1]
    if abs(dx) + abs(dy) < 1e-6:
        return float(RNG.uniform(0, 360))
    return math.degrees(math.atan2(-dy, -dx))


def thin(points, spacing):
    """Greedy spacing filter (points: list of (x, y, ...))."""
    kept = []
    for p in points:
        if all((p[0] - k[0]) ** 2 + (p[1] - k[1]) ** 2 >= spacing ** 2 for k in kept):
            kept.append(p)
    return kept


def mask_edge_points(mask, g, other=None):
    m = mask > 0.5
    edge = np.zeros_like(m)
    edge[1:-1, 1:-1] = m[1:-1, 1:-1] & ~(m[2:, 1:-1] & m[:-2, 1:-1] & m[1:-1, 2:] & m[1:-1, :-2])
    if other is not None:
        edge &= other
    ii, jj = np.nonzero(edge)
    order = RNG.permutation(len(ii))
    return [g.world(ii[k], jj[k]) for k in order]


def plan_decals(lay, L, h, layers, masks, survey):
    g = Grid(L)
    decals = []
    taken = []   # (x, y, r) to avoid stacking decals of different groups on one spot

    def free(x, y, r):
        return all((x - a) ** 2 + (y - b) ** 2 >= (r + c) ** 2 for a, b, c in taken)

    def fitted_depth(group, x, y, half_y, half_z):
        """Projection half-depth that just covers the landscape relief under the footprint (+60 cm). A deep box
        smears the decal up every wall, rock or building it touches (2026-11-26: a 5 m cliff-foot box streaked the
        cliff face), so groups that sit against meshes are capped lower."""
        i, j = g.texel(x, y)
        i, j = min(max(i, 0), g.n - 1), min(max(j, 0), g.n - 1)
        r = int(math.ceil(max(half_y, half_z) / g.q))
        win = h[max(i - r, 0):i + r + 1, max(j - r, 0):j + r + 1]
        reach = max(abs(win.max() - h[i, j]), abs(win.min() - h[i, j])) * HEIGHT_CM
        cap = MESH_SIDE_DEPTH_CAP if group in ("CliffFoot", "RockSeat", "SiteSeat") else 400.0
        return int(round(min(max(reach + 60.0, 100.0), cap)))

    def add(group, key, x, y, yaw, half_y, half_z, _old_depth=None, spacing_r=None):
        """_old_depth: the fixed depth of the first run, ignored now (fitted_depth)."""
        if not g.inside_play(x, y, 600.0):
            return False
        r = spacing_r if spacing_r is not None else 0.6 * max(half_y, half_z)
        if not free(x, y, r):
            return False
        depth = fitted_depth(group, x, y, half_y, half_z)
        taken.append((x, y, r))
        decals.append(["ORE_Life_%s_%03d" % (group, sum(1 for d in decals if d[0].startswith("ORE_Life_" + group))),
                       key, round(x), round(y), round(yaw % 360.0, 1), depth, round(half_y), round(half_z)])
        return True

    # A. Rock seating: big rockworks get a scree fan downslope and a fine-gravel ring; medium rocks a chip scatter;
    #    one small rock in six a small chip patch.
    rockworks = json.load(open(T + "Ore_rockworks.json"))["rocks"]
    for name, x, y, yaw, scale, kind in rockworks:
        d = downslope_yaw(h, g, x, y)
        rad = 350.0 * max(1.0, min(scale, 6.0)) ** 0.5
        add("RockSeat", "gravel", x, y, RNG.uniform(0, 360), rad * 1.4, rad * 1.4, 500, rad * 0.8)
        fx, fy = x + math.cos(math.radians(d)) * rad * 1.3, y + math.sin(math.radians(d)) * rad * 1.3
        add("RockSeat", "arid_debris", fx, fy, d - 90.0, rad * 1.1, rad * 0.8, 500, rad * 0.6)
    # Crushable rocks are pale grey: the CliffStone chips (tinted for the red cliff kit) read as orange flakes next
    # to them (2026-11-26 shots), so they get the neutral Megascans gravel.
    crush = json.load(open(T + "Ore_crush_rocks.json"))["rocks"]
    small_i = 0
    for name, x, y, yaw, scale, _z, kind in crush:
        if name.startswith("M"):
            add("RockSeat", "gravel", x, y, RNG.uniform(0, 360), 260 * scale, 260 * scale, 300, 150)
        else:
            small_i += 1
            if small_i % 6 == 0:
                add("RockSeat", "gravel", x, y, RNG.uniform(0, 360), 160 * scale, 160 * scale, 250, 110)

    # B. Cliff and escarpment feet: the outer edge of the cliff mask, every ~20 m, scree fan / rubble strip / slab.
    cliff = np.maximum(masks.get("cliff", 0), masks.get("escarpment", 0))
    feet = thin(mask_edge_points(cliff, g), 2000.0)
    cycle = ["scree_fan", "rubble_strip", "fine_gravel", "half_slab"]
    # The CliffStone decals (made for the Intro map, never used here before) read as orange flakes against this
    # map's pale rock (2026-11-26 shots), so every cliff / escarpment foot gets the neutral Megascans gravel / debris.
    neutral = {"scree_fan": "gravel", "rubble_strip": "arid_debris", "fine_gravel": "gravel", "half_slab": "arid_debris"}
    # With pitch -90 and yaw d - 90 the decal's Y half-size runs downslope (away from the wall) and Z along the
    # cliff, so a strip's long side goes in Z and the decal is pushed out by its whole downslope half-size.
    for k, (x, y) in enumerate(feet[:70]):
        d = downslope_yaw(h, g, x, y)
        key = cycle[k % len(cycle)]
        half_down, half_along = {"scree_fan": (300, 420), "rubble_strip": (180, 500), "fine_gravel": (300, 380),
                                 "half_slab": (200, 260)}[key]
        key = neutral[key]
        out = 250.0 + half_down
        ox, oy = x + math.cos(math.radians(d)) * out, y + math.sin(math.radians(d)) * out
        add("CliffFoot", key, ox, oy, d - 90.0, half_down, half_along, spacing_r=260)

    # C. Hard layer edges: where a weightmap changes fastest (contrasting layers), soften with gravel / debris / dust.
    names = list(layers)
    grad = np.zeros_like(h)
    for nm in ("IronOxide", "DarkEarth", "MountainTop", "RockDesert", "SlateScree", "Ochre", "Asphalt"):
        if nm in layers:
            gy, gx = np.gradient(layers[nm])
            grad = np.maximum(grad, np.hypot(gx, gy))
    road = masks.get("road", np.zeros_like(h)) > 0.5
    ii, jj = np.nonzero((grad > 0.33) & ~road)
    pts = [g.world(ii[k], jj[k]) for k in RNG.permutation(len(ii))]
    edge_keys = ["gravel", "arid_debris", "dust_spill", "rut_fade"]
    for k, (x, y) in enumerate(thin(pts, 2200.0)[:60]):
        add("LayerEdge", edge_keys[k % len(edge_keys)], x, y, RNG.uniform(0, 360), RNG.uniform(260, 420),
            RNG.uniform(220, 380), 400, 240)

    # D. Dead forest floor: sparse debris, a few bones, damp patches in bog-side woods.
    woods = masks.get("woods_dense", np.zeros_like(h)) > 0.5
    ii, jj = np.nonzero(woods)
    pts = thin([g.world(ii[k], jj[k]) for k in RNG.permutation(len(ii))[:6000]], 2400.0)
    for k, (x, y) in enumerate(pts[:34]):
        key = "bones" if k % 7 == 3 else ("damp_patch" if k % 5 == 1 else "arid_debris")
        size = (140, 110) if key == "bones" else (RNG.uniform(260, 380), RNG.uniform(220, 320))
        add("WoodFloor", key, x, y, RNG.uniform(0, 360), size[0], size[1], 300, 200)

    # E. Road shoulders: wear and dust along the valley road (alternating sides), mud churn at track junctions.
    roads = {p.get("name") or p.get("id"): p for p in lay["paths"]}
    valley = roads.get("valley_road")
    if valley:
        pts = valley["points"]
        acc, last = 0.0, None
        side = 1
        for (ax, ay), (bx, by) in zip(pts[:-1], pts[1:]):
            seg = math.hypot(bx - ax, by - ay)
            heading = math.degrees(math.atan2(by - ay, bx - ax))
            t = 0.0
            while t < seg:
                if acc >= 3500.0:
                    acc = 0.0
                    px, py = ax + (bx - ax) * t / seg, ay + (by - ay) * t / seg
                    nx, ny = -math.sin(math.radians(heading)) * side, math.cos(math.radians(heading)) * side
                    key = "dust_spill" if side > 0 else "rut_fade"
                    add("RoadShoulder", key, px + nx * 650, py + ny * 650, heading - 90.0, 420, 200, 300, 220)
                    side = -side
                step = min(500.0, seg - t)
                t += step
                acc += step
        for name in ("depot_track", "ammo_route", "flank_w", "flank_e"):
            p = roads.get(name)
            if p and p.get("points"):
                x, y = p["points"][0]
                add("Junction", "mud_churn", x, y, RNG.uniform(0, 360), 520, 420, 400, 300)

    # F. Sites and buildings: dust / churned ground around each, so they sit in the ground.
    for label, cls, x, y, z, ex, ey, yaw in survey.get("objects", []):
        if label.startswith("ORE_Prop_") and ("Sandbag" in label or "Crate" in label or "Barrel" in label):
            continue
        if "Fence" in label or "Fence" in cls or label.startswith("ORE_PH_"):
            continue
        if ex < 150 and ey < 150:
            continue
        if abs(ex - abs(x)) <= 1 and abs(ey - abs(y)) <= 1:
            # Bounds stretched to a component at the world origin (memory: actor-bounds-seating-trap): the survey's
            # origin is half-way to (0, 0), not the actor. Five poles / logs / kerbs on 2026-11-26.
            continue
        # MudChurn blown up past ~4.5 m reads as a dark hairy rosette (2026-11-26 battery shots): big footprints get
        # the light dust spill instead, and no seat is wider than 7 m.
        r = min(max(ex, ey) * 1.25, 700.0)
        busy = "Base" in label or "Battery" in label or "Checkpoint" in label or "Station" in label
        key = "mud_churn" if busy and r <= 450.0 else "dust_spill"
        add("SiteSeat", key, x, y, yaw + RNG.uniform(-15, 15), r, r * 0.8, 600, r * 0.5)
    return decals


def plan_animals(L, h, masks, survey):
    """Crows over the dead forest (3 flocks), ravens over the north woods (1), feral goats on the foothill slopes
    (2 herds), chickens in the Soviet convoy yard (Base A)."""
    g = Grid(L)
    animals = []
    woods = masks.get("woods_dense", np.zeros_like(h)) > 0.5
    # the three biggest wood blobs (coarse 20 m cells)
    cells = {}
    ii, jj = np.nonzero(woods)
    for i, j in zip(ii, jj):
        key = (i // 20, j // 20)
        cells[key] = cells.get(key, 0) + 1
    centres = []
    for (ci, cj), count in sorted(cells.items(), key=lambda kv: -kv[1]):
        x, y = g.world(ci * 20 + 10, cj * 20 + 10)
        if g.inside_play(x, y, 6000.0) and all(math.hypot(x - a, y - b) > 12000 for a, b in centres):
            centres.append((x, y))
        if len(centres) == 4:
            break
    for k, (x, y) in enumerate(centres):
        kind = "raven" if k == 3 else "crow"
        animals.append({"label": "ORE_Life_Flock_%d" % k, "kind": kind, "x": round(x), "y": round(y),
                        "yaw": float(RNG.uniform(0, 180)), "semi_x": 6000.0 + 1000 * k, "semi_y": 3200.0,
                        "birds": 7 if kind == "crow" else 6, "height": 1500.0, "scale": 0.45 if kind == "crow" else 0.55,
                        "speed": 850.0, "seed": 1976 + k, "clockwise": bool(k % 2)})
    # goats: open slopes in the north foothills (positive X), away from roads, woods, sites and the bases
    road = (masks.get("road", 0) + masks.get("track", 0)) > 0.3
    site_xy = [(o[2], o[3]) for o in survey.get("objects", [])]
    candidates = []
    gy, gx = np.gradient(h)
    slope = np.hypot(gx, gy)
    for i in range(0, g.n, 6):
        for j in range(0, g.n, 6):
            x, y = g.world(i, j)
            if x < 12000 or not g.inside_play(x, y, 3000.0):
                continue
            if road[i, j] or woods[i, j] or slope[i, j] < 8 or slope[i, j] > 60:
                continue
            if min((math.hypot(x - a, y - b) for a, b in site_xy), default=1e9) < 5000:
                continue
            candidates.append((x, y))
    herds = thin([candidates[k] for k in RNG.permutation(len(candidates))], 9000.0)[:2]
    for hk, (x, y) in enumerate(herds):
        for k in range(4 if hk == 0 else 3):
            a = RNG.uniform(0, 2 * math.pi)
            animals.append({"label": "ORE_Life_Goat_%d_%d" % (hk, k), "kind": "goat", "x": round(x + math.cos(a) * 350),
                            "y": round(y + math.sin(a) * 350), "yaw": float(RNG.uniform(0, 360)), "wander": 1600.0})
    # chickens: a few in the Base A yard, west of the plant, clear of the road
    for k, (ox, oy) in enumerate(((200, 25600), (450, 25900), (-100, 26100), (300, 26300))):
        animals.append({"label": "ORE_Life_Chicken_%d" % k, "kind": "chicken", "x": ox, "y": oy,
                        "yaw": float(RNG.uniform(0, 360)), "wander": 700.0})
    return animals


def preview(decals, animals, L, layers):
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return None
    g = Grid(L)
    base = (layers["MountainTop"] * 200 + layers["DarkEarth"] * 60 + layers["RockDesert"] * 120).clip(0, 255)
    img = Image.fromarray(base.T[::-1].astype(np.uint8)).convert("RGB")
    d = ImageDraw.Draw(img)
    colours = {"RockSeat": (255, 160, 0), "CliffFoot": (255, 60, 60), "LayerEdge": (60, 200, 255),
               "WoodFloor": (80, 255, 80), "RoadShoulder": (255, 255, 0), "Junction": (255, 0, 255), "SiteSeat": (255, 255, 255)}
    for label, key, x, y, yaw, depth, hy, hz in decals:
        i, j = g.texel(x, y)
        group = label.split("_")[2]
        r = max(2, int(max(hy, hz) / 100))
        d.ellipse([i - r, (g.n - 1 - j) - r, i + r, (g.n - 1 - j) + r], outline=colours.get(group, (200, 200, 200)))
    for a in animals:
        i, j = g.texel(a["x"], a["y"])
        if "semi_x" in a:
            rx, ry = int(a["semi_x"] / 100), int(a["semi_y"] / 100)
            d.ellipse([i - rx, (g.n - 1 - j) - ry, i + rx, (g.n - 1 - j) + ry], outline=(180, 0, 255))
        else:
            d.rectangle([i - 2, (g.n - 1 - j) - 2, i + 2, (g.n - 1 - j) + 2], fill=(255, 0, 140))
    path = OUT_DIR + "life_plan.png"
    img.resize((g.n * 2, g.n * 2)).save(path)
    return path


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    lay, L, h, layers, masks = load()
    survey = json.load(open(OUT_DIR + "survey.json")) if os.path.exists(OUT_DIR + "survey.json") else {}
    decals = plan_decals(lay, L, h, layers, masks, survey)
    animals = plan_animals(L, h, masks, survey)
    groups = {}
    for d in decals:
        groups[d[0].split("_")[2]] = groups.get(d[0].split("_")[2], 0) + 1
    json.dump({"materials": MATERIALS, "animals_bp": ANIMALS, "decals": decals, "animals": animals,
               "groups": groups}, open(OUT_DIR + "life_plan.json", "w"), indent=1)
    print("decals", len(decals), groups, "animals", len(animals), "preview", preview(decals, animals, L, layers))


if __name__ == "__main__":
    main()
