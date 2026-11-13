"""Builds the GermanVultures "Atemhauch" (breath fog) portrait in the editor. UECP execute_python:
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/VulturePortrait/build_vulture_fog.py").read(), ns); ns["run"]()

  T_Portrait_VultureFog            Blender render (CommanderPortraits/Tools/vulture_portrait.py), 1024 x 1350 = the
                                   PortraitImage aspect 176 x 232
  T_Portrait_VultureFog_VisorMask  visor mask pass from the same render (white = visor glass), grayscale, sRGB off
  M_UI_VultureFogVisor             UI material: portrait + breath fog on the visor (Custom node = vulture_fog_visor.hlsl),
                                   pause-ignoring Time with a 600 s period (+ TimeOffset for captures); opacity =
                                   VertexColor.a only (widget fade-in)
  SB_Portrait_VultureFog           Slate brush drawing the material
run('point') sets W_BP_Portrait.PortraitBrushes[GermanVultures] (that entry was empty before).
Source PNGs are copied into Saved/VulturePortrait (UECP python only opens files under the project root).
"""
import unreal

PROJECT = "D:/UE5Projects/RTS_Survival"
SRC_DIR = PROJECT + "/Saved/VulturePortrait/"
HLSL = PROJECT + "/Source/Tools/VulturePortrait/vulture_fog_visor.hlsl"
CAPTURE_DIR = PROJECT + "/Saved/VulturePortrait"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/VulturePortrait"
TEX = "T_Portrait_VultureFog"
MASK = "T_Portrait_VultureFog_VisorMask"
MAT = "M_UI_VultureFogVisor"
BRUSH = "SB_Portrait_VultureFog"
SIZE_BRUSH = "/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope"
PORTRAIT_BP = "/Game/RTS_Survival/Blueprints/GameUI/Announcer/W_BP_Portrait"
REF_TEX = "/Game/RTS_Survival/Textures/GameUI/Commanders/Portraits/T_Commander_Balanced"
TEX_PROPS = ("lod_group", "mip_gen_settings", "compression_settings", "srgb", "never_stream", "lod_bias")

# Anchors from CommanderPortraits/Renders/Vulture/Vulture_anchors.json (image UV, v down), in height units
# p = (u * ASPECT, v): the mouth point sits at 20 % of the visor height above its lower edge.
RES = (1024, 1350)
ASPECT = RES[0] / RES[1]
VISOR_LOW_UV = (0.7068621516227722, 0.5263137817382812)
VISOR_V = (0.23949205875396729, 0.5958761274814606)
MOUTH = (VISOR_LOW_UV[0] * ASPECT, VISOR_LOW_UV[1])
SCALARS = {"Aspect": ASPECT, "VisH": VISOR_V[1] - VISOR_V[0], "Voice": 1.0, "Strength": 1.0, "Spread": 1.0,
           "TimeOffset": 0.0}

lib = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


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


def _import(name):
    t = unreal.AssetImportTask()
    t.filename = SRC_DIR + name + ".png"
    t.destination_path = FOLDER
    t.destination_name = name
    t.replace_existing = True
    t.automated = True
    t.save = False
    tools.import_asset_tasks([t])
    tex = existing(FOLDER + "/" + name)
    ref = unreal.load_asset(REF_TEX)
    for p in TEX_PROPS:
        tex.set_editor_property(p, ref.get_editor_property(p))
    return tex


def import_textures():
    tex = _import(TEX)
    save(FOLDER + "/" + TEX)
    mask = _import(MASK)
    mask.set_editor_property("srgb", False)
    mask.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
    save(FOLDER + "/" + MASK)
    return tex, mask


def _scalar(material, name, x, y):
    node = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", float(SCALARS[name]))
    return node


def build_material(tex, mask):
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

    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1600, -600)
    tobj = lib.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1600, -800)
    tobj.set_editor_property("parameter_name", "Portrait")
    tobj.set_editor_property("texture", tex)
    tobj.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    sample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -1200, -800)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    sample.set_editor_property("texture", tex)  # the object input wins; this only keeps the validator quiet
    lib.connect_material_expressions(tobj, "", sample, "Tex")
    lib.connect_material_expressions(uv, "", sample, "UVs")
    msample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -1200, -560)
    msample.set_editor_property("parameter_name", "VisorMask")
    msample.set_editor_property("texture", mask)
    msample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    lib.connect_material_expressions(uv, "", msample, "UVs")

    seconds = lib.create_material_expression(material, unreal.MaterialExpressionTime, -1600, -350)
    seconds.set_editor_property("ignore_pause", True)
    seconds.set_editor_property("override_period", True)
    seconds.set_editor_property("period", 600.0)
    offset = _scalar(material, "TimeOffset", -1600, -250)
    t_add = lib.create_material_expression(material, unreal.MaterialExpressionAdd, -1350, -320)
    lib.connect_material_expressions(seconds, "", t_add, "A")
    lib.connect_material_expressions(offset, "", t_add, "B")

    mouth = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1600, -150)
    mouth.set_editor_property("parameter_name", "Mouth")
    mouth.set_editor_property("default_value", unreal.LinearColor(MOUTH[0], MOUTH[1], 0.0, 0.0))
    mouth_rg = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1350, -150)
    for ch in "rgba":
        mouth_rg.set_editor_property(ch, ch in "rg")
    lib.connect_material_expressions(mouth, "", mouth_rg, "")

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -300)
    custom.set_editor_property("code", open(HLSL, encoding="utf8").read())
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "VultureFogVisor")
    names = ["Base", "Tex", "Mask", "UV", "T", "Aspect", "Mouth", "VisH", "Voice", "Strength", "Spread"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    lib.connect_material_expressions(sample, "RGB", custom, "Base")
    lib.connect_material_expressions(tobj, "", custom, "Tex")
    lib.connect_material_expressions(msample, "R", custom, "Mask")
    lib.connect_material_expressions(uv, "", custom, "UV")
    lib.connect_material_expressions(t_add, "", custom, "T")
    lib.connect_material_expressions(mouth_rg, "", custom, "Mouth")
    y = 0
    for name in ("Aspect", "VisH", "Voice", "Strength", "Spread"):
        lib.connect_material_expressions(_scalar(material, name, -1600, y), "", custom, name)
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
    brushes[unreal.RTSPortraitTypes.GERMAN_VULTURES] = brush_asset
    cdo.set_editor_property("PortraitBrushes", brushes)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # save_asset(path) once returned without writing the file (2026-11-12): save the loaded object and check.
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError("W_BP_Portrait did not save")
    check = unreal.get_default_object(bp.generated_class()).get_editor_property("PortraitBrushes")
    return check[unreal.RTSPortraitTypes.GERMAN_VULTURES]


def run(step="all"):
    out = {}
    if step in ("all", "texture"):
        tex, mask = import_textures()
    else:
        tex, mask = existing(FOLDER + "/" + TEX), existing(FOLDER + "/" + MASK)
    out["texture"] = "%s %dx%d" % (tex.get_path_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y())
    out["mask"] = "%s %dx%d" % (mask.get_path_name(), mask.blueprint_get_size_x(), mask.blueprint_get_size_y())
    if step in ("all", "material"):
        mat = build_material(tex, mask)
        out["material"] = mat.get_path_name()
        out["brush"] = build_brush(mat).get_path_name()
    if step in ("all", "point"):
        out["vultures_brush_now"] = str(point_portrait(existing(FOLDER + "/" + BRUSH)))
    print("VULTURE_FOG", out)
    return out


def capture(times=(0.3, 1.0, 2.35, 2.91, 5.1, 7.9), size=(528, 696), prefix="fog", voice=1.0):
    """Draw the material onto a render target at several TimeOffsets and export PNGs into Saved/VulturePortrait."""
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
    print("VULTURE_CAPTURE", files)
    return files


def save_all():
    """After PIE: write the assets (they were built while the editor was in play mode)."""
    return {n: save(FOLDER + "/" + n) for n in (TEX, MASK, MAT, BRUSH)}
