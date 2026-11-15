"""Builds the SovietIS2Driver "Pribory" (the instruments) portrait: the IS-2 driver at his station; while he talks the
panel light pulses on his face, the dials wobble and the station around him trembles, and his head never moves.
UECP execute_python (or a throwaway editor, see is2_driver_build_headless.py):
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/IS2DriverPortrait/build_is2_driver.py").read(), ns); ns["run"]()

  T_Portrait_IS2Driver           Blender render (CommanderPortraits/Tools/is2_driver_portrait.py), 1024 x 1350
                                 = the PortraitImage aspect 176 x 232
  T_Portrait_IS2Driver_PanelOff  the same render with the instrument panel unlit (its difference = the panel light)
  T_Portrait_IS2Driver_Mask      pack_is2_driver_mask.py: R = gauge faces, G = tremor weight (0 on the man, 0 on his
                                 head). sRGB off, BC1 masks
  T_Portrait_IS2Driver_Swing     the hanging canteen alone (pack_is2_driver_mask.py: premultiplied RGBA), swung
                                 about its hook by the material
  M_UI_IS2DriverPribory          UI material (Custom node = is2_driver_pribory.hlsl), pause-ignoring Time with a 600 s
                                 period (+ TimeOffset for captures); opacity = VertexColor.a only (widget fade-in)
  SB_Portrait_IS2Driver          Slate brush drawing the material
run('point') sets W_BP_Portrait.PortraitBrushes[SovietIS2Driver] (needs the C++ build with the new enum value).
"""
import json

import unreal

PROJECT = "D:/UE5Projects/RTS_Survival"
RENDERS = PROJECT + "/CommanderPortraits/Renders/IS2Driver/"
HLSL = PROJECT + "/Source/Tools/IS2DriverPortrait/is2_driver_pribory.hlsl"
CAPTURE_DIR = PROJECT + "/Saved/IS2Portrait"
FOLDER = "/Game/RTS_Survival/Textures/GameUI/IS2DriverPortrait"
TEX = "T_Portrait_IS2Driver"
PANEL_OFF = "T_Portrait_IS2Driver_PanelOff"
MASK = "T_Portrait_IS2Driver_Mask"
SWING = "T_Portrait_IS2Driver_Swing"
SOURCES = {TEX: "IS2Driver.png", PANEL_OFF: "IS2Driver_PanelOff.png", MASK: "IS2Driver_TalkMask.png",
           SWING: "IS2Driver_SwingPremul.png"}
MAT = "M_UI_IS2DriverPribory"
BRUSH = "SB_Portrait_IS2Driver"
SIZE_BRUSH = "/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope"
PORTRAIT_BP = "/Game/RTS_Survival/Blueprints/GameUI/Announcer/W_BP_Portrait"
REF_TEX = "/Game/RTS_Survival/Textures/GameUI/Commanders/Portraits/T_Commander_Balanced"
TEX_PROPS = ("lod_group", "mip_gen_settings", "compression_settings", "srgb", "never_stream", "lod_bias")
SPEAKER = "IS-2 Driver"
ENUM_KEY = "SOVIET_IS2_DRIVER"


def anchors():
    """Material vectors from the render's anchors, in height units p = (u * Aspect, v): the five gauge centres."""
    an = json.load(open(RENDERS + "IS2Driver_anchors.json"))
    aspect = an["res"][0] / an["res"][1]

    def hu(uv):
        return (uv[0] * aspect, uv[1])

    g = [hu(p) for p in an["gauges"]]
    return aspect, {"G01": g[0] + g[1], "G23": g[2] + g[3], "G4": g[4] + (0.0, 0.0),
                    "SwingPivot": hu(an["swing_pivot"]) + hu(an["swing_c"])}


ASPECT, VECTORS = anchors()
SCALARS = {"Aspect": ASPECT, "Voice": 1.0, "Tremor": 1.0, "Needles": 1.0, "Glow": 1.0, "Sway": 1.0, "TimeOffset": 0.0}

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
    for name in (TEX, PANEL_OFF, SWING):
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
    custom.set_editor_property("description", "IS2DriverPribory")
    names = ["Tex", "PanelOff", "Mask", "Swing", "UV", "T", "Aspect", "G01", "G23", "G4", "SwingPivot", "Voice", "Tremor",
             "Needles", "Glow", "Sway"]
    inputs = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)

    for i, (pin, pname, key, masks) in enumerate((("Tex", "Portrait", TEX, False), ("PanelOff", "PanelOff", PANEL_OFF, False),
                                                  ("Mask", "TalkMask", MASK, True), ("Swing", "Canteen", SWING, False))):
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
        # Vector parameters output RGB on the default pin; the alpha carries the fourth component.
        src = lib.create_material_expression(material, unreal.MaterialExpressionAppendVector, -1350, y)
        lib.connect_material_expressions(node, "", src, "A")
        lib.connect_material_expressions(node, "A", src, "B")
        lib.connect_material_expressions(src, "", custom, name)
        y += 120
    for name in ("Aspect", "Voice", "Tremor", "Needles", "Glow", "Sway"):
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
    key = getattr(unreal.RTSPortraitTypes, ENUM_KEY)
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
    # "assets" = textures + material + brush, without touching W_BP_Portrait.
    if step in ("all", "texture", "assets"):
        texs = import_textures()
    else:
        texs = {n: existing(FOLDER + "/" + n) for n in (TEX, PANEL_OFF, MASK, SWING)}
    out["textures"] = {n: "%dx%d" % (t.blueprint_get_size_x(), t.blueprint_get_size_y()) for n, t in texs.items()}
    if step in ("all", "material", "assets"):
        mat = build_material(texs)
        out["material"] = mat.get_path_name()
        out["brush"] = build_brush(mat).get_path_name()
    if step in ("all", "point"):
        out["is2_now"] = str(point_portrait(existing(FOLDER + "/" + BRUSH)))
    print("IS2_DRIVER", out)
    return out


def capture(times=(0.3, 1.0, 2.35, 2.91, 5.1, 7.9), size=(528, 696), prefix="pribory", **params):
    """Draw the material onto a render target at several TimeOffsets and export PNGs into Saved/IS2Portrait.
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
    print("IS2_DRIVER_CAPTURE", files)
    return files
