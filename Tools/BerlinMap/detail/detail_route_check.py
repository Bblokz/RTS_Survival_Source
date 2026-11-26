"""Nav check for the detail pass: every leg of the Reichstag patrol ring / gate route and the HQ->lane points path
completely for every RecastNavMesh agent (editor world, navmesh built)."""
import math
import unreal

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
acts = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()}
legs = []
for k in ("RT_PatrolRoute_Ring", "RT_PatrolRoute_Gate"):
    pts = [unreal.Vector(p.x, p.y, 50) for p in acts[k].get_world_patrol_points()]
    if k.endswith("Ring"):
        pts.append(pts[0])
    legs += [(k, a, b) for a, b in zip(pts[:-1], pts[1:])]
hq = unreal.Vector(13900, 43300, 0)
for name, t in (("E1", (1550, 46200)), ("E2", (19772, 16716)), ("lane_n", (18092, 23333)), ("alex", (7000, 28000))):
    legs.append(("HQ->" + name, hq, unreal.Vector(t[0], t[1], 0)))
bad = 0
for nav in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh):
    for k, a, b in legs:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(w, a, b, nav, None)
        pts = list(p.path_points) if p else []
        ok = bool(pts) and not p.is_partial() and math.hypot(pts[-1].x - b.x, pts[-1].y - b.y) < 400
        if not ok:
            bad += 1
            print("FAIL", nav.get_name(), k, (round(a.x), round(a.y)), "->", (round(b.x), round(b.y)))
print("route legs", len(legs), "x agents, failures", bad)
