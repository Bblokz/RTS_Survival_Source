"""Imports airfield candidate B ("hangar and apron") and builds its paint. Headless, no GPU:
  sh run_py.sh build_airfield_b.py        (after blender_build.py wrote the FBX and paint_params.json)
Log: Saved/WorldBuildSitePieces/airfield_B/build.txt

Assets in /Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/B (nothing outside it is touched):
  SM_BuildSitePiece_Airfield_B   German airbase building (open roof) + Ju 87 VTO + apron, one static mesh, no collision
  M_AirfieldB_Paint              object-space paint: German camo noise, height shade, emissive term, and for the plane
                                 the theatre markings (yellow wing tips + fuselage band, Balkenkreuz on the wings)
  MI_AirfieldB_*                 one instance per slot
"""
import json
import os
import sys

import unreal

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))), "WorldMapPieces"))
import piece_lib as lib  # noqa: E402

FOLDER = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/B"
MESH_NAME = "SM_BuildSitePiece_Airfield_B"
MASTER_NAME = "M_AirfieldB_Paint"
WORK = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                    "WorldBuildSitePieces", "airfield_B")
FBX = os.path.join(WORK, MESH_NAME + ".fbx")
library = unreal.MaterialEditingLibrary

PAINT_CODE = """// Camo patches in object units (value noise), height shade, and optional plane markings.
struct AFBPaint
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
AFBPaint c;
float3 q=P/max(Scale,0.01)+Seed;
q.z*=1.5;
float e=0.03;
float green=smoothstep(GreenCut-e,GreenCut+e,c.fbm(q));
float brown=smoothstep(BrownCut-e,BrownCut+e,c.fbm(q+float3(37.1,-12.4,5.3)))*(1.0-green);
float3 col=lerp(Base,A,green*Amount);
col=lerp(col,B,brown*Amount);
// Plane markings in the plane's own frame: l = R(-yaw) (p.xy - centre); lx < 0 towards the nose.
float yaw=-radians(PlaneYaw);
float2 d=P.xy-float2(PlaneX,PlaneY);
float2 l=float2(d.x*cos(yaw)-d.y*sin(yaw),d.x*sin(yaw)+d.y*cos(yaw));
float tip=step(TipY,abs(l.y));
float band=step(BandX0,l.x)*step(l.x,BandX1);
float2 k=float2(abs(l.x-CrossX),abs(abs(l.y)-CrossY));
float outer=max(step(k.x,CrossL+CrossB)*step(k.y,CrossW+CrossB),step(k.x,CrossW+CrossB)*step(k.y,CrossL+CrossB));
float inner=max(step(k.x,CrossL)*step(k.y,CrossW),step(k.x,CrossW)*step(k.y,CrossL));
col=lerp(col,Accent,saturate(tip+band)*Mark);
col=lerp(col,float3(0.85,0.85,0.82),outer*Mark);
col=lerp(col,float3(0.02,0.02,0.02),inner*Mark);
float shade=lerp(Bottom,1.0,saturate(P.z/max(Top,0.01)));
return col*shade;"""

SCALARS = {"CamoAmount": 1.0, "CamoScale": 3.0, "GreenCut": 0.53, "BrownCut": 0.56, "Seed": 3.0,
           "BottomShade": 0.75, "ShadeTop": 8.0, "Roughness": 0.7, "EmissiveStrength": 0.8, "MarkAmount": 0.0,
           "PlaneX": 0.0, "PlaneY": 0.0, "PlaneYaw": 0.0, "TipY": 1000.0, "BandX0": 1000.0, "BandX1": 1000.0,
           "CrossX": 1000.0, "CrossY": 1000.0, "CrossL": 0.0, "CrossW": 0.0, "CrossB": 0.0}
VECTORS = {"BaseColor": (0.68, 0.46, 0.165), "CamoColorA": (0.117, 0.15, 0.034), "CamoColorB": (0.216, 0.058, 0.023),
           "AccentColor": (1.0, 0.62, 0.0)}
PLANE_KEYS = ("PlaneX", "PlaneY", "PlaneYaw", "TipY", "BandX0", "BandX1", "CrossX", "CrossY", "CrossL", "CrossW",
              "CrossB")

# Linear colours. Walls: bright Dunkelgelb with olive and red-brown patches (as the approved watch post). Roof flaps:
# olive with Dunkelgelb patches, so the open flaps frame the bay. Floor: light concrete; pad: off-white. Plane: RLM
# 70/71 style dark greens with the yellow theatre markings (the accent) and Balkenkreuz, a dark silhouette on the
# light apron. Apron: light concrete with white lines and yellow threshold bars (the same accent).
INSTANCES = {
    "HB_Walls": ("MI_AirfieldB_Walls", {}, {}),
    "HB_Roof": ("MI_AirfieldB_Roof", {"BaseColor": (0.22, 0.26, 0.075), "CamoColorA": (0.55, 0.38, 0.13),
                                       "CamoColorB": (0.12, 0.14, 0.05)}, {"GreenCut": 0.6, "BrownCut": 0.6}),
    "HB_Floor": ("MI_AirfieldB_Floor", {"BaseColor": (0.5, 0.5, 0.46)}, {"CamoAmount": 0.0, "BottomShade": 1.0}),
    "HB_Pad": ("MI_AirfieldB_Pad", {"BaseColor": (0.8, 0.8, 0.74)}, {"CamoAmount": 0.0, "BottomShade": 1.0}),
    "PL_Body": ("MI_AirfieldB_Plane", {"BaseColor": (0.075, 0.12, 0.05), "CamoColorA": (0.04, 0.065, 0.03),
                                       "CamoColorB": (0.075, 0.12, 0.05)},
                {"CamoScale": 1.6, "GreenCut": 0.5, "BottomShade": 0.85, "Roughness": 0.5, "MarkAmount": 1.0}),
    "AP_Concrete": ("MI_AirfieldB_Apron", {"BaseColor": (0.6, 0.58, 0.52)}, {"CamoAmount": 0.0, "BottomShade": 0.6,
                                                                            "ShadeTop": 0.6, "Roughness": 0.85}),
    "AP_Mark": ("MI_AirfieldB_Mark", {"BaseColor": (0.9, 0.9, 0.86)}, {"CamoAmount": 0.0, "BottomShade": 1.0}),
    "AP_Accent": ("MI_AirfieldB_Accent", {"BaseColor": (1.0, 0.62, 0.0)}, {"CamoAmount": 0.0, "BottomShade": 1.0}),
}


def connect(source, output, target, target_input):
    if not library.connect_material_expressions(source, output, target, target_input):
        lib.log("CONNECT FAILED %s.%s -> %s.%s" % (source.get_name(), output, target.get_name(), target_input))


def node(material, node_class, x, y, **properties):
    expression = library.create_material_expression(material, node_class, x, y)
    for name, value in properties.items():
        expression.set_editor_property(name, value)
    return expression


def build_master():
    path = FOLDER + "/" + MASTER_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    material = lib.asset_tools.create_asset(MASTER_NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    scalars = {name: node(material, unreal.MaterialExpressionScalarParameter, -1400, -600 + index * 110,
                          parameter_name=name, default_value=value)
               for index, (name, value) in enumerate(SCALARS.items())}
    vectors = {name: node(material, unreal.MaterialExpressionVectorParameter, -1700, -1100 + index * 160,
                          parameter_name=name, default_value=unreal.LinearColor(value[0], value[1], value[2], 1.0))
               for index, (name, value) in enumerate(VECTORS.items())}
    world_position = node(material, unreal.MaterialExpressionWorldPosition, -1400, -1400)
    local_position = node(material, unreal.MaterialExpressionTransformPosition, -1150, -1400,
                          transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                          transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    connect(world_position, "", local_position, "")
    pins = [("P", local_position), ("Base", vectors["BaseColor"]), ("A", vectors["CamoColorA"]),
            ("B", vectors["CamoColorB"]), ("Accent", vectors["AccentColor"]), ("Scale", scalars["CamoScale"]),
            ("Amount", scalars["CamoAmount"]), ("GreenCut", scalars["GreenCut"]), ("BrownCut", scalars["BrownCut"]),
            ("Seed", scalars["Seed"]), ("Bottom", scalars["BottomShade"]), ("Top", scalars["ShadeTop"]),
            ("Mark", scalars["MarkAmount"])] + [(key, scalars[key]) for key in PLANE_KEYS]
    inputs = []
    for name, _source in pins:
        custom_input = unreal.CustomInput()
        custom_input.set_editor_property("input_name", name)
        inputs.append(custom_input)
    paint = node(material, unreal.MaterialExpressionCustom, -800, -800, description="AFB_Paint",
                 code=PAINT_CODE, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, inputs=inputs)
    for name, source in pins:
        connect(source, "", paint, name)
    library.connect_material_property(paint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    emissive = node(material, unreal.MaterialExpressionMultiply, -500, -500)
    connect(paint, "", emissive, "A")
    connect(scalars["EmissiveStrength"], "", emissive, "B")
    library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    library.connect_material_property(scalars["Roughness"], "", unreal.MaterialProperty.MP_ROUGHNESS)
    library.recompile_material(material)
    lib.save(material)
    lib.log("master built with %d expressions" % library.get_num_material_expressions(material))
    return material


def make_instances(master, plane_params):
    result = {}
    for slot_name, (name, vectors, scalars) in INSTANCES.items():
        scalars = dict(scalars)
        if slot_name == "PL_Body":
            scalars.update({key: float(plane_params[key]) for key in PLANE_KEYS})
        result[slot_name] = lib.make_instance(FOLDER, name, master, vectors=vectors, scalars=scalars)
    return result


def import_mesh():
    before = set(unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False, include_folder=False))
    task = unreal.AssetImportTask()
    task.filename = FBX
    task.destination_path = FOLDER
    task.destination_name = MESH_NAME
    task.automated = True
    task.replace_existing = True
    task.save = False
    lib.asset_tools.import_asset_tasks([task])
    after = set(unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False, include_folder=False))
    created = sorted(after - before)
    lib.log("import created %s" % created)
    return unreal.load_asset(FOLDER + "/" + MESH_NAME), created


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
        body_setup.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    lib.save(mesh)
    keep = {FOLDER + "/" + MESH_NAME, FOLDER + "/" + MASTER_NAME} | {FOLDER + "/" + v[0] for v in INSTANCES.values()}
    for path in created:
        short = path.split(".")[0]
        if short in keep or not short.startswith(FOLDER + "/"):
            continue
        asset = unreal.load_asset(short)
        if isinstance(asset, (unreal.MaterialInterface, unreal.Texture)):
            lib.log("deleting importer leftover %s: %s" % (short, unreal.EditorAssetLibrary.delete_asset(short)))


def report(mesh):
    box = mesh.get_bounding_box()
    description = mesh.get_static_mesh_description(0)
    radius = 0.0
    highest = None
    lowest_x = None
    for index in range(description.get_vertex_count()):
        position = description.get_vertex_position(unreal.VertexID(index))
        radius = max(radius, (position.x ** 2 + position.y ** 2) ** 0.5)
        if highest is None or position.z > highest.z:
            highest = position
        # The plane's nose: the most -X point above the apron's top that is near the centre line.
        if abs(position.y) < 3.0 and position.z > 1.2 and (lowest_x is None or position.x < lowest_x.x):
            lowest_x = position
    body_setup = mesh.get_editor_property("body_setup")
    shapes = -1
    if body_setup is not None:
        geometry = body_setup.get_editor_property("agg_geom")
        shapes = sum(len(geometry.get_editor_property(kind)) for kind in
                     ("sphere_elems", "box_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems"))
    lib.log("MESH %s tris=%d lods=%d nanite=%s collision_shapes=%d" % (
        mesh.get_path_name(), mesh.get_num_triangles(0), mesh.get_num_lods(),
        mesh.get_editor_property("nanite_settings").enabled, shapes))
    lib.log("  box min (%.2f %.2f %.2f) max (%.2f %.2f %.2f) max radius from pivot %.2f" % (
        box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, radius))
    lib.log("  highest vertex (tower, expect x > 0) %s" % highest)
    lib.log("  plane nose (expect x about -13.4, y about -1.0 for yaw +12) %s" % lowest_x)
    lib.log("  CHECK radius<=20 %s base z=0 %s tris<15000 %s" % (radius <= 20.0, abs(box.min.z) < 0.01,
                                                                 mesh.get_num_triangles(0) < 15000))
    for slot_index, static_material in enumerate(mesh.static_materials):
        interface = static_material.material_interface
        lib.log("  slot %d %s -> %s" % (slot_index, static_material.material_slot_name,
                                        interface.get_path_name() if interface else None))
    for path in sorted(unreal.EditorAssetLibrary.list_assets(FOLDER, recursive=False, include_folder=False)):
        lib.log("  asset " + path)


def main():
    if not os.path.exists(FBX):
        lib.log("FBX MISSING " + FBX)
        return
    with open(os.path.join(WORK, "paint_params.json"), encoding="utf-8") as handle:
        plane_params = json.load(handle)
    if not unreal.EditorAssetLibrary.does_directory_exist(FOLDER):
        unreal.EditorAssetLibrary.make_directory(FOLDER)
    master = build_master()
    materials = make_instances(master, plane_params)
    mesh, created = import_mesh()
    if mesh is None:
        lib.log("MESH NOT FOUND after import")
        return
    finish(mesh, materials, created)
    report(mesh)


lib.OUT_DIR = WORK
try:
    main()
except Exception:
    import traceback
    lib.log(traceback.format_exc())
lib.write_log("build.txt")
