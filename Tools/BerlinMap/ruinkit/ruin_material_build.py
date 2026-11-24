"""Builds the Berlin ruin kit material set (idempotent). Docs: Source/Docs/Environment/BerlinRuinKitMaterial.md

Creates / refreshes in /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/:
  T_BerlinRuin_Plaster_D, _Brick_D, _N, _Rubble_D   (from Saved/Berlin/ruinkit_textures/, ruin_textures_gen.py)
  M_BerlinRuin                opaque master (graph rebuilt in place, so instances and mesh slots stay valid)
  MI_BerlinRuin_Wall          Ruin_IsRubble off
  MI_BerlinRuin_Rubble        Ruin_IsRubble on
  MI_BerlinRuin_Rubble_Foot   child of the rubble instance for the footprint skirts (darker, thinner dust)
  M_BerlinRuin_WallHoles      masked copy made by Tools/WallChunks/make_wall_holes_material.masked_master_for
                              (deleted and re-made each run so it never goes stale against M_BerlinRuin)
  MI_BerlinRuin_Wall_Holes    make_holes_instance(MI_BerlinRuin_Wall, NoGrid, two-sided, inner tint)

Headless (never in the shared editor: clearing a master that a level uses asserts there):
  sh ruin_cmd_run.sh ruin_material_build.py <logname> RUIN_MODE=all
  = UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
    -AllowCommandletRendering (the stats need a renderer)
Modes (env RUIN_MODE): build (default) | stats | all
Every editor that has the kit loaded keeps the .uasset files open and the saves then fail ("Error moving file"):
see "Rebuild while editors are open" in the doc (ruin_wait_unlocked.ps1).
Cost attribution of the graph: ruin_cost_probe.py. Pictures: ruin_look_test.py.
"""
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'WallChunks')))
import ruin_params as RP  # noqa: E402
import make_wall_holes_material as WH  # noqa: E402

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROP = unreal.MaterialProperty
ME = unreal

FOLDER = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials'
MASTER = FOLDER + '/M_BerlinRuin'
HOLES_MASTER = MASTER + WH.SUFFIX
PROJECT = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..'))
TEX_DIR = os.path.join(PROJECT, 'Saved', 'Berlin', 'ruinkit_textures')
PLACEHOLDER_MESH = '/Engine/BasicShapes/Cube'   # NoGrid: the hole instance only needs bounds for unused grid params
DBK_COMPARE = ['/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/MI_Berlin_Ruin_Brick',
               '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/MI_Berlin_Ruin_Cinderblock',
               '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/MI_Berlin_Ruin_Concrete',
               '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/M_RusBunker_Concrete',
               '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/M_RusBunker_Concrete_WallHoles']


# Material flags of the master (the hole master is a copy, so it gets them too). Measured in ruin_cost_probe.py.
MASTER_FLAGS = {
    # No baked lighting in this project: the flag only adds lightmap shader permutations (and makes get_statistics
    # report the surface-lightmap base pass instead of the one every other material is measured with).
    'used_with_static_lighting': False,
    # The map draws rubble heaps and skirts as hierarchical instanced meshes: without the flag a cooked game shows
    # the default material on them (the editor only sets it on the fly, on whatever copy is loaded).
    'used_with_instanced_static_meshes': True,
    # Roughness forced to 1 and no specular: the kit is 0.8-0.97 rough everywhere, and the base pass saves 12
    # instructions. The roughness pin is then ignored, so build_graph does not build one.
    'fully_rough': True,
    # Decals still colour the kit (tracks, dirt, craters across skirts and heaps) but no longer change its normal and
    # roughness: one DBuffer texture read instead of three, 23 instructions less.
    'material_decal_response': unreal.MaterialDecalResponse.MDR_COLOR,
}


def log(msg):
    unreal.log('RUINKIT_MAT ' + msg)


RUN_START = [0.0]


def _save(asset):
    if not EAL.save_loaded_asset(asset, False):
        log('SAVE FAILED ' + asset.get_path_name())
        raise RuntimeError('save failed (Error moving file? asset open elsewhere): ' + asset.get_path_name())


# ----------------------------------------------------------------------------- textures

def import_textures():
    tasks = []
    for name in RP.TEXTURES:
        path = os.path.join(TEX_DIR, name + '.png')
        if not os.path.isfile(path):
            raise RuntimeError('missing texture source ' + path + ' (run ruin_textures_gen.py)')
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = FOLDER
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False   # save=True silently misses the disk in this setup; saved explicitly below
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    textures = {}
    for name, cfg in RP.TEXTURES.items():
        tex = unreal.load_asset(FOLDER + '/' + name)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
        tex.set_editor_property('srgb', cfg['srgb'])
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        _save(tex)
        textures[name] = tex
    log('textures imported: ' + ', '.join(textures))
    return textures


# ----------------------------------------------------------------------------- graph helpers

class Graph:
    def __init__(self, material):
        self.m = material
        self.x = -2400
        self.y = -1200
        self.failed = 0
        self.flags = dict(MASTER_FLAGS)   # material flags the graph depends on (Fully Rough: no roughness pin)
        self.switch_defaults = {}   # the cost probe compiles the branches of an unsaved master through these
        self.samples = {}           # texture name -> sample node (the cost probe taps them)

    def node(self, cls, **props):
        n = MEL.create_material_expression(self.m, cls, self.x, self.y)
        self.y += 90
        if self.y > 2400:
            self.y = -1200
            self.x += 260
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, src, out, dst, pin):
        if not MEL.connect_material_expressions(src, out, dst, pin):
            self.failed += 1
            log('LINK FAILED %s.%s -> %s.%s' % (src.get_name(), out, dst.get_name(), pin))

    def _in(self, dst, pin, value):
        """value: (node, output) tuple, node, or a float (written as the pin's const)."""
        if isinstance(value, (int, float)):
            dst.set_editor_property('const_' + pin.lower(), float(value))
            return
        node, out = value if isinstance(value, tuple) else (value, '')
        self.link(node, out, dst, pin)

    def op(self, cls, a, b):
        n = self.node(cls)
        self._in(n, 'A', a)
        self._in(n, 'B', b)
        return n

    def add(self, a, b):
        return self.op(unreal.MaterialExpressionAdd, a, b)

    def sub(self, a, b):
        return self.op(unreal.MaterialExpressionSubtract, a, b)

    def mul(self, a, b):
        return self.op(unreal.MaterialExpressionMultiply, a, b)

    def div(self, a, b):
        return self.op(unreal.MaterialExpressionDivide, a, b)

    def max(self, a, b):
        return self.op(unreal.MaterialExpressionMax, a, b)

    def lerp(self, a, b, alpha):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        self._in(n, 'A', a)
        self._in(n, 'B', b)
        self._in(n, 'Alpha', alpha)
        return n

    def unary(self, cls, a):
        n = self.node(cls)
        node, out = a if isinstance(a, tuple) else (a, '')
        self.link(node, out, n, '')
        return n

    def sat(self, a):
        return self.unary(unreal.MaterialExpressionSaturate, a)

    def mask(self, a, channels):
        n = self.node(unreal.MaterialExpressionComponentMask)
        for c in 'rgba':
            n.set_editor_property(c, c in channels)
        node, out = a if isinstance(a, tuple) else (a, '')
        self.link(node, out, n, '')
        return n

    def scalar(self, name):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name,
                         default_value=float(RP.SCALARS[name]), group='BerlinRuin')

    def vector_rgb(self, name):
        r, g, b = RP.VECTORS[name]
        n = self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                      default_value=unreal.LinearColor(r, g, b, 1.0), group='BerlinRuin')
        return self.mask(n, 'rgb')

    def switch(self, when_rubble, when_wall, name=None):
        name = name or RP.STATIC_SWITCH
        n = self.node(unreal.MaterialExpressionStaticSwitchParameter, parameter_name=name,
                      default_value=bool(self.switch_defaults.get(name, False)), group='BerlinRuin')
        self._in(n, 'True', when_rubble)
        self._in(n, 'False', when_wall)
        return n

    def close_up(self, when_on, when_off):
        return self.switch(when_on, when_off, RP.CLOSE_UP_SWITCH)

    def const(self, value):
        return self.node(unreal.MaterialExpressionConstant, r=float(value))

    def append(self, a, b):
        n = self.node(unreal.MaterialExpressionAppendVector)
        self._in(n, 'A', a)
        self._in(n, 'B', b)
        return n

    def to_pixel(self, value):
        """Vertex interpolator: everything feeding it runs in the vertex shader."""
        n = self.node(unreal.MaterialExpressionVertexInterpolator)
        node, out = value if isinstance(value, tuple) else (value, '')
        self.link(node, out, n, '')
        return n

    def cpd_scalar(self, name, index):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=0.0,
                         use_custom_primitive_data=True, primitive_data_index=index, group='BerlinRuin')

    def texture(self, tex, uv, linear=False):
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if linear else \
            unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        n = self.node(unreal.MaterialExpressionTextureSample, texture=tex, sampler_type=sampler)
        self.link(uv, '', n, 'UVs')
        self.samples[tex.get_name()] = n
        return n


# ----------------------------------------------------------------------------- master

def _customized_uv0():
    names = [n for n in dir(PROP) if 'CUSTOMIZED' in n.upper() and n.endswith('0')]
    if not names:
        raise RuntimeError('no customized UV property in ' + str([n for n in dir(PROP) if n.startswith('MP_')]))
    return getattr(PROP, names[0])


def build_graph(g, textures):
    """The whole M_BerlinRuin graph on g.m. Returns (base colour, roughness, normal) nodes.
    Per-pixel work is kept to what the game camera (20-40 m up) shows; everything that is constant per vertex or per
    component (UV tile, custom primitive data fallbacks, vertex-colour scaling) runs in the vertex shader and reaches
    the pixel shader through three vertex interpolators (7 scalars). Parameter-only maths folds into the preshader."""
    # UVs in the vertex shader: UV0 (1 unit = 4 m) x the tile scalar picked by the rubble switch.
    # Customized UVs are not reachable from Python; a vertex interpolator moves the multiply to the VS the same way.
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    uv = g.to_pixel(g.mul(uv0, g.switch(g.scalar('RuinRubbleTile'), g.scalar('RuinWallTile'))))
    vc = g.node(unreal.MaterialExpressionVertexColor)
    with_roughness = not g.flags.get('fully_rough', False)
    flat = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 1, 0))

    # ---- per component, vertex shader: CPD 0-3 = tint RGB + soot amount. Unset CPD reads 0 -> parameter fallbacks.
    cpd = g.node(unreal.MaterialExpressionVectorParameter, parameter_name='RuinCPD_TintSoot',
                 default_value=unreal.LinearColor(1, 1, 1, 1), use_custom_primitive_data=True,
                 primitive_data_index=RP.CPD_TINT_FIRST, group='BerlinRuin')
    tint_set = g.sat(g.mul(g.add(g.add((cpd, 'R'), (cpd, 'G')), (cpd, 'B')), 1.0e6))
    tint = g.lerp(g.vector_rgb('RuinTintFallback'), g.mask(cpd, 'rgb'), tint_set)
    tint = g.switch(g.mul(tint, g.vector_rgb('RuinRubbleColor')), tint)      # rubble: its colour multiplier rides along
    soot_set = g.sat(g.mul(g.unary(unreal.MaterialExpressionAbs, (cpd, 'A')), 1.0e6))
    soot_amount = g.lerp(g.scalar('RuinSootAmountFallback'), g.max((cpd, 'A'), 0.0), soot_set)
    per_component = g.to_pixel(g.append(tint, g.mul((vc, 'R'), soot_amount)))   # rgb tint, a = vertex soot x amount
    tint_ps = g.mask(per_component, 'rgb')
    soot_ps = g.mask(per_component, 'a')

    # ---- wall: plaster / brick by vertex G thresholded against the plaster chip height (Plaster_D.A).
    plas = g.texture(textures['T_BerlinRuin_Plaster_D'], uv)
    brick = g.texture(textures['T_BerlinRuin_Brick_D'], uv)
    chip = (plas, 'A')
    cav = (brick, 'A')
    width = g.scalar('RuinPlasterEdgeWidth')
    # t = (G + loss) * (1 + 2w) - w = G * k + t0 (vertex shader) ; d = (chip - t) / w ; plaster = saturate(d)
    k = g.add(g.mul(width, 2.0), 1.0)
    t0 = g.sub(g.mul(g.scalar('RuinPlasterLoss'), k), width)
    threshold = g.to_pixel(g.add(g.mul((vc, 'G'), k), t0))
    d = g.mul(g.sub(chip, threshold), g.div(1.0, width))
    plaster = g.sat(d)
    plaster_col = g.mul((plas, 'RGB'), g.vector_rgb('RuinPlasterColor'))
    brick_col = g.mul((brick, 'RGB'), g.vector_rgb('RuinBrickColor'))
    # Ruin_CloseUpDetail (off by default): rim light and shadow at the plaster edge, roughness by cavity, normal map.
    edge = g.mul(g.sat(g.sub(2.0, d)), plaster)                 # bright broken rim, 1..2 widths inside
    shadow = g.mul(g.sat(g.add(g.mul(d, 0.5), 1.0)), g.unary(unreal.MaterialExpressionOneMinus, plaster))
    plaster_rim = g.mul(plaster_col, g.add(g.mul(edge, g.scalar('RuinPlasterEdgeLight')), 1.0))
    brick_rim = g.mul(brick_col, g.sub(1.0, g.mul(shadow, g.scalar('RuinPlasterEdgeShadow'))))
    wall_base = g.lerp(g.close_up(brick_rim, brick_col), g.close_up(plaster_rim, plaster_col), plaster)
    nrm = g.texture(textures['T_BerlinRuin_N'], uv, linear=True)
    nxy = g.lerp(g.mask((nrm, 'RGBA'), 'rg'), g.mask((nrm, 'RGBA'), 'ba'), plaster)
    nxy = g.mul(g.add(g.mul(nxy, 2.0), -1.0), g.scalar('RuinNormalStrength'))
    normal_detail = g.append(nxy, g.const(1.0))                  # the shader normalises the tangent normal
    wall_normal = g.close_up(normal_detail, flat)
    wall_noise = g.close_up(g.lerp(cav, chip, plaster), chip)    # soot / dust breakup: the chip height is noise enough

    # ---- rubble: one texture; desaturation (CPD 4 or the parameter) and contrast about a grey pivot, then the tint.
    rub = g.texture(textures['T_BerlinRuin_Rubble_D'], uv)
    desat_cpd = g.cpd_scalar('RuinCPD_Desaturation', RP.CPD_DESATURATION)
    desat_set = g.sat(g.mul(g.unary(unreal.MaterialExpressionAbs, desat_cpd), 1.0e6))
    desat = g.to_pixel(g.lerp(g.scalar('RuinRubbleDesaturation'), g.sat(desat_cpd), desat_set))
    luma = g.node(unreal.MaterialExpressionDotProduct)
    g._in(luma, 'A', (rub, 'RGB'))
    g._in(luma, 'B', g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0.3, 0.59, 0.11, 0)))
    rub_grey = g.lerp((rub, 'RGB'), luma, desat)
    contrast = g.scalar('RuinRubbleContrast')
    pivot = g.mul(g.scalar('RuinRubblePivot'), g.unary(unreal.MaterialExpressionOneMinus, contrast))
    rub_base = g.add(g.mul(rub_grey, contrast), pivot)

    base0 = g.switch(rub_base, wall_base)
    rough = None
    if with_roughness:      # only when the master is not Fully Rough: one scalar, by cavity in the close-up branch
        roughness = g.scalar('RuinRoughness')
        rough_detail = g.lerp(g.lerp(g.scalar('RuinBrickRoughJoint'), g.scalar('RuinBrickRoughFace'), cav),
                              g.scalar('RuinPlasterRoughness'), plaster)
        rough = g.switch(roughness, g.close_up(rough_detail, roughness))
    noise = g.switch((rub, 'A'), wall_noise)
    normal = g.switch(flat, wall_normal)

    # ---- tint, soot, dust (shared). The breakup terms are written as noise * a + b so a and b fold into the preshader.
    base1 = g.mul(base0, tint_ps)
    soot_breakup = g.scalar('RuinSootBreakup')
    soot_k = g.add(g.mul(noise, soot_breakup), g.sub(g.scalar('RuinSootStrength'), g.mul(soot_breakup, 0.5)))
    soot = g.sat(g.mul(soot_ps, soot_k))
    # lerp(base, base * SootColor, soot) = base * (1 + soot * (SootColor - 1))
    base2 = g.mul(base1, g.add(g.mul(g.add(g.vector_rgb('RuinSootColor'), -1.0), soot), 1.0))
    dust_breakup = g.scalar('RuinDustBreakup')
    dust_k = g.sub(g.add(g.scalar('RuinDustStrength'), g.mul(dust_breakup, 0.5)), g.mul(noise, dust_breakup))
    dust = g.mul(g.sat(g.mul((vc, 'B'), dust_k)), g.scalar('RuinDustOpacity'))
    base3 = g.lerp(base2, g.vector_rgb('RuinDustColor'), dust)
    return base3, rough, normal


def clear_graph(material):
    for _ in range(30):   # delete_all_material_expressions only removes about half per call
        left = MEL.get_num_material_expressions(material)
        if left == 0:
            break
        log('clearing %d expressions' % left)
        MEL.delete_all_material_expressions(material)


def build_master(textures):
    if EAL.does_asset_exist(MASTER):
        material = unreal.load_asset(MASTER)
        clear_graph(material)        # headless only: this asserts in an editor that has the master in use on a level
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_BerlinRuin', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property('two_sided', False)
    for key, value in MASTER_FLAGS.items():
        material.set_editor_property(key, value)
    g = Graph(material)
    base, rough, normal = build_graph(g, textures)
    for node, prop in ((base, PROP.MP_BASE_COLOR), (rough, PROP.MP_ROUGHNESS), (normal, PROP.MP_NORMAL)):
        if node is not None and not MEL.connect_material_property(node, '', prop):
            g.failed += 1
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    _save(material)
    log('master built, %d expressions, %d failed links' % (MEL.get_num_material_expressions(material), g.failed))
    if g.failed:
        raise RuntimeError('M_BerlinRuin: %d failed links' % g.failed)
    return material


# ----------------------------------------------------------------------------- instances

def _apply(mi, scalars, vectors, switches):
    for name, value in switches.items():
        MEL.set_material_instance_static_switch_parameter_value(mi, name, bool(value))
    for name, value in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, name, float(value))
    for name, (r, g, b) in vectors.items():
        MEL.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(r, g, b, 1.0))
    MEL.update_material_instance(mi)


def build_instances():
    out = {}
    for name, (parent, scalars, vectors, switches) in RP.INSTANCES.items():
        path = FOLDER + '/' + name
        if EAL.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, unreal.load_asset(FOLDER + '/' + parent))
        _apply(mi, scalars, vectors, switches)
        _save(mi)
        out[name] = mi
    return out


def build_holes():
    """The masked hole variant, exactly by the shared library (never edited afterwards)."""
    if EAL.does_asset_exist(HOLES_MASTER) and not EAL.delete_asset(HOLES_MASTER):
        # Never fall back to deleting MI_BerlinRuin_Wall_Holes: every ruin Blueprint references it. The library would
        # silently reuse the stale master, so stop here instead.
        raise RuntimeError('could not delete the stale ' + HOLES_MASTER + ' (open in another editor?)')
    mi = WH.make_holes_instance(FOLDER + '/MI_BerlinRuin_Wall', FOLDER, unreal.load_asset(PLACEHOLDER_MESH),
                                grid_axis=None, two_sided=True, name=RP.HOLES_INSTANCE, inner_tint=True,
                                inner_darken=RP.HOLES_INNER_DARKEN, inner_color=RP.HOLES_INNER_COLOR)
    _parent, scalars, vectors, switches = RP.INSTANCES['MI_BerlinRuin_Wall']
    _apply(mi, scalars, vectors, switches)       # every static switch: the kept instance must not drift from the wall
    _save(mi)
    master = unreal.load_asset(HOLES_MASTER)
    names = [str(n) for n in MEL.get_scalar_parameter_names(master)]
    fresh = 'RuinRubbleContrast' in names and 'WallHole0R' in names
    log('holes master %s blend=%s fresh=%s, instance parent=%s two_sided=%s' % (
        master.get_name(), master.get_editor_property('blend_mode'), fresh,
        mi.get_editor_property('parent').get_name(),
        mi.get_editor_property('base_property_overrides').get_editor_property('two_sided')))
    if not fresh or mi.get_editor_property('parent') != master:
        raise RuntimeError('M_BerlinRuin_WallHoles is not a fresh copy of the new master')
    return mi


def verify_on_disk():
    content = os.path.join(PROJECT, 'Content')
    names = list(RP.TEXTURES) + ['M_BerlinRuin', 'M_BerlinRuin' + WH.SUFFIX, RP.HOLES_INSTANCE] + list(RP.INSTANCES)
    rel = FOLDER.replace('/Game/', '')
    def fresh(n):
        f = os.path.join(content, rel, n + '.uasset')
        return os.path.isfile(f) and os.path.getmtime(f) >= RUN_START[0] - 2
    missing = [n for n in names if not fresh(n)]
    log('on disk and written this run: %d / %d%s' % (len(names) - len(missing), len(names), (' MISSING ' + str(missing)) if missing else ''))
    return not missing


# ----------------------------------------------------------------------------- stats

def stats():
    rows = []
    paths = [FOLDER + '/MI_BerlinRuin_Wall', FOLDER + '/' + RP.HOLES_INSTANCE, FOLDER + '/MI_BerlinRuin_Rubble',
             FOLDER + '/MI_BerlinRuin_Rubble_Foot', MASTER, HOLES_MASTER] + DBK_COMPARE
    for path in paths:
        if not EAL.does_asset_exist(path):
            log('STATS %s missing' % path)
            continue
        asset = unreal.load_asset(path)
        s = MEL.get_statistics(asset)
        try:
            used = len(MEL.get_used_textures(asset))
        except Exception:
            used = -1
        row = '%s ps=%d vs=%d samplers=%d ps_tex=%d vt=%d interp=%d used_textures=%d' % (
            path.split('/')[-1], s.num_pixel_shader_instructions, s.num_vertex_shader_instructions, s.num_samplers,
            s.num_pixel_texture_samples, s.num_virtual_texture_samples, s.num_interpolator_scalars, used)
        rows.append(row)
        log('STATS ' + row)
    # Hole contract: same WallHole*/WallGrid* parameters as an existing library-built _WallHoles master.
    def hole_params(path):
        return sorted(str(n) for n in MEL.get_scalar_parameter_names(unreal.load_asset(path))
                      if str(n).startswith(('WallHole', 'WallGrid')))
    ours, ref = hole_params(HOLES_MASTER), hole_params(DBK_COMPARE[-1])
    row = 'HOLE_CONTRACT ours=%d ref=%d same=%s' % (len(ours), len(ref), set(ref) <= set(ours))
    rows.append(row)
    log('STATS ' + row + ' extra=' + str(sorted(set(ours) - set(ref))))
    # Baseline: an unsaved default-lit material with a constant base colour (what every opaque surface pays here).
    empty = unreal.new_object(unreal.Material, name='M_RuinKitBaseline')
    const = MEL.create_material_expression(empty, unreal.MaterialExpressionConstant3Vector, 0, 0)
    MEL.connect_material_property(const, '', PROP.MP_BASE_COLOR)
    MEL.recompile_material(empty)
    s = MEL.get_statistics(empty)
    row = 'BASELINE(empty default lit) ps=%d vs=%d samplers=%d ps_tex=%d' % (
        s.num_pixel_shader_instructions, s.num_vertex_shader_instructions, s.num_samplers, s.num_pixel_texture_samples)
    rows.append(row)
    log('STATS ' + row)
    with open(os.path.join(TEX_DIR, 'ruinkit_stats.txt'), 'w') as f:
        f.write('\n'.join(rows) + '\n')
    return rows


def run(mode=None):
    mode = mode or os.environ.get('RUIN_MODE', 'build')
    if mode in ('build', 'all'):
        import time
        RUN_START[0] = time.time()
        textures = import_textures()
        build_master(textures)
        build_instances()
        build_holes()
        ok = verify_on_disk()
        log('BUILD RESULT %s' % ('DONE' if ok else 'FAIL'))
    if mode in ('stats', 'all'):
        stats()


if __name__ == '__main__':
    run()
