"""Pripyat Part B models (Blender 5 background): bridge dressing kits, log causeway, boardwalk, radio masts.

Out: CustomMapItems/PripyatCrossings/FBX/*.FBX + PripyatCrossings.blend + Preview/*.png, and
     CustomMapItems/PripyatRadioMast/FBX/*.FBX + Preview, report Saved/Pripyat/assets/partB_report.json.
Units cm, +X along the lane / bridge, +Z up, pivot on the lane surface (z 0) unless noted.
Slots: 0 Wood/stone = farm atlas (MI_Polesie_FarmAtlas; bands from FarmBuildings/out/atlas_bands.json),
       1 Steel (M_Prypyat_KitSteel, tiling rusty metal, planar UV 2 m), 2 Algae (MI_Phytophage_BlackPlum_Solid).

blender --background --factory-startup --python blender_partB.py [-- only=<prefix>]
"""
import json
import math
import random
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets')
import pry_blender_lib as L  # noqa: E402

CMI = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/'
OUT_X = CMI + 'PripyatCrossings/'
OUT_M = CMI + 'PripyatRadioMast/'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/partB_report.json'
FARM_D = 'D:/Blender/BlenderProjects/BlenderTanks/FarmBuildings/out/T_FarmBuildings_D.png'
STEEL_D = CMI + 'SovietsStuff/SovietCrane/T_CraneMetal.PNG'
MATS = ['M_Prypyat_KitWood', 'M_Prypyat_KitSteel', 'M_Prypyat_KitAlgae']
WOOD, STEEL, ALGAE = 0, 1, 2
BANDS = {'timber': (0.8125, 0.84375), 'planks': (0.65625, 0.8125), 'stone': (0.5, 0.546875),
         'charred': (0.03125, 0.078125), 'roof_old': (0.28125, 0.390625)}
U_PER_CM = 1.0 / 1600.0
ALGAE_RECT = (0.55, 0.55, 0.95, 0.95)
report = {}
rng = random.Random(1944)


def xf(loc=(0, 0, 0), yaw=0.0, pitch=0.0, roll=0.0):
    return (Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(yaw), 4, 'Z') @
            Matrix.Rotation(math.radians(pitch), 4, 'Y') @ Matrix.Rotation(math.radians(roll), 4, 'X'))


def band_rect(band, length_cm, cross_cm=None):
    if band == 'charred':
        # Farm atlas rubble band, u 0.36-0.64 is the charred-timber field (dark splinters).
        u0 = rng.uniform(0.37, 0.47)
        return (u0, 0.0, u0 + min(length_cm * U_PER_CM, 0.16), 0.03125)
    v0, v1 = BANDS[band]
    u0 = rng.uniform(0, 1)
    if cross_cm is not None:
        v1 = v0 + (v1 - v0) * min(1.0, cross_cm / ((v1 - v0) * 1600.0))
    return (u0, v0, u0 + length_cm * U_PER_CM, v1)


def plank(b, center, lx, ly, lz, band='planks', yaw=0.0, pitch=0.0, roll=0.0, long_axis='y'):
    """Box centred at center; atlas rect: grain (V) along the long axis."""
    m = xf(center, yaw, pitch, roll)
    if long_axis == 'y':
        rect = band_rect(band, lx, ly) if band == 'planks' else band_rect(band, ly, lx)
    else:
        rect = band_rect(band, ly, lx) if band == 'planks' else band_rect(band, lx, ly)
    b.box((-lx / 2, -ly / 2, -lz / 2), (lx / 2, ly / 2, lz / 2), WOOD, xform=m, uv_rect=rect)


def log(b, a, c, radius, band='timber', sides=6, radius_b=None):
    length = (Vector(c) - Vector(a)).length
    b.prism(a, c, radius, sides, WOOD, band_rect(band, length), radius_b=radius_b)


def beam(b, a, c, w, h=None, mat=STEEL):
    """Steel member from a to c with a w x h rectangular section (planar UVs)."""
    a, c = Vector(a), Vector(c)
    d = c - a
    length = d.length
    h = h or w
    yaw = math.degrees(math.atan2(d.y, d.x))
    pitch = -math.degrees(math.atan2(d.z, math.hypot(d.x, d.y)))
    m = xf(a, yaw, pitch, 0)
    if mat == WOOD:
        b.box((0, -w / 2, -h / 2), (length, w / 2, h / 2), mat, xform=m, uv_rect=band_rect('timber', length))
        return
    b.box((0, -w / 2, -h / 2), (length, w / 2, h / 2), mat, uv_scale=200.0,
          uv_offset=(rng.uniform(0, 1), rng.uniform(0, 1)), xform=m)


def stone_block(b, lo, hi, m=None):
    v0, v1 = BANDS['stone']
    u0 = rng.uniform(0, 1)
    b.box(lo, hi, WOOD, xform=m, uv_rect=(u0, v0, u0 + (hi[0] - lo[0] + hi[1] - lo[1]) * 0.5 * U_PER_CM, v1))


def algae_strip(b, pts, width, sag=0.0):
    """Ribbon of flat quads through pts (purple algae dragging in the current)."""
    for k in range(len(pts) - 1):
        a, c = Vector(pts[k]), Vector(pts[k + 1])
        d = (c - a)
        side = Vector((-d.y, d.x, 0)).normalized() * (width / 2)
        if side.length < 1e-3:
            side = Vector((width / 2, 0, 0))
        u0, v0, u1, v1 = ALGAE_RECT
        t0, t1 = k / (len(pts) - 1), (k + 1) / (len(pts) - 1)
        b.quad_uv([a - side, a + side, c + side, c - side], ALGAE,
                  (u0, v0 + (v1 - v0) * t0, u1, v0 + (v1 - v0) * t1), normal=(0, 0, 1))


# ------------------------------------------------------------------------------------------------------------ truss
def truss(b, length, height, m=None, panels=8, broken_from=None, drop_deg=0.0, hinge=None):
    """Warren truss in the XZ plane (y = 0), x from -length/2 to +length/2. Members beyond broken_from (x) are
    rotated by drop_deg about the hinge (a point on the bottom chord), as a collapsed half."""
    half = length / 2
    pw = length / panels
    nodes_b = [Vector((-half + i * pw, 0, 20)) for i in range(panels + 1)]
    nodes_t = [Vector((-half + (i + 0.5) * pw, 0, height)) for i in range(panels)]
    drop = None
    if broken_from is not None:
        hp = Vector((broken_from, 0, 20)) if hinge is None else Vector(hinge)
        drop = Matrix.Translation(hp) @ Matrix.Rotation(math.radians(drop_deg), 4, 'Y') @ Matrix.Translation(-hp)

    def place(p):
        q = p
        if drop is not None and p.x > broken_from + 1:
            q = drop @ p
        return (m @ q) if m is not None else q

    def member(p, q, w, h=None):
        if broken_from is not None and (p.x - broken_from) * (q.x - broken_from) < 0:
            # Torn member: keep a stub on the standing side, a bent stub on the dropped side.
            mid = p.lerp(q, 0.45) if p.x < q.x else q.lerp(p, 0.45)
            near = p if p.x < broken_from else q
            far = q if near is p else p
            beam(b, place(near), place(near.lerp(mid, 0.8)), w, h)
            beam(b, place(far), place(far.lerp(mid, 0.6) + Vector((0, 15, -25))), w, h)
            return
        beam(b, place(p), place(q), w, h)

    for i in range(panels):                       # bottom chord
        member(nodes_b[i], nodes_b[i + 1], 34, 42)
    for i in range(panels - 1):                   # top chord
        member(nodes_t[i], nodes_t[i + 1], 30, 30)
    for i in range(panels):                       # diagonals (Warren) + verticals
        member(nodes_b[i], nodes_t[i], 22)
        member(nodes_t[i], nodes_b[i + 1], 22)
        member(nodes_t[i] + Vector((0, 0, 0)), Vector((nodes_t[i].x, 0, 20)), 14)
    return place


def rail_bridge_deck():
    b = L.Builder('SM_PrypyatRailBridge_Deck', MATS)
    length, width = 2200.0, 1000.0
    x = -length / 2 + 14
    while x < length / 2 - 10:
        for side in (-1, 1):
            if rng.random() < 0.08:
                continue                           # missing plank: the landscape shows through
            band = 'charred' if rng.random() < 0.18 else 'planks'
            ly = width / 2 - rng.uniform(0, 30)
            plank(b, (x + rng.uniform(-2, 2), side * (ly / 2 + rng.uniform(0, 8)), 2.5 + rng.uniform(-1, 1)),
                  rng.uniform(22, 26), ly, 5, band, yaw=rng.uniform(-1.5, 1.5), roll=rng.uniform(-1.2, 1.2))
        x += 27
    # Sleepers + rails (Russian gauge 1520 mm), a rail broken near the +X end.
    x = -length / 2 + 20
    while x < length / 2 - 10:
        if rng.random() > 0.06:
            plank(b, (x, 0, 10), 24, 270, 10, 'timber' if rng.random() < 0.8 else 'charred',
                  yaw=rng.uniform(-3, 3), long_axis='y')
        x += 55
    for y in (-76, 76):
        if y < 0:
            beam(b, (-length / 2, y, 20), (length / 2, y, 20), 8, 14)
        else:
            beam(b, (-length / 2, y, 20), (520, y, 20), 8, 14)
            beam(b, (560, y, 20), (length / 2, y + 30, 18), 8, 14)       # sprung, bent section
    # Steel edge stringers flush with the deck.
    for y in (-width / 2 + 6, width / 2 - 6):
        beam(b, (-length / 2, y, 2), (length / 2, y, 2), 12, 8)
    return b, []


def rail_truss(name, broken):
    b = L.Builder(name, MATS)
    length, height = 2200.0, 330.0
    if broken:
        truss(b, length, height, broken_from=150.0, drop_deg=24.0)
        hulls = [L.box_points((-length / 2, -25, 0), (150, 25, height + 15))]
        drop = Matrix.Translation((150, 0, 20)) @ Matrix.Rotation(math.radians(24), 4, 'Y') @ Matrix.Translation((-150, 0, -20))
        hulls.append(L.box_points((150, -25, 0), (length / 2, 25, height), drop))
    else:
        truss(b, length, height)
        hulls = [L.box_points((-length / 2, -25, 0), (length / 2, 25, height + 15))]
    # Bearing shoes at both ends.
    for x in (-length / 2, length / 2 if not broken else None):
        if x is not None:
            beam(b, (x - 30, 0, 0), (x + 30, 0, 0), 50, 30)
    return b, hulls


def fallen_span():
    """A 12 m truss section lying in the river, rolled onto its side, half under the algae, strands trailing +X."""
    b = L.Builder('SM_PrypyatRailBridge_FallenSpan', MATS)
    m = xf((0, 0, -120), yaw=0, pitch=7, roll=68)
    truss(b, 1200, 330, m=m, panels=4)
    m2 = xf((0, -260, -150), yaw=4, pitch=-5, roll=-20)
    # second (lower) chord plane and a few deck sleepers caught in it
    for x in range(-560, 560, 140):
        beam(b, m @ Vector((x, 0, 20)), m2 @ Vector((x + 20, 0, 20)), 18)
    for k in range(9):
        p = m @ Vector((rng.uniform(-550, 550), 0, rng.uniform(30, 300)))
        plank(b, (p.x, p.y + rng.uniform(-60, 60), max(p.z, -10)), 24, 260, 10,
              rng.choice(('timber', 'charred')), yaw=rng.uniform(-50, 50), roll=rng.uniform(-20, 20), long_axis='y')
    # Algae mats draped over the top chord and dragging downstream (+X), lying on the water (z ~ 2).
    for k in range(7):
        x0 = rng.uniform(-500, 450)
        top = m @ Vector((x0, 0, 330))
        pts = [top + Vector((0, 0, 8))]
        for s in range(1, 6):
            pts.append(Vector((top.x + s * rng.uniform(70, 110), top.y + rng.uniform(-40, 40) + s * 10,
                               max(2.0, top.z - s * 70))))
        algae_strip(b, pts, rng.uniform(60, 120))
    return b, []


def abutment():
    b = L.Builder('SM_PrypyatRailBridge_Abutment', MATS)
    stone_block(b, (-150, -125, -350), (150, 125, 40))
    stone_block(b, (-165, -140, 40), (165, 140, 70))               # coping
    m = xf((0, 0, 0), yaw=0, pitch=0, roll=0)
    for k in range(5):                                             # fallen coping stones
        stone_block(b, (-30, -20, -15), (30, 20, 15), xf((rng.uniform(-200, 200), rng.uniform(150, 260),
                    rng.uniform(-60, -10)), rng.uniform(0, 90), rng.uniform(-20, 20), rng.uniform(-20, 20)))
    beam(b, (-120, -60, 72), (120, -60, 72), 40, 20)              # rusted bearing plate
    return b, [L.box_points((-165, -140, -350), (165, 140, 70))]


def pier():
    b = L.Builder('SM_PrypyatRailBridge_Pier', MATS)
    stone_block(b, (-110, -60, -320), (110, 60, 150))
    stone_block(b, (-125, -72, 150), (125, 72, 180))
    # pointed cutwater on -X (upstream)
    v0, v1 = BANDS['stone']
    pts = [(-110, -60, -320), (-110, 60, -320), (-190, 0, -320), (-110, -60, 150), (-110, 60, 150), (-190, 0, 150)]
    P = [Vector(p) for p in pts]
    b.quad_uv([P[2], P[0], P[3], P[5]], WOOD, (0.1, v0, 0.2, v1))
    b.quad_uv([P[1], P[2], P[5], P[4]], WOOD, (0.3, v0, 0.4, v1))
    b.face([P[3], P[4], P[5]], WOOD, [(0.1, v0), (0.2, v0), (0.15, v1)])
    beam(b, (-90, 0, 195), (90, 0, 195), 60, 30)                    # torn bearing
    beam(b, (60, 0, 210), (260, 30, 120), 34, 42)                   # stub of the dropped girder
    return b, [L.box_points((-190, -72, -320), (125, 72, 180))]


# ------------------------------------------------------------------------------------------------------- log kit
def log_row(b, x, y0, y1, r, z_top, sunk=0.0, tilt=0.0, yaw=0.0, band='timber'):
    zc = z_top - r - sunk
    dy = (y1 - y0) / 2
    cy = (y0 + y1) / 2
    a = Vector((x, cy - dy, zc + tilt))
    c = Vector((x, cy + dy, zc - tilt))
    rot = Matrix.Rotation(math.radians(yaw), 4, 'Z')
    centre = Vector((x, cy, zc))
    a = centre + rot @ (a - centre)
    c = centre + rot @ (c - centre)
    log(b, a, c, r, band, radius_b=r * rng.uniform(0.85, 1.0))


def log_deck():
    b = L.Builder('SM_PrypyatLogBridge_Deck', MATS)
    length, width = 2200.0, 1000.0
    x = -length / 2 + 14
    while x < length / 2 - 10:
        split = rng.uniform(-80, 80)
        for (y0, y1) in ((-width / 2 - 20, split + 15), (split - 15, width / 2 + 20)):
            if rng.random() < 0.05:
                continue
            log_row(b, x, y0, y1, rng.uniform(12, 15), 16, sunk=rng.uniform(0, 4), tilt=rng.uniform(-3, 3),
                    yaw=rng.uniform(-2, 2), band='charred' if rng.random() < 0.08 else 'timber')
        x += 28
    for yc in (-230, 230):                                            # plank wheel runs on top
        for k in range(4):
            y = yc - 75 + k * 50
            xs = -length / 2
            while xs < length / 2 - 50:
                seg = min(rng.uniform(380, 520), length / 2 - xs)
                if rng.random() > 0.1:
                    plank(b, (xs + seg / 2, y, 19), seg - 6, 22, 5, 'planks', yaw=rng.uniform(-0.6, 0.6),
                          long_axis='x')
                xs += seg
    return b, []


def log_siderail():
    b = L.Builder('SM_PrypyatLogBridge_SideRail', MATS)
    length = 2200.0
    for k in range(9):
        x = -length / 2 + k * length / 8
        lean = rng.uniform(-4, 4)
        log(b, (x, 0, -380), (x + lean, rng.uniform(-3, 3), rng.uniform(85, 110)), rng.uniform(15, 18), sides=7)
    log(b, (-length / 2, 0, 35), (length / 2, 0, 35), 16, sides=7)          # kerb log
    xs = -length / 2
    while xs < length / 2 - 10:                                             # broken top rail in pieces
        seg = rng.uniform(300, 600)
        xe = min(xs + seg, length / 2)
        if rng.random() > 0.25:
            log(b, (xs, 0, 95), (xe, rng.uniform(-6, 6), 95 + rng.uniform(-12, 4)), 11, sides=6)
        xs = xe + rng.uniform(20, 120)
    return b, [L.box_points((-length / 2, -22, 0), (length / 2, 22, 110))]


def pile_cluster():
    b = L.Builder('SM_PrypyatLogBridge_PileCluster', MATS)
    for k in range(7):
        x, y = rng.uniform(-160, 160), rng.uniform(-90, 90)
        top = rng.uniform(40, 210)
        log(b, (x, y, -320), (x + rng.uniform(-25, 25), y + rng.uniform(-25, 25), top), rng.uniform(13, 18), sides=7,
            band='timber' if k % 3 else 'charred')
    log(b, (-170, -60, 120), (150, 70, 95), 12, sides=6)
    algae_strip(b, [(-120, -40, 60), (-40, -30, 25), (60, -10, 4), (180, 10, 2)], 70)
    return b, []


def corduroy(name, width, sunk_range, missing, tilt):
    b = L.Builder(name, MATS)
    length = 800.0
    x = -length / 2 + 13
    while x < length / 2 - 5:
        split = rng.uniform(-width * 0.2, width * 0.2)
        for (y0, y1) in ((-width / 2 - rng.uniform(0, 40), split + 20), (split - 20, width / 2 + rng.uniform(0, 40))):
            if rng.random() < missing:
                continue
            log_row(b, x + rng.uniform(-3, 3), y0, y1, rng.uniform(11, 14), 12, sunk=rng.uniform(*sunk_range),
                    tilt=rng.uniform(-tilt, tilt), yaw=rng.uniform(-tilt * 0.6, tilt * 0.6),
                    band='charred' if rng.random() < 0.1 else 'timber')
        x += 26
    # A few stringer logs along the road edge, half buried.
    for y in (-width / 2 + 40, width / 2 - 40):
        log(b, (-length / 2, y, -8), (length / 2, y + rng.uniform(-10, 10), -10), 13, sides=6)
    return b, []


# ------------------------------------------------------------------------------------------------------ boardwalk
def boardwalk(name, broken):
    b = L.Builder(name, MATS)
    length, width = 600.0, 450.0

    def sag(p):
        p = Vector(p)
        if broken and p.x > 0:
            drop = Matrix.Translation((0, 0, 0)) @ Matrix.Rotation(math.radians(7), 4, 'Y')
            q = drop @ Vector(p)
            return Vector((q.x, q.y, q.z - 6))
        return Vector(p)

    for y in (-180, 0, 180):                                      # stringers
        if broken and y == 180:
            beam(b, sag((-300, y, -8)), sag((60, y, -8)), 15, 15, WOOD)
            beam(b, sag((90, y, -30)), sag((300, y + 20, -55)), 15, 15, WOOD)
            continue
        beam(b, sag((-300, y, -8)), sag((300, y, -8)), 15, 15, WOOD)
    x = -length / 2 + 13
    while x < length / 2 - 5:
        for side in (-1, 1):
            if rng.random() < (0.3 if broken else 0.04):
                continue
            c = sag((x, side * 112, 2.5))
            loose = broken and rng.random() < 0.15
            plank(b, (c.x + (rng.uniform(-20, 20) if loose else 0), c.y, c.z + (3 if loose else 0)),
                  22, 226, 5, 'charred' if rng.random() < 0.12 else 'planks',
                  yaw=rng.uniform(-25, 25) if loose else rng.uniform(-1, 1),
                  pitch=(7 if broken and x > 0 else 0) + rng.uniform(-1, 1), roll=rng.uniform(-1.5, 1.5))
        x += 26
    for x in (-300, 0):                                            # pile pairs (next tile adds +300)
        for y in (-215, 215):
            top = rng.uniform(15, 45)
            p0, p1 = sag((x, y, -130)), sag((x, y, top))
            log(b, p0, p1, 9, sides=6)
    return b, []


def boardwalk_post():
    b = L.Builder('SM_PrypyatBoardwalk_Post', MATS)
    log(b, (0, 0, -130), (rng.uniform(-6, 6), rng.uniform(-6, 6), 95), 10, sides=7)
    plank(b, (0, 12, 70), 6, 30, 30, 'planks', long_axis='y')        # small cleat
    return b, []


# ------------------------------------------------------------------------------------------------------ radio masts
def mast(name, height, lean_lo, lean_hi, kink, snapped, crossarms):
    b = L.Builder(name, MATS)
    side = 90.0
    rad = side / math.sqrt(3)
    sec = 250.0
    nsec = int(height / sec)

    def axis(z):
        """Mast centre line: leans lean_lo below the kink height, lean_hi above (toward +X)."""
        zk = height * kink
        if z <= zk:
            return Vector((z * math.tan(math.radians(lean_lo)), 0, z))
        base = Vector((zk * math.tan(math.radians(lean_lo)), 0, zk))
        dz = z - zk
        return base + Vector((dz * math.sin(math.radians(lean_hi)), 0, dz * math.cos(math.radians(lean_hi))))

    def leg(i, z):
        a = math.radians(90 + 120 * i)
        return axis(z) + Vector((math.cos(a) * rad, math.sin(a) * rad, 0))

    for s in range(nsec):
        z0, z1 = s * sec, (s + 1) * sec
        for i in range(3):
            beam(b, leg(i, z0), leg(i, z1), 7)
            j = (i + 1) % 3
            beam(b, leg(i, z0), leg(j, z1), 4)
            beam(b, leg(j, z0), leg(i, z1), 4)
            beam(b, leg(i, z1), leg(j, z1), 4)
    top = axis(nsec * sec)
    beam(b, top, top + Vector((0, 0, 260)), 6)                       # whip antenna
    for k, cz in enumerate(crossarms):
        c = axis(cz)
        beam(b, c + Vector((0, -180, 0)), c + Vector((0, 180, 0)), 6)
        for yy in (-170, 170):
            beam(b, c + Vector((0, yy, 0)), c + Vector((0, yy, -90)), 3)
    stone_block(b, (-110, -110, -40), (110, 110, 25))                 # footing
    # Guys: 3 anchors at 120 deg, from two levels.
    anchors = []
    for i in range(3):
        a = math.radians(30 + 120 * i)
        anchors.append(Vector((math.cos(a) * 1150, math.sin(a) * 1150, 10)))
        stone_block(b, (-45, -45, -25), (45, 45, 20), xf(tuple(anchors[-1] - Vector((0, 0, 10))), yaw=i * 120))
    levels = [height * 0.45, height * 0.85]
    for li, lz in enumerate(levels):
        for i, anc in enumerate(anchors):
            p = axis(lz)
            if snapped and li == 1 and i == 0:
                # snapped guy: hangs from the mast and coils on the ground
                pts = [p, p + Vector((40, 30, -lz * 0.35)), p + Vector((90, 60, -lz * 0.7)), Vector((p.x + 160, 120, 8)),
                       Vector((p.x + 420, 260, 6))]
                for k in range(len(pts) - 1):
                    beam(b, pts[k], pts[k + 1], 3)
                continue
            beam(b, p, anc, 3)
    hulls = [L.box_points((-110, -110, -40), (110 + height * math.tan(math.radians(lean_lo)) * 0.12, 110, 300))]
    return b, hulls


BUILDS = {
    'SM_PrypyatRailBridge_Deck': (rail_bridge_deck, OUT_X),
    'SM_PrypyatRailBridge_Truss': (lambda: rail_truss('SM_PrypyatRailBridge_Truss', False), OUT_X),
    'SM_PrypyatRailBridge_TrussBroken': (lambda: rail_truss('SM_PrypyatRailBridge_TrussBroken', True), OUT_X),
    'SM_PrypyatRailBridge_FallenSpan': (fallen_span, OUT_X),
    'SM_PrypyatRailBridge_Abutment': (abutment, OUT_X),
    'SM_PrypyatRailBridge_Pier': (pier, OUT_X),
    'SM_PrypyatLogBridge_Deck': (log_deck, OUT_X),
    'SM_PrypyatLogBridge_SideRail': (log_siderail, OUT_X),
    'SM_PrypyatLogBridge_PileCluster': (pile_cluster, OUT_X),
    'SM_PrypyatCorduroy_14m': (lambda: corduroy('SM_PrypyatCorduroy_14m', 1400.0, (0, 5), 0.04, 3.0), OUT_X),
    'SM_PrypyatCorduroy_Sunken_10m': (lambda: corduroy('SM_PrypyatCorduroy_Sunken_10m', 1000.0, (4, 12), 0.2, 6.0), OUT_X),
    'SM_PrypyatBoardwalk_6m': (lambda: boardwalk('SM_PrypyatBoardwalk_6m', False), OUT_X),
    'SM_PrypyatBoardwalk_Broken_6m': (lambda: boardwalk('SM_PrypyatBoardwalk_Broken_6m', True), OUT_X),
    'SM_PrypyatBoardwalk_Post': (boardwalk_post, OUT_X),
    'SM_PrypyatRadioMast_Leaning': (lambda: mast('SM_PrypyatRadioMast_Leaning', 2500.0, 5.0, 13.0, 0.55, True,
                                                 [2300.0]), OUT_M),
    'SM_PrypyatRadioMast_Island': (lambda: mast('SM_PrypyatRadioMast_Island', 2250.0, 0.6, 0.6, 0.5, False,
                                                [1500.0, 2000.0, 2200.0]), OUT_M),
}

# Preview layouts: (name, location, yaw)
PREVIEWS = {
    'RailBridge': ([('SM_PrypyatRailBridge_Deck', (0, 0, 0), 0), ('SM_PrypyatRailBridge_Truss', (0, -525, 0), 0),
                    ('SM_PrypyatRailBridge_TrussBroken', (0, 525, 0), 180),
                    ('SM_PrypyatRailBridge_FallenSpan', (300, 1150, 0), 0),
                    ('SM_PrypyatRailBridge_Abutment', (-1100, -640, 0), 0), ('SM_PrypyatRailBridge_Abutment', (-1100, 640, 0), 0),
                    ('SM_PrypyatRailBridge_Abutment', (1100, -640, 0), 180), ('SM_PrypyatRailBridge_Abutment', (1100, 640, 0), 180),
                    ('SM_PrypyatRailBridge_Pier', (0, 1150, 0), 90)], (0, 200, 0), 4200),
    'LogBridge': ([('SM_PrypyatLogBridge_Deck', (0, 0, 0), 0), ('SM_PrypyatLogBridge_SideRail', (0, -520, 0), 0),
                   ('SM_PrypyatLogBridge_SideRail', (0, 520, 0), 0), ('SM_PrypyatLogBridge_PileCluster', (300, 1000, 0), 0),
                   ('SM_PrypyatCorduroy_14m', (1500, 0, 0), 0), ('SM_PrypyatCorduroy_14m', (2300, 0, 0), 0),
                   ('SM_PrypyatCorduroy_Sunken_10m', (-1500, 0, 0), 0)], (300, 0, 0), 4800),
    'Boardwalk': ([('SM_PrypyatBoardwalk_6m', (-600, 0, 0), 0), ('SM_PrypyatBoardwalk_6m', (0, 0, 0), 0),
                   ('SM_PrypyatBoardwalk_Broken_6m', (600, 0, 0), 0), ('SM_PrypyatBoardwalk_Post', (900, 300, 0), 0)],
                  (100, 0, 0), 1900),
    'RadioMasts': ([('SM_PrypyatRadioMast_Leaning', (0, 0, 0), 0), ('SM_PrypyatRadioMast_Island', (3000, 0, 0), 0)],
                   (1500, 0, 1100), 6500),
}


def preview(key, built):
    L.reset_scene()
    wood = L.image_material('M_Prypyat_KitWood', FARM_D, tint=(0.64, 0.64, 0.64))
    steel = L.image_material('M_Prypyat_KitSteel', STEEL_D, tint=(0.45, 0.43, 0.42), roughness=0.7)
    algae = L.flat_material('M_Prypyat_KitAlgae', (0.16, 0.03, 0.22), 0.3)
    mats = {'Wood': wood, 'Steel': steel, 'Algae': algae}
    items, target, dist = PREVIEWS[key]
    for name, loc, yaw in items:
        bpy.ops.import_scene.fbx(filepath=built[name])
        for o in bpy.context.selected_objects:
            if o.name.startswith('UCX_'):
                bpy.data.objects.remove(o)
                continue
            o.location = loc
            o.rotation_euler = (0, 0, math.radians(yaw))
            for i, m in enumerate(o.data.materials):
                for k, v in mats.items():
                    if m and k in m.name:
                        o.data.materials[i] = v
    path = (OUT_M if key == 'RadioMasts' else OUT_X) + 'Preview/%s_Preview.png' % key
    L.render_preview(path, target=target, distance=dist, elevation_deg=58, azimuth_deg=-62,
                     ground_rgb=(0.20, 0.17, 0.12) if key != 'RailBridge' else (0.13, 0.10, 0.14),
                     ground_z=-90.0 if key == 'RailBridge' else -1.0)
    return path


def main():
    only = None
    if '--' in sys.argv:
        for arg in sys.argv[sys.argv.index('--') + 1:]:
            if arg.startswith('only='):
                only = arg[5:]
    previous = {}
    try:
        previous = json.load(open(REPORT))
    except Exception:  # noqa: BLE001
        pass
    L.reset_scene()
    built = {k: v['fbx'] for k, v in previous.items() if 'fbx' in v}
    for name, (fn, out) in BUILDS.items():
        if only and not name.startswith(only):
            continue
        builder, hulls = fn()
        obj = builder.to_object()
        previous[name] = L.export_fbx(obj, out + 'FBX/' + name + '.FBX', hulls)
        built[name] = previous[name]['fbx']
    bpy.ops.wm.save_as_mainfile(filepath=OUT_X + 'PripyatCrossings_build.blend')
    for key in PREVIEWS:
        if only and not any(n.startswith(only) for n, _, _ in PREVIEWS[key][0]):
            continue
        if all(n in built for n, _, _ in PREVIEWS[key][0]):
            path = preview(key, built)
            for n, _, _ in PREVIEWS[key][0]:
                previous[n]['preview'] = path
    with open(REPORT, 'w') as handle:
        json.dump(previous, handle, indent=1)
    print('PARTB_DONE', json.dumps({k: (v['min'], v['max'], v['tris'], v['hulls']) for k, v in previous.items()}))


main()
