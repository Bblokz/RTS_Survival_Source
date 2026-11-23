"""Throwaway-editor comparison of dead-tree hole looks (2026-11-11, the wall-holes Shorts: trunk holes did not read).

Three BP_Scorch_Dest_DeadTree side by side, each hit by the same debug shells (no health cap), then HighResShot stills:
  A  as shipped (charred inner tint, HoleRadiusPerCalibreMm 0.4, max 40 cm)
  B  inner faces fresh pale wood (runtime MID of the hole instance: WallHoleInnerColor / WallHoleInnerDarken)
  C  B plus HoleRadiusPerCalibreMm 0.65, max 55 cm
Nothing is saved: the variants are runtime overrides in PIE. Run in a throwaway DebugGame editor, never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_SurvivalShorts.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Output: Saved/WallChunks/Trees/visibility_*.png and visibility_result.json.
"""
import json
import os

import unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()) + 'WallChunks/Trees/'
os.makedirs(ROOT, exist_ok=True)
OUT = ROOT + 'visibility_result.json'
BP = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/BP_Scorch_Dest_DeadTree.BP_Scorch_Dest_DeadTree_C'
SPOT = unreal.Vector(0, 0, 0)
SCOUT = ('/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G.'
         'Ger_Vehicle_PanzerIV_G_C')
GAP = 700.0
PALE_WOOD = unreal.LinearColor(0.50, 0.34, 0.19, 1.0)
VARIANTS = [('A', None, None), ('B', PALE_WOOD, None), ('C', PALE_WOOD, (0.65, 55.0))]
# (height cm, sideways offset cm, calibre mm): shells from -X, as the tanks fire.
SHOTS = [(170, 0, 20), (230, -10, 20), (300, 8, 20), (380, -5, 20), (260, 0, 37), (450, 0, 75)]
VIEWS = {'all': ((-1500, 0, 380), (0, 0, 330), 22.0), 'treeA': ((-650, -GAP - 200, 330), (0, -GAP, 320), 30.0),
         'treeB': ((-650, -200, 330), (0, 0, 320), 30.0), 'treeC': ((-650, GAP - 200, 330), (0, GAP, 320), 30.0)}
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'views': list(VIEWS.items())}


def log(message):
    S['log'].append(message)
    unreal.log('TREEVIS ' + message)


def finish():
    json.dump(S['log'], open(OUT, 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()
    S['phase'] = 'done'


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def trees(world):
    found = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
             if a.get_class().get_name() == 'BP_Scorch_Dest_DeadTree_C']
    return sorted(found, key=lambda a: a.get_actor_location().y)


def tune_hole_materials(tree, inner_color):
    """After the first hole swapped the masked instance in: replace it on the mesh with a dynamic copy (the
    component's DamagedMaterials is EditDefaultsOnly, so the runtime override goes on the mesh slot)."""
    mesh = [c for c in tree.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]
    for slot in range(mesh.get_num_materials()):
        material = mesh.get_material(slot)
        if material is None or 'WallHoles' not in material.get_name():
            continue
        dynamic = mesh.create_dynamic_material_instance(slot, material)
        dynamic.set_vector_parameter_value('WallHoleInnerColor', inner_color)
        dynamic.set_scalar_parameter_value('WallHoleInnerDarken', 1.0)


def shoot(world):
    for tree, (name, inner, radius) in zip(trees(world), VARIANTS):
        holes = tree.get_component_by_class(unreal.WallHoleDamageComponent)
        mesh = [c for c in tree.get_components_by_class(unreal.StaticMeshComponent) if c.get_name() == 'StaticMesh'][0]
        base = tree.get_actor_location()
        # HoleSettings is EditDefaultsOnly too: a bigger radius per mm is the same as a bigger calibre (capped at the
        # shipped 40 cm maximum, so C under-states 75 mm a little).
        calibre_scale = radius[0] / 0.4 if radius else 1.0
        for index, (height, side, calibre) in enumerate(SHOTS):
            start = base + unreal.Vector(-600, side, height)
            end = base + unreal.Vector(600, side, height)
            result = mesh.line_trace_component(start, end, False, False, False)
            if not result:
                log(f'{name} shot {height}/{calibre} missed')
                continue
            removed = holes.debug_fire_test_hit(result[0], unreal.Vector(1, 0, 0), float(calibre) * calibre_scale)
            log(f'{name} shot {height} cm {calibre} mm removed {removed:.2f} m2')
        if inner is not None:
            # After every hit: the component sets its masked instance on the slot again with each new hole.
            tune_hole_materials(tree, inner)
        log(f'{name} holes {holes.get_hole_count()} mesh {mesh.static_mesh.get_name()} slot0 {mesh.get_material(0).get_name()}')


def aim_camera(world, eye, look, focal):
    camera = S['camera']
    camera.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    camera.set_actor_location(SPOT + unreal.Vector(*eye), False, True)
    camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(SPOT + unreal.Vector(*eye), SPOT + unreal.Vector(*look)), True)
    camera.get_cine_camera_component().set_editor_property('current_focal_length', focal)
    unreal.GameplayStatics.get_player_controller(world, 0).set_view_target_with_blend(camera, 0.0)


def tick(delta):
    S['t'] += delta
    if S.get('busy'):
        return
    phase = S['phase']
    try:
        if phase == 'wait_load' and S['t'] > 15.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            ground = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(0, 0, 20000), unreal.Vector(0, 0, -20000),
                                                            unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                            unreal.DrawDebugTrace.NONE, True)
            if ground:
                SPOT.z = ground.to_tuple()[4].z
            actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            for index in range(len(VARIANTS)):
                actors.spawn_actor_from_class(unreal.load_class(None, BP), SPOT + unreal.Vector(0, (index - 1) * GAP, 0))
            actors.spawn_actor_from_class(unreal.CineCameraActor, SPOT + unreal.Vector(-1500, 0, 400))
            # Fog of war darkens everything no player unit sees: a player-owned tank behind the camera gives vision.
            scout = actors.spawn_actor_from_class(unreal.load_class(None, SCOUT), SPOT + unreal.Vector(-2600, 0, 80))
            scout.get_component_by_class(unreal.RTSComponent).set_editor_property('owning_player', 1)
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            world = game_world()
            if world is not None and S['t'] - S['mark'] > 12.0:
                unreal.GameplayStatics.set_game_paused(world, False)
                cameras = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.CineCameraActor)]
                S['camera'] = min(cameras, key=lambda c: (c.get_actor_location() - (SPOT + unreal.Vector(-1500, 0, 400))).length())
                log(f'trees {len(trees(world))}')
                shoot(world)
                S['phase'], S['mark'] = 'shots', S['t']
            elif S['t'] - S['mark'] > 120.0:
                log('pie never started')
                finish()
        elif phase == 'shots':
            world = game_world()
            if not S['views']:
                if S['t'] - S['mark'] > 3.0:
                    finish()
                return
            if S['t'] - S['mark'] > 2.0:
                name, (eye, look, focal) = S['views'].pop(0)
                aim_camera(world, eye, look, focal)
                S['pending'] = name
                S['phase'], S['mark'] = 'settle', S['t']
        elif phase == 'settle' and S['t'] - S['mark'] > 2.0 and 'pending' in S:
            name = S.pop('pending')
            S['phase'], S['mark'] = 'shots', S['t']
            S['busy'] = True
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(900, 1600, f'{ROOT}visibility_{name}.png', camera=S['camera'])
            finally:
                S['busy'] = False
            log(f'picture {name}')
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
