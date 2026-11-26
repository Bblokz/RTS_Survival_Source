"""Pictures of the airfield build site piece on a built hex plate, from the map camera angle (70 degrees down).

Run in a throwaway DebugGame editor with a renderer (run_capture.ps1 waits for GPU room), never the shared one.
AIRFIELD_CAPTURE_MODE=neutral: on a unit test map under a plain sun + sky light.
AIRFIELD_CAPTURE_MODE=campaign: in the world campaign level, with copies at several emissive strengths.
Pictures: Saved/WorldBuildSitePieces/airfield_<mode>_*.png. The level is not saved.
"""
import math
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
MODE = os.environ.get("AIRFIELD_CAPTURE_MODE", "neutral")
PIECE = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/SM_BuildSitePiece_Airfield"
PLATE_MATERIAL = "/Game/RTS_Survival/WorldCampaign/BuildSites/M_BuildSiteHexPlate"
ENEMY_AIRFIELD = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Airfield/BP_EnemyAirfield"
ENEMY_MESH = "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Airfield/Mesh/SM_Global_SovietAirbase"
PLATE_SCALE = 57.6 / 52.0
MAP_PITCH = -70.0
ORIGIN = unreal.Vector(0.0, 0.0, 0.0) if MODE == "neutral" else unreal.Vector(1200.0, -6700.0, 0.0)
EMISSIVE_VARIANTS = [None]
# The real build node puts its plate 6 units over the ground: above the translucent country fill.
PLATE_HEIGHT = 6.0
SPACING = 140.0


def log(message):
    unreal.log("AIRFIELDCAP " + str(message))


class Capture:
    def __init__(self):
        self.phase = "wait"
        self.mark = 0.0
        self.shots = []
        self.pending = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def camera(self, target, pitch, yaw, distance):
        pitch_r, yaw_r = math.radians(pitch), math.radians(yaw)
        forward = unreal.Vector(math.cos(pitch_r) * math.cos(yaw_r), math.cos(pitch_r) * math.sin(yaw_r),
                                math.sin(pitch_r))
        location = unreal.Vector(target.x - forward.x * distance, target.y - forward.y * distance,
                                 target.z - forward.z * distance)
        return location, unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0)

    def spawn_site(self, actors, location, emissive=None):
        plate = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
        plate.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        plate.set_actor_scale3d(unreal.Vector(PLATE_SCALE, PLATE_SCALE, 1.0))
        plate_material = plate.static_mesh_component.create_dynamic_material_instance(
            0, unreal.load_asset(PLATE_MATERIAL))
        plate_material.set_scalar_parameter_value("State", 2.0)
        plate_material.set_scalar_parameter_value("SignIndex", 6.0)
        piece = actors.spawn_actor_from_class(unreal.StaticMeshActor,
                                              unreal.Vector(location.x, location.y, location.z + 0.05))
        piece.static_mesh_component.set_static_mesh(unreal.load_asset(PIECE))
        piece.static_mesh_component.set_collision_profile_name("NoCollision")
        if emissive is not None:
            for slot in range(3):
                instance = piece.static_mesh_component.create_dynamic_material_instance(
                    slot, piece.static_mesh_component.get_material(slot))
                instance.set_scalar_parameter_value("EmissiveStrength", emissive)
        return piece

    def spawn_enemy(self, actors, location):
        enemy_class = unreal.load_object(None, ENEMY_AIRFIELD + ".BP_EnemyAirfield_C")
        enemy = actors.spawn_actor_from_class(enemy_class, location) if enemy_class else None
        if enemy is None:
            enemy = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
            enemy.static_mesh_component.set_static_mesh(unreal.load_asset(ENEMY_MESH))
            log("enemy airfield: plain mesh")
        else:
            log("enemy airfield: blueprint, scale %s" % enemy.get_actor_scale3d())
            for component in enemy.get_components_by_class(unreal.StaticMeshComponent):
                log("  enemy component %s mesh %s scale %s visible %s hidden %s" % (
                    component.get_name(), component.static_mesh.get_name() if component.static_mesh else None,
                    component.get_world_scale(), component.is_visible(), component.hidden_in_game))
                component.set_visibility(True, True)

    def spawn(self):
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        log("world %s mode %s" % (world.get_name(), MODE))
        ground = 0.0
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(ORIGIN.x, ORIGIN.y, 5000.0), unreal.Vector(ORIGIN.x, ORIGIN.y, -5000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        if hit is not None:
            ground = hit.to_tuple()[4].z
        log("ground z %.1f" % ground)
        base = unreal.Vector(ORIGIN.x, ORIGIN.y, ground + PLATE_HEIGHT)
        prefix = "airfield_" + MODE
        if MODE == "neutral":
            sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0.0, 0.0, 800.0),
                                                unreal.Rotator(pitch=-50.0, yaw=25.0, roll=0.0))
            sun.light_component.set_intensity(6.0)
            actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0.0, 0.0, 0.0))
            sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0.0, 0.0, 600.0))
            sky.light_component.set_editor_property("real_time_capture", True)
            sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
            floor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0.0, 0.0, ground))
            floor.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
            floor.set_actor_scale3d(unreal.Vector(40.0, 40.0, 1.0))
            self.spawn_site(actors, base)
            enemy_at = unreal.Vector(base.x, base.y + 900.0, base.z)
            self.spawn_enemy(actors, enemy_at)
            middle = unreal.Vector(base.x, base.y + 450.0, base.z)
            self.shots = [
                (prefix + "_close", self.camera(base, MAP_PITCH, 0.0, 260.0), 30.0),
                (prefix + "_with_enemy", self.camera(middle, MAP_PITCH, 0.0, 1500.0), 45.0),
                (prefix + "_low", self.camera(base, -35.0, 25.0, 200.0), 35.0),
            ]
        else:
            centres = []
            for index, emissive in enumerate(EMISSIVE_VARIANTS):
                location = unreal.Vector(base.x, base.y + index * SPACING, base.z)
                self.spawn_site(actors, location, emissive)
                centres.append(location)
                log("site %d emissive %s at %s" % (index, emissive, location))
            self.spawn_enemy(actors, unreal.Vector(base.x, base.y + 900.0, base.z))
            middle = unreal.Vector(base.x, base.y + SPACING * 2.0, ground)
            middle = unreal.Vector(base.x, base.y + 450.0, base.z)
            self.shots = [
                (prefix + "_close", self.camera(centres[0], MAP_PITCH, 0.0, 260.0), 30.0),
                (prefix + "_with_enemy", self.camera(middle, MAP_PITCH, 0.0, 1500.0), 45.0),
                (prefix + "_mapsize", self.camera(centres[0], MAP_PITCH, 0.0, 2800.0), 60.0),
            ]

    def tick(self, delta):
        import os as system
        import time as clock
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
        elif self.phase == "settle" and now - self.mark > 70.0:
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
            self.pending = system.path.join(OUT, name + ".png")
            unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, self.pending, camera)
            log("shot %s" % name)
            self.mark = now
        elif self.phase == "quit" and now - self.mark > 5.0:
            self.phase = "done"
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.SystemLibrary.quit_editor()


CAPTURE = Capture()
