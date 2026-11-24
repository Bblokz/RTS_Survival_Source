"""Throwaway-editor look test of the Berlin ruin kit material (M_BerlinRuin): pictures from a game-like camera.
Never on Ger_Berlin, never in the shared editor. Launch with ruin_look_run.sh <outname> (same folder):
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
The map is never saved; the editor quits at the end. No PIE: plain mesh actors in the editor world.

Scene (on a plane of the Berlin ash ground texture, with a mud and a landscape-rubble patch as references), seen from
+Y so the sun lights the faces toward the camera:
  back row    kit pieces as on the map: E7 half house and E5 shell on their skirts, B3 facade; custom primitive data
              0-3 as step_ruins writes them (tint + soot). A second group 90 m behind: E1 corner tower on its skirt,
              B7 wall core, B5 corner.
  middle row  loose rubble heaps on MI_BerlinRuin_Rubble, one per berlin_map_build.RUBBLE_TINTS entry, one without
              primitive data and three pieces of flat street litter (also without).
  front row   collapse heaps on MI_BerlinRuin_Rubble_Foot, one per FOOT_TINTS entry.
  group 3     (picture game_25m_instanced) two hierarchical instanced components made by
              berlin_map_build._instance_actor with the brickiest and the greyest RUBBLE_TINTS entry, and a dirt-tracks
              decal across a collapse heap and the ground.
The lights are set to berlin_map_build.step_lighting's values as of 2026-11-13 morning (cold sun, sky, fog 0.0035,
saturation 0.86, 6900 K); the template's drifting cloud-shadow light function is switched off so two runs compare
(RUIN_LOOK_CLOUDS=1 keeps it). The picture is darker than the map's own critic shots; judge relative to the patches.
Pictures (1600 x 900, editor viewport high-res screenshot through a CameraActor, as the map critic shots):
  game_42m, game_42m_heroes (42 m up, pitch -48, fov 60), game_25m_pieces, game_25m_heaps, game_25m_heaps_left,
  close_12m_heaps, top (straight down from 120 m, for the colour sampling).
Colour sampling (ruin_look_colours.py, run by the launcher): mean display sRGB of `top` round each heap and patch
-> <out>/colours.json. Before | after sheets of two runs: ruin_look_compare.py.
Results: Saved/Berlin/ruinkit_look/<outname>/.
Env: RUIN_LOOK_OUT (folder name, set by the launcher), RUIN_LOOK_EXEC (a python file run in this module before the
scene is placed: unsaved experiments such as other tint tables or instance parameters).
"""
import json
import math
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..')))
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/ruinkit_look/' + os.environ.get('RUIN_LOOK_OUT', 'run') + '/'
KIT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/'
GROUND = '/Game/RTS_Survival/Materials/ground/Landscape/Berlin/Textures/T_Berlin_Berlin%s_D'
DBK = '/Game/DBK/Models/Rubble/'
CENTRE = (0.0, 0.0)
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'pics': [], 'spots': {}, 'log': []}

try:
    import berlin_map_build as BMB
    RUBBLE_TINTS, FOOT_TINTS = BMB.RUBBLE_TINTS, BMB.FOOT_TINTS
except Exception as error:  # noqa: BLE001
    unreal.log_warning('RUINLOOK berlin_map_build not importable (%r): no tints' % (error,))
    RUBBLE_TINTS = FOOT_TINTS = ((),)

# Group 1 (centre CENTRE, seen from +Y looking -Y: the sun lights the +Y faces; picture right = +X).
# kit pieces: (piece, x, y, yaw, skirt mesh or None, cpd 0-3) ; typical tint / soot values of the dress file
PIECES = [('E7_HalfHouse_A', -2100, -1700, 90, 'SM_BerlinFoot_HalfHouse_A', (0.876, 0.867, 0.857, 0.924)),
          ('B3_Facade4St', 0, -1700, 90, None, (0.906, 0.916, 0.926, 1.182)),
          ('E5_Shell1St_A', 2100, -1700, 90, 'SM_BerlinFoot_Shell_A', (0.95, 0.949, 0.948, 0.674)),
          # group 2, 90 m further along -Y
          ('E1_CornerTower', -1500, -10700, 90, 'SM_BerlinFoot_CornerTenement', (0.888, 0.865, 0.843, 1.306)),
          ('B7_WallCore', 1300, -10500, 90, None, (1.033, 1.055, 1.078, 1.072)),
          ('B5_CornerL', 2900, -9300, 90, None, (0.873, 0.868, 0.863, 0.871))]
HEAPS = ['Concrete/DBK_Concrete_Rubble_Mid_A', 'Brick/DBK_Brick_Rubble_A', 'Concrete/DBK_Concrete_Rubble_Mid_B',
         'Cinderblock/DBK_Cinderblock_Rubble_A']
HEAP_X = (-2250, -1350, -450, 450)
UNTINTED = ('Concrete/DBK_Concrete_Rubble_Mid_C', 1350)
LITTER = [('Brick/DBK_Brick_Rubble_C', 2100, -150), ('Concrete/DBK_Concrete_Rubble_Small_B', 2400, 120),
          ('Brick/DBK_Brick_Rubble_D', 2150, 250)]
COLLAPSE_X = (-1800, -600, 600, 1800)


def log(message):
    S['log'].append(message)
    unreal.log('RUINLOOK ' + message)


def finish():
    try:
        json.dump(S['log'], open(ROOT + 'log.json', 'w'), indent=1)
    finally:
        unreal.SystemLibrary.quit_editor()
        S['phase'] = 'done'


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else 0.0


def ground_material(layer, tiles):
    """Unsaved material: one Berlin ground albedo, `tiles` repeats over the plane (4 m per tile)."""
    mel = unreal.MaterialEditingLibrary
    m = unreal.new_object(unreal.Material, name='M_RuinLook_' + layer)
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    uv.set_editor_property('u_tiling', float(tiles))
    uv.set_editor_property('v_tiling', float(tiles))
    tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -350, 0)
    tex.set_editor_property('texture', unreal.load_asset(GROUND % layer))
    mel.connect_material_expressions(uv, '', tex, 'UVs')
    mel.connect_material_property(tex, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -350, 300)
    rough.set_editor_property('r', 0.92)
    mel.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    return m


def spawn_mesh(mesh, x, y, z, yaw=0.0, scale=1.0, material=None, cpd=None, label=None):
    actor = actors().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, yaw))
    comp = actor.static_mesh_component
    comp.set_static_mesh(mesh)
    if isinstance(scale, (int, float)):
        scale = (scale, scale, scale)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if material is not None:
        for slot in range(comp.get_num_materials()):
            comp.set_material(slot, material)
    for index, value in enumerate(cpd or ()):
        comp.set_default_custom_primitive_data_float(index, float(value))
    if label:
        actor.set_actor_label(label)
    return actor


def set_lighting():
    """berlin_map_build.step_lighting's values on this map's own lights (the unit-test map is lit for the desert)."""
    report = []
    for actor in actors().get_all_level_actors():
        name = actor.get_class().get_name()
        if name == 'DirectionalLight':
            comp = actor.light_component
            comp.set_intensity(3.4)
            comp.set_light_color(unreal.LinearColor(0.93, 0.95, 1.0, 1.0))
            actor.set_actor_rotation(unreal.Rotator(0.0, -48.0, -120.0), False)
            if os.environ.get('RUIN_LOOK_CLOUDS') != '1':
                # the template's drifting cloud-shadow light function makes two runs differ: off for the comparison
                comp.set_light_function_material(None)
            report.append('sun')
        elif name == 'SkyLight':
            comp = actor.light_component
            comp.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
            comp.set_intensity(1.15)
            comp.set_light_color(unreal.LinearColor(0.88, 0.92, 1.0, 1.0))
            comp.recapture_sky()
            report.append('sky')
        elif name == 'ExponentialHeightFog':
            comp = actor.component
            comp.set_fog_inscattering_color(unreal.LinearColor(0.36, 0.39, 0.43, 1.0))
            comp.set_fog_density(0.0035)
            report.append('fog')
        elif name == 'PostProcessVolume':
            settings = actor.settings
            settings.set_editor_property('override_color_saturation', True)
            settings.set_editor_property('color_saturation', unreal.Vector4(0.86, 0.86, 0.86, 1.0))
            settings.set_editor_property('override_white_temp', True)
            settings.set_editor_property('white_temp', 6900.0)
            actor.set_editor_property('settings', settings)
            report.append('post(unbound %s)' % actor.get_editor_property('unbound'))
    log('lighting %s' % report)


def place_all():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log('editor map %s, out %s' % (world.get_path_name(), ROOT))
    cx, cy = CENTRE
    z = ground_z(world, cx, cy) + 60.0          # a plane above the map's own ground: nothing of the desert shows
    S['z'] = z
    plane = unreal.load_asset('/Engine/BasicShapes/Plane')
    spawn_mesh(plane, cx, cy - 4000, z, 0, 260.0, ground_material('Ash', 65), label='LOOK_Ground')
    spawn_mesh(plane, cx - 3000, cy + 100, z + 2, 0, (5.0, 5.0, 1.0), ground_material('Mud', 1.25), label='LOOK_Mud')
    spawn_mesh(plane, cx + 3050, cy + 100, z + 2, 0, (5.0, 5.0, 1.0), ground_material('Rubble', 1.25), label='LOOK_LsRubble')
    S['spots']['ground_ash'] = (cx + 3600, cy + 2600, 250)
    S['spots']['ground_mud'] = (cx - 3000, cy + 100, 180)
    S['spots']['ground_landscape_rubble'] = (cx + 3050, cy + 100, 180)
    foot = unreal.load_asset(KIT + 'Materials/MI_BerlinRuin_Rubble_Foot')
    rubble = unreal.load_asset(KIT + 'Materials/MI_BerlinRuin_Rubble')
    for index, (piece, x, y, yaw, skirt, cpd) in enumerate(PIECES):
        mesh = unreal.load_asset(KIT + 'Meshes/SM_BerlinRuin_' + piece)
        actor = spawn_mesh(mesh, cx + x, cy + y, z, yaw, cpd=cpd, label='LOOK_' + piece)
        comp = actor.static_mesh_component
        log('%s slots %s' % (piece, [comp.get_material(i).get_name() for i in range(comp.get_num_materials())]))
        if skirt:
            tint = FOOT_TINTS[index % len(FOOT_TINTS)]
            spawn_mesh(unreal.load_asset(KIT + 'Footprints/' + skirt), cx + x, cy + y, z, yaw, material=foot, cpd=tint,
                       label='LOOK_Foot_' + piece)
    # middle row: loose heaps, one per RUBBLE_TINTS entry, then one without primitive data and the flat litter
    for index, tint in enumerate(RUBBLE_TINTS[:len(HEAP_X)]):
        x = HEAP_X[index]
        spawn_mesh(unreal.load_asset(DBK + HEAPS[index]), cx + x, cy, z, 37.0 * index, 1.0, rubble, tint, 'LOOK_Heap_%d' % index)
        S['spots']['heap_tint%d' % index] = (cx + x, cy, 110)
    spawn_mesh(unreal.load_asset(DBK + UNTINTED[0]), cx + UNTINTED[1], cy, z, 20.0, 1.0, rubble, None, 'LOOK_Heap_untinted')
    S['spots']['heap_untinted'] = (cx + UNTINTED[1], cy, 110)
    for index, (name, x, y) in enumerate(LITTER):
        spawn_mesh(unreal.load_asset(DBK + name), cx + x, cy + y, z, 60.0 * index, 1.0, rubble, None, 'LOOK_Litter_%d' % index)
    # front row: collapse heaps on the skirt instance, one per FOOT_TINTS entry
    for index, tint in enumerate(FOOT_TINTS[:len(COLLAPSE_X)]):
        name = 'SM_BerlinFoot_CollapseHeap_' + ('L' if index % 2 else 'S')
        x = COLLAPSE_X[index]
        spawn_mesh(unreal.load_asset(KIT + 'Footprints/' + name), cx + x, cy + 1500, z, 90.0 * index, 1.0, foot, tint,
                   'LOOK_Collapse_%d' % index)
        S['spots']['collapse_tint%d' % index] = (cx + x, cy + 1500, 170)
    place_instanced(cx, cy, z, foot)
    set_lighting()
    S['camera'] = actors().spawn_actor_from_class(unreal.CameraActor, unreal.Vector(cx, cy, z + 4200))
    S['camera'].camera_component.set_editor_property('constrain_aspect_ratio', False)
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    # (name, eye offset, pitch, yaw, fov)
    S['pics'] = [('_warmup', (0, 3000, 4200), -48, -90, 60),
                 ('game_42m', (0, 3800, 4200), -48, -90, 60),
                 ('game_42m_heroes', (700, -5200, 4200), -48, -90, 60),
                 ('game_25m_pieces', (0, 900, 2500), -48, -90, 60),
                 ('game_25m_heaps', (0, 2700, 2500), -48, -90, 60),
                 ('game_25m_heaps_left', (-1400, 2400, 2500), -48, -90, 60),
                 ('close_12m_heaps', (-900, 1100, 1200), -48, -90, 60),
                 ('game_25m_instanced', (0, 8900, 2500), -48, -90, 60),
                 ('top', (0, 0, 12000), -90, -90, 60)]


DECAL = '/Game/RTS_Survival/Materials/Decals/DirtTracks/MI_Decal_Dirt_Tracks_02'


def place_instanced(cx, cy, z, foot):
    """Group 3 (60 m toward the camera side): the map's own instanced path and a ground decal across a heap.
    Two hierarchical instanced components of one heap mesh, built by berlin_map_build._instance_actor with the
    greyest and the brickiest RUBBLE_TINTS entry (custom primitive data per component, desaturation in slot 4), and a
    dirt-tracks decal laid across a collapse heap and the ground as step_decals places them."""
    try:
        mesh = unreal.load_asset(DBK + 'Brick/DBK_Brick_Rubble_A')
        rubble = unreal.load_asset(KIT + 'Materials/MI_BerlinRuin_Rubble')
        by_grey = sorted(RUBBLE_TINTS, key=lambda t: t[4] if len(t) > 4 else 0.0)
        for name, tint, x0 in (('brickiest', by_grey[0], -2100.0), ('greyest', by_grey[-1], 300.0)):
            transforms = [unreal.Transform(unreal.Vector(cx + x0 + 700.0 * k, cy + 6000.0, z), unreal.Rotator(0, 0, 50.0 * k),
                                           unreal.Vector(1, 1, 1)) for k in range(3)]
            actor = BMB._instance_actor('LOOK_ISM_' + name, 'Look', mesh, transforms, material=rubble, cpd=tint)
            comp = actor.get_component_by_class(unreal.HierarchicalInstancedStaticMeshComponent)
            log('instanced %s: %d instances, tint %s, component data %s' % (
                name, comp.get_instance_count(), tint,
                [round(v, 2) for v in comp.get_editor_property('custom_primitive_data').get_editor_property('data')]))
            S['spots']['ism_' + name] = (cx + x0 + 700.0, cy + 6000.0, 110)
        spawn_mesh(unreal.load_asset(KIT + 'Footprints/SM_BerlinFoot_CollapseHeap_L'), cx, cy + 7300.0, z, 0.0, 1.0, foot,
                   FOOT_TINTS[0], 'LOOK_Collapse_decal')
        decal = actors().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(cx + 500.0, cy + 7300.0, z + 20.0),
                                                unreal.Rotator(0.0, -90.0, 0.0))
        decal.decal.set_decal_material(unreal.load_asset(DECAL))
        decal.decal.set_editor_property('decal_size', unreal.Vector(260.0, 500.0, 500.0))
        decal.set_actor_label('LOOK_Decal')
    except Exception as error:  # noqa: BLE001
        log('instanced group failed: %r' % (error,))


def picture():
    name, eye, pitch, yaw, fov = S['current']
    if S.get('taken') == name:        # the screenshot pumps Slate too
        return
    S['taken'] = name
    unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, '%s%s.png' % (ROOT, name), S['camera'])
    log('picture %s' % name)


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done':
        return
    try:
        if S['phase'] == 'wait_load' and S['t'] > 15.0:
            S['phase'] = 'placing'      # loading assets pumps Slate: without this the tick re-enters and places again
            os.makedirs(ROOT, exist_ok=True)
            hook = os.environ.get('RUIN_LOOK_EXEC')
            if hook:        # unsaved experiments: a python file run in this module before the scene is placed
                exec(compile(open(hook).read(), hook, 'exec'), globals())
                log('hook %s run' % hook)
            place_all()
            json.dump({'spots': S['spots'], 'centre': CENTRE, 'top_height': 12000, 'fov': 60, 'size': [1600, 900]},
                      open(ROOT + 'spots.json', 'w'), indent=1)
            S['phase'], S['mark'] = 'settle', S['t']
        elif S['phase'] == 'settle' and S['t'] - S['mark'] > 25.0:      # shaders, sky capture, Lumen
            S['phase'], S['mark'] = 'pics', S['t'] - 100.0
        elif S['phase'] == 'pics':
            if S.get('current') and not S.get('shot') and S['t'] - S['mark'] > 4.0:
                picture()
                S['shot'] = True
                S['mark'] = S['t']
            elif (not S.get('current') or (S.get('shot') and S['t'] - S['mark'] > 5.0)):
                if not S['pics']:
                    finish()
                    return
                S['current'] = S['pics'].pop(0)
                name, eye, pitch, yaw, fov = S['current']
                camera = S['camera']
                camera.set_actor_location(unreal.Vector(CENTRE[0] + eye[0], CENTRE[1] + eye[1], S['z'] + eye[2]), False, True)
                camera.set_actor_rotation(unreal.Rotator(0, pitch, yaw), True)
                camera.camera_component.set_field_of_view(fov)
                S['shot'], S['mark'] = False, S['t']
    except Exception as error:  # noqa: BLE001
        log('ERROR %s: %r' % (S['phase'], error))
        finish()
    if S['t'] > 400.0 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
