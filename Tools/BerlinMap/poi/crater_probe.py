# Read-only probe for the start crater build (sent to crater_editor_server.py). Exports the landscape region heights +
# layer weights to Saved/BerlinCrater/land/orig_*, writes grid.json, and lists what stands in the edit footprint
# (actors, foliage / HISM instances) plus the start resources' components. Nothing is modified.
import json
import math
import os

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/BerlinCrater/land/"
REGION = (14400.0, 20200.0, 44200.0, 49200.0)      # x0, x1, y0, y1 (cm); the dip itself ends inside X 14500-20100, Y 44300-49100
os.makedirs(OUT, exist_ok=True)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
print("world", world.get_path_name())
actors = sub.get_all_level_actors()
lands = [a for a in actors if isinstance(a, unreal.LandscapeProxy)]
print("landscapes", [(a.get_actor_label(), a.get_class().get_name()) for a in lands])
land = lands[0]
loc, sc = land.get_actor_location(), land.get_actor_scale3d()
print("land loc", loc, "scale", sc)
print("grid", unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(land))
i0 = int(math.floor((REGION[0] - loc.x) / sc.x))
i1 = int(math.ceil((REGION[1] - loc.x) / sc.x))
j0 = int(math.floor((REGION[2] - loc.y) / sc.y))
j1 = int(math.ceil((REGION[3] - loc.y) / sc.y))
tl = land.get_editor_property("target_layers")
names = [str(k) for k in tl.keys()]
infos = [tl[k].get_editor_property("layer_info_obj") for k in tl.keys()]
ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(
    land, i0, j0, i1, j1, OUT + "orig_height.r16", infos, [OUT + "orig_%s.r8" % n for n in names])
print("export", ok, (i0, j0, i1, j1), names)
json.dump(dict(i0=i0, j0=j0, i1=i1, j1=j1, lx=loc.x, ly=loc.y, lz=loc.z, sx=sc.x, sy=sc.y, sz=sc.z, layers=names),
          open(OUT + "grid.json", "w"), indent=1)


def inside(x, y, pad=0.0):
    return REGION[0] - pad <= x <= REGION[1] + pad and REGION[2] - pad <= y <= REGION[3] + pad


rows = []
for a in actors:
    if a in lands:
        continue
    p = a.get_actor_location()
    o, e = a.get_actor_bounds(False)
    if not (inside(p.x, p.y) or (o.x + e.x >= REGION[0] and o.x - e.x <= REGION[1] and o.y + e.y >= REGION[2]
                                 and o.y - e.y <= REGION[3] and e.x < 6000 and e.y < 6000)):
        continue
    rows.append(dict(label=a.get_actor_label(), cls=a.get_class().get_name(), x=round(p.x), y=round(p.y), z=round(p.z),
                     ex=round(e.x), ey=round(e.y), ez=round(e.z), folder=str(a.get_folder_path())))
print("actors in/over region:", len(rows))
for r in sorted(rows, key=lambda r: r["label"]):
    print("  %-44s %-34s (%6d %6d %5d) ext (%d %d %d) %s" % (r["label"][:44], r["cls"][:34], r["x"], r["y"], r["z"],
                                                             r["ex"], r["ey"], r["ez"], r["folder"]))
inst = []
for a in actors:
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        n = c.get_instance_count()
        hits = 0
        for k in range(n):
            t = c.get_instance_transform(k, True)
            if inside(t.translation.x, t.translation.y):
                hits += 1
        if hits:
            inst.append((a.get_actor_label(), c.get_name(), str(c.static_mesh.get_name() if c.static_mesh else None), hits))
print("instances in region:", inst)
res = {}
for a in actors:
    if a.get_actor_label().startswith("RES_Start_Radixite") or a.get_actor_label().startswith("RES_Start_CraterDecal"):
        comps = []
        for c in a.get_components_by_class(unreal.SceneComponent):
            d = dict(name=c.get_name(), cls=c.get_class().get_name(), vis=c.is_visible())
            if isinstance(c, unreal.PrimitiveComponent):
                d["col"] = str(c.get_collision_enabled())
                d["nav"] = c.get_editor_property("can_ever_affect_navigation")
            if isinstance(c, unreal.StaticMeshComponent):
                d["mesh"] = c.static_mesh.get_name() if c.static_mesh else None
            comps.append(d)
        p = a.get_actor_location()
        res[a.get_actor_label()] = dict(cls=a.get_class().get_name(), loc=[p.x, p.y, p.z],
                                        rot=a.get_actor_rotation().yaw, scale=a.get_actor_scale3d().x, comps=comps)
json.dump(dict(actors=rows, instances=inst, resources=res), open(OUT + "probe.json", "w"), indent=1)
for k, v in sorted(res.items()):
    print(k, v["cls"], [round(x) for x in v["loc"]], "yaw", round(v["rot"]))
    for c in v["comps"]:
        print("     ", c)
navs = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh)
print("navmeshes", [(n.get_name(), n.get_editor_property("agent_radius") if hasattr(n, "get_editor_property") else None) for n in navs])
