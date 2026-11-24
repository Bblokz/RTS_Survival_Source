"""Builds the GermanScavengers "Atemdampf" (breath vapour) portrait in the editor. UECP execute_python:
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/ScavengerPortrait/build_scavenger_vapour.py").read(), ns); ns["run"]()

  T_Portrait_ScavengerVapour      the old scavenger thumbnail's 2048 source (Thumb_BP_Thumbnail_RusHazmatThumbnail_Scavenger,
                                  exported with AssetExportTask), cropped to the PortraitImage aspect 176 x 232
                                  (source x 247..1801, full height) = 1554 x 2048. The old brush squeezed the square image.
  M_UI_ScavengerBreathVapour      UI material: portrait + vapour from the two cheek filters + glowing, bubbling chest vials
                                  (Custom node = scavenger_breath_vapour.hlsl; vials added later on 2026-11-12), pause-ignoring
                                  Time with a 600 s period (+ TimeOffset for captures); opacity = VertexColor.a only
                                  (widget fade-in)
  SB_Portrait_ScavengerVapour     Slate brush drawing the material
run('point') sets W_BP_Portrait.PortraitBrushes[GermanScavengers]; the old brush
/Game/Thumbnails/Thumb_BP_Thumbnail_RusHazmatThumbnail_Scavenger_Brush is untouched (revert = one map entry).
Filter positions come from Saved/ScavengerPortrait/features.json (source pixels).
"""
import unreal

PROJECT = "D:/UE5Projects/RTS_Survival"
SRC_PNG = PROJECT + "/Saved/ScavengerPortrait/T_Portrait_ScavengerVapour.png"
HLSL = PROJECT + "/Source/Tools/ScavengerPortrait/scavenger_breath_vapour.hlsl"
CAPTURE_DIR = PROJECT + "/Saved/ScavengerPortrait"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/ScavengerPortrait"
TEX = "T_Portrait_ScavengerVapour"
MAT = "M_UI_ScavengerBreathVapour"
BRUSH = "SB_Portrait_ScavengerVapour"
SIZE_BRUSH = "/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope"
PORTRAIT_BP = "/Game/RTS_Survival/Blueprints/GameUI/Announcer/W_BP_Portrait"
REF_TEX = "/Game/RTS_Survival/Textures/GameUI/Commanders/Portraits/T_Commander_Balanced"
TEX_PROPS = ("lod_group", "mip_gen_settings", "compression_settings", "srgb", "never_stream", "lod_bias")

# Cheek filters from features.json: centre (x, y) and radius in source px, side -1 = left, +1 = right. Converted to height
# units p = (u * Aspect, v) of the crop; z carries side * radius.
CROP_X0, CROP_W, SRC_H = 247.0, 1554.0, 2048.0
ASPECT = CROP_W / SRC_H
FILTERS = {"FilL": (937, 778, 42, -1), "FilR": (1127, 778, 38, 1)}
# Chest vials from features.json: capsule end centres (lower end first, bubbles rise toward the second) and radius, px.
VIALS = {"V1": ((752, 1098), (822, 1024), 27), "V2": ((792, 1192), (858, 1117), 29)}
SCALARS = {"Aspect": ASPECT, "Voice": 1.0, "Strength": 1.0, "Rise": 1.0, "VialStrength": 1.0, "TimeOffset": 0.0,
           "V1R": VIALS["V1"][2] / SRC_H, "V2R": VIALS["V2"][2] / SRC_H}


def px_to_hu(x, y):
    return ((x - CROP_X0) / SRC_H, y / SRC_H)

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def filter_vec(x, y, r, side):
    return ((x - CROP_X0) / SRC_H, y / SRC_H, side * r / SRC_H)


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
    custom.set_editor_property("code", open(HLSL, encoding="utf8").read())
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "ScavengerBreathVapour")
    names = ["Base", "UV", "T", "Aspect", "FilL", "FilR", "Voice", "Strength", "Rise",
             "V1A", "V1B", "V2A", "V2B", "V1R", "V2R", "VialStrength"]
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
    for name, f in FILTERS.items():
        v = filter_vec(*f)
        node = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1500, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(v[0], v[1], v[2], 0.0))
        m = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1280, y)
        for ch in "rgba":
            m.set_editor_property(ch, ch in "rgb")
        lib.connect_material_expressions(node, "", m, "")
        lib.connect_material_expressions(m, "", custom, name)
        y += 120
    for vial, (a, b, _r) in VIALS.items():
        for suffix, pt in (("A", a), ("B", b)):
            hu = px_to_hu(*pt)
            node = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1500, y)
            node.set_editor_property("parameter_name", vial + suffix)
            node.set_editor_property("default_value", unreal.LinearColor(hu[0], hu[1], 0.0, 0.0))
            m = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1280, y)
            for ch in "rgba":
                m.set_editor_property(ch, ch in "rg")
            lib.connect_material_expressions(node, "", m, "")
            lib.connect_material_expressions(m, "", custom, vial + suffix)
            y += 120
    for name in ("Aspect", "Voice", "Strength", "Rise", "VialStrength", "V1R", "V2R"):
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
    brushes[unreal.RTSPortraitTypes.GERMAN_SCAVENGERS] = brush_asset
    cdo.set_editor_property("PortraitBrushes", brushes)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # save_asset(path) once returned without writing the file (2026-11-12): save the loaded object and check.
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError("W_BP_Portrait did not save")
    check = unreal.get_default_object(bp.generated_class()).get_editor_property("PortraitBrushes")
    return check[unreal.RTSPortraitTypes.GERMAN_SCAVENGERS]


def run(step="all"):
    out = {}
    tex = import_texture() if step in ("all", "texture") else existing(FOLDER + "/" + TEX)
    out["texture"] = "%s %dx%d" % (tex.get_path_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    if step in ("all", "material"):
        mat = build_material(tex)
        out["material"] = mat.get_path_name()
        out["brush"] = build_brush(mat).get_path_name()
    if step in ("all", "point"):
        out["scavenger_brush_now"] = str(point_portrait(existing(FOLDER + "/" + BRUSH)))
    print("SCAVENGER_VAPOUR", out)
    return out


def capture(times=(0.3, 1.0, 2.35, 2.91, 5.1, 7.9), size=(528, 696), prefix="vapour", voice=1.0):
    """Draw the material onto a render target at several TimeOffsets and export PNGs into Saved/ScavengerPortrait.
    The editor clock is added to TimeOffset, so frames are relative to an unknown start: sweep offsets to compare."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world() or ues.get_game_world()
    rt = unreal.RenderingLibrary.create_render_target2d(world, size[0], size[1],
                                                        unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB,
                                                        unreal.LinearColor(0, 0, 0, 1))
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, existing(FOLDER + "/" + MAT))
    mid.set_scalar_parameter_value("Voice", voice)
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
    print("SCAVENGER_CAPTURE", files)
    return files


def save_all():
    """After PIE: write the assets (they were built while the editor was in play mode)."""
    return {n: save(FOLDER + "/" + n) for n in (TEX, MAT, BRUSH)}
