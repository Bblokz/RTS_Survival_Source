"""Puts the small Ural Shield wall on the world map wall and takes pictures. Run with run_swap.ps1 (throwaway editor).

The two wall splines (BP_RoadSpline actors whose Road Mesh is SM_Global_SovietWall) get SM_WorldWall_UralShield, the
gate actor gets SM_WorldWall_UralShield_Gate. URALWALL_SAVE=1 saves RTS_WorldCampaign afterwards (only that package);
without it nothing is saved and the run is a preview. URALWALL_SWAP=0 leaves the old wall, for a before picture.
Pictures: Saved/WorldMapWall/wall_<mode>_wide.png, _gate.png, _north.png
"""
import math
import os
import time

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldMapWall")
NEW = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Wall/UralShield/"
OLD_WALL = "SM_Global_SovietWall"
OLD_GATE = "SM_Global_SovietWallWithGate2"
SAVE = os.environ.get("URALWALL_SAVE", "0") == "1"
SWAP = os.environ.get("URALWALL_SWAP", "1") == "1"
MODE = "saved" if SAVE else ("preview" if SWAP else "old")
MAP_PITCH = -70.0
EXPECTED_ACTORS = 3
# The map camera looks north (-Y).
SHOTS = [("wide", (3750.0, -7150.0, 40.0), 3300.0), ("gate", (3552.0, -7145.0, 40.0), 1300.0),
         ("north", (3900.0, -6700.0, 40.0), 1500.0)]


def log(message):
    unreal.log("URALWALL " + str(message))


def swap():
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    wall = unreal.load_asset(NEW + "SM_WorldWall_UralShield")
    gate = unreal.load_asset(NEW + "SM_WorldWall_UralShield_Gate")
    changed = 0
    for actor in actors:
        if actor.get_class().get_name() == "BP_RoadSpline_C":
            mesh = actor.get_editor_property("road_mesh")
            if mesh is None or mesh.get_name() != OLD_WALL:
                continue
            # Setting the property in the editor reruns the construction script, which rebuilds the pieces.
            actor.set_editor_property("road_mesh", wall)
            pieces = actor.get_components_by_class(unreal.SplineMeshComponent)
            log("spline %s: %d pieces, mesh %s" % (actor.get_actor_label(), len(pieces),
                                                  pieces[0].get_editor_property("static_mesh").get_name()))
            changed += 1
        elif isinstance(actor, unreal.StaticMeshActor):
            component = actor.static_mesh_component
            mesh = component.get_editor_property("static_mesh")
            if mesh is None or mesh.get_name() != OLD_GATE:
                continue
            component.set_editor_property("override_materials", [])
            component.set_static_mesh(gate)
            log("gate %s -> %s, material %s" % (actor.get_actor_label(),
                                               component.get_editor_property("static_mesh").get_name(),
                                               component.get_material(0).get_name()))
            changed += 1
    log("changed actors %d (expected %d)" % (changed, EXPECTED_ACTORS))
    return changed


def camera(target, distance):
    pitch = math.radians(MAP_PITCH)
    location = unreal.Vector(target[0], target[1] + math.cos(pitch) * distance, target[2] - math.sin(pitch) * distance)
    return location, unreal.Rotator(pitch=MAP_PITCH, yaw=-90.0, roll=0.0)


class Run:
    def __init__(self):
        self.phase = "wait"
        self.start = time.time()
        self.mark = 0.0
        self.changed = 0
        self.cameras = []
        self.shots = list(SHOTS)
        self.pending = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def shoot(self, now):
        name, target, distance = self.shots.pop(0)
        location, rotation = camera(target, distance)
        shot_camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
            unreal.CameraActor, location, rotation)
        shot_camera.camera_component.set_field_of_view(30.0)
        self.cameras.append(shot_camera)
        self.pending = os.path.join(OUT, "wall_%s_%s.png" % (MODE, name))
        if os.path.exists(self.pending):
            os.remove(self.pending)
        unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, self.pending, shot_camera)
        log("shot %s" % name)
        self.mark = now

    def save(self):
        for shot_camera in self.cameras:
            shot_camera.destroy_actor()
        if not SAVE:
            return
        if self.changed != EXPECTED_ACTORS:
            log("NOT saved: changed %d actors, expected %d" % (self.changed, EXPECTED_ACTORS))
            return
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        log("saved level: %s" % unreal.EditorLoadingAndSavingUtils.save_packages([world.get_outermost()], False))

    def tick(self, delta):
        now = time.time() - self.start
        if self.pending:
            if not os.path.exists(self.pending) and now - self.mark < 40.0:
                return
            self.pending = None
            self.mark = now
        if self.phase == "wait" and now > 25.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log("world %s mode %s" % (world.get_name(), MODE))
            self.changed = swap() if SWAP else 0
            self.phase = "settle"
            self.mark = now
        elif self.phase == "settle" and now - self.mark > 40.0:
            self.phase = "shoot"
            self.mark = now - 10.0
        elif self.phase == "shoot" and now - self.mark > 5.0:
            if self.shots:
                self.shoot(now)
                return
            self.phase = "save"
            self.mark = now
        elif self.phase == "save" and now - self.mark > 3.0:
            self.save()
            self.phase = "quit"
            self.mark = now
        elif self.phase == "quit" and now - self.mark > 4.0:
            self.phase = "done"
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.SystemLibrary.quit_editor()


os.makedirs(OUT, exist_ok=True)
RUN = Run()
