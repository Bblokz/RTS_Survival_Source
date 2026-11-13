"""Hanging wires, curtains, shutter, lamp and the perched crow for Ger_Berlin (headless, idempotent).

    sh Source/Tools/BerlinMap/ambient/run_headless.sh D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ambient/ambient_dressing_build.py dressing

Source meshes: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinAmbient/FBX (generate_berlin_ambient.py).
Builds, under /Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/Dressing:
  SM_BerlinAmb_*              9 meshes, no collision, pivot at the attachment
  T_BerlinAmbient_Cloth       curtain cloth, torn hem in alpha
  M_BerlinAmbient_Dressing    one master: tint or texture, static switches MotionFlutter / MotionSway that call the
                              project's MF_BF_Flutter / MF_BF_Sway (BreakthroughShared/Motion, used as they are)
  MI_BerlinAmb_Cloth / _Wire / _Rigid
  M_BerlinAmbient_Crow        perched crow: two flat colours picked by UV0.x, head nod in the vertex shader,
                              per-instance custom data 0 phase seconds, 1 rate, 2 airborne (1 folds the bird away)
No shared asset is modified: the motion functions are only called.
"""
import json

import unreal

MODEL = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinAmbient/FBX/"
TEXTURE_DIR = "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/textures/"
FOLDER = "/Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/Dressing"
MOTION = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/Motion/"
REPORT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/ambient/dressing_build_report.json"
MESHES = {  # mesh: material instance
    "SM_BerlinAmb_WireSpan_A": "MI_BerlinAmb_Wire", "SM_BerlinAmb_WireSpan_B": "MI_BerlinAmb_Wire",
    "SM_BerlinAmb_CableDangle_A": "MI_BerlinAmb_Wire", "SM_BerlinAmb_CableDangle_B": "MI_BerlinAmb_Wire",
    "SM_BerlinAmb_Curtain_A": "MI_BerlinAmb_Cloth", "SM_BerlinAmb_Curtain_B": "MI_BerlinAmb_Cloth",
    "SM_BerlinAmb_Shutter": "MI_BerlinAmb_Rigid", "SM_BerlinAmb_Lamp": "MI_BerlinAmb_Rigid",
    "SM_BerlinAmb_CrowPerched": "M_BerlinAmbient_Crow",
}
CROW_INSTANCE_DATA = {"PhaseSeconds": 0, "PlayRate": 1, "Airborne": 2}      # keep in step with the future crow manager

MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
FLOAT3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3

CROW_WPO = """
float t = (T + Ph) * Rate + Seed;
// the head holds, turns, holds; now and then a quick dip
float look = sin(t * 0.7) * sin(t * 0.23 + 1.3);
look = sign(look) * smoothstep(0.15, 0.55, abs(look));
float nod = pow(saturate(sin(t * 0.9 + 2.0)), 12.0);
float3 r = L - float3(11.0, 0.0, 23.0);
float yaw = look * 0.7 * W.x;
float pitch = nod * 0.55 * W.x;
float3 p = float3(r.x * cos(yaw) - r.y * sin(yaw), r.x * sin(yaw) + r.y * cos(yaw), r.z);
p = float3(p.x * cos(pitch) + p.z * sin(pitch), p.y, -p.x * sin(pitch) + p.z * cos(pitch));
return lerp(p - r, -L, saturate(Hidden));
"""
SEED = "return frac(sin(dot(O.xy, float2(0.1291, 0.7823))) * 43758.5453) * 40.0;"


def log(text):
    unreal.log("AMB_DRESSING " + str(text))


def save(asset):
    ok = ASSETS.save_loaded_asset(asset, False)
    log("saved %s %s" % (asset.get_path_name().split(".")[0], ok))
    return ok


def node(material, cls, x, y, **props):
    e = MEL.create_material_expression(material, getattr(unreal, "MaterialExpression" + cls), x, y)
    for key, value in props.items():
        e.set_editor_property(key, value)
    return e


def link(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        raise RuntimeError("link failed %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def wire_wpo(material, expression):
    """Python's MaterialProperty has no World Position Offset; the project's editor library wires it."""
    lib = unreal.NomadicDeployEditorLibrary
    if not lib.connect_world_position_offset(expression, ""):
        raise RuntimeError("World Position Offset not wired on " + material.get_name())
    lib.refresh_material(material)
    MEL.recompile_material(material)
    log("%s WPO: %s" % (material.get_name(), lib.describe_world_position_offset(material)))


def fresh_material(name):
    path = FOLDER + "/" + name
    if ASSETS.does_asset_exist(path):
        material = unreal.load_asset(path)
        for _ in range(20):
            if MEL.get_num_material_expressions(material) == 0:
                break
            MEL.delete_all_material_expressions(material)
        return material
    return TOOLS.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def custom(material, code, inputs, x, y, description):
    e = node(material, "Custom", x, y, code=code, output_type=FLOAT3, description=description)
    pins = []
    for name, _source, _out in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    e.set_editor_property("inputs", pins)
    for name, source, out in inputs:
        link(source, out, e, name)
    return e


# ---- textures and meshes -------------------------------------------------------------------------------------------------
def import_texture(name):
    task = unreal.AssetImportTask()
    task.filename = TEXTURE_DIR + name + ".png"
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    texture = unreal.load_asset(FOLDER + "/" + name)
    texture.set_editor_property("srgb", True)
    save(texture)
    return texture


def import_mesh(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + ".FBX"
    task.destination_path = FOLDER
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    TOOLS.import_asset_tasks([task])


def finish_mesh(name, material):
    mesh = unreal.load_asset(FOLDER + "/" + name)
    if mesh is None:
        raise RuntimeError("import made nothing for " + name)
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    mesh.set_material(0, material)
    sms.remove_collisions(mesh)
    body = mesh.get_editor_property("body_setup")
    if body is not None:
        response = body.get_editor_property("default_instance")
        response.set_editor_property("collision_profile_name", "NoCollision")
        body.set_editor_property("default_instance", response)
    # the vertex shader moves vertices up to about a metre out of the rest bounds: pad them, or the mesh pops at screen edges
    pad = unreal.Vector(70.0, 70.0, 30.0) if "Crow" not in name else unreal.Vector(10.0, 10.0, 10.0)
    mesh.set_editor_property("positive_bounds_extension", pad)
    mesh.set_editor_property("negative_bounds_extension", pad)
    mesh.modify()
    save(mesh)
    box = mesh.get_bounding_box()
    return dict(verts=sms.get_number_verts(mesh, 0), tris=mesh.get_num_triangles(0), uv_channels=sms.get_num_uv_channels(mesh, 0),
                bounds=[round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
                material=material.get_name())


# ---- materials ---------------------------------------------------------------------------------------------------------
def build_dressing_master(cloth):
    m = fresh_material("M_BerlinAmbient_Dressing")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    tint = node(m, "VectorParameter", -1100, -100, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    texture = node(m, "TextureSampleParameter2D", -1100, 150, parameter_name="BaseTexture", texture=cloth)
    textured = node(m, "Multiply", -800, 100)
    link(texture, "RGB", textured, "A")
    link(tint, "", textured, "B")
    use_color = node(m, "StaticSwitchParameter", -500, 0, parameter_name="UseTexture", default_value=False)
    link(textured, "", use_color, "True")
    link(tint, "", use_color, "False")
    one = node(m, "Constant", -800, 350, r=1.0)
    use_mask = node(m, "StaticSwitchParameter", -500, 300, parameter_name="UseTexture", default_value=False)
    link(texture, "A", use_mask, "True")
    link(one, "", use_mask, "False")
    rough = node(m, "ScalarParameter", -500, 500, parameter_name="Roughness", default_value=0.85)
    total = None
    for index, (switch_name, function_name) in enumerate((("MotionFlutter", "MF_BF_Flutter"), ("MotionSway", "MF_BF_Sway"))):
        y = 800 + 260 * index
        call = node(m, "MaterialFunctionCall", -1100, y)
        call.set_material_function(unreal.load_asset(MOTION + function_name))
        zero = node(m, "Constant3Vector", -1100, y + 120, constant=unreal.LinearColor(0, 0, 0, 0))
        if function_name == "MF_BF_Sway":       # MotionWeights.x: 1 swings, 0 stays (the lamp's wall bracket)
            weights = node(m, "TextureCoordinate", -1500, y, coordinate_index=2)
            weight = node(m, "ComponentMask", -1300, y, r=True, g=False, b=False, a=False)
            link(weights, "", weight, "")
            link(weight, "", call, "Weight")
        switch = node(m, "StaticSwitchParameter", -800, y, parameter_name=switch_name, default_value=False)
        link(call, "WPO", switch, "True")
        link(zero, "", switch, "False")
        if total is None:
            total = switch
        else:
            add = node(m, "Add", -500, y)
            link(total, "", add, "A")
            link(switch, "", add, "B")
            total = add
    MEL.connect_material_property(use_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(use_mask, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    wire_wpo(m, total)
    save(m)
    return m


def instance(name, parent, switches, scalars, tint, opaque):
    path = FOLDER + "/" + name
    if not ASSETS.does_asset_exist(path):
        TOOLS.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    MEL.set_material_instance_parent(mi, parent)
    for switch in switches:
        MEL.set_material_instance_static_switch_parameter_value(mi, switch, True)
    for key, value in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, key, value)     # its return value is not reliable
        back = MEL.get_material_instance_scalar_parameter_value(mi, key)
        if abs(back - value) > 1e-4:
            log("SCALAR NOT SET %s on %s: reads %s" % (key, name, back))
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint))
    if opaque:          # wires and the rigid pieces need no mask: opaque keeps them in the cheapest depth path
        try:
            overrides = mi.get_editor_property("base_property_overrides")
            overrides.set_editor_property("override_blend_mode", True)
            overrides.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
            mi.set_editor_property("base_property_overrides", overrides)
        except Exception as error:  # noqa: BLE001
            log("blend override failed on %s: %s" % (name, error))
    MEL.update_material_instance(mi)
    save(mi)
    return mi


def build_crow_material():
    m = fresh_material("M_BerlinAmbient_Crow")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("used_with_instanced_static_meshes", True)
    black = node(m, "VectorParameter", -900, -200, parameter_name="BlackPlumage", default_value=unreal.LinearColor(0.012, 0.012, 0.014, 1))
    grey = node(m, "VectorParameter", -900, 0, parameter_name="GreyPlumage", default_value=unreal.LinearColor(0.16, 0.155, 0.15, 1))
    uv = node(m, "TextureCoordinate", -900, 200, coordinate_index=0)
    mask = node(m, "ComponentMask", -700, 200, r=True, g=False, b=False, a=False)
    link(uv, "", mask, "")
    color = node(m, "LinearInterpolate", -450, -50)
    link(black, "", color, "A")
    link(grey, "", color, "B")
    link(mask, "", color, "Alpha")
    rough = node(m, "ScalarParameter", -450, 200, parameter_name="Roughness", default_value=0.5)

    def instance_data(index, default, y):
        e = node(m, "PerInstanceCustomData", -1100, y, data_index=index)
        link(node(m, "Constant", -1300, y, r=default), "", e, "DefaultValue")
        return e

    seed = custom(m, SEED, [("O", node(m, "ObjectPositionWS", -1300, 500), "")], -1100, 500, "crow seed")
    seed.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inputs = [("L", node(m, "LocalPosition", -1100, 600), ""), ("W", node(m, "TextureCoordinate", -1100, 700, coordinate_index=2), ""),
              ("T", node(m, "Time", -1100, 800), ""),
              ("Ph", instance_data(CROW_INSTANCE_DATA["PhaseSeconds"], 0.0, 900), ""),
              ("Rate", instance_data(CROW_INSTANCE_DATA["PlayRate"], 1.0, 1000), ""),
              ("Hidden", instance_data(CROW_INSTANCE_DATA["Airborne"], 0.0, 1100), ""), ("Seed", seed, "")]
    local = custom(m, CROW_WPO, inputs, -750, 700, "crow head nod + airborne fold")
    world = node(m, "Transform", -450, 700, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                 transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(local, "", world, "")
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    wire_wpo(m, world)
    save(m)
    return m


def run():
    report = {"meshes": {}, "kit_bounds": {}}
    cloth = import_texture("T_BerlinAmbient_Cloth")
    master = build_dressing_master(cloth)
    materials = {
        "MI_BerlinAmb_Cloth": instance("MI_BerlinAmb_Cloth", master, ["UseTexture", "MotionFlutter"],
                                       {"FlutterSwayCm": 55.0, "FlutterLiftCm": 10.0, "FlutterSpeed": 1.1}, (1, 1, 1, 1), False),
        "MI_BerlinAmb_Wire": instance("MI_BerlinAmb_Wire", master, ["MotionFlutter"],
                                      {"FlutterSwayCm": 24.0, "FlutterLiftCm": 7.0, "FlutterSpeed": 0.8, "Roughness": 0.6},
                                      (0.03, 0.028, 0.026, 1), True),
        "MI_BerlinAmb_Rigid": instance("MI_BerlinAmb_Rigid", master, ["MotionSway"],
                                       {"SwayAmpDeg": 9.0, "SwayRate": 0.3, "Roughness": 0.7}, (0.07, 0.085, 0.07, 1), True),
        "M_BerlinAmbient_Crow": build_crow_material(),
    }
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        for name in MESHES:
            import_mesh(name)
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")
    for name, material_name in MESHES.items():
        report["meshes"][name] = finish_mesh(name, materials[material_name])
    # which way round do the kit pieces come in? (Blender Y against Unreal Y, read by kit_attach_probe users)
    for piece in ("B2_Facade2St_B", "E1_CornerTower", "B5_CornerL"):
        mesh = unreal.load_asset("/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Meshes/SM_BerlinRuin_" + piece)
        if mesh is not None:
            box = mesh.get_bounding_box()
            report["kit_bounds"][piece] = [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)]
    with open(REPORT, "w") as handle:
        json.dump(report, handle, indent=1)
    log("REPORT " + json.dumps(report))
    log("DONE")


run()
