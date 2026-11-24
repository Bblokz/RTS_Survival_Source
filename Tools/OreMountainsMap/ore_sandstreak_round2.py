import unreal
# SandStreak masked rework, round 2 (2026-11-16), run in the main editor through UECP execute_python (inline code,
# never `py file`). After the first A/B: grains were 10-30 px blobs that boiled frame to frame, coverage ~0.49 in the
# core against ~0.23 effective for the old haze, and the sand rendered near white. Adds a tiling grain-noise sampler
# (GrainTX at TexCoord * NoiseTiling, lerped in for the flipbook's B noise by UseGrainNoise) to M_SandStreak_Masked
# without deleting anything (the master is in use), and retunes the MIs.
MEL = unreal.MaterialEditingLibrary
DST = '/Game/RTS_Survival/SpecialEffects/Dust/SandStreak'
M = DST + '/M_SandStreak_Masked'
P = M + '.M_SandStreak_Masked:'
out = []

task = unreal.AssetImportTask()
task.filename = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sandstreak/tex/T_SandStreak_GrainNoise.png'
task.destination_path = DST + '/Textures'
task.destination_name = 'T_SandStreak_GrainNoise'
task.automated, task.replace_existing, task.save = True, True, False
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
noise = unreal.load_asset(DST + '/Textures/T_SandStreak_GrainNoise')
noise.set_editor_property('srgb', False)
noise.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
noise.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
noise.set_editor_property('do_scale_mips_for_alpha_coverage', True)
noise.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0.5, 0.0, 0.0, 0.0))
out.append('noise saved %s' % unreal.EditorAssetLibrary.save_loaded_asset(noise, False))

mat = unreal.load_asset(M)
tex = unreal.find_object(None, P + 'MaterialExpressionTextureSampleParameter2D_0')
mask = unreal.find_object(None, P + 'MaterialExpressionCustom_1')
colour = unreal.find_object(None, P + 'MaterialExpressionCustom_2')
uv = unreal.find_object(None, P + 'MaterialExpressionTextureCoordinate_0')
out.append('found %s' % [bool(x) for x in (tex, mask, colour, uv)])


def node(cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, ao, b, bi):
    ok = MEL.connect_material_expressions(a, ao, b, bi)
    out.append('%s %s.%s -> %s.%s' % ('ok' if ok else 'FAILED', a.get_name(), ao, b.get_name(), bi))


tiling = node(unreal.MaterialExpressionScalarParameter, -1700, 1700, parameter_name='NoiseTiling', default_value=3.0, group='SandStreak')
use = node(unreal.MaterialExpressionScalarParameter, -1700, 1850, parameter_name='UseGrainNoise', default_value=1.0, group='SandStreak')
mul = node(unreal.MaterialExpressionMultiply, -1500, 1700)
grain = node(unreal.MaterialExpressionTextureSampleParameter2D, -1300, 1700, parameter_name='GrainTX', texture=noise,
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
lerp = node(unreal.MaterialExpressionLinearInterpolate, -1050, 1700)
rg = node(unreal.MaterialExpressionComponentMask, -1050, 1550, r=True, g=True, b=False, a=False)
app3 = node(unreal.MaterialExpressionAppendVector, -900, 1600)
app4 = node(unreal.MaterialExpressionAppendVector, -750, 1600)
link(uv, '', mul, 'A')
link(tiling, '', mul, 'B')
link(mul, '', grain, 'UVs')
link(tex, 'B', lerp, 'A')
link(grain, 'R', lerp, 'B')
link(use, '', lerp, 'Alpha')
link(tex, 'RGB', rg, '')
link(rg, '', app3, 'A')
link(lerp, '', app3, 'B')
link(app3, '', app4, 'A')
link(tex, 'A', app4, 'B')
link(app4, '', mask, 'T')
link(app4, '', colour, 'T')
MEL.recompile_material(mat)
out.append('material saved %s' % unreal.EditorAssetLibrary.save_loaded_asset(mat, False))

TUNE = {
    'MI_SandStreak_WaveA': {'Boost': 2.0, 'NoiseTiling': 0.3},
    'MI_SandStreak_WaveB': {'Boost': 2.0, 'NoiseTiling': 0.3},
    'MI_SandStreak_PuffA': {'Boost': 2.5, 'NoiseTiling': 3.0},
    'MI_SandStreak_PuffB': {'Boost': 3.5, 'NoiseTiling': 3.0},
    'MI_SandStreak_Grain': {'UseGrainNoise': 0.0},
}
for name, params in TUNE.items():
    mi = unreal.load_asset(DST + '/' + name)
    params = dict(params, ColourGain=0.7, EmissiveScale=0.3)
    for key, value in params.items():
        MEL.set_material_instance_scalar_parameter_value(mi, key, value)
    MEL.update_material_instance(mi)
    out.append('%s saved %s' % (name, unreal.EditorAssetLibrary.save_loaded_asset(mi, False)))
print('\n'.join(out))
