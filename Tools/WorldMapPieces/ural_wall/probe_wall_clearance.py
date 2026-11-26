"""In a -game run of the world campaign: which anchors and map pieces stand in or near the wall.

Run with run_probe.ps1. Writes Saved/WorldMapWall/clearance.txt: the wall centre line (spline samples and the gate
actor), then every anchor / map object within NEAR units of it with its XY bounds radius and the gap that is left.
"""
import math
import os
import time

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldMapWall")
WALL_MESH_PREFIX = "SM_WorldWall_UralShield"
SAMPLE_STEP = 20.0
NEAR = 500.0
LINES = []


def log(text):
    LINES.append(str(text))
    unreal.log("URALWALL " + str(text))


def wall_points(actors):
    points = []
    for actor in actors:
        for spline_mesh in actor.get_components_by_class(unreal.SplineMeshComponent):
            mesh = spline_mesh.get_editor_property("static_mesh")
            if mesh is None or not mesh.get_name().startswith(WALL_MESH_PREFIX):
                continue
            for spline in actor.get_components_by_class(unreal.SplineComponent):
                length = spline.get_spline_length()
                for index in range(int(length / SAMPLE_STEP) + 1):
                    p = spline.get_location_at_distance_along_spline(index * SAMPLE_STEP,
                                                                     unreal.SplineCoordinateSpace.WORLD)
                    points.append((p.x, p.y))
            break
        if isinstance(actor, unreal.StaticMeshActor):
            mesh = actor.static_mesh_component.get_editor_property("static_mesh")
            if mesh is not None and mesh.get_name().startswith(WALL_MESH_PREFIX):
                origin, extent = actor.get_actor_bounds(False)
                yaw = math.radians(actor.get_actor_rotation().yaw)
                half = mesh.get_bounding_box().max.x * actor.get_actor_scale3d().x
                for index in range(-10, 11):
                    offset = half * index / 10.0
                    points.append((origin.x + math.cos(yaw) * offset, origin.y + math.sin(yaw) * offset))
                log("GATE origin (%.0f %.0f) extent (%.0f %.0f %.0f)" % (origin.x, origin.y, extent.x, extent.y,
                                                                          extent.z))
    return points


def game_world():
    # A -game run has no editor subsystems; the loaded map's world is found by its object path.
    return unreal.find_object(None, "/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign.RTS_WorldCampaign")


def probe():
    world = game_world()
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    points = wall_points(actors)
    log("world %s actors %d wall samples %d" % (world.get_name(), len(actors), len(points)))
    rows = []
    for actor in actors:
        class_name = actor.get_class().get_name()
        is_anchor = isinstance(actor, unreal.AnchorPoint)
        is_object = isinstance(actor, unreal.WorldMapObject) or "BuildNode" in class_name or "Division" in class_name
        if not is_anchor and not is_object:
            continue
        location = actor.get_actor_location()
        distance = min(math.hypot(location.x - x, location.y - y) for x, y in points)
        if distance > NEAR:
            continue
        origin, extent = actor.get_actor_bounds(False)
        radius = max(extent.x, extent.y)
        rows.append((distance, "%s %s loc (%.0f %.0f) scale %.2f bounds radius %.0f (extent %.0f %.0f %.0f) "
                     "distance to wall line %.0f" % ("ANCHOR" if is_anchor else "OBJECT", class_name, location.x,
                                                     location.y, actor.get_actor_scale3d().x, radius, extent.x,
                                                     extent.y, extent.z, distance)))
    for _distance, text in sorted(rows):
        log(text)


class Run:
    def __init__(self):
        self.start = time.time()
        self.done = False
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, delta):
        if self.done or time.time() - self.start < 60.0:
            return
        self.done = True
        try:
            probe()
        except Exception:
            import traceback
            log(traceback.format_exc())
        os.makedirs(OUT, exist_ok=True)
        with open(os.path.join(OUT, "clearance.txt"), "w", encoding="utf-8") as handle:
            handle.write("\n".join(LINES))
        unreal.unregister_slate_post_tick_callback(self.handle)
        unreal.SystemLibrary.execute_console_command(game_world(), "quit")


RUN = Run()
