"""Read-only dump of Ger_Berlin for the detail pass (2026-11-26). Runs in a -nullrhi pythonscript commandlet (no GPU):

    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this> -unattended -nop4 -nullrhi -nosplash

Writes Saved/Berlin/detail/dump/:
  actors.json   every actor: label, class, folder, location, yaw, scale, first static mesh + its local box, decal
                material/size, and for instanced meshes every instance (mesh, x, y, z, yaw, scale)
  land_height.r16 / land_<layer>.r8 + land_grid.txt   the whole landscape (heights + paint weights)
"""
import json
import os

import unreal

MAP = "/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin"
OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/detail/dump/"
os.makedirs(OUT, exist_ok=True)

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
rows, mesh_box, land = [], {}, None


def box_of(sm):
    key = sm.get_path_name()
    if key not in mesh_box:
        b = sm.get_bounding_box()
        mesh_box[key] = [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1),
                         round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)]
    return key.split(".")[-1], mesh_box[key]


for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    cls = a.get_class().get_name()
    if cls in ("WorldDataLayers", "WorldPartitionMiniMap", "Brush"):
        continue
    if isinstance(a, unreal.LandscapeProxy) and land is None:
        land = a
    loc, rot = a.get_actor_location(), a.get_actor_rotation()
    row = {"label": a.get_actor_label(), "class": cls, "folder": str(a.get_folder_path()),
           "x": round(loc.x, 1), "y": round(loc.y, 1), "z": round(loc.z, 1), "yaw": round(rot.yaw, 2),
           "scale": round(a.get_actor_scale3d().x, 3)}
    if cls == "DecalActor":
        d = a.decal
        m = d.get_decal_material()
        row["mat"] = m.get_name() if m else None
        s = d.get_editor_property("decal_size")
        row["size"] = [round(s.x, 1), round(s.y, 1), round(s.z, 1)]
        row["fade"] = round(d.get_editor_property("fade_screen_size"), 4)
    else:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            if isinstance(c, unreal.InstancedStaticMeshComponent) or c.static_mesh is None:
                continue
            row["mesh"], row["box"] = box_of(c.static_mesh)
            break
        inst = []
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            if c.static_mesh is None:
                continue
            nm, bx = box_of(c.static_mesh)
            for i in range(c.get_instance_count()):
                tr = c.get_instance_transform(i, True)
                inst.append([nm, round(tr.translation.x, 1), round(tr.translation.y, 1), round(tr.translation.z, 1),
                             round(tr.rotation.rotator().yaw, 1), round(tr.scale3d.x, 2)])
        if inst:
            row["instances"] = inst
        dc = a.get_components_by_class(unreal.DecalComponent)
        if dc:
            row["decal_comps"] = len(dc)
    rows.append(row)

json.dump({"map": MAP, "actors": rows, "mesh_box": mesh_box}, open(OUT + "actors.json", "w"))
print("DETAIL_DUMP actors", len(rows))

if land is not None:
    info = unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land)
    print("DETAIL_DUMP grid", repr(info))
    x0, y0, x1, y1, comp = [int(v) for v in info.split()]
    tl = land.get_editor_property("target_layers")
    names = [str(k) for k in tl.keys()]
    infos = [tl[k].get_editor_property("layer_info_obj") for k in tl.keys()]
    ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
        land, x0, y0, x1, y1, OUT + "land_height.r16", infos, [OUT + "land_%s.r8" % n for n in names])
    loc, sc = land.get_actor_location(), land.get_actor_scale3d()
    with open(OUT + "land_grid.txt", "w") as f:
        f.write("%d %d %d %d\n%f %f %f\n%f %f %f\n%s\n" % (x0, y0, x1, y1, loc.x, loc.y, loc.z, sc.x, sc.y, sc.z,
                                                         " ".join(names)))
    print("DETAIL_DUMP landscape", ok, info, names)
