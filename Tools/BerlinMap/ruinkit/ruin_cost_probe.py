"""Where the pixel-shader instructions of M_BerlinRuin go: compiles UNSAVED variants and prints their statistics.
Nothing is written to Content. Docs: Source/Docs/Environment/BerlinRuinKitMaterial.md ("Where the instructions go").

  sh ruin_cmd_run.sh ruin_cost_probe.py cost_probe      -> Saved/Berlin/ruinkit_textures/ruinkit_cost_probe.txt

Sections:
  baseline   an empty default-lit material and what the engine's own base pass costs with a few material flags
  legacy     the 2026-11-12 graph (351 instructions) with one feature knocked out at a time (the delta is its cost)
  current    ruin_material_build.build_graph: wall, wall close-up, rubble
  holes      the current wall + the shared library's hole subgraph (make_wall_holes_material._inject, unchanged),
             one-sided / two-sided, and three library changes that are only PROPOSED (never saved, never in the library):
             no grid code for NoGrid instances, mesh-space position through a vertex interpolator, the plaster chip
             height as the edge noise instead of the extra LowResBlurredNoise sample
Numbers are MaterialEditingLibrary.get_statistics on the editor's shader platform (PCD3D_SM6), default quality level.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ruin_material_build as B  # noqa: E402
import ruin_params as RP  # noqa: E402

MEL = unreal.MaterialEditingLibrary
PROP = unreal.MaterialProperty
WH = B.WH
ROWS = []
COUNT = [0]


def log(row):
    ROWS.append(row)
    unreal.log('RUINKIT_PROBE ' + row)


def fresh(blend=None, two_sided=False):
    COUNT[0] += 1
    m = unreal.new_object(unreal.Material, name='M_RuinProbe_%d' % COUNT[0])
    if blend is not None:
        m.set_editor_property('blend_mode', blend)
        m.set_editor_property('opacity_mask_clip_value', 0.5)
    m.set_editor_property('two_sided', two_sided)
    return m


def measure(label, m, note=''):
    MEL.recompile_material(m)
    s = MEL.get_statistics(m)
    log('%-46s ps=%d vs=%d samplers=%d ps_tex=%d interp=%d %s' % (
        label, s.num_pixel_shader_instructions, s.num_vertex_shader_instructions, s.num_samplers,
        s.num_pixel_texture_samples, s.num_interpolator_scalars, note))
    return s.num_pixel_shader_instructions


def connect(g, base, rough, normal):
    for node, prop in ((base, PROP.MP_BASE_COLOR), (rough, PROP.MP_ROUGHNESS), (normal, PROP.MP_NORMAL)):
        if node is not None and not MEL.connect_material_property(node, '', prop):
            g.failed += 1
    if g.failed:
        log('FAILED LINKS %d on %s' % (g.failed, g.m.get_name()))


# ----------------------------------------------------------------------------- baseline

def baseline():
    def empty(**flags):
        m = fresh()
        const = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, 0, 0)
        MEL.connect_material_property(const, '', PROP.MP_BASE_COLOR)
        for key, value in flags.items():
            try:
                m.set_editor_property(key, value)
            except Exception as error:  # noqa: BLE001
                log('baseline: cannot set %s (%r)' % (key, error))
        return m
    base = measure('baseline empty default lit', empty())
    measure('baseline + fully rough', empty(fully_rough=True))
    try:
        measure('baseline + no decal response', empty(material_decal_response=unreal.MaterialDecalResponse.MDR_NONE))
    except Exception as error:  # noqa: BLE001
        log('baseline: decal response not settable (%r)' % (error,))
    for name in ('MDR_COLOR', 'MDR_COLOR_NORMAL', 'MDR_COLOR_ROUGHNESS', 'MDR_NORMAL', 'MDR_ROUGHNESS'):
        value = getattr(unreal.MaterialDecalResponse, name, None)
        if value is not None:
            measure('baseline + decal response %s' % name, empty(material_decal_response=value))
    try:
        measure('baseline + fully rough + decal colour only', empty(
            fully_rough=True, material_decal_response=unreal.MaterialDecalResponse.MDR_COLOR))
    except Exception as error:  # noqa: BLE001
        log('baseline: combination failed (%r)' % (error,))
    m = empty()
    flat = MEL.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, 0, 200)
    flat.set_editor_property('constant', unreal.LinearColor(0, 0, 1, 0))
    MEL.connect_material_property(flat, '', PROP.MP_NORMAL)
    measure('baseline + constant flat normal connected', m)
    m = empty()
    vc = MEL.create_material_expression(m, unreal.MaterialExpressionVertexColor, 0, 200)
    MEL.connect_material_property(vc, '', PROP.MP_BASE_COLOR)
    measure('baseline + vertex colour as base colour', m)
    return base


# ----------------------------------------------------------------------------- legacy graph (2026-11-12), with knock-outs

def legacy(g, textures, cut=()):
    """The graph as it was built on 2026-11-12 (351 instructions). `cut` names features to leave out."""
    u = unreal
    uv0 = g.node(u.MaterialExpressionTextureCoordinate, coordinate_index=0)
    uv = g.to_pixel(g.mul(uv0, g.scalar('RuinWallTile')))
    vc = g.node(u.MaterialExpressionVertexColor)
    plas = g.texture(textures['T_BerlinRuin_Plaster_D'], uv)
    brick = plas if 'brick_sample' in cut else g.texture(textures['T_BerlinRuin_Brick_D'], uv)
    chip, cav = (plas, 'A'), (brick, 'A')
    width = g.scalar('RuinPlasterEdgeWidth')
    loss = g.add((vc, 'G'), g.scalar('RuinPlasterLoss'))
    threshold = g.sub(g.mul(loss, g.add(g.mul(width, 2.0), 1.0)), width)
    d = g.div(g.sub(chip, threshold), width)
    plaster = g.sat(d)
    plaster_col = g.mul((plas, 'RGB'), g.vector_rgb('RuinPlasterColor'))
    if 'edge_light' not in cut:
        edge = g.mul(g.sat(g.sub(2.0, d)), plaster)
        plaster_col = g.mul(plaster_col, g.add(g.mul(edge, g.scalar('RuinPlasterEdgeLight')), 1.0))
    brick_col = g.mul((brick, 'RGB'), g.vector_rgb('RuinBrickColor'))
    if 'edge_shadow' not in cut:
        shadow = g.mul(g.sat(g.add(g.mul(d, 0.5), 1.0)), g.unary(u.MaterialExpressionOneMinus, plaster))
        brick_col = g.mul(brick_col, g.sub(1.0, g.mul(shadow, g.scalar('RuinPlasterEdgeShadow'))))
    base0 = g.lerp(brick_col, plaster_col, plaster)
    if 'rough' in cut:
        rough0 = g.scalar('RuinRoughness')
    else:
        rough0 = g.lerp(g.lerp(g.scalar('RuinBrickRoughJoint'), g.scalar('RuinBrickRoughFace'), cav),
                        g.scalar('RuinPlasterRoughness'), plaster)
    if 'normal' in cut:
        normal = None
    else:
        nrm = g.texture(textures['T_BerlinRuin_N'], uv, linear=True)
        nxy = g.lerp(g.mask((nrm, 'RGBA'), 'rg'), g.mask((nrm, 'RGBA'), 'ba'), plaster)
        nxy = g.mul(g.add(g.mul(nxy, 2.0), -1.0), g.scalar('RuinNormalStrength'))
        normal = g.append(nxy, g.const(1.0))
    noise = chip if 'noise_lerp' in cut else g.lerp(cav, chip, plaster)
    if 'cpd' in cut:
        tint = g.vector_rgb('RuinTintFallback')
        soot_amount = g.scalar('RuinSootAmountFallback')
    else:
        cpd = g.node(u.MaterialExpressionVectorParameter, parameter_name='RuinCPD_TintSoot',
                     default_value=u.LinearColor(1, 1, 1, 1), use_custom_primitive_data=True,
                     primitive_data_index=RP.CPD_TINT_FIRST, group='BerlinRuin')
        tint_set = g.sat(g.mul(g.add(g.add((cpd, 'R'), (cpd, 'G')), (cpd, 'B')), 1.0e6))
        tint = g.lerp(g.vector_rgb('RuinTintFallback'), g.mask(cpd, 'rgb'), tint_set)
        soot_set = g.sat(g.mul(g.unary(u.MaterialExpressionAbs, (cpd, 'A')), 1.0e6))
        soot_amount = g.lerp(g.scalar('RuinSootAmountFallback'), g.max((cpd, 'A'), 0.0), soot_set)
    base = g.mul(base0, tint)
    soot = None
    if 'soot' not in cut:
        soot_k = g.add(g.scalar('RuinSootStrength'), g.mul(g.scalar('RuinSootBreakup'), g.sub(noise, 0.5)))
        soot = g.sat(g.mul(g.mul((vc, 'R'), soot_amount), soot_k))
        base = g.lerp(base, g.mul(base, g.vector_rgb('RuinSootColor')), soot)
    dust = None
    if 'dust' not in cut:
        dust_k = g.add(g.scalar('RuinDustStrength'), g.mul(g.scalar('RuinDustBreakup'), g.sub(0.5, noise)))
        dust = g.mul(g.sat(g.mul((vc, 'B'), dust_k)), g.scalar('RuinDustOpacity'))
        base = g.lerp(base, g.vector_rgb('RuinDustColor'), dust)
    rough = rough0
    if 'matte' not in cut and 'rough' not in cut and soot is not None and dust is not None:
        rough = g.lerp(rough0, g.const(0.97), g.max(soot, dust))
    return base, rough, normal


def legacy_section(textures):
    def build(cut=()):
        m = fresh()
        g = B.Graph(m)
        connect(g, *legacy(g, textures, cut))
        return m
    full = measure('legacy wall (2026-11-12 graph)', build())
    for cut, what in ((('normal',), 'normal map sample + blend + tangent transform'),
                      (('edge_light',), 'plaster rim light'),
                      (('edge_shadow',), 'brick shadow under the plaster edge'),
                      (('edge_light', 'edge_shadow'), 'both edge terms'),
                      (('rough',), 'roughness by cavity/plaster + matte -> one scalar'),
                      (('matte',), 'matte roughness under soot and dust'),
                      (('noise_lerp',), 'breakup noise lerp(cavity, chip) -> chip'),
                      (('cpd',), 'custom primitive data fallbacks in the pixel shader'),
                      (('soot',), 'soot'),
                      (('dust',), 'dust'),
                      (('brick_sample',), 'the brick texture sample (plaster used for both)'),
                      (('normal', 'edge_light', 'edge_shadow', 'rough', 'noise_lerp', 'cpd'), 'all cuts taken')):
        ps = measure('legacy - ' + '+'.join(cut), build(cut), '')
        log('    -> %s: %d instructions' % (what, full - ps))


# ----------------------------------------------------------------------------- current graph

def current(textures, rubble=False, close_up=False, blend=None, two_sided=False, flags=None):
    m = fresh(blend, two_sided)
    for key, value in (B.MASTER_FLAGS if flags is None else flags).items():
        m.set_editor_property(key, value)
    g = B.Graph(m)
    g.switch_defaults = {RP.STATIC_SWITCH: rubble, RP.CLOSE_UP_SWITCH: close_up}
    connect(g, *B.build_graph(g, textures))
    return m, g


def current_section(textures):
    mdr = unreal.MaterialDecalResponse
    measure('current wall graph, engine default flags', current(textures, flags={})[0])
    measure('current wall graph, fully rough', current(textures, flags={'fully_rough': True})[0])
    measure('current wall graph, decal colour only', current(textures, flags={'material_decal_response': mdr.MDR_COLOR})[0])
    measure('current wall graph, no decal response', current(textures, flags={'material_decal_response': mdr.MDR_NONE})[0])
    measure('current rubble graph, engine default flags', current(textures, rubble=True, flags={})[0])
    log('--- with the build flags %s' % {k: str(v) for k, v in B.MASTER_FLAGS.items()})
    measure('current wall', current(textures)[0])
    measure('current wall, Ruin_CloseUpDetail', current(textures, close_up=True)[0])
    measure('current rubble', current(textures, rubble=True)[0])


# ----------------------------------------------------------------------------- holes

NO_GRID_CODE = '''float3 P = LocalPos;
float MinRatio = 1.0e4;
''' + WH._hole_code() + '''
float Visible = 1.0 - step(MinRatio, 1.0);
float Rim = 1.0 - smoothstep(1.0, 1.45, MinRatio);
return float2(Visible, Rim);'''


class Recorder:
    """Stands in for MaterialEditingLibrary inside the library's _inject so the probe can find the nodes it made."""
    def __init__(self):
        self.nodes = []

    def __getattr__(self, name):
        return getattr(MEL, name)

    def create_material_expression(self, material, cls, x, y):
        node = MEL.create_material_expression(material, cls, x, y)
        self.nodes.append(node)
        return node


def holes(textures, two_sided, no_grid=False, vs_local=False, chip_noise=False, inner_tint=True):
    m, g = current(textures, blend=unreal.BlendMode.BLEND_MASKED, two_sided=two_sided)
    recorder = Recorder()
    WH.MEL = recorder
    try:
        failed = WH._inject(m, inner_tint)
    finally:
        WH.MEL = MEL
    if failed:
        log('holes: %d failed links in the library subgraph' % failed)
    of = lambda cls: [n for n in recorder.nodes if isinstance(n, cls)]
    custom = of(unreal.MaterialExpressionCustom)[0]
    if no_grid:
        custom.set_editor_property('code', NO_GRID_CODE)
    if vs_local:
        local = of(unreal.MaterialExpressionTransformPosition)[0]
        interp = MEL.create_material_expression(m, unreal.MaterialExpressionVertexInterpolator, -1000, 800)
        masks = of(unreal.MaterialExpressionComponentMask)[:2]      # the noise UV's xy and z masks
        ok = [MEL.connect_material_expressions(local, '', interp, ''),
              MEL.connect_material_expressions(interp, '', custom, 'LocalPos')]
        ok += [MEL.connect_material_expressions(interp, '', mask, '') for mask in masks]
        if not all(ok):
            log('holes: vs_local relink failed %s' % ok)
    if chip_noise:
        if not MEL.connect_material_expressions(g.samples['T_BerlinRuin_Plaster_D'], 'A', custom, 'Noise'):
            log('holes: chip noise relink failed')
    return m


def holes_section(textures):
    measure('current wall + holes, one-sided', holes(textures, False))
    measure('current wall + holes, two-sided (= the instance)', holes(textures, True))
    measure('current wall + holes, two-sided, no inner tint', holes(textures, True, inner_tint=False))
    measure('PROPOSAL no-grid code, two-sided', holes(textures, True, no_grid=True))
    measure('PROPOSAL mesh-space position in the VS, two-sided', holes(textures, True, vs_local=True))
    measure('PROPOSAL chip height as edge noise, two-sided', holes(textures, True, chip_noise=True))
    measure('PROPOSAL all three, two-sided', holes(textures, True, no_grid=True, vs_local=True, chip_noise=True))


def saved_section():
    """The assets on disk as they are, and with `used_with_static_lighting` switched off IN MEMORY (never saved).
    get_statistics reports the surface-lightmap base pass for a material with that flag and the volumetric-lightmap
    base pass for one without: counts are only comparable inside one of the two."""
    names = ['MI_BerlinRuin_Wall', RP.HOLES_INSTANCE, 'MI_BerlinRuin_Rubble']

    def rows(tag):
        for name in names:
            asset = unreal.load_asset(B.FOLDER + '/' + name)
            s = MEL.get_statistics(asset)
            log('%-46s ps=%d vs=%d samplers=%d ps_tex=%d interp=%d' % (
                'saved %s%s' % (name, tag), s.num_pixel_shader_instructions, s.num_vertex_shader_instructions,
                s.num_samplers, s.num_pixel_texture_samples, s.num_interpolator_scalars))
    masters = [unreal.load_asset(B.MASTER), unreal.load_asset(B.HOLES_MASTER)]
    log('saved masters: used_with_static_lighting %s, fully_rough %s, decal response %s' % (
        [m.get_editor_property('used_with_static_lighting') for m in masters],
        [m.get_editor_property('fully_rough') for m in masters],
        [str(m.get_editor_property('material_decal_response')) for m in masters]))
    rows('')
    if any(m.get_editor_property('used_with_static_lighting') for m in masters):
        for m in masters:
            m.set_editor_property('used_with_static_lighting', False)
            MEL.recompile_material(m)
        rows(' (static lighting flag off, unsaved)')


def run():
    textures = {name: unreal.load_asset(B.FOLDER + '/' + name) for name in RP.TEXTURES}
    for name in ('RuinRoughness',):
        RP.SCALARS.setdefault(name, 0.9)
    saved_section()
    baseline()
    legacy_section(textures)
    current_section(textures)
    holes_section(textures)
    with open(os.path.join(B.TEX_DIR, 'ruinkit_cost_probe.txt'), 'w') as f:
        f.write('\n'.join(ROWS) + '\n')
    log('PROBE DONE')


if __name__ == '__main__':
    run()
