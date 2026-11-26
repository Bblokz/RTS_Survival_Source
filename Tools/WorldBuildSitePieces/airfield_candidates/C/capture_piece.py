"""Pictures of airfield candidate C on a built hex plate in the world campaign level's lighting.

Run with run_capture.ps1 (throwaway DebugGame editor with a renderer, own save slot; waits for the GPU). Level not saved.
Main row (map camera, 70 degrees down): player HQ piece at y -340, candidate C at 0, the approved watch post at +170.
Variant row far outside that frame (y +2200): candidate C with every slot's EmissiveStrength x 0.6 / 1.0 / 1.6
(dynamic instances, nothing saved). Plates: Plane + M_BuildSiteHexPlate, State 2, SignIndex 6, scale 57.6 / 52,
6 units over the ground, the way AWorldBuildNode shows them.
Pictures: Saved/WorldBuildSitePieces/airfield_candidate_C_*.png
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
PIECE = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/C/SM_BuildSitePiece_Airfield_C"
WATCH_POST = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/WatchPost/SM_BuildSitePiece_WatchPost"
PLATE_MATERIAL = "/Game/RTS_Survival/WorldCampaign/BuildSites/M_BuildSiteHexPlate"
PLATE_SCALE = 57.6 / 52.0
HQ = ("/Game/RTS_Survival/WorldCampaign/MapObjects/Player/HQ/BP_PlayerHQ", -340.0)
ORIGIN = unreal.Vector(1200.0, -6700.0, 0.0)
WATCH_POST_OFFSET = 170.0
VARIANT_OFFSET = 2200.0
VARIANT_SPACING = 90.0
EMISSIVE_FACTORS = [0.6, 1.0, 1.6]
MAP_PITCH = -70.0
SLOTS = ["Walls", "Roof", "Stuka", "Accent", "Dark"]
# EmissiveStrength saved on MI_AirfieldC_* (build_asset.py INSTANCES), slot order as SLOTS.
SAVED_EMISSIVE = [0.45, 0.5, 0.55, 0.9, 0.15]


class Capture:
    def __init__(self):
        self.phase = "wait"
        self.mark = 0.0
        self.shots = []
        self.pending = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def log(self, message):
        import unreal as engine
        engine.log("AIRFIELDCCAP " + str(message))

    def ground(self, world, x, y):
        import unreal as engine
        hit = engine.SystemLibrary.line_trace_single(
            world, engine.Vector(x, y, 5000.0), engine.Vector(x, y, -5000.0),
            engine.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], engine.DrawDebugTrace.NONE, True)
        return hit.to_tuple()[4].z if hit is not None else 0.0

    def plate_with(self, actors, mesh, x, y, ground):
        import unreal as engine
        base = engine.Vector(x, y, ground + 6.0)
        plate = actors.spawn_actor_from_class(engine.StaticMeshActor, base)
        plate.static_mesh_component.set_static_mesh(engine.load_asset("/Engine/BasicShapes/Plane"))
        plate.set_actor_scale3d(engine.Vector(PLATE_SCALE, PLATE_SCALE, 1.0))
        plate_instance = plate.static_mesh_component.create_dynamic_material_instance(
            0, engine.load_asset(PLATE_MATERIAL))
        plate_instance.set_scalar_parameter_value("State", 2.0)
        plate_instance.set_scalar_parameter_value("SignIndex", 6.0)
        piece = actors.spawn_actor_from_class(engine.StaticMeshActor, base)
        piece.static_mesh_component.set_static_mesh(mesh)
        piece.static_mesh_component.set_collision_profile_name("NoCollision")
        return piece

    def spawn(self):
        import unreal as engine
        actors = engine.get_editor_subsystem(engine.EditorActorSubsystem)
        world = engine.get_editor_subsystem(engine.UnrealEditorSubsystem).get_editor_world()
        self.log("world %s" % world.get_name())
        piece_mesh = engine.load_asset(PIECE)
        ground = self.ground(world, ORIGIN.x, ORIGIN.y)
        self.log("ground z %.1f" % ground)
        self.plate_with(actors, piece_mesh, ORIGIN.x, ORIGIN.y, ground)
        post_ground = self.ground(world, ORIGIN.x, ORIGIN.y + WATCH_POST_OFFSET)
        self.plate_with(actors, engine.load_asset(WATCH_POST), ORIGIN.x, ORIGIN.y + WATCH_POST_OFFSET, post_ground)
        hq_class = engine.load_object(None, HQ[0] + "." + HQ[0].split("/")[-1] + "_C")
        hq = actors.spawn_actor_from_class(hq_class, engine.Vector(ORIGIN.x, ORIGIN.y + HQ[1], ground + 1.0))
        self.log("HQ spawned %s" % (hq is not None))
        variant_y = ORIGIN.y + VARIANT_OFFSET
        variant_ground = self.ground(world, ORIGIN.x, variant_y)
        for index, factor in enumerate(EMISSIVE_FACTORS):
            y = variant_y + (index - (len(EMISSIVE_FACTORS) - 1) * 0.5) * VARIANT_SPACING
            piece = self.plate_with(actors, piece_mesh, ORIGIN.x, y, variant_ground)
            component = piece.static_mesh_component
            values = []
            for slot in range(component.get_num_materials()):
                try:
                    instance = component.create_dynamic_material_instance(slot)
                    value = SAVED_EMISSIVE[slot]
                    instance.set_scalar_parameter_value("EmissiveStrength", value * factor)
                    values.append(round(value * factor, 2))
                except Exception as error:
                    self.log("variant slot %d failed: %s" % (slot, error))
            self.log("variant x%.1f emissive %s" % (factor, values))
        target = engine.Vector(ORIGIN.x, ORIGIN.y, ground + 12.0)
        middle = engine.Vector(ORIGIN.x, ORIGIN.y - 60.0, ground + 12.0)
        variants = engine.Vector(ORIGIN.x, variant_y, variant_ground + 12.0)
        self.shots = [
            ("airfield_candidate_C_warmup", self.camera(target, MAP_PITCH, 0.0, 260.0), 30.0),
            ("airfield_candidate_C_close", self.camera(target, MAP_PITCH, 0.0, 260.0), 30.0),
            ("airfield_candidate_C_mapsize", self.camera(middle, MAP_PITCH, 0.0, 1550.0), 30.0),
            ("airfield_candidate_C_side", self.camera(target, -24.0, 35.0, 230.0), 30.0),
            ("airfield_candidate_C_variants", self.camera(variants, MAP_PITCH, 0.0, 560.0), 30.0),
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
