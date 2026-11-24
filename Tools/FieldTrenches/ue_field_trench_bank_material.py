"""Builds M_FieldTrench_Bank, the earth bank of the field trench kits (both factions; slot FieldTrench_Bank).

The bank takes its colour from the landscape under it: base colour (and roughness) from the RVT_Environment runtime
virtual texture, sampled in the pixel shader, so the foot of the bank disappears into the ground. The mesh keeps its
own normal (the slope is what shows the bank). Vertex colour R from the Blender generator (1 on the crest, 0 down the
slope) blends in darker, fresher spoil next to the bags, broken up by T_RTS_PackedNoise. A map without an
RVT_Environment volume samples black; the material then falls back to FallbackColor.

Run in the editor: execute_python with
    import sys; sys.path.append('D:/UE5Projects/RTS_Survival/Source/Tools/FieldTrenches')
    import ue_field_trench_bank_material as m; m.build()
"""
import sys

import unreal

sys.path.append('D:/UE5Projects/RTS_Survival/Source/Docs/Landscape')
import ls_graph as g  # noqa: E402

FOLDER = '/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Materials'
NAME = 'M_FieldTrench_Bank'
PATH = FOLDER + '/' + NAME
RVT = '/Game/RTS_Survival/Meshes/RVTs/RVT_Environment'
NOISE = '/Game/RTS_Survival/Textures/Noise/T_RTS_PackedNoise'
GROUP = 'Bank'
mel = unreal.MaterialEditingLibrary


def _material():
    if not unreal.EditorAssetLibrary.does_asset_exist(PATH):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Material,
                                                                unreal.MaterialFactoryNew())
    material = unreal.load_asset(PATH)
    # delete_all_material_expressions removes only every other node: delete until none are left.
    for _ in range(20):
        expressions = g.live_expressions(material)
        if not expressions:
            break
        for expression in expressions:
            mel.delete_material_expression(material, expression)
        unreal.SystemLibrary.collect_garbage()
    return material


def build():
    material = _material()
    g.failures.clear()
    g.begin(material, desc='Bank', group=GROUP)

    # Landscape colour from the RVT, or the fallback where the map has no RVT_Environment volume.
    rvt = g.N('RuntimeVirtualTextureSample', -1400, -200, virtual_texture=unreal.load_asset(RVT),
              material_type=unreal.RuntimeVirtualTextureMaterialType.BASE_COLOR_NORMAL_SPECULAR)
    brightness = g.N('DotProduct', -1100, -320)
    g.C(rvt, 'BaseColor', brightness, 'A')
    g.C(g.N('Constant3Vector', -1250, -260, constant=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)), '', brightness, 'B')
    has_rvt = g.sat(g.mulc(brightness, '', 400.0, -950, -320), '', -820, -320)
    fallback = g.V('FallbackColor', unreal.LinearColor(0.30, 0.24, 0.17, 1.0), -1100, -120)
    ground = g.lerp(fallback, '', rvt, 'BaseColor', has_rvt, '', -650, -200)

    # Fresh spoil next to the bags: vertex colour R, broken up by noise in world space.
    world = g.N('WorldPosition', -1500, 200)
    world_xy = g.mask(world, '', 'RG', -1350, 200)
    noise_uv = g.mulc(world_xy, '', 1.0 / 350.0, -1200, 200)
    noise = g.N('TextureSample', -1050, 200, texture=unreal.load_asset(NOISE),
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    g.C(noise_uv, '', noise, 'UVs')
    vertex = g.N('VertexColor', -1050, 420)
    strength = g.S('FreshStrength', 0.6, -1050, 520)
    breakup = g.addc(g.mulc(noise, 'R', 0.9, -850, 200), '', 0.55, -720, 200)
    # Squared: the fresh spoil stays on the crest and the upper slope; the lower slope is pure landscape colour.
    fresh_mask = g.mul(vertex, 'R', vertex, 'R', -950, 440)
    fresh = g.sat(g.mul(g.mul(fresh_mask, '', strength, '', -850, 440), '', breakup, '', -650, 360), '', -520, 360)
    tint = g.V('FreshTint', unreal.LinearColor(0.72, 0.6, 0.5, 1.0), -650, 40)
    # Fresh spoil is turned-up soil whatever grows on top: the darkened ground colour pulled toward FreshSoilColor
    # (on grass the crest goes brown, on sand it goes a darker, damper brown).
    soil = g.V('FreshSoilColor', unreal.LinearColor(0.16, 0.11, 0.07, 1.0), -650, -60)
    fresh_color = g.lerp(g.mul(ground, '', tint, '', -520, 20), '', soil, '', g.S('FreshSoilAmount', 0.55, -650, 100),
                         '', -420, 0)
    base_color = g.lerp(ground, '', fresh_color, '', fresh, '', -300, -120)

    roughness = g.lerp(g.S('Roughness', 0.92, -500, 520), '', g.S('FreshRoughness', 0.82, -500, 600), '', fresh, '',
                       -300, 520)
    specular = g.S('Specular', 0.25, -300, 680)

    # Ground detail (ripples, clods) from the landscape's world-space RVT normal, laid on the bank's own slope normal:
    # N = normalize(vertex normal + RVT normal.xy * DetailNormalStrength), world space (tangent_space_normal off).
    vertex_normal = g.N('VertexNormalWS', -650, 760)
    detail_xy = g.mask(rvt, 'Normal', 'RG', -1100, 820)
    detail_strength = g.mul(g.S('DetailNormalStrength', 1.0, -1100, 900), '', has_rvt, '', -950, 900)
    detail = g.mul(detail_xy, '', detail_strength, '', -800, 840)
    detail3 = g.N('AppendVector', -650, 860)
    g.C(detail, '', detail3, 'A')
    g.C(g.const(0.0, -760, 940), '', detail3, 'B')
    bank_normal = g.op('Add', vertex_normal, '', detail3, '', -500, 800)
    # Toward the foot (vertex colour G 0 -> 1) the bank takes the landscape's own normal from the RVT, so where it
    # meets the ground it is lit exactly like the ground and no crease line shows.
    foot = g.mul(vertex, 'G', has_rvt, '', -500, 980)
    blended = g.lerp(bank_normal, '', rvt, 'Normal', foot, '', -400, 880)
    normal = g.N('Normalize', -250, 880)
    g.C(blended, '', normal, '')
    material.set_editor_property('tangent_space_normal', False)
    mel.connect_material_property(normal, '', unreal.MaterialProperty.MP_NORMAL)

    mel.connect_material_property(base_color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(specular, '', unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    stats = mel.get_statistics(material)
    print('FIELDTRENCH_BANK_MATERIAL saved', saved, 'failures', g.failures,
          'instructions', stats.num_pixel_shader_instructions, 'samplers', stats.num_samplers)
    return material
