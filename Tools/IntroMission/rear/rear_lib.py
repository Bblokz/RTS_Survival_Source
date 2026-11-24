"""Helpers for the Ger_IntroMission rear areas (exec'd in the throwaway editor through rear_cmd.py).

Usage from a request:  exec(open(LIB).read())  then call the functions; the namespace persists between requests.
Everything this work spawns is labelled REAR_* and lives in the outliner folder IntroRear/...; clear_rear() removes it.
Terrain edits go through URTSShortsLandscapeLibrary.Export/ImportLandscapeRegion on one rectangle (REGION); the
untouched original of that rectangle is kept in Saved/IntroRear/terrain/orig_* so restore_terrain() undoes it.
"""
import math
import os

import unreal

MAP = '/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission'
ROOT = 'D:/UE5Projects/RTS_Survival/Saved/IntroRear/'
TERRAIN = ROOT + 'terrain/'
SHOTS = ROOT + 'shots/'
FOLDER = 'IntroRear/'
PREFIX = 'REAR_'
LANDSCAPE_ORIGIN = (-51600.0, -32701.0)   # vertex (0,0); 100 cm per quad, z scale 100 -> 1/128 cm per unit
QUAD = 100.0
LAYER_NAMES = ['Sand', 'DesertRockDark', 'Weedy', 'DesertGravel', 'ScorchSandyStone', 'ScorchConcrete']

for d in (ROOT, TERRAIN, SHOTS):
    os.makedirs(d, exist_ok=True)


def log(text):
    print(text)
    unreal.log_warning('[IntroRear] ' + str(text))


def actors_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def require_map():
    path = world().get_path_name()
    if not path.startswith(MAP):
        raise RuntimeError('wrong map open: ' + path)


def all_actors():
    return actors_sub().get_all_level_actors()


def landscape():
    for a in all_actors():
        if isinstance(a, unreal.Landscape) and a.get_actor_label() == 'Landscape1':
            return a
    raise RuntimeError('Landscape1 not found')


def grid_info():
    parts = unreal.RTSShortsLandscapeLibrary.get_landscape_grid_info(landscape()).split()
    return [int(p) for p in parts]


_LAYER_INFOS = None


def layer_infos():
    """LayerInfo objects of the landscape's paint layers, in LAYER_NAMES order."""
    global _LAYER_INFOS
    if _LAYER_INFOS is not None:
        return _LAYER_INFOS
    found = {}
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath('/Script/Landscape', 'LandscapeLayerInfoObject')],
                          recursive_classes=True)
    for data in reg.get_assets(flt):
        obj = data.get_asset()
        name = str(obj.get_editor_property('layer_name'))
        found.setdefault(name, []).append(obj)
    used = landscape_used_layer_infos()
    infos = []
    for name in LAYER_NAMES:
        pick = used.get(name)
        if pick is None:
            raise RuntimeError('no layer info in use for ' + name + ' candidates ' + str(found.get(name)))
        infos.append(pick)
    _LAYER_INFOS = infos
    return infos


def landscape_used_layer_infos():
    """Layer infos the landscape components actually reference (weightmap allocations)."""
    used = {}
    land = landscape()
    try:
        settings = land.get_editor_property('target_layers')
        for name, s in settings.items():
            obj = s.get_editor_property('layer_info_obj')
            if obj:
                used[str(name)] = obj
    except Exception:
        pass
    if not used:
        try:
            for s in land.get_editor_property('editor_layer_settings'):
                obj = s.get_editor_property('layer_info_obj')
                if obj:
                    used[str(obj.get_editor_property('layer_name'))] = obj
        except Exception as ex:  # noqa
            log('editor_layer_settings unreadable: %s' % ex)
    return used


# ---- region io -------------------------------------------------------------------------------------------------------
def world_to_vertex(x, y):
    return (x - LANDSCAPE_ORIGIN[0]) / QUAD, (y - LANDSCAPE_ORIGIN[1]) / QUAD


def vertex_to_world(i, j):
    return LANDSCAPE_ORIGIN[0] + i * QUAD, LANDSCAPE_ORIGIN[1] + j * QUAD


def export_region(tag, rect):
    """rect = (minx, miny, maxx, maxy) vertex indices. Writes TERRAIN/<tag>_height.r16 + <tag>_<layer>.r8."""
    hfile = TERRAIN + tag + '_height.r16'
    lfiles = [TERRAIN + '%s_%s.r8' % (tag, n) for n in LAYER_NAMES]
    ok = unreal.RTSShortsLandscapeLibrary.export_landscape_region(landscape(), rect[0], rect[1], rect[2], rect[3],
                                                                  hfile, layer_infos(), lfiles)
    log('export %s %s -> %s' % (tag, rect, ok))
    return ok


def import_region(tag, rect, heights=True):
    hfile = TERRAIN + tag + '_height.r16' if heights else ''
    lfiles = [TERRAIN + '%s_%s.r8' % (tag, n) for n in LAYER_NAMES]
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(landscape(), rect[0], rect[1], rect[2], rect[3],
                                                                  hfile, layer_infos(), lfiles)
    log('import %s %s -> %s' % (tag, rect, ok))
    return ok


# ---- heights from the region file (actors never in the way) ----------------------------------------------------------
_HEIGHTS = {}


def load_heights(tag, rect):
    import array
    with open(TERRAIN + tag + '_height.r16', 'rb') as handle:
        data = array.array('H', handle.read())
    _HEIGHTS['cur'] = (rect, data)


def ground_z(x, y):
    rect, data = _HEIGHTS['cur']
    sx = rect[2] - rect[0] + 1
    sy = rect[3] - rect[1] + 1
    fi, fj = world_to_vertex(x, y)
    fi -= rect[0]
    fj -= rect[1]
    fi = min(max(fi, 0.0), sx - 1.001)
    fj = min(max(fj, 0.0), sy - 1.001)
    i, j = int(math.floor(fi)), int(math.floor(fj))
    tx, ty = fi - i, fj - j

    def h(a, b):
        return (data[b * sx + a] - 32768.0) * 100.0 / 128.0 + 100.0   # landscape actor z = 100

    return (h(i, j) * (1 - tx) * (1 - ty) + h(i + 1, j) * tx * (1 - ty) + h(i, j + 1) * (1 - tx) * ty +
            h(i + 1, j + 1) * tx * ty)


def trace_z(x, y, top=20000.0):
    hit = unreal.SystemLibrary.line_trace_single(world(), unreal.Vector(x, y, top), unreal.Vector(x, y, -20000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    t = hit.to_tuple()
    return t[4].z, t[9]


# ---- spawning --------------------------------------------------------------------------------------------------------
def clear_prefix(prefix=PREFIX):
    n = 0
    for a in all_actors():
        if a.get_actor_label().startswith(prefix):
            actors_sub().destroy_actor(a)
            n += 1
    log('cleared %d %s actors' % (n, prefix))
    return n


def spawn_bp(path, label, loc, yaw=0.0, folder='Props', scale=None, pitch=0.0, roll=0.0):
    cls = unreal.load_class(None, path if path.endswith('_C') else path + '.' + path.split('/')[-1] + '_C')
    if cls is None:
        raise RuntimeError('missing class ' + path)
    a = actors_sub().spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(roll, pitch, yaw))
    a.set_actor_label(label)
    a.set_folder_path(FOLDER + folder)
    if scale is not None:
        a.set_actor_scale3d(unreal.Vector(*scale) if isinstance(scale, (tuple, list)) else unreal.Vector(scale, scale, scale))
    return a


def spawn_mesh(mesh_path, label, loc, yaw=0.0, folder='Props', scale=None, collision=True, pitch=0.0, roll=0.0,
               materials=None):
    mesh = unreal.load_asset(mesh_path)
    if mesh is None:
        raise RuntimeError('missing mesh ' + mesh_path)
    a = actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(roll, pitch, yaw))
    comp = a.static_mesh_component
    comp.set_static_mesh(mesh)
    a.set_actor_label(label)
    a.set_folder_path(FOLDER + folder)
    if scale is not None:
        a.set_actor_scale3d(unreal.Vector(*scale) if isinstance(scale, (tuple, list)) else unreal.Vector(scale, scale, scale))
    if not collision:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property('can_ever_affect_navigation', False)
    if materials:
        for i, m in enumerate(materials):
            if m:
                comp.set_material(i, unreal.load_asset(m))
    return a


# ---- pictures --------------------------------------------------------------------------------------------------------
def shot_top(name, centre, width, size=2048, source=None):
    """Ortho top-down capture, +X up in the image is NOT guaranteed: capture yaw 0 -> image up = +X."""
    rt = unreal.RenderingLibrary.create_render_target2d(world(), size, size, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = actors_sub().spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(centre[0], centre[1], 30000.0),
                                              unreal.Rotator(0.0, -90.0, 0.0))
    comp = cap.capture_component2d
    comp.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    comp.set_editor_property('ortho_width', width)
    comp.set_editor_property('texture_target', rt)
    comp.set_editor_property('capture_source', source or unreal.SceneCaptureSource.SCS_BASE_COLOR)
    comp.set_editor_property('capture_every_frame', False)
    comp.capture_scene()
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world(), rt, SHOTS, name + '.png')
    actors_sub().destroy_actor(cap)
    log('shot %s%s.png' % (SHOTS, name))


def nav_ready():
    w = world()
    return not unreal.NavigationSystemV1.get_navigation_system(w).is_navigation_being_built(w)


def save_level():
    require_map()
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([world().get_outermost()], False)
    log('save_packages %s' % ok)
    return ok


def view_shot(name, eye, target, res=(1600, 900)):
    """Lit editor-viewport picture (HighResShot of the level viewport). Call through a deferred tick sequence:
    returns a callable that finishes after the file exists."""
    import time
    ev, tv = unreal.Vector(*eye), unreal.Vector(*target)
    rot = unreal.MathLibrary.find_look_at_rotation(ev, tv)
    state = {'n': 0, 'path': None, 'start': None}
    shots_dir = unreal.Paths.project_saved_dir() + 'Screenshots/WindowsEditor/'

    def step():
        state['n'] += 1
        if state['n'] == 1:
            state['start'] = time.time()
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(ev, rot)
        if state['n'] == 30:
            before = set(os.listdir(shots_dir)) if os.path.isdir(shots_dir) else set()
            state['before'] = before
            unreal.AutomationLibrary.take_high_res_screenshot(res[0], res[1], SHOTS + name + '.png', None, False, False,
                                                              unreal.ComparisonTolerance.LOW)
        if state['n'] > 30 and os.path.exists(SHOTS + name + '.png'):
            return True
        if time.time() - state['start'] > 120:
            print('view_shot timeout', name)
            return True
        return False
    return step


def run_steps(steps):
    """Chain deferred callables (each returns True when done)."""
    q = list(steps)

    def tick():
        while q:
            if q[0]():
                q.pop(0)
                return False if q else True
            return False
        return True
    return tick


def import_heights(tag, rect):
    """Heights only (no paint) for a rectangle: TERRAIN/<tag>_height.r16, e.g. the full-grid edge_full file."""
    ok = unreal.RTSShortsLandscapeLibrary.import_landscape_region(landscape(), rect[0], rect[1], rect[2], rect[3],
                                                                  TERRAIN + tag + '_height.r16', [], [])
    log('import heights %s %s -> %s' % (tag, rect, ok))
    return ok
