"""Imports the watch post build site piece and builds its paint.

Run headless (no GPU), after blender_build.py wrote the FBX:
  run_headless.sh build_watch_post
Log: Saved/WorldBuildSitePieces/watch_post/build.txt

Assets in /Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/WatchPost:
  SM_BuildSitePiece_WatchPost   the Soviet metal guard tower (SM_Rus_GuardMgTower), 4.8 % size, no collision
  M_WatchPost_Paint             object-space German three-tone camo (3D noise, no UVs), height shade, emissive term
  MI_WatchPost_Body / _Roof / _Lattice / _Net
Environment: WATCHPOST_REBUILD_MATERIAL=1 rebuilds the master graph (only on purpose: see ue-python-material-gotchas).
"""
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                                "WorldMapPieces"))
import piece_lib as lib  # noqa: E402

FOLDER = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/WatchPost"
MESH_NAME = "SM_BuildSitePiece_WatchPost"
MASTER_NAME = "M_WatchPost_Paint"
FBX = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces", "watch_post", MESH_NAME + ".fbx")
LOG_DIR = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                       "WorldBuildSitePieces", "watch_post")
library = unreal.MaterialEditingLibrary

CAMO_CODE = """// German three-tone camo in object units: value-noise patches of olive and red-brown on dark yellow.
struct WPCamo
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
WPCamo c;
float3 q=P/max(Scale,0.01)+Seed;
// Patches a little longer across than up, like sprayed bands.
q.z*=1.5;
float e=0.03;
float green=smoothstep(GreenCut-e,GreenCut+e,c.fbm(q));
float brown=smoothstep(BrownCut-e,BrownCut+e,c.fbm(q+float3(37.1,-12.4,5.3)))*(1.0-green);
float3 col=lerp(Base,A,green*Amount);
col=lerp(col,B,brown*Amount);
float shade=lerp(Bottom,1.0,saturate(P.z/max(Top,0.01)));
return col*shade;"""

SCALARS = {"CamoAmount": 1.0, "CamoScale": 7.0, "GreenCut": 0.53, "BrownCut": 0.56, "Seed": 3.0,
           "BottomShade": 0.7, "ShadeTop": 45.0, "Roughness": 0.7, "EmissiveStrength": 0.12}
VECTORS = {"BaseColor": (0.68, 0.46, 0.165), "CamoColorA": (0.117, 0.15, 0.034), "CamoColorB": (0.216, 0.058, 0.023)}

# Emissive 0.3 (roof 0.5) picked from 0 / 0.12 / 0.3 / 0.6 in the campaign level (capture_watch_post.py).
# Linear colours. Body: bright dark yellow (Dunkelgelb) with olive and red-brown patches. Roof: cream, the campaign's
# accent colour and the brightest thing on the piece, so the post reads from the map camera. Deck and bracing: a
# darker plain dark yellow, so the roof stands out against it. Rail infill: olive canvas.
INSTANCES = {
    "WP_Body": ("MI_WatchPost_Body", {}, {"EmissiveStrength": 0.3}),
    "WP_Roof": ("MI_WatchPost_Roof", {"BaseColor": (0.87, 0.77, 0.55)},
                {"CamoAmount": 0.0, "BottomShade": 1.0, "Roughness": 0.55, "EmissiveStrength": 0.5}),
    "WP_Lattice": ("MI_WatchPost_Lattice", {"BaseColor": (0.55, 0.37, 0.13)},
                   {"CamoAmount": 0.0, "EmissiveStrength": 0.3}),
    "WP_Net": ("MI_WatchPost_Net", {"BaseColor": (0.155, 0.19, 0.045)}, {"CamoAmount": 0.0, "Roughness": 0.85, "EmissiveStrength": 0.3}),
}


def connect(source, output, target, target_input):
    if not library.connect_material_expressions(source, output, target, target_input):
        lib.log("CONNECT FAILED %s.%s -> %s.%s" % (source.get_name(), output, target.get_name(), target_input))


def node(material, node_class, x, y, **properties):
    expression = library.create_material_expression(material, node_class, x, y)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def ensure_master():
    path = FOLDER + "/" + MASTER_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        if os.environ.get("WATCHPOST_REBUILD_MATERIAL", "") != "1":
            lib.log("master kept as it is")
            return material
        for _attempt in range(12):
            if library.get_num_material_expressions(material) == 0:
                break
            library.delete_all_material_expressions(material)
    else:
        material = lib.asset_tools.create_asset(MASTER_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())

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
    connect(world_position, "", local_position, "")

    pins = [("P", local_position), ("Base", vectors["BaseColor"]), ("A", vectors["CamoColorA"]),
            ("B", vectors["CamoColorB"]), ("Scale", scalars["CamoScale"]), ("Amount", scalars["CamoAmount"]),
            ("GreenCut", scalars["GreenCut"]), ("BrownCut", scalars["BrownCut"]), ("Seed", scalars["Seed"]),
            ("Bottom", scalars["BottomShade"]), ("Top", scalars["ShadeTop"])]
    inputs = []
    for name, _source in pins:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    camo = node(material, unreal.MaterialExpressionCustom, -800, -800, description="WP_WatchPostCamo",
                code=CAMO_CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=inputs)
    for name, source in pins:
        connect(source, "", camo, name)
    library.connect_material_property(camo, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emissive = node(material, unreal.MaterialExpressionMultiply, -500, -500)
    connect(camo, "", emissive, "A")
    connect(scalars["EmissiveStrength"], "", emissive, "B")
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(scalars["Roughness"], "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    lib.save(material)
    lib.log("master built with %d expressions" % library.get_num_material_expressions(material))
    return material


def make_instances(master):
    result = {}
    for slot_name, (name, vectors, scalars) in INSTANCES.items():
        result[slot_name] = lib.make_instance(FOLDER, name, master, vectors=vectors, scalars=scalars)
    return result


def import_mesh():
    before = set(unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=True, include_folder=False))
    task = unreal.AssetImportTask()
    task.filename = FBX
    task.destination_path = FOLDER
    task.destination_name = MESH_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    lib.asset_tools.import_asset_tasks([task])
    after = set(unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=True, include_folder=False))
    created = sorted(after - before)
    lib.log("import created %s" % created)
    mesh = unreal.load_asset(FOLDER + "/" + MESH_NAME)
    if mesh is None:
        lib.log("MESH NOT FOUND after import")
    return mesh, created


def finish(mesh, materials, created):
    for slot_index, static_material in enumerate(mesh.static_materials):
        slot_name = str(static_material.material_slot_name)
        material = next((value for key, value in materials.items() if slot_name.startswith(key)), None)
        if material is None:
            lib.log("NO MATERIAL FOR SLOT %d '%s'" % (slot_index, slot_name))
            continue
        mesh.set_material(slot_index, material)
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    subsystem = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    subsystem.remove_collisions(mesh)
    body_setup = mesh.get_editor_property("body_setup")
    if body_setup is not None:
        # No simple shapes and no per-triangle collision either.
        body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    lib.save(mesh)

    keep = {FOLDER + "/" + MESH_NAME, FOLDER + "/" + MASTER_NAME} | {FOLDER + "/" + v[0] for v in INSTANCES.values()}
    for path in created:
        short = path.split(".")[0]
        if short in keep:
            continue
        asset = unreal.load_asset(short)
        if isinstance(asset, (unreal.MaterialInterface, unreal.Texture)):
            lib.log("deleting importer leftover %s: %s" % (short, unreal.EditorAssetLibrary.delete_asset(short)))


def report(mesh):
    box = mesh.get_bounding_box()
    description = mesh.get_static_mesh_description(0)
    radius = 0.0
    for index in range(description.get_vertex_count()):
        position = description.get_vertex_position(unreal.VertexID(index))
        radius = max(radius, (position.x ** 2 + position.y ** 2) ** 0.5)
    body_setup = mesh.get_editor_property("body_setup")
    shapes = -1
    if body_setup is not None:
        geometry = body_setup.get_editor_property("agg_geom")
        shapes = sum(len(geometry.get_editor_property(kind)) for kind in
                     ("sphere_elems", "box_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems"))
    lib.log("MESH %s tris=%d lods=%d nanite=%s collision_shapes=%d trace=%s" % (
        mesh.get_path_name(), mesh.get_num_triangles(0), mesh.get_num_lods(),
        mesh.get_editor_property("nanite_settings").enabled, shapes,
        body_setup.get_editor_property("collision_trace_flag") if body_setup else None))
    lib.log("  box min (%.2f %.2f %.2f) max (%.2f %.2f %.2f) max radius from pivot %.2f" % (
        box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, radius))
    for slot_index, static_material in enumerate(mesh.static_materials):
        interface = static_material.material_interface
        lib.log("  slot %d %s -> %s" % (slot_index, static_material.material_slot_name,
                                        interface.get_path_name() if interface else None))
    for path in sorted(unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=True, include_folder=False)):
        lib.log("  asset " + path)


def main():
    if not os.path.exists(FBX):
        lib.log("FBX MISSING " + FBX)
        return
    master = ensure_master()
    materials = make_instances(master)
    mesh, created = import_mesh()
    if mesh is None:
        return
    finish(mesh, materials, created)
    report(mesh)


lib.OUT_DIR = LOG_DIR
try:
    main()
except Exception:
    import traceback
    lib.log(traceback.format_exc())
lib.write_log("build.txt")
