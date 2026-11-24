"""Throwaway-editor test of the rubble decal conventions (RTS_ToCopySmall, nothing saved).

Places, around the first landscape hit near the origin:
  A  cell 0 (fan, source = atlas top edge) yaw 0 at (0, 0), marker cube 300 cm towards +X
  B  cell 0 yaw 90 at (0, 650)
  C  cell 14 (sandstone, strong normals) yaw 0 at (0, -650)
  D  cell 0 yaw 0, DecalColor B = 0 (mirrored) at (650, 0)
Top-down camera (pitch -90, yaw 0: image up = +X, image right = +Y), narrow FOV, two pictures with the sun at yaw 0 and
yaw 180 (pitch -35). Output: Saved/Berlin/rubble_decals/orient/{sun0,sun180}.png + log.json.
"""
import json
import os

import unreal

ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/orient/'
FOLDER = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/'
S = {'t': 0.0, 'phase': 'wait_load', 'log': [], 'shots': [('sun0', 0.0), ('sun180', 180.0)]}
FOV = 12.0
HEIGHT = 10000.0


def log(m):
    S['log'].append(m)
    unreal.log('RUBORIENT ' + m)


def finish():
    try:
        json.dump(S['log'], open(ROOT + 'log.json', 'w'), indent=1)
    finally:
        S['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def ground_z(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    t = hit.to_tuple()
    return (t[4].z, t[9].get_name() if t[9] else None) if t[0] else (None, None)


def decal(world, cell, x, y, yaw, color=None, scale=1.0):
    z, _ = ground_z(world, x, y)
    a = actors().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(x, y, z + 20), unreal.Rotator(0, -90, yaw))
    c = a.decal
    names = ["BrickSpillFan", "BrickHeapSpread", "MasonryStrip", "CornerPile", "PlasterDust", "ConcreteRebar",
             "RoofTiles", "SlateShards", "CharredTimber", "AshSoot", "GlassShards", "SettsTornUp", "PavingSlabs",
             "SteelScrap", "SandstoneBlocks", "GritSpill"]
    c.set_decal_material(unreal.load_asset(FOLDER + 'MI_BerlinRubbleDecal_%02d_%s' % (cell, names[cell])))
    c.set_editor_property('decal_size', unreal.Vector(150.0, 200.0 * scale, 200.0 * scale))
    c.set_editor_property('fade_screen_size', 0.0)
    if color:
        c.set_editor_property('decal_color', unreal.LinearColor(*color))
    return a


def place():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    z0, hit = ground_z(world, 0, 0)
    log('map %s ground z %s hit %s' % (world.get_path_name(), z0, hit))
    S['z'] = z0
    decal(world, 0, 0, 0, 0)
    decal(world, 0, 0, 650, 90)
    decal(world, 14, 0, -650, 0)
    decal(world, 0, 650, 0, 0, (1, 1, 0, 1))
    z1, _ = ground_z(world, -650, 0)
    ref = actors().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(-650, 0, z1 + 20), unreal.Rotator(0, -90, 0))
    ref.decal.set_decal_material(unreal.load_asset('/Game/RTS_Survival/Materials/Decals/LandscapeImpacts/MI_LandscapeImpactDecal'))
    ref.decal.set_editor_property('decal_size', unreal.Vector(150.0, 200.0, 200.0))
    ref.decal.set_editor_property('decal_color', unreal.LinearColor(15.5 / 16.0, 0.5, 1, 1))
    for tn in ('T_BerlinRubbleDecal_BCO', 'T_BerlinRubbleDecal_NRS'):
        t = unreal.load_asset(FOLDER + tn)
        t.set_editor_property('never_stream', True)      # test only, never saved: full mips for the picture
        log('%s size %s srgb %s comp %s' % (tn, t.blueprint_get_size_x(), t.get_editor_property('srgb'),
                                         t.get_editor_property('compression_settings')))
    m = unreal.load_asset(FOLDER + 'M_BerlinRubbleDecal')
    st = unreal.MaterialEditingLibrary.get_statistics(m)
    log('mat PS %s samplers %s domain %s blend %s' % (st.num_pixel_shader_instructions, st.num_samplers,
        m.get_editor_property('material_domain'), m.get_editor_property('blend_mode')))
    cube = actors().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(300, 0, z0 + 10))
    cube.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    cube.set_actor_scale3d(unreal.Vector(0.25, 0.25, 0.25))
    cam = actors().spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0, 0, z0 + HEIGHT),
                                          unreal.Rotator(0, -90, 0))
    cam.camera_component.set_field_of_view(FOV)
    S['camera'] = cam
    for a in actors().get_all_level_actors():
        n = a.get_class().get_name()
        if n == 'DirectionalLight':
            S['sun'] = a
            a.light_component.set_light_function_material(None)
        if n in ('ExponentialHeightFog',):
            a.component.set_fog_density(0.0)
    log('placed; camera fwd %s up %s' % (cam.get_actor_forward_vector(), cam.get_actor_up_vector()))


LOGFILE = [a.split('=', 1)[1] for a in unreal.SystemLibrary.get_command_line().split() if a.lower().startswith('-abslog=')]


def textures_ready():
    sizes = [unreal.load_asset(FOLDER + n).blueprint_get_size_x() for n in ('T_BerlinRubbleDecal_BCO', 'T_BerlinRubbleDecal_NRS')]
    if S['t'] - S.get('tex_logged', -100) > 20.0:
        S['tex_logged'] = S['t']
        log('t %.0f texture sizes %s' % (S['t'], sizes))
    return min(sizes) >= 2048


def shaders_done():
    """True once this editor's log says the shader queue is empty after the decals were placed."""
    if not LOGFILE:
        return S['t'] - S['mark'] > 150.0
    if 'log_off' not in S:
        S['log_off'] = os.path.getsize(LOGFILE[0])
        S['last_busy'] = S['t']
        return False
    with open(LOGFILE[0], 'rb') as f:
        f.seek(S['log_off'])
        new = f.read().decode('utf-8', 'ignore')
    S['log_off'] += len(new.encode('utf-8'))
    for line in new.splitlines():
        if 'Shaders left to compile' in line:
            S['last_left'] = line.rsplit(' ', 1)[-1].strip()
            S['last_busy'] = S['t']
    return S.get('last_left') == '0' and S['t'] - S['last_busy'] > 8.0 or S['t'] - S['mark'] > 330.0


def tick(delta):
    S['t'] += delta
    if S['phase'] == 'done':
        return
    try:
        if S['phase'] == 'wait_load' and S['t'] > 15.0:
            S['phase'] = 'placing'
            os.makedirs(ROOT, exist_ok=True)
            place()
            S['phase'], S['mark'] = 'settle', S['t']
        elif S['phase'] == 'settle' and S['t'] - S['mark'] > 20.0 and textures_ready() and shaders_done():
            S['phase'] = 'shots'
            S['mark'] = S['t'] - 100
            S['cur'] = None
        elif S['phase'] == 'shots':
            if S['cur'] is None or (S.get('taken') and S['t'] - S['mark'] > 6.0):
                if not S['shots']:
                    finish()
                    return
                S['cur'] = S['shots'].pop(0)
                S['taken'] = False
                name, yaw = S['cur']
                S['sun'].set_actor_rotation(unreal.Rotator(0, -35.0, yaw), False)
                log('sun %s fwd %s' % (name, S['sun'].get_actor_forward_vector()))
                S['mark'] = S['t']
            elif not S['taken'] and S['t'] - S['mark'] > 6.0:
                S['taken'] = True
                S['mark'] = S['t'] + 4.0      # the screenshot pumps Slate: this tick re-enters before it returns
                name = S['cur'][0]
                unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, ROOT + name + '.png', S['camera'])
                log('shot ' + name)
    except Exception as e:  # noqa: BLE001
        log('ERROR %r' % e)
        finish()
    if S['t'] > 700 and S['phase'] != 'done':
        log('timeout')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('loaded')
