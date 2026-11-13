"""Builds the Berlin ruin kit material set (idempotent). Docs: Source/Docs/Environment/BerlinRuinKitMaterial.md

Creates / refreshes in /Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/:
  T_BerlinRuin_Plaster_D, _Brick_D, _N, _Rubble_D   (from Saved/Berlin/ruinkit_textures/, ruin_textures_gen.py)
  M_BerlinRuin                opaque master (graph rebuilt in place, so instances and mesh slots stay valid)
  MI_BerlinRuin_Wall          Ruin_IsRubble off
  MI_BerlinRuin_Rubble        Ruin_IsRubble on
  M_BerlinRuin_WallHoles      masked copy made by Tools/WallChunks/make_wall_holes_material.masked_master_for
                              (deleted and re-made each run so it never goes stale against M_BerlinRuin)
  MI_BerlinRuin_Wall_Holes    make_holes_instance(MI_BerlinRuin_Wall, NoGrid, two-sided, inner tint)

Headless (never in the shared editor):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
  stats only (needs rendering):  set RUIN_MODE=stats and add -AllowCommandletRendering
Modes (env RUIN_MODE): build (default) | stats | all
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

    def switch(self, when_rubble, when_wall):
        n = self.node(unreal.MaterialExpressionStaticSwitchParameter, parameter_name=RP.STATIC_SWITCH,
                      default_value=False, group='BerlinRuin')
        self._in(n, 'True', when_rubble)
        self._in(n, 'False', when_wall)
        return n

    def texture(self, tex, uv, linear=False):
        sampler = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if linear else \
            unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
        n = self.node(unreal.MaterialExpressionTextureSample, texture=tex, sampler_type=sampler)
        self.link(uv, '', n, 'UVs')
        return n


# ----------------------------------------------------------------------------- master

def _customized_uv0():
    names = [n for n in dir(PROP) if 'CUSTOMIZED' in n.upper() and n.endswith('0')]
    if not names:
        raise RuntimeError('no customized UV property in ' + str([n for n in dir(PROP) if n.startswith('MP_')]))
    return getattr(PROP, names[0])


def build_master(textures):
    if EAL.does_asset_exist(MASTER):
        material = unreal.load_asset(MASTER)
        for _ in range(30):   # delete_all_material_expressions only removes about half per call
            left = MEL.get_num_material_expressions(material)
            if left == 0:
                break
            log('clearing %d expressions' % left)
            MEL.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_BerlinRuin', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property('two_sided', False)
    material.set_editor_property('used_with_static_lighting', True)
    g = Graph(material)

    # UVs in the vertex shader: UV0 (1 unit = 4 m) x the tile scalar picked by the rubble switch.
    uv0 = g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    tile = g.switch(g.scalar('RuinRubbleTile'), g.scalar('RuinWallTile'))
    uv_vs = g.mul(uv0, tile)
    # Customized UVs are not reachable from Python; a vertex interpolator moves the multiply to the VS the same way.
    uv = g.node(unreal.MaterialExpressionVertexInterpolator)
    g.link(uv_vs, '', uv, '')
    vc = g.node(unreal.MaterialExpressionVertexColor)

    # ---- wall: plaster / brick by vertex G thresholded against the plaster chip height (Plaster_D.A).
    plas = g.texture(textures['T_BerlinRuin_Plaster_D'], uv)
    brick = g.texture(textures['T_BerlinRuin_Brick_D'], uv)
    nrm = g.texture(textures['T_BerlinRuin_N'], uv, linear=True)
    chip = (plas, 'A')
    cav = (brick, 'A')
    width = g.scalar('RuinPlasterEdgeWidth')
    loss = g.add((vc, 'G'), g.scalar('RuinPlasterLoss'))
    # t = loss * (1 + 2w) - w ; d = (chip - t) / w ; plaster = saturate(d)
    threshold = g.sub(g.mul(loss, g.add(g.mul(width, 2.0), 1.0)), width)
    d = g.div(g.sub(chip, threshold), width)
    plaster = g.sat(d)
    edge = g.mul(g.sat(g.sub(2.0, d)), plaster)                 # bright broken rim, 1..2 widths inside
    shadow = g.mul(g.sat(g.add(g.mul(d, 0.5), 1.0)), g.unary(unreal.MaterialExpressionOneMinus, plaster))
    plaster_col = g.mul(g.mul((plas, 'RGB'), g.vector_rgb('RuinPlasterColor')),
                        g.add(g.mul(edge, g.scalar('RuinPlasterEdgeLight')), 1.0))
    brick_col = g.mul(g.mul((brick, 'RGB'), g.vector_rgb('RuinBrickColor')),
                      g.sub(1.0, g.mul(shadow, g.scalar('RuinPlasterEdgeShadow'))))
    wall_base = g.lerp(brick_col, plaster_col, plaster)
    wall_rough = g.lerp(g.lerp(g.scalar('RuinBrickRoughJoint'), g.scalar('RuinBrickRoughFace'), cav),
                        g.scalar('RuinPlasterRoughness'), plaster)
    nxy = g.lerp(g.mask((nrm, 'RGBA'), 'rg'), g.mask((nrm, 'RGBA'), 'ba'), plaster)
    nxy = g.mul(g.add(g.mul(nxy, 2.0), -1.0), g.scalar('RuinNormalStrength'))
    wall_normal = g.node(unreal.MaterialExpressionAppendVector)
    g.link(nxy, '', wall_normal, 'A')
    one = g.node(unreal.MaterialExpressionConstant, r=1.0)
    g.link(one, '', wall_normal, 'B')                            # the shader normalises the tangent normal
    wall_noise = g.lerp(cav, chip, plaster)

    # ---- rubble: one texture, no blend, flat normal.
    rub = g.texture(textures['T_BerlinRuin_Rubble_D'], uv)
    rub_base = g.mul((rub, 'RGB'), g.vector_rgb('RuinRubbleColor'))
    flat = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 1, 0))

    base0 = g.switch(rub_base, wall_base)
    rough0 = g.switch(g.scalar('RuinRubbleRoughness'), wall_rough)
    noise = g.switch((rub, 'A'), wall_noise)
    normal = g.switch(flat, wall_normal)

    # ---- per actor: CPD 0-3 = tint RGB + soot amount. Unset CPD reads 0 -> parameter fallbacks.
    cpd = g.node(unreal.MaterialExpressionVectorParameter, parameter_name='RuinCPD_TintSoot',
                 default_value=unreal.LinearColor(1, 1, 1, 1), use_custom_primitive_data=True,
                 primitive_data_index=RP.CPD_TINT_FIRST, group='BerlinRuin')
    cpd_rgb = g.mask(cpd, 'rgb')
    tint_set = g.sat(g.mul(g.add(g.add((cpd, 'R'), (cpd, 'G')), (cpd, 'B')), 1.0e6))
    tint = g.lerp(g.vector_rgb('RuinTintFallback'), cpd_rgb, tint_set)
    soot_set = g.sat(g.mul(g.unary(unreal.MaterialExpressionAbs, (cpd, 'A')), 1.0e6))
    soot_amount = g.lerp(g.scalar('RuinSootAmountFallback'), g.max((cpd, 'A'), 0.0), soot_set)

    base1 = g.mul(base0, tint)
    soot_k = g.add(g.scalar('RuinSootStrength'), g.mul(g.scalar('RuinSootBreakup'), g.sub(noise, 0.5)))
    soot = g.sat(g.mul(g.mul((vc, 'R'), soot_amount), soot_k))
    base2 = g.lerp(base1, g.mul(base1, g.vector_rgb('RuinSootColor')), soot)
    dust_k = g.add(g.scalar('RuinDustStrength'), g.mul(g.scalar('RuinDustBreakup'), g.sub(0.5, noise)))
    dust = g.mul(g.sat(g.mul((vc, 'B'), dust_k)), g.scalar('RuinDustOpacity'))
    base3 = g.lerp(base2, g.vector_rgb('RuinDustColor'), dust)
    rough = g.lerp(rough0, g.scalar('RuinMatteRoughness'), g.max(soot, dust))

    for node, prop in ((base3, PROP.MP_BASE_COLOR), (rough, PROP.MP_ROUGHNESS), (normal, PROP.MP_NORMAL)):
        if not MEL.connect_material_property(node, '', prop):
            g.failed += 1
    MEL.layout_material_expressions(material)
    MEL.recompile_material(material)
    _save(material)
    log('master built, %d expressions, %d failed links' % (MEL.get_num_material_expressions(material), g.failed))
    if g.failed:
        raise RuntimeError('M_BerlinRuin: %d failed links' % g.failed)
    return material


# ----------------------------------------------------------------------------- instances

def _apply(mi, scalars, vectors, rubble):
    MEL.set_material_instance_static_switch_parameter_value(mi, RP.STATIC_SWITCH, rubble)
    for name, value in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, name, float(value))
    for name, (r, g, b) in vectors.items():
        MEL.set_material_instance_vector_parameter_value(mi, name, unreal.LinearColor(r, g, b, 1.0))
    MEL.update_material_instance(mi)


def build_instances(master):
    out = {}
    for name, (scalars, vectors, rubble) in RP.INSTANCES.items():
        path = FOLDER + '/' + name
        if EAL.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        _apply(mi, scalars, vectors, rubble)
        _save(mi)
        out[name] = mi
    return out


def build_holes():
    """The masked hole variant, exactly by the shared library (never edited afterwards)."""
    holes_path = FOLDER + '/' + RP.HOLES_INSTANCE
    if EAL.does_asset_exist(HOLES_MASTER):
        if EAL.delete_asset(HOLES_MASTER):
            log('holes: deleted stale M_BerlinRuin_WallHoles, instance kept')
        else:
            log('holes: FALLBACK, deleting MI_BerlinRuin_Wall_Holes too (Blueprint references to it break)')
            # Fall back: the holes instance pins it; drop that too (it is re-made below).
            EAL.delete_asset(holes_path)
            if not EAL.delete_asset(HOLES_MASTER):
                raise RuntimeError('could not delete stale ' + HOLES_MASTER)
    mi = WH.make_holes_instance(FOLDER + '/MI_BerlinRuin_Wall', FOLDER, unreal.load_asset(PLACEHOLDER_MESH),
                                grid_axis=None, two_sided=True, name=RP.HOLES_INSTANCE, inner_tint=True,
                                inner_darken=RP.HOLES_INNER_DARKEN, inner_color=RP.HOLES_INNER_COLOR)
    scalars, vectors, rubble = RP.INSTANCES['MI_BerlinRuin_Wall']
    _apply(mi, scalars, vectors, rubble)
    _save(mi)
    master = unreal.load_asset(HOLES_MASTER)
    log('holes master %s blend=%s, instance parent=%s two_sided=%s' % (
        master.get_name(), master.get_editor_property('blend_mode'),
        mi.get_editor_property('parent').get_name(),
        mi.get_editor_property('base_property_overrides').get_editor_property('two_sided')))
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
             MASTER, HOLES_MASTER] + DBK_COMPARE
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
        master = build_master(textures)
        build_instances(master)
        build_holes()
        ok = verify_on_disk()
        log('BUILD RESULT %s' % ('DONE' if ok else 'FAIL'))
    if mode in ('stats', 'all'):
        stats()


if __name__ == '__main__':
    run()
