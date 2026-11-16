"""Build the SandDrift material (NS_SandPush round 3, 2026-11-16) - headless commandlet, never the shared editor:

  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi

Imports T_SandDrift_Atlas / T_SandDrift_Detail (ore_sanddrift_atlas.py), builds M_SandDrift_Masked and
MI_SandDrift_A. Few large ground-lying sprites: each picks one of the 4 atlas cells with Particle Random (Niagara's
InitializeParticle writes Particles.MaterialRandom), draws where density * particle alpha (+ a little detail jitter)
beats Threshold - contiguous shapes, no grain noise - and gets its detail from a tileable normal/shade map scrolled
downwind (PanSpeed, in detail tiles a second). Masked, two-sided, default lit (tangent-space normal on the flat
sprite), decal response none, no dynamic parameters (AddWhite/Emessive overrides are not used; ColourGain and
EmissiveScale replace them).
MI switches for the wind axis if the in-game look shows it crossways: SwapUV (U <-> V), FlipU (head <-> tail).

Edits made afterwards in the main editor (2026-11-16, python + UECP; a rebuild with this script loses them):
  * WPO: Python's MaterialProperty has no WorldPositionOffset; wired over UECP connect_material_nodes(to_node=-1).
    WPO = (AbsoluteWorldPosition(excl. offsets) - ParticlePositionWS) * (SizeMult - 1) + (0,0,LiftCm): scales each
    quad about its particle (the emitter's size comes from a nested 200-500 random range the bridge cannot reach;
    InitializeParticle's Uniform Sprite Size is not what drives it) and lifts it off the ground.
  * Fade: DriftMask.PAlpha is fed by a 'DriftFade' Custom on ParticleRelativeTime (FadeIn 0.2 / FadeOut 0.35), not by
    particle colour alpha (the pack's per-particle alpha is tiny: its haze was many faint layers).
  * Colour: DriftColour uses a 'SandTint' vector (0.60, 0.49, 0.33) x ColourGain instead of the particle colour
    (Smoke A's particle colour renders near white). Under this map's sun an albedo near 0.3 matches the ground.
"""
import unreal

DST = '/Game/RTS_Survival/SpecialEffects/Dust/SandStreak'
TEX_SRC = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sandstreak/tex/'
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log('SANDDRIFT ' + msg)


def import_texture(name):
    task = unreal.AssetImportTask()
    task.filename = TEX_SRC + name + '.png'
    task.destination_path = DST + '/Textures'
    task.destination_name = name
    task.automated, task.replace_existing, task.save = True, True, False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    tex = unreal.load_asset(DST + '/Textures/' + name)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    EAL.save_loaded_asset(tex, False)
    log('texture %s %dx%d' % (name, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
    return tex


UVS = '''float2 uv = UV;
if (SwapUV > 0.5) uv = uv.yx;
if (FlipU > 0.5) uv.x = 1.0 - uv.x;
float mirror = (MirrorV > 0.5 && frac(Rand * 13.73) > 0.5) ? 1.0 : 0.0;
if (mirror > 0.5) uv.y = 1.0 - uv.y;
float idx = CellBase + min(floor(saturate(Rand) * CellCount), CellCount - 1.0);
float col = fmod(idx, Cells);
float row = floor(idx / Cells);
float2 atlasUV = (uv + float2(col, row)) / Cells;
float2 detailUV = uv * DetailTiling + float2(-Time * PanSpeed + Rand * 7.0, Rand * 3.0);
return float4(atlasUV, detailUV);'''

MASK = '''return 0.5 + D * saturate(PAlpha) + EdgeJitter * (DetailShade - 0.5) - Threshold;'''

COLOUR = '''float shade = 1.0 + ShadeContrast * ((BodyShade - 0.5) + (DetailShade - 0.5));
float core = lerp(0.92, 1.05, saturate(D * 1.5));
return PColor.rgb * ColourGain * shade * core;'''

NORMAL = '''float2 n = (N.xy * 2.0 - 1.0) * NormalStrength;
if (SwapUV > 0.5) n = n.yx;
if (FlipU > 0.5) n.x = -n.x;
return normalize(float3(n, 1.0));'''


class Graph:
    def __init__(self, mat):
        self.mat, self.y = mat, 0

    def node(self, cls, x=-900, **props):
        self.y += 120
        e = MEL.create_material_expression(self.mat, cls, x, self.y)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def scalar(self, name, value):
        return self.node(unreal.MaterialExpressionScalarParameter, -1500, parameter_name=name, default_value=value, group='SandDrift')

    def custom(self, desc, code, inputs, out_type):
        pins = []
        for n in inputs:
            pin = unreal.CustomInput()
            pin.set_editor_property('input_name', n)
            pins.append(pin)
        return self.node(unreal.MaterialExpressionCustom, -600, code=code, description=desc, output_type=out_type, inputs=pins)

    def link(self, a, ao, b, bi):
        if not MEL.connect_material_expressions(a, ao, b, bi):
            log('LINK FAILED %s.%s -> %s.%s' % (a.get_name(), ao, b.get_name(), bi))


def build_material(atlas, detail):
    path = DST + '/M_SandDrift_Masked'
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_SandDrift_Masked', DST, unreal.Material, unreal.MaterialFactoryNew())
    for prop, value in (('blend_mode', unreal.BlendMode.BLEND_MASKED), ('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT),
                        ('two_sided', True), ('opacity_mask_clip_value', 0.5), ('dithered_lod_transition', False),
                        ('material_decal_response', unreal.MaterialDecalResponse.MDR_NONE),
                        ('used_with_niagara_sprites', True), ('used_with_niagara_mesh_particles', True)):
        mat.set_editor_property(prop, value)
    g = Graph(mat)
    uv = g.node(unreal.MaterialExpressionTextureCoordinate, -1500)
    rand = g.node(unreal.MaterialExpressionParticleRandom, -1500)
    time = g.node(unreal.MaterialExpressionTime, -1500)
    pcol = g.node(unreal.MaterialExpressionParticleColor, -1500)
    p = {name: g.scalar(name, v) for name, v in (
        ('Cells', 2.0), ('CellBase', 0.0), ('CellCount', 4.0), ('SwapUV', 0.0), ('FlipU', 0.0), ('MirrorV', 1.0),
        ('DetailTiling', 3.0), ('PanSpeed', 0.08), ('Threshold', 0.3), ('EdgeJitter', 0.12), ('ShadeContrast', 0.35),
        ('ColourGain', 0.7), ('EmissiveScale', 0.25), ('NormalStrength', 0.6), ('LiftCm', 70.0))}

    uvs = g.custom('DriftUVs', UVS, ['UV', 'Rand', 'Time', 'Cells', 'CellBase', 'CellCount', 'SwapUV', 'FlipU', 'MirrorV',
                                     'DetailTiling', 'PanSpeed'], unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    g.link(uv, '', uvs, 'UV')
    g.link(rand, '', uvs, 'Rand')
    g.link(time, '', uvs, 'Time')
    for name in ('Cells', 'CellBase', 'CellCount', 'SwapUV', 'FlipU', 'MirrorV', 'DetailTiling', 'PanSpeed'):
        g.link(p[name], '', uvs, name)
    atlas_uv = g.node(unreal.MaterialExpressionComponentMask, -800, r=True, g=True, b=False, a=False)
    detail_uv = g.node(unreal.MaterialExpressionComponentMask, -800, r=False, g=False, b=True, a=True)
    g.link(uvs, '', atlas_uv, '')
    g.link(uvs, '', detail_uv, '')
    at = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -700, parameter_name='AtlasTX', texture=atlas,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    dt = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -700, parameter_name='DetailTX', texture=detail,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    g.link(atlas_uv, '', at, 'UVs')
    g.link(detail_uv, '', dt, 'UVs')

    mask = g.custom('DriftMask', MASK, ['D', 'PAlpha', 'DetailShade', 'EdgeJitter', 'Threshold'], unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    g.link(at, 'R', mask, 'D')
    g.link(pcol, 'A', mask, 'PAlpha')
    g.link(dt, 'B', mask, 'DetailShade')
    g.link(p['EdgeJitter'], '', mask, 'EdgeJitter')
    g.link(p['Threshold'], '', mask, 'Threshold')

    colour = g.custom('DriftColour', COLOUR, ['PColor', 'BodyShade', 'DetailShade', 'D', 'ColourGain', 'ShadeContrast'],
                      unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.link(pcol, '', colour, 'PColor')
    g.link(at, 'G', colour, 'BodyShade')
    g.link(dt, 'B', colour, 'DetailShade')
    g.link(at, 'R', colour, 'D')
    g.link(p['ColourGain'], '', colour, 'ColourGain')
    g.link(p['ShadeContrast'], '', colour, 'ShadeContrast')

    normal = g.custom('DriftNormal', NORMAL, ['N', 'NormalStrength', 'SwapUV', 'FlipU'], unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    g.link(dt, '', normal, 'N')
    g.link(p['NormalStrength'], '', normal, 'NormalStrength')
    g.link(p['SwapUV'], '', normal, 'SwapUV')
    g.link(p['FlipU'], '', normal, 'FlipU')

    emis = g.node(unreal.MaterialExpressionMultiply, -300)
    g.link(colour, '', emis, 'A')
    g.link(p['EmissiveScale'], '', emis, 'B')
    rough = g.node(unreal.MaterialExpressionConstant, -300, r=1.0)
    spec = g.node(unreal.MaterialExpressionConstant, -300, r=0.2)
    # Lift: the actors sit ~10 cm over the ground and the particles spawn near that height, so flat 13-26 m sprites
    # vanish into the landscape's undulations (2026-11-16 look check: only slivers). WPO raises them by LiftCm.
    up = g.node(unreal.MaterialExpressionConstant3Vector, -500, constant=unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    lift = g.node(unreal.MaterialExpressionMultiply, -300)
    g.link(up, '', lift, 'A')
    g.link(p['LiftCm'], '', lift, 'B')
    MP = unreal.MaterialProperty
    outputs = [(colour, 'MP_BASE_COLOR', 'BaseColor'), (emis, 'MP_EMISSIVE_COLOR', 'EmissiveColor'),
               (mask, 'MP_OPACITY_MASK', 'OpacityMask'), (normal, 'MP_NORMAL', 'Normal'), (rough, 'MP_ROUGHNESS', 'Roughness'),
               (spec, 'MP_SPECULAR', 'Specular'), (lift, 'MP_WORLD_POSITION_OFFSET', 'WorldPositionOffset')]
    for expr, name, _ in outputs:
        if hasattr(MP, name):
            if not MEL.connect_material_property(expr, '', getattr(MP, name)):
                log('PROPERTY LINK FAILED ' + name)
        else:
            # Python's MaterialProperty has no MP_WORLD_POSITION_OFFSET: wire it afterwards over UECP,
            # connect_material_nodes(from_node=<index of the lift Multiply>, from_output='', to_node=-1,
            # to_input='WorldPositionOffset') on the saved material, then validate + save.
            log('NOT WIRED %s (do it over UECP)' % name)
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat, False)
    log('material nodes %d' % MEL.get_num_material_expressions(mat))
    return mat


def build_instance(mat):
    name, path = 'MI_SandDrift_A', DST + '/MI_SandDrift_A'
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, DST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, mat)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    log('instance ' + name)


try:
    atlas = unreal.load_asset(DST + '/Textures/T_SandDrift_Atlas') or import_texture('T_SandDrift_Atlas')
    detail = unreal.load_asset(DST + '/Textures/T_SandDrift_Detail') or import_texture('T_SandDrift_Detail')
    build_instance(build_material(atlas, detail))
    log('done')
except Exception as exc:
    log('ERROR ' + repr(exc))
    raise
