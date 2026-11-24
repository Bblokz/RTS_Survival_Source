"""Builds the SovietMosin "Kopf & Augen" (head nod + blinks) portrait in the editor. UECP execute_python:
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/MosinPortrait/build_mosin_head_nod.py").read(), ns); ns["run"]()

  T_Portrait_MosinRifleman       Blender render (CommanderPortraits/Tools/mosin_portrait.py), 1024 x 1350 = the
                                 PortraitImage aspect 176 x 232
  T_Portrait_MosinRifleman_Mask  mask pass from the same render: R = face scarf, G = follows the head (head bone weight,
                                 helmet, scarf). sRGB off, BC1 masks
  M_UI_MosinHeadNod              UI material: the portrait re-sampled through a small nod/tilt about the neck + blinks
                                 (Custom node = mosin_head_nod.hlsl), pause-ignoring Time with a 600 s period
                                 (+ TimeOffset for captures); opacity = VertexColor.a only (widget fade-in)
  SB_Portrait_MosinRifleman      Slate brush drawing the material
run('point') sets W_BP_Portrait.PortraitBrushes[SovietMosin] (needs the C++ build with ERTSPortraitTypes::SovietMosin).
Source PNGs are copied into Saved/MosinPortrait (UECP python only opens files under the project root).
"""
import unreal

PROJECT = "D:/UE5Projects/RTS_Survival"
SRC_DIR = PROJECT + "/Saved/MosinPortrait/"
HLSL = PROJECT + "/Source/Tools/MosinPortrait/mosin_head_nod.hlsl"
CAPTURE_DIR = PROJECT + "/Saved/MosinPortrait"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/MosinPortrait"
TEX = "T_Portrait_MosinRifleman"
MASK = "T_Portrait_MosinRifleman_Mask"
MAT = "M_UI_MosinHeadNod"
BRUSH = "SB_Portrait_MosinRifleman"
SIZE_BRUSH = "/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope"
PORTRAIT_BP = "/Game/RTS_Survival/Blueprints/GameUI/Announcer/W_BP_Portrait"
REF_TEX = "/Game/RTS_Survival/Textures/GameUI/Commanders/Portraits/T_Commander_Balanced"
TEX_PROPS = ("lod_group", "mip_gen_settings", "compression_settings", "srgb", "never_stream", "lod_bias")

# Anchors from CommanderPortraits/Renders/Mosin/Mosin_anchors.json in height units p = (u * Aspect, v).
RES = (1024, 1350)
ASPECT = RES[0] / RES[1]
VECTORS = {
    "Neck": (0.37925921404803237, 0.3961256146430969, 0.0, 0.0),
    "Eye0": (0.46389426902488423, 0.24841797351837158, 0.0, 0.0),
    "Eye0Axes": (-0.015090422453703702, -0.0008907318115234375, 0.0002995243778935185, -0.010163962841033936),
    "Eye1": (0.3893351236979167, 0.24390780925750732, 0.0, 0.0),
    "Eye1Axes": (-0.01549343532986111, -0.0009148120880126953, 0.00029296875, -0.010302364826202393),
}
SCALARS = {"Aspect": ASPECT, "Voice": 1.0, "Nod": 1.0, "Blink": 1.0, "TimeOffset": 0.0}

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
    mask.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    save(FOLDER + "/" + MASK)
    return tex, mask


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

    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1600, -700)
    tobj = lib.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1600, -900)
    tobj.set_editor_property("parameter_name", "Portrait")
    tobj.set_editor_property("texture", tex)
    tobj.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mobj = lib.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1600, -800)
    mobj.set_editor_property("parameter_name", "HeadMask")
    mobj.set_editor_property("texture", mask)
    mobj.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)

    seconds = lib.create_material_expression(material, unreal.MaterialExpressionTime, -1600, -550)
    seconds.set_editor_property("ignore_pause", True)
    seconds.set_editor_property("override_period", True)
    seconds.set_editor_property("period", 600.0)
    offset = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1600, -450)
    offset.set_editor_property("parameter_name", "TimeOffset")
    offset.set_editor_property("default_value", 0.0)
    t_add = lib.create_material_expression(material, unreal.MaterialExpressionAdd, -1350, -520)
    lib.connect_material_expressions(seconds, "", t_add, "A")
    lib.connect_material_expressions(offset, "", t_add, "B")

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -300)
    custom.set_editor_property("code", open(HLSL, encoding="utf8").read())
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "MosinHeadNod")
    names = ["Tex", "Mask", "UV", "T", "Aspect", "Neck", "Eye0", "Eye0Axes", "Eye1", "Eye1Axes", "Voice", "Nod", "Blink"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    lib.connect_material_expressions(tobj, "", custom, "Tex")
    lib.connect_material_expressions(mobj, "", custom, "Mask")
    lib.connect_material_expressions(uv, "", custom, "UV")
    lib.connect_material_expressions(t_add, "", custom, "T")

    y = -350
    for name, v in VECTORS.items():
        node = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1600, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(*v))
        src = node
        if name == "Neck":
            src = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1350, y)
            for ch in "rgba":
                src.set_editor_property(ch, ch in "rg")
            lib.connect_material_expressions(node, "", src, "")
            lib.connect_material_expressions(src, "", custom, name)
        else:
            # Vector parameters output RGB on the default pin; the alpha is needed for the axes (b.y).
            app = lib.create_material_expression(material, unreal.MaterialExpressionAppendVector, -1350, y)
            lib.connect_material_expressions(node, "", app, "A")
            lib.connect_material_expressions(node, "A", app, "B")
            lib.connect_material_expressions(app, "", custom, name)
        y += 120
    for name in ("Aspect", "Voice", "Nod", "Blink"):
        node = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1600, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", float(SCALARS[name]))
        lib.connect_material_expressions(node, "", custom, name)
        y += 70

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
    key = unreal.RTSPortraitTypes.SOVIET_MOSIN
    brushes = cdo.get_editor_property("PortraitBrushes")
    brushes[key] = brush_asset
    cdo.set_editor_property("PortraitBrushes", brushes)
    names = cdo.get_editor_property("SpeakerNames")
    if key not in names:
        names[key] = "Rifleman"   # the BP's map may predate the C++ default
        cdo.set_editor_property("SpeakerNames", names)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # save_asset(path) once returned without writing the file (2026-11-12): save the loaded object and check.
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
        raise RuntimeError("W_BP_Portrait did not save")
    check = unreal.get_default_object(bp.generated_class())
    return check.get_editor_property("PortraitBrushes")[key], check.get_editor_property("SpeakerNames")[key]


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
        out["mosin_now"] = str(point_portrait(existing(FOLDER + "/" + BRUSH)))
    print("MOSIN_HEAD", out)
    return out


def capture(times=(0.3, 1.0, 2.35, 2.91, 5.1, 7.9), size=(528, 696), prefix="head", voice=1.0, blink=1.0):
    """Draw the material onto a render target at several TimeOffsets and export PNGs into Saved/MosinPortrait.
    The editor clock is added to TimeOffset, so frames are relative to an unknown start: sweep offsets to compare."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world() or ues.get_game_world()
    rt = unreal.RenderingLibrary.create_render_target2d(world, size[0], size[1],
                                                        unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB,
                                                        unreal.LinearColor(0, 0, 0, 1))
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, existing(FOLDER + "/" + MAT))
    mid.set_scalar_parameter_value("Voice", voice)
    mid.set_scalar_parameter_value("Blink", blink)
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
    print("MOSIN_CAPTURE", files)
    return files


def save_all():
    """After PIE: write the assets (they were built while the editor was in play mode)."""
    return {n: save(FOLDER + "/" + n) for n in (TEX, MASK, MAT, BRUSH)}
