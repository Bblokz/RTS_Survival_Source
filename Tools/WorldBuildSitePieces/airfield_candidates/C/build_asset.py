"""Materials and static mesh asset for airfield candidate C (imported by build_piece.py, runs in the same commandlet).

Folder: /Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/C
  M_AirfieldC_Paint      the watch post paint graph (object-space three-tone camo from 3D noise, height shade, emissive
                         = base colour x EmissiveStrength), no textures, no UVs needed
  MI_AirfieldC_Walls     Dunkelgelb with olive and red-brown patches (building walls)
  MI_AirfieldC_Roof      cream (roof slabs, tower roof, opened roof panels)
  MI_AirfieldC_Stuka     light blue-grey with grey mottle (both Ju 87s: cool hue against the warm building, green plate)
  MI_AirfieldC_Accent    RLM 04 yellow (landing pad markings, Ju 87 wing tips, cowling, fin, fuselage band)
  MI_AirfieldC_Dark      near black steel (roof struts, props, the take-off stand)
  SM_BuildSitePiece_Airfield_C
"""
import os

import unreal

FOLDER = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/C"
MESH_NAME = "SM_BuildSitePiece_Airfield_C"
MASTER_NAME = "M_AirfieldC_Paint"
SLOT_NAMES = ["Walls", "Roof", "Stuka", "Accent", "Dark"]
library = unreal.MaterialEditingLibrary
assets = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

CAMO_CODE = """// Three-tone camo in object units: value-noise patches of colour A and B on Base (copy of the watch post graph).
struct AFCCamo
{
    float hash(float3 p)
    {
        p=frac(p*0.3183099+0.1);
        p*=17.0;
        return frac(p.x*p.y*p.z*(p.x+p.y+p.z));
    }
    float noise(float3 x)
    {
        float3 i=floor(x);
        float3 f=frac(x);
        f=f*f*(3.0-2.0*f);
        float a=lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y);
        float b=lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y);
        return lerp(a,b,f.z);
    }
    float fbm(float3 x)
    {
        return noise(x)*0.62+noise(x*2.03+11.7)*0.28+noise(x*4.11+3.1)*0.10;
    }
};
AFCCamo c;
float3 q=P/max(Scale,0.01)+Seed;
q.z*=1.5;
float e=0.03;
float green=smoothstep(GreenCut-e,GreenCut+e,c.fbm(q));
float brown=smoothstep(BrownCut-e,BrownCut+e,c.fbm(q+float3(37.1,-12.4,5.3)))*(1.0-green);
float3 col=lerp(Base,A,green*Amount);
col=lerp(col,B,brown*Amount);
float shade=lerp(Bottom,1.0,saturate(P.z/max(Top,0.01)));
return col*shade;"""

SCALARS = {"CamoAmount": 1.0, "CamoScale": 4.0, "GreenCut": 0.53, "BrownCut": 0.56, "Seed": 3.0,
           "BottomShade": 0.7, "ShadeTop": 10.0, "Roughness": 0.7, "EmissiveStrength": 0.5}
VECTORS = {"BaseColor": (0.68, 0.46, 0.165), "CamoColorA": (0.117, 0.15, 0.034), "CamoColorB": (0.216, 0.058, 0.023)}

# Linear colours. Emissive: the watch post settled on 0.3 / 0.5, the airfield session on 0.8 for darker textures;
# these flat colours are brighter, so 0.45 - 0.6 on the big areas and 0.9 on the yellow accent.
INSTANCES = {
    "Walls": ("MI_AirfieldC_Walls", {}, {"EmissiveStrength": 0.45}),
    "Roof": ("MI_AirfieldC_Roof", {"BaseColor": (0.87, 0.77, 0.55)},
             {"CamoAmount": 0.0, "BottomShade": 0.85, "Roughness": 0.6, "EmissiveStrength": 0.5}),
    "Stuka": ("MI_AirfieldC_Stuka", {"BaseColor": (0.42, 0.50, 0.56), "CamoColorA": (0.20, 0.23, 0.27),
                                     "CamoColorB": (0.29, 0.32, 0.36)},
              {"CamoAmount": 0.85, "CamoScale": 1.6, "GreenCut": 0.58, "BrownCut": 0.6, "BottomShade": 1.0,
               "Roughness": 0.5, "EmissiveStrength": 0.55}),
    "Accent": ("MI_AirfieldC_Accent", {"BaseColor": (1.0, 0.55, 0.02)},
               {"CamoAmount": 0.0, "BottomShade": 1.0, "Roughness": 0.5, "EmissiveStrength": 0.9}),
    "Dark": ("MI_AirfieldC_Dark", {"BaseColor": (0.07, 0.07, 0.065)},
             {"CamoAmount": 0.0, "BottomShade": 1.0, "Roughness": 0.6, "EmissiveStrength": 0.15}),
}


def save(asset, log):
    if not assets.save_loaded_asset(asset, False):
        log("SAVE FAILED " + asset.get_path_name())


def connect(source, output, target, target_input, log):
    if not library.connect_material_expressions(source, output, target, target_input):
        log("CONNECT FAILED %s.%s -> %s.%s" % (source.get_name(), output, target.get_name(), target_input))


def node(material, node_class, x, y, **properties):
    expression = library.create_material_expression(material, node_class, x, y)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def ensure_master(log):
    path = FOLDER + "/" + MASTER_NAME
    if assets.does_asset_exist(path) and os.environ.get("AIRFIELDC_REBUILD_MATERIAL", "") != "1":
        log("master kept as it is")
        return unreal.load_asset(path)
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = asset_tools.create_asset(MASTER_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    scalars = {name: node(material, unreal.MaterialExpressionScalarParameter, -1400, -600 + index * 110,
                          parameter_name=name, default_value=value)
               for index, (name, value) in enumerate(SCALARS.items())}
    vectors = {name: node(material, unreal.MaterialExpressionVectorParameter, -1400, -1100 + index * 160,
                          parameter_name=name, default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))
               for index, (name, value) in enumerate(VECTORS.items())}
    world_position = node(material, unreal.MaterialExpressionWorldPosition, -1400, -1400)
    local_position = node(material, unreal.MaterialExpressionTransformPosition, -1150, -1400,
                          transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                          transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    connect(world_position, "", local_position, "", log)
    pins = [("P", local_position), ("Base", vectors["BaseColor"]), ("A", vectors["CamoColorA"]),
            ("B", vectors["CamoColorB"]), ("Scale", scalars["CamoScale"]), ("Amount", scalars["CamoAmount"]),
            ("GreenCut", scalars["GreenCut"]), ("BrownCut", scalars["BrownCut"]), ("Seed", scalars["Seed"]),
            ("Bottom", scalars["BottomShade"]), ("Top", scalars["ShadeTop"])]
    inputs = []
    for name, _source in pins:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    camo = node(material, unreal.MaterialExpressionCustom, -800, -800, description="AFC_AirfieldCamo",
                code=CAMO_CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=inputs)
    for name, source in pins:
        connect(source, "", camo, name, log)
    library.connect_material_property(camo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emissive = node(material, unreal.MaterialExpressionMultiply, -500, -500)
    connect(camo, "", emissive, "A", log)
    connect(scalars["EmissiveStrength"], "", emissive, "B", log)
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(scalars["Roughness"], "", unreal.MaterialProperty.MP_ROUGHNESS)
    material.set_editor_property("two_sided", True)   # single-sided source panels are seen from both sides when open
    library.recompile_material(material)
    save(material, log)
    log("master built with %d expressions" % library.get_num_material_expressions(material))
    return material


def make_instance(name, master, vectors, scalars, log):
    path = FOLDER + "/" + name
    instance = unreal.load_asset(path) if assets.does_asset_exist(path) else asset_tools.create_asset(
        name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    library.set_material_instance_parent(instance, master)
    try:
        library.clear_all_material_instance_parameters(instance)
    except Exception as error:
        log("clear parameters: %s" % error)
    for parameter, value in vectors.items():
        library.set_material_instance_vector_parameter_value(instance, parameter,
                                                             unreal.LinearColor(value[0], value[1], value[2], 1.0))
    for parameter, value in scalars.items():
        library.set_material_instance_scalar_parameter_value(instance, parameter, value)
    library.update_material_instance(instance)
    save(instance, log)
    return instance


def write_mesh(dynamic_mesh, slot_materials, log):
    path = FOLDER + "/" + MESH_NAME
    gs_assets = unreal.GeometryScript_AssetUtils
    if assets.does_asset_exist(path):
        static_mesh = unreal.load_asset(path)
        options = unreal.GeometryScriptCopyMeshToAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.replace_materials = True
        options.new_materials = slot_materials
        options.new_material_slot_names = SLOT_NAMES
        options.emit_transaction = False
        options.defer_mesh_post_edit_change = False
        gs_assets.copy_mesh_to_static_mesh(dynamic_mesh, static_mesh, options, unreal.GeometryScriptMeshWriteLOD(), False)
        log("updated existing " + path)
    else:
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        options.enable_recompute_normals = False
        options.enable_recompute_tangents = True
        options.enable_nanite = False
        options.enable_collision = False
        options.collision_mode = unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX
        options.use_original_vertex_order = False
        _mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
            dynamic_mesh, path, options)
        log("created %s: %s" % (path, outcome))
        static_mesh = unreal.load_asset(path)
    static_mesh.set_editor_property("static_materials", [
        unreal.StaticMaterial(material_interface=material, material_slot_name=SLOT_NAMES[index])
        for index, material in enumerate(slot_materials)])
    nanite = static_mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    static_mesh.set_editor_property("nanite_settings", nanite)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(static_mesh)
    body = static_mesh.get_editor_property("body_setup")
    if body is not None:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    build = subsystem.get_lod_build_settings(static_mesh, 0)
    build.generate_lightmap_u_vs = False
    build.recompute_normals = False
    build.recompute_tangents = True
    subsystem.set_lod_build_settings(static_mesh, 0, build)
    static_mesh.set_editor_property("light_map_resolution", 4)
    save(static_mesh, log)
    return static_mesh


def write(dynamic_mesh, log):
    if not assets.does_directory_exist(FOLDER):
        assets.make_directory(FOLDER)
    master = ensure_master(log)
    slot_materials = [make_instance(INSTANCES[slot][0], master, INSTANCES[slot][1], INSTANCES[slot][2], log)
                      for slot in SLOT_NAMES]
    static_mesh = write_mesh(dynamic_mesh, slot_materials, log)
    bounds = static_mesh.get_bounding_box()
    log("asset %s tris=%d slots=%s box (%.2f %.2f %.2f)..(%.2f %.2f %.2f)" % (
        static_mesh.get_path_name(), static_mesh.get_num_triangles(0),
        [str(m.material_slot_name) for m in static_mesh.static_materials], bounds.min.x, bounds.min.y, bounds.min.z,
        bounds.max.x, bounds.max.y, bounds.max.z))
