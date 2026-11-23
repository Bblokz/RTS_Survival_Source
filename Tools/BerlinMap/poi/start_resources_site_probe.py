# Probe (read-only): what stands on the option A sites now, ground heights there, existing resources / start actors.
import math
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
CRATER = (17300, 47300)
DEPOT = (10700, 14800, 47200, 49000)      # x0, x1, y0, y1
START = (13542, 44444)


def ground(x, y):
    hits = unreal.SystemLibrary.line_trace_multi(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    for h in hits or []:
        t = h.to_tuple()
        actor = t[9] if len(t) > 9 else None
        if actor and actor.get_class().get_name().startswith("Landscape"):
            return round(t[4].z, 1)
    return None


near = []
for a in sub.get_all_level_actors():
    cls = a.get_class().get_name()
    if cls.startswith("Landscape") or cls in ("WorldSettings", "Brush"):
        continue
    p = a.get_actor_location()
    d = math.hypot(p.x - CRATER[0], p.y - CRATER[1])
    in_depot = DEPOT[0] - 800 < p.x < DEPOT[1] + 800 and DEPOT[2] - 800 < p.y < DEPOT[3] + 800
    if d < 2200 or in_depot or "Resource" in cls or "PlayerStart" in cls or "StartPosition" in cls or "StartLoc" in cls:
        near.append((("crater %4.0f m" % (d / 100)) if d < 2200 else ("depot" if in_depot else "-"), cls,
                     a.get_actor_label(), round(p.x), round(p.y), round(p.z)))
for row in sorted(near):
    print(row)
for name, (x, y) in {"crater": CRATER, "start": START, "depot_w": (12700, 47400), "depot_e": (12700, 48800),
                     "depot_s": (11000, 48100), "depot_n": (14500, 48100)}.items():
    print("ground", name, x, y, ground(x, y))
