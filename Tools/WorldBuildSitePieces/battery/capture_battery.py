"""Pictures of the battery piece on build site hex plates, in the world campaign level's own lighting.

Run in a throwaway DebugGame editor with a renderer (run_capture.ps1 waits for the graphics card), never the shared one.
Four plates in a row; the piece on each has another Emissive value (dynamic instances, nothing saved):
EMISSIVE_VARIANTS. Pictures: Saved/WorldBuildSitePieces/battery_*.png. The level is not saved.
"""
import math
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
                   "WorldBuildSitePieces")
PIECE = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Battery/SM_BuildSitePiece_Battery"
PLATE_MATERIAL = "/Game/RTS_Survival/WorldCampaign/BuildSites/M_BuildSiteHexPlate"
PLATE_SCALE = 57.6 / 52.0
ORIGIN = unreal.Vector(1200.0, -6700.0, 0.0)   # open ground used by the story piece line-up
SPACING = 90.0
EMISSIVE_VARIANTS = [0.2, 0.4, 0.6, 0.8]
PLATE_HEIGHT = 6.0   # AWorldBuildNode: plate and piece 6 units above the node's ground
MAP_PITCH = -70.0


class Capture:
    def __init__(self):
        self.phase = "wait"
        self.mark = 0.0
        self.shots = []
        self.pending = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def log(self, message):
        unreal.log("BATTERYCAP " + str(message))

    def camera(self, target, pitch, yaw, distance):
        pitch_radians, yaw_radians = math.radians(pitch), math.radians(yaw)
        forward = unreal.Vector(math.cos(pitch_radians) * math.cos(yaw_radians),
                                math.cos(pitch_radians) * math.sin(yaw_radians), math.sin(pitch_radians))
        location = unreal.Vector(target.x - forward.x * distance, target.y - forward.y * distance,
                                 target.z - forward.z * distance)
        return location, unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)

    def spawn(self):
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        self.log("world %s" % world.get_name())
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(ORIGIN.x, ORIGIN.y, 5000.0), unreal.Vector(ORIGIN.x, ORIGIN.y, -5000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        ground = hit.to_tuple()[4].z if hit is not None else 0.0
        self.log("ground z %.1f" % ground)
        piece = unreal.load_asset(PIECE)
        plate_material = unreal.load_asset(PLATE_MATERIAL)
        plane = unreal.load_asset("/Engine/BasicShapes/Plane")
        centres = []
        for index, emissive in enumerate(EMISSIVE_VARIANTS):
            location = unreal.Vector(ORIGIN.x, ORIGIN.y + (index - 1.5) * SPACING, ground + PLATE_HEIGHT)
            plate = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
            component = plate.static_mesh_component
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_static_mesh(plane)
            plate.set_actor_scale3d(unreal.Vector(PLATE_SCALE, PLATE_SCALE, 1.0))
            plate_instance = component.create_dynamic_material_instance(0, plate_material)
            plate_instance.set_scalar_parameter_value("State", 2.0)
            plate_instance.set_scalar_parameter_value("SignIndex", 6.0)  # no sign: a model stands there

            holder = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
            mesh_component = holder.static_mesh_component
            mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            mesh_component.set_static_mesh(piece)
            mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            for slot in range(mesh_component.get_num_materials()):
                instance = mesh_component.create_dynamic_material_instance(slot, mesh_component.get_material(slot))
                instance.set_scalar_parameter_value("Emissive", emissive)
            centres.append(location)
            self.log("piece %d emissive %.2f at (%.0f %.0f %.0f)" % (index, emissive, location.x, location.y,
                                                                     location.z))
        middle = unreal.Vector(ORIGIN.x, ORIGIN.y, ground)
        self.shots = [
            ("battery_row_close", self.camera(middle, MAP_PITCH, 0.0, 900.0), 30.0),
            ("battery_row_side", self.camera(middle, -30.0, 0.0, 900.0), 30.0),
            # About the size the map shows a build site at its normal zoom (plates about 30 px wide).
            ("battery_row_mapdistance", self.camera(middle, MAP_PITCH, 0.0, 3200.0), 60.0),
        ]
        for index, emissive in enumerate(EMISSIVE_VARIANTS):
            self.shots.append(("battery_e%03d_close" % int(emissive * 100),
                               self.camera(centres[index], MAP_PITCH, 0.0, 260.0), 30.0))
        self.shots.append(("battery_e040_low", self.camera(centres[1], -35.0, 0.0, 220.0), 30.0))

    def tick(self, delta):
        import time
        if not hasattr(self, "start"):
            self.start = time.time()
        now = time.time() - self.start
        if self.pending:
            if not os.path.exists(self.pending) and now - self.mark < 40.0:
                return
            self.pending = None
            self.mark = now
        if self.phase == "wait" and now > 25.0:
            try:
                self.spawn()
            except Exception as error:
                self.log("spawn failed: %s" % error)
                self.phase = "quit"
                return
            self.phase = "settle"
            self.mark = now
        elif self.phase == "settle" and now - self.mark > 20.0:
            self.phase = "shoot"
            self.mark = now - 10.0
        elif self.phase == "shoot" and now - self.mark > 4.0:
            if not self.shots:
                self.phase = "quit"
                self.mark = now
                return
            name, (location, rotation), field_of_view = self.shots.pop(0)
            camera = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
                unreal.CameraActor, location, rotation)
            camera.camera_component.set_field_of_view(field_of_view)
            self.pending = os.path.join(OUT, name + ".png")
            unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, self.pending, camera)
            self.log("shot %s" % name)
            self.mark = now
        elif self.phase == "quit" and now - self.mark > 5.0:
            self.phase = "done"
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.SystemLibrary.quit_editor()


CAPTURE = Capture()
