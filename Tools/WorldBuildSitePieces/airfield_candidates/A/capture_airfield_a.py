"""Pictures of airfield candidate A on a built hex plate, in the world campaign level's lighting.

Run with run_capture.ps1 (throwaway DebugGame editor with a renderer, own save slot; waits for the GPU). The level is
not saved. The piece stands as a bare StaticMeshActor on a Plane with M_BuildSiteHexPlate (State 2 = built,
SignIndex 6), plane scale 57.6 / 52, 6 units over the ground, the way AWorldBuildNode shows build site pieces.

Shots (Saved/WorldBuildSitePieces/):
  airfield_candidate_A_close.png     one plate fills most of the frame, map camera (70 degrees down, looking +X)
  airfield_candidate_A_mapsize.png   map distance, BP_PlayerHQ next to it for scale (like watch_post_context_70)
  airfield_candidate_A_variants.png  a row far away: saved paint, emissive 0.5, emissive 1.2, brighter aircraft
                                     (dynamic instances, nothing saved)
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
PIECE = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/A/SM_BuildSitePiece_Airfield_A"
PLATE_MATERIAL = "/Game/RTS_Survival/WorldCampaign/BuildSites/M_BuildSiteHexPlate"
HQ = "/Game/RTS_Survival/WorldCampaign/MapObjects/Player/HQ/BP_PlayerHQ"
PLATE_SCALE = 57.6 / 52.0
PLATE_HEIGHT = 6.0
SIGN_INDEX = 6.0
ORIGIN = unreal.Vector(1200.0, -6700.0, 0.0)
HQ_OFFSET_Y = -340.0
VARIANTS_OFFSET_Y = 1600.0
VARIANT_SPACING = 90.0
MAP_PITCH = -70.0
# Each variant: {slot name or "*": {scalar: value}} on dynamic instances; {} = the saved instances.
# Render 1 compared emissive 0.8 / 0.5 / 1.2 (little difference); render 2 compares the aircraft brightness.
VARIANTS = [{}, {"Stuka": {"EmissiveStrength": 1.6}}, {"Stuka": {"EmissiveStrength": 0.6}},
            {"Floor": {"EmissiveStrength": 0.0}, "Stuka": {"EmissiveStrength": 1.3}}]


def log(message):
    unreal.log("AIRFIELD_A " + str(message))


class Capture:
    def __init__(self):
        self.phase = "wait"
        self.mark = 0.0
        self.shots = []
        self.pending = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def ground_z(self, world, x, y):
        import unreal as engine
        hit = engine.SystemLibrary.line_trace_single(
            world, engine.Vector(x, y, 5000.0), engine.Vector(x, y, -5000.0),
            engine.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], engine.DrawDebugTrace.NONE, True)
        return hit.to_tuple()[4].z if hit is not None else 0.0

    def plate_with_piece(self, actors, x, y, ground, overrides):
        import unreal as engine
        base = engine.Vector(x, y, ground + PLATE_HEIGHT)
        plate = actors.spawn_actor_from_class(engine.StaticMeshActor, base)
        plate.static_mesh_component.set_static_mesh(engine.load_asset("/Engine/BasicShapes/Plane"))
        plate.set_actor_scale3d(engine.Vector(PLATE_SCALE, PLATE_SCALE, 1.0))
        plate_instance = plate.static_mesh_component.create_dynamic_material_instance(
            0, engine.load_asset(PLATE_MATERIAL))
        plate_instance.set_scalar_parameter_value("State", 2.0)
        plate_instance.set_scalar_parameter_value("SignIndex", SIGN_INDEX)
        piece = actors.spawn_actor_from_class(engine.StaticMeshActor, base)
        component = piece.static_mesh_component
        component.set_static_mesh(engine.load_asset(PIECE))
        component.set_collision_profile_name("NoCollision")
        names = [str(name) for name in component.get_material_slot_names()]
        for slot_index, slot_name in enumerate(names):
            values = dict(overrides.get("*", {}), **overrides.get(slot_name, {}))
            if not values:
                continue
            instance = component.create_dynamic_material_instance(slot_index)
            for parameter, value in values.items():
                instance.set_scalar_parameter_value(parameter, value)
        return base

    def spawn(self):
        import unreal as engine
        actors = engine.get_editor_subsystem(engine.EditorActorSubsystem)
        world = engine.get_editor_subsystem(engine.UnrealEditorSubsystem).get_editor_world()
        log("world %s" % world.get_name())
        ground = self.ground_z(world, ORIGIN.x, ORIGIN.y)
        log("ground z %.1f" % ground)
        main = self.plate_with_piece(actors, ORIGIN.x, ORIGIN.y, ground, {})
        hq_class = engine.load_object(None, HQ + "." + HQ.split("/")[-1] + "_C")
        hq = actors.spawn_actor_from_class(hq_class, engine.Vector(ORIGIN.x, ORIGIN.y + HQ_OFFSET_Y, ground + 1.0))
        log("hq spawned: %s" % (hq is not None))
        row_y = ORIGIN.y + VARIANTS_OFFSET_Y
        row_ground = self.ground_z(world, ORIGIN.x, row_y)
        for index, overrides in enumerate(VARIANTS):
            y = row_y + (index - (len(VARIANTS) - 1) * 0.5) * VARIANT_SPACING
            self.plate_with_piece(actors, ORIGIN.x, y, row_ground, overrides)
            log("variant %d %s at y %.0f" % (index, overrides, y))
        focus = engine.Vector(main.x, main.y, ground + 12.0)
        context = engine.Vector(ORIGIN.x, ORIGIN.y + HQ_OFFSET_Y * 0.5, ground + 20.0)
        self.shots = [
            ("airfield_candidate_A_warmup", self.camera(focus, MAP_PITCH, 260.0), 30.0),
            ("airfield_candidate_A_close", self.camera(focus, MAP_PITCH, 250.0), 30.0),
            ("airfield_candidate_A_mapsize", self.camera(context, MAP_PITCH, 1550.0), 30.0),
            ("airfield_candidate_A_variants", self.camera(engine.Vector(ORIGIN.x, row_y, row_ground + 15.0), MAP_PITCH,
                                                          720.0), 30.0),
        ]

    def camera(self, target, pitch, distance):
        import math
        import unreal as engine
        pitch_radians = math.radians(pitch)
        forward = engine.Vector(math.cos(pitch_radians), 0.0, math.sin(pitch_radians))
        location = engine.Vector(target.x - forward.x * distance, target.y, target.z - forward.z * distance)
        return location, engine.Rotator(pitch=pitch, yaw=0.0, roll=0.0)

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
                log("spawn failed: %s" % error)
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
            log("shot %s" % name)
            self.mark = now
        elif self.phase == "quit" and now - self.mark > 5.0:
            self.phase = "done"
            engine.unregister_slate_post_tick_callback(self.handle)
            engine.SystemLibrary.quit_editor()


CAPTURE = Capture()
