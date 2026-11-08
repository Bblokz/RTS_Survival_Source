"""Read-only material graph summary (headless). Usage: material_dump.py <material path> [...]
Per material: blend/shading/two-sided/translucency settings, which outputs are wired (WPO, PDO, refraction, opacity...),
expression class counts, material function calls, texture samples (with texture names) and custom HLSL nodes.
Writes Saved/PerfAudit/material_dump.json and logs RTS_MAT_DUMP lines."""
import json
import os
import sys
import unreal

MEL = unreal.MaterialEditingLibrary
OUT = unreal.Paths.project_saved_dir() + "PerfAudit/material_dump.json"
PROPS = ["MP_BASE_COLOR", "MP_METALLIC", "MP_SPECULAR", "MP_ROUGHNESS", "MP_EMISSIVE_COLOR", "MP_OPACITY",
         "MP_OPACITY_MASK", "MP_NORMAL", "MP_WORLD_POSITION_OFFSET", "MP_PIXEL_DEPTH_OFFSET", "MP_REFRACTION",
         "MP_AMBIENT_OCCLUSION", "MP_SUBSURFACE_COLOR", "MP_MATERIAL_ATTRIBUTES", "MP_ANISOTROPY", "MP_TANGENT"]


def prop(obj, name):
    try:
        return str(obj.get_editor_property(name)).split(".")[-1].split(":")[0]
    except Exception:
        return None


def dump(path):
    asset = unreal.load_asset(path)
    if asset is None:
        return {"path": path, "error": "not found"}
    mat = asset.get_base_material() if isinstance(asset, unreal.MaterialInstance) else asset
    info = {"path": path, "base": mat.get_path_name(), "blend": prop(mat, "blend_mode"), "shading": prop(mat, "shading_model"),
            "two_sided": prop(mat, "two_sided"), "translucency_lighting": prop(mat, "translucency_lighting_mode"),
            "use_material_attributes": prop(mat, "use_material_attributes"), "outputs": [], "classes": {}, "functions": {},
            "textures": [], "custom": []}
    for p in PROPS:
        try:
            node = MEL.get_material_property_input_node(mat, getattr(unreal.MaterialProperty, p))
            if node:
                info["outputs"].append("%s<-%s" % (p[3:], node.get_class().get_name()))
        except Exception:
            pass
    for expr in unreal.ObjectIterator(unreal.MaterialExpression):
        try:
            if expr.get_typed_outer(unreal.Material) != mat:
                continue
        except Exception:
            continue
        cls = expr.get_class().get_name()
        info["classes"][cls] = info["classes"].get(cls, 0) + 1
        if cls == "MaterialExpressionMaterialFunctionCall":
            fn = expr.get_editor_property("material_function")
            name = fn.get_path_name() if fn else "None"
            info["functions"][name] = info["functions"].get(name, 0) + 1
        elif "TextureSample" in cls or cls == "MaterialExpressionTextureObject":
            try:
                tex = expr.get_editor_property("texture")
                info["textures"].append("%s:%s" % (cls.replace("MaterialExpression", ""), tex.get_name() if tex else None))
            except Exception:
                info["textures"].append(cls)
        elif cls == "MaterialExpressionCustom":
            code = str(expr.get_editor_property("code"))
            info["custom"].append({"desc": str(expr.get_editor_property("description")), "len": len(code),
                                   "loops": code.count("for") + code.count("while"), "code": code[:400]})
    info["num_expressions"] = sum(info["classes"].values())
    unreal.log_warning("RTS_MAT_DUMP %s expr=%d funcs=%d tex=%d custom=%d outputs=%s" % (
        path.split("/")[-1], info["num_expressions"], len(info["functions"]), len(info["textures"]), len(info["custom"]),
        ",".join(info["outputs"])))
    return info


rows = [dump(p) for p in sys.argv[1:]]
os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w") as fh:
    json.dump(rows, fh, indent=1)
unreal.log_warning("RTS_MAT_DUMP wrote %s" % OUT)
