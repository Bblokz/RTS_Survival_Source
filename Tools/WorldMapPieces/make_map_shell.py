"""The artillery shell of the world map fire mission: a glowing spindle with a long tapering tail.

Two modes in one file:
  python -I make_map_shell.py            writes Saved/WorldMapPieces/SM_WorldMapShell.glb (plain Python)
  run_build.sh make_map_shell.py         imports it into /Game/RTS_Survival/WorldCampaign/BatteryFire (headless Unreal)
The game's own shell (NS_75mmShell) is a thin trail without a body and cannot be seen at map size.
Mesh frame in Unreal: nose at the origin pointing +X (the fire mission code turns +X along the flight path), tail on -X.
UV: U runs 0 at the nose to 1 at the tail tip; the material fades and cools along U.
"""
import json
import math
import os
import struct

GLB_PATH = "D:/UE5Projects/RTS_Survival/Saved/WorldMapPieces/SM_WorldMapShell.glb"
SIDES = 12
# (distance behind the nose in metres, radius in metres): short hot head, long thin tail.
PROFILE = [(0.00, 0.000), (0.04, 0.030), (0.10, 0.045), (0.22, 0.036), (0.60, 0.022), (1.20, 0.012), (2.00, 0.000)]
LENGTH = PROFILE[-1][0]


def write_glb():
    positions, normals, uvs, indices = [], [], [], []
    for distance, radius in PROFILE:
        for side in range(SIDES + 1):
            angle = 2.0 * math.pi * side / SIDES
            # glTF: +Y up. The importer puts glTF X on Unreal X; the tail goes to -X.
            positions.append((-distance, radius * math.cos(angle), radius * math.sin(angle)))
            normals.append((0.0, math.cos(angle), math.sin(angle)))
            uvs.append((distance / LENGTH, side / SIDES))
    ring = SIDES + 1
    for row in range(len(PROFILE) - 1):
        for side in range(SIDES):
            a, b = row * ring + side, row * ring + side + 1
            c, d = (row + 1) * ring + side, (row + 1) * ring + side + 1
            indices += [a, c, b, b, c, d]
    buffer = bytearray()
    views, accessors = [], []

    def add(data, component, kind, count, target, low=None, high=None):
        while len(buffer) % 4:
            buffer.append(0)
        views.append({"buffer": 0, "byteOffset": len(buffer), "byteLength": len(data), "target": target})
        buffer.extend(data)
        accessor = {"bufferView": len(views) - 1, "componentType": component, "count": count, "type": kind}
        if low is not None:
            accessor["min"], accessor["max"] = low, high
        accessors.append(accessor)
        return len(accessors) - 1

    low = [min(p[i] for p in positions) for i in range(3)]
    high = [max(p[i] for p in positions) for i in range(3)]
    position = add(b"".join(struct.pack("<3f", *p) for p in positions), 5126, "VEC3", len(positions), 34962, low, high)
    normal = add(b"".join(struct.pack("<3f", *n) for n in normals), 5126, "VEC3", len(positions), 34962)
    uv = add(b"".join(struct.pack("<2f", *t) for t in uvs), 5126, "VEC2", len(positions), 34962)
    index = add(struct.pack("<%dH" % len(indices), *indices), 5123, "SCALAR", len(indices), 34963)
    name = "SM_WorldMapShell"
    document = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
                "nodes": [{"mesh": 0, "name": name}],
                "meshes": [{"name": name, "primitives": [{
                    "attributes": {"POSITION": position, "NORMAL": normal, "TEXCOORD_0": uv}, "indices": index,
                    "material": 0}]}],
                "materials": [{"name": "M_WorldMapShell", "doubleSided": True, "pbrMetallicRoughness": {
                    "baseColorFactor": [1.0, 0.7, 0.3, 1.0], "roughnessFactor": 1.0, "metallicFactor": 0.0}}],
                "accessors": accessors, "bufferViews": views, "buffers": [{"byteLength": len(buffer)}]}
    text = json.dumps(document).encode("utf-8")
    text += b" " * (-len(text) % 4)
    while len(buffer) % 4:
        buffer.append(0)
    os.makedirs(os.path.dirname(GLB_PATH), exist_ok=True)
    with open(GLB_PATH, "wb") as handle:
        handle.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(text) + 8 + len(buffer)))
        handle.write(struct.pack("<II", len(text), 0x4E4F534A) + text)
        handle.write(struct.pack("<II", len(buffer), 0x004E4942) + bytes(buffer))
    print("wrote %s: %d triangles" % (GLB_PATH, len(indices) // 3))


def import_into_unreal():
    import sys

    import unreal

    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import piece_lib as lib

    folder = "/Game/RTS_Survival/WorldCampaign/BatteryFire"
    library = unreal.MaterialEditingLibrary
    material_path = folder + "/M_WorldMapShell"
    if unreal.EditorAssetLibrary.does_asset_exist(material_path):
        material = unreal.load_asset(material_path)
        lib.log("material kept as it is")
    else:
        material = lib.asset_tools.create_asset("M_WorldMapShell", folder, unreal.Material, unreal.MaterialFactoryNew())
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        material.set_editor_property("two_sided", True)

        def node(node_class, x, y, **properties):
            expression = library.create_material_expression(material, node_class, x, y)
            for name, value in properties.items():
                expression.set_editor_property(name, value)
            return expression

        coordinates = node(unreal.MaterialExpressionTextureCoordinate, -1100, 0)
        along = node(unreal.MaterialExpressionComponentMask, -900, 0, r=True, g=False, b=False, a=False)
        library.connect_material_expressions(coordinates, "", along, "")
        head = node(unreal.MaterialExpressionVectorParameter, -900, -300, parameter_name="HeadColor",
                    default_value=unreal.LinearColor(1.0, 0.85, 0.55, 1.0))
        tail = node(unreal.MaterialExpressionVectorParameter, -900, -120, parameter_name="TailColor",
                    default_value=unreal.LinearColor(1.0, 0.32, 0.06, 1.0))
        colour = node(unreal.MaterialExpressionLinearInterpolate, -600, -200)
        library.connect_material_expressions(head, "", colour, "A")
        library.connect_material_expressions(tail, "", colour, "B")
        library.connect_material_expressions(along, "", colour, "Alpha")
        # Brightness falls off along the tail: (1 - U) ^ Falloff, times Intensity.
        one_minus = node(unreal.MaterialExpressionOneMinus, -700, 150)
        library.connect_material_expressions(along, "", one_minus, "")
        falloff = node(unreal.MaterialExpressionScalarParameter, -700, 300, parameter_name="Falloff", default_value=1.6)
        fade = node(unreal.MaterialExpressionPower, -500, 200)
        library.connect_material_expressions(one_minus, "", fade, "Base")
        library.connect_material_expressions(falloff, "", fade, "Exp")
        intensity = node(unreal.MaterialExpressionScalarParameter, -500, 400, parameter_name="Intensity",
                         default_value=14.0)
        strength = node(unreal.MaterialExpressionMultiply, -300, 250)
        library.connect_material_expressions(fade, "", strength, "A")
        library.connect_material_expressions(intensity, "", strength, "B")
        emissive = node(unreal.MaterialExpressionMultiply, -100, 0)
        library.connect_material_expressions(colour, "", emissive, "A")
        library.connect_material_expressions(strength, "", emissive, "B")
        library.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        library.recompile_material(material)
        lib.save(material)

    mesh, created = lib.import_mesh(GLB_PATH, folder, "SM_WorldMapShell")
    if mesh is None:
        lib.log("IMPORT FAILED")
    else:
        lib.finish_mesh(mesh, {"M_WorldMapShell": material}, created, {material_path})
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).remove_collisions(mesh)
        lib.save(mesh)
    lib.write_log("map_shell.txt")


try:
    import unreal  # noqa: F401
    IN_UNREAL = True
except ImportError:
    IN_UNREAL = False

if IN_UNREAL:
    import_into_unreal()
else:
    write_glb()
