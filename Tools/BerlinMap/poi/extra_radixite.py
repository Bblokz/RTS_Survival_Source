"""Ger_Berlin extra Radixite fields, owner's pick 2026-11-18: sites E1, E2, E5, E6 of the concept round
Docs/Concepts/BerlinExtraRadixite/v01 (spec: D:/Blender/.../BerlinExtraRadixite/ber_sites.py).

Built only from the existing Radixite Blueprints, no Growth variants (Berlin Radixite does not root or regrow).
The concept positions were re-fitted on the live map (2026-11-18) with the measured Blueprint footprints: nothing on
the map is removed, the pieces are shifted / turned so they clear ruins, cover heaps, fences, rubble instances,
streets and each other. Every Blueprint of the concept mix is kept.

Labels RES_Extra_<site>_* under the outliner folder Berlin/ExtraRadixite; place() clears only its own prefix
(per site), so it can be rerun. Other prefixes (RES_Start_, BER_, POI_, RUIN_, FOOT_) are never touched.

    import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi")
    import extra_radixite as xr; importlib.reload(xr)
    xr.check_sites()          # read-only: what stands in (or next to) every piece footprint now
    xr.place()                # spawn, seated on the landscape by a pivot trace (ignore loop)
    xr.measure()              # measured footprints of the placed pieces vs the plan
"""
import math

import unreal

PREFIX = "RES_Extra_"
FOLDER = "Berlin/ExtraRadixite"
RAD = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/Resource_Radixite_"

# Planning footprints. Crater0 is fixed (measured mesh box, cm from the pivot at yaw 0). The crystal Blueprints roll
# their mesh, mesh yaw, scale (Large 1.17-1.63) and amount at construction, so their footprint is a circle round the
# pivot that holds the typical roll (Large meshes measured 560-710 cm out on the long side, 250-500 on the others).
CRATER_BOX = (-461.0, 524.0, -531.0, 438.0)
RADIUS = {
    "Crystal_Large": 550.0,
    "Crystal": 260.0,
    "Crystal_LargePoint": 240.0,
    "Crystal_SmallDecal": 220.0,
    "Crystal_NoDecal": 240.0,
}

# site: (name, concept centre, [(label, blueprint, dx, dy, yaw)]) offsets from the concept centre, +X north, +Y east.
# Fitted 2026-11-18 on the live map (traced objects, colliding components, instanced-mesh boxes, ber_data streets /
# side streets / hard keep-outs / ruin cores, landscape relief).
SITES = {
    "E1": ("Boxhagener Trichter", (1700.0, 46300.0), [
        ("Crater", "Crater0", -150, -100, 0),     # on the NE half of the old bomb-crater depression
        ("Crystal_1", "Crystal", 900, 500, 30),
        ("Crystal_2", "Crystal", -400, 700, 200),
        ("Small_1", "Crystal_SmallDecal", 1100, -900, 80),
        ("Small_2", "Crystal_SmallDecal", 600, -1300, 150),
    ]),
    "E2": ("Rosenthaler Trichter", (19650.0, 16850.0), [
        ("Crater", "Crater0", 122, -134, 0),
        ("Large", "Crystal_Large", 40, 950, 35),
        ("Crystal_1", "Crystal", 1100, -150, 120),
        ("Crystal_2", "Crystal", 200, -1100, 250),
        ("Small_1", "Crystal_SmallDecal", 1000, -1050, 10),
        ("Small_2", "Crystal_SmallDecal", -700, -400, 300),
    ]),
    "E5": ("Drehscheibe", (-6050.0, 20750.0), [
        ("Crater", "Crater0", -800, 100, 0),
        ("Large_1", "Crystal_Large", 400, -700, 70),
        ("Large_2", "Crystal_Large", 400, 600, 250),
        ("Crystal_1", "Crystal", -500, -800, 10),
        ("Crystal_2", "Crystal", -300, -1500, 190),
        ("NoDecal_1", "Crystal_NoDecal", 500, -1600, 100),
        ("NoDecal_2", "Crystal_NoDecal", 1000, -1950, 300),
    ]),
    "E6": ("Kraterrand Sued", (-8000.0, -2900.0), [
        ("Large_1", "Crystal_Large", -170, 320, 20),
        ("Large_2", "Crystal_Large", 500, -900, 300),
        ("Large_3", "Crystal_Large", 1000, 200, 80),
        ("Crystal_1", "Crystal", 1400, -600, 150),
        ("Crystal_2", "Crystal", -350, 1250, 40),
        ("Point", "Crystal_LargePoint", 1300, 1100, 0),
        ("NoDecal_1", "Crystal_NoDecal", 1100, -1600, 230),
        ("NoDecal_2", "Crystal_NoDecal", -600, 1850, 120),
    ]),
}
PIECE_GAP = 60.0        # cm between piece footprints
CLEAR = 60.0            # cm between a piece footprint and anything on the map
SKIP_CLASSES = ("DecalActor", "NiagaraActor", "Landscape", "LandscapeStreamingProxy", "LandscapeProxy",
                "RecastNavMesh", "NavMeshBoundsVolume", "WorldSettings", "PostProcessVolume", "Brush")


def _sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def pieces(sites=None):
    """[(site, full label, blueprint name, x, y, yaw, bp)] in world cm (last field: the planning shape key)."""
    out = []
    for key, (name, (cx, cy), pcs) in SITES.items():
        if sites and key not in sites:
            continue
        for label, bp, dx, dy, yaw in pcs:
            out.append((key, "%s%s_%s" % (PREFIX, key, label), bp, cx + dx, cy + dy, yaw, bp))
    return out


def _corners(x, y, yaw, bp, grow=0.0):
    """Planning footprint polygon: the Crater0 mesh box (turned by yaw) or a 16-gon for a crystal."""
    if bp == "Crater0":
        x0, x1, y0, y1 = CRATER_BOX
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        return [(x + a * c - b * s, y + a * s + b * c) for a, b in
                ((x1 + grow, y1 + grow), (x1 + grow, y0 - grow), (x0 - grow, y0 - grow), (x0 - grow, y1 + grow))]
    r = RADIUS[bp] + grow
    return [(x + r * math.cos(2 * math.pi * k / 16), y + r * math.sin(2 * math.pi * k / 16)) for k in range(16)]


def _inside(poly, x, y):
    c = False
    n = len(poly)
    for i in range(n):
        (x1, y1), (x2, y2) = poly[i], poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            c = not c
    return c


def _sat(pa, pb):
    """True when two convex polygons overlap."""
    for poly in (pa, pb):
        n = len(poly)
        for i in range(n):
            p, q = poly[i], poly[(i + 1) % n]
            nn = (q[1] - p[1], p[0] - q[0])
            da = [nn[0] * v[0] + nn[1] * v[1] for v in pa]
            db = [nn[0] * v[0] + nn[1] * v[1] for v in pb]
            if max(da) <= min(db) or max(db) <= min(da):
                return False
    return True


def _aabb_quad(x0, x1, y0, y1):
    return [(x1, y1), (x1, y0), (x0, y0), (x0, y1)]


def _own_actors():
    return [a for a in _sub().get_all_level_actors() if a.get_actor_label().startswith(PREFIX)]


def _trace_loop(x, y, ignore, z0=20000.0, z1=-20000.0, max_hits=24):
    """Single traces with a growing ignore list until the landscape: [(z, actor, comp)] top to bottom, landscape last."""
    ign = list(ignore)
    hits = []
    for _ in range(max_hits):
        h = unreal.SystemLibrary.line_trace_single(_world(), unreal.Vector(x, y, z0), unreal.Vector(x, y, z1),
                                                   unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ign,
                                                   unreal.DrawDebugTrace.NONE, True)
        if h is None:
            break
        t = h.to_tuple()
        if not t[0]:
            break
        actor, comp = t[9], t[10]
        hits.append((t[4].z, actor, comp))
        if actor is None or actor.get_class().get_name().startswith("Landscape"):
            break
        ign.append(actor)
    return hits


def ground_z(x, y, ignore=()):
    for z, actor, comp in _trace_loop(x, y, ignore):
        if actor is not None and actor.get_class().get_name().startswith("Landscape"):
            return z
    raise RuntimeError("no landscape under %.0f %.0f" % (x, y))


CRATER_RING = 450.0     # cm: the Crater0 rim radius


def crater_z(x, y, ignore=()):
    """Seat height for Crater0: the mean landscape height on its rim, not at the pivot. Most sites put the crater on
    an old bomb-crater depression; seated at the pivot (the bottom of the dip) the rim (64 cm above the pivot) is
    buried and the crater disappears. The bowl reaches 155 cm below the pivot, so it still meets the dip floor."""
    zs = [ground_z(x + CRATER_RING * math.cos(2 * math.pi * k / 12), y + CRATER_RING * math.sin(2 * math.pi * k / 12),
                   ignore) for k in range(12)]
    return max(sum(zs) / len(zs), ground_z(x, y, ignore))


def seat_craters(sites=None):
    """Re-seat the placed Crater0 pieces with crater_z (Crater0 rolls nothing, so moving it changes nothing else)."""
    own = _own_actors()
    for actor in own:
        lab = actor.get_actor_label()
        if not lab.endswith("_Crater") or (sites and lab.split("_")[2] not in sites):
            continue
        p = actor.get_actor_location()
        z = crater_z(p.x, p.y, own)
        actor.set_actor_location(unreal.Vector(p.x, p.y, z), False, False)
        print("seated %s z %.1f -> %.1f (pivot ground %.1f)" % (lab, p.z, z, ground_z(p.x, p.y, own)))


def layout_problems(sites=None):
    """Overlaps between the planned piece footprints (grown by half the gap each)."""
    out = []
    ps = pieces(sites)
    for i in range(len(ps)):
        for j in range(i + 1, len(ps)):
            a, b = ps[i], ps[j]
            if a[0] != b[0]:
                continue
            if _sat(_corners(a[3], a[4], a[5], a[6], PIECE_GAP / 2), _corners(b[3], b[4], b[5], b[6], PIECE_GAP / 2)):
                out.append("overlap %s / %s" % (a[1], b[1]))
    return out


def _ism_boxes(region):
    """Every instanced-mesh instance whose mesh box (times the instance transform) reaches into region
    (x0, x1, y0, y1): [(actor label, mesh, quad)]."""
    x0, x1, y0, y1 = region
    out = []
    for actor in _sub().get_all_level_actors():
        if actor.get_actor_label().startswith(PREFIX):
            continue
        for comp in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
            n = comp.get_instance_count()
            mesh = comp.static_mesh
            if not n or mesh is None:
                continue
            bb = mesh.get_bounding_box()
            mn, mx = bb.min, bb.max
            local = [(mn.x, mn.y), (mn.x, mx.y), (mx.x, mx.y), (mx.x, mn.y)]
            for k in range(n):
                tf = comp.get_instance_transform(k, True)
                p = tf.translation
                if not (x0 - 3000 <= p.x <= x1 + 3000 and y0 - 3000 <= p.y <= y1 + 3000):
                    continue
                pts = [unreal.MathLibrary.transform_location(tf, unreal.Vector(a, b, 0)) for a, b in local]
                xs, ys = [q.x for q in pts], [q.y for q in pts]
                if max(xs) < x0 or min(xs) > x1 or max(ys) < y0 or min(ys) > y1:
                    continue
                out.append((actor.get_actor_label(), mesh.get_name(), [(q.x, q.y) for q in pts]))
    return out


def _comp_quad(comp):
    """Oriented xy box of a component's local bounds in world space, or None."""
    try:
        r = comp.get_local_bounds()
    except Exception:
        return None
    mn, mx = r if isinstance(r, tuple) else (r.min, r.max)
    tf = comp.get_world_transform()
    pts = [unreal.MathLibrary.transform_location(tf, unreal.Vector(a, b, 0))
           for a, b in ((mx.x, mx.y), (mx.x, mn.y), (mn.x, mn.y), (mn.x, mx.y))]
    return [(p.x, p.y) for p in pts]


def _actor_boxes(region):
    """Oriented boxes of the colliding primitive components of every other actor reaching into region
    (per component, so one stray component at the world origin does not blow up an actor's bounds):
    [(label, class / component, quad)]. Instanced meshes are handled per instance in _ism_boxes."""
    x0, x1, y0, y1 = region
    out = []
    for actor in _sub().get_all_level_actors():
        lab = actor.get_actor_label()
        cls = actor.get_class().get_name()
        if lab.startswith(PREFIX) or cls.startswith(SKIP_CLASSES):
            continue
        p = actor.get_actor_location()
        if not (x0 - 8000 <= p.x <= x1 + 8000 and y0 - 8000 <= p.y <= y1 + 8000):
            continue
        for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
            cn = comp.get_class().get_name()
            if cn in ("BillboardComponent", "ArrowComponent", "TextRenderComponent") or "Decal" in cn \
                    or "Instanced" in cn or "Niagara" in cn:
                continue
            if comp.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION:
                continue
            q = _comp_quad(comp)
            if q is None:
                continue
            xs, ys = [v[0] for v in q], [v[1] for v in q]
            if max(xs) < x0 or min(xs) > x1 or max(ys) < y0 or min(ys) > y1:
                continue
            if max(xs) - min(xs) > 12000 or max(ys) - min(ys) > 12000:
                continue
            out.append((lab, "%s / %s" % (cls, comp.get_name()), q))
    return out


def check_sites(sites=None, step=50.0, clear=CLEAR, quiet=False):
    """Read-only: what stands in every planned piece footprint grown by `clear`: a trace grid (step cm, ignore loop
    past anything until the landscape, own pieces ignored), colliding component boxes of other actors and
    instanced-mesh boxes. Also the landscape relief under each footprint. Traces do not see streets: the street /
    side-street / keep-out masks were checked offline against ber_data (all 0 at the fitted positions).
    Returns ({piece label: [findings]}, {piece label: relief cm})."""
    ps = pieces(sites)
    own = _own_actors()
    found = {}
    reliefs = {}
    for key in sorted(set(p[0] for p in ps)):
        sp = [p for p in ps if p[0] == key]
        allc = [c for p in sp for c in _corners(p[3], p[4], p[5], p[6], clear)]
        region = (min(c[0] for c in allc), max(c[0] for c in allc), min(c[1] for c in allc), max(c[1] for c in allc))
        ab = _actor_boxes(region)
        ib = _ism_boxes(region)
        for site, label, bp, x, y, yaw, shape in sp:
            quad = _corners(x, y, yaw, shape, clear)
            core = _corners(x, y, yaw, shape)
            rows = []
            for lab, cls, q in ab:
                if _sat(quad, q):
                    rows.append("ACTOR %s (%s) box" % (lab, cls))
            for lab, mesh, q in ib:
                if _sat(quad, q):
                    rows.append("INSTANCE %s / %s" % (lab, mesh))
            traced = {}
            zs = []
            qx0, qx1 = min(v[0] for v in quad), max(v[0] for v in quad)
            qy0, qy1 = min(v[1] for v in quad), max(v[1] for v in quad)
            px = qx0
            while px <= qx1 + 1:
                py = qy0
                while py <= qy1 + 1:
                    if not _inside(quad, px, py):
                        py += step
                        continue
                    hits = _trace_loop(px, py, own)
                    gz = None
                    for z, actor, comp in hits:
                        if actor is not None and actor.get_class().get_name().startswith("Landscape"):
                            gz = z
                    if gz is not None and _inside(core, px, py):
                        zs.append(gz)
                    for z, actor, comp in hits:
                        if actor is None or actor.get_class().get_name().startswith("Landscape"):
                            continue
                        if gz is not None and z <= gz + 5:
                            continue
                        k2 = (actor.get_actor_label(), comp.get_name() if comp else "?")
                        n, top = traced.get(k2, (0, -1e9))
                        traced[k2] = (n + 1, max(top, z - (gz or 0)))
                    py += step
                px += step
            for (lab, comp), (n, top) in sorted(traced.items()):
                rows.append("TRACE %s / %s: %d cells, top %.0f cm" % (lab, comp, n, top))
            rel = (max(zs) - min(zs)) if zs else -1
            reliefs[label] = rel
            found[label] = rows
            if not quiet:
                print("%-26s %-20s %7.0f %7.0f yaw %3.0f relief %4.0f cm  %s" % (
                    label, bp, x, y, yaw, rel, "clear" if not rows else ""))
                for r in rows:
                    print("      " + r)
    for p in layout_problems(sites):
        print("LAYOUT", p)
    n = sum(1 for v in found.values() if v)
    print("check_sites: %d pieces, %d with findings, %d layout problems" % (len(found), n, len(layout_problems(sites))))
    return found, reliefs


def clear_own(sites=None):
    n = 0
    for actor in _own_actors():
        lab = actor.get_actor_label()
        if sites and not any(lab.startswith("%s%s_" % (PREFIX, k)) for k in sites):
            continue
        _sub().destroy_actor(actor)
        n += 1
    return n


def place(sites=None):
    problems = layout_problems(sites)
    if problems:
        raise RuntimeError("layout problems: %s" % problems)
    removed = clear_own(sites)
    own_left = _own_actors()
    totals = {}
    for site, label, bp, x, y, yaw, shape in pieces(sites):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(RAD + bp)
        if cls is None:
            raise RuntimeError("missing " + RAD + bp)
        z = crater_z(x, y, own_left) if bp == "Crater0" else ground_z(x, y, own_left)
        actor = _sub().spawn_actor_from_class(cls, unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw))
        actor.set_actor_label(label)
        actor.set_folder_path(FOLDER)
        own_left.append(actor)
        rc = actor.get_component_by_class(unreal.ResourceComponent)
        amount = rc.get_editor_property("total_amount")
        totals[site] = totals.get(site, 0) + amount
        print("placed %-26s %-20s at %7.0f %7.0f z %6.1f yaw %4.0f amount %d" % (label, bp, x, y, z, yaw, amount))
    print("removed %d old, placed %d, totals %s" % (removed, len(pieces(sites)), totals))
    return totals


def totals():
    out = {}
    for actor in _own_actors():
        site = actor.get_actor_label()[len(PREFIX):].split("_")[0]
        rc = actor.get_component_by_class(unreal.ResourceComponent)
        out[site] = out.get(site, 0) + rc.get_editor_property("total_amount")
    return out


def measure():
    """Placed pieces: footprint of the geometry (no billboard / decal / ResourceComponent) in actor-local cm,
    against the planned HALF. Returns {label: (min x, max x, min y, max y)}."""
    out = {}
    for actor in sorted(_own_actors(), key=lambda a: a.get_actor_label()):
        inv = actor.get_actor_transform()
        xs, ys = [], []
        for comp in actor.get_components_by_class(unreal.PrimitiveComponent):
            cn = comp.get_class().get_name()
            if cn in ("BillboardComponent", "DecalComponent", "ArrowComponent") or "Decal" in cn:
                continue
            try:
                r = comp.get_local_bounds()
            except Exception:
                continue
            mn, mx = r if isinstance(r, tuple) else (r.min, r.max)
            tf = comp.get_world_transform()
            for a in (mn.x, mx.x):
                for b in (mn.y, mx.y):
                    w = unreal.MathLibrary.transform_location(tf, unreal.Vector(a, b, 0))
                    loc = unreal.MathLibrary.inverse_transform_location(inv, w)
                    xs.append(loc.x)
                    ys.append(loc.y)
        if xs:
            out[actor.get_actor_label()] = (min(xs), max(xs), min(ys), max(ys))
            print("%-26s local x %6.0f..%6.0f  y %6.0f..%6.0f" % (actor.get_actor_label(), min(xs), max(xs), min(ys), max(ys)))
    return out


def _placed_quads(actor):
    """Oriented xy boxes of a placed piece's crystal / crater meshes (no decals, billboards, empty collections)."""
    out = []
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        if comp.get_class().get_name() != "StaticMeshComponent" or comp.static_mesh is None:
            continue
        q = _comp_quad(comp)
        if q:
            out.append((comp.get_name(), q))
    return out


def check_placed(sites=None, clear=30.0):
    """After place(): the rolled meshes of the placed pieces against each other and against everything else
    (component boxes, instanced-mesh boxes) grown by `clear`; a trace loop at every mesh-box corner and centre
    reports what stands there. Returns [problems]."""
    own = [a for a in _own_actors()
           if not sites or any(a.get_actor_label().startswith("%s%s_" % (PREFIX, k)) for k in sites)]
    probs = []
    meshes = [(a.get_actor_label(), n, q) for a in own for n, q in _placed_quads(a)]
    for i in range(len(meshes)):
        for j in range(i + 1, len(meshes)):
            if meshes[i][0] != meshes[j][0] and meshes[i][0].split("_")[2] == meshes[j][0].split("_")[2]                     and _sat(meshes[i][2], meshes[j][2]):
                probs.append("mesh box overlap %s / %s" % (meshes[i][0], meshes[j][0]))
    xs = [v[0] for m in meshes for v in m[2]]
    ys = [v[1] for m in meshes for v in m[2]]
    if not xs:
        return probs
    region = (min(xs), max(xs), min(ys), max(ys))
    ab = _actor_boxes(region)
    ib = _ism_boxes(region)
    for lab, comp, q in meshes:
        cx = sum(v[0] for v in q) / 4
        cy = sum(v[1] for v in q) / 4
        grown = [(cx + (v[0] - cx) * (1 + clear / max(1.0, math.hypot(v[0] - cx, v[1] - cy))),
                  cy + (v[1] - cy) * (1 + clear / max(1.0, math.hypot(v[0] - cx, v[1] - cy)))) for v in q]
        for l2, c2, q2 in ab:
            if _sat(grown, q2):
                probs.append("%s %s box vs ACTOR %s (%s)" % (lab, comp, l2, c2))
        for l2, m2, q2 in ib:
            if _sat(grown, q2):
                probs.append("%s %s box vs INSTANCE %s / %s" % (lab, comp, l2, m2))
        for px, py in list(q) + [(cx, cy)]:
            for z, actor, c3 in _trace_loop(px, py, own):
                if actor is not None and not actor.get_class().get_name().startswith("Landscape"):
                    probs.append("%s %s corner %.0f %.0f traced %s" % (lab, comp, px, py, actor.get_actor_label()))
    for p in probs:
        print("PLACED", p)
    print("check_placed: %d mesh boxes, %d findings" % (len(meshes), len(probs)))
    return probs


def move_to_plan(sites=None):
    """Move placed pieces to their SITES position (pivot trace seat) without re-spawning: set_actor_location keeps
    the current roll (the editor-subsystem transform setter re-runs the construction script and re-rolls)."""
    own = _own_actors()
    by = {a.get_actor_label(): a for a in own}
    for site, label, bp, x, y, yaw, shape in pieces(sites):
        a = by.get(label)
        if a is None:
            continue
        p = a.get_actor_location()
        if abs(p.x - x) < 1 and abs(p.y - y) < 1:
            continue
        z = crater_z(x, y, own) if bp == "Crater0" else ground_z(x, y, own)
        a.set_actor_location(unreal.Vector(x, y, z), False, False)
        print("moved %s to %.0f %.0f z %.1f" % (label, x, y, z))
