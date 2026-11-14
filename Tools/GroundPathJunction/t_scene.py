"""Test scene for the road junction overlay, sent to the throwaway editor with junction_cmd.py (never saved).
Set JT (dict) in the persistent namespace before running to override the defaults below."""
import math

ROOT = "/Game/RTS_Survival/Blueprints/Environment/Splines/GroundPath/"
DEFAULTS = dict(centre=(2000.0, 2000.0), main_heading=-86.0, side_heading=0.0, bp="BP_GroundPath_RoadJunction",
                z_above_tiles=6.0, side_from=-3200.0, side_to=3200.0, bend=True, junction=True, skew=None,
                side_offset=0.0)
P = dict(DEFAULTS)
P.update(globals().get("JT", {}))
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = ues.get_editor_world()
assert world.get_path_name().startswith("/Game/RTS_Survival/Maps/ToCopy/RTS_ToCopySmall"), world.get_path_name()


def ground_z(x, y):
    hits = unreal.SystemLibrary.line_trace_multi(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    for hit in hits:
        actor = hit.to_tuple()[9]
        if actor and actor.get_actor_label().startswith("Landscape"):
            return hit.to_tuple()[4].z
    raise RuntimeError("no ground")


def road(label, points):
    cls = unreal.load_class(None, ROOT + "Road/BP_GroundPathSpline_Road.BP_GroundPathSpline_Road_C")
    first = points[0]
    actor = eas.spawn_actor_from_class(cls, unreal.Vector(first[0], first[1], ground_z(*first) + 150.0))
    actor.set_actor_label(label)
    spline = actor.get_component_by_class(unreal.SplineComponent)
    spline.set_spline_points([unreal.Vector(x, y, ground_z(x, y) + 150.0) for x, y in points],
                             unreal.SplineCoordinateSpace.WORLD, True)
    spline.set_editor_property("spline_has_been_edited", True)
    actor.set_editor_property("PathMaterial", unreal.load_asset(ROOT + "Road/MI_GroundPath_Road_WarTorn"))
    actor.set_editor_property("PathWidth", 901.0)
    actor.set_editor_property("PathWidth", 900.0)
    return actor


for actor in eas.get_all_level_actors():
    if actor.get_actor_label().startswith("JT_"):
        eas.destroy_actor(actor)

cx, cy = P["centre"]
mh = math.radians(P["main_heading"])
sh = math.radians(P["side_heading"])
md, sd = (math.cos(mh), math.sin(mh)), (math.cos(sh), math.sin(sh))
# main road: straight through the crossing, with the real map's gentle bends 25 m either side
main_pts = []
for k, turn in ((-3, 6.0), (-2, 3.5), (-1, 0.0), (1, 0.0), (2, -4.0), (3, -5.5)):
    s = k * 1250.0
    side = (abs(k) - 1) * 1250.0 * math.tan(math.radians(turn)) if P["bend"] else 0.0
    main_pts.append((cx + md[0] * s - md[1] * side, cy + md[1] * s + md[0] * side))
main = road("JT_Road_Main", main_pts)
ox, oy = -sd[1] * P["side_offset"], sd[0] * P["side_offset"]
side_pts = [(cx + ox + sd[0] * P["side_from"], cy + oy + sd[1] * P["side_from"]),
            (cx + ox + sd[0] * P["side_to"], cy + oy + sd[1] * P["side_to"])]
side = road("JT_Road_Side", side_pts)

tiles = main.get_components_by_class(unreal.SplineMeshComponent)
def bounds(component):
    origin, extent, _radius = unreal.SystemLibrary.get_component_bounds(component)
    return origin, extent


tile = min(tiles, key=lambda t: (bounds(t)[0].x - cx) ** 2 + (bounds(t)[0].y - cy) ** 2)
tile_z = bounds(tile)[0].z
print("ground", ground_z(cx, cy), "tile z", tile_z, "tiles main", len(tiles), "side", len(side.get_components_by_class(unreal.SplineMeshComponent)))
print("tile flags:", {k: str(tile.get_editor_property(k)) for k in (
    "cast_shadow", "receives_decals", "can_ever_affect_navigation", "mobility", "translucency_sort_priority",
    "render_custom_depth", "bounds_scale", "visible_in_ray_tracing", "affect_distance_field_lighting",
    "affect_dynamic_indirect_lighting")}, "collision", tile.get_collision_enabled(), tile.get_collision_profile_name())
print("tile rvt", [r.get_name() for r in tile.get_editor_property("runtime_virtual_textures")])
ends = sorted((bounds(t)[0].x, bounds(t)[1].x)
              for t in side.get_components_by_class(unreal.SplineMeshComponent))
print("side road tiles span x %.0f .. %.0f (spline %.0f .. %.0f)" % (ends[0][0] - ends[0][1], ends[-1][0] + ends[-1][1],
                                                                     side_pts[0][0], side_pts[1][0]))

if P["junction"]:
    cls = unreal.load_class(None, ROOT + "Road/Junction/%s.%s_C" % (P["bp"], P["bp"]))
    junction = eas.spawn_actor_from_class(cls, unreal.Vector(cx, cy, tile_z + P["z_above_tiles"]),
                                          unreal.Rotator(0.0, 0.0, P["main_heading"]))
    junction.set_actor_label("JT_Junction")
    delta = P["side_heading"] - P["main_heading"]
    skew = ((delta - 90.0 + 90.0) % 180.0) - 90.0 if P["skew"] is None else P["skew"]
    comp = junction.get_component_by_class(unreal.StaticMeshComponent)
    comp.set_default_custom_primitive_data_float(0, skew)
    print("junction at", junction.get_actor_location(), "yaw", P["main_heading"], "skew", skew,
          "cpd", comp.get_editor_property("custom_primitive_data").get_editor_property("data"),
          "material", comp.get_material(0).get_name(), "mesh", comp.static_mesh.get_name())
    print("junction flags:", {k: str(comp.get_editor_property(k)) for k in (
        "cast_shadow", "receives_decals", "can_ever_affect_navigation", "mobility", "bounds_scale")},
          "collision", comp.get_collision_enabled(), comp.get_collision_profile_name())
