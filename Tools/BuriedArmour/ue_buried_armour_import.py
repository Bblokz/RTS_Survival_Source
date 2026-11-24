"""Buried 1945 armour: imports the five sand mound meshes, makes MI_BuriedArmour_Sand, checks them, and takes in-game
shots of each vehicle assembled on Ger_2ndOreMountains (spawned only for the shots; the map is never saved).

Throwaway full editor (never the shared one):
  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains
    -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BuriedArmour/ue_buried_armour_import.py"
    -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Set BURIED_NO_SHOTS=1 to import only. Report: Saved/BuriedArmour/import_report.json, shots in Saved/BuriedArmour/shots.

Sand meshes are in actor space (root on the ground, nose +X). The vehicle parts go under a 'Pose' scene component
whose relative transform is BuriedArmour_poses.json -> pose_component (pitch/roll/sink of style C3); each part keeps
its original relative transform from the vehicle Blueprint (BuriedArmour_vehicle_dump.json) under it.
"""
import json
import math
import os
import sys
import time

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/BuriedArmour/'
FBX = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BuriedArmour/Export/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/BuriedArmour'
MESHES = ROOT + '/Meshes'
MATS = ROOT + '/Materials'
PARENT = '/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Materials/M_FieldTrench_Bank'
MI_NAME = 'MI_BuriedArmour_Sand'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/BuriedArmour/'
VEHICLES = ['StuGIII', 'Wespe', 'FlakLuchs', 'PzIV_F1_Assault', 'Sdkfz251_MortarTW']
SKIP = ('SK_PostApoSoldier', 'SM_GerPit_Shell')
NO_SHOTS = os.environ.get('BURIED_NO_SHOTS') == '1'
REPORT = {'log': []}
mel = unreal.MaterialEditingLibrary
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
os.makedirs(OUT + 'shots', exist_ok=True)


def log(m):
    REPORT['log'].append(m)
    unreal.log('BURIEDARMOUR ' + m)


def save_report():
    json.dump(REPORT, open(OUT + 'import_report.json', 'w'), indent=1)


def options():
    o = unreal.FbxImportUI()
    o.automated_import_should_detect_type = False
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.import_mesh = True
    o.import_as_skeletal = False
    o.import_materials = False
    o.import_textures = False
    d = o.static_mesh_import_data
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', False)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    return o


def make_mi():
    path = MATS + '/' + MI_NAME
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(MI_NAME, MATS, unreal.MaterialInstanceConstant,
                                                                unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    mel.set_material_instance_parent(mi, unreal.load_asset(PARENT))
    mel.clear_all_material_instance_parameters(mi)  # only the overrides below, nothing left from earlier tries
    # Drift sand = the ground colour under the mound (RVT) brightened toward a pale sand up the slopes (vertex R).
    # User 2026-11-18 tried a paler sand (FreshTint 1.40/1.28/1.10 + FreshSoilColor 0.62/0.48/0.30 at 0.55) and went
    # back to this: "blends better with the landscape". FreshSoilAmount 0 = FreshSoilColor unused.
    vec = {'FreshTint': (1.32, 1.18, 0.98), 'FallbackColor': (0.42, 0.32, 0.20)}
    sca = {'FreshStrength': 1.0, 'FreshSoilAmount': 0.0, 'Roughness': 0.93, 'FreshRoughness': 0.9,
           'DetailNormalStrength': 0.6}
    for k, v in vec.items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    for k, v in sca.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    log('MI ready ' + path)
    return mi


def import_mesh(veh, mi):
    name = 'SM_BuriedSand_' + veh
    path = MESHES + '/' + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)  # reimport keeps old settings: delete, then import
    task = unreal.AssetImportTask()
    task.filename = FBX + name + '.fbx'
    task.destination_path = MESHES
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = options()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    log(veh + ' imported paths ' + str(list(task.get_editor_property('imported_object_paths'))))
    mesh = unreal.load_asset(path)
    if not mesh:
        log(veh + ': IMPORT FAILED')
        return None
    sms.set_nanite_settings(mesh, unreal.MeshNaniteSettings(enabled=False), apply_changes=True)
    mesh.set_material(0, mi)
    sms.remove_collisions(mesh)
    body = mesh.get_editor_property('body_setup')
    if body:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
    # checks: orientation (highest sand must be at the rear right = UE -X / +Y), normals, vertex colours, bounds
    sec = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, 0)
    verts, _, _, _, _ = sec[0], sec[1], sec[2], sec[3], sec[4]
    top = max(verts, key=lambda v: v.z)
    bb = mesh.get_bounding_box()
    bs = sms.get_lod_build_settings(mesh, 0)
    info = dict(path=path, verts=len(verts), tris=len(sec[1]) // 3,
                top_vertex=[top.x, top.y, top.z],
                bounds_min=[bb.min.x, bb.min.y, bb.min.z], bounds_max=[bb.max.x, bb.max.y, bb.max.z],
                has_vertex_colors=bool(sms.has_vertex_colors(mesh)),
                recompute_normals=bool(bs.recompute_normals), nanite=bool(mesh.get_editor_property(
                    'nanite_settings').enabled))
    log(veh + ' ' + json.dumps(info))
    REPORT.setdefault('meshes', {})[veh] = info
    return mesh


def quat_rot(q):
    return unreal.Quat(q[0], q[1], q[2], q[3]).rotator()


def xf(d):
    return unreal.Transform(unreal.Vector(*d['loc']), quat_rot(d['quat']), unreal.Vector(*d['scale']))


def require_ore():
    path = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name()
    if 'Ger_2ndOreMountains' not in path:
        raise RuntimeError('wrong map open: ' + path)


def flat_spots(n):
    sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMap')
    import ore_map_build as omb
    omb.load_layout()
    cand = []
    for ix in range(-12, 13):
        for iy in range(-12, 13):
            x, y = ix * 2200.0, iy * 2200.0
            ring = [omb.ground_z(x + 1200 * math.cos(a), y + 1200 * math.sin(a)) for a in
                    [k * math.pi / 4 for k in range(8)]] + [omb.ground_z(x, y)]
            cand.append((max(ring) - min(ring), x, y, omb.ground_z(x, y)))
    # natural ground (not the levelled base pads / road beds, which read 0 cm and carry buildings)
    cand = sorted(c for c in cand if 4.0 <= c[0] <= 60.0)
    picked = []
    for c in cand:
        if all(math.hypot(c[1] - p[1], c[2] - p[2]) > 7000 for p in picked):
            picked.append(c)
        if len(picked) == n:
            break
    return picked


def assemble(veh, mesh, spot, yaw):
    dump = json.load(open(HERE + 'BuriedArmour_vehicle_dump.json'))['vehicles'][veh]['components']
    pose = json.load(open(HERE + 'BuriedArmour_poses.json'))[veh]['pose_component']
    actor_t = unreal.Transform(unreal.Vector(spot[1], spot[2], spot[3]), unreal.Rotator(0, 0, yaw),
                               unreal.Vector(1, 1, 1))
    pose_t = unreal.Transform(unreal.Vector(*pose['location_cm']), quat_rot(pose['quat_xyzw']), unreal.Vector(1, 1, 1))
    spawned = []
    sand = eas.spawn_actor_from_class(unreal.StaticMeshActor, actor_t.translation, actor_t.rotation.rotator())
    sand.static_mesh_component.set_static_mesh(mesh)
    sand.set_actor_label('BURIED_TEST_Sand_' + veh)
    spawned.append(sand)
    for r in dump:
        name = r.get('mesh', '').split('/')[-1]
        if not name or not r.get('visible', True) or any(s in name for s in SKIP) or 'Tracks' in name \
                or 'Wheels' in name:
            continue
        world = xf(r['xf']) * pose_t * actor_t
        asset = unreal.load_asset(r['mesh'])
        if isinstance(asset, unreal.StaticMesh):
            a = eas.spawn_actor_from_class(unreal.StaticMeshActor, world.translation, world.rotation.rotator())
            a.static_mesh_component.set_static_mesh(asset)
            for i, m in enumerate(r.get('mats') or []):
                if m:
                    a.static_mesh_component.set_material(i, unreal.load_asset(m))
        else:
            a = eas.spawn_actor_from_class(unreal.SkeletalMeshActor, world.translation, world.rotation.rotator())
            a.skeletal_mesh_component.set_skeletal_mesh_asset(asset)
            for i, m in enumerate(r.get('mats') or []):
                if m:
                    a.skeletal_mesh_component.set_material(i, unreal.load_asset(m))
        a.set_actor_scale3d(world.scale3d)
        a.set_actor_label('BURIED_TEST_' + veh + '_' + name)
        spawned.append(a)
    return spawned


S = {'stage': 'wait', 't': time.time(), 'i': 0, 'moved': False, 'shot': False, 'mark': 0.0}


def run_import():
    if os.environ.get('BURIED_SKIP_IMPORT') == '1':  # shots only, meshes already imported
        meshes = {v: unreal.load_asset(MESHES + '/SM_BuriedSand_' + v) for v in VEHICLES}
        return {k: m for k, m in meshes.items() if m}
    mi = make_mi()
    meshes = {}
    for veh in VEHICLES:
        m = import_mesh(veh, mi)
        if m:
            meshes[veh] = m
    pose = json.load(open(HERE + 'BuriedArmour_poses.json'))['StuGIII']['pose_component']
    r = quat_rot(pose['quat_xyzw'])
    REPORT['pose_rotator_ue'] = [r.roll, r.pitch, r.yaw]
    log('pose rotator (roll, pitch, yaw) %.2f %.2f %.2f (want right side down + nose up)' % (r.roll, r.pitch, r.yaw))
    save_report()
    return meshes


def tick(dt):
    try:
        _tick()
    except Exception:  # noqa: BLE001
        import traceback
        log('ERROR ' + traceback.format_exc())
        save_report()
        unreal.SystemLibrary.quit_editor()
        S['stage'] = 'done'


def _tick():
    if S['stage'] in ('done', 'busy'):
        return
    if S['stage'] == 'wait':
        if time.time() - S['t'] < 15:
            return
        # import_asset_tasks pumps Slate ticks (Interchange): without this guard the tick re-enters run_import
        S['stage'] = 'busy'
        S['meshes'] = run_import()
        if NO_SHOTS:
            S['stage'] = 'done'
            unreal.SystemLibrary.quit_editor()
            return
        require_ore()
        spots = flat_spots(len(S['meshes']))
        S['spots'] = []
        for (veh, mesh), spot in zip(S['meshes'].items(), spots):
            assemble(veh, mesh, spot, 35.0)
            S['spots'].append((veh, spot[1], spot[2], spot[3]))
            log('placed %s at %.0f %.0f %.0f (ground range %.0f cm)' % (veh, spot[1], spot[2], spot[3], spot[0]))
        REPORT['spots'] = S['spots']
        save_report()
        unreal.SystemLibrary.execute_console_command(None, 't.IdleWhenNotForeground 0')
        S['stage'], S['t'] = 'compile', time.time()
        return
    if S['stage'] == 'compile':
        # let shaders for the new meshes compile before PIE
        if time.time() - S['t'] < 40:
            return
        les.editor_request_begin_play()
        S['stage'], S['t'] = 'pie', time.time()
        return
    gw = ues.get_game_world()
    if S['stage'] == 'pie':
        if gw is None:
            if time.time() - S['t'] > 240:
                log('PIE never started')
                S['stage'] = 'quit'
            return
        if unreal.GameplayStatics.is_game_paused(gw):
            unreal.GameplayStatics.set_game_paused(gw, False)
        if unreal.GameplayStatics.get_time_seconds(gw) > 10.0:
            unreal.SystemLibrary.execute_console_command(gw, 'ShowFlag.PostProcessMaterial 0')
            S['stage'] = 'shots'
        return
    if S['stage'] == 'shots':
        if gw is None:
            S['stage'] = 'quit'
            return
        if unreal.GameplayStatics.is_game_paused(gw):
            unreal.GameplayStatics.set_game_paused(gw, False)
        gt = unreal.GameplayStatics.get_time_seconds(gw)
        if S['i'] >= len(S['spots']):
            les.editor_request_end_play()
            S['stage'], S['t'] = 'quit', time.time()
            return
        veh, x, y, z = S['spots'][S['i']]
        if not S['moved']:
            pawn = unreal.GameplayStatics.get_player_pawn(gw, 0)
            pawn.set_actor_location(unreal.Vector(x, y, z + 43.0), False, False)
            pawn.set_actor_rotation(unreal.Rotator(0.0, 0.0, 0.0), False)
            S['moved'], S['mark'] = True, gt
            return
        if not S['shot'] and gt - S['mark'] > 4.0:
            unreal.SystemLibrary.execute_console_command(
                gw, 'HighResShot 1600x900 filename=%sshots/%s.png' % (OUT, veh))
            S['shot'], S['mark'] = True, gt
            return
        if S['shot'] and gt - S['mark'] > 2.0:
            log('shot ' + veh)
            S['i'] += 1
            S['moved'], S['shot'] = False, False
        return
    if S['stage'] == 'quit':
        if time.time() - S['t'] < 3:
            return
        save_report()
        S['stage'] = 'done'
        unreal.SystemLibrary.quit_editor()


S['handle'] = unreal.register_slate_post_tick_callback(tick)
