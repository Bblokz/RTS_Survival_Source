"""Procedural (node) materials for the intro-cinematic props. They exist only to be baked: every material exposes
colour / roughness / metallic / height (/ alpha) sockets that props_lib.bake_set routes to the output per pass.
No image textures, no text, no symbols: noise, voronoi, waves and coordinates only.
"""
import bpy, math

MATINFO = {}


class V:
    """A float/colour/vector value in a node graph: either a socket or a python constant."""

    def __init__(self, g, x):
        self.g, self.x = g, x

    def _op(self, op, o=None, clamp=False):
        n = self.g.N.new('ShaderNodeMath')
        n.operation = op
        n.use_clamp = clamp
        self.g.set(n.inputs[0], self)
        if o is not None:
            self.g.set(n.inputs[1], o)
        return V(self.g, n.outputs[0])

    def __add__(self, o): return self._op('ADD', o)
    def __radd__(self, o): return self._op('ADD', o)
    def __sub__(self, o): return self._op('SUBTRACT', o)
    def __rsub__(self, o): return V(self.g, o)._op('SUBTRACT', self)
    def __mul__(self, o): return self._op('MULTIPLY', o)
    def __rmul__(self, o): return self._op('MULTIPLY', o)
    def abs(self): return self._op('ABSOLUTE')
    def max(self, o): return self._op('MAXIMUM', o)
    def min(self, o): return self._op('MINIMUM', o)
    def pow(self, o): return self._op('POWER', o)
    def sin(self): return self._op('SINE')
    def sat(self): return self._op('ADD', 0.0, clamp=True)


class G:
    def __init__(self, name):
        self.mat = bpy.data.materials.new(name)
        self.mat.use_nodes = True
        self.nt = self.mat.node_tree
        self.nt.nodes.clear()
        self.N, self.L = self.nt.nodes, self.nt.links
        tc = self.N.new('ShaderNodeTexCoord')
        self.obj = V(self, tc.outputs['Object'])
        self.uv = V(self, tc.outputs['UV'])
        geo = self.N.new('ShaderNodeNewGeometry')
        self.pointiness = V(self, geo.outputs['Pointiness'])
        at = self.N.new('ShaderNodeAttribute')
        at.attribute_name = 'pid'
        self.pid = V(self, at.outputs['Fac'])
        # object coordinates shifted per part so separate planks / bars do not share one noise field
        self.P = self.vadd(self.obj, self.comb(self.pid * 37.0, self.pid * 91.0, self.pid * 53.0))
        MATINFO[name] = self

    # ---------------------------------------------------------------- plumbing
    def set(self, inp, v):
        if isinstance(v, V):
            v = v.x
        if isinstance(v, bpy.types.NodeSocket):
            self.L.new(v, inp)
        elif isinstance(v, (int, float)):
            if inp.type in ('RGBA',):
                inp.default_value = (v, v, v, 1)
            elif inp.type == 'VECTOR':
                inp.default_value = (v, v, v)
            else:
                inp.default_value = v
        else:
            v = tuple(v)
            if inp.type == 'RGBA' and len(v) == 3:
                v = v + (1.0,)
            inp.default_value = v

    def node(self, typ, **props):
        n = self.N.new(typ)
        for k, val in props.items():
            setattr(n, k, val)
        return n

    def noise(self, scale, detail=4.0, rough=0.55, vec=None, distort=0.0):
        n = self.node('ShaderNodeTexNoise')
        self.set(n.inputs['Vector'], vec if vec is not None else self.P)
        self.set(n.inputs['Scale'], scale)
        self.set(n.inputs['Detail'], detail)
        self.set(n.inputs['Roughness'], rough)
        self.set(n.inputs['Distortion'], distort)
        return V(self, n.outputs['Fac'])

    def vor(self, scale, vec=None, feature='F1', rand=1.0):
        n = self.node('ShaderNodeTexVoronoi', feature=feature)
        self.set(n.inputs['Vector'], vec if vec is not None else self.P)
        self.set(n.inputs['Scale'], scale)
        self.set(n.inputs['Randomness'], rand)
        return V(self, n.outputs['Distance'])

    def wave(self, scale, vec, distort=2.0, detail=2.0, dscale=1.0, direction='X'):
        n = self.node('ShaderNodeTexWave', wave_type='BANDS', bands_direction=direction)
        self.set(n.inputs['Vector'], vec)
        self.set(n.inputs['Scale'], scale)
        self.set(n.inputs['Distortion'], distort)
        self.set(n.inputs['Detail'], detail)
        self.set(n.inputs['Detail Scale'], dscale)
        return V(self, n.outputs['Fac'])

    def mr(self, x, a, b, c=0.0, d=1.0, smooth=False):
        n = self.node('ShaderNodeMapRange', clamp=True, interpolation_type='SMOOTHSTEP' if smooth else 'LINEAR')
        for k, val in zip(('Value', 'From Min', 'From Max', 'To Min', 'To Max'), (x, a, b, c, d)):
            self.set(n.inputs[k], val)
        return V(self, n.outputs[0])

    def mix(self, f, a, b):
        n = self.node('ShaderNodeMix', data_type='RGBA')
        self.set(n.inputs[0], f)
        self.set(n.inputs[6], a)
        self.set(n.inputs[7], b)
        return V(self, n.outputs[2])

    def mixf(self, f, a, b):
        n = self.node('ShaderNodeMix', data_type='FLOAT')
        self.set(n.inputs[0], f)
        self.set(n.inputs[2], a)
        self.set(n.inputs[3], b)
        return V(self, n.outputs[0])

    def mapv(self, vec, scale=(1, 1, 1), loc=(0, 0, 0), rot=(0, 0, 0)):
        n = self.node('ShaderNodeMapping')
        self.set(n.inputs['Vector'], vec)
        n.inputs['Location'].default_value = loc
        n.inputs['Rotation'].default_value = rot
        n.inputs['Scale'].default_value = scale
        return V(self, n.outputs[0])

    def xyz(self, vec):
        n = self.node('ShaderNodeSeparateXYZ')
        self.set(n.inputs[0], vec)
        return V(self, n.outputs[0]), V(self, n.outputs[1]), V(self, n.outputs[2])

    def comb(self, x, y, z):
        n = self.node('ShaderNodeCombineXYZ')
        for i, val in enumerate((x, y, z)):
            self.set(n.inputs[i], val)
        return V(self, n.outputs[0])

    def vadd(self, a, b):
        n = self.node('ShaderNodeVectorMath', operation='ADD')
        self.set(n.inputs[0], a)
        self.set(n.inputs[1], b)
        return V(self, n.outputs[0])

    def f(self, x):
        return x if isinstance(x, V) else V(self, x)

    # ---------------------------------------------------------------- outputs
    def finish(self, color, rough, metal, height=None, bump=0.0005, alpha=1.0, emit=0.0):
        self.ch = dict(color=color, rough=rough, metal=metal, alpha=alpha, mask=1.0)
        self.bsdf = self.node('ShaderNodeBsdfPrincipled')
        self.set(self.bsdf.inputs['Base Color'], color)
        self.set(self.bsdf.inputs['Roughness'], rough)
        self.set(self.bsdf.inputs['Metallic'], metal)
        if emit:
            self.set(self.bsdf.inputs['Emission Color'], color)
            self.bsdf.inputs['Emission Strength'].default_value = emit
        if height is not None:
            b = self.node('ShaderNodeBump')
            b.inputs['Strength'].default_value = 1.0
            b.inputs['Distance'].default_value = bump
            self.set(b.inputs['Height'], height)
            self.L.new(b.outputs[0], self.bsdf.inputs['Normal'])
        self.emis = self.node('ShaderNodeEmission')
        self.out = self.node('ShaderNodeOutputMaterial')
        self.tgt = self.node('ShaderNodeTexImage')
        self.L.new(self.bsdf.outputs[0], self.out.inputs[0])
        return self.mat

    def target(self, img, passname):
        self.tgt.image = img
        for n in self.N:
            n.select = False
        self.tgt.select = True
        self.N.active = self.tgt
        for l in list(self.out.inputs[0].links):
            self.L.remove(l)
        if passname == 'normal':
            self.L.new(self.bsdf.outputs[0], self.out.inputs[0])
            return
        for l in list(self.emis.inputs[0].links):
            self.L.remove(l)
        self.set(self.emis.inputs[0], self.ch[passname])
        self.L.new(self.emis.outputs[0], self.out.inputs[0])


# ====================================================================================== shared recipes
RUST_A = (0.125, 0.050, 0.022)
RUST_B = (0.07, 0.03, 0.018)
STEEL = (0.23, 0.22, 0.21)


def worn(g, paint, s=40.0, paint_rough=0.5, paint_metal=0.0, under=STEEL, under_rough=0.38, under_metal=1.0,
         chip=0.5, edge=0.6, rust=0.5, scratch=0.4, dirt=0.4, var=0.2, soot=None, extra_reveal=None):
    """Paint (or plating) over a base metal: chips, edge wear, hairline scratches, rust blooms, grime, optional soot.
    s = feature frequency per metre (40 suits a 30 cm object)."""
    P = g.P
    n1 = g.noise(s, 6.0, 0.62, P)
    n2 = g.noise(s * 5.0, 3.0, 0.5, P)
    edge_m = g.mr(g.pointiness, 0.515, 0.60)
    cm = n1 * 0.72 + n2 * 0.28 + edge_m * (edge * 0.10)
    t = 0.74 - chip * 0.2
    chipm = g.mr(cm, t, t + 0.012)
    halo = g.mr(cm, t - 0.07, t + 0.01, smooth=True)
    sn = g.noise(s * 0.5, 1.0, 0.5, g.mapv(P, scale=(0.6, 45.0, 6.0), rot=(0.4, 0.7, 0.3)))
    sn2 = g.noise(s * 0.4, 1.0, 0.5, g.mapv(P, scale=(40.0, 0.6, 5.0), rot=(0.2, -0.5, 1.1)))
    patch = g.mr(g.noise(s * 0.35, 2.0, 0.5, P), 0.42, 0.6)
    scr = (g.mr((sn - 0.5).abs(), 0.006, 0.0) .max(g.mr((sn2 - 0.5).abs(), 0.005, 0.0))) * patch * scratch
    reveal = chipm.max(scr)
    if extra_reveal is not None:
        reveal = reveal.max(extra_reveal)
    rn = g.noise(s * 0.6, 5.0, 0.65, g.vadd(P, (3.1, 7.7, 1.3)))
    rustm = g.mr(rn, 0.62 - rust * 0.25, 0.70 - rust * 0.2)
    pv = g.noise(s * 0.18, 3.0, 0.5, P)
    pa = tuple(c * (1 - var) for c in paint)
    pb = tuple(min(1.0, c * (1 + var)) for c in paint)
    paintc = g.mix(g.mr(pv, 0.3, 0.7), pa, pb)
    dm = g.mr(g.noise(s * 0.5, 5.0, 0.7, g.vadd(P, (9.0, 2.0, 5.0))), 0.45, 0.8) * dirt
    paintc = g.mix(dm, paintc, tuple(c * 0.35 + 0.01 for c in paint))
    paintc = g.mix(halo * rustm * (0.75 * rust), paintc, RUST_A)
    rustc = g.mix(n2, RUST_B, RUST_A)
    underc = g.mix(rustm, under, rustc)
    col = g.mix(reveal, paintc, underc)
    rough = g.mixf(reveal, g.f(paint_rough) + dm * 0.25 + (n2 - 0.5) * 0.12, g.mixf(rustm, under_rough, 0.88))
    metal = g.mixf(reveal, paint_metal, g.mixf(rustm, under_metal, 0.0))
    height = (1.0 - chipm) * 0.7 + n2 * 0.12 - scr * 0.35 + rustm * reveal * 0.2
    if soot is not None:
        col = g.mix(soot, col, (0.012, 0.011, 0.010))
        rough = g.mixf(soot, rough, 0.93)
        metal = g.mixf(soot, metal, 0.0)
    return col, rough, metal, height


def wood(g, dark, light, axis='X', s=1.0, rough=0.5, wear=0.5, stain=0.5, dust=0.0):
    """Sawn timber with the grain along axis. s scales the grain frequency."""
    P = g.P
    sc_f = {'X': (0.035, 1, 1), 'Y': (1, 0.035, 1), 'Z': (1, 1, 0.035)}[axis]
    sc_r = {'X': (0.12, 1, 1), 'Y': (1, 0.12, 1), 'Z': (1, 1, 0.12)}[axis]
    streak = g.noise(260.0 * s, 5.0, 0.6, g.mapv(P, scale=sc_f))
    rings = g.wave(55.0 * s, g.mapv(P, scale=sc_r), distort=5.0, detail=3.0, dscale=0.6,
                   direction='Y' if axis != 'Y' else 'X')
    pores = g.mr(g.noise(1400.0 * s, 2.0, 0.5, g.mapv(P, scale=tuple(c if c == 1 else 0.02 for c in sc_f))), 0.62, 0.72)
    gr = streak * 0.6 + rings * 0.4
    col = g.mix(g.mr(gr, 0.25, 0.75), dark, light)
    col = g.mix(pores * 0.55, col, tuple(c * 0.4 for c in dark))
    st = g.mr(g.noise(3.5, 5.0, 0.65, g.vadd(P, (4.0, 1.0, 8.0))), 0.48, 0.72, smooth=True) * stain
    col = g.mix(st, col, tuple(c * 0.3 for c in dark))
    edge_m = g.mr(g.pointiness, 0.52, 0.62)
    wn = g.noise(9.0, 5.0, 0.7, g.vadd(P, (1.0, 6.0, 2.0)))
    wr = (g.mr(wn, 0.58, 0.7) * 0.6 + edge_m * 0.15).sat() * wear
    col = g.mix(wr, col, tuple(min(1.0, c * 1.3 + 0.01) for c in light))
    sn = g.noise(30.0, 2.0, 0.5, g.mapv(P, scale=(1.0, 9.0, 2.0), rot=(0.1, 0.2, 0.9)))
    scr = g.mr((sn - 0.5).abs(), 0.004, 0.0) * g.mr(g.noise(4.0, 2.0, 0.5, P), 0.45, 0.6) * wear
    col = g.mix(scr * 0.7, col, tuple(min(1.0, c * 1.8 + 0.05) for c in light))
    r = g.f(rough) + (streak - 0.5) * 0.25 - st * 0.18 + wr * 0.2 + scr * 0.2
    if dust:
        dn = g.mr(g.noise(6.0, 4.0, 0.6, g.vadd(P, (2.0, 2.0, 2.0))), 0.35, 0.75) * dust
        col = g.mix(dn, col, (0.30, 0.27, 0.23))
        r = r + dn * 0.2
    height = gr * 0.5 - pores * 0.4 - scr * 0.8 + streak * 0.3
    return col, r.sat(), 0.0, height


# ====================================================================================== materials
def _reg(fn):
    REG[fn.__name__[2:]] = fn
    return fn


REG = {}


def get_mat(name):
    m = bpy.data.materials.get(name)
    if m is None or name not in MATINFO:
        m = REG[name](name)
    return m


# ---- oil lamp (object space: pivot at the bail top, lamp base at z = -0.335)
LAMP_BASE = -0.335


def _lamp_soot(g, lo=0.150, hi=0.225, amt=1.0):
    x, y, z = g.xyz(g.obj)
    n = g.noise(55.0, 5.0, 0.65)
    return (g.mr(z + (n - 0.5) * 0.06, LAMP_BASE + lo, LAMP_BASE + hi, smooth=True) * (0.55 + n * 0.6) * amt).sat()


@_reg
def m_lamp_paint(name):
    g = G(name)
    c, r, m, h = worn(g, (0.066, 0.090, 0.076), s=42.0, paint_rough=0.42, chip=0.70, rust=0.75, scratch=0.7, dirt=0.7,
                      soot=_lamp_soot(g))
    return g.finish(c, r, m, h, bump=0.00035)


@_reg
def m_lamp_brass(name):
    g = G(name)
    c, r, m, h = worn(g, (0.52, 0.36, 0.12), s=70.0, paint_rough=0.36, paint_metal=1.0, under=(0.10, 0.075, 0.045),
                      under_rough=0.6, under_metal=0.6, chip=0.7, rust=0.15, scratch=0.5, dirt=0.8,
                      soot=_lamp_soot(g, 0.085, 0.13, 0.9))
    return g.finish(c, r, m, h, bump=0.0002)


@_reg
def m_lamp_wire(name):
    g = G(name)
    c, r, m, h = worn(g, (0.30, 0.29, 0.27), s=120.0, paint_rough=0.42, paint_metal=1.0, chip=0.55, rust=0.9,
                      scratch=0.2, dirt=0.6, soot=_lamp_soot(g, 0.17, 0.26, 0.7))
    return g.finish(c, r, m, h, bump=0.0002)


@_reg
def m_lamp_glass(name):
    g = G(name)
    x, y, z = g.xyz(g.obj)
    n = g.noise(38.0, 5.0, 0.7)
    streak = g.noise(60.0, 4.0, 0.6, g.mapv(g.obj, scale=(1.0, 1.0, 0.12)))
    soot = (g.mr(z + (n - 0.5) * 0.05, LAMP_BASE + 0.125, LAMP_BASE + 0.19, smooth=True) * (0.35 + streak * 0.9)).sat()
    haze = g.mr(n, 0.4, 0.75) * 0.25
    col = g.mix(soot, (0.62, 0.66, 0.60), (0.018, 0.015, 0.012))
    alpha = (0.10 + haze * 0.35 + soot * 0.78).sat()
    rough = (0.06 + haze * 0.5 + soot * 0.55).sat()
    return g.finish(col, rough, 0.0, n, bump=0.00005, alpha=alpha)


@_reg
def m_lamp_flame(name):
    g = G(name)
    x, y, z = g.xyz(g.obj)
    t = g.mr(z, LAMP_BASE + 0.100, LAMP_BASE + 0.138)
    col = g.mix(g.mr(t, 0.0, 0.3), (0.05, 0.12, 0.9), (1.0, 0.62, 0.12))
    col = g.mix(g.mr(t, 0.3, 0.6), col, (1.0, 0.86, 0.45))
    col = g.mix(g.mr(t, 0.75, 1.0), col, (1.0, 0.40, 0.05))
    return g.finish(col, 1.0, 0.0, alpha=g.mr(t, 1.0, 0.7, 0.35, 1.0), emit=8.0)


# ---- canister (long axis X)
@_reg
def m_can_paint(name):
    g = G(name)
    x, y, z = g.xyz(g.obj)
    # long drag scuffs along the axis from being dropped and hauled
    dn = g.noise(26.0, 3.0, 0.6, g.mapv(g.obj, scale=(0.06, 1.0, 1.0)))
    drag = g.mr(dn, 0.69, 0.74) * g.mr(g.noise(3.0, 2.0, 0.5), 0.45, 0.6)
    c, r, m, h = worn(g, (0.085, 0.100, 0.084), s=11.0, paint_rough=0.56, chip=0.72, rust=0.8, scratch=0.9, dirt=0.85,
                      var=0.14, extra_reveal=drag)
    # rain streaks running around the girth, darker underside
    sv = g.noise(9.0, 4.0, 0.6, g.mapv(g.obj, scale=(1.0, 0.1, 0.1)))
    c = g.mix(g.mr(sv, 0.55, 0.8) * 0.35, c, (0.03, 0.028, 0.022))
    c = g.mix(g.mr(z, 0.02, -0.17) * 0.35, c, (0.035, 0.03, 0.022))
    return g.finish(c, r, m, h, bump=0.0006)


@_reg
def m_can_steel(name):
    g = G(name)
    c, r, m, h = worn(g, (0.26, 0.26, 0.25), s=16.0, paint_rough=0.40, paint_metal=1.0, chip=0.5, rust=0.95,
                      scratch=1.0, dirt=0.7, var=0.25)
    return g.finish(c, r, m, h, bump=0.0004)


# ---- Purple Death canister (2026-11-21): near-black violet casing, purple bands, glowing ampoules
@_reg
def m_pdcan_paint(name):
    g = G(name)
    c, r, m, h = worn(g, (0.040, 0.028, 0.050), s=10.0, paint_rough=0.5, chip=0.65, rust=0.7, scratch=0.9, dirt=0.8,
                      var=0.12)
    return g.finish(c, r, m, h, bump=0.0006)


@_reg
def m_pdcan_band(name):
    g = G(name)
    c, r, m, h = worn(g, (0.26, 0.045, 0.36), s=14.0, paint_rough=0.45, chip=0.6, rust=0.5, scratch=0.8, dirt=0.7,
                      var=0.15)
    return g.finish(c, r, m, h, bump=0.0005)


@_reg
def m_pdcan_glow(name):
    g = G(name)
    return g.finish((0.55, 0.14, 0.85), 0.15, 0.0, emit=3.0)


# ---- pins
@_reg
def m_pin_head(name):
    g = G(name)
    c, r, m, h = worn(g, (0.74, 0.71, 0.62), s=900.0, paint_rough=0.24, under=(0.10, 0.09, 0.08), under_metal=0.0,
                      under_rough=0.6, chip=0.35, edge=0.0, rust=0.0, scratch=0.7, dirt=0.5, var=0.06)
    return g.finish(c, r, m, h, bump=0.00003)


@_reg
def m_pin_needle(name):
    g = G(name)
    c, r, m, h = worn(g, (0.50, 0.50, 0.50), s=900.0, paint_rough=0.30, paint_metal=1.0, chip=0.4, edge=0.0, rust=0.8,
                      scratch=0.3, dirt=0.4, var=0.1)
    return g.finish(c, r, m, h, bump=0.00002)


@_reg
def m_star_head(name):
    g = G(name)
    c, r, m, h = worn(g, (0.30, 0.013, 0.012), s=420.0, paint_rough=0.28, under=(0.42, 0.29, 0.10), under_rough=0.35,
                      chip=0.22, edge=1.5, rust=0.0, scratch=0.8, dirt=0.6, var=0.12)
    return g.finish(c, r, m, h, bump=0.00004)


@_reg
def m_star_brass(name):
    g = G(name)
    c, r, m, h = worn(g, (0.50, 0.34, 0.11), s=600.0, paint_rough=0.34, paint_metal=1.0, under=(0.10, 0.075, 0.045),
                      under_rough=0.6, under_metal=0.5, chip=0.6, edge=0.0, rust=0.1, scratch=0.5, dirt=0.7)
    return g.finish(c, r, m, h, bump=0.00003)


# ---- paper tag (UV space: 9 x 4.5 cm, hole at u = 8/9, v = 0.5)
def _paper(name, char):
    g = G(name)
    u, v, _ = g.xyz(g.uv)
    X, Y = u * 0.09, v * 0.045
    de = X.min(0.09 - X).min(Y).min(0.045 - Y)
    de = de.min(((0.09 - X) + Y - 0.0075) * 0.7071).min(((0.09 - X) + (0.045 - Y) - 0.0075) * 0.7071)
    uvm = g.comb(X, Y, 0.0)
    fib = g.noise(900.0, 3.0, 0.6, g.mapv(uvm, scale=(0.18, 1.0, 1.0)))
    fine = g.noise(4000.0, 2.0, 0.5, uvm)
    blot = g.noise(70.0, 5.0, 0.65, uvm)
    col = g.mix(g.mr(fib, 0.3, 0.7), (0.56, 0.44, 0.27), (0.68, 0.56, 0.36))
    col = g.mix(g.mr(blot, 0.5, 0.75) * 0.55, col, (0.36, 0.25, 0.13))
    col = g.mix(g.mr(fine, 0.62, 0.72) * 0.35, col, (0.30, 0.22, 0.13))
    en = g.noise(160.0, 4.0, 0.7, uvm)
    col = g.mix(g.mr(de + (en - 0.5) * 0.004, 0.0035, 0.0, smooth=True) * 0.7, col, (0.25, 0.17, 0.09))
    dx, dy = X - 0.08, Y - 0.0225
    rr = (dx * dx + dy * dy).pow(0.5)
    ring = g.mr(rr, 0.0064, 0.0061)
    col = g.mix(ring * 0.8, col, (0.40, 0.21, 0.12))
    cr = g.noise(45.0, 1.0, 0.5, g.mapv(uvm, rot=(0, 0, 0.5)))
    crease = g.mr(cr, 0.45, 0.75, smooth=True) * 0.25          # soft buckling only, no line marks
    rough = (0.82 + (fib - 0.5) * 0.2 - ring * 0.1).sat()
    height = fib * 0.5 + fine * 0.3 + ring * 1.5 + crease * 1.2
    if char:
        bn = g.noise(55.0, 5.0, 0.7, g.vadd(uvm, (0.3, 0.1, 0.0)))
        scorch = g.mr(de + (bn - 0.5) * 0.030, 0.026, 0.004, smooth=True)
        burnt = g.mr(de + (bn - 0.5) * 0.022, 0.0095, 0.003)
        spots = g.mr(g.noise(110.0, 4.0, 0.6, g.vadd(uvm, (0.7, 0.2, 0.0))), 0.66, 0.74)
        col = g.mix(0.45, col, (0.33, 0.20, 0.09))
        col = g.mix((scorch + spots * 0.7).sat(), col, (0.10, 0.05, 0.02))
        col = g.mix(burnt, col, (0.012, 0.011, 0.010))
        ash = g.mr(g.noise(700.0, 3.0, 0.6, uvm), 0.6, 0.75) * burnt
        col = g.mix(ash * 0.22, col, (0.22, 0.21, 0.20))
        rough = (rough + scorch * 0.1).sat()
        blister = g.noise(260.0, 3.0, 0.6, uvm)
        height = height + scorch * blister * 3.0 - burnt * 1.0
    return g.finish(col, rough, 0.0, height, bump=0.00012)


@_reg
def m_paper_front(name): return _paper(name, False)
@_reg
def m_paper_back(name): return _paper(name, False)
@_reg
def m_paper_char_front(name): return _paper(name, True)
@_reg
def m_paper_char_back(name): return _paper(name, True)


# ---- banner (UV space 0-1 over 2.2 x 7 m; 160 x 512 thread pairs -> about 13 x 4 px per pair at 2048)
@_reg
def m_cloth_red(name):
    g = G(name)
    u, v, _ = g.xyz(g.uv)
    X, Y = u * 2.2, v * 7.0
    M = g.comb(X, Y, 0.0)
    a = (u * (160.0 * 2 * math.pi)).sin()
    b = (v * (512.0 * 2 * math.pi) + 0.7854).sin()
    slub = g.noise(55.0, 3.0, 0.6, g.mapv(M, scale=(1.0, 0.05, 1.0)))
    slub2 = g.noise(55.0, 3.0, 0.6, g.mapv(M, scale=(0.05, 1.0, 1.0), loc=(3.0, 0, 0)))
    weave = (a * b) * 0.5 + 0.5
    hgt = weave * 0.7 + slub * 0.25 + slub2 * 0.25
    base = g.mix(g.mr(g.noise(1.3, 4.0, 0.6, M), 0.3, 0.7), (0.26, 0.012, 0.014), (0.36, 0.022, 0.020))
    col = g.mix(g.mr(weave, 0.0, 1.0) * 0.45 + (slub - 0.5) * 0.5, tuple(c * 0.55 for c in (0.26, 0.012, 0.014)), base)
    # sun-faded upper part, grime rising from the hem, damp streaks
    fade = g.mr(g.noise(0.9, 3.0, 0.6, g.vadd(M, (5.0, 1.0, 0.0))), 0.4, 0.75) * g.mr(v, 0.35, 1.0)
    col = g.mix(fade * 0.35, col, (0.42, 0.10, 0.07))
    streak = g.noise(7.0, 4.0, 0.65, g.mapv(M, scale=(1.0, 0.06, 1.0), loc=(0.0, 4.0, 0.0)))
    col = g.mix(g.mr(streak, 0.55, 0.8) * 0.45, col, (0.07, 0.012, 0.012))
    grime = (g.mr(v + (g.noise(4.0, 4.0, 0.6, M) - 0.5) * 0.12, 0.16, 0.0, smooth=True) * 0.75).sat()
    col = g.mix(grime, col, (0.045, 0.030, 0.022))
    blot = g.mr(g.noise(2.6, 5.0, 0.7, g.vadd(M, (8.0, 3.0, 0.0))), 0.62, 0.78) * 0.4
    col = g.mix(blot, col, (0.09, 0.015, 0.012))
    # stitched hems: 3 cm folds down both sides and along the bottom
    hem = g.mr(X.min(2.2 - X).min(Y), 0.032, 0.029)
    stitch = g.mr((X.min(2.2 - X).min(Y) - 0.026).abs(), 0.0022, 0.0008) * g.mr(((X + Y) * 420.0).sin(), -0.2, 0.3)
    col = g.mix(stitch * 0.6, col, (0.10, 0.02, 0.02))
    col = g.mix(hem * 0.15, col, (0.12, 0.01, 0.01))
    rough = (0.86 + (slub - 0.5) * 0.15 + grime * 0.08).sat()
    height = hgt + hem * 0.9 - stitch * 0.8
    return g.finish(col, rough, 0.0, height, bump=0.0022)


@_reg
def m_cloth_sleeve(name):
    g = G(name)
    n = g.noise(700.0, 2.0, 0.5, g.mapv(g.obj, scale=(1.0, 1.0, 1.0)))
    slub = g.noise(55.0, 3.0, 0.6, g.mapv(g.obj, scale=(0.05, 1.0, 1.0)))
    col = g.mix(g.mr(n * 0.5 + slub * 0.5, 0.3, 0.7), (0.15, 0.008, 0.009), (0.34, 0.02, 0.02))
    return g.finish(col, 0.88, 0.0, n, bump=0.0012)


@_reg
def m_wood_pole(name):
    g = G(name)
    c, r, m, h = wood(g, (0.045, 0.026, 0.014), (0.13, 0.075, 0.035), axis='X', rough=0.42, wear=0.7, stain=0.5)
    return g.finish(c, r, m, h, bump=0.0005)


@_reg
def m_pole_iron(name):
    g = G(name)
    c, r, m, h = worn(g, (0.035, 0.035, 0.038), s=60.0, paint_rough=0.5, chip=0.6, rust=0.8, scratch=0.5, dirt=0.4)
    return g.finish(c, r, m, h, bump=0.0003)


# ---- picture frame
@_reg
def m_gilt(name):
    g = G(name)
    cav = g.mr(g.pointiness, 0.5, 0.43)
    c, r, m, h = worn(g, (0.40, 0.26, 0.065), s=14.0, paint_rough=0.40, paint_metal=1.0, under=(0.095, 0.034, 0.022),
                      under_rough=0.7, under_metal=0.0, chip=0.58, edge=1.2, rust=0.0, scratch=0.8, dirt=0.9, var=0.22)
    grime = (cav * 0.8 + g.mr(g.noise(5.0, 5.0, 0.7), 0.5, 0.8) * 0.5).sat()
    c = g.mix(grime * 0.75, c, (0.035, 0.026, 0.016))
    r = g.mixf(grime * 0.7, r, 0.8)
    m = g.mixf(grime * 0.6, m, 0.0)
    # fine craquelure in the gesso under the leaf
    crk = g.mr(g.vor(260.0, feature='DISTANCE_TO_EDGE'), 0.012, 0.0) * g.mr(g.noise(9.0, 3.0, 0.5), 0.5, 0.65)
    c = g.mix(crk * 0.7, c, (0.05, 0.03, 0.02))
    return g.finish(c, r, m, h - crk * 0.6, bump=0.0006)


def _framewood(name, axis):
    g = G(name)
    c, r, m, h = wood(g, (0.115, 0.070, 0.036), (0.30, 0.205, 0.115), axis=axis, rough=0.78, wear=0.35, stain=0.8,
                      dust=0.5)
    return g.finish(c, r, m, h, bump=0.0006)


@_reg
def m_fwood_x(name): return _framewood(name, 'X')
@_reg
def m_fwood_z(name): return _framewood(name, 'Z')


@_reg
def m_backboard(name):
    g = G(name)
    P = g.P
    n = g.noise(5.0, 6.0, 0.7)
    fine = g.noise(900.0, 2.0, 0.5)
    lin = g.noise(1200.0, 2.0, 0.5, g.mapv(P, scale=(1.0, 1.0, 0.03)))
    col = g.mix(g.mr(n, 0.3, 0.7), (0.105, 0.070, 0.040), (0.19, 0.135, 0.080))
    col = g.mix(g.mr(fine * 0.5 + lin * 0.5, 0.4, 0.7) * 0.35, col, (0.26, 0.20, 0.13))
    tide = g.noise(2.2, 4.0, 0.6, g.vadd(P, (6.0, 0.0, 3.0)))
    col = g.mix(g.mr(tide, 0.42, 0.80, smooth=True) * 0.45, col, (0.075, 0.052, 0.032))
    sn = g.noise(14.0, 2.0, 0.5, g.mapv(P, scale=(7.0, 1.0, 1.0), rot=(0.0, 0.4, 0.0)))
    scr = g.mr((sn - 0.5).abs(), 0.003, 0.0) * g.mr(g.noise(3.0, 2.0, 0.5), 0.5, 0.62)
    col = g.mix(scr * 0.6, col, (0.34, 0.27, 0.18))
    dust = g.mr(g.noise(3.0, 5.0, 0.6, g.vadd(P, (1.0, 1.0, 1.0))), 0.4, 0.8) * 0.45
    col = g.mix(dust, col, (0.27, 0.24, 0.20))
    return g.finish(col, (0.86 + (fine - 0.5) * 0.1).sat(), 0.0, fine * 0.5 + lin * 0.5 - scr, bump=0.0004)


@_reg
def m_frontpanel(name):
    g = G(name)
    n = g.noise(4.0, 5.0, 0.6)
    fine = g.noise(1500.0, 2.0, 0.5)
    col = g.mix(g.mr(n, 0.3, 0.7), (0.035, 0.030, 0.026), (0.060, 0.050, 0.040))
    return g.finish(col, (0.6 + (n - 0.5) * 0.2).sat(), 0.0, fine, bump=0.0002)


@_reg
def m_hardware(name):
    g = G(name)
    c, r, m, h = worn(g, (0.27, 0.26, 0.24), s=260.0, paint_rough=0.45, paint_metal=1.0, chip=0.6, rust=0.9,
                      scratch=0.3, dirt=0.6)
    return g.finish(c, r, m, h, bump=0.0002)


# ---- map table
@_reg
def m_table_wood(name):
    g = G(name)
    c, r, m, h = wood(g, (0.034, 0.019, 0.010), (0.115, 0.064, 0.030), axis='X', s=0.8, rough=0.44, wear=0.5,
                      stain=0.75)
    return g.finish(c, r, m, h, bump=0.0009)


@_reg
def m_table_steel(name):
    g = G(name)
    x, y, z = g.xyz(g.obj)
    c, r, m, h = worn(g, (0.060, 0.072, 0.068), s=9.0, paint_rough=0.5, chip=0.6, edge=1.2, rust=0.8, scratch=0.9,
                      dirt=0.7, var=0.18)
    foot = g.mr(z + (g.noise(12.0, 4.0, 0.6) - 0.5) * 0.18, 0.14, 0.0, smooth=True) * 0.6
    c = g.mix(foot, c, (0.085, 0.042, 0.022))
    r = g.mixf(foot, r, 0.85)
    m = g.mixf(foot, m, 0.0)
    return g.finish(c, r, m, h, bump=0.0006)


# ---- placeholders for sets whose textures are generated with numpy (string, burn ring)
@_reg
def m_twine(name):
    g = G(name)
    return g.finish((0.36, 0.27, 0.15), 0.9, 0.0)


@_reg
def m_burn(name):
    g = G(name)
    return g.finish((0.03, 0.02, 0.015), 0.85, 0.0)


# ---- ammunition on the table (object space, metres)
@_reg
def m_ammo_mag(name):
    g = G(name)
    # blued steel: dark, worn bright only on edges and in fine scratches
    c, r, m, h = worn(g, (0.028, 0.029, 0.033), s=120.0, paint_rough=0.38, paint_metal=0.85, under=(0.30, 0.30, 0.31),
                      under_rough=0.3, chip=0.0, edge=0.0, rust=0.1, scratch=0.5, dirt=0.35, var=0.08)
    return g.finish(c, r, m, h, bump=0.00003)


@_reg
def m_ammo_brass(name):
    g = G(name)
    c, r, m, h = worn(g, (0.66, 0.47, 0.19), s=900.0, paint_rough=0.28, paint_metal=1.0, under=(0.26, 0.18, 0.08),
                      under_rough=0.55, under_metal=0.8, chip=0.15, edge=0.0, rust=0.0, scratch=0.5, dirt=0.65, var=0.14)
    return g.finish(c, r, m, h, bump=0.00001)


@_reg
def m_ammo_copper(name):
    g = G(name)
    c, r, m, h = worn(g, (0.58, 0.29, 0.16), s=900.0, paint_rough=0.3, paint_metal=1.0, under=(0.2, 0.1, 0.06),
                      under_rough=0.6, under_metal=0.7, chip=0.15, edge=0.0, rust=0.0, scratch=0.4, dirt=0.6, var=0.12)
    return g.finish(c, r, m, h, bump=0.00001)


@_reg
def m_ammo_card(name):
    g = G(name)
    c, r, m, h = worn(g, (0.40, 0.34, 0.22), s=180.0, paint_rough=0.86, paint_metal=0.0, under=(0.27, 0.22, 0.15),
                      under_rough=0.92, under_metal=0.0, chip=0.25, edge=1.2, rust=0.0, scratch=0.25, dirt=0.75, var=0.18)
    return g.finish(c, r, m, h, bump=0.00004)
