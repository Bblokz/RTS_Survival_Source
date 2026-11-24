"""Three mineral layers of M_RTS_Landscape_DryCastle, added for Ger_2ndOreMountains (layers 11-13):

    IronOxide   vivid red hematite earth: an accent, painted in a few named places only
    Ochre       yellow-brown limonite ground: the halo of the iron and broad stains on the pale ground
    SlateScree  cool grey slate gravel: breaks the warm valley floor

Each is one texture set in world space at two scales, desaturated and tinted, weight-blended like DarkEarth
(ls_darkearth_build.py, which this follows). A landscape that does not paint a layer compiles it out.

    import sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
    import ls_ore_layers_build as B; B.run()        # builds, checks, does NOT save
    B.save()                                        # after the checks pass
    B.layer_infos()                                 # headless only (the LayerName console trick)

- Nodes carry the desc LS<Layer>: a re-run replaces them; ls_master_build.run() leaves them alone.
- Parameters per layer (group "<Layer>"): <Layer>_Tint, _Desaturation, _Roughness, _Tiling, _FarMix, texture objects
  <Layer>_BaseColor / <Layer>_Normal. Tune on the map's own instance.
- Not counted as soft ground for the tank trails and not in the ground-surface settings.
"""
import importlib

import unreal

import ls_graph as G
import ls_darkearth_build as D

importlib.reload(G)
importlib.reload(D)
from ls_graph import N, C

MAT_PATH = D.MAT_PATH
DATA_DIR = "/Game/RTS_Survival/Materials/ground/Landscape/CastleGrassDesertDry/DATA"
MEGA = "/Game/Megascans/Surfaces/"
ST = unreal.MaterialSamplerType
# name, base colour, normal, tiling cm, tint (linear, may exceed 1), desaturation, roughness, far mix, debug colour, note
LAYERS = [
    ("IronOxide", MEGA + "Arid_Soil_Ground_wihhfexbw/T_Arid_Soil_Ground_wihhfexbw_2K_D",
     MEGA + "Arid_Soil_Ground_wihhfexbw/T_Arid_Soil_Ground_wihhfexbw_2K_N", 520.0, (1.75, 0.24, 0.09), 0.9, 0.9, 0.45,
     (0.6, 0.08, 0.04), "vivid red hematite earth; an accent, never a field"),
    ("Ochre", MEGA + "Sulphur_Rock_vmijee1g/T_Sulphur_Rock_vmijee1g_2K_D",
     MEGA + "Sulphur_Rock_vmijee1g/T_Sulphur_Rock_vmijee1g_2K_N", 640.0, (1.0, 0.56, 0.14), 0.7, 0.88, 0.5,
     (0.6, 0.4, 0.08), "yellow-brown limonite ground"),
    ("SlateScree", MEGA + "Desert_Western_Ground_Gravel_Slate_01_venoehv/T_Desert_Western_Ground_Gravel_Slate_01_venoehv_2K_D",
     MEGA + "Desert_Western_Ground_Gravel_Slate_01_venoehv/T_Desert_Western_Ground_Gravel_Slate_01_venoehv_2K_N", 560.0,
     (0.5, 0.5, 0.52), 0.75, 0.8, 0.4, (0.25, 0.27, 0.32), "cool grey slate gravel"),
]
X0, Y0, ROW = -4300, 8400, 950


def ensure_layer(material, name):
    blend = D.layer_blend(material)
    layers = list(blend.get_editor_property("layers"))
    if any(str(layer.get_editor_property("layer_name")) == name for layer in layers):
        return False
    entry = unreal.LayerBlendInput()
    entry.set_editor_property("layer_name", name)
    entry.set_editor_property("blend_type", unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
    entry.set_editor_property("preview_weight", 0.0)
    layers.append(entry)
    blend.set_editor_property("layers", layers)
    return True


def check_texture(path, normal):
    """A texture the landscape samples in world space must be a plain streamed 2D texture with the right compression."""
    tex = unreal.load_asset(path)
    if tex is None:
        G.failures.append("missing texture " + path)
        return None
    if tex.get_editor_property("virtual_texture_streaming"):
        G.failures.append("virtual texture: " + path)
    compression = tex.get_editor_property("compression_settings")
    if normal != (compression == unreal.TextureCompressionSettings.TC_NORMALMAP):
        G.failures.append("compression %s on %s" % (compression, path))
    if not normal and not tex.get_editor_property("srgb"):
        G.failures.append("not sRGB: " + path)
    return tex


def build_layer(material, index, spec):
    name, base, normal_path, tiling_cm, tint_rgb, desat_value, rough, far_value, _, note = spec
    G.begin(material, "LS" + name, name)
    x, y = X0, Y0 + index * ROW
    G.comment("%s layer: %s. One texture set in world space at two scales (%s_Tiling cm per tile, the far copy 5.3x "
              "larger hides the repeat), desaturated and tinted. Built by ls_ore_layers_build.py for Ger_2ndOreMountains; "
              "tune the %s_* parameters on the map's instance." % (name, note, name, name),
              x - 80, y - 140, 2500, 760, G.LAYER_COLOR)
    world = G.mask(N("WorldPosition", x, y), "", "RG", x + 240, y)
    tiling = G.S(name + "_Tiling", tiling_cm, x + 240, y + 140)
    uv_near = G.op("Divide", world, "", tiling, "", x + 460, y)
    uv_far = N("Divide", x + 460, y + 200, const_b=5.3)
    C(uv_near, "", uv_far, "A")
    color_tex = N("TextureObjectParameter", x + 460, y + 380, parameter_name=name + "_BaseColor",
                  texture=check_texture(base, False), sampler_type=ST.SAMPLERTYPE_COLOR, group=name)
    normal_tex = N("TextureObjectParameter", x + 460, y - 100, parameter_name=name + "_Normal",
                   texture=check_texture(normal_path, True), sampler_type=ST.SAMPLERTYPE_NORMAL, group=name)
    near = D.sample(color_tex, uv_near, ST.SAMPLERTYPE_COLOR, x + 800, y + 100)
    far = D.sample(color_tex, uv_far, ST.SAMPLERTYPE_COLOR, x + 800, y + 360)
    normal = D.sample(normal_tex, uv_near, ST.SAMPLERTYPE_NORMAL, x + 800, y - 160)
    color = G.lerp(near, "RGB", far, "RGB", G.S(name + "_FarMix", far_value, x + 1060, y + 420), "", x + 1260, y + 160)
    desat = N("Desaturation", x + 1460, y + 160)
    C(color, "", desat, "")
    C(G.S(name + "_Desaturation", desat_value, x + 1260, y + 340), "", desat, "Fraction")
    tint = G.V(name + "_Tint", unreal.LinearColor(tint_rgb[0], tint_rgb[1], tint_rgb[2], 1.0), x + 1460, y + 340)
    tinted = G.mul(desat, "", G.mask(tint, "", "RGB", x + 1580, y + 260), "", x + 1700, y + 160)
    make = N("MakeMaterialAttributes", x + 1980, y)
    C(tinted, "", make, "BaseColor")
    C(normal, "RGB", make, "Normal")
    C(G.S(name + "_Roughness", rough, x + 1700, y + 420), "", make, "Roughness")
    C(G.const(0.2, x + 1700, y + 520), "", make, "Specular")
    C(make, "", D.layer_blend(material), "Layer " + name)
    return make


def run():
    G.failures.clear()
    material = unreal.load_asset(MAT_PATH)
    before, _, _ = D.blend_inputs(material)
    names = [spec[0] for spec in LAYERS]
    removed = sum(G.remove_tagged(material, "LS" + name) for name in names)
    added = [name for name in names if ensure_layer(material, name)]
    for index, spec in enumerate(LAYERS):
        build_layer(material, index, spec)
    after, layer_count, source_count = D.blend_inputs(material)
    kept = dict(before)
    for name, source in after:
        if name not in names and kept.get(name) != source:
            G.failures.append("layer %s lost its input: %s -> %s" % (name, kept.get(name), source))
    for name in names:
        if dict(after).get(name) is None:
            G.failures.append(name + " is not connected")
    if layer_count != source_count:
        G.failures.append("%d layers but %d inputs" % (layer_count, source_count))
    G.mel.recompile_material(material)
    print("removed", removed, "layers added", added)
    print("layer blend:", after)
    print("failures", G.failures)
    return G.failures


def stats(paths):
    out = {}
    for path in paths:
        s = G.mel.get_statistics(unreal.load_asset(path))
        out[path.split("/")[-1]] = (s.num_pixel_shader_instructions, s.num_vertex_shader_instructions, s.num_samplers,
                                    s.num_pixel_texture_samples)
    return out


def save():
    if G.failures:
        raise RuntimeError("not saved, failures: %s" % G.failures)
    material = unreal.load_asset(MAT_PATH)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    print("saved", ok)
    return ok


def layer_infos():
    """Headless only: LayerName is read-only to Python and the console SET trick fills it in a commandlet."""
    for spec in LAYERS:
        name, color = spec[0], spec[8]
        path = "%s/%s_LayerInfo" % (DATA_DIR, name)
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            info = unreal.load_asset(path)
        else:
            unreal.AssetToolsHelpers.get_asset_tools().create_asset(name + "_LayerInfo", DATA_DIR,
                                                                    unreal.LandscapeLayerInfoObject, None)
            info = unreal.load_asset(path)
            unreal.SystemLibrary.execute_console_command(None, "set %s LayerName %s" % (info.get_path_name(), name))
            info.set_editor_property("no_weight_blend", False)
            info.set_editor_property("layer_usage_debug_color", unreal.LinearColor(color[0], color[1], color[2], 1.0))
        found = str(info.get_editor_property("layer_name"))
        if found != name:
            # Never leave a misnamed info on disk: importing a landscape with it crashes.
            unreal.log_error("[ls_ore_layers] layer name is %s, not %s: not saved" % (found, name))
            continue
        unreal.EditorAssetLibrary.save_loaded_asset(info, only_if_is_dirty=False)
        unreal.log("[ls_ore_layers] saved %s with layer name %s" % (path, found))
