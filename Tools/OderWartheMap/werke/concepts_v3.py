"""Round 3: three refined Felsnase (7a-c), three refined Werkgruppe (8a-c) and the new Peilturm (10).
German details added in this round: Tobruk ringstands (Ringstand 58c), the Werkeingang (entrance with a splinter wall
in front of the door and a close-defence port), Regelbau 667-style casemates that fire sideways along the teeth behind a
projecting Flankenmauer, steel embrasure plates, earth heaped over the roofs (only the concrete fronts bare), stepped
wing walls, a dry anti-tank ditch with a concrete counterscarp, and the Peilstand tower (stacked observation slots)."""
import math
import numpy as np
from concepts import (Werk, X, Y, smoothstep, blur, seg_field, poly_sdf, rect, mesa, notch, hill_union, arc_pts)


# ----------------------------------------------------------------------------------------------------- shared hills
def felsnase_hill(w, glacis=False, top=13.5, roof_slope=None):
    """Crag-and-tail: 13.5 m nose at the front, tail tapering back to the tunnel. glacis=True: the nose front is an
    earth glacis (no concrete face), the Oder-Warthe way of turning the enemy side into earth."""
    hx = np.clip((X + 46.0) / 58.0, 0.0, 1.0)
    height = top * np.sqrt(hx)
    half_w = 9.0 + 9.0 * hx
    d = np.maximum(np.abs(Y) - half_w * 0.55, 0.0)
    side = np.clip(1.0 - d / (height / math.tan(math.radians(30.0)) + 1e-3), 0.0, 1.0)
    z = height * side * (X <= 14.0)
    if glacis:
        nose = mesa(poly_sdf([(10.0, -16.0), (10.0, 16.0), (-2.0, 20.0), (-2.0, -20.0)]), 13.5, 24.0, 1.5)
        w.H = blur(np.maximum(z, nose), 1.2)
    else:
        nose = mesa(poly_sdf([(14.0, -18.0), (14.0, 18.0), (2.0, 21.0), (2.0, -21.0)]), top, 34.0, 1.0)
        w.H = blur(np.maximum(z, nose), 1.0)
        cap = np.minimum(0.3 + np.maximum(np.abs(Y) - 14.0, 0.0) * 1.2, top - (X - 13.0) * 0.67)
        w.H = np.where(X > 13.0, np.minimum(w.H, np.maximum(cap, 0.3)), w.H)
        if roof_slope is not None:
            # (user 2026-11-17: "smooth out the top part") no earth cliff over the casemates: from the earth over their
            # roofs (z_roof at x_lip) the nose rises at deg degrees to its top, across the casemate front only
            x_lip, z_roof, deg = roof_slope
            ramp = z_roof + np.maximum(x_lip - X, 0.0) * math.tan(math.radians(deg))
            across = 1.0 - smoothstep(15.5, 19.0, np.abs(Y))
            w.H = np.where(X > 0.0, w.H * (1.0 - across) + np.minimum(w.H, ramp) * across, w.H)
        w.H = blur(w.H, 0.6)
    w.paint["sod"] += smoothstep(5.0, 10.0, w.H)


def stepped_wings(w, fx, fy, half, h0, sg_list=(-1.0, 1.0), base=0.3):
    """Two-step wing walls (Fluegelmauern) either side of a casemate face."""
    for sg in sg_list:
        a = (fx + 0.4, fy + sg * (half + 0.6))
        b = (fx + 4.2, fy + sg * (half + 2.2))
        c = (fx + 8.5, fy + sg * (half + 4.0))
        w.ear(a, b, h0, h0 * 0.62, base=base)
        w.ear(b, c, h0 * 0.5, 0.6, base=base)


def werkgruppe_hills(w, glacis=False):
    hills = [
        ([(22.0, -14.0), (22.0, 14.0), (8.0, 20.0), (-10.0, 16.0), (-12.0, -12.0), (4.0, -20.0)], 10.0, 30.0),
        ([(2.0, 26.0), (-2.0, 44.0), (-20.0, 46.0), (-26.0, 30.0), (-12.0, 22.0)], 7.0, 30.0),
        ([(0.0, -26.0), (-8.0, -22.0), (-30.0, -38.0), (-24.0, -46.0), (-4.0, -42.0)], 7.0, 30.0),
        ([(-26.0, 0.0), (-28.0, 12.0), (-40.0, 12.0), (-42.0, -2.0), (-34.0, -6.0)], 5.0, 28.0),
    ]
    zs = []
    for k, (p, h, sl) in enumerate(hills):
        sdf = poly_sdf(p)
        if glacis:
            # front sides fall gently (a glacis), back sides steeper
            run_front = h / math.tan(math.radians(20.0))
            run_back = h / math.tan(math.radians(30.0))
            run = np.where(X > np.mean([q[0] for q in p]), run_front, run_back)
            zs.append(blur(h * np.clip(1.0 - np.maximum(sdf, 0.0) / run, 0.0, 1.0), 1.2))
        else:
            zs.append(mesa(sdf, h, sl, 1.2))
    z = hill_union(*zs)
    trench_d = np.full(X.shape, 1e9)
    for line in ([(-2.0, 16.0), (-8.0, 24.0)], [(-6.0, -16.0), (-12.0, -28.0)], [(-12.0, 2.0), (-28.0, 4.0)],
                 [(-24.0, 14.0), (-28.0, 26.0)], [(-30.0, -6.0), (-20.0, -30.0)]):
        trench_d = np.minimum(trench_d, seg_field(line)[0])
    w.H = z - 2.0 * smoothstep(1.6, 0.8, trench_d)
    w.paint["mud"] += smoothstep(1.8, 1.0, trench_d)
    w.paint["gravel"] += smoothstep(10.0, 6.0, np.hypot(X + 18.0, Y - 4.0))
    w.paint["sod"] += smoothstep(4.0, 8.0, z)


def tobruk(w, x, y):
    w.mod("tobruk", x, y, 0.0, base=w.ground(x, y) - 0.55, W=3.2, D=3.2, H=1.0)


# ----------------------------------------------------------------------------------------------------- Felsnase
def c7a():
    w = Werk("7a", "Felsnase A", "Batterienase: an earth-covered two-storey Longues casemate, stepped wings, Tobruks")
    felsnase_hill(w)
    w.paint["gravel"] += poly_sdf(rect(21.0, 0.0, 12.0, 32.0)) < 0
    w.bank(15.0, 0.0, 0.0, 26.0, 14.0, 0.3 + 10.5 + 1.0, 30.0, front_open=True, cover_from=-4.5, reach=3.0)
    w.mod("m272", 15.0, 0.0, 0.0, base=0.3, W=26.0, D=14.0, H=10.5, n=3, door=True, storeys=2)
    stepped_wings(w, 15.0, 0.0, 13.0, 6.5)
    for sg in (-1.0, 1.0):
        tobruk(w, 6.0, sg * 15.0)
    for x, y in ((-4.0, 0.0), (-19.0, -3.0), (-33.0, 2.0)):
        w.cupola(x, y)
    w.mod("eingang", -38.0, 9.0, 90.0, W=7.0, D=6.0, H=3.6)
    w.teeth(arc_pts(6.0, 0.0, 38.0, -84.0, 84.0, 60), gaps=[(20.0, 30.0)])
    w.mast(-26.0, 14.0)
    w.portal()
    w.hq = [(16.0, 0.0, "command entrance (main gate)"), (-26.0, 14.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (15.0, 0.0, 0.3, 0.0, 19.0, -14.0, 2.0, 5.5)
    w.scale_units(52.0, 26.0)
    w.notes = {
        "form": "The crag-and-tail of concept 7: a 13.5 m nose, the tail tapering 60 m back to the tunnel; 36 m wide at the nose.",
        "bunker": "A two-storey Longues casemate (26 m rounded front, stepped ports, gun slot under the cantilevered roof) with the earth heaped over its roof, so only the concrete front shows, as on the real batteries. Stepped wing walls; two Tobruk ringstands on the nose shoulders; three cupolas on the spine; a Werkeingang at the tail.",
        "block buried": "Yes: earth over the roof, only the front and the roof lip are concrete.",
        "nav": "Spine reached from the tail; the casemate front and wing walls are blockers; Tobruks and cupolas are low (walk round).",
        "per site": "Narrow (36 m): fits the tight necks best.",
    }
    return w.finish()


def c7b():
    w = Werk("7b", "Felsnase B", "Flankenwerk: an earth glacis to the enemy, 667 casemates firing along a dry ditch")
    felsnase_hill(w, glacis=True)
    r = np.hypot(X - 4.0, Y)
    ang = np.degrees(np.arctan2(Y, X - 4.0))
    arc = (np.abs(ang) < 78.0)
    ditch = arc & (r > 34.0) & (r < 40.0)
    w.H = np.where(ditch, np.minimum(w.H, -2.6), w.H)
    w.H = blur(w.H, 0.5)
    w.paint["mud"] += blur(ditch.astype(float), 0.8)
    w.wall(arc_pts(4.0, 0.0, 40.4, -76.0, 76.0, 20), 0.0, thick=0.9, base=-2.8, top=0.4)
    for sg in (-1.0, 1.0):
        yaw = 70.0 * sg
        fx, fy = 10.0, sg * 23.0
        cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        w.H = notch(w.H, rect(fx + cy_ * 6.0, fy + sy_ * 6.0, 12.0, 20.0, yaw), 0.3, 42.0)
        w.bank(fx, fy, yaw, 11.0, 9.0, 0.3 + 5.0 + 1.2, 30.0, front_open=True, cover_from=-2.0, reach=6.0)
        w.mod("regelbau", fx, fy, yaw, base=0.3, W=11.0, D=9.0, H=5.0, pier=-sg, ear=True, door=False, n=0,
              side_emb=True, plate=True)
    for x, y in ((4.0, -8.0), (4.0, 8.0), (-10.0, 0.0), (-26.0, 0.0)):
        w.cupola(x, y)
    tobruk(w, 14.0, 0.0)
    w.mod("eingang", -40.0, 10.0, 90.0, W=7.0, D=6.0, H=3.6)
    w.teeth(arc_pts(4.0, 0.0, 44.0, -80.0, 80.0, 60), gaps=[(24.0, 34.0)])
    w.mast(-28.0, -14.0)
    w.portal()
    w.hq = [(-40.0, 10.0, "command entrance (Werkeingang)"), (-28.0, -14.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (10.0, 23.0, 0.3, 70.0, 9.0, -11.0, 1.8, 3.0)
    w.scale_units(58.0, 32.0)
    w.notes = {
        "form": "The crag-and-tail with its nose turned into an earth glacis (24 deg): nothing concrete faces the enemy. A dry anti-tank ditch, 6 m wide and 2.6 m deep, curls round the nose with a concrete counterscarp; the teeth stand beyond it.",
        "bunker": "Oder-Warthe practice: two Regelbau 667-style casemates in the nose shoulders fire sideways along the ditch and the teeth, their embrasures (steel plates) hidden behind a projecting Flankenmauer; roofs under earth. Four cupolas on the nose and spine, a Tobruk on the nose, a Werkeingang (door, splinter wall, close-defence port) at the tail.",
        "block buried": "Fully: from the front it is a grass hill with cupolas.",
        "nav": "The ditch is a tank wall (one causeway at the teeth gap needed, or tanks go round); infantry can climb the glacis. Casemate fronts and the counterscarp need blockers.",
        "per site": "The ditch and teeth make it wider (88 m); the most authentic Panzerwerk look.",
    }
    return w.finish()


def c7c():
    w = Werk("7c", "Felsnase C", "Kasemattennase: twin 667 casemates and a Werkeingang in the bluff, a Leitstand on top")
    felsnase_hill(w, top=10.5, roof_slope=(12.4, 7.3, 28.0))
    w.paint["gravel"] += poly_sdf(rect(20.0, 0.0, 10.0, 34.0)) < 0
    for sg in (-1.0, 1.0):
        w.bank(15.0, sg * 9.0, 0.0, 12.0, 12.0, 0.3 + 6.0 + 1.0, 30.0, front_open=True, cover_from=-2.0, reach=3.0)
    w.bank(2.0, 0.0, 0.0, 11.0, 10.0, w.ground(2.0, 0.0) + 1.8, 30.0, front_open=False, centred=True, reach=3.5)
    for sg in (-1.0, 1.0):
        w.mod("regelbau", 15.0, sg * 9.0, 0.0, base=0.3, W=12.0, D=12.0, H=6.0, pier=sg, ear=False, door=False, n=0,
              front_emb=True, plate=True)
    w.mod("eingang", 15.0, 0.0, 0.0, base=0.3, W=6.0, D=6.0, H=4.2)
    stepped_wings(w, 15.0, 0.0, 15.8, 6.2)
    w.mod("leitstand", 2.0, 0.0, 0.0, base=w.ground(2.0, 0.0) - 0.6, W=11.0, D=10.0, H=8.0)
    for sg in (-1.0, 1.0):
        tobruk(w, 6.0, sg * 16.0)
    for x, y in ((-14.0, -3.0), (-28.0, 2.0)):
        w.cupola(x, y)
    w.teeth(arc_pts(6.0, 0.0, 38.0, -84.0, 84.0, 60), gaps=[(20.0, 30.0)])
    w.mast(-30.0, 16.0)
    w.portal()
    w.hq = [(15.0, 0.0, "command entrance (bluff door)"), (-30.0, 16.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (15.0, 0.0, 0.3, 0.0, 19.0, -12.0, 2.0, 5.0)
    w.scale_units(52.0, 26.0)
    w.notes = {
        "form": "The crag-and-tail of concept 7 (10.5 m) with a 32 m wide bluff at the nose; over the casemates the nose rises at 28 degrees from the earth on their roofs to the Leitstand.",
        "bunker": "Two Regelbau 667-style anti-tank casemates side by side in the bluff, stepped embrasures with steel plates, outer Flankenmauern; between them the Werkeingang with its splinter wall; roofs under earth; stepped wing walls. A Leitstand (slit band, rangefinder) on the nose top, two Tobruks, two cupolas.",
        "block buried": "Yes, earth over both casemate roofs; the Leitstand stands proud.",
        "nav": "Bluff face is a wall; the spine is reached from the tail.",
        "per site": "Narrow: fits the tight necks.",
    }
    return w.finish()


# ----------------------------------------------------------------------------------------------------- Werkgruppe
def c8a():
    w = Werk("8a", "Werkgruppe A", "Kampfgruppe: earth-covered Longues casemate, 667 outworks with Tobruks, Leitstand")
    werkgruppe_hills(w)
    w.H = notch(w.H, rect(27.0, 0.0, 10.0, 22.0), 0.3, 45.0)
    w.bank(22.0, 0.0, 0.0, 17.0, 12.0, 0.3 + 7.5 + 1.2, 30.0, front_open=True, cover_from=-4.0)
    w.mod("m272", 22.0, 0.0, 0.0, base=0.3, W=17.0, D=12.0, H=7.5, n=2, door=True)
    stepped_wings(w, 22.0, 0.0, 8.5, 4.5)
    for fx, fy, yaw, sg in ((2.0, 38.0, 55.0, -1), (-2.0, -40.0, -60.0, 1)):
        w.bank(fx, fy, yaw, 10.0, 8.0, w.ground(fx, fy) + 5.5, 30.0, front_open=True, cover_from=-2.0)
        w.mod("regelbau", fx, fy, yaw, W=10.0, D=8.0, H=5.0, pier=sg, ear=True, door=False, n=0, side_emb=True, plate=True)
    tobruk(w, -8.0, 30.0)
    tobruk(w, -12.0, -32.0)
    tobruk(w, 12.0, 14.0)
    w.mod("leitstand", -35.0, 3.0, 0.0, base=w.ground(-35.0, 3.0) - 0.5, W=8.0, D=8.0, H=6.5)
    w.mod("eingang", -16.0, -6.0, 180.0, W=7.0, D=6.0, H=3.6)
    for x, y in ((8.0, -8.0), (8.0, 8.0), (-12.0, 38.0), (-16.0, -38.0)):
        w.cupola(x, y)
    w.teeth([(-24.0, -60.0), (20.0, -52.0), (44.0, -20.0), (46.0, 14.0), (26.0, 50.0), (-22.0, 62.0)], gaps=[(70.0, 80.0)])
    w.mast(-20.0, 10.0)
    w.portal()
    w.hq = [(-16.0, -6.0, "command entrance (Werkeingang in the yard)"), (-20.0, 10.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (22.0, 0.0, 0.3, 0.0, 17.0, -9.0, 4.0, 4.0)
    w.scale_units(62.0, -6.0)
    w.notes = {
        "form": "The four angular hillocks of concept 8 (10 m main hill, 5-7 m outworks) linked by trenches round a yard.",
        "bunker": "Main hill: a Longues casemate with earth over its roof and stepped wing walls. Outworks: Regelbau 667-style casemates firing sideways behind Flankenmauern (steel embrasure plates). Three Tobruk ringstands along the trenches, a Leitstand on the rear hillock, the Werkeingang in the yard.",
        "block buried": "Yes, every roof under earth.",
        "nav": "Lanes between the hillocks are open; trenches are infantry paths.",
        "per site": "Hillock positions can shift per site; the kit stays the same.",
    }
    return w.finish()


def c8b():
    w = Werk("8b", "Werkgruppe B", "Flankierend: glacis hillocks with cupolas; 667 casemates cross their fire over the gaps")
    werkgruppe_hills(w, glacis=True)
    pairs = [(6.0, 19.0, 90.0, -1), (6.0, -19.0, -90.0, 1), (-8.0, 23.5, -95.0, 1), (-8.0, -24.5, 95.0, -1)]
    bases = []
    for fx, fy, yaw, sg in pairs:
        g0 = 0.3
        cy_, sy_ = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        w.H = notch(w.H, rect(fx + cy_ * 5.5, fy + sy_ * 5.5, 11.0, 17.0, yaw), g0, 42.0)
        bases.append(g0)
    for (fx, fy, yaw, sg), g0 in zip(pairs, bases):
        w.bank(fx, fy, yaw, 9.0, 8.0, g0 + 4.6 + 1.2, 30.0, front_open=True, cover_from=-2.0, reach=6.0)
    for (fx, fy, yaw, sg), g0 in zip(pairs, bases):
        w.mod("regelbau", fx, fy, yaw, base=g0, W=9.0, D=8.0, H=4.6, pier=sg, ear=False, door=False, n=0, side_emb=True, plate=True)
    w.mod("leitstand", -32.0, 3.0, 90.0, base=w.ground(-32.0, 3.0) - 0.5, W=8.0, D=8.0, H=6.5)
    w.mod("eingang", -16.0, -6.0, 180.0, W=7.0, D=6.0, H=3.6)
    for x, y in ((13.0, 0.0), (2.0, 0.0), (-14.0, 38.0), (-18.0, -38.0)):
        w.cupola(x, y)
    tobruk(w, 18.0, -8.0)
    w.teeth([(-24.0, -60.0), (20.0, -52.0), (44.0, -20.0), (46.0, 14.0), (26.0, 50.0), (-22.0, 62.0)], gaps=[(70.0, 80.0)])
    w.mast(-20.0, 10.0)
    w.portal()
    w.hq = [(-16.0, -6.0, "command entrance (Werkeingang)"), (-20.0, 10.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (6.0, 19.0, 0.3, 90.0, 8.0, -10.0, 1.8, 3.0)
    w.scale_units(62.0, -6.0)
    w.notes = {
        "form": "The four hillocks of concept 8 as glacis mounds: 20 deg to the front, 30 deg to the back, so from the enemy side they read as grass hills.",
        "bunker": "Panzerwerk practice: no embrasure faces the enemy. Four Regelbau 667-style casemates sit in the hillock sides facing each other across the two gaps (crossfire on anything entering the yard), embrasures with steel plates behind Flankenmauern, roofs under earth. Five six-loophole cupolas on the tops, a Tobruk, a Leitstand on the rear hillock, the Werkeingang in the yard.",
        "block buried": "Fully.",
        "nav": "The gaps between the hillocks are the only ways into the yard for tanks: kill zones.",
        "per site": "Hillocks can shift per site; identical kit.",
    }
    return w.finish()


def c8c():
    w = Werk("8c", "Werkgruppe C", "Stuetzpunkt: an Eperlecques monolith on the main hill, Longues outworks, Tobruks")
    werkgruppe_hills(w)
    w.H = notch(w.H, rect(27.0, 0.0, 10.0, 24.0), 0.3, 45.0)
    w.bank(22.0, 0.0, 0.0, 20.0, 14.0, 6.5, 32.0, front_open=True)
    w.mod("blockhaus", 22.0, 0.0, 0.0, base=0.3, W=20.0, D=14.0, H=10.0, rows=2, cols=4, panels=3, door=True, lip=1.2, stepped=True)
    for fx, fy, yaw in ((4.0, 37.0, 50.0), (0.0, -39.0, -55.0)):
        w.bank(fx, fy, yaw, 11.0, 10.0, w.ground(fx, fy) + 6.8, 30.0, front_open=True, cover_from=-3.5)
        w.mod("m272", fx, fy, yaw, W=11.0, D=10.0, H=6.0, n=1, door=False)
    tobruk(w, -6.0, 18.0)
    tobruk(w, -10.0, -18.0)
    tobruk(w, 14.0, -16.0)
    w.mod("leitstand", -35.0, 3.0, 0.0, base=w.ground(-35.0, 3.0) - 0.5, W=8.0, D=8.0, H=6.5)
    w.mod("eingang", -16.0, -6.0, 180.0, W=7.0, D=6.0, H=3.6)
    for x, y in ((8.0, -10.0), (8.0, 10.0), (-12.0, 38.0), (-16.0, -38.0)):
        w.cupola(x, y)
    w.teeth([(-24.0, -60.0), (20.0, -52.0), (44.0, -20.0), (46.0, 14.0), (26.0, 50.0), (-22.0, 62.0)], gaps=[(70.0, 80.0)])
    w.mast(-20.0, 10.0)
    w.portal()
    w.hq = [(22.6, 0.0, "command entrance (monolith gate)"), (-20.0, 10.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (22.0, 0.0, 0.3, 0.0, 20.0, -11.0, 2.5, 5.0)
    w.scale_units(62.0, -6.0)
    w.notes = {
        "form": "The four hillocks of concept 8; the main hill notched for a monolith front.",
        "bunker": "Main hill: a 20 m Eperlecques-style monolith (board-form panels, stepped ports, heavy roof lip, armoured gate) with earth banked to its shoulders. Outworks: Longues casemates under earth, turned outward. Three Tobruks along the trenches, a Leitstand on the rear hillock, the Werkeingang in the yard.",
        "block buried": "Outworks yes; the monolith is half-banked and shows its face.",
        "nav": "As concept 8: open lanes between hillocks, trenches for infantry.",
        "per site": "Hillocks can shift per site.",
    }
    return w.finish()


# ----------------------------------------------------------------------------------------------------- Peilturm
def c10():
    w = Werk("10", "Peilturm", "A Marine-Peilstand tower on a rocky knoll: stacked observation slots, pillowed annexes")
    base = mesa(poly_sdf([(26.0, -18.0), (24.0, 20.0), (6.0, 30.0), (-22.0, 26.0), (-34.0, 6.0), (-28.0, -20.0), (2.0, -30.0)]),
                9.0, 27.0, 2.0)
    lumps = 1.2 * np.sin(X / 6.3) * np.cos(Y / 5.1) + 0.8 * np.sin((X - Y) / 4.4)
    z = base + lumps * smoothstep(1.0, 5.0, base)
    crest = 3.0 * np.exp(-((X - 6.0) ** 2 + Y ** 2) / (2 * 14.0 ** 2))
    w.H = blur(z + crest * smoothstep(2.0, 7.0, base), 0.8)
    w.paint["sod"] += 0.6 * smoothstep(3.0, 8.0, w.H)
    # the tower and its lower comb casemate down the front-right slope
    zt = w.ground(8.0, 0.0)
    w.bank(8.0, 0.0, 0.0, 10.0, 10.0, zt + 1.6, 30.0, front_open=False)
    w.mod("peilturm", 8.0, 0.0, 0.0, base=zt - 0.5, W=10.0, D=10.0, H=21.0, floors=3, annex=True)
    zl = w.ground(24.0, 13.0)
    w.H = notch(w.H, rect(28.0, 13.0, 8.0, 14.0), zl - 1.0, 40.0)
    zl = w.ground(26.0, 13.0)
    w.bank(24.0, 13.0, 0.0, 12.0, 10.0, zl + 4.0, 30.0, front_open=True)
    w.mod("peilkasematte", 24.0, 13.0, 0.0, base=zl, W=12.0, D=10.0, H=6.0, floors=1)
    rocks = [(-4.0, -20.0, "rock3", 2.6, 20), (14.0, -18.0, "rock2", 2.8, 70), (-18.0, 18.0, "rock3", 2.2, 140),
             (-26.0, -6.0, "rock1", 3.0, 200), (20.0, 22.0, "rock2", 2.4, 250), (-8.0, 26.0, "rock1", 2.6, 310),
             (2.0, -26.0, "rock2", 2.0, 30), (-30.0, 12.0, "rock2", 2.4, 95), (22.0, -8.0, "rock1", 2.0, 160)]
    for x, y, k, sc, yaw in rocks:
        w.kit(k, x, y, yaw, -0.3 * sc, sc)
    for x, y in ((-10.0, -10.0), (-12.0, 12.0)):
        w.cupola(x, y)
    tobruk(w, 18.0, -12.0)
    w.mod("eingang", -24.0, -2.0, 180.0, W=7.0, D=6.0, H=3.6)
    w.teeth(arc_pts(2.0, 0.0, 42.0, -80.0, 80.0, 60), gaps=[(84.0, 94.0)])
    w.mast(-16.0, 2.0)
    w.portal()
    w.hq = [(8.0, 0.0, "command post (the tower)"), (-16.0, 2.0, "transmitter"), (-50.0, 0.0, "rail entrance")]
    w.hero = (14.0, 0.0, 6.0, 0.0, 34.0, -22.0, 3.5, 7.0)
    w.scale_units(58.0, 26.0)
    w.notes = {
        "form": "A rocky knoll, 60 x 60 m, 9-12 m high, with granite outcrops breaking through the grass (the Channel Islands look of the reference).",
        "bunker": "A Marine-Peilstand tower (M3 type): 21 m over the knoll, a solid head block over three stacked observation floors whose slabs cantilever out between dark slots; two pillowed annexes at its foot; a lower comb casemate stepping down the front slope; two cupolas, a Tobruk, the Werkeingang at the back.",
        "block buried": "No: the tower is the landmark; the annexes and lower casemate are banked into the knoll.",
        "nav": "Knoll slopes 27 deg (climbable) between the rocks; the tower and casemates are blockers.",
        "per site": "Story fit: a Peilstand is a direction-finding/rangefinding post, so the HQ could hide in exactly this kind of tower. Identical stamp.",
    }
    return w.finish()


CONCEPTS = [c7a, c7b, c7c, c8a, c8b, c8c, c10]
