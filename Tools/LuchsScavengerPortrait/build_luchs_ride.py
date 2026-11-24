"""Builds the GermanLuchsScavenger "Ruetteln" (ride jolt + background flow + head nod + rag flutter + filter breath)
portrait.
UECP execute_python:
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/LuchsScavengerPortrait/build_luchs_ride.py").read(), ns); ns["run"]()

  T_Portrait_LuchsScavenger       Blender render (CommanderPortraits/Tools/lighttank_fahrtwind_portrait.py), 1024 x 1350
                                  = the PortraitImage aspect 176 x 232
  T_Portrait_LuchsScavenger_Mask  mask pass: R = rags, G = follows the head (head bone weight, headset, goggles),
                                  B = background (everything that is not the crewman or the tank). sRGB off, BC1 masks
  M_UI_LuchsScavengerRide         UI material (Custom node = luchs_ride.hlsl), pause-ignoring Time with a 600 s period
                                  (+ TimeOffset for captures); opacity = VertexColor.a only (widget fade-in)
  SB_Portrait_LuchsScavenger      Slate brush drawing the material
run('point') sets W_BP_Portrait.PortraitBrushes[GermanLuchsScavenger] (needs the C++ build with the new enum value).
"""
import json

import unreal

PROJECT = "D:/UE5Projects/RTS_Survival"
RENDERS = PROJECT + "/CommanderPortraits/Renders/LightTankA/"
HLSL = PROJECT + "/Source/Tools/LuchsScavengerPortrait/luchs_ride.hlsl"
CAPTURE_DIR = PROJECT + "/Saved/LightTankPortrait"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/LuchsScavengerPortrait"
TEX = "T_Portrait_LuchsScavenger"
MASK = "T_Portrait_LuchsScavenger_Mask"
SOURCES = {TEX: "Fahrtwind.png", MASK: "Fahrtwind_Mask.png"}
MAT = "M_UI_LuchsScavengerRide"
BRUSH = "SB_Portrait_LuchsScavenger"
SIZE_BRUSH = "/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope"
PORTRAIT_BP = "/Game/RTS_Survival/Blueprints/GameUI/Announcer/W_BP_Portrait"
REF_TEX = "/Game/RTS_Survival/Textures/GameUI/Commanders/Portraits/T_Commander_Balanced"
TEX_PROPS = ("lod_group", "mip_gen_settings", "compression_settings", "srgb", "never_stream", "lod_bias")
SPEAKER = "Luchs Commander"


def anchors():
    """Material vectors from the render's anchors, in height units p = (u * Aspect, v). The vanishing point (where the
    background streams to) sits behind him along the projected travel direction, as in the preview page."""
    an = json.load(open(RENDERS + "Fahrtwind_anchors.json"))
    aspect = an["res"][0] / an["res"][1]

    def hu(uv):
        return (uv[0] * aspect, uv[1])

    vp = (an["neck"][0] + an["travel_screen"][0] * 2.2, an["neck"][1] + an["travel_screen"][1] * 2.2 - 0.12)
    return aspect, {"NeckVP": hu(an["neck"]) + hu(vp), "Rag": hu(an["rag_root"]) + hu(an["rag_tip"]),
                    "Filters": hu(an["filters"][0]) + hu(an["filters"][1])}


ASPECT, VECTORS = anchors()
SCALARS = {"Aspect": ASPECT, "Voice": 1.0, "Jolt": 1.0, "Nod": 1.0, "Flow": 1.0, "Flutter": 1.0, "Breath": 1.0,
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
    return unreal.EditorAssetLibrary.save_loaded_asset(existing(path), only_if_is_dirty=False)


def _import(name):
    t = unreal.AssetImportTask()
    t.filename = RENDERS + SOURCES[name]
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
    out = {}
    for name in (TEX,):
        out[name] = _import(name)
        save(FOLDER + "/" + name)
    mask = _import(MASK)
    mask.set_editor_property("srgb", False)
    mask.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    save(FOLDER + "/" + MASK)
    out[MASK] = mask
    return out


def _tex_param(material, name, tex, y, masks=False):
    node = lib.create_material_expression(material, unreal.MaterialExpressionTextureObjectParameter, -1600, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("texture", tex)
    node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if masks
                             else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    return node


def build_material(texs):
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

    custom = lib.create_material_expression(material, unreal.MaterialExpressionCustom, -500, -300)
    custom.set_editor_property("code", open(HLSL, encoding="utf8").read())
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", "LuchsScavengerRide")
    names = ["Tex", "Mask", "UV", "T", "Aspect", "NeckVP", "Rag", "Voice", "Jolt", "Nod", "Flow", "Flutter",
             "Filters", "Breath"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    for i, (pin, pname, key, masks) in enumerate((("Tex", "Portrait", TEX, False), ("Mask", "RideMask", MASK, True))):
        node = _tex_param(material, pname, texs[key], -1200 + i * 100, masks)
        lib.connect_material_expressions(node, "", custom, pin)
    uv = lib.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1600, -780)
    lib.connect_material_expressions(uv, "", custom, "UV")
    seconds = lib.create_material_expression(material, unreal.MaterialExpressionTime, -1600, -700)
    seconds.set_editor_property("ignore_pause", True)
    seconds.set_editor_property("override_period", True)
    seconds.set_editor_property("period", 600.0)
    offset = lib.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1600, -620)
    offset.set_editor_property("parameter_name", "TimeOffset")
    offset.set_editor_property("default_value", 0.0)
    t_add = lib.create_material_expression(material, unreal.MaterialExpressionAdd, -1350, -660)
    lib.connect_material_expressions(seconds, "", t_add, "A")
    lib.connect_material_expressions(offset, "", t_add, "B")
    lib.connect_material_expressions(t_add, "", custom, "T")

    y = -520
    for name, v in VECTORS.items():
        node = lib.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1600, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(*v))
        if name == "Neck":
            src = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1350, y)
            for ch in "rgba":
                src.set_editor_property(ch, ch in "rg")
            lib.connect_material_expressions(node, "", src, "")
        else:
            # Vector parameters output RGB on the default pin; the alpha carries the fourth component.
            src = lib.create_material_expression(material, unreal.MaterialExpressionAppendVector, -1350, y)
            lib.connect_material_expressions(node, "", src, "A")
            lib.connect_material_expressions(node, "A", src, "B")
        lib.connect_material_expressions(src, "", custom, name)
        y += 120
    for name in ("Aspect", "Voice", "Jolt", "Nod", "Flow", "Flutter", "Breath"):
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
    key = unreal.RTSPortraitTypes.GERMAN_LUCHS_SCAVENGER
    brushes = cdo.get_editor_property("PortraitBrushes")
    brushes[key] = brush_asset
    cdo.set_editor_property("PortraitBrushes", brushes)
    names = cdo.get_editor_property("SpeakerNames")
    if key not in names:
        names[key] = SPEAKER   # the BP's map may predate the C++ default
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
        texs = import_textures()
    else:
        texs = {n: existing(FOLDER + "/" + n) for n in (TEX, MASK)}
    out["textures"] = {n: "%dx%d" % (t.blueprint_get_size_x(), t.blueprint_get_size_y()) for n, t in texs.items()}
    if step in ("all", "material"):
        mat = build_material(texs)
        out["material"] = mat.get_path_name()
        out["brush"] = build_brush(mat).get_path_name()
    if step in ("all", "point"):
        out["luchs_now"] = str(point_portrait(existing(FOLDER + "/" + BRUSH)))
    print("LUCHS_RIDE", out)
    return out


def capture(times=(0.3, 1.0, 2.35, 2.91, 5.1, 7.9), size=(528, 696), prefix="ride", **params):
    """Draw the material onto a render target at several TimeOffsets and export PNGs into Saved/LightTankPortrait.
    The editor clock is added to TimeOffset, so frames are relative to an unknown start: sweep offsets to compare."""
    ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = ues.get_editor_world() or ues.get_game_world()
    rt = unreal.RenderingLibrary.create_render_target2d(world, size[0], size[1],
                                                        unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB,
                                                        unreal.LinearColor(0, 0, 0, 1))
    mid = unreal.MaterialLibrary.create_dynamic_material_instance(world, existing(FOLDER + "/" + MAT))
    for k, v in params.items():
        mid.set_scalar_parameter_value(k, float(v))
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
    print("LUCHS_RIDE_CAPTURE", files)
    return files
