"""Shared node helpers for the landscape builders (ls_*_build.py).

The helpers write into the current target, set by begin(): a material (nodes tagged with a desc so a re-run can find
and remove them) or a material function (rebuilt from scratch by function_asset()). Parameters created by S() join the
target's parameter group, so an instance shows them together.
"""
import os

import unreal

mel = unreal.MaterialEditingLibrary
_target = None
_desc = ""
_group = ""
failures = []
# Function inputs and outputs kept through a rebuild, by (kind, name): callers bind to their ids, so re-creating them
# would unhook every material that calls the function.
_kept_pins = {}

FUNCTION_DIR = "/Game/RTS_Survival/Materials/ground/Landscape/Functions"
LIBRARY_CATEGORY = "RTS Landscape"
CONTENT_DIR = "D:/UE5Projects/RTS_Survival/Content/"

SCALAR = unreal.FunctionInputType.FUNCTION_INPUT_SCALAR
VECTOR3 = unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3
VECTOR4 = unreal.FunctionInputType.FUNCTION_INPUT_VECTOR4
ATTRIBUTES = unreal.FunctionInputType.FUNCTION_INPUT_MATERIAL_ATTRIBUTES


def begin(target, desc="", group=""):
    global _target, _desc, _group
    _target, _desc, _group = target, desc, group


def tagged_expressions(owner, desc):
    """Every expression of the material carrying desc. Walks the material's real expressions: looking them up by
    name index misses nodes once repeated rebuilds push the numbering past any fixed range."""
    return [e for e in unreal.ObjectIterator(unreal.MaterialExpression)
            if e.get_outer() == owner and e.get_editor_property("desc").split(" ")[0] == desc]


def remove_tagged(material, desc):
    removed = 0
    for e in tagged_expressions(material, desc):
        mel.delete_material_expression(material, e)
        removed += 1
    return removed


def live_expressions(owner):
    """The owner's expressions; deleted ones linger in the object iterator until garbage collection."""
    return [e for e in unreal.ObjectIterator(unreal.MaterialExpression)
            if e.get_outer() == owner and unreal.SystemLibrary.is_valid(e)]


def function_asset(name, description):
    """The function asset, created on first use and emptied for a rebuild except for its inputs and outputs, which
    function_input() / function_output() reuse by name (delete_all_material_expressions_in_function is avoided: one
    pass removes only every other node)."""
    path = FUNCTION_DIR + "/" + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, FUNCTION_DIR, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    # A just-created function rejects new nodes until it is loaded again.
    function = unreal.load_asset(path)
    _kept_pins.clear()
    for e in live_expressions(function):
        if isinstance(e, unreal.MaterialExpressionFunctionInput):
            _kept_pins[("in", str(e.get_editor_property("input_name")))] = e
        elif isinstance(e, unreal.MaterialExpressionFunctionOutput):
            _kept_pins[("out", str(e.get_editor_property("output_name")))] = e
        else:
            mel.delete_material_expression_in_function(function, e)
    function.set_editor_property("description", description)
    function.set_editor_property("expose_to_library", True)
    function.set_editor_property("library_categories_text", [LIBRARY_CATEGORY])
    return function


def finish_function(function):
    """Removes inputs and outputs the rebuild no longer uses, compiles and saves the function; new-asset saves have
    silently missed disk in this project, so check it."""
    for e in _kept_pins.values():
        mel.delete_material_expression_in_function(function, e)
    _kept_pins.clear()
    mel.update_material_function(function)
    unreal.EditorAssetLibrary.save_loaded_asset(function, False)
    disk_path = CONTENT_DIR + function.get_path_name().split(".")[0][len("/Game/"):] + ".uasset"
    if not os.path.exists(disk_path):
        failures.append("not on disk: " + disk_path)
    return function


def N(cls, x, y, **props):
    expression_class = getattr(unreal, "MaterialExpression" + cls)
    if isinstance(_target, unreal.MaterialFunction):
        e = mel.create_material_expression_in_function(_target, expression_class, x, y)
    else:
        e = mel.create_material_expression(_target, expression_class, x, y)
    if _desc:
        e.set_editor_property("desc", _desc)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def C(a, a_out, b, b_in):
    if not mel.connect_material_expressions(a, a_out, b, b_in):
        failures.append("%s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def S(name, value, x, y):
    return N("ScalarParameter", x, y, parameter_name=name, default_value=value, group=_group)


def V(name, color, x, y):
    return N("VectorParameter", x, y, parameter_name=name, default_value=color, group=_group)


def const(value, x, y):
    return N("Constant", x, y, r=value)


def op(cls, a, a_out, b, b_out, x, y):
    e = N(cls, x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    return e


def mul(a, a_out, b, b_out, x, y):
    return op("Multiply", a, a_out, b, b_out, x, y)


def mulc(a, a_out, k, x, y):
    e = N("Multiply", x, y, const_b=k)
    C(a, a_out, e, "A")
    return e


def addc(a, a_out, k, x, y):
    e = N("Add", x, y, const_b=k)
    C(a, a_out, e, "A")
    return e


def sat(a, a_out, x, y):
    e = N("Saturate", x, y)
    C(a, a_out, e, "")
    return e


def lerp(a, a_out, b, b_out, alpha, alpha_out, x, y):
    e = N("LinearInterpolate", x, y)
    C(a, a_out, e, "A")
    C(b, b_out, e, "B")
    C(alpha, alpha_out, e, "Alpha")
    return e


def smooth(lo, hi, value, value_out, x, y):
    e = N("SmoothStep", x, y)
    C(lo, "", e, "Min")
    C(hi, "", e, "Max")
    C(value, value_out, e, "Value")
    return e


def mask(a, a_out, channels, x, y):
    r, g, b, w = [c in channels for c in "RGBA"]
    e = N("ComponentMask", x, y, r=r, g=g, b=b, a=w)
    C(a, a_out, e, "")
    return e


def desaturate(color, fraction, x, y):
    e = N("Desaturation", x, y)
    C(color, "", e, "")
    C(const(fraction, x - 140, y + 40), "", e, "Fraction")
    return e


def custom(code, output_type, description, input_names, x, y):
    e = N("Custom", x, y, code=code, output_type=output_type, description=description)
    inputs = []
    for name in input_names:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    e.set_editor_property("inputs", inputs)
    return e


def reuse_pin(kind, name, x, y, **props):
    """The kept input/output called name, moved and updated, or None."""
    e = _kept_pins.pop((kind, name), None)
    if e is None:
        return None
    for k, v in dict(props, material_expression_editor_x=x, material_expression_editor_y=y).items():
        e.set_editor_property(k, v)
    return e


def function_input(name, input_type, priority, x, y, description="", default=None, default_out=""):
    """A function input; with default, an unconnected pin falls back to that node (the input's preview)."""
    props = dict(input_name=name, input_type=input_type, sort_priority=priority, description=description,
                 use_preview_value_as_default=default is not None)
    e = reuse_pin("in", name, x, y, **props) or N("FunctionInput", x, y, **props)
    if default is not None:
        C(default, default_out, e, "Preview")
    return e


def function_output(name, source, source_out, priority, x, y, description=""):
    props = dict(output_name=name, sort_priority=priority, description=description)
    e = reuse_pin("out", name, x, y, **props) or N("FunctionOutput", x, y, **props)
    C(source, source_out, e, "")
    return e


def function_call(function, x, y):
    e = N("MaterialFunctionCall", x, y)
    e.set_material_function(function)
    return e


# Comment colours: one per kind of section, so the graph reads at a glance.
INPUT_COLOR = unreal.LinearColor(0.15, 0.35, 0.6, 1.0)
LAYER_COLOR = unreal.LinearColor(0.25, 0.45, 0.2, 1.0)
EFFECT_COLOR = unreal.LinearColor(0.55, 0.35, 0.15, 1.0)
OUTPUT_COLOR = unreal.LinearColor(0.45, 0.2, 0.45, 1.0)


SIZE_TAG = "size="


def comment(text, x, y, width, height, color=EFFECT_COLOR, font_size=18):
    """A comment frame. Its size is editor state Python cannot set; URTSMaterialGraphLibrary.SetCommentSize can. The
    size also goes into the comment's desc ("size=WxH"), so ls_editor_layout.apply() can set it later from an editor
    that has the function (the shared editor may still run an older DLL)."""
    e = N("Comment", x, y, text=text, comment_color=color, font_size=font_size, color_comment_bubble=True,
          comment_bubble_visible_in_details_panel=True)
    e.set_editor_property("desc", ((_desc + " ") if _desc else "") + "%s%dx%d" % (SIZE_TAG, width, height))
    set_comment_size(e)
    return e


def set_comment_size(e):
    """Applies the desc's size=WxH; False when this editor lacks URTSMaterialGraphLibrary or there is no size."""
    library = getattr(unreal, "RTSMaterialGraphLibrary", None)
    sizes = [part[len(SIZE_TAG):] for part in e.get_editor_property("desc").split(" ") if part.startswith(SIZE_TAG)]
    if library is None or not sizes:
        return False
    width, height = sizes[0].split("x")
    return library.set_comment_size(e, int(width), int(height))
