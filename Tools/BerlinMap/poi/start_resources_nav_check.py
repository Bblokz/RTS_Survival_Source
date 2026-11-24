# Read-only: nav paths from the assumed HQ spot to points beside each field, for every agent's RecastNavMesh.
import math
import unreal

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
HQ = unreal.Vector(13900, 43300, 0)
GOALS = {"crater_w": (16000, 46300), "crater_s": (16500, 48700), "crater_n": (19000, 47200),
         "depot_w": (12700, 46600), "depot_shed": (10500, 47300), "depot_tanks": (14900, 46900)}
for nav in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh):
    row = []
    for name, (x, y) in GOALS.items():
        goal = unreal.Vector(x, y, 0)
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(w, HQ, goal, nav, None)
        pts = list(p.path_points) if p else []
        ok = bool(pts) and math.hypot(pts[-1].x - x, pts[-1].y - y) < 300 and not p.is_partial()
        length = sum((pts[i + 1] - pts[i]).length() for i in range(len(pts) - 1)) / 100 if pts else 0
        row.append("%s %s %.0fm" % (name, "OK" if ok else "FAIL", length))
    print(nav.get_name(), " | ".join(row))
