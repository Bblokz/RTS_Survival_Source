"""Throwaway-editor test map for the Reichstag ruin (never run in the shared editor).

Launch from PowerShell (Git Bash rewrites /Game/ paths):
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecutePythonScript=<this file> -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
The editor quits when the script returns, so everything runs synchronously.

1. Saves the open RTS_ToCopySmall as UT_ReichstagRuin (or, when that map is the one open, clears the actors this script
   placed before).
2. Places BP_ReichstagRuin_B2 and the existing Soviet Blueprints from RT_Layout.json: each Blueprint's main mesh
   bounds centre goes on its layout spot (the meshes' pivots are off centre), seated on the ground by a trace.
3. Rebuilds navigation, saves, and writes captures + a report to Saved/ReichstagRuin/.
"""
import json
import math

import unreal

LAYOUT = 'D:/UE5Projects/RTS_Survival/Source/Tools/ReichstagRuin/RT_Layout.json'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/ReichstagRuin/'
NEW_MAP = '/Game/RTS_Survival/Maps/UnitTests/UT_ReichstagRuin'
RUIN = '/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin/BP_ReichstagRuin_B2'
B = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
BLUEPRINTS = {
    'Rus_Building_Factory': B + 'RusBuildings/Rus_Building_Factory',
    'Rus_Building_Barracks': B + 'RusBuildings/Rus_Building_Barracks',
    'Rus_BXP_120MortarPit': B + 'RusMortarPit/Rus_BXP_120MortarPit',
    'Rus_BXP_DShKPit': B + 'RusDShKPit/Rus_BXP_DShKPit',
    'Rus_BXP_BS3Pit': B + 'RusBS3Pit/Rus_BXP_BS3Pit',
    'Rus_BXP_53KPit': B + 'Rus53KPit/Rus_BXP_53KPit',
}
TAG = 'RT_Test'
EXPOSURE_BIAS = 1.5  # EV for the manual-exposure captures
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
REPORT = {'log': []}


def log(msg):
    REPORT['log'].append(msg)
    unreal.log('RTTEST ' + str(msg))


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground_z(w, x, y, ignore):
    hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 50000), unreal.Vector(x, y, -50000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    if not t[0]:
        return None
    vecs = [v for v in t if isinstance(v, unreal.Vector)]
    return vecs[0].z if vecs else None


def main_mesh(actor):
    best, size = None, -1
    for c in actor.get_components_by_class(unreal.StaticMeshComponent):
        if isinstance(c, unreal.InstancedStaticMeshComponent) or c.static_mesh is None:
            continue
        ext = c.static_mesh.get_bounds().box_extent
        s = ext.x * ext.y
        if s > size:
            best, size = c, s
    return best


def place_blueprint(w, name, target_xy, ground_ignore):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(BLUEPRINTS[name])
    a = ACTORS.spawn_actor_from_class(cls, unreal.Vector(target_xy[0], target_xy[1], 0))
    a.tags = [TAG]
    comp = main_mesh(a)
    if comp is None:
        log(f'{name}: no static mesh component')
        return a
    centre = comp.get_world_transform().transform_location(comp.static_mesh.get_bounds().origin)
    loc = a.get_actor_location()
    gz = ground_z(w, target_xy[0], target_xy[1], ground_ignore + [a])
    a.set_actor_location(unreal.Vector(loc.x + target_xy[0] - centre.x, loc.y + target_xy[1] - centre.y,
                                       gz if gz is not None else 0.0), False, False)
    centre = comp.get_world_transform().transform_location(comp.static_mesh.get_bounds().origin)
    log(f'{name}: mesh {comp.static_mesh.get_name()} centre at ({centre.x:.0f}, {centre.y:.0f}), ground {gz}')
    return a


def fixed_exposure(comp, bias=EXPOSURE_BIAS):
    """Manual exposure: a lone capture in a map without built lighting otherwise comes out nearly black."""
    pp = comp.get_editor_property('post_process_settings')
    pp.set_editor_property('override_auto_exposure_method', True)
    pp.set_editor_property('auto_exposure_method', unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property('override_auto_exposure_bias', True)
    pp.set_editor_property('auto_exposure_bias', bias)
    comp.set_editor_property('post_process_settings', pp)
    comp.set_editor_property('post_process_blend_weight', 1.0)


def capture(w, name, eye, target, ortho_width=None, fov=45.0, res=(1600, 1000), source=None):
    rt = unreal.RenderingLibrary.create_render_target2d(w, res[0], res[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cam = ACTORS.spawn_actor_from_class(unreal.SceneCapture2D, eye)
    comp = cam.capture_component2d
    comp.texture_target = rt
    comp.capture_source = source or unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    fixed_exposure(comp)
    if ortho_width:
        comp.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC
        comp.ortho_width = ortho_width
    else:
        comp.fov_angle = fov
    cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), False)
    for _ in range(3):           # first captures can run before materials and streaming settle
        comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, OUT, name + '.png')
    ACTORS.destroy_actor(cam)
    log(f'capture {name}')


def top_capture(w, centre, width):
    rt = unreal.RenderingLibrary.create_render_target2d(w, 1000, 1000, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cam = ACTORS.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(centre.x, centre.y, centre.z + 20000))
    comp = cam.capture_component2d
    comp.texture_target = rt
    comp.capture_source = unreal.SceneCaptureSource.SCS_BASE_COLOR
    comp.projection_type = unreal.CameraProjectionMode.ORTHOGRAPHIC
    comp.ortho_width = width
    # looking straight down, yaw 180: north (-X) is up in the picture and east (-Y) to the right, like the B2 top view
    cam.set_actor_rotation(unreal.Rotator(roll=0, pitch=-90, yaw=180), False)
    for _ in range(3):
        comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(w, rt, OUT, 'ue_top.png')
    ACTORS.destroy_actor(cam)
    log('capture ue_top')


# Routes the layout promises (Unreal cm from the ruin origin; +X south to the Gate, +Y west to the portico)
NAV_ROUTES = {
    'plaza -> hall (portico cut, facade gap, inner west gap)': ((0, 9000), (0, 1800)),
    'plaza -> north courtyard (barracks)': ((0, 9000), (-2675, 0)),
    'plaza -> south courtyard (pits)': ((0, 9000), (2675, 0)),
    'east outside -> south courtyard (SE rubble ramp)': ((3000, -7000), (2675, -500)),
}


def nav_checks(w, origin):
    """Path queries on the rebuilt navmesh: reachable or partial, and how long compared with the straight line."""
    out = {}
    for name, ((sx, sy), (ex, ey)) in NAV_ROUTES.items():
        a = unreal.Vector(origin.x + sx, origin.y + sy, origin.z + 100)
        b = unreal.Vector(origin.x + ex, origin.y + ey, origin.z + 100)
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(w, a, b)
        if path is None or not path.path_points:
            out[name] = 'no path'
        else:
            pts = path.path_points
            end = pts[-1]
            miss = math.hypot(end.x - b.x, end.y - b.y)
            out[name] = dict(partial=bool(path.is_partial()), end_miss_cm=round(miss), length_m=round(path.get_path_length() / 100, 1),
                             straight_m=round(math.hypot(b.x - a.x, b.y - a.y) / 100, 1))
        log(f'nav {name}: {out[name]}')
    REPORT['nav'] = out


def main():
    w = world()
    pkg = w.get_outer().get_name() if w.get_outer() else ''
    log(f'open map {w.get_name()} package {pkg}')
    if w.get_name() == 'UT_ReichstagRuin':
        for a in ACTORS.get_all_level_actors():
            if TAG in [str(t) for t in a.tags]:
                ACTORS.destroy_actor(a)
    else:
        ok = unreal.EditorLoadingAndSavingUtils.save_map(w, NEW_MAP)
        log(f'saved as {NEW_MAP}: {ok}')
        w = world()
    for a in ACTORS.get_all_level_actors():
        if a.get_class().get_name() in ('Landscape', 'LandscapeProxy', 'StaticMeshActor'):
            o, e = a.get_actor_bounds(False)
            log(f'ground actor {a.get_name()} centre ({o.x:.0f},{o.y:.0f},{o.z:.0f}) extent ({e.x:.0f},{e.y:.0f},{e.z:.0f})')

    layout = json.loads(open(LAYOUT, encoding='utf-8').read())
    gz = ground_z(w, 0, 0, [])
    origin = unreal.Vector(0, 0, gz if gz is not None else 0.0)
    log(f'ruin origin ground z {gz}')
    ruin = ACTORS.spawn_actor_from_class(unreal.EditorAssetLibrary.load_blueprint_class(RUIN), origin)
    ruin.tags = [TAG]
    isms = ruin.get_components_by_class(unreal.InstancedStaticMeshComponent)
    count = sum(c.get_instance_count() for c in isms)
    REPORT['instances'] = count
    log(f'ruin ISM components {len(isms)} instances {count} (layout {len(layout["pieces"])})')

    placed = []
    for row in layout['blueprints']:
        x, y = row['bounds_centre_cm']
        placed.append(place_blueprint(w, row['blueprint'], (origin.x + x, origin.y + y), [ruin]))

    # clearance: factory footprint against the ruin's hall walls (component bounds of the placed factory)
    fac = [a for a in placed if a.get_class().get_name().startswith('Rus_Building_Factory')]
    if fac:
        comp = main_mesh(fac[0])
        o, e = comp.static_mesh.get_bounds().origin, comp.static_mesh.get_bounds().box_extent
        c = comp.get_world_transform().transform_location(o)
        REPORT['factory_x_range_cm'] = [round(c.x - e.x), round(c.x + e.x)]
        REPORT['factory_y_range_cm'] = [round(c.y - e.y), round(c.y + e.y)]
        log(f'factory x {c.x - e.x:.0f}..{c.x + e.x:.0f} (hall walls inner faces at -1430 / 1430), y {c.y - e.y:.0f}..{c.y + e.y:.0f}')

    unreal.SystemLibrary.execute_console_command(w, 'RebuildNavigation')
    # save_map to the map's own path is a no-op once it has that name; save the level itself
    saved = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    log(f'saved map {saved}')

    nav_checks(w, origin)
    unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 0')
    try:
        target = unreal.Vector(origin.x, origin.y, origin.z + 600)
        # the concept sheets' review camera: from the south-west (front-right of the portico), 32 degrees up
        az, el = math.radians(54.2), math.radians(32.4)
        d = unreal.Vector(math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))
        capture(w, 'ue_review', target + d * 60000, target, ortho_width=15000)
        el = math.radians(55.0)
        d = unreal.Vector(math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))
        capture(w, 'ue_game_angle', target + d * 17000, target, fov=50.0)
        capture(w, 'ue_game_angle_albedo', target + d * 17000, target, fov=50.0,
                source=unreal.SceneCaptureSource.SCS_BASE_COLOR)
        top_capture(w, origin, 17000)
    finally:
        unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 1')
    json.dump(REPORT, open(OUT + 'report.json', 'w'), indent=1)


try:
    main()
except Exception as exc:
    log(f'FAILED {exc!r}')
    json.dump(REPORT, open(OUT + 'report.json', 'w'), indent=1)
    raise
