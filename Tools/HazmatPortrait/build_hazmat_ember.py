"""Builds the SovietHazmat "Ember Breath" portrait in the editor (UECP execute_python:
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/HazmatPortrait/build_hazmat_ember.py").read(), ns); ns["run"]()).

  T_Portrait_HazmatEmber   the old hazmat thumbnail (2048 source, previously capped at 256 and squeezed into the box)
                           cropped to the PortraitImage aspect 176 x 232 (source x 198..1752) = 971 x 1280
  M_UI_HazmatEmberEyes     UI material: portrait + red lens glow (Custom node = hazmat_ember_eyes.hlsl), pause-ignoring
                           Time with a 600 s period (+ TimeOffset for captures); opacity = VertexColor.a only
  SB_Portrait_HazmatEmber  Slate brush drawing the material
Then W_BP_Portrait.PortraitBrushes[SovietHazmat] points at the new brush. The old /Game/Thumbnails/Portrait_RusHazmat
brush is untouched, so reverting is one map entry.
"""
import math

import unreal

PROJECT = "D:/UE5Projects/RTS_Survival"
SRC_PNG = PROJECT + "/Saved/HazmatPortrait/T_Portrait_HazmatEmber.png"
HLSL = PROJECT + "/Source/Tools/HazmatPortrait/hazmat_ember_eyes.hlsl"
CAPTURE_DIR = PROJECT + "/Saved/HazmatPortrait"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/HazmatPortrait"
TEX = "T_Portrait_HazmatEmber"
MAT = "M_UI_HazmatEmberEyes"
BRUSH = "SB_Portrait_HazmatEmber"
SIZE_BRUSH = "/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope"
PORTRAIT_BP = "/Game/RTS_Survival/Blueprints/GameUI/Announcer/W_BP_Portrait"
REF_TEX = "/Game/RTS_Survival/Textures/GameUI/Commanders/Portraits/T_Commander_Balanced"
TEX_PROPS = ("lod_group", "mip_gen_settings", "compression_settings", "srgb", "never_stream", "lod_bias")

# Crop of the 2048 source: x 198..1752, full height. Lens ellipses fitted on the source (centre px, minor/major
# semi-axis px, major-axis angle in degrees, image y down) and expressed in height units p = (u * ASPECT, v).
CROP_X0, CROP_W, SRC_H = 198.0, 1554.0, 2048.0
ASPECT = CROP_W / SRC_H
LENSES = {"L": (899, 868, 40, 51, 116.0), "R": (1051, 871, 38, 53, 74.5)}
SCALARS = {"Aspect": ASPECT, "Voice": 1.0, "BreathHz": 0.22, "Intensity": 1.0, "Spill": 0.32, "SpillRadius": 0.07,
           "TimeOffset": 0.0}

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def lens_vectors(cx, cy, minor, major, ang):
    a = math.radians(ang)
    dx, dy = math.cos(a), math.sin(a)
    centre = ((cx - CROP_X0) / SRC_H, cy / SRC_H)
    axis_a = (dx * major / SRC_H, dy * major / SRC_H)
    axis_b = (-dy * minor / SRC_H, dx * minor / SRC_H)
    return centre, axis_a, axis_b


def in_pie():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()


def existing(path):
    """In-memory first: EditorAssetLibrary refuses every call while the editor is in Play-In-Editor."""
    name = path.rsplit("/", 1)[1]
    return unreal.find_object(None, path + "." + name) or (None if in_pie() else unreal.load_asset(path))


def save(path):
    if in_pie():
        print("SAVE DEFERRED (PIE)", path)
        return False
    return unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)


def hlsl_body():
    return open(HLSL, encoding="utf8").read()


def import_texture():
    t = unreal.AssetImportTask()
    t.filename = SRC_PNG
    t.destination_path = FOLDER
    t.destination_name = TEX
    t.replace_existing = True
    t.automated = True
    t.save = False
    tools.import_asset_tasks([t])
    tex = existing(FOLDER + "/" + TEX)
    ref = unreal.load_asset(REF_TEX)
    for p in TEX_PROPS:
        tex.set_editor_property(p, ref.get_editor_property(p))
    save(FOLDER + "/" + TEX)
    return tex


def _vec2_param(material, name, value, x, y):
    node = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", unreal.LinearColor(value[0], value[1], 0.0, 0.0))
    m = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, x + 220, y)
    for ch in "rgba":
        m.set_editor_property(ch, ch in "rg")
    lib.connect_material_expressions(node, "", m, "")
    return m


def build_material(tex):
    path = FOLDER + "/" + MAT
    material = existing(path)
    if material is not None:
        for _ in range(8):
            lib.delete_all_material_expressions(material)
            if lib.get_num_material_expressions(material) == 0:
                break
    else:
        material = tools.create_asset(MAT, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1500, -500)
    sample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1100, -700)
    sample.set_editor_property("parameter_name", "Portrait")
    sample.set_editor_property("texture", tex)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    lib.connect_material_expressions(uv, "", sample, "UVs")

    seconds = lib.create_material_expression(material, unreal.MaterialExpressionTime, -1500, -350)
    seconds.set_editor_property("ignore_pause", True)
    seconds.set_editor_property("override_period", True)
    seconds.set_editor_property("period", 600.0)
    offset = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1500, -250)
    offset.set_editor_property("parameter_name", "TimeOffset")
    offset.set_editor_property("default_value", 0.0)
    t_add = lib.create_material_expression(material, unreal.MaterialExpressionAdd, -1250, -320)
    lib.connect_material_expressions(seconds, "", t_add, "A")
    lib.connect_material_expressions(offset, "", t_add, "B")

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -300)
    custom.set_editor_property("code", hlsl_body())
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "HazmatEmberEyes")
    names = ["Base", "UV", "T", "Aspect", "LCenter", "LAxisA", "LAxisB", "RCenter", "RAxisA", "RAxisB", "Voice",
             "BreathHz", "Intensity", "Spill", "SpillRadius"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    lib.connect_material_expressions(sample, "RGB", custom, "Base")
    lib.connect_material_expressions(uv, "", custom, "UV")
    lib.connect_material_expressions(t_add, "", custom, "T")

    y = -150
    for side, lens in LENSES.items():
        centre, axis_a, axis_b = lens_vectors(*lens)
        for suffix, val in (("Center", centre), ("AxisA", axis_a), ("AxisB", axis_b)):
            m = _vec2_param(material, "Lens" + side + suffix, val, -1500, y)
            lib.connect_material_expressions(m, "", custom, side + suffix)
            y += 120
    for name in ("Aspect", "Voice", "BreathHz", "Intensity", "Spill", "SpillRadius"):
        node = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1500, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", float(SCALARS[name]))
        lib.connect_material_expressions(node, "", custom, name)
        y += 70

    # Only the widget's opacity comes from VertexColor (fade-in); colour is not tinted (canvas captures feed a
    # non-white vertex colour).
    vc = lib.create_material_expression(material, unreal.MaterialExpressionVertexColor, -250, -100)
    lib.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    lib.connect_material_property(vc, "A", unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    save(path)
    return material


def build_brush(material):
    path = FOLDER + "/" + BRUSH
    brush_asset = existing(path)
    if brush_asset is None:
        brush_asset = tools.create_asset(BRUSH, FOLDER, unreal.SlateBrushAsset, unreal.SlateBrushAssetFactory())
    ref = unreal.load_asset(SIZE_BRUSH).get_editor_property("brush")
    b = brush_asset.get_editor_property("brush")
    b.set_editor_property("resource_object", material)
    b.set_editor_property("image_size", ref.get_editor_property("image_size"))
    b.set_editor_property("draw_as", unreal.SlateBrushDrawType.IMAGE)
    b.set_editor_property("tint_color", ref.get_editor_property("tint_color"))
    brush_asset.set_editor_property("brush", b)
    save(path)
    return brush_asset


def point_portrait(brush_asset):
    if in_pie():
        raise RuntimeError("stop Play-In-Editor first: W_BP_Portrait is compiled and saved here")
    bp = unreal.load_asset(PORTRAIT_BP)
    cdo = unreal.get_default_object(bp.generated_class())
    brushes = cdo.get_editor_property("PortraitBrushes")
    brushes[unreal.RTSPortraitTypes.SOVIET_HAZMAT] = brush_asset
    cdo.set_editor_property("PortraitBrushes", brushes)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(PORTRAIT_BP, only_if_is_dirty=False)
    check = unreal.get_default_object(bp.generated_class()).get_editor_property("PortraitBrushes")
    return check[unreal.RTSPortraitTypes.SOVIET_HAZMAT]


def run(step="all"):
    out = {}
    tex = import_texture() if step in ("all", "texture") else existing(FOLDER + "/" + TEX)
    out["texture"] = "%s %dx%d" % (tex.get_path_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    if step in ("all", "material"):
        mat = build_material(tex)
        out["material"] = mat.get_path_name()
        out["brush"] = build_brush(mat).get_path_name()
    if step in ("all", "point"):
        out["hazmat_brush_now"] = str(point_portrait(existing(FOLDER + "/" + BRUSH)))
    print("HAZMAT_EMBER", out)
    return out


def capture(times=(0.3, 1.0, 2.35, 3.3, 5.1, 7.9), size=(528, 696), prefix="ember"):
    """Draw the material onto a render target at several TimeOffsets and export PNGs into Saved/HazmatPortrait."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world() or ues.get_game_world()
    rt = unreal.RenderingLibrary.create_render_target2d(world, size[0], size[1],
                                                        unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB,
                                                        unreal.LinearColor(0, 0, 0, 1))
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, existing(FOLDER + "/" + MAT))
    files = []
    for i, t in enumerate(times):
        unreal.RenderingLibrary.clear_render_target2d(world, rt, unreal.LinearColor(0, 0, 0, 1))
        mid.set_scalar_parameter_value("TimeOffset", t)
        canvas, sz, ctx = unreal.RenderingLibrary.begin_draw_canvas_to_render_target(world, rt)
        canvas.draw_material(mid, unreal.Vector2D(0, 0), unreal.Vector2D(size[0], size[1]), unreal.Vector2D(0, 0),
                             unreal.Vector2D(1, 1))
        unreal.RenderingLibrary.end_draw_canvas_to_render_target(world, ctx)
        name = "%s_%d.png" % (prefix, i)
        unreal.RenderingLibrary.export_render_target(world, rt, CAPTURE_DIR, name)
        files.append(name)
    print("HAZMAT_CAPTURE", files)
    return files


def save_all():
    """After PIE: write the three assets (they were built while the editor was in play mode)."""
    return {n: save(FOLDER + "/" + n) for n in (TEX, MAT, BRUSH)}
