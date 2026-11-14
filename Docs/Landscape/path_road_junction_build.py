"""Builds the junction overlay for the old macadam road (BP_GroundPathSpline_Road, Docs/Environment/GroundPathRoad.md).

Placing it (BP_GroundPath_RoadJunction = cross, BP_GroundPath_RoadJunction_Tee = T):
  - Both road splines run straight through the crossing (Tee: the side road's spline ends 0 to 3 m short of the main
    road's centreline). The overlay is solid over the whole overlap, so it hides the crossing shoulders and the
    coplanar tiles, and fades out into the roads 6.8 to 9 m from the centre.
  - Pivot: where the two centrelines cross. Yaw: the main road's heading there (local +X along the main road; either
    direction for the cross; for the Tee the side road leaves towards local +Y = heading + 90).
  - Z: the road tiles' height plus 6 cm = ground + GroundOffset (20) + 6. Flat ground within a few cm over 9 m.
  - Crossing not square: component.set_default_custom_primitive_data_float(0, skew) with
    skew = ((side heading - main heading) mod 180) - 90 degrees (side 0, main -86: 86 - 90 = -4), tested at -20, -4
    and +20 (70 to 110 degree crossings). Take the pivot, the yaw and the tile height from the placed main road, not from a sketch.
  - Roads 900 cm wide (PathWidth 900). No collision, no navigation effect, no shadow; 40 / 30 triangles, one material.
Test scene and screenshots: Source/Tools/GroundPathJunction (t_scene.py, t_shots.py in a throwaway editor).

Headless:
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
      -DisablePlugins=Fab,BpGeneratorUltimate
Sources: D:/Blender/BlenderProjects/BlenderTanks/GroundPath/Junction (gen_junction.py -> out_junction/*.png,
gen_junction_mesh.py -> export/*.fbx).

run() imports the atlas and the two meshes, builds M_GroundPath_RoadJunction, its instances and the two Blueprints, all in
/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/Road/Junction. It never writes the road's own assets: it
only reads the shader text and the graph helpers from path_road_build.py and the parameter values from the road's
material instances, so the junction shades exactly like the road it lies on. Rerun it after the road shader or its
instances change.

The junction material is the road material with three differences:
  - the distance from the carriageway centre (edge-crack boost) comes from the atlas (M.b), not from U;
  - vertex colour R fades the overlay out into the road underneath at the arm ends (opacity only, no ground tint);
  - SkewDeg (custom primitive data 0) shears the mesh in the vertex shader for crossings that are not square.
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals()
                else "D:/UE5Projects/RTS_Survival/Source/Docs/Landscape")
import path_road_build as road      # helpers and ROAD_HLSL only; never call its run/build/make functions from here

SRC = "D:/Blender/BlenderProjects/BlenderTanks/GroundPath/Junction"
ROAD_DIR = road.DIR
DIR = ROAD_DIR + "/Junction"
MAT = DIR + "/M_GroundPath_RoadJunction"
ATLAS_EXTENT = 1800.0           # cm covered by the atlas (gen_junction.py EXT)
SKEW_DATA_INDEX = 0             # custom primitive data slot of SkewDeg
# (junction instance, road instance its values are copied from)
INSTANCES = (("MI_GroundPath_RoadJunction", "MI_GroundPath_Road"),
             ("MI_GroundPath_RoadJunction_WarTorn", "MI_GroundPath_Road_WarTorn"))
DEFAULT_INSTANCE = "MI_GroundPath_RoadJunction_WarTorn"
MESHES = ("SM_GroundPath_RoadJunction_Cross", "SM_GroundPath_RoadJunction_Tee")
BLUEPRINTS = (("BP_GroundPath_RoadJunction", "SM_GroundPath_RoadJunction_Cross"),
              ("BP_GroundPath_RoadJunction_Tee", "SM_GroundPath_RoadJunction_Tee"))
COMPONENT_NAME = "JunctionMesh"

TEXTURES = {
    "T_GroundPath_RoadJunction_BC": road.TEXTURES["T_GroundPath_Road_BC"],
    "T_GroundPath_RoadJunction_N": road.TEXTURES["T_GroundPath_Road_N"],
    "T_GroundPath_RoadJunction_M": road.TEXTURES["T_GroundPath_Road_M"],
}

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
N, C, S, V = road.N, road.C, road.S, road.V
failures = road.failures


def patched_hlsl():
    """The road's shader text with the junction's two changes; fails loudly when the road shader was reworded."""
    code = road.ROAD_HLSL
    for old, new in (
            ("float X = abs(U * 2.0 - 1.0);", "float X = M.b;"),
            ("Opacity = saturate(BC.a * 1.35 - 0.2 + (NoiseFine - 0.5) * 0.35);",
             "Opacity = saturate(BC.a * 1.35 - 0.2 + (NoiseFine - 0.5) * 0.35);\n"
             "// Arm ends: fade into the road underneath, ragged so the end is not a straight line.\n"
             "float FadeRagged = saturate(Fade + (NoiseFine - 0.5) * 2.4 * Fade * (1.0 - Fade));\n"
             "Opacity *= smoothstep(0.0, 1.0, FadeRagged);")):
        if old not in code:
            raise RuntimeError("path_road_build.ROAD_HLSL changed, junction patch anchor not found: " + old)
        code = code.replace(old, new)
    return code


SKEW_HLSL = """// Shear for crossings that are not square: the side road (local Y) turns by Deg, both roads keep their width.
float2 Local = (UV - 0.5) * Extent;
float Angle = radians(Deg);
return float3(Local.x * (1.0 / cos(Angle) - 1.0) - Local.y * tan(Angle), 0.0, 0.0);"""


def import_textures():
    tasks = []
    for name in TEXTURES:
        task = unreal.AssetImportTask()
        task.filename = "%s/out_junction/%s.png" % (SRC, name)
        task.destination_path = DIR
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    for name, (compression, srgb, group) in TEXTURES.items():
        tex = unreal.load_asset("%s/%s" % (DIR, name))
        tex.set_editor_property("compression_settings", compression)
        tex.set_editor_property("srgb", srgb)
        tex.set_editor_property("lod_group", group)
        eal.save_loaded_asset(tex, False)


def custom_node(mat, x, y, code, description, output_type, inputs_spec, outputs=()):
    node = N(mat, "Custom", x, y, code=code, description=description, output_type=output_type)
    inputs = []
    for name, _, _ in inputs_spec:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", name)
        inputs.append(ci)
    node.set_editor_property("inputs", inputs)
    outs = []
    for name in outputs:
        co = unreal.CustomOutput()
        co.set_editor_property("output_name", name)
        co.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        outs.append(co)
    node.set_editor_property("additional_outputs", outs)
    for name, source, out in inputs_spec:
        C(mat, source, out, node, name)
    return node


def build_material():
    mat = unreal.load_asset(MAT)
    if mat is None:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_GroundPath_RoadJunction", DIR, unreal.Material, unreal.MaterialFactoryNew())
    road.USED.extend(c for c in ("VertexColor",) if c not in road.USED)
    road.clear_material(mat)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    mat.set_editor_property("opacity_mask_clip_value", 0.5)
    mat.set_editor_property("two_sided", False)
    mat.set_editor_property("tangent_space_normal", False)

    T = unreal.MaterialSamplerType
    uv0 = N(mat, "TextureCoordinate", -2400, -400)
    bc = road.sample(mat, DIR + "/T_GroundPath_RoadJunction_BC", T.SAMPLERTYPE_COLOR, -2000, -700, "RoadColor")
    nrm = road.sample(mat, DIR + "/T_GroundPath_RoadJunction_N", T.SAMPLERTYPE_NORMAL, -2000, -400, "RoadNormal")
    msk = road.sample(mat, DIR + "/T_GroundPath_RoadJunction_M", T.SAMPLERTYPE_MASKS, -2000, -100, "RoadMasks")
    for t in (bc, nrm, msk):
        C(mat, uv0, "", t, "UVs")

    wp = N(mat, "WorldPosition", -2800, 300)
    wpxy = N(mat, "ComponentMask", -2600, 300, r=True, g=True, b=False, a=False)
    C(mat, wp, "", wpxy, "")
    crack_scale = S(mat, "CrackScale", 1200.0, -2600, 420)
    block_scale = S(mat, "AlligatorScale", 420.0, -2600, 560)
    large_scale = S(mat, "NoiseLargeScale", 6000.0, -2600, 700)
    medium_scale = S(mat, "NoiseMediumScale", 1500.0, -2600, 840)
    cracks = road.sample(mat, ROAD_DIR + "/T_GroundPath_RoadCracks", T.SAMPLERTYPE_LINEAR_COLOR, -2000, 300, "CrackField")
    C(mat, road.world_uv(mat, wpxy, crack_scale, -2200, 300), "", cracks, "UVs")
    blocks = road.sample(mat, ROAD_DIR + "/T_GroundPath_RoadCracks", T.SAMPLERTYPE_LINEAR_COLOR, -2000, 560)
    C(mat, road.world_uv(mat, wpxy, block_scale, -2200, 560, swap=True), "", blocks, "UVs")
    noise_large = road.sample(mat, road.NOISE, T.SAMPLERTYPE_MASKS, -2000, 800)
    C(mat, road.world_uv(mat, wpxy, large_scale, -2200, 800), "", noise_large, "UVs")
    noise_medium = road.sample(mat, road.NOISE, T.SAMPLERTYPE_MASKS, -2000, 1040)
    C(mat, road.world_uv(mat, wpxy, medium_scale, -2200, 1040), "", noise_medium, "UVs")
    rvt_env = N(mat, "RuntimeVirtualTextureSample", -2000, 1300, virtual_texture=unreal.load_asset(road.RVT_ENV),
                material_type=unreal.RuntimeVirtualTextureMaterialType.BASE_COLOR_NORMAL_SPECULAR)
    vertex_colour = N(mat, "VertexColor", -1500, 800)

    # Same parameter names and master defaults as M_GroundPath_Road; the instances copy the road instances' values.
    inputs_spec = [
        ("BC", bc, "RGBA"), ("M", msk, "RGBA"), ("C1", cracks, "RGBA"), ("C2", blocks, "RGBA"),
        ("NL", noise_large, "RGBA"), ("NM", noise_medium, "RGBA"),
        ("CrackAmount", S(mat, "CrackAmount", 0.45, -1500, -600), ""),
        ("CrackWidth", S(mat, "CrackWidth", 0.07, -1500, -500), ""),
        ("EdgeCrackBoost", S(mat, "EdgeCrackBoost", 0.25, -1500, -400), ""),
        ("AlligatorAmount", S(mat, "AlligatorAmount", 0.35, -1500, -300), ""),
        ("HairAmount", S(mat, "HairAmount", 0.6, -1500, -200), ""),
        ("WeedAmount", S(mat, "WeedAmount", 0.25, -1500, -100), ""),
        ("DustAmount", S(mat, "DustAmount", 0.5, -1500, 0), ""),
        ("DampAmount", S(mat, "DampAmount", 0.5, -1500, 100), ""),
        ("CrackColor", V(mat, "CrackColor", (0.035, 0.032, 0.028), -1500, 200), ""),
        ("WeedColor", V(mat, "WeedColor", (0.10, 0.10, 0.045), -1500, 300), ""),
        ("DustTint", V(mat, "DustTint", (1.14, 1.09, 1.0), -1500, 400), ""),
        ("GroundColor", rvt_env, "BaseColor"),
        ("GroundBlend", S(mat, "GroundBlend", 0.85, -1500, 600), ""),
        ("Fade", vertex_colour, "R"),
    ]
    custom = custom_node(mat, -1100, 200, patched_hlsl(), "RoadJunctionShade",
                         unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs_spec, ("Roughness", "AO", "Opacity"))
    world_normal = road.landscape_shading_normal(mat, nrm, rvt_env)
    dither = N(mat, "MaterialFunctionCall", -700, 500)
    dither.set_material_function(unreal.load_asset(road.DITHER))
    C(mat, custom, "Opacity", dither, "Alpha Threshold")

    # Skew: a local-space shear from the UVs (planar over the atlas), turned into world space.
    skew_deg = S(mat, "SkewDeg", 0.0, -1500, 1500)
    skew_deg.set_editor_property("group", "Junction")
    skew_deg.set_editor_property("use_custom_primitive_data", True)
    skew_deg.set_editor_property("primitive_data_index", SKEW_DATA_INDEX)
    extent = N(mat, "Constant", -1500, 1600, r=ATLAS_EXTENT)
    skew = custom_node(mat, -1100, 1500, SKEW_HLSL, "JunctionSkew", unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                       [("UV", uv0, ""), ("Deg", skew_deg, ""), ("Extent", extent, "")])
    skew_world = N(mat, "TransformVector" if hasattr(unreal, "MaterialExpressionTransformVector") else "Transform", -800, 1500,
                   transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                   transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    C(mat, skew, "", skew_world, "")

    for prop, node, out in (
            (unreal.MaterialProperty.MP_BASE_COLOR, custom, ""),
            (unreal.MaterialProperty.MP_ROUGHNESS, custom, "Roughness"),
            (unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, custom, "AO"),
            (unreal.MaterialProperty.MP_NORMAL, world_normal, ""),
            (unreal.MaterialProperty.MP_OPACITY_MASK, dither, "Result")):
        if not mel.connect_material_property(node, out, prop):
            failures.append("%s.%s -> %s" % (node.get_name(), out, prop))
    # Python's MaterialProperty has no World Position Offset; the project's editor library wires it.
    lib = unreal.NomadicDeployEditorLibrary
    if not lib.connect_world_position_offset(skew_world, ""):
        failures.append("World Position Offset not wired")
    lib.refresh_material(mat)
    print("JUNCTION WPO:", lib.describe_world_position_offset(mat))
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat, False)
    return mat


def make_instance(name, road_instance_name):
    """A junction instance carrying every scalar and colour of the road instance it belongs to."""
    path = "%s/%s" % (DIR, name)
    mi = unreal.load_asset(path)
    if mi is None:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset(MAT))
    source = unreal.load_asset("%s/%s" % (ROAD_DIR, road_instance_name))
    road_master = unreal.load_asset(road.MAT)
    copied = {}
    for key in mel.get_scalar_parameter_names(road_master):
        value = mel.get_material_instance_scalar_parameter_value(source, key)
        mel.set_material_instance_scalar_parameter_value(mi, key, value)
        copied[str(key)] = round(value, 4)
    for key in mel.get_vector_parameter_names(road_master):
        mel.set_material_instance_vector_parameter_value(mi, key, mel.get_material_instance_vector_parameter_value(source, key))
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    print("JUNCTION instance", name, "from", road_instance_name, copied)
    return mi


def import_meshes(material):
    meshes = {}
    for name in MESHES:
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_mesh = True
        options.import_as_skeletal = False
        options.import_materials = False
        options.import_textures = False
        data = options.static_mesh_import_data
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("auto_generate_collision", False)
        data.set_editor_property("generate_lightmap_u_vs", False)
        data.set_editor_property("build_nanite", False)
        data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
        task = unreal.AssetImportTask()
        task.filename = "%s/export/%s.fbx" % (SRC, name)
        task.destination_path = DIR
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        task.options = options
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        mesh = unreal.load_asset("%s/%s" % (DIR, name))
        if mesh is None:
            failures.append("mesh import " + name)
            continue
        settings = mesh.get_editor_property("nanite_settings")
        settings.enabled = False
        mesh.set_editor_property("nanite_settings", settings)
        mesh.set_material(0, material)
        body = mesh.get_editor_property("body_setup")
        if body:
            body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
        eal.save_loaded_asset(mesh, False)
        check_mesh(mesh)
        meshes[name] = mesh
    return meshes


def check_mesh(mesh):
    """Bounds in cm, planar UVs (u = x / extent + 0.5, v = y / extent + 0.5) and the fade colours, read back."""
    bounds = mesh.get_bounds()
    description = mesh.get_static_mesh_description(0)
    worst_uv, fades = 0.0, set()
    for index in range(description.get_vertex_instance_count()):
        instance = unreal.VertexInstanceID(index)
        position = description.get_vertex_position(description.get_vertex_instance_vertex(instance))
        uv = description.get_vertex_instance_uv(instance, 0)
        worst_uv = max(worst_uv, abs(uv.x - (position.x / ATLAS_EXTENT + 0.5)), abs(uv.y - (position.y / ATLAS_EXTENT + 0.5)))
    colours = unreal.EditorStaticMeshLibrary.has_vertex_colors(mesh)
    print("JUNCTION mesh %s: extent %.1f %.1f %.1f, origin %.1f %.1f %.1f, triangles %d, worst uv error %.5f, vertex colours %s, collision %d" % (
        mesh.get_name(), bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z, bounds.origin.x, bounds.origin.y,
        bounds.origin.z, description.get_triangle_count(), worst_uv, colours,
        unreal.EditorStaticMeshLibrary.get_simple_collision_count(mesh)))
    if worst_uv > 0.001 or not colours or abs(bounds.box_extent.x - 900.0) > 1.0:
        failures.append("mesh check " + mesh.get_name())


def make_blueprint(name, mesh):
    path = "%s/%s" % (DIR, name)
    bp = unreal.load_asset(path)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.Actor)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, DIR, unreal.Blueprint, factory)
    component = None
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles:
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.StaticMeshComponent):
            component = obj
    if component is None:
        handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(
            parent_handle=handles[0], new_class=unreal.StaticMeshComponent, blueprint_context=bp))
        sds.rename_subobject(handle, unreal.Text(COMPONENT_NAME))
        component = lib.get_object(lib.get_data(handle))
    component.set_editor_property("static_mesh", mesh)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    component.set_collision_profile_name("NoCollision")
    component.set_editor_property("generate_overlap_events", False)
    component.set_editor_property("can_ever_affect_navigation", False)
    component.set_editor_property("cast_shadow", False)
    # The skew moves vertices up to 3.9 m at 20 degrees; keep the culling bounds around them.
    component.set_editor_property("bounds_scale", 1.6)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)
    print("JUNCTION blueprint", name, component.get_name(), mesh.get_name())
    return bp


def run(textures=True):
    failures.clear()
    if textures:
        import_textures()
    build_material()
    instances = {name: make_instance(name, source) for name, source in INSTANCES}
    meshes = import_meshes(instances[DEFAULT_INSTANCE])
    for name, mesh_name in BLUEPRINTS:
        if mesh_name in meshes:
            make_blueprint(name, meshes[mesh_name])
    print("JUNCTION build failures:", failures)
    return failures


if __name__ == "__main__":
    run()
