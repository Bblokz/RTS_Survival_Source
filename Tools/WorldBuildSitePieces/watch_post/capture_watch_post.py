"""Pictures of the watch post piece on built hex plates, in the world campaign level's lighting.

Run with run_capture.ps1 (throwaway DebugGame editor with a renderer; waits for the GPU). The level is not saved.
Row, left to right on screen: player HQ piece, four watch posts with EmissiveStrength 0 / 0.12 / 0.3 / 0.6 (dynamic
instances, nothing saved), Oder-Warthe story piece. The posts stand as bare StaticMeshActors on a Plane with
M_BuildSiteHexPlate (State 2 = built), plane scale 57.6 / 52, the way the placement code will show them.
Pictures: Saved/WorldBuildSitePieces/watch_post_*.png
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
PIECE = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/WatchPost/SM_BuildSitePiece_WatchPost"
PLATE_MATERIAL = "/Game/RTS_Survival/WorldCampaign/BuildSites/M_BuildSiteHexPlate"
PLATE_SCALE = 57.6 / 52.0
REFERENCES = [("/Game/RTS_Survival/WorldCampaign/MapObjects/Player/HQ/BP_PlayerHQ", -340.0),
              ("/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Story/OderWarthe/BP_Mission_OderWarthe", 340.0)]
# None = the saved instances as they are. The first run used [0.0, 0.12, 0.3, 0.6] to pick the emissive strength.
EMISSIVE_STEPS = [None, None, None, None]
POST_SPACING = 90.0
ORIGIN = unreal.Vector(1200.0, -6700.0, 0.0)
MAP_PITCH = -70.0


class Capture:
    def __init__(self):
        self.phase = "wait"
        self.mark = 0.0
        self.shots = []
        self.pending = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def log(self, message):
        import unreal as engine
        engine.log("WATCHPOST " + str(message))

    def spawn(self):
        import unreal as engine
        actors = engine.get_editor_subsystem(engine.EditorActorSubsystem)
        world = engine.get_editor_subsystem(engine.UnrealEditorSubsystem).get_editor_world()
        self.log("world %s" % world.get_name())
        ground = 0.0
        hit = engine.SystemLibrary.line_trace_single(
            world, engine.Vector(ORIGIN.x, ORIGIN.y, 5000.0), engine.Vector(ORIGIN.x, ORIGIN.y, -5000.0),
            engine.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], engine.DrawDebugTrace.NONE, True)
        if hit is not None:
            ground = hit.to_tuple()[4].z
        self.log("ground z %.1f" % ground)
        plate_material = engine.load_asset(PLATE_MATERIAL)
        plane = engine.load_asset("/Engine/BasicShapes/Plane")
        piece = engine.load_asset(PIECE)
        posts = []
        for index, emissive in enumerate(EMISSIVE_STEPS):
            y = ORIGIN.y + (index - (len(EMISSIVE_STEPS) - 1) * 0.5) * POST_SPACING
            # AWorldBuildNode puts the plate and the piece 6 units over the ground (HexPlateHeightAboveGround).
            base = engine.Vector(ORIGIN.x, y, ground + 6.0)
            plate = actors.spawn_actor_from_class(engine.StaticMeshActor, base)
            plate.static_mesh_component.set_static_mesh(plane)
            plate.set_actor_scale3d(engine.Vector(PLATE_SCALE, PLATE_SCALE, 1.0))
            plate_instance = plate.static_mesh_component.create_dynamic_material_instance(0, plate_material)
            plate_instance.set_scalar_parameter_value("State", 2.0)
            plate_instance.set_scalar_parameter_value("SignIndex", 2.0)
            post = actors.spawn_actor_from_class(engine.StaticMeshActor, base)
            post.static_mesh_component.set_static_mesh(piece)
            post.static_mesh_component.set_collision_profile_name("NoCollision")
            for slot in range(post.static_mesh_component.get_num_materials() if emissive is not None else 0):
                instance = post.static_mesh_component.create_dynamic_material_instance(slot)
                instance.set_scalar_parameter_value("EmissiveStrength", emissive)
            posts.append(base)
            self.log("post %d emissive %s at y %.0f" % (index, emissive, y))
        for path, offset in REFERENCES:
            piece_class = engine.load_object(None, path + "." + path.split("/")[-1] + "_C")
            location = engine.Vector(ORIGIN.x, ORIGIN.y + offset, ground + 1.0)
            actor = actors.spawn_actor_from_class(piece_class, location)
            self.log("reference %s: %s" % (path.split("/")[-1], actor is not None))
        middle = engine.Vector(ORIGIN.x, ORIGIN.y, ground + 20.0)
        self.shots = [
            ("watch_post_row_70", self.camera(middle, MAP_PITCH, 0.0, 720.0), 30.0),
            ("watch_post_close_70", self.camera(engine.Vector(posts[1].x, posts[1].y, ground + 20.0), MAP_PITCH, 0.0,
                                                300.0), 30.0),
            ("watch_post_context_70", self.camera(middle, MAP_PITCH, 0.0, 1550.0), 30.0),
            ("watch_post_mapsize_70", self.camera(middle, MAP_PITCH, 0.0, 2800.0), 60.0),
            ("watch_post_side", self.camera(engine.Vector(posts[1].x, posts[1].y, ground + 22.0), -22.0, 25.0,
                                            260.0), 30.0),
        ]

    def camera(self, target, pitch, yaw, distance):
        import math
        import unreal as engine
        pitch_radians, yaw_radians = math.radians(pitch), math.radians(yaw)
        forward = engine.Vector(math.cos(pitch_radians) * math.cos(yaw_radians),
                                math.cos(pitch_radians) * math.sin(yaw_radians), math.sin(pitch_radians))
        location = engine.Vector(target.x - forward.x * distance, target.y - forward.y * distance,
                                 target.z - forward.z * distance)
        return location, engine.Rotator(pitch=pitch, yaw=yaw, roll=0.0)

    def tick(self, delta):
        import os as system
        import time as clock
        import unreal as engine
        if not hasattr(self, "start"):
            self.start = clock.time()
        now = clock.time() - self.start
        if self.pending:
            if not system.path.exists(self.pending) and now - self.mark < 40.0:
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
        elif self.phase == "settle" and now - self.mark > 45.0:
            # A first throwaway shot lets the new material's shaders finish before the real ones.
            self.phase = "shoot"
            self.mark = now - 10.0
        elif self.phase == "shoot" and now - self.mark > 5.0:
            if not self.shots:
                self.phase = "quit"
                self.mark = now
                return
            name, (location, rotation), field_of_view = self.shots.pop(0)
            camera = engine.get_editor_subsystem(engine.EditorActorSubsystem).spawn_actor_from_class(
                engine.CameraActor, location, rotation)
            camera.camera_component.set_field_of_view(field_of_view)
            self.pending = system.path.join(OUT, name + ".png")
            engine.AutomationLibrary.take_high_res_screenshot(1920, 1080, self.pending, camera)
            self.log("shot %s" % name)
            self.mark = now
        elif self.phase == "quit" and now - self.mark > 5.0:
            self.phase = "done"
            engine.unregister_slate_post_tick_callback(self.handle)
            engine.SystemLibrary.quit_editor()


CAPTURE = Capture()
