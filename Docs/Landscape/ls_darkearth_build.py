"""DarkEarth: the tenth painted layer of M_RTS_Landscape_DryCastle, a neutral dark earth.

The master had no dark ground: full Asphalt is mid grey, RTSSand salmon, RockDesert only darkens on slopes. DarkEarth is
a forest-floor soil texture in world space at two scales, desaturated and tinted, weight-blended like the other layers.
A landscape that does not paint it compiles it out, so the other maps on this master pay nothing.

    import sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
    import ls_darkearth_build; ls_darkearth_build.run()            # builds, checks, does NOT save
    ls_darkearth_build.save()                                      # after the checks pass

- The nodes carry the desc LSDarkEarth: a re-run replaces them; ls_master_build.run() leaves them alone.
- Tank trails: the layer's weight is added to SoftGround. ls_master_build.layer_weights() does the same from
  SOFT_ONLY_LAYERS; the splice made here (desc LSDarkEarthSoft) is for a master built before that and is removed by the
  next ls_master_build.run().
- Layer info: DATA/DarkEarth_LayerInfo, made by ls_darkearth_layerinfo.py in a headless run.
- Tune on the map's own instance: DarkEarth_Tint, DarkEarth_Desaturation, DarkEarth_Roughness, DarkEarth_Tiling,
  DarkEarth_FarMix, DarkEarth_BaseColor / DarkEarth_Normal (texture objects).
"""
import importlib

import unreal

import ls_graph as G

importlib.reload(G)
from ls_graph import N, C

MAT_PATH = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/M_RTS_Landscape_DryCastle"
LAYER = "DarkEarth"
DESC = "LSDarkEarth"
SOFT_DESC = "LSDarkEarthSoft"
GROUP = "Dark Earth"
TEX_DIR = "/Game/UVB80/Assets/UVB80/Environment/Textures/"
BASE_COLOR = TEX_DIR + "T_forrest_ground_01_diff_4k"
NORMAL = TEX_DIR + "T_forrest_ground_01_nor_4k_N"
TRAILS_FUNCTION = "MF_RTS_LS_TankTrails"
# Top-left of the section, under the original layer column.
X0, Y0 = -4300, 7400
WRAP = unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS
ST = unreal.MaterialSamplerType


def layer_blend(material):
    e = unreal.find_object(material, "MaterialExpressionLandscapeLayerBlend_0")
    if not e:
        raise RuntimeError("no LandscapeLayerBlend_0 in the master")
    return e


def blend_inputs(material):
    """[(layer name, connected expression name)] of the layer blend, in pin order."""
    blend = layer_blend(material)
    names = [str(layer.get_editor_property("layer_name")) for layer in blend.get_editor_property("layers")]
    sources = [e.get_name() if e else None for e in G.mel.get_inputs_for_material_expression(material, blend)]
    return list(zip(names, sources)), len(names), len(sources)


def ensure_layer(material):
    """Appends the DarkEarth entry to the blend's layers; the nine existing entries keep their connections."""
    blend = layer_blend(material)
    layers = list(blend.get_editor_property("layers"))
    if any(str(layer.get_editor_property("layer_name")) == LAYER for layer in layers):
        return False
    entry = unreal.LayerBlendInput()
    entry.set_editor_property("layer_name", LAYER)
    entry.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
    entry.set_editor_property("preview_weight", 0.0)
    layers.append(entry)
    blend.set_editor_property("layers", layers)
    return True


def sample(texture_object, uv, sampler_type, x, y):
    e = N("TextureSample", x, y, sampler_type=sampler_type, sampler_source=WRAP)
    C(texture_object, "", e, "Tex")
    C(uv, "", e, "UVs")
    return e


def build_layer(material):
    G.begin(material, DESC, GROUP)
    x, y = X0, Y0
    G.comment("Dark earth layer (DarkEarth): forest-floor soil in world space at two scales (DarkEarth_Tiling cm per "
              "tile, the far copy 5.3x larger hides the repeat), desaturated and tinted dark. The only dark ground of "
              "this master. Built by ls_darkearth_build.py; tune the DarkEarth_* parameters on the map's instance.",
              x - 80, y - 140, 2500, 760, G.LAYER_COLOR)
    world = G.mask(N("WorldPosition", x, y), "", "RG", x + 240, y)
    tiling = G.S("DarkEarth_Tiling", 700.0, x + 240, y + 140)
    uv_near = G.op("Divide", world, "", tiling, "", x + 460, y)
    uv_far = N("Divide", x + 460, y + 200, const_b=5.3)
    C(uv_near, "", uv_far, "A")

    color_tex = N("TextureObjectParameter", x + 460, y + 380, parameter_name="DarkEarth_BaseColor",
                  texture=unreal.load_asset(BASE_COLOR), sampler_type=ST.SAMPLERTYPE_COLOR, group=GROUP)
    normal_tex = N("TextureObjectParameter", x + 460, y - 100, parameter_name="DarkEarth_Normal",
                   texture=unreal.load_asset(NORMAL), sampler_type=ST.SAMPLERTYPE_NORMAL, group=GROUP)
    near = sample(color_tex, uv_near, ST.SAMPLERTYPE_COLOR, x + 800, y + 100)
    far = sample(color_tex, uv_far, ST.SAMPLERTYPE_COLOR, x + 800, y + 360)
    normal = sample(normal_tex, uv_near, ST.SAMPLERTYPE_NORMAL, x + 800, y - 160)

    far_mix = G.S("DarkEarth_FarMix", 0.5, x + 1060, y + 420)
    color = G.lerp(near, "RGB", far, "RGB", far_mix, "", x + 1260, y + 160)
    desat = N("Desaturation", x + 1460, y + 160)
    C(color, "", desat, "")
    C(G.S("DarkEarth_Desaturation", 0.45, x + 1260, y + 340), "", desat, "Fraction")
    tint = G.V("DarkEarth_Tint", unreal.LinearColor(0.42, 0.38, 0.36, 1.0), x + 1460, y + 340)
    tinted = G.mul(desat, "", G.mask(tint, "", "RGB", x + 1580, y + 260), "", x + 1700, y + 160)

    make = N("MakeMaterialAttributes", x + 1980, y)
    C(tinted, "", make, "BaseColor")
    C(normal, "RGB", make, "Normal")
    C(G.S("DarkEarth_Roughness", 0.92, x + 1700, y + 420), "", make, "Roughness")
    C(G.const(0.2, x + 1700, y + 520), "", make, "Specular")
    C(make, "", layer_blend(material), "Layer " + LAYER)
    return make


def trails_call(material):
    for e in G.live_expressions(material):
        if isinstance(e, unreal.MaterialExpressionMaterialFunctionCall):
            function = e.get_editor_property("material_function")
            if function and function.get_name() == TRAILS_FUNCTION:
                return e
    raise RuntimeError("no call of " + TRAILS_FUNCTION)


def master_has_soft_layer(material):
    """True when ls_master_build already samples DarkEarth for SoftGround (its nodes carry the LSMaster desc)."""
    return any(isinstance(e, unreal.MaterialExpressionLandscapeLayerSample)
               and str(e.get_editor_property("parameter_name")) == LAYER
               for e in G.tagged_expressions(material, "LSMaster") if unreal.SystemLibrary.is_valid(e))


def splice_soft_ground(material):
    """SoftGround = saturate(old SoftGround + DarkEarth weight), so tanks leave ruts in the dark earth too."""
    if master_has_soft_layer(material):
        return "master samples it"
    call = trails_call(material)
    sources = G.mel.get_inputs_for_material_expression(material, call)
    soft = sources[1]                     # inputs in priority order: Attributes, SoftGround
    if soft is None:
        raise RuntimeError("the tank trails call has no SoftGround input")
    G.begin(material, SOFT_DESC)
    x = call.get_editor_property("material_expression_editor_x") - 520
    y = call.get_editor_property("material_expression_editor_y") + 420
    weight = N("LandscapeLayerSample", x, y, parameter_name=LAYER, preview_weight=0.0)
    total = G.sat(G.op("Add", soft, "", weight, "", x + 240, y), "", x + 380, y)
    C(total, "", call, "SoftGround")
    return "spliced after " + soft.get_name()


def unsplice_soft_ground(material):
    """Hands SoftGround back to the node the splice read, so a re-run can remove the splice."""
    for e in G.tagged_expressions(material, SOFT_DESC):
        if unreal.SystemLibrary.is_valid(e) and isinstance(e, unreal.MaterialExpressionAdd):
            original = G.mel.get_inputs_for_material_expression(material, e)[0]
            C(original, "", trails_call(material), "SoftGround")


def run():
    G.failures.clear()
    material = unreal.load_asset(MAT_PATH)
    before, _, _ = blend_inputs(material)
    unsplice_soft_ground(material)
    removed = G.remove_tagged(material, DESC) + G.remove_tagged(material, SOFT_DESC)
    added = ensure_layer(material)
    build_layer(material)
    soft = splice_soft_ground(material)
    after, layer_count, source_count = blend_inputs(material)
    kept = dict(before)
    for name, source in after:
        if name != LAYER and kept.get(name) != source:
            G.failures.append("layer %s lost its input: %s -> %s" % (name, kept.get(name), source))
    if dict(after).get(LAYER) is None:
        G.failures.append("DarkEarth is not connected")
    if layer_count != source_count:
        G.failures.append("%d layers but %d inputs" % (layer_count, source_count))
    G.mel.recompile_material(material)
    print("removed", removed, "layer added", added, "soft ground:", soft)
    print("layer blend:", after)
    print("failures", G.failures)
    return G.failures


def save():
    if G.failures:
        raise RuntimeError("not saved, failures: %s" % G.failures)
    material = unreal.load_asset(MAT_PATH)
    print("saved", unreal.EditorAssetLibrary.save_loaded_asset(material, False))
