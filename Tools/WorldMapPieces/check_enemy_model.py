"""Checks one enemy map object prototype against the contract in Saved/WorldMapUIDesigns/EnemyObjects/BRIEF.md.

Plain Python, no Unreal:  python -I check_enemy_model.py <Item>      (Item: HQ, Wall, Factory, ...)
Prints one line per check and ends with RESULT: PASS or RESULT: FAIL.
"""
import json
import os
import struct
import sys

ROOT = "D:/UE5Projects/RTS_Survival/Saved/WorldMapUIDesigns/EnemyObjects"
MAX_TRIANGLES = 8000
HALF_FOOTPRINT_M = 0.90
MAX_HEIGHT_M = 1.50
TOLERANCE_M = 0.005


def read_glb(path):
    with open(path, "rb") as handle:
        magic, _version, _length = struct.unpack("<4sII", handle.read(12))
        if magic != b"glTF":
            raise ValueError("not a binary glTF file")
        chunk_length, _chunk_type = struct.unpack("<II", handle.read(8))
        return json.loads(handle.read(chunk_length).decode("utf-8"))


def node_matrix_is_identity(node):
    return not any(key in node for key in ("matrix", "translation", "rotation", "scale"))


def main(item):
    folder = os.path.join(ROOT, item, "model")
    name = "SM_WorldPiece_Enemy" + item
    glb_path = os.path.join(folder, name + ".glb")
    results = []

    def check(ok, text):
        results.append(ok)
        print(("ok   " if ok else "FAIL ") + text)

    check(os.path.exists(glb_path), "file " + glb_path)
    if not os.path.exists(glb_path):
        print("RESULT: FAIL")
        return
    document = read_glb(glb_path)
    meshes = document.get("meshes", [])
    check(len(meshes) == 1, "one mesh (found %d)" % len(meshes))
    check(all(node_matrix_is_identity(node) for node in document.get("nodes", [])), "no node transforms")
    accessors = document["accessors"]
    triangles = 0
    low, high = [1e9] * 3, [-1e9] * 3
    used_materials = set()
    for mesh in meshes:
        for primitive in mesh["primitives"]:
            triangles += accessors[primitive["indices"]]["count"] // 3
            position = accessors[primitive["attributes"]["POSITION"]]
            low = [min(a, b) for a, b in zip(low, position["min"])]
            high = [max(a, b) for a, b in zip(high, position["max"])]
            used_materials.add(document["materials"][primitive["material"]]["name"])
            check("NORMAL" in primitive["attributes"] and "TEXCOORD_0" in primitive["attributes"],
                  "primitive '%s' has normals and UVs" % document["materials"][primitive["material"]]["name"])
    check(triangles <= MAX_TRIANGLES, "triangles %d (limit %d)" % (triangles, MAX_TRIANGLES))
    # glTF: +Y up, metres. Footprint is X and Z.
    check(abs(low[1]) <= TOLERANCE_M, "bottom at Y 0 (%.4f m)" % low[1])
    check(high[1] <= MAX_HEIGHT_M + TOLERANCE_M, "height %.3f m (limit %.2f)" % (high[1], MAX_HEIGHT_M))
    for axis, label in ((0, "X"), (2, "Z")):
        check(low[axis] >= -HALF_FOOTPRINT_M - TOLERANCE_M and high[axis] <= HALF_FOOTPRINT_M + TOLERANCE_M,
              "%s within +-%.2f m (%.3f .. %.3f)" % (label, HALF_FOOTPRINT_M, low[axis], high[axis]))
    check(high[0] - low[0] >= 1.5 and high[2] - low[2] >= 1.5, "ground slab fills the footprint")
    prefix = "M_WP_EN_%s_" % item
    texture_dir = os.path.join(folder, "textures")
    for material in sorted(used_materials):
        check(material.startswith(prefix), "material name '%s' starts with %s" % (material, prefix))
        short = material[len(prefix):] if material.startswith(prefix) else material
        stem = os.path.join(texture_dir, "T_WP_EN_%s_%s" % (item, short))
        has_color = os.path.exists(stem + "_D.png")
        if "Concrete" in short:
            print("     slot '%s' gets the shared concrete material" % material)
        elif has_color:
            check(os.path.exists(stem + "_N.png") and os.path.exists(stem + "_ORM.png"),
                  "textures _D, _N and _ORM for '%s'" % short)
        else:
            print("     slot '%s' has no textures: flat colour from the file" % material)
    check(not any("Marking" in material or "Red" in material for material in used_materials),
          "no marking in the model")
    for file_name in ("preview.png", "HANDOVER.md"):
        check(os.path.exists(os.path.join(folder, file_name)), "file " + file_name)
    print("materials: " + ", ".join(sorted(used_materials)))
    print("RESULT: " + ("PASS" if all(results) else "FAIL"))


if __name__ == "__main__":
    main(sys.argv[1])
