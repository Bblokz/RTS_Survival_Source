"""Review shots of the two clue Blueprints in a throwaway editor (nothing is saved).

  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Builds two context scenes 200 m above the test map: (A) the Berlin loading ramp + three track panels with
BP_OW_Clue_QuarantineWagon at the ramp origin (exactly how it goes on the map); (B) the real Mittelsee shore patch
(SM_OW_ClueReview_Shore, imported unsaved) with a water sheet on MI_OderWarthe_AlgaeWater at the waterline and
BP_OW_Clue_SamplingCamp at its origin. Shots: RTS camera (pitch 55) + close views -> Saved/OderWarthe/clues/review/.
"""
import json
import math
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OderWarthe/clues/review/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Clues'
RAMP = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RailYardTarget/SM_Berlin_RailRamp'
TRACK = '/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01'
ALGAE_WATER = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Map/MI_OderWarthe_AlgaeWater'
LAKE_WATER = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Map/MI_OderWarthe_LakeWater'
EARTH = '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Materials/MI_OW_Earth'
SHORE_FBX = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Clues/FBX/SM_OW_ClueReview_Shore.fbx'
Z0 = 20000.0
A = unreal.Vector(0.0, 0.0, Z0)
B = unreal.Vector(8000.0, 0.0, Z0)
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'shots': [], 'busy': False}
os.makedirs(OUT, exist_ok=True)


def log(m):
    S['log'].append(m)
    unreal.log_warning('OWREVIEW ' + m)


def finish():
    json.dump(S['log'], open(OUT + 'review_log.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def mesh_actor(path, loc, rot=(0, 0, 0), scale=(1, 1, 1), mat=None):
    a = actors().spawn_actor_from_class(unreal.StaticMeshActor, loc, unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
    c = a.static_mesh_component
    c.set_static_mesh(unreal.load_asset(path))
    a.set_actor_scale3d(unreal.Vector(*scale))
    if mat:
        for i in range(c.get_num_materials()):
            c.set_material(i, unreal.load_asset(mat))
    return a


def import_shore():
    task = unreal.AssetImportTask()
    task.filename = SHORE_FBX
    task.destination_path = HOME + '/_Review'
    task.destination_name = 'SM_OW_ClueReview_Shore'
    task.automated, task.save, task.replace_existing = True, False, True
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    ui.get_editor_property('static_mesh_import_data').set_editor_property('build_nanite', False)
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    return HOME + '/_Review/SM_OW_ClueReview_Shore'


def build():
    w = world()
    lights = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.DirectionalLight)
    log('map lights: directional %d, sky %d' % (len(lights), len(unreal.GameplayStatics.get_all_actors_of_class(
        w, unreal.SkyLight))))
    if not lights:
        sun = actors().spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, Z0 + 5000),
                                              unreal.Rotator(roll=0, pitch=-48, yaw=-35))
        sun.light_component.set_editor_property('intensity', 6.0)
        actors().spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, Z0 + 5000))
        actors().spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    # (A) ramp + track + ground, wagon BP at the ramp origin
    mesh_actor('/Engine/BasicShapes/Plane', unreal.Vector(0, 0, Z0 - 1), scale=(120, 120, 1), mat=EARTH)
    mesh_actor(RAMP, A)
    for k in range(3):
        mesh_actor(TRACK, unreal.Vector(-260.0, 775.0 - k * 1000.0, Z0 - 22.0), (0, 0, -90.0))
    wbp = unreal.EditorAssetLibrary.load_blueprint_class(HOME + '/BP_OW_Clue_QuarantineWagon')
    wa = actors().spawn_actor_from_class(wbp, A, unreal.Rotator(0, 0, 0))
    # (B) shore + water + camp BP
    shore = import_shore()
    mesh_actor(shore, B, mat=EARTH)
    mesh_actor('/Engine/BasicShapes/Plane', unreal.Vector(B.x, B.y + 1500, Z0), scale=(60, 60, 1), mat=ALGAE_WATER)
    cbp = unreal.EditorAssetLibrary.load_blueprint_class(HOME + '/BP_OW_Clue_SamplingCamp')
    ca = actors().spawn_actor_from_class(cbp, B, unreal.Rotator(0, 0, 0))
    for a in (wa, ca):
        for c in a.get_components_by_class(unreal.ChildActorComponent):
            ch = c.get_editor_property('child_actor')
            info = {'child': ch.get_class().get_name() if ch else None}
            if ch:
                for p in ('TriggerRadius', 'PortraitType'):
                    try:
                        info[p] = str(ch.get_editor_property(p))
                    except Exception as e:  # noqa: BLE001
                        info[p] = 'ERR %s' % e
                info['loc'] = str(ch.get_actor_location())
            log('%s trigger %s' % (a.get_class().get_name(), json.dumps(info)))
        log('%s components %d' % (a.get_class().get_name(), len(a.get_components_by_class(unreal.ActorComponent))))


def view(name, target, d, dist, pitch, fov, res=(1600, 900)):
    t = unreal.Vector(*target)
    dv = unreal.Vector(d[0], d[1], 0.0)
    dv = dv * (1.0 / math.sqrt(d[0] ** 2 + d[1] ** 2))
    p = math.radians(pitch)
    eye = t - dv * (dist * math.cos(p)) + unreal.Vector(0, 0, dist * math.sin(p))
    return (name, eye, t, fov, res)


def plan():
    a, b = (A.x, A.y, A.z), (B.x, B.y, B.z)
    return [
        view('wagon_rts', (a[0] - 150, a[1] + 250, a[2] + 200), (-0.951, 0.309), 4880, 55, 50),
        view('wagon_close', (a[0] - 120, a[1] + 200, a[2] + 230), (-0.75, -0.66), 2100, 24, 40),
        view('wagon_stencils', (a[0] - 105, a[1] + 251, a[2] + 300), (-1.0, 0.05), 1300, 12, 40),
        view('camp_rts', (b[0], b[1] - 300, b[2] + 100), (1.0, 0.0), 4880, 55, 50),
        view('camp_close', (b[0] + 100, b[1] - 650, b[2] + 150), (0.8, 0.6), 1600, 30, 40),
        ('camp_jars_vs_water', unreal.Vector(b[0] + 170, b[1] - 1180, b[2] + 330),
         unreal.Vector(b[0] + 150, b[1] + 600, b[2] + 0), 50, (1600, 900)),
        view('camp_rts_shore', (b[0] + 300, b[1] + 300, b[2] + 50), (1.0, 0.0), 3000, 55, 50),
    ]


def tick(dt):
    if S['busy']:
        return
    S['t'] += dt
    ph = S['phase']
    try:
        if ph == 'wait_load' and S['t'] > 20.0:
            S['busy'] = True
            try:
                build()
            finally:
                S['busy'] = False
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_game_view(True)
            S['views'] = plan()
            S['phase'], S['mark'] = 'warm', S['t']
        elif ph == 'warm' and S['t'] - S['mark'] > 60.0:
            S['phase'] = 'aim'
        elif ph == 'aim':
            if not S['views']:
                S['phase'], S['mark'] = 'tail', S['t']
                return
            name, eye, tgt, fov, res = S['views'][0]
            cam = actors().spawn_actor_from_class(unreal.CameraActor, eye,
                                                  unreal.MathLibrary.find_look_at_rotation(eye, tgt))
            cam.camera_component.set_editor_property('field_of_view', fov)
            S['cam'] = cam
            unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
                eye, unreal.MathLibrary.find_look_at_rotation(eye, tgt))
            S['phase'], S['mark'] = 'shoot', S['t']
        elif ph == 'shoot' and S['t'] - S['mark'] > 6.0:
            name, eye, tgt, fov, res = S['views'].pop(0)
            fn = OUT + name + '.png'
            S['busy'] = True
            try:
                unreal.AutomationLibrary.take_high_res_screenshot(res[0], res[1], fn, S['cam'])
            finally:
                S['busy'] = False
            log('shot %s -> %s' % (name, fn))
            S['phase'], S['mark'] = 'after', S['t']
        elif ph == 'after' and S['t'] - S['mark'] > 10.0:
            S['phase'] = 'aim'
        elif ph == 'tail' and S['t'] - S['mark'] > 15.0:
            finish()
        elif S['t'] > 900.0:
            log('timeout in ' + ph)
            finish()
    except Exception as e:  # noqa: BLE001
        log('ERROR %s: %r' % (ph, e))
        finish()


def guarded(dt):
    if S.get('in_tick'):
        return
    S['in_tick'] = True
    try:
        tick(dt)
    finally:
        S['in_tick'] = False


unreal.register_slate_post_tick_callback(guarded)
log('script loaded')
