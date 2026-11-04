"""Read-only look at every team colour target before the rollout (run in the editor through the bridge).

For each root: blend mode, material attributes, what feeds Base Color, the textures upstream of it, and any custom
primitive data parameters (indices 4-7 must be free). For each instance of a root: its texture parameter values.
Writes Saved/TeamColorTest/team_color_targets_inspect.json.
"""
import json
import os
import sys

import unreal

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
import team_color_targets as targets  # noqa: E402

SAVED = os.path.join(unreal.Paths.project_saved_dir(), "TeamColorTest")
SURVEY_FILE = os.path.join(SAVED, "german_vehicle_materials.json")
OUTPUT_FILE = os.path.join(SAVED, "team_color_targets_inspect.json")
MEL = unreal.MaterialEditingLibrary


def get_expressions(material):
    found = []
    for expression in unreal.ObjectIterator(unreal.MaterialExpression):
        if expression.get_outer() == material:
            found.append(expression)
    return found


def get_upstream_textures(material, expression, seen):
    if expression is None or expression in seen:
        return []
    seen.add(expression)
    textures = []
    if isinstance(expression, unreal.MaterialExpressionTextureBase):
        texture = expression.get_editor_property("texture")
        name = str(expression.get_editor_property("parameter_name")) if isinstance(
            expression, unreal.MaterialExpressionTextureSampleParameter) else ""
        textures.append({"node": expression.get_name(), "parameter": name,
                         "texture": texture.get_path_name() if texture else "None"})
    for upstream in MEL.get_inputs_for_material_expression(material, expression):
        textures += get_upstream_textures(material, upstream, seen)
    return textures


def get_primitive_data_parameters(material):
    found = []
    for expression in get_expressions(material):
        if isinstance(expression, (unreal.MaterialExpressionScalarParameter, unreal.MaterialExpressionVectorParameter)):
            if expression.get_editor_property("use_custom_primitive_data"):
                found.append({"parameter": str(expression.get_editor_property("parameter_name")),
                              "index": expression.get_editor_property("primitive_data_index")})
        elif "PrimitiveData" in expression.get_class().get_name():
            found.append({"node": expression.get_class().get_name()})
    return found


def inspect_root(root_path):
    material = unreal.load_asset(root_path)
    if not isinstance(material, unreal.Material):
        return {"error": "not a material"}
    base_color = MEL.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    return {
        "blend_mode": str(material.get_editor_property("blend_mode")),
        "material_attributes": material.get_editor_property("use_material_attributes"),
        "base_color_node": base_color.get_name() if base_color else "None",
        "base_color_node_class": base_color.get_class().get_name() if base_color else "None",
        "base_color_output": MEL.get_material_property_input_node_output_name(
            material, unreal.MaterialProperty.MP_BASE_COLOR) if base_color else "",
        "textures": get_upstream_textures(material, base_color, set()),
        "primitive_data": get_primitive_data_parameters(material),
        "pixel_instructions": MEL.get_statistics(material).num_pixel_shader_instructions,
    }


def inspect_instance(instance_path):
    instance = unreal.load_asset(instance_path)
    textures = {}
    for parameter in MEL.get_texture_parameter_names(instance):
        value = MEL.get_material_instance_texture_parameter_value(instance, parameter)
        textures[str(parameter)] = value.get_path_name() if value else "None"
    return {"textures": textures}


def resolve_roots(survey):
    by_name = {}
    for info in survey["materials"].values():
        root = info.get("root", "None").split(".")[0]
        by_name[root.rsplit("/", 1)[-1]] = root
    roots = {}
    for path in targets.MASTER_ROOTS + targets.STANDALONE_ROOTS:
        name = path.rsplit("/", 1)[-1]
        roots[path] = by_name.get(name, path)
    return roots


def main():
    unreal.uecp.allow_bulk_loads() if hasattr(unreal, "uecp") else None
    with open(SURVEY_FILE, encoding="utf-8") as handle:
        survey = json.load(handle)
    result = {"roots": {}, "instances": {}}
    for listed_path, root_path in resolve_roots(survey).items():
        result["roots"][root_path] = inspect_root(root_path)
        result["roots"][root_path]["listed_as"] = listed_path
        result["roots"][root_path]["is_master"] = listed_path in targets.MASTER_ROOTS
    root_set = set(result["roots"])
    for material_path, info in survey["materials"].items():
        root = info.get("root", "None").split(".")[0]
        if root in root_set and info.get("chain"):
            instance_path = material_path.split(".")[0]
            result["instances"][instance_path] = inspect_instance(instance_path)
            result["instances"][instance_path]["root"] = root
            result["instances"][instance_path]["units"] = len(info["units"])
    with open(OUTPUT_FILE, "w", encoding="utf-8") as handle:
        json.dump(result, handle, indent=1)
    print(f"TeamColorInspect: {len(result['roots'])} roots, {len(result['instances'])} instances -> {OUTPUT_FILE}")


main()
