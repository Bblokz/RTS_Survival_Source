"""Splices MF_TeamColor_CamoSwap into the German vehicle materials and sets each camo's keys.

Run inside the open editor through the bridge, one stage per call (shader compiles happen between stages):
  TEAM_COLOR_STAGE = "function" | "masters" | "roots" | "instances" | "report"
  exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/TeamColor/rollout_team_color.py").read())
Inputs: Saved/TeamColorTest/team_color_targets_inspect.json and team_color_keys.json (inspect, export and pick scripts).
Re-running a stage is safe: existing calls and parameters are reused and rewired. Assets with unsaved edits in the
editor are skipped and reported, never saved over; TEAM_COLOR_OWN_DIRTY lists roots this rollout dirtied itself.
"""
import json

import unreal

SAVED = unreal.Paths.project_saved_dir() + "TeamColorTest/"
FUNCTION_FOLDER = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/TeamColor"
FUNCTION_NAME = "MF_TeamColor_CamoSwap"
FUNCTION_PATH = FUNCTION_FOLDER + "/" + FUNCTION_NAME
MEL = unreal.MaterialEditingLibrary
PARAMETER_GROUP = "TeamColor"
# The masters' hull camo sample feeds this reroute; the unit stamps and icons are added after it and stay untinted.
MASTER_DIFFUSE_REROUTE = "DiffuseTankHull"
TEAM_COLOR_PRIMITIVE_DATA_INDEX = 4
RED_BROWN_LINEAR = [0.19 ** 2.2, 0.085 ** 2.2, 0.002 ** 2.2]
DARK_GREEN_LINEAR = [0.07 ** 2.2, 0.10 ** 2.2, 0.005 ** 2.2]
DEFAULT_STRENGTH = 0.95

CAMO_SWAP_HLSL = """// Default look: without a team colour (alpha 0 on every enemy, neutral and unset primitive) the camo passes through
// untouched. Custom primitive data is uniform per draw, so this branch is coherent and the default costs nothing.
[branch] if (TeamWeight <= 0.0)
{
    return Base.rgb;
}
// Blend in a perceptual (gamma) space: the camo is mostly dark, linear maths would only darken it.
float3 S = pow(max(Base.rgb, 0.0), 1.0 / 2.2);
float Lum = dot(S, float3(0.299, 0.587, 0.114));
float Chroma = max(S.r, max(S.g, S.b)) - min(S.r, min(S.g, S.b));
// Painted plates are coloured; bare steel, grilles, tools and soot are grey.
float PaintMask = smoothstep(PaintMin, PaintMax, Chroma);
float3 Tc = pow(saturate(Team.rgb), 1.0 / 2.2);

// Key up to two camo bands by chromaticity (colour with brightness divided out), so wear and shading inside a band stay
// keyed. Per camo TC_KeyColor and TC_KeyColor2 pick the red-brown and dark green bands and leave the dunkelgelb base
// coat; KeyColor2Weight (TC_KeyColor2 alpha) switches the second band off on camos that have only one.
float3 Chromaticity = S / (S.r + S.g + S.b + 0.001);
float3 Key1S = pow(max(KeyColor.rgb, 0.0), 1.0 / 2.2);
float3 Key2S = pow(max(KeyColor2.rgb, 0.0), 1.0 / 2.2);
float Band1 = 1.0 - smoothstep(KeyMin, KeyMax, length(Chromaticity - Key1S / (Key1S.r + Key1S.g + Key1S.b + 0.001)));
float Band2 = (1.0 - smoothstep(KeyMin, KeyMax, length(Chromaticity - Key2S / (Key2S.r + Key2S.g + Key2S.b + 0.001))))
    * saturate(KeyColor2Weight);
float Mask = max(Band1, Band2) * PaintMask;

// Overlay the team colour on the camo's value so wear, grime and panel shading survive; Lift pulls the value to mid
// grey so dark camo still reads as the team colour.
float L2 = lerp(Lum, 0.5, Lift);
float3 Low = 2.0 * L2 * Tc;
float3 High = 1.0 - 2.0 * (1.0 - L2) * (1.0 - Tc);
float3 Painted = lerp(Low, High, step(0.5, L2));
float W = saturate(Strength * Mask * TeamWeight);
return pow(lerp(S, Painted, W), 2.2);
"""
CUSTOM_INPUTS = ["Base", "Team", "TeamWeight", "Strength", "Lift", "PaintMin", "PaintMax", "KeyMin", "KeyMax",
                 "KeyColor", "KeyColor2", "KeyColor2Weight"]
FUNCTION_SCALARS = {"TC_Lift": 0.45, "TC_PaintMin": 0.035, "TC_PaintMax": 0.10}
# Chromaticity distance from a key: fully repainted within the first, untouched beyond the second. Per material
# (TC_KeyMin / TC_KeyMax): pale camo whose blotches sit close to the base coat needs a tighter radius (Brummbar).
DEFAULT_KEY_RADIUS = (0.08, 0.15)
# Parameters the first function version kept inside the function; now function inputs fed per material.
REMOVED_FUNCTION_PARAMETERS = ["TC_KeyMin", "TC_KeyMax"]


def log(message):
    unreal.log(f"TeamColorRollout: {message}")


def expressions_of(outer, expression_class=unreal.MaterialExpression):
    return [expression for expression in unreal.ObjectIterator(expression_class) if expression.get_outer() == outer]


def get_dirty_package_names():
    return {package.get_name() for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}


def is_dirty(asset_path, dirty_packages):
    return asset_path.split(".")[0] in dirty_packages


def save(asset):
    return unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)


def linear_color(values, alpha=1.0):
    return unreal.LinearColor(values[0], values[1], values[2], alpha)


# ---------------------------------------------------------------- function

def make_function_input(function, name, input_type, sort_priority, preview, y):
    existing = [e for e in expressions_of(function, unreal.MaterialExpressionFunctionInput)
                if str(e.get_editor_property("input_name")) == name]
    node = existing[0] if existing else MEL.create_material_expression_in_function(
        function, unreal.MaterialExpressionFunctionInput, -900, y)
    node.set_editor_property("input_name", name)
    node.set_editor_property("input_type", input_type)
    node.set_editor_property("sort_priority", sort_priority)
    # Vector4f has no field accessors in Python; import its text form.
    preview_value = unreal.Vector4f()
    preview_value.import_text("(X={},Y={},Z={},W={})".format(*preview))
    node.set_editor_property("preview_value", preview_value)
    node.set_editor_property("use_preview_value_as_default", True)
    return node


def make_function_parameter(function, parameter_class, name, value, y):
    existing = [e for e in expressions_of(function, parameter_class)
                if str(e.get_editor_property("parameter_name")) == name]
    node = existing[0] if existing else MEL.create_material_expression_in_function(function, parameter_class, -900, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("group", PARAMETER_GROUP)
    node.set_editor_property("default_value", value)
    return node


def build_function():
    if not unreal.EditorAssetLibrary.does_asset_exist(FUNCTION_PATH):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            FUNCTION_NAME, FUNCTION_FOLDER, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    function = unreal.load_asset(FUNCTION_PATH)
    function.set_editor_property("description", "Team colour camo swap for German vehicles: repaints up to two camo "
                                 "bands with the owner's team colour from custom primitive data 4-7 (alpha 0 = the "
                                 "unchanged camo). See Docs/Rendering/TeamColorShaders.md.")
    vector3 = unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3
    scalar = unreal.FunctionInputType.FUNCTION_INPUT_SCALAR
    base = make_function_input(function, "BaseColor", vector3, 0, (0.5, 0.5, 0.5, 1.0), 0)
    key = make_function_input(function, "KeyColor", vector3, 1, (*RED_BROWN_LINEAR, 1.0), 150)
    key2 = make_function_input(function, "KeyColor2", vector3, 2, (*DARK_GREEN_LINEAR, 1.0), 300)
    key2_weight = make_function_input(function, "KeyColor2Weight", scalar, 3, (1.0, 0, 0, 0), 375)
    strength = make_function_input(function, "Strength", scalar, 4, (DEFAULT_STRENGTH, 0, 0, 0), 450)
    key_min = make_function_input(function, "KeyMin", scalar, 5, (DEFAULT_KEY_RADIUS[0], 0, 0, 0), 525)
    key_max = make_function_input(function, "KeyMax", scalar, 6, (DEFAULT_KEY_RADIUS[1], 0, 0, 0), 560)
    for node in expressions_of(function, unreal.MaterialExpressionScalarParameter):
        if str(node.get_editor_property("parameter_name")) in REMOVED_FUNCTION_PARAMETERS:
            MEL.delete_material_expression_in_function(function, node)

    team = make_function_parameter(function, unreal.MaterialExpressionVectorParameter, "TeamColor",
                                   unreal.LinearColor(0, 0, 0, 0), 600)
    team.set_editor_property("use_custom_primitive_data", True)
    team.set_editor_property("primitive_data_index", TEAM_COLOR_PRIMITIVE_DATA_INDEX)
    scalars = {}
    for index, (name, value) in enumerate(FUNCTION_SCALARS.items()):
        scalars[name] = make_function_parameter(function, unreal.MaterialExpressionScalarParameter, name, value,
                                                800 + index * 100)

    customs = expressions_of(function, unreal.MaterialExpressionCustom)
    custom = customs[0] if customs else MEL.create_material_expression_in_function(
        function, unreal.MaterialExpressionCustom, -400, 300)
    custom.set_editor_property("description", "TeamColor_CamoSwap")
    custom.set_editor_property("code", CAMO_SWAP_HLSL)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for name in CUSTOM_INPUTS:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    custom.set_editor_property("inputs", inputs)

    outputs = expressions_of(function, unreal.MaterialExpressionFunctionOutput)
    output = outputs[0] if outputs else MEL.create_material_expression_in_function(
        function, unreal.MaterialExpressionFunctionOutput, 0, 300)
    output.set_editor_property("output_name", "Result")

    # Setting the inputs array drops links, so every input is wired again.
    links = [MEL.connect_material_expressions(base, "", custom, "Base"),
             MEL.connect_material_expressions(team, "", custom, "Team"),
             MEL.connect_material_expressions(team, "A", custom, "TeamWeight"),
             MEL.connect_material_expressions(strength, "", custom, "Strength"),
             MEL.connect_material_expressions(key, "", custom, "KeyColor"),
             MEL.connect_material_expressions(key2, "", custom, "KeyColor2"),
             MEL.connect_material_expressions(key2_weight, "", custom, "KeyColor2Weight"),
             MEL.connect_material_expressions(key_min, "", custom, "KeyMin"),
             MEL.connect_material_expressions(key_max, "", custom, "KeyMax"),
             MEL.connect_material_expressions(custom, "", output, "")]
    for name in ["Lift", "PaintMin", "PaintMax"]:
        links.append(MEL.connect_material_expressions(scalars["TC_" + name], "", custom, name))
    MEL.update_material_function(function)
    log(f"function links={links} saved={save(function)}")
    return all(links)


# ---------------------------------------------------------------- roots

def find_function_call(material):
    for call in expressions_of(material, unreal.MaterialExpressionMaterialFunctionCall):
        function = call.get_editor_property("material_function")
        if function is not None and function.get_path_name().split(".")[0] == FUNCTION_PATH:
            return call
    return None


def find_master_reroute(material):
    for reroute in expressions_of(material, unreal.MaterialExpressionNamedRerouteDeclaration):
        if str(reroute.get_editor_property("name")) == MASTER_DIFFUSE_REROUTE:
            return reroute
    return None


def get_or_make_parameter(material, parameter_class, name, x, y):
    for node in expressions_of(material, parameter_class):
        if str(node.get_editor_property("parameter_name")) == name:
            return node
    node = MEL.create_material_expression(material, parameter_class, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("group", PARAMETER_GROUP)
    return node


def get_key_values(key_info):
    """@return (key 1 linear, key 2 linear, key 2 alpha, strength, key radius) from a team_color_keys.json entry."""
    keys = key_info.get("keys", [])
    strength = key_info.get("strength", 0.0) if keys else 0.0
    key1 = keys[0] if keys else RED_BROWN_LINEAR
    key2 = keys[1] if len(keys) > 1 else DARK_GREEN_LINEAR
    radius = key_info.get("key_radius", DEFAULT_KEY_RADIUS)
    return key1, key2, 1.0 if len(keys) > 1 else 0.0, strength, radius


def splice_root(material_path, key_info, is_master):
    material = unreal.load_asset(material_path)
    function = unreal.load_asset(FUNCTION_PATH)
    call = find_function_call(material)
    reroute = find_master_reroute(material) if is_master else None
    if reroute is not None:
        # The first input of the call is its BaseColor: the hull camo sample once spliced.
        source = MEL.get_inputs_for_material_expression(material, call if call is not None else reroute)[0]
        source_output = "RGB"
    else:
        node = MEL.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
        if node is None:
            return "no Base Color input"
        if call is not None and node == call:
            source = MEL.get_inputs_for_material_expression(material, call)[0]
            source_output = ""
        else:
            source = node
            source_output = MEL.get_material_property_input_node_output_name(
                material, unreal.MaterialProperty.MP_BASE_COLOR)
    x = source.get_editor_property("material_expression_editor_x") + 320
    y = source.get_editor_property("material_expression_editor_y")
    if call is None:
        call = MEL.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, x, y)
        call.set_material_function(function)
    else:
        MEL.update_material_function(function)
    key1, key2, key2_alpha, strength, radius = get_key_values(key_info)
    key_min_node = get_or_make_parameter(material, unreal.MaterialExpressionScalarParameter, "TC_KeyMin", x - 250,
                                         y + 640)
    key_min_node.set_editor_property("default_value", radius[0])
    key_max_node = get_or_make_parameter(material, unreal.MaterialExpressionScalarParameter, "TC_KeyMax", x - 250,
                                         y + 800)
    key_max_node.set_editor_property("default_value", radius[1])
    key_node = get_or_make_parameter(material, unreal.MaterialExpressionVectorParameter, "TC_KeyColor", x - 250, y + 160)
    key_node.set_editor_property("default_value", linear_color(key1))
    key2_node = get_or_make_parameter(material, unreal.MaterialExpressionVectorParameter, "TC_KeyColor2", x - 250,
                                      y + 320)
    key2_node.set_editor_property("default_value", linear_color(key2, key2_alpha))
    strength_node = get_or_make_parameter(material, unreal.MaterialExpressionScalarParameter, "TC_Strength", x - 250,
                                          y + 480)
    strength_node.set_editor_property("default_value", strength)
    links = [MEL.connect_material_expressions(source, source_output, call, "BaseColor"),
             MEL.connect_material_expressions(key_node, "", call, "KeyColor"),
             MEL.connect_material_expressions(key2_node, "", call, "KeyColor2"),
             # The alpha of TC_KeyColor2 switches the second band on.
             MEL.connect_material_expressions(key2_node, "A", call, "KeyColor2Weight"),
             MEL.connect_material_expressions(strength_node, "", call, "Strength"),
             MEL.connect_material_expressions(key_min_node, "", call, "KeyMin"),
             MEL.connect_material_expressions(key_max_node, "", call, "KeyMax")]
    if reroute is not None:
        links.append(MEL.connect_material_expressions(call, "Result", reroute, ""))
    else:
        links.append(MEL.connect_material_property(call, "Result", unreal.MaterialProperty.MP_BASE_COLOR))
    MEL.recompile_material(material)
    if not all(links):
        return f"links failed {links}"
    return f"spliced ({'before stamps' if reroute is not None else 'before Base Color'}) saved={save(material)}"


def splice_roots(only_masters):
    inspect, keys = load_inputs()
    dirty = get_dirty_package_names()
    # Optional name filter set by the caller, to re-key a few materials without re-saving all of them.
    only = globals().get("TEAM_COLOR_ONLY", [])
    for root_path, info in inspect["roots"].items():
        if info.get("is_master", False) != only_masters:
            continue
        if only and not any(root_path.endswith("/" + name) for name in only):
            continue
        if is_dirty(root_path, dirty) and root_path not in globals().get("TEAM_COLOR_OWN_DIRTY", []):
            log(f"SKIPPED {root_path}: unsaved edits in the editor")
            continue
        log(f"{root_path.rsplit('/', 1)[-1]}: {splice_root(root_path, keys.get(root_path, {}), only_masters)}")


# ---------------------------------------------------------------- instances

def set_instance_keys():
    inspect, keys = load_inputs()
    dirty = get_dirty_package_names()
    # Optional name filter set by the caller, to re-key a few instances without re-saving all of them.
    only = globals().get("TEAM_COLOR_ONLY", [])
    for instance_path in inspect["instances"]:
        if only and not any(instance_path.endswith("/" + name) for name in only):
            continue
        key_info = keys.get(instance_path, {})
        if key_info.get("inherit"):
            log(f"{instance_path.rsplit('/', 1)[-1]}: keeps the root's keys")
            continue
        if is_dirty(instance_path, dirty):
            log(f"SKIPPED {instance_path}: unsaved edits in the editor")
            continue
        instance = unreal.load_asset(instance_path)
        key1, key2, key2_alpha, strength, radius = get_key_values(key_info)
        MEL.set_material_instance_vector_parameter_value(instance, "TC_KeyColor", linear_color(key1))
        MEL.set_material_instance_vector_parameter_value(instance, "TC_KeyColor2", linear_color(key2, key2_alpha))
        MEL.set_material_instance_scalar_parameter_value(instance, "TC_Strength", strength)
        MEL.set_material_instance_scalar_parameter_value(instance, "TC_KeyMin", radius[0])
        MEL.set_material_instance_scalar_parameter_value(instance, "TC_KeyMax", radius[1])
        MEL.update_material_instance(instance)
        log(f"{instance_path.rsplit('/', 1)[-1]}: {key_info.get('reason')} saved={save(instance)}")


def report():
    inspect, _ = load_inputs()
    for root_path, info in inspect["roots"].items():
        material = unreal.load_asset(root_path)
        statistics = MEL.get_statistics(material)
        log(f"{root_path.rsplit('/', 1)[-1]}: spliced={find_function_call(material) is not None} "
            f"ps={statistics.num_pixel_shader_instructions} samplers={statistics.num_samplers}")


def load_inputs():
    with open(SAVED + "team_color_targets_inspect.json", encoding="utf-8") as handle:
        inspect = json.load(handle)
    with open(SAVED + "team_color_keys.json", encoding="utf-8") as handle:
        keys = json.load(handle)
    return inspect, keys


STAGES = {"function": build_function, "masters": lambda: splice_roots(True),
          "roots": lambda: splice_roots(False), "instances": set_instance_keys, "report": report}
STAGES[TEAM_COLOR_STAGE]()  # noqa: F821 - set by the caller before exec
