"""Writes the shared plinth of the enemy map object prototypes, in three versions of the Soviet marking.

Plain Python, no Unreal:  python -I make_enemy_base.py
Output: Saved/WorldMapUIDesigns/EnemyObjects/_Base/SM_WorldPiece_EnemyBase_<A|B|C>.glb  (glTF, +Y up, metres)
  A  dark steel plinth, red top rim and red band round the side
  B  dark steel plinth, and a red ring lying on the map round it
  C  the whole plinth red, with four red corner posts
Slots: M_WP_EN_Base_Steel, M_WP_EN_Base_Red. The piece's own ground slab (180 cm octagon) sits on top at 12 cm.
"""
import json
import math
import os
import struct

OUT = "D:/UE5Projects/RTS_Survival/Saved/WorldMapUIDesigns/EnemyObjects/_Base"
PLINTH_SIZE, PLINTH_CUT, PLINTH_HEIGHT = 2.00, 0.29, 0.12
SLAB_SIZE, SLAB_CUT = 1.80, 0.26
STEEL, RED = "M_WP_EN_Base_Steel", "M_WP_EN_Base_Red"
COLOURS = {STEEL: (0.035, 0.038, 0.042, 0.45, 1.0), RED: (0.42, 0.018, 0.012, 0.42, 0.0)}


def octagon(size, cut):
    """Corner points (x, z), counter-clockwise seen from above (+Y)."""
    half = size / 2.0
    points = [(half, -half + cut), (half, half - cut), (half - cut, half), (-half + cut, half),
              (-half, half - cut), (-half, -half + cut), (-half + cut, -half), (half - cut, -half)]
    # Seen from +Y with X right and Z towards the viewer, this order is clockwise; reverse for counter-clockwise.
    return points[::-1]


class Model:
    def __init__(self):
        self.parts = {}

    def quad(self, material, corners):
        """corners: four (x, y, z) counter-clockwise seen from outside."""
        self.polygon(material, corners)

    def polygon(self, material, corners):
        a, b, c = corners[0], corners[1], corners[2]
        u = [b[i] - a[i] for i in range(3)]
        v = [c[i] - a[i] for i in range(3)]
        normal = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
        length = math.sqrt(sum(n * n for n in normal)) or 1.0
        normal = [n / length for n in normal]
        part = self.parts.setdefault(material, {"positions": [], "normals": [], "uvs": [], "indices": []})
        start = len(part["positions"])
        for corner in corners:
            part["positions"].append(corner)
            part["normals"].append(normal)
            part["uvs"].append((corner[0] + corner[2], corner[1] + corner[2] * 0.5))
        for index in range(1, len(corners) - 1):
            part["indices"] += [start, start + index, start + index + 1]

    def ring_wall(self, material, outline, low, high, outward=True):
        count = len(outline)
        for index in range(count):
            (x0, z0), (x1, z1) = outline[index], outline[(index + 1) % count]
            corners = [(x0, low, z0), (x1, low, z1), (x1, high, z1), (x0, high, z0)]
            self.quad(material, corners if outward else corners[::-1])

    def flat_ring(self, material, outer, inner, height):
        count = len(outer)
        for index in range(count):
            following = (index + 1) % count
            self.quad(material, [(outer[index][0], height, outer[index][1]),
                                 (outer[following][0], height, outer[following][1]),
                                 (inner[following][0], height, inner[following][1]),
                                 (inner[index][0], height, inner[index][1])])

    def cap(self, material, outline, height):
        self.polygon(material, [(x, height, z) for x, z in outline])

    def box(self, material, centre, half, low, high):
        outline = [(centre[0] + half, centre[1] - half), (centre[0] + half, centre[1] + half),
                   (centre[0] - half, centre[1] + half), (centre[0] - half, centre[1] - half)][::-1]
        self.ring_wall(material, outline, low, high)
        self.cap(material, outline, high)

    def write(self, path, name):
        buffer = bytearray()
        views, accessors, primitives, materials = [], [], [], []

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

        for material, part in self.parts.items():
            positions = part["positions"]
            low = [min(p[i] for p in positions) for i in range(3)]
            high = [max(p[i] for p in positions) for i in range(3)]
            position = add(b"".join(struct.pack("<3f", *p) for p in positions), 5126, "VEC3", len(positions),
                           34962, low, high)
            normal = add(b"".join(struct.pack("<3f", *n) for n in part["normals"]), 5126, "VEC3", len(positions),
                         34962)
            uv = add(b"".join(struct.pack("<2f", *t) for t in part["uvs"]), 5126, "VEC2", len(positions), 34962)
            index = add(struct.pack("<%dH" % len(part["indices"]), *part["indices"]), 5123, "SCALAR",
                        len(part["indices"]), 34963)
            red, green, blue, roughness, metallic = COLOURS[material]
            materials.append({"name": material, "pbrMetallicRoughness": {
                "baseColorFactor": [red, green, blue, 1.0], "roughnessFactor": roughness,
                "metallicFactor": metallic}})
            primitives.append({"attributes": {"POSITION": position, "NORMAL": normal, "TEXCOORD_0": uv},
                               "indices": index, "material": len(materials) - 1})
        document = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}],
                    "nodes": [{"mesh": 0, "name": name}], "meshes": [{"name": name, "primitives": primitives}],
                    "materials": materials, "accessors": accessors, "bufferViews": views,
                    "buffers": [{"byteLength": len(buffer)}]}
        text = json.dumps(document).encode("utf-8")
        text += b" " * (-len(text) % 4)
        while len(buffer) % 4:
            buffer.append(0)
        with open(path, "wb") as handle:
            handle.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(text) + 8 + len(buffer)))
            handle.write(struct.pack("<II", len(text), 0x4E4F534A) + text)
            handle.write(struct.pack("<II", len(buffer), 0x004E4942) + bytes(buffer))
        triangles = sum(len(part["indices"]) // 3 for part in self.parts.values())
        print("%s: %d triangles, slots %s" % (os.path.basename(path), triangles, list(self.parts)))


def plinth(model, side_material, band_material, rim_material):
    outer = octagon(PLINTH_SIZE, PLINTH_CUT)
    inner = octagon(SLAB_SIZE, SLAB_CUT)
    model.ring_wall(side_material, outer, 0.0, 0.035)
    model.ring_wall(band_material, outer, 0.035, 0.090)
    model.ring_wall(side_material, outer, 0.090, PLINTH_HEIGHT)
    model.flat_ring(rim_material, outer, inner, PLINTH_HEIGHT)
    # Under the piece's slab: closes the plinth if a piece is smaller than its slab.
    model.cap(side_material, inner, PLINTH_HEIGHT - 0.002)


def build(version):
    model = Model()
    if version == "A":
        plinth(model, STEEL, RED, RED)
    elif version == "B":
        plinth(model, STEEL, STEEL, STEEL)
        # A red ring on the map, 14 cm clear of the plinth and 20 cm wide, 0.4 cm thick so it does not flicker.
        ring_outer = octagon(PLINTH_SIZE + 0.68, PLINTH_CUT + 0.20)
        ring_inner = octagon(PLINTH_SIZE + 0.28, PLINTH_CUT + 0.08)
        model.flat_ring(RED, ring_outer, ring_inner, 0.004)
        model.ring_wall(RED, ring_outer, 0.0, 0.004)
        model.ring_wall(RED, ring_inner, 0.0, 0.004, outward=False)
    else:
        plinth(model, RED, RED, RED)
        reach = PLINTH_SIZE / 2.0 - PLINTH_CUT / 2.0
        for sign_x in (-1.0, 1.0):
            for sign_z in (-1.0, 1.0):
                model.box(RED, (sign_x * reach, sign_z * reach), 0.035, PLINTH_HEIGHT, PLINTH_HEIGHT + 0.26)
    name = "SM_WorldPiece_EnemyBase_" + version
    model.write(os.path.join(OUT, name + ".glb"), name)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for letter in ("A", "B", "C"):
        build(letter)
