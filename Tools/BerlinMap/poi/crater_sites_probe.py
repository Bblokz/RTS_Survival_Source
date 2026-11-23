# Read-only probe for the crater pass on the OTHER Radixite fields of Ger_Berlin (sent to crater_editor_server.py).
# 1) every Radixite resource actor on the map, clustered into fields (40 m link distance);
# 2) per field (not the start field): landscape region export (heights + layers) into Saved/BerlinCrater/sites/<key>/,
#    every primitive component and instanced mesh whose box reaches the region, the pieces' measured boxes;
# 3) a 1 m on-nav grid per agent over the region (point = exported landscape height + 50 cm, 30 cm extent).
# Nothing is modified.
import array
import json
import math
import os

import unreal

ROOT = "D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/sites/"
PAD = 3000.0                      # region = piece boxes + 30 m
START = (17300.0, 47300.0)
os.makedirs(ROOT, exist_ok=True)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
NS = unreal.NavigationSystemV1
actors = sub.get_all_level_actors()
lands = [a for a in actors if isinstance(a, unreal.LandscapeProxy)]
land = lands[0]
loc, sc = land.get_actor_location(), land.get_actor_scale3d()
tl = land.get_editor_property("target_layers")
names = [str(k) for k in tl.keys()]
infos = [tl[k].get_editor_property("layer_info_obj") for k in tl.keys()]
navs = {n.get_name().replace("RecastNavMesh-", ""): n for n in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh)}
print("navmeshes", sorted(navs))

# ---- 1) Radixite actors
rad = []
for a in actors:
    cn = a.get_class().get_name()
    if "Radixite" in cn or "Radixite" in a.get_actor_label():
        p = a.get_actor_location()
        o, e = a.get_actor_bounds(False)
        rad.append(dict(label=a.get_actor_label(), cls=cn, x=p.x, y=p.y, z=p.z, yaw=a.get_actor_rotation().yaw,
                        bx=[o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z]))
clusters = []
for r in rad:
    hit = [c for c in clusters if any(math.hypot(r["x"] - q["x"], r["y"] - q["y"]) < 4000 for q in c)]
    merged = [r]
    for c in hit:
        merged += c
        clusters.remove(c)
    clusters.append(merged)
fields = []
for c in clusters:
    cx = sum(q["x"] for q in c) / len(c)
    cy = sum(q["y"] for q in c) / len(c)
    labels = sorted(q["label"] for q in c)
    key = None
    for q in labels:
        if q.startswith("RES_Extra_"):
            key = q.split("_")[2]
            break
    if math.hypot(cx - START[0], cy - START[1]) < 3000:
        key = "START"
    key = key or "F%d_%d" % (round(cx / 100), round(cy / 100))
    fields.append(dict(key=key, cx=cx, cy=cy, pieces=c))
    print("field %-14s centre (%.0f, %.0f) %d pieces: %s" % (key, cx, cy, len(c), ", ".join("%s[%s]" % (q["label"], q["cls"]) for q in sorted(c, key=lambda q: q["label"]))))
json.dump(fields, open(ROOT + "fields.json", "w"), indent=1)


FOOT_XF = []


def _mat(t):
    """2D affine of a transform: world xy = A @ local xy + b (local z ignored, foot pads are flat)."""
    o = t.transform_location(unreal.Vector(0, 0, 0))
    ex = t.transform_location(unreal.Vector(1, 0, 0)) - o
    ey = t.transform_location(unreal.Vector(0, 1, 0)) - o
    return [ex.x, ey.x, ex.y, ey.y, o.x, o.y, o.z]


def comp_rows(region):
    x0, x1, y0, y1 = region
    rows = []
    for a in actors:
        if a in lands:
            continue
        cn = a.get_class().get_name()
        if cn in ("RecastNavMesh", "NavMeshBoundsVolume", "WorldSettings", "PostProcessVolume", "SkyLight",
                  "DirectionalLight", "ExponentialHeightFog", "SkyAtmosphere", "VolumetricCloud", "LandscapeStreamingProxy"):
            continue
        o, e = a.get_actor_bounds(False)
        if o.x + e.x < x0 or o.x - e.x > x1 or o.y + e.y < y0 or o.y - e.y > y1:
            continue
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            if isinstance(c, unreal.InstancedStaticMeshComponent):
                continue
            o2, e2, _r = unreal.SystemLibrary.get_component_bounds(c)
            if o2.x + e2.x < x0 or o2.x - e2.x > x1 or o2.y + e2.y < y0 or o2.y - e2.y > y1:
                continue
            if e2.x > 20000 or e2.y > 20000:
                continue
            col = str(c.get_collision_enabled())
            obb = None
            if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
                bb = c.static_mesh.get_bounding_box()
                t = c.get_world_transform()
                obb = [[round(q.x, 1), round(q.y, 1)] for q in (t.transform_location(unreal.Vector(px, py, 0)) for px, py in
                       ((bb.min.x, bb.min.y), (bb.max.x, bb.min.y), (bb.max.x, bb.max.y), (bb.min.x, bb.max.y)))]
                if c.static_mesh.get_name().startswith("SM_BerlinFoot"):
                    FOOT_XF.append(dict(mesh=c.static_mesh.get_name(), m=_mat(t)))
            rows.append(dict(obb=obb, actor=a.get_actor_label(), acls=cn, comp=c.get_name(), ccls=c.get_class().get_name(),
                             box=[o2.x - e2.x, o2.x + e2.x, o2.y - e2.y, o2.y + e2.y, o2.z - e2.z, o2.z + e2.z],
                             col=col, nav=bool(c.get_editor_property("can_ever_affect_navigation")),
                             vis=bool(c.is_visible()), yaw=c.get_world_rotation().yaw,
                             mesh=(c.static_mesh.get_name() if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh else None)))
    return rows


def inst_rows(region):
    x0, x1, y0, y1 = region
    out = []
    for a in actors:
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            sm = c.static_mesh
            if sm is None:
                continue
            bb = sm.get_bounding_box()
            mn, mx = bb.min, bb.max
            col = str(c.get_collision_enabled())
            for k in range(c.get_instance_count()):
                t = c.get_instance_transform(k, True)
                p = t.translation
                if p.x < x0 - 1500 or p.x > x1 + 1500 or p.y < y0 - 1500 or p.y > y1 + 1500:
                    continue
                xs, ys = [], []
                for cx_ in (mn.x, mx.x):
                    for cy_ in (mn.y, mx.y):
                        q = t.transform_location(unreal.Vector(cx_, cy_, 0))
                        xs.append(q.x)
                        ys.append(q.y)
                if max(xs) < x0 or min(xs) > x1 or max(ys) < y0 or min(ys) > y1:
                    continue
                if sm.get_name().startswith("SM_BerlinFoot"):
                    FOOT_XF.append(dict(mesh=sm.get_name(), m=_mat(t)))
                obb = [[round(q.x, 1), round(q.y, 1)] for q in (t.transform_location(unreal.Vector(px, py, 0)) for px, py in
                       ((mn.x, mn.y), (mx.x, mn.y), (mx.x, mx.y), (mn.x, mx.y)))]
                out.append(dict(obb=obb, actor=a.get_actor_label(), comp=c.get_name(), mesh=sm.get_name(), col=col,
                                box=[min(xs), max(xs), min(ys), max(ys)], z=p.z,
                                h=(mx.z - mn.z) * t.scale3d.z))
    return out


for f in fields:
    if f["key"] == "START":
        continue
    d = ROOT + f["key"] + "/"
    os.makedirs(d, exist_ok=True)
    bx = [min(q["x"] for q in f["pieces"]) - 700, max(q["x"] for q in f["pieces"]) + 700,
          min(q["y"] for q in f["pieces"]) - 700, max(q["y"] for q in f["pieces"]) + 700]   # pivots (actor bounds can reach the origin)
    region = (bx[0] - PAD, bx[1] + PAD, bx[2] - PAD, bx[3] + PAD)
    i0 = int(math.floor((region[0] - loc.x) / sc.x))
    i1 = int(math.ceil((region[1] - loc.x) / sc.x))
    j0 = int(math.floor((region[2] - loc.y) / sc.y))
    j1 = int(math.ceil((region[3] - loc.y) / sc.y))
    ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
        land, i0, j0, i1, j1, d + "orig_height.r16", infos, [d + "orig_%s.r8" % n for n in names])
    grid = dict(i0=i0, j0=j0, i1=i1, j1=j1, lx=loc.x, ly=loc.y, lz=loc.z, sx=sc.x, sy=sc.y, sz=sc.z, layers=names,
                region=region)
    json.dump(grid, open(d + "grid.json", "w"), indent=1)
    comps = comp_rows(region)
    inst = inst_rows(region)
    # on-nav grid
    nx, ny = i1 - i0 + 1, j1 - j0 + 1
    h = array.array("H")
    h.frombytes(open(d + "orig_height.r16", "rb").read())
    navgrid = {}
    for agent in ("LightTank", "HeavyTank"):
        rows = []
        for jj in range(ny):
            row = []
            for ii in range(nx):
                x = loc.x + (ii + i0) * sc.x
                y = loc.y + (jj + j0) * sc.y
                z = loc.z + (h[jj * nx + ii] - 32768) * sc.z / 128.0
                q = NS.project_point_to_navigation(w, unreal.Vector(x, y, z + 50), navs[agent], None, unreal.Vector(30, 30, 250))
                row.append(0 if (q is None or (q.x == 0 and q.y == 0 and q.z == 0)) else 1)
            rows.append("".join(map(str, row)))
        navgrid[agent] = rows
    foot = list(FOOT_XF)
    FOOT_XF.clear()
    json.dump(dict(comps=comps, inst=inst, nav=navgrid, export_ok=bool(ok), foot_xf=foot), open(d + "probe.json", "w"))
    print("field %s region %s export %s verts %dx%d comps %d inst %d nav LT %d HT %d" % (
        f["key"], [round(v) for v in region], ok, nx, ny, len(comps), len(inst),
        sum(r.count("1") for r in navgrid["LightTank"]), sum(r.count("1") for r in navgrid["HeavyTank"])))
    for c in comps:
        if c["col"] != "CollisionEnabled.NO_COLLISION" or c["nav"]:
            pass
    cls = {}
    for c in comps:
        cls[c["acls"]] = cls.get(c["acls"], 0) + 1
    print("   actor classes:", sorted(cls.items(), key=lambda t: -t[1])[:40])
    spawn = sorted(set(c["actor"] for c in comps if any(k in c["acls"] for k in ("Enemy", "Spawner", "Placement", "Route", "Strong", "Mission", "Trigger"))))
    print("   gameplay actors:", spawn)


# foot pad meshes: LOD0 triangles (local xy) for exact footprints
geo = {}
for f in fields:
    if f["key"] == "START":
        continue
    pr = json.load(open(ROOT + f["key"] + "/probe.json"))
    for fx in pr["foot_xf"]:
        geo.setdefault(fx["mesh"], None)
for name in list(geo):
    found = unreal.EditorAssetLibrary.list_assets("/Game/", recursive=True, include_folder=False)
    break
paths = {}
for pth in unreal.EditorAssetLibrary.list_assets("/Game/RTS_Survival", recursive=True, include_folder=False):
    nm = pth.split(".")[-1]
    if nm in geo:
        paths[nm] = pth
for name, pth in paths.items():
    sm = unreal.load_asset(pth)
    tris = []
    for sec in range(sm.get_num_sections(0)):
        v, t, n, uv, tan = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(sm, 0, sec)
        vs = [(round(q.x, 1), round(q.y, 1)) for q in v]
        tris += [[vs[t[k]], vs[t[k + 1]], vs[t[k + 2]]] for k in range(0, len(t), 3)]
    geo[name] = tris
    print("foot mesh", name, len(tris), "tris")
json.dump(geo, open(ROOT + "foot_geo.json", "w"))
