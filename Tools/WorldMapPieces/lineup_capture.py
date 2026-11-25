"""Line-up picture of the five story mission pieces beside the player HQ piece, in the world campaign level.

Run in a throwaway DebugGame editor with a renderer (check nvidia-smi first), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -RenderOffscreen
The level is not saved. Pictures: Saved/WorldMapPieces/lineup_*.png
"""
import os

import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldMapPieces")
STORY = "/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Story/"
PIECES = [
    "/Game/RTS_Survival/WorldCampaign/MapObjects/Player/HQ/BP_PlayerHQ",
    STORY + "OreMountains/BP_Mission_OreMountains",
    STORY + "Berlin/BP_Mission_Berlin",
    STORY + "OderWarthe/BP_Mission_OderWarthe",
    STORY + "Pripyat/BP_Mission_Pripyat",
    STORY + "FrozenLake/BP_Mission_FrozenLake",
]
# WORLDPIECES_TESTMAP=1: the run is on a lit unit-test map, with the real level meshes beside the pieces.
ON_TEST_MAP = os.environ.get("WORLDPIECES_TESTMAP", "") == "1"
ORIGIN = unreal.Vector(0.0, 0.0, 0.0) if ON_TEST_MAP else unreal.Vector(1200.0, -6700.0, 0.0)
GAME = "/Game/RTS_Survival/Blueprints/"
REFERENCES = [
    (GAME + "Environment/Berlin/FlakTower/SM_BF_FlakTower_Damaged", 0.0152, (300.0, -330.0)),
    (GAME + "GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/IV_H/SM_PanzerIV_H", 0.05, (0.0, -330.0)),
]
# WORLDPIECES_VARIANTS=1: no pieces; the concrete meshes in a row, the current look first, then the test settings of
# concrete_variants.py. One picture per mesh: variants_<mesh>.png
SHOW_VARIANTS = os.environ.get("WORLDPIECES_VARIANTS", "") == "1"
VARIANT_NAMES = ["A", "B", "C", "D"]
VARIANT_FOLDER = STORY + "Shared/ConcreteTest/MI_WP_ConcreteTest_%s_%s"
# (mesh, scale, concrete slots, instance kind)
VARIANT_MESHES = [
    (STORY + "OderWarthe/SM_WorldPiece_OderWarthe", (1.0, 1.0, 1.0), (0, 5), "Model"),
    (STORY + "OreMountains/SM_WorldPiece_OreMountains", (1.0, 1.0, 1.0), (3,), "Model"),
    (GAME + "Environment/Berlin/FlakTower/SM_BF_FlakTower_Damaged", (0.0152, 0.0152, 0.019), (0,), "Tower"),
]
SPACING = 300.0
MAP_PITCH = -70.0


class Lineup:
    def __init__(self):
        self.time = 0.0
        self.phase = "wait"
        self.mark = 0.0
        self.shots = []
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def log(self, message):
        import unreal as engine
        engine.log("LINEUP " + str(message))

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
        if SHOW_VARIANTS:
            self.spawn_variants(actors, ground)
            self.spawn_neutral_light(actors, ground)
            return
        self.centres = []
        for index, path in enumerate(PIECES):
            column, row = index % 3, index // 3
            location = engine.Vector(ORIGIN.x + column * SPACING, ORIGIN.y + row * SPACING, ground + 1.0)
            piece_class = engine.load_object(None, path + "." + path.split("/")[-1] + "_C")
            actor = actors.spawn_actor_from_class(piece_class, location)
            self.log("spawned %s: %s" % (path.split("/")[-1], actor is not None))
            self.centres.append(location)
        middle = engine.Vector(ORIGIN.x + SPACING, ORIGIN.y + SPACING * 0.5, ground)
        # The map camera looks along +X at 70 degrees down; a close shot, a map-distance shot and a low side shot.
        self.shots = [
            ("lineup_close", self.camera(middle, MAP_PITCH, 0.0, 1500.0), 30.0),
            ("lineup_mapsize", self.camera(middle, MAP_PITCH, 0.0, 2800.0), 60.0),
            ("lineup_side", self.camera(middle, -25.0, 0.0, 1300.0), 35.0),
            ("lineup_back", self.camera(middle, -35.0, 180.0, 1500.0), 35.0),
        ]
        for index in range(1, 6):
            self.shots.append(("piece_%d" % index, self.camera(self.centres[index], -45.0, 0.0, 620.0), 30.0))
        if ON_TEST_MAP:
            # A plain neutral light set, so the colours can be judged: sun from the camera side, sky fill, grey floor.
            sun = actors.spawn_actor_from_class(engine.DirectionalLight, engine.Vector(0.0, 0.0, 800.0),
                                                engine.Rotator(pitch=-50.0, yaw=25.0, roll=0.0))
            sun.light_component.set_intensity(6.0)
            actors.spawn_actor_from_class(engine.SkyAtmosphere, engine.Vector(0.0, 0.0, 0.0))
            sky = actors.spawn_actor_from_class(engine.SkyLight, engine.Vector(0.0, 0.0, 600.0))
            sky.light_component.set_editor_property("real_time_capture", True)
            sky.light_component.set_mobility(engine.ComponentMobility.MOVABLE)
            floor = actors.spawn_actor_from_class(engine.StaticMeshActor, engine.Vector(300.0, 0.0, ground - 1.0))
            floor.static_mesh_component.set_static_mesh(engine.load_asset("/Engine/BasicShapes/Plane"))
            floor.set_actor_scale3d(engine.Vector(40.0, 40.0, 1.0))
            for path, scale, offset in REFERENCES:
                location = engine.Vector(ORIGIN.x + offset[0], ORIGIN.y + offset[1], ground + 1.0)
                reference = actors.spawn_actor_from_class(engine.StaticMeshActor, location)
                reference.static_mesh_component.set_static_mesh(engine.load_asset(path))
                reference.set_actor_scale3d(engine.Vector(scale, scale, scale))
                self.shots.append(("reference_" + path.split("/")[-1],
                                   self.camera(location, -45.0, 0.0, 300.0), 30.0))

    def spawn_variants(self, actors, ground):
        import unreal as engine
        column_step, row_step = 230.0, 1400.0
        self.shots = []
        for row, (path, scale, slots, kind) in enumerate(VARIANT_MESHES):
            mesh = engine.load_asset(path)
            for column, variant in enumerate([None] + VARIANT_NAMES):
                location = engine.Vector(row * row_step, (column - 2) * column_step, ground + 1.0)
                actor = actors.spawn_actor_from_class(engine.StaticMeshActor, location,
                                                      engine.Rotator(pitch=0.0, yaw=90.0, roll=0.0))
                actor.static_mesh_component.set_static_mesh(mesh)
                actor.set_actor_scale3d(engine.Vector(*scale))
                if variant is None:
                    continue
                material = engine.load_asset(VARIANT_FOLDER % (variant, kind))
                for slot in slots:
                    actor.static_mesh_component.set_material(slot, material)
            centre = engine.Vector(row * row_step, 0.0, ground + 45.0)
            name = path.split("/")[-1]
            self.shots.append(("variants_" + name, self.camera(centre, -32.0, 0.0, 1500.0), 44.0))
            self.shots.append(("variants_map_" + name, self.camera(centre, MAP_PITCH, 0.0, 1700.0), 40.0))
            near = engine.Vector(row * row_step, 0.5 * column_step, ground + 45.0)
            self.shots.append(("variants_near_" + name, self.camera(near, -28.0, 0.0, 520.0), 44.0))

    def spawn_neutral_light(self, actors, ground):
        import unreal as engine
        sun = actors.spawn_actor_from_class(engine.DirectionalLight, engine.Vector(0.0, 0.0, 800.0),
                                            engine.Rotator(pitch=-50.0, yaw=25.0, roll=0.0))
        sun.light_component.set_intensity(6.0)
        actors.spawn_actor_from_class(engine.SkyAtmosphere, engine.Vector(0.0, 0.0, 0.0))
        sky = actors.spawn_actor_from_class(engine.SkyLight, engine.Vector(0.0, 0.0, 600.0))
        sky.light_component.set_editor_property("real_time_capture", True)
        sky.light_component.set_mobility(engine.ComponentMobility.MOVABLE)
        floor = actors.spawn_actor_from_class(engine.StaticMeshActor, engine.Vector(1400.0, 0.0, ground - 1.0))
        floor.static_mesh_component.set_static_mesh(engine.load_asset("/Engine/BasicShapes/Plane"))
        floor.set_actor_scale3d(engine.Vector(80.0, 80.0, 1.0))

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
        # Wall clock: slate ticks many times inside one slow frame, so summed deltas run ahead of the renderer.
        if not hasattr(self, "start"):
            self.start = clock.time()
        self.time = clock.time() - self.start
        if getattr(self, "pending", None):
            if not system.path.exists(self.pending) and self.time - self.mark < 40.0:
                return
            self.pending = None
            self.mark = self.time
        if self.phase == "wait" and self.time > 25.0:
            try:
                self.spawn()
            except Exception as error:
                self.log("spawn failed: %s" % error)
                self.phase = "quit"
                return
            self.phase = "settle"
            self.mark = self.time
        elif self.phase == "settle" and self.time - self.mark > 20.0:
            self.phase = "shoot"
            self.mark = self.time - 10.0
        elif self.phase == "shoot" and self.time - self.mark > 4.0:
            if not self.shots:
                self.phase = "quit"
                self.mark = self.time
                return
            name, (location, rotation), field_of_view = self.shots.pop(0)
            world = engine.get_editor_subsystem(engine.UnrealEditorSubsystem).get_editor_world()
            camera = engine.get_editor_subsystem(engine.EditorActorSubsystem).spawn_actor_from_class(
                engine.CameraActor, location, rotation)
            camera.camera_component.set_field_of_view(field_of_view)
            self.pending = system.path.join(OUT, name + ".png")
            engine.AutomationLibrary.take_high_res_screenshot(1920, 1080, self.pending, camera)
            self.log("shot %s" % name)
            self.mark = self.time
        elif self.phase == "quit" and self.time - self.mark > 5.0:
            self.phase = "done"
            engine.unregister_slate_post_tick_callback(self.handle)
            engine.SystemLibrary.quit_editor()


LINEUP = Lineup()
