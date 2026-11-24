"""Applies Saved/IntroDetail/plan.json inside the live editor. exec() this after intro_detail_lib.py in one namespace.

Steps (each safe to re-run):  erode()  ->  paint_all(start, count)  ->  crater_paint()  ->  spawn_decals()  ->  spawn_props()
clean() removes every actor this tool spawned (label prefix IDT_); undo_erosion() puts the surveyed heights back.
Existing actors are never modified.
"""
import json
import math
import unreal

PAINT_Z = 3000.0     # far above the terrain: with lower-only the heights stay untouched and only the layer is painted


def plan():
    return json.load(open(OUT + "plan.json"))


def _down_yaw(x, y, r):
    gx = (ground_z(x + r, y) or 0) - (ground_z(x - r, y) or 0)
    gy = (ground_z(x, y + r) or 0) - (ground_z(x, y - r) or 0)
    if abs(gx) + abs(gy) < 8:
        return None
    return math.degrees(math.atan2(-gy, -gx))


def bowl(x, y, r, depth, yaw):
    """Crater-like hollow. The strip runs down the slope so the level cross-section never cuts a terrace."""
    dy = _down_yaw(x, y, r)
    yaw = yaw if dy is None else dy
    c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
    pts = []
    for i in range(9):
        t = -1.0 + i * 0.25
        px, py = x + c * r * t, y + s * r * t
        g = backup_z(px, py)          # surveyed height, so a re-run never digs deeper
        if g is None:
            g = ground_z(px, py)
        if g is None:
            return False
        pts.append((px, py, g - depth * (math.cos(t * math.pi) * 0.5 + 0.5)))
    apply(pts, r * 0.42, r * 0.78, lower=True, raise_=False, subdiv=24)
    return True


def trough(pts2d, w, f, depth):
    """Shallow channel along a polyline, tapered to nothing at both ends."""
    dense = []
    for a, b in zip(pts2d[:-1], pts2d[1:]):
        n = max(1, int(math.hypot(b[0] - a[0], b[1] - a[1]) / 150))
        for i in range(n):
            dense.append((a[0] + (b[0] - a[0]) * i / n, a[1] + (b[1] - a[1]) * i / n))
    dense.append(tuple(pts2d[-1]))
    out = []
    n = len(dense)
    for i, (px, py) in enumerate(dense):
        g = backup_z(px, py)
        if g is None:
            g = ground_z(px, py)
        if g is None:
            continue
        taper = min(1.0, i / 3.0, (n - 1 - i) / 3.0)
        out.append((px, py, g - depth * taper))
    if len(out) >= 2:
        apply(out, w, f, lower=True, raise_=False, subdiv=len(out) * 3)


def erode():
    p = plan()
    done = []
    for d in p["dips"]:
        before = ground_z(d["x"], d["y"])
        ok = bowl(d["x"], d["y"], d["r"], d["depth"], d["yaw"])
        after = ground_z(d["x"], d["y"])
        done.append("%s %d,%d r%d  %s -> %s" % (d["why"][:22], d["x"], d["y"], d["r"], round(before or 0), round(after or 0)))
    for t in p["troughs"]:
        trough(t["pts"], t["w"], t["f"], t["depth"])
        m = t["pts"][len(t["pts"]) // 2]
        done.append("%s mid %d,%d -> %s (was %s)" % (t["why"], m[0], m[1], round(ground_z(m[0], m[1]) or 0),
                                                    round(backup_z(m[0], m[1]) or 0)))
    log("erode.txt", "\n".join(done))
    return len(done)


def undo_erosion():
    p = plan()
    for d in p["dips"]:
        m = d["r"] * 1.4 + 200
        restore(d["x"] - m, d["x"] + m, d["y"] - m, d["y"] + m)
    for t in p["troughs"]:
        xs, ys = [q[0] for q in t["pts"]], [q[1] for q in t["pts"]]
        restore(min(xs) - 500, max(xs) + 500, min(ys) - 500, max(ys) + 500)


def paint_all(start=0, count=100000):
    strokes = plan()["paint"][start:start + count]
    for s in strokes:
        apply([(q[0], q[1], PAINT_Z) for q in s["pts"]], s["w"], s["f"], lower=True, raise_=False, paint=s["layer"],
              subdiv=max(4, len(s["pts"]) * 2))
    return len(strokes)


def unpaint_v1():
    """Paint Sand back over the first-version strokes (and its crater/trough paint), outside the user's own patches."""
    p = plan()
    keep = p["user_paint"]
    hit_user = lambda pts: any(math.hypot(q[0] - u[0], q[1] - u[1]) < u[2] for q in pts for u in keep)
    n = 0
    for s in p["paint_v1"]:
        if s["layer"] == "Sand" or hit_user(s["pts"]):
            continue
        apply([(q[0], q[1], PAINT_Z) for q in s["pts"]], s["w"] + 50, s["f"] + 80, lower=True, raise_=False,
              paint="Sand", subdiv=max(4, len(s["pts"]) * 2))
        n += 1
    for d in p["dips"]:
        if d.get("paint"):
            r = d["r"]
            apply([(d["x"] - r * 0.2, d["y"], PAINT_Z), (d["x"] + r * 0.2, d["y"], PAINT_Z)], r * 0.3, r * 0.7,
                  lower=True, raise_=False, paint="Sand", subdiv=6)
    for t in p["troughs"]:
        if not hit_user(t["pts"]):
            apply([(q[0], q[1], PAINT_Z) for q in t["pts"]], t["w"], t["f"] + 60, lower=True, raise_=False,
                  paint="Sand", subdiv=len(t["pts"]) * 3)
    return n


def crater_paint():
    n = 0
    for d in plan()["dips"]:
        if d.get("paint"):
            r = d["r"]
            apply([(d["x"] - r * 0.15, d["y"], PAINT_Z), (d["x"] + r * 0.15, d["y"], PAINT_Z)], r * 0.22, r * 0.62,
                  lower=True, raise_=False, paint=d["paint"], subdiv=6)
            n += 1
    for t in plan()["troughs"]:
        apply([(q[0], q[1], PAINT_Z) for q in t["pts"]], t["w"] * 0.6, t["f"], lower=True, raise_=False, paint=t["paint"],
              subdiv=len(t["pts"]) * 3)
    return n


def repair():
    """Puts the user's own dark patches back (see intro_detail_repair.py): sand over the first-pass stone dabs inside
    the two lake zones, then the stored dark shapes."""
    rep = json.load(open(OUT + "repair.json"))
    n = 0
    for z in rep["zones"]:
        x0, x1, y0, y1 = z["box"]
        if z["name"] != "hut":
            for s in plan()["paint_v1"]:
                if s["layer"] != "Sand" and any(x0 <= q[0] <= x1 and y0 <= q[1] <= y1 for q in s["pts"]):
                    apply([(q[0], q[1], PAINT_Z) for q in s["pts"]], s["w"] + 50, s["f"] + 80, lower=True,
                          raise_=False, paint="Sand", subdiv=max(4, len(s["pts"]) * 2))
        for s in z["strokes"]:
            apply([(q[0], q[1], PAINT_Z) for q in s["pts"]], s["w"], s["f"], lower=True, raise_=False,
                  paint=s["layer"], subdiv=8)
            n += 1
    return n


# ---------------------------------------------------------------- actors
def mine():
    return [a for a in actors() if a.get_actor_label().startswith(PREFIX)]


def clean(tag=None):
    n = 0
    for a in mine():
        if tag is None or a.get_actor_label().startswith(PREFIX + tag):
            a.destroy_actor()
            n += 1
    return n


def _hit(x, y):
    global _ignore
    if _ignore is None:
        ground_z(x, y)
    hit = unreal.SystemLibrary.line_trace_single(
        world(), unreal.Vector(x, y, 6000), unreal.Vector(x, y, -3000),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, _ignore, unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None, None
    t = hit.to_tuple()
    return t[4], t[7]      # location, impact normal


def spawn_decals():
    clean("Decal")
    n, missing = 0, set()
    for i, d in enumerate(plan()["decals"]):
        mat = unreal.load_asset(d["mat"])
        loc, _ = _hit(d["x"], d["y"])
        if not mat or loc is None:
            missing.add(d["mat"])
            continue
        a = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.DecalActor, unreal.Vector(d["x"], d["y"], loc.z), unreal.Rotator(pitch=-90, yaw=d["yaw"], roll=0))
        a.decal.set_decal_material(mat)
        a.decal.set_editor_property("decal_size", unreal.Vector(d["size"][0], d["size"][1], d["size"][2]))
        a.decal.set_editor_property("sort_order", 2 if d["tag"] in ("Crater", "DustSpill", "RoadDamage") else 1)
        a.set_actor_label("%sDecal_%s_%03d" % (PREFIX, d["tag"], i))
        a.set_folder_path("%s/Decals/%s" % (FOLDER, d["tag"]))
        n += 1
    log("decals.txt", "spawned %d\nmissing %s" % (n, sorted(missing)))
    return n


def spawn_props(tags=None):
    """tags: only (re)spawn these prop tags, e.g. ("Rocky", "RimRock"); None = all."""
    if tags is None:
        clean("Prop")
    else:
        for t in tags:
            clean("Prop_" + t)
    n, missing = 0, set()
    for i, p in enumerate(plan()["props"]):
        if tags is not None and p["tag"] not in tags:
            continue
        asset = unreal.load_asset(p["asset"])
        loc, nrm = _hit(p["x"], p["y"])
        if not asset or loc is None:
            missing.add(p["asset"])
            continue
        rot = unreal.Rotator(pitch=0, yaw=p["yaw"], roll=0)
        if p["align"] and nrm is not None:
            fwd = unreal.Vector(math.cos(math.radians(p["yaw"])), math.sin(math.radians(p["yaw"])), 0)
            rot = unreal.MathLibrary.make_rot_from_zx(nrm, fwd)
        pos = unreal.Vector(p["x"], p["y"], loc.z - p["sink"])
        if p["cls"]:
            a = unreal.EditorLevelLibrary.spawn_actor_from_class(asset.generated_class(), pos, rot)
        else:
            a = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.StaticMeshActor, pos, rot)
            c = a.static_mesh_component
            c.set_static_mesh(asset)
            s = p["s"]
            if p["tag"] == "Cactus":
                s = min(s, 270.0 / max(asset.get_bounding_box().max.z, 1.0))
            a.set_actor_scale3d(unreal.Vector(s, s, s))
            c.set_collision_profile_name("NoCollision")
            c.set_editor_property("can_ever_affect_navigation", False)
            c.set_editor_property("generate_overlap_events", False)
            c.set_mobility(unreal.ComponentMobility.STATIC)
        a.set_actor_label("%sProp_%s_%03d" % (PREFIX, p["tag"], i))
        a.set_folder_path("%s/Props/%s" % (FOLDER, p["tag"]))
        n += 1
    log("props.txt", "spawned %d\nmissing %s" % (n, sorted(missing)))
    return n
