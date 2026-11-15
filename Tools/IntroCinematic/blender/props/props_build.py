"""Builds the intro-cinematic props: model, verify normals, UV, bake PBR textures, export FBX, render a preview.

Run (Git Bash or PowerShell, Windows-style paths):
  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P props_build.py                  (everything)
  D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P props_build.py -- --only OilLamp,MapPin
  ... -- --only MapTable --no-bake      (shape iteration: procedural-material preview, no textures, FBX still written)
Then props_preview_all.py re-imports the FBX files, checks them, renders preview_all.png and writes README.md.
"""
import bpy, bmesh, math, os, sys, json
import numpy as np
from mathutils import Vector, Matrix

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import props_lib as L
import props_mats as M
from props_lib import Builder, smooth_path, TRS

PI = math.pi
ARGV = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
NO_BAKE = '--no-bake' in ARGV
ONLY = ARGV[ARGV.index('--only') + 1].split(',') if '--only' in ARGV else None
# --diag x,y,z,dir : extra render of the finished prop from view x,y,z into dir (not a deliverable)
DIAG = ARGV[ARGV.index('--diag') + 1].split(',') if '--diag' in ARGV else None


def cols(cx, cy, cz, loc=(0, 0, 0)):
    """4x4 matrix from the images of the local X, Y, Z axes."""
    m = Matrix.Identity(4)
    for i, c in enumerate((cx, cy, cz)):
        for r in range(3):
            m[r][i] = c[r]
    for r in range(3):
        m[r][3] = loc[r]
    return m


def axis_m(origin, d):
    """Matrix putting local Z along d at origin."""
    d = Vector(d).normalized()
    up = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
    x = up.cross(d).normalized()
    y = d.cross(x)
    return cols(x, y, d, origin)


def remat(b, faces, mat, cond):
    mi = b._mi(mat)
    for f in faces:
        if cond(f):
            f.material_index = mi


# ====================================================================================== 1 oil lamp
def build_oillamp():
    b = Builder(11)
    tank = [(0, 0.006), (0.056, 0.006), (0.062, 0.0), (0.071, 0.0), (0.076, 0.004), (0.078, 0.012), (0.078, 0.020),
            (0.0805, 0.023), (0.078, 0.026), (0.078, 0.038), (0.075, 0.046), (0.062, 0.054), (0.040, 0.059),
            (0.030, 0.060), (0, 0.060)]
    b.add(b.lathe(tank, 24), 'lamp_paint', smooth=32)
    burner = [(0, 0.058), (0.029, 0.058), (0.029, 0.068), (0.041, 0.072), (0.043, 0.076), (0.041, 0.0795),
              (0.033, 0.0805), (0.030, 0.086), (0.022, 0.096), (0.012, 0.101), (0, 0.101)]
    b.add(b.lathe(burner, 20), 'lamp_brass', smooth=32)
    go = [(0.035, 0.080), (0.041, 0.092), (0.048, 0.110), (0.052, 0.130), (0.050, 0.150), (0.043, 0.168),
          (0.036, 0.180), (0.033, 0.186)]
    gi = [(r - 0.002, z) for r, z in reversed(go)]
    b.add(b.lathe(go + gi, 20, loop=True), 'lamp_glass', smooth=60)
    cap = [(0, 0.189), (0.032, 0.189), (0.036, 0.184), (0.039, 0.186), (0.039, 0.193), (0.031, 0.201), (0.026, 0.203),
           (0.026, 0.236), (0.031, 0.238), (0.036, 0.243), (0.036, 0.248), (0.026, 0.259), (0.012, 0.264), (0, 0.265)]
    b.add(b.lathe(cap, 20), 'lamp_paint', smooth=32)
    for s in (-1, 1):
        pts = [(s * 0.064, 0, 0.046), (s * 0.067, 0, 0.09), (s * 0.067, 0, 0.15), (s * 0.066, 0, 0.18),
               (s * 0.058, 0, 0.203), (s * 0.044, 0, 0.217), (s * 0.024, 0, 0.222)]
        b.add(b.tube(smooth_path(pts, 13), 0.0085, 8), 'lamp_paint', smooth=50)
        b.add(b.cyl((s * 0.068, 0, 0.190), (s * 0.081, 0, 0.190), 0.004, 8), 'lamp_paint', smooth=50)
    # wire guard: two rings and four uprights
    for z, rx, ry in ((0.108, 0.0585, 0.0515), (0.158, 0.0585, 0.0500)):
        ring = [(rx * math.cos(2 * PI * i / 22), ry * math.sin(2 * PI * i / 22), z) for i in range(22)]
        b.add(b.tube(ring, 0.0012, 5, loop=True), 'lamp_wire', smooth=180)
    prof = [(0.043, 0.079), (0.050, 0.095), (0.0545, 0.115), (0.0565, 0.135), (0.0540, 0.155), (0.046, 0.172),
            (0.040, 0.186)]
    for deg in (50, 130, 230, 310):
        a = math.radians(deg)
        pts = [(r * math.cos(a), r * math.sin(a) * 0.93, z) for r, z in prof]
        b.add(b.tube(smooth_path(pts, 9), 0.0012, 5), 'lamp_wire', smooth=180)
    # bail (upright, hanging pose); its centreline apex is the pivot
    bail = [(0.0765 * math.cos(PI * i / 26), 0, 0.190 + 0.145 * math.sin(PI * i / 26)) for i in range(27)]
    b.add(b.tube(bail, 0.002, 6), 'lamp_wire', smooth=180)
    # wick wheel, filler cap
    b.add(b.tube([(0, -0.026, 0.071), (0, -0.083, 0.066)], 0.0013, 5), 'lamp_wire', smooth=180)
    b.add(b.cyl((0, -0.082, 0.066), (0, -0.0855, 0.066), 0.008, 10), 'lamp_brass', smooth=50)
    b.add(b.lathe([(0, -0.002), (0.013, -0.002), (0.013, 0.006), (0.011, 0.008), (0, 0.008)], 12,
                  m=TRS((0, 0.052, 0.0565), (-0.22, 0, 0))), 'lamp_brass', smooth=40)
    flame = [(0, 0.099), (0.004, 0.102), (0.0075, 0.109), (0.006, 0.119), (0.003, 0.129), (0, 0.138)]
    b.add(b.lathe(flame, 8, sy=0.45), 'lamp_flame', smooth=180)
    b.transform(Matrix.Translation((0, 0, -0.335)))
    return dict(sets=[dict(b=b, tex='OilLamp', size=2048, uv='smart', alpha=True, ao_exclude=('lamp_glass', 'lamp_flame'),
                           slots={'lamp_paint': 'Metal', 'lamp_brass': 'Metal', 'lamp_wire': 'Metal',
                                  'lamp_glass': 'Glass', 'lamp_flame': 'Flame'})],
                slots=['Metal', 'Glass', 'Flame'], kinds={'Glass': 'glass', 'Flame': 'emit'}, max_tris=4000,
                view=(1.0, -1.5, 0.25), pivot='top of the carrying bail (wire centreline apex)')


# ====================================================================================== 2 canister
def build_canister():
    b = Builder(22)
    R = 0.175
    prof = [(0, 0.0)]
    for a in (18, 36, 54, 72, 90):
        prof.append((R * math.sin(math.radians(a)), 0.07 * (1 - math.cos(math.radians(a)))))
    bands = ((0.255, 0.300), (0.680, 0.725))
    for z0, z1 in bands:
        prof += [(R, z0 - 0.004), (R + 0.006, z0), (R + 0.006, z1), (R, z1 + 0.004)]
    prof.append((R, 0.91))
    for t in (18, 36, 54, 66):
        prof.append((R * math.cos(math.radians(t)), 0.91 + 0.12 * math.sin(math.radians(t))))
    prof += [(0.070, 1.020), (0.070, 1.031), (0.046, 1.031), (0.046, 1.078), (0.041, 1.086), (0, 1.086)]
    body = b.add(b.lathe(prof, 32), 'can_paint', smooth=38)

    def steel(f):
        z = f.calc_center_median().z
        return z > 1.0205 or any(z0 - 0.004 < z < z1 + 0.004 for z0, z1 in bands)
    remat(b, body, 'can_steel', steel)
    b.add(b.lathe([(0, 1.078), (0.024, 1.078), (0.024, 1.100), (0, 1.100)], 6), 'can_steel', smooth=20)
    fin = [(0.020, 0.125), (0.020, 0.255), (0.105, 0.265), (0.250, 0.170)]
    for k in range(4):
        a = math.radians(45 + 90 * k)
        er = Vector((math.cos(a), math.sin(a), 0))
        et = Vector((-math.sin(a), math.cos(a), 0))
        b.add(b.prism(fin, -0.003, 0.003, m=cols((0, 0, 1), er, et)), 'can_paint', smooth=30)
    arch = [(-(R - 0.006 + 0.047 * math.sin(PI * i / 8)), 0, 0.490 + 0.042 * math.cos(PI * i / 8)) for i in range(9)]
    b.add(b.tube(arch, 0.008, 8), 'can_steel', smooth=180)
    b.add(b.box((-(R + 0.001), 0, 0.490), (0.012, 0.062, 0.135)), 'can_steel', smooth=30)
    b.transform(Matrix.Rotation(PI / 2, 4, 'Y'))          # lathe axis Z -> X, lug side -> +Z
    # pivot = centre of mass (volume centroid of all closed shells)
    vol, c = 0.0, Vector((0, 0, 0))
    for f in b.bm.faces:
        vs = [v.co for v in f.verts]
        for i in range(1, len(vs) - 1):
            v = vs[0].dot(vs[i].cross(vs[i + 1])) / 6.0
            vol += v
            c += v * (vs[0] + vs[i] + vs[i + 1]) / 4.0
    b.transform(Matrix.Translation(-c / vol))
    return dict(sets=[dict(b=b, tex='Canister', size=2048, uv='smart',
                           slots={'can_paint': 'Steel', 'can_steel': 'Steel'})],
                slots=['Steel'], max_tris=3000, view=(0.9, -1.3, 0.55),
                pivot='centre of mass (volume centroid), long axis X, lifting lug up (+Z), valve cap at +X')


# ====================================================================================== 3 / 4 pins
def build_mappin():
    b = Builder(33)
    r = 0.005
    head = [(r * math.sin(PI * i / 6), 0.035 - r * math.cos(PI * i / 6)) for i in range(7)]
    head[0] = (0, head[0][1])
    head[-1] = (0, head[-1][1])
    b.add(b.lathe(head, 13), 'pin_head', smooth=180)
    # needle with a small ferrule under the head; the open top is hidden inside the head
    b.add(b.lathe([(0, 0), (0.0005, 0.006), (0.0005, 0.0285), (0.0013, 0.0288), (0.0013, 0.0312)], 5), 'pin_needle',
          smooth=80, ref=(0, 0, 0.015))
    return dict(sets=[dict(b=b, tex='MapPin', size=1024, uv='smart', slots={'pin_head': 'Head', 'pin_needle': 'Needle'})],
                slots=['Head', 'Needle'], max_tris=200, view=(1.0, -1.3, 0.5), pivot='needle tip')


def build_starpin():
    b = Builder(44)
    R, ri = 0.01577, 0.01577 * 0.382
    zt, zb, za = 0.0486, 0.0466, 0.0500
    top = [b.bm.verts.new(((R if i % 2 == 0 else ri) * math.sin(PI * i / 5), (R if i % 2 == 0 else ri) * math.cos(PI * i / 5), zt))
           for i in range(10)]
    bot = [b.bm.verts.new((v.co.x, v.co.y, zb)) for v in top]
    ca = b.bm.verts.new((0, 0, za))
    cb = b.bm.verts.new((0, 0, zb))
    faces = []
    for i in range(10):
        j = (i + 1) % 10
        faces.append(b.bm.faces.new((ca, top[i], top[j])))
        faces.append(b.bm.faces.new((cb, bot[j], bot[i])))
        faces.append(b.bm.faces.new((top[i], bot[i], bot[j], top[j])))
    b.add(faces, 'star_head', smooth=0.0)
    b.add(b.lathe([(0, 0.040), (0.0022, 0.040), (0.0022, 0.0445), (0.0036, 0.0468), (0, 0.0468)], 8), 'star_brass',
          smooth=50)
    b.add(b.lathe([(0, 0), (0.0006, 0.007), (0.0006, 0.041), (0, 0.041)], 6), 'pin_needle', smooth=80)
    return dict(sets=[dict(b=b, tex='StarPin', size=1024, uv='smart',
                           slots={'star_head': 'Head', 'star_brass': 'Needle', 'pin_needle': 'Needle'})],
                slots=['Head', 'Needle'], max_tris=400, view=(0.9, -1.2, 0.9), pivot='needle tip')


# ====================================================================================== 5 paper tag
TAG_L, TAG_W, TAG_T = 0.09, 0.045, 0.0003
HOLE = (0.08, 0.0225)


def tag_deformer(strong):
    k0, k1 = (10.0, 843.0) if strong else (2.0, 30.0)
    skew = 0.35 if strong else 0.15
    wamp = 0.0011 if strong else 0.00025
    cy = 0.0045 if strong else 0.0006

    def base(x, y):
        s = HOLE[0] - x
        yy = y / TAG_W - 0.5
        if s <= 0:
            return Vector((x, y, 0.5 * k0 * 0.3 * s * s)), Vector((0, 0, 1)), 0.0
        c = 1.0 + skew * yy
        n = 48
        ds = s / n
        X = Z = 0.0
        for i in range(n):
            sm = (i + 0.5) * ds
            th = (k0 * sm + 0.5 * k1 * sm * sm) * c
            X += math.cos(th) * ds
            Z += math.sin(th) * ds
        th = (k0 * s + 0.5 * k1 * s * s) * c
        return Vector((HOLE[0] - X, y, Z)), Vector((math.sin(th), 0, math.cos(th))), s

    def surf(x, y):
        p, n, s = base(x, y)
        yy = y / TAG_W - 0.5
        d = math.hypot(x - HOLE[0], y - HOLE[1])
        env = min(1.0, max(0.0, (d - 0.006) / 0.02))
        env = env * env * (3 - 2 * env)
        w = wamp * (math.sin(x * 140 + y * 60) * 0.5 + math.sin(y * 190 - x * 45 + 1.0) * 0.3
                    + math.sin(x * 310 + y * 240) * 0.2) * env
        w += cy * 4 * yy * yy * min(1.0, s / 0.08) + (cy * 0.6 * (2 * yy) * (s / 0.08) ** 2 if strong else 0.0)
        return p + n * w

    def f(x, y, z):
        e = 1e-5
        p = surf(x, y)
        nrm = (surf(x + e, y) - surf(x - e, y)).cross(surf(x, y + e) - surf(x, y - e)).normalized()
        return p + nrm * z - Vector((HOLE[0], HOLE[1], 0))
    return f


def build_tag_mesh(strong, front_mat, back_mat):
    b = Builder(55)
    bm = b.bm
    NX, NY, c = 36, 18, 0.0025
    hi, hj = 32, 9
    par = {}

    def outside(i, j):
        return (i - j > 33) or (i + j > 51)

    def inblock(i, j):
        return hi - 2 < i < hi + 2 and hj - 2 < j < hj + 2

    def mk(x, y, z):
        v = bm.verts.new((x, y, z))
        par[v] = (x, y, z)
        return v
    grid = {(i, j): mk(i * c, j * c, 0.0) for i in range(NX + 1) for j in range(NY + 1)
            if not outside(i, j) and not inblock(i, j)}
    front = []
    for i in range(NX):
        for j in range(NY):
            if hi - 2 <= i < hi + 2 and hj - 2 <= j < hj + 2:
                continue
            vs = [grid[k] for k in ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1)) if k in grid]
            if len(vs) >= 3:
                front.append(bm.faces.new(vs))
    ring = [(i, j) for i in range(hi - 2, hi + 3) for j in range(hj - 2, hj + 3)
            if max(abs(i - hi), abs(j - hj)) == 2]
    ring.sort(key=lambda k: math.atan2(k[1] - hj, k[0] - hi))
    circ = [mk(HOLE[0] + 0.0025 * math.cos(a), HOLE[1] + 0.0025 * math.sin(a), 0.0)
            for a in (math.atan2(k[1] - hj, k[0] - hi) for k in ring)]
    for n in range(16):
        m = (n + 1) % 16
        front.append(bm.faces.new((grid[ring[n]], grid[ring[m]], circ[m], circ[n])))
    for f in front:
        f.normal_update()
    bmesh.ops.recalc_face_normals(bm, faces=front)
    if sum(f.normal.z for f in front) < 0:
        bmesh.ops.reverse_faces(bm, faces=front)
    # back sheet + rim: a closed 0.3 mm solid
    fverts = set(v for f in front for v in f.verts)
    twin = {v: mk(par[v][0], par[v][1], -TAG_T) for v in fverts}
    border = [e for e in bm.edges if len(e.link_faces) == 1]
    back = [bm.faces.new([twin[v] for v in reversed(f.verts)]) for f in front]
    rim = [bm.faces.new((e.verts[0], e.verts[1], twin[e.verts[1]], twin[e.verts[0]])) for e in border]
    b.add(front + back + rim, back_mat, smooth=40)
    fm = b._mi(front_mat)
    for f in front:
        f.material_index = fm
    for f in bm.faces:
        for l in f.loops:
            x, y, z = par[l.vert]
            l[b.uv].uv = (x / TAG_L, y / TAG_W)
    fn = tag_deformer(strong)
    for v in bm.verts:
        v.co = fn(*par[v])
    return b


def build_string():
    b = Builder(56)
    pts = [(0, 0, 0.0012), (0.006, 0.0015, 0.0011), (0.014, 0.004, 0.0009), (0.032, 0.009, 0.0006),
           (0.052, 0.008, 0.0006), (0.066, 0.002, 0.0006), (0.066, -0.002, 0.0006), (0.052, -0.008, 0.0006),
           (0.032, -0.009, 0.0006), (0.014, -0.004, -0.0006), (0.006, -0.0015, -0.0011), (0, 0, -0.0012)]
    n, sides = 56, 5
    path = smooth_path(pts, n, loop=True)
    faces = b.tube(path, 0.0005, sides, loop=True)
    length = sum((path[(i + 1) % n] - path[i]).length for i in range(n))
    reps = max(1, round(length / 0.02))
    uvm = {}
    for idx, f in enumerate(faces):
        i, k = divmod(idx, sides)
        u0, u1 = reps * i / n, reps * (i + 1) / n
        uvm[f] = dict(zip(f.verts, ((u0, k / sides), (u0, (k + 1) / sides), (u1, (k + 1) / sides), (u1, k / sides))))
    b.add(faces, 'twine', smooth=180)
    for f, d in uvm.items():
        for l in f.loops:
            l[b.uv].uv = d[l.vert]
    knot = b.add(b.lathe([(0, -0.0013), (0.0013, -0.0007), (0.0017, 0.0), (0.0013, 0.0007), (0, 0.0013)], 8,
                         m=TRS((0.0672, 0, 0.0006))), 'twine', smooth=180)
    for f in knot:
        for l in f.loops:
            l[b.uv].uv = (0.5 + l.vert.co.y * 60, 0.5 + l.vert.co.z * 60)
    return b


def _tag(strong):
    fm, bk = ('paper_char_front', 'paper_char_back') if strong else ('paper_front', 'paper_back')
    tex = 'PaperTag_Charred' if strong else 'PaperTag'
    return dict(sets=[dict(b=build_tag_mesh(strong, fm, bk), tex=tex, size=1024, uv='keep', exclude=(bk,),
                           slots={fm: 'Paper', bk: 'PaperBack'}),
                      dict(b=build_string(), tex='PaperTag_String', size=1024, uv='keep', gen=gen_twine,
                           slots={'twine': 'String'})],
                slots=['Paper', 'PaperBack', 'String'], max_tris=5000, view=(0.25, -0.9, 0.8),
                pivot='centre of the punched hole (front surface)')


def build_papertag(): return _tag(False)
def build_papertag_curled(): return _tag(True)


# ====================================================================================== 6 burn ring
def build_burnring():
    b = Builder(66)
    R, n = 0.03, 32
    c = b.bm.verts.new((0, 0, 0))
    rim = [b.bm.verts.new((R * math.cos(2 * PI * i / n), R * math.sin(2 * PI * i / n), 0)) for i in range(n)]
    faces = [b.bm.faces.new((c, rim[i], rim[(i + 1) % n])) for i in range(n)]
    b.add(faces, 'burn', smooth=180, explicit=(0, 0, 1))
    for f in faces:
        for l in f.loops:
            l[b.uv].uv = (0.5 + 0.5 * l.vert.co.x / R, 0.5 + 0.5 * l.vert.co.y / R)
    return dict(sets=[dict(b=b, tex='BurnRing', size=1024, uv='keep', gen=gen_burn, slots={'burn': 'Burn'})],
                slots=['Burn'], kinds={'Burn': 'alpha'}, max_tris=64, view=(0.2, -0.8, 1.0), pivot='disc centre')


# ====================================================================================== 7 banner
def build_banner():
    W, H, NX, NY = 2.2, 7.0, 24, 64
    ZP, RS = -0.025, 0.029
    b = Builder(77)

    def fold(u, d):
        t = min(1.0, d / 1.2)
        amp = 0.095 * t * t * (3 - 2 * t)
        return amp * (0.55 * math.sin(2 * PI * (1.6 * u + 0.05 * d)) + 0.30 * math.sin(2 * PI * (3.3 * u - 0.03 * d + 0.2))
                      + 0.15 * math.sin(2 * PI * (0.7 * u + 0.02 * d) + 1.0))
    g = [[b.bm.verts.new(((i / NX - 0.5) * W, fold(i / NX, j / NY * H), ZP - RS - j / NY * H)) for i in range(NX + 1)]
         for j in range(NY + 1)]
    faces = []
    for j in range(NY):
        for i in range(NX):
            f = b.bm.faces.new((g[j][i], g[j + 1][i], g[j + 1][i + 1], g[j][i + 1]))
            for l, (ii, jj) in zip(f.loops, ((i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j))):
                l[b.uv].uv = (ii / NX, 1.0 - jj / NY)
            faces.append(f)
    b.add(faces, 'cloth_red', smooth=180, explicit=(0, -1, 0))
    p = Builder(78)
    half = [(0, -1.25), (0.018, -1.246), (0.033, -1.228), (0.038, -1.207), (0.033, -1.187), (0.022, -1.174),
            (0.030, -1.166), (0.030, -1.156), (0.025, -1.150)]
    prof = half + [(r, -z) for r, z in reversed(half)]
    rot = Matrix.Translation((0, 0, ZP)) @ Matrix.Rotation(PI / 2, 4, 'Y')
    pole = p.add(p.lathe(prof, 12, m=rot), 'wood_pole', smooth=45)
    remat(p, pole, 'pole_iron', lambda f: 1.148 < abs(f.calc_center_median().x) < 1.170)
    p.add(p.cyl((-W / 2 - 0.004, 0, ZP), (W / 2 + 0.004, 0, ZP), RS, 12, caps=False), 'cloth_sleeve', smooth=180)
    return dict(sets=[dict(b=b, tex='Banner', size=2048, uv='keep', slots={'cloth_red': 'Cloth'}),
                      dict(b=p, tex='Banner_Pole', size=1024, uv='smart',
                           slots={'wood_pole': 'Pole', 'pole_iron': 'Pole', 'cloth_sleeve': 'Pole'})],
                slots=['Cloth', 'Pole'], max_tris=4000, view=(0.75, -1.5, 0.12),
                pivot='top centre of the pole shaft (finial balls rise 1.3 cm above it)')


# ====================================================================================== 8 picture frame
def build_frame():
    b = Builder(88)
    W, H = 1.2, 1.6
    prof = [(0, 0), (0, -0.045), (0.012, -0.060), (0.030, -0.072), (0.050, -0.070), (0.075, -0.052), (0.095, -0.046),
            (0.105, -0.050), (0.118, -0.044), (0.120, -0.030), (0.120, -0.022), (0.105, -0.022), (0.105, 0)]

    def vertical(f, w, h):
        c = f.calc_center_median()
        return (w / 2 - abs(c.x)) < (h / 2 - abs(c.z))
    mould = b.add(b.frame_sweep(prof, W, H), 'gilt', smooth=50)

    def is_back(f):
        c = f.calc_center_median()
        return f.normal.y > 0.5 or (c.y > -0.0225 and abs(f.normal.y) < 0.5 and
                                    min(W / 2 - abs(c.x), H / 2 - abs(c.z)) > 0.09)
    remat(b, mould, 'fwood_x', lambda f: is_back(f) and not vertical(f, W, H))
    remat(b, mould, 'fwood_z', lambda f: is_back(f) and vertical(f, W, H))
    SW, SH, BW = W - 0.214, H - 0.214, 0.06
    sprof = [(0, -0.021), (BW, -0.021), (BW, 0.003), (BW - 0.003, 0.006), (0.003, 0.006), (0, 0.003)]
    st = b.add(b.frame_sweep(sprof, SW, SH), 'fwood_x', smooth=30)
    remat(b, st, 'fwood_z', lambda f: vertical(f, SW, SH))
    b.add(b.box((0, -0.0085, 0), (0.05, 0.025, SH - 2 * BW + 0.004)), 'fwood_z', smooth=30)
    hl = (SW - 2 * BW - 0.05) / 2
    for s in (-1, 1):
        b.add(b.box((s * (0.025 + hl / 2), -0.0085, 0.02), (hl + 0.002, 0.025, 0.05)), 'fwood_x', smooth=30)
    # corner keys (wedges) in the stretcher corners
    for sx in (-1, 1):
        for sz in (-1, 1):
            cx, cz = sx * (SW / 2 - BW), sz * (SH / 2 - BW)
            for a, c2 in (((0.050, 0.0), (0.0, 0.020)), ((0.0, 0.050), (0.020, 0.0))):
                tri = [(cx - sx * 0.012, cz - sz * 0.012), (cx - sx * a[0] - sx * 0.004, cz - sz * a[1] - sz * 0.004),
                       (cx - sx * c2[0] - sx * 0.004, cz - sz * c2[1] - sz * 0.004)]
                b.add(b.prism(tri, -0.004, 0.0005, m=cols((1, 0, 0), (0, 0, 1), (0, 1, 0))), 'fwood_x', smooth=30)
    # backing (seen between the bars) and the plain front panel
    def quad(w, h, y):
        vs = [b.bm.verts.new((sx * w / 2, y, sz * h / 2)) for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        return [b.bm.faces.new(vs)]
    b.add(quad(SW - 0.01, SH - 0.01, -0.0185), 'backboard', explicit=(0, 1, 0))
    b.add(quad(SW, SH, -0.0225), 'frontpanel', explicit=(0, -1, 0))
    # Z-clips holding the stretcher in the rabbet, each with a screw
    clip = [(0, 0), (0.022, 0), (0.022, 0.006), (0.045, 0.006), (0.045, 0.0075), (0.0205, 0.0075), (0.0205, 0.0015),
            (0, 0.0015)]
    spots = [((x, sz * H / 2), (0, -sz)) for x in (-0.28, 0.28) for sz in (-1, 1)] + \
            [((sx * W / 2, z), (-sx, 0)) for z in (-0.40, 0.40) for sx in (-1, 1)]
    for (ex, ez), (ix, iz) in spots:
        inw = Vector((ix, 0, iz))
        along = inw.cross(Vector((0, 1, 0)))
        o = Vector((ex, 0, ez)) + inw * 0.085
        m = cols(inw, (0, 1, 0), along, o)
        b.add(b.prism(clip, -0.009, 0.009, m=m), 'hardware', smooth=30)
        b.add(b.lathe([(0.0045, 0.0), (0.0045, 0.0012), (0.0025, 0.0025), (0, 0.0027)], 8,
                      m=axis_m(o + inw * 0.010 + Vector((0, 0.0015, 0)), (0, 1, 0))), 'hardware', smooth=50,
              ref=o + inw * 0.010)
    # screw eyes + slack hanging wire
    ex, ez = W / 2 - 0.055, H / 2 - 0.45
    for s in (-1, 1):
        ring = [(s * ex, 0.012 + 0.008 * math.cos(2 * PI * i / 10), ez + 0.008 * math.sin(2 * PI * i / 10)) for i in range(10)]
        b.add(b.tube(ring, 0.0017, 5, loop=True), 'hardware', smooth=180)
        b.add(b.cyl((s * ex, -0.002, ez), (s * ex, 0.005, ez), 0.0024, 6), 'hardware', smooth=80)
    wire = []
    for i in range(23):
        t = i / 22 * 2 - 1
        wire.append((t * ex, 0.012 - 0.0045 * (1 - t * t) ** 0.5 if abs(t) < 1 else 0.012, ez - 0.055 * (1 - t * t)))
    b.add(b.tube(wire, 0.0011, 5), 'hardware', smooth=180)
    for s in (-1, 1):                                      # wire twisted back on itself next to each eye
        p0, p1 = Vector(wire[1 if s < 0 else -2]), Vector(wire[3 if s < 0 else -4])
        b.add(b.cyl(p0, p1, 0.0024, 6), 'hardware', smooth=80)
    b.transform(Matrix.Translation((0, 0, -H / 2)))
    return dict(sets=[dict(b=b, tex='PictureFrame', size=2048, uv='smart',
                           slots={'gilt': 'Frame', 'fwood_x': 'Back', 'fwood_z': 'Back', 'backboard': 'Back',
                                  'hardware': 'Hardware', 'frontpanel': 'Front'})],
                slots=['Frame', 'Back', 'Hardware', 'Front'], max_tris=4000, view=(0.75, 1.5, 0.25),
                pivot='top centre, on the rear plane of the moulding')


# ====================================================================================== 9 map table
def build_maptable():
    b = Builder(99)
    LX, LY, ZT, T, PT = 3.6, 2.4, 0.95, 0.005, 0.07
    n, gap = 12, 0.003
    pw = (LY - 2 * T - (n - 1) * gap) / n
    hw, c = pw / 2, 0.004
    plank = [(-hw, ZT - PT), (hw, ZT - PT), (hw, ZT - c), (hw - c, ZT), (-hw + c, ZT), (-hw, ZT - c)]
    for i in range(n):
        y = -LY / 2 + T + hw + i * (pw + gap)
        b.add(b.prism(plank, -LX / 2 + T, LX / 2 - T, m=cols((0, 1, 0), (0, 0, 1), (1, 0, 0), (0, y, 0))), 'table_wood',
              smooth=30)
    strip = [(0, ZT - PT - 0.015), (T, ZT - PT - 0.015), (T, ZT), (0, ZT)]
    b.add(b.frame_sweep(strip, LX, LY, m=cols((1, 0, 0), (0, 0, 1), (0, 1, 0))), 'table_steel', smooth=30)

    def dome(o, d, r=0.010):
        o = Vector(o)
        b.add(b.lathe([(r, 0), (r * 0.78, r * 0.45), (0, r * 0.56)], 6, m=axis_m(o, d)), 'table_steel', smooth=180, ref=o)

    def hexb(o, d, r=0.013, h=0.009):
        o = Vector(o)
        b.add(b.lathe([(r, 0), (r, h), (0, h)], 6, m=axis_m(o, d)), 'table_steel', smooth=30, ref=o)
    for i in range(9):
        for s in (-1, 1):
            dome((-1.6 + 0.4 * i, s * LY / 2, ZT - 0.040), (0, s, 0))
    for i in range(6):
        for s in (-1, 1):
            dome((s * LX / 2, -1.0 + 0.4 * i, ZT - 0.040), (s, 0, 0))

    def angle(a, t, sx, sy):
        return [(0, 0), (sx * a, 0), (sx * a, sy * t), (sx * t, sy * t), (sx * t, sy * a), (0, sy * a)]
    ZA = ZT - PT                                            # underside of the planks
    YL, XS = 1.06, (-1.62, -0.54, 0.54, 1.62)
    LA, LT = 0.10, 0.010                                    # leg angle iron 100 x 100 x 10
    AA, AT = 0.08, 0.008                                    # apron angle iron 80 x 80 x 8
    RA, RT = 0.06, 0.006                                    # lower rails 60 x 60 x 6
    BW, BT = 0.06, 0.008                                    # flat-bar braces 60 x 8
    for s in (-1, 1):                                       # long apron rails + long lower rails
        b.add(b.prism(angle(AA, AT, -s, -1), -1.74, 1.74, m=cols((0, 1, 0), (0, 0, 1), (1, 0, 0), (0, s * YL, ZA))),
              'table_steel', smooth=30)
        b.add(b.prism(angle(RA, RT, -s, 1), -1.74, 1.74, m=cols((0, 1, 0), (0, 0, 1), (1, 0, 0), (0, s * YL, 0.20))),
              'table_steel', smooth=30)
    for x in XS:
        sx = -1 if x > 0 else 1
        xr = x + sx * LT
        b.add(b.prism(angle(AA, AT, sx, -1), -YL + AT, YL - AT, m=cols((1, 0, 0), (0, 0, 1), (0, 1, 0), (xr, 0, ZA))),
              'table_steel', smooth=30)
        b.add(b.prism(angle(RA, RT, sx, 1), -YL + RT, YL - RT, m=cols((1, 0, 0), (0, 0, 1), (0, 1, 0), (xr, 0, 0.27))),
              'table_steel', smooth=30)
        for s in (-1, 1):                                   # legs, feet, bolts
            yo = s * (YL + LT)
            b.add(b.prism(angle(LA, LT, sx, -s), 0.012, ZA - 0.002, m=Matrix.Translation((x, yo, 0))), 'table_steel',
                  smooth=30)
            b.add(b.box((x + sx * 0.04, yo - s * 0.04, 0.006), (0.18, 0.18, 0.012)), 'table_steel', smooth=30)
            for z in (ZA - 0.040, 0.230):
                hexb((x + sx * 0.055, yo, z), (0, s, 0))
            for z in (ZA - 0.040, 0.300):
                hexb((x, yo - s * 0.055, z), (-sx, 0, 0))
        # X brace across the trestle (flat bars), bolted at the crossing
        ln = math.hypot(2 * YL - 0.06, 0.46)
        an = math.atan2(0.46, 2 * YL - 0.06)
        x0 = LT + AT + 0.0005
        for k, sg in enumerate((-1, 1)):
            b.add(b.box((0, 0, 0), (BT, ln, BW), m=TRS((x + sx * (x0 + BT / 2 + (BT + 0.001) * k), 0, 0.56), (sg * an, 0, 0))),
                  'table_steel', smooth=30)
        hexb((x + sx * (x0 + 2 * BT + 0.001), 0, 0.56), (sx, 0, 0))
    for s in (-1, 1):                                       # long diagonal braces on both long sides
        for xa, xb in ((-1.62, -0.54), (1.62, 0.54)):
            ln = math.hypot(xb - xa, 0.46) - 0.06
            an = math.atan2(0.46, xb - xa)
            b.add(b.box((0, 0, 0), (ln, BT, BW),
                        m=TRS(((xa + xb) / 2, s * (YL - AT - BT / 2 - 0.0005), 0.56), (0, -an, 0))), 'table_steel',
                  smooth=30)
    return dict(sets=[dict(b=b, tex='MapTable', size=2048, uv='smart',
                           slots={'table_wood': 'Wood', 'table_steel': 'Steel'})],
                slots=['Wood', 'Steel'], max_tris=3000, view=(1.0, -1.4, 0.55), pivot='floor centre')


# ====================================================================================== numpy-made texture sets
def gen_twine(tex, size):
    rng = np.random.default_rng(5)
    v, u = np.mgrid[0:size, 0:size].astype(np.float32) / size
    ply = np.abs(np.sin(PI * (8 * u + 2 * v)))
    fibre = np.abs(np.sin(PI * (40 * u + 10 * v + 0.6 * np.sin(2 * PI * 3 * u))))
    n = L.value_noise(size, 16, rng, 5)
    h = ply ** 0.6 * 0.75 + fibre * 0.15 + n * 0.25
    base = np.array([0.36, 0.27, 0.15], np.float32)
    dark = np.array([0.10, 0.07, 0.04], np.float32)
    t = np.clip(h * 1.1 - 0.1 + (n - 0.5) * 0.5, 0, 1)[..., None]
    bc = dark * (1 - t) + base * t
    orm = np.dstack([0.45 + 0.55 * np.clip(ply ** 0.5, 0, 1), np.full_like(h, 0.9), np.zeros_like(h)])
    L.save_set(tex, bc, L.height_to_normal_dx(h, 5.0), orm)


def gen_burn(tex, size):
    """Scorch ring: RGB = char colour, alpha = burn mask (opaque black ring, soft brown halo, clear centre)."""
    rng = np.random.default_rng(7)
    v, u = np.mgrid[0:size, 0:size].astype(np.float32) / size
    r = np.hypot(u - 0.5, v - 0.5) * 2
    n = L.value_noise(size, 6, rng, 5, tile=False)
    n2 = L.value_noise(size, 48, rng, 4, tile=False)
    rr = r + (n - 0.5) * 0.22 + (n2 - 0.5) * 0.05
    ring = np.clip(1 - np.abs(rr - 0.62) / 0.16, 0, 1) ** 0.7
    halo = np.clip(1 - np.abs(rr - 0.60) / 0.34, 0, 1) ** 1.5
    edge = np.clip((0.98 - r) / 0.08, 0, 1)
    alpha = np.clip(ring * 0.95 + halo * 0.55, 0, 1) * edge
    brown = np.array([0.20, 0.10, 0.04], np.float32)
    black = np.array([0.012, 0.010, 0.009], np.float32)
    ash = np.array([0.23, 0.22, 0.20], np.float32)
    t = np.clip(ring * 1.7 - 0.15, 0, 1)[..., None]
    col = brown * (1 - t) + black * t
    a = (np.clip((n2 - 0.66) / 0.15, 0, 1) * ring)[..., None] * 0.18
    col = col * (1 - a) + ash * a
    h = -ring * 0.6 + n2 * 0.3 * ring
    orm = np.dstack([np.ones_like(r), 0.75 + 0.2 * ring, np.zeros_like(r)])
    L.save_set(tex, np.dstack([col, alpha]), L.height_to_normal_dx(h, 6.0), orm)


def gen_weave_detail():
    """Tileable plain-weave detail (normal + grey BC/ORM), 16 x 16 thread pairs per tile, for close shots."""
    size = 1024
    rng = np.random.default_rng(9)
    v, u = np.mgrid[0:size, 0:size].astype(np.float32) / size
    n = L.value_noise(size, 64, rng, 3)
    wu = np.sin(2 * PI * 16 * u)
    wv = np.sin(2 * PI * 16 * v)
    warp = np.abs(np.cos(PI * 32 * u)) ** 0.6 * (0.5 + 0.5 * wu * wv)
    weft = np.abs(np.cos(PI * 32 * v)) ** 0.6 * (0.5 - 0.5 * wu * wv)
    h = np.maximum(warp, weft) * 0.85 + n * 0.15
    g = (0.35 + 0.65 * h)[..., None] * np.ones(3, np.float32)
    orm = np.dstack([0.5 + 0.5 * h, 0.85 + 0.1 * (1 - h), np.zeros_like(h)])
    L.save_set('Banner_WeaveDetail', g, L.height_to_normal_dx(h, 9.0), orm)


# ====================================================================================== pipeline
PROPS = dict(OilLamp=build_oillamp, Canister=build_canister, MapPin=build_mappin, StarPin=build_starpin,
             PaperTag=build_papertag, PaperTag_Curled=build_papertag_curled, BurnRing=build_burnring,
             Banner=build_banner, PictureFrame=build_frame, MapTable=build_maptable)
GENERATED = set()


def to_slots(ob, order, mats):
    """Collapse the bake materials on ob into the export slots, in the given order."""
    me = ob.data
    cur = [m.name for m in me.materials]
    idx = [order.index(cur[p.material_index]) for p in me.polygons]
    me.materials.clear()
    for s in order:
        me.materials.append(mats[s])
    me.polygons.foreach_set('material_index', idx)
    me.update()


def run(key):
    sc = L.reset()
    M.MATINFO.clear()
    name = 'SM_Intro_' + key
    cfg = PROPS[key]()
    stats = dict(name=name, key=key, pivot=cfg['pivot'], slots=cfg['slots'], sets=[], shells_closed=0, shells_open=0,
                 kinds=cfg.get('kinds', {}), view=cfg['view'])
    objs, slot_tex = [], {}
    for i, s in enumerate(cfg['sets']):
        nc, no = s['b'].verify()
        stats['shells_closed'] += nc
        stats['shells_open'] += no
        ob = s['b'].to_object('%s_set%d' % (name, i), sc, M.get_mat)
        objs.append(ob)
        for slot in s['slots'].values():
            assert slot_tex.setdefault(slot, s['tex']) == s['tex'], 'slot spans two texture sets'
    for ob, s in zip(objs, cfg['sets']):
        if s['uv'] == 'smart':
            L.smart_uv(ob)
    if NO_BAKE:
        L.render_preview(sc, objs, os.path.join(L.OUT, 'preview_%s.png' % name), cfg['view'], samples=32)
    else:
        for ob, s in zip(objs, cfg['sets']):
            if 'gen' in s:
                if s['tex'] not in GENERATED:
                    s['gen'](s['tex'], s['size'])
                    GENERATED.add(s['tex'])
            else:
                cov = L.bake_set(sc, ob, s['tex'], s['size'], M.MATINFO, exclude=s.get('exclude', ()),
                                 ao_exclude=s.get('ao_exclude', ()), alpha=s.get('alpha', False))
                print('BAKED %s %d coverage %.2f' % (s['tex'], s['size'], cov))
    for s in cfg['sets']:
        stats['sets'].append(dict(tex=s['tex'], size=s['size'], slots=sorted(set(s['slots'].values())),
                                  alpha=bool(s.get('alpha')) or s['tex'] == 'BurnRing'))
    # export slots (bare materials named exactly like the slots)
    bare = {sl: bpy.data.materials.new(sl) for sl in cfg['slots']}
    for ob, s in zip(objs, cfg['sets']):
        me = ob.data
        names = [s['slots'][m.name] for m in me.materials]
        idx = [cfg['slots'].index(names[p.material_index]) for p in me.polygons]
        me.materials.clear()
        for sl in cfg['slots']:
            me.materials.append(bare[sl])
        me.polygons.foreach_set('material_index', idx)
        me.update()
    L.select_only(objs[0])
    for o in objs:
        o.select_set(True)
    if len(objs) > 1:
        bpy.ops.object.join()
    ob = objs[0]
    ob.name = name
    ob.data.name = name
    me = ob.data
    for ca in list(me.color_attributes):
        me.color_attributes.remove(ca)
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    lo, hi = L.bbox([ob])
    stats.update(tris=tris, bbox_min_cm=[round(v * 100, 3) for v in lo], bbox_max_cm=[round(v * 100, 3) for v in hi],
                 size_cm=[round((hi[i] - lo[i]) * 100, 2) for i in range(3)], max_tris=cfg['max_tris'])
    assert tris <= cfg['max_tris'], '%s: %d tris over the %d budget' % (name, tris, cfg['max_tris'])
    assert len(me.uv_layers) == 1
    L.export_fbx(ob, os.path.join(L.OUT, name + '.fbx'))
    with open(os.path.join(L.OUT, '_stats_%s.json' % key), 'w') as f:
        json.dump(stats, f, indent=1)
    print('PROP %s tris=%d size_cm=%s closed=%d open=%d' % (name, tris, stats['size_cm'], stats['shells_closed'],
                                                            stats['shells_open']))
    if not NO_BAKE:
        for i, sl in enumerate(cfg['slots']):
            me.materials[i] = L.pv_material('PV_' + sl, slot_tex[sl], cfg.get('kinds', {}).get(sl, 'opaque'))
        L.render_preview(sc, [ob], os.path.join(L.OUT, 'preview_%s.png' % name), cfg['view'])
        if DIAG:
            L.render_preview(sc, [ob], os.path.join(DIAG[3], 'diag_%s.png' % name),
                             tuple(float(v) for v in DIAG[:3]))


if __name__ == '__main__':
    os.makedirs(L.OUT, exist_ok=True)
    for k in (ONLY or list(PROPS)):
        run(k)
    if not NO_BAKE and (ONLY is None or 'Banner' in ONLY):
        gen_weave_detail()
    print('DONE')
