"""Logs where a map places static meshes whose name contains a substring (component location, bounds radius, cast
shadow), plus the landscape bounds. Read-only. Usage: mesh_positions.py <map package> <name substring>"""
import sys
import unreal
map_path, needle = sys.argv[1], sys.argv[2]
unreal.EditorLoadingAndSavingUtils.load_map(map_path)
world = unreal.EditorLevelLibrary.get_editor_world()
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = comp.get_editor_property("static_mesh")
        if not mesh or needle not in mesh.get_name():
            continue
        loc = comp.get_world_location()
        unreal.log_warning("RTS_MESHPOS %s actor=%s loc=(%.0f,%.0f,%.0f) radius=%.0f shadow=%s" % (
            mesh.get_name(), actor.get_name(), loc.x, loc.y, loc.z, comp.get_local_bounds()[1].length() * comp.get_world_scale().x,
            comp.get_editor_property("cast_shadow")))
for land in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LandscapeProxy):
    origin, extent = land.get_actor_bounds(False)
    unreal.log_warning("RTS_MESHPOS landscape %s origin=(%.0f,%.0f) extent=(%.0f,%.0f)" % (
        land.get_name(), origin.x, origin.y, extent.x, extent.y))
unreal.log_warning("RTS_MESHPOS DONE")
