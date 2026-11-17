"""PBR bark for the Oder-Warthe dead pines: detail textures + M_OW_PineBark master, then MI_OW_PineBark onto it.

Headless (new assets only; nothing that a map editor holds):
    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi -nosplash
        -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding        (env OW_BARK_MODE=build, the default)
Inside an editor (exec the file with OW_BARK_MODE set in globals or env):
    build   import T_OW_BarkPlates_N/_M + T_OW_DeadWood_N/_M (ow_bark_textures.py), (re)build M_OW_PineBark
    apply   reparent MI_OW_PineBark (used by SM_OW_DeadPine_01..05) to M_OW_PineBark and save it. Run this in the
            process that has Ger_OderWarthe loaded, or with no editor holding it, or the save fails (file lock).

M_OW_PineBark (opaque, ISM-ready, no WPO):
  colour   BaseTex strip (UV0, unchanged T_OW_PineBark_D zones) * Tint * cavity(AO) * plate height * per-tree tone,
           lichen patches (mask alpha, facing LichenDir, none on the char) blended over it
  detail   UV0 * (DetailU, DetailV): DetailV = 21 makes 1 tile per metre up the trunk; the branch band (1-V > 0.93)
           switches from plate bark to dead wood (grain along the branch)
  normal   lerp(plates, wood) scaled per zone (char / lower bark / fox bark / wood), flattened under lichen
  rough    lerp(RoughMin, RoughMax, mask R), lichen matte; AO from mask G
Every look value is a scalar/vector parameter, so tuning happens on MI_OW_PineBark without a rebuild.
"""
import json
import os

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Textures/'
KIT = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit'
MASTER = KIT + '/Materials/M_OW_PineBark'
MI = KIT + '/Materials/MI_OW_PineBark'
STRIP = KIT + '/Textures/T_OW_PineBark_D'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/assets/bark_material.json'

_EAL = unreal.EditorAssetLibrary
_MEL = unreal.MaterialEditingLibrary
_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
OUT = {'log': []}


def log(*a):
    line = ' '.join(str(x) for x in a)
    OUT['log'].append(line)
    print('OWBARK ' + line)
    unreal.log_warning('OWBARK ' + line)


def save(asset, what):
    ok = _EAL.save_loaded_asset(asset, False)
    log('save', what, ok)
    if not ok:
        OUT.setdefault('failed_saves', []).append(what)
    return ok


# ------------------------------------------------------------------------------------------------------ textures
def import_texture(name, normal):
    task = unreal.AssetImportTask()
    task.filename = SRC + name + '.png'
    task.destination_path = KIT + '/Textures'
    task.destination_name = name
    task.automated, task.save, task.replace_existing = True, False, True
    _TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(KIT + '/Textures/' + name)
    if tex is None:
        raise RuntimeError('texture import failed ' + name)
    tex.set_editor_property('srgb', False)
    if normal:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        tex.set_editor_property('flip_green_channel', False)
    else:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    save(tex, name)
    return tex


# ------------------------------------------------------------------------------------------------------ material
def build_master(tex):
    if _EAL.does_asset_exist(MASTER):
        m = unreal.load_asset(MASTER)
        for _ in range(8):
            if _MEL.get_num_material_expressions(m) == 0:
                break
            _MEL.delete_all_material_expressions(m)
        log('master cleared', _MEL.get_num_material_expressions(m))
    else:
        m = _TOOLS.create_asset('M_OW_PineBark', KIT + '/Materials', unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)

    def node(cls, **props):
        n = _MEL.create_material_expression(m, cls)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, ao, b, bi):
        if not _MEL.connect_material_expressions(a, ao, b, bi):
            raise RuntimeError('link failed %s.%s -> %s.%s' % (a.get_name(), ao, b.get_name(), bi))

    def scalar(name, v, group='Bark'):
        return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v, group=group)

    def vec(name, rgb, group='Bark'):
        return node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                    default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0), group=group)

    def op(cls, a, b, ao='', bo=''):
        n = node(cls)
        link(a, ao, n, 'A')
        link(b, bo, n, 'B')
        return n

    def mul(a, b, ao='', bo=''):
        return op(unreal.MaterialExpressionMultiply, a, b, ao, bo)

    def add(a, b, ao='', bo=''):
        return op(unreal.MaterialExpressionAdd, a, b, ao, bo)

    def lerp(a, b, t, ao='', bo='', to=''):
        n = node(unreal.MaterialExpressionLinearInterpolate)
        link(a, ao, n, 'A')
        link(b, bo, n, 'B')
        link(t, to, n, 'Alpha')
        return n

    def const(v):
        return node(unreal.MaterialExpressionConstant, r=v)

    def mask(a, r=False, g=False, b=False, al=False, ao=''):
        n = node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=al)
        link(a, ao, n, '')
        return n

    def smooth(lo, hi, x, xo=''):
        n = node(unreal.MaterialExpressionSmoothStep, const_min=lo, const_max=hi)
        link(x, xo, n, 'Value')
        return n

    def node_one_minus(a, ao=''):
        n = node(unreal.MaterialExpressionOneMinus)
        link(a, ao, n, '')
        return n

    def sat(a, ao=''):
        n = node(unreal.MaterialExpressionSaturate)
        link(a, ao, n, '')
        return n

    def sample(name, t, uvs, sampler):
        n = node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, texture=t, sampler_type=sampler,
                 group='Textures')
        link(uvs, '', n, 'UVs')
        return n

    # ---- UVs and zones (height h = 1 - UV0.y: 0 = ground, 0.92 = trunk top, > 0.93 = branches / twigs / stubs)
    uv = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    # UE flips V on FBX import (and the strip PNG is written bottom-up), so in the engine UV0.y = 1 - height
    v = node_one_minus(mask(uv, g=True))
    det = node(unreal.MaterialExpressionAppendVector)
    link(scalar('DetailU', 1.0, 'Detail'), '', det, 'A')
    link(scalar('DetailV', 21.0, 'Detail'), '', det, 'B')
    duv = mul(uv, det)
    wood = smooth(0.925, 0.935, v)                     # 1 on branches
    fox = smooth(0.30, 0.48, v)                        # 1 on the upper fox bark
    charz = smooth(0.10, 0.14, v)                      # 0 on the charred base, 1 above
    char = node(unreal.MaterialExpressionOneMinus)
    link(charz, '', char, '')

    # ---- detail samples
    pn = sample('PlatesNormal', tex['T_OW_BarkPlates_N'], duv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    pm = sample('PlatesMask', tex['T_OW_BarkPlates_M'], duv, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    wuv = mul(duv, scalar('WoodScale', 1.0, 'Detail'))
    wn = sample('WoodNormal', tex['T_OW_DeadWood_N'], wuv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    wm = sample('WoodMask', tex['T_OW_DeadWood_M'], wuv, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    nrm = lerp(pn, wn, wood, 'RGB', 'RGB')
    msk = lerp(pm, wm, wood, 'RGBA', 'RGBA')
    m_r = mask(msk, r=True)
    m_g = mask(msk, g=True)
    m_b = mask(msk, b=True)
    m_a = mask(msk, al=True)

    # ---- lichen: mask alpha * amount * facing, none on the char
    lv = vec('LichenDir', (-0.55, 0.65, 0.52), 'Lichen')
    vn = node(unreal.MaterialExpressionVertexNormalWS)
    facing = sat(op(unreal.MaterialExpressionDotProduct, vn, lv))
    face_w = lerp(scalar('LichenBackside', 0.25, 'Lichen'), const(1.0), facing)
    lich = sat(mul(mul(mul(m_a, scalar('LichenAmount', 0.6, 'Lichen')), face_w), charz))
    lich = mul(lich, smooth(0.45, 0.8, m_b))           # crusts sit on the plate tops, not in the fissures

    # ---- normal: per-zone strength, flattened under lichen
    s_trunk = lerp(scalar('NormalLowerBark', 1.0, 'Detail'), scalar('NormalFoxBark', 0.35, 'Detail'), fox)
    s_trunk = lerp(s_trunk, scalar('NormalChar', 0.8, 'Detail'), char)
    s = lerp(s_trunk, scalar('NormalWood', 1.4, 'Detail'), wood)
    s = mul(s, lerp(const(1.0), const(0.45), lich))
    nxy = mul(mask(nrm, r=True, g=True), s)
    nfin = node(unreal.MaterialExpressionAppendVector)
    link(nxy, '', nfin, 'A')
    link(mask(nrm, b=True), '', nfin, 'B')

    # ---- base colour
    strip = node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name='BaseTex',
                 texture=unreal.load_asset(STRIP), group='Textures')
    link(uv, '', strip, 'UVs')
    col = mul(strip, vec('Tint', (1.0, 1.0, 1.0)), 'RGB', '')
    cav = lerp(const(1.0), m_g, scalar('CavityStrength', 0.85, 'Detail'))
    hgt = lerp(scalar('HeightDark', 0.5, 'Detail'), scalar('HeightBright', 1.22, 'Detail'), m_b)
    # thin flaky fox bark up the trunk: plates read as scales there, so fade their shading (not on the wood band)
    fade = mul(mul(fox, scalar('FoxDetailFade', 0.65, 'Detail')), node_one_minus(wood))
    shade = lerp(mul(cav, hgt), const(1.0), fade)
    col = mul(col, shade)
    # per-tree tone from the actor position (trees stand 6-7 m apart)
    ap = mask(node(unreal.MaterialExpressionActorPositionWS), r=True, g=True)
    hsh = node(unreal.MaterialExpressionFrac)
    k = node(unreal.MaterialExpressionConstant2Vector, r=0.0137, g=0.0071)
    link(op(unreal.MaterialExpressionDotProduct, ap, k), '', hsh, '')
    tone = lerp(scalar('TreeToneDark', 0.84), scalar('TreeToneBright', 1.08), hsh)
    col = mul(col, tone)
    lcol = mul(vec('LichenColor', (0.24, 0.255, 0.2), 'Lichen'), lerp(const(0.75), const(1.15), m_b))
    col = lerp(col, lcol, lich)
    _MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)

    # ---- roughness / AO / spec
    rough = lerp(scalar('RoughMin', 0.62), scalar('RoughMax', 1.0), m_r)
    rough = lerp(rough, scalar('LichenRoughness', 0.95, 'Lichen'), lich)
    _MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    _MEL.connect_material_property(lerp(const(1.0), m_g, scalar('AOStrength', 1.0, 'Detail')), '',
                                   unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    _MEL.connect_material_property(scalar('Specular', 0.35), '', unreal.MaterialProperty.MP_SPECULAR)
    _MEL.connect_material_property(nfin, '', unreal.MaterialProperty.MP_NORMAL)
    _MEL.layout_material_expressions(m)
    _MEL.recompile_material(m)
    log('master built', MASTER, 'expressions', _MEL.get_num_material_expressions(m))
    save(m, 'M_OW_PineBark')
    return m


def build():
    tex = {}
    for name, normal in (('T_OW_BarkPlates_N', True), ('T_OW_BarkPlates_M', False),
                         ('T_OW_DeadWood_N', True), ('T_OW_DeadWood_M', False)):
        tex[name] = import_texture(name, normal)
    build_master(tex)


def apply():
    m = unreal.load_asset(MASTER)
    mi = unreal.load_asset(MI)
    old = mi.get_editor_property('parent')
    tint = _MEL.get_material_instance_vector_parameter_value(mi, 'Tint')
    _MEL.set_material_instance_parent(mi, m)
    _MEL.clear_all_material_instance_parameters(mi)
    _MEL.set_material_instance_texture_parameter_value(mi, 'BaseTex', unreal.load_asset(STRIP))
    _MEL.set_material_instance_vector_parameter_value(mi, 'Tint', tint)
    _MEL.update_material_instance(mi)
    log('MI reparented', old.get_path_name() if old else None, '->', MASTER, 'tint', tint)
    save(mi, 'MI_OW_PineBark')


def main():
    mode = globals().get('OW_BARK_MODE') or os.environ.get('OW_BARK_MODE', 'build')
    log('mode', mode)
    if mode in ('build', 'all'):
        build()
    if mode in ('apply', 'all'):
        apply()
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w', encoding='utf-8') as f:
        json.dump(OUT, f, indent=1)
    log('DONE', 'failed saves:', OUT.get('failed_saves', []))


main()
