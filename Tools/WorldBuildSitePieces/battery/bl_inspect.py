"""Blender (headless): lists the objects of the exported source FBXs with triangle counts and bounds.
Run: blender.exe -b --factory-startup -P bl_inspect.py
"""
import bpy

SRC = "D:/UE5Projects/RTS_Survival/Saved/WorldBuildSitePieces/battery/src/"
FILES = [SRC + "SM_GerPit_Howitzer_LODs.fbx", SRC + "SM_GerPit_Shell_150.fbx",
         "D:/UE5Projects/RTS_Survival/CommanderPortraits/Assets/SFH18/SK_Ger_SFH18Anim.fbx"]

for path in FILES:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, use_anim=False)
    print("FILE", path)
    for obj in sorted(set(bpy.data.objects) - before, key=lambda o: o.name):
        line = "  %s type=%s parent=%s loc=%s rot=%s scale=%s" % (
            obj.name, obj.type, obj.parent.name if obj.parent else None, tuple(round(v, 3) for v in obj.location),
            tuple(round(v, 3) for v in obj.rotation_euler), tuple(round(v, 3) for v in obj.scale))
        if obj.type == "MESH":
            mesh = obj.data
            tris = sum(len(p.vertices) - 2 for p in mesh.polygons)
            world = [obj.matrix_world @ v.co for v in mesh.vertices]
            low = [min(p[i] for p in world) for i in range(3)]
            high = [max(p[i] for p in world) for i in range(3)]
            line += " tris=%d uvs=%d mats=%s min=%s max=%s" % (
                tris, len(mesh.uv_layers), [m.name if m else None for m in mesh.materials],
                tuple(round(v, 3) for v in low), tuple(round(v, 3) for v in high))
        print(line)
