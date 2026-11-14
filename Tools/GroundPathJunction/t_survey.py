import collections
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = ues.get_editor_world()
print("world", world.get_path_name())
counts = collections.Counter(a.get_class().get_name() for a in eas.get_all_level_actors())
print(dict(counts))
for a in eas.get_all_level_actors():
    n = a.get_class().get_name()
    if "Landscape" in n or "VirtualTexture" in n or "Light" in n or "Sky" in n or "PostProcess" in n or "Fog" in n:
        o, e = a.get_actor_bounds(False)
        print(n, a.get_actor_label(), "origin", [round(v) for v in (o.x, o.y, o.z)], "extent", [round(v) for v in (e.x, e.y, e.z)])
    if "Landscape" == n:
        print("  landscape material", a.get_editor_property("landscape_material").get_path_name())
        print("  rvts", [r.get_path_name() if r else None for r in a.get_editor_property("runtime_virtual_textures")])


def gz(x, y):
    hits = unreal.SystemLibrary.line_trace_multi(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return [(round(h.to_tuple()[4].z, 1), h.to_tuple()[9].get_actor_label() if h.to_tuple()[9] else None) for h in hits]


for x, y in ((0, 0), (2000, 0), (0, 2000), (-2000, -2000), (4000, 4000), (-4000, 3000)):
    print("ground", x, y, gz(x, y))
