"""Pictures of airfield candidate B on a built hex plate in the world campaign level, map camera (70 degrees down).

Run with run_capture.ps1 (throwaway DebugGame editor with a renderer, own save slot; the level is not saved).
Scene, along +Y: BP_PlayerHQ at -340, the candidate (saved materials) at 0, and far outside both final frames three
variants with dynamic instances (emissive 0.5 / 1.1, and a lighter plane green) for the variants picture.
Pictures: Saved/WorldBuildSitePieces/airfield_candidate_B_{close,mapsize,variants}.png
"""
import math
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
PIECE = "/Game/RTS_Survival/WorldCampaign/BuildSites/Pieces/Airfield/Candidates/B/SM_BuildSitePiece_Airfield_B"
PLATE_MATERIAL = "/Game/RTS_Survival/WorldCampaign/BuildSites/M_BuildSiteHexPlate"
HQ = "/Game/RTS_Survival/WorldCampaign/MapObjects/Player/HQ/BP_PlayerHQ"
PLATE_SCALE = 57.6 / 52.0
PLATE_HEIGHT = 6.0           # AWorldBuildNode puts plate and piece 6 units over the ground
ORIGIN = unreal.Vector(1200.0, -6700.0, 0.0)
MAP_PITCH = -70.0
HQ_OFFSET = -340.0
# (y offset, scalar overrides for every slot, vector overrides on the plane slot)
VARIANTS = [(760.0, {"EmissiveStrength": 0.5}, {}),
            (860.0, {"EmissiveStrength": 1.1}, {}),
            (960.0, {}, {"BaseColor": (0.12, 0.19, 0.07), "CamoColorB": (0.12, 0.19, 0.07)})]
PLANE_SLOT = "PL_Body"


def log(message):
    unreal.log("AIRFIELDBCAP " + str(message))


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

    def spawn_site(self, actors, location, scalars=None, plane_vectors=None):
        plate = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
        plate.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane"))
        plate.set_actor_scale3d(unreal.Vector(PLATE_SCALE, PLATE_SCALE, 1.0))
        plate_material = plate.static_mesh_component.create_dynamic_material_instance(
            0, unreal.load_asset(PLATE_MATERIAL))
        plate_material.set_scalar_parameter_value("State", 2.0)
        plate_material.set_scalar_parameter_value("SignIndex", 6.0)
        piece = actors.spawn_actor_from_class(unreal.StaticMeshActor, location)
        component = piece.static_mesh_component
        component.set_static_mesh(unreal.load_asset(PIECE))
        component.set_collision_profile_name("NoCollision")
        if scalars or plane_vectors:
            names = component.get_material_slot_names()
            for slot in range(component.get_num_materials()):
                instance = component.create_dynamic_material_instance(slot, component.get_material(slot))
                for name, value in (scalars or {}).items():
                    instance.set_scalar_parameter_value(name, value)
                if str(names[slot]) == PLANE_SLOT:
                    for name, value in (plane_vectors or {}).items():
                        instance.set_vector_parameter_value(name, unreal.LinearColor(value[0], value[1], value[2], 1))
        return piece

    def spawn(self):
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        log("world %s" % world.get_name())
        ground = 0.0
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(ORIGIN.x, ORIGIN.y, 5000.0), unreal.Vector(ORIGIN.x, ORIGIN.y, -5000.0),
            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        if hit is not None:
            ground = hit.to_tuple()[4].z
        log("ground z %.1f" % ground)
        base = unreal.Vector(ORIGIN.x, ORIGIN.y, ground + PLATE_HEIGHT)
        self.spawn_site(actors, base)
        hq_class = unreal.load_object(None, HQ + "." + HQ.split("/")[-1] + "_C")
        hq = actors.spawn_actor_from_class(hq_class, unreal.Vector(ORIGIN.x, ORIGIN.y + HQ_OFFSET, ground + 1.0))
        log("player HQ spawned: %s" % (hq is not None))
        for offset, scalars, plane_vectors in VARIANTS:
            self.spawn_site(actors, unreal.Vector(base.x, base.y + offset, base.z), scalars, plane_vectors)
            log("variant at +%.0f: %s %s" % (offset, scalars, plane_vectors))
        target = unreal.Vector(base.x, base.y, base.z + 4.0)
        middle = unreal.Vector(base.x, base.y + HQ_OFFSET * 0.5, base.z + 10.0)
        variants = unreal.Vector(base.x, base.y + VARIANTS[1][0], base.z + 4.0)
        self.shots = [
            ("airfield_candidate_B_shaderwarmup", self.camera(target, MAP_PITCH, 0.0, 300.0), 30.0),
            ("airfield_candidate_B_close", self.camera(target, MAP_PITCH, 0.0, 250.0), 30.0),
            ("airfield_candidate_B_mapsize", self.camera(middle, MAP_PITCH, 0.0, 1550.0), 30.0),
            # Left to right: emissive 0.5, emissive 1.1, lighter plane green.
            ("airfield_candidate_B_variants", self.camera(variants, MAP_PITCH, 0.0, 700.0), 30.0),
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
        elif self.phase == "shoot" and now - self.mark > 5.0:
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
