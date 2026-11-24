"""Sanded Fachwerk ruins: imports the Blender exports, builds one Blueprint per ruin, takes proof shots on
Ger_IntroMission (actors spawned only for the shots; the map is never saved).

Blender side: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/SandedHouses/Model (sh_lib.py + ruins/).
Per ruin it reads Export/SandedHouse_<Name>.json, SM_SandedHouse_<Name>.fbx and SM_SandedHouse_<Name>_Sand.fbx.

Throwaway full editor (never the shared one):
  set MSYS_NO_PATHCONV=1 (Git Bash), then
  UnrealEditor.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
    -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/SandedHouses/ue_sanded_houses.py"
    -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Env: SH_ONLY=Wehe,Gerippe (subset), SH_NO_SHOTS=1 (import + BPs only), SH_SKIP_IMPORT=1 (shots only).
Report: Saved/SandedHouses/report.json, shots in Saved/SandedHouses/shots.

Content: /Game/RTS_Survival/Blueprints/Environment/Desert/SandedHouses
  Meshes/SM_SandedHouse_<Name> (slot 0 = MI_FarmBuildings, complex-as-simple collision)
  Meshes/SM_SandedHouse_<Name>_Sand (MI_SandedHouse_Sand, complex-as-simple collision)
  Materials/MI_SandedHouse_Sand = child of M_FieldTrench_Bank: base colour from RVT_Environment, so the sand takes
    the ground colour of whatever map it stands on; vertex R = paler drift up the mound, G = foot blend.
  BP_SandedHouse_<Name> (Actor): House + Sand mesh components, Plant_NN (existing cactus / thistle meshes, no
    collision), NavBlock_NN (dynamic nav obstacles, NavArea_Null, no collision) from the JSON blockers.
Blender -> UE: x*100, -y*100, z*100; yaw -> -yaw (FBX import flips Y).
"""
import json
import math
import os
import time

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/SandedHouses/Export/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Desert/SandedHouses'
MESHES = ROOT + '/Meshes'
MATS = ROOT + '/Materials'
BANK = '/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/Materials/M_FieldTrench_Bank'
FARM_MI = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Farm/Materials/MI_FarmBuildings'
SAND_MI = 'MI_SandedHouse_Sand'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/SandedHouses/'
ORDER = ['Wehe', 'Sattelbruch', 'Gerippe', 'Schieflage', 'Mauergarten', 'Kakteenhaus']
ONLY = [s for s in os.environ.get('SH_ONLY', '').split(',') if s]
NO_SHOTS = os.environ.get('SH_NO_SHOTS') == '1'
SKIP_IMPORT = os.environ.get('SH_SKIP_IMPORT') == '1'
PLACED = os.environ.get('SH_PLACED') == '1'  # with SH_SKIP_IMPORT: shoot the ruins placed in the map

CACTI = '/Game/RTS_Survival/Meshes/Desert/Cacti/'
BUSH = CACTI + 'CactusBush/SM_RTS_CactusBush_'
THISTLE = '/Game/RTS_Survival/Blueprints/Environment/CrushableInstancedActor/DryScrub/Meshes/SM_DeadThistle_'
# kind -> (candidate meshes, target height in metres at scale 1)
PLANTS = {
    'saguaro': ([CACTI + 'SM_Desert_Cactus%d' % i for i in (1, 2, 3, 4)], 4.2),
    'prickly_pear': ([BUSH + 'Medium_Green_0%d' % i for i in (1, 2, 3)], 1.1),
    'cactus_bush_s': ([BUSH + 'Small_Green_0%d' % i for i in (1, 2, 3)], 0.7),
    'cactus_bush_m': ([BUSH + 'Medium_Green_0%d' % i for i in (1, 2, 3)], 1.1),
    'cactus_bush_l': ([BUSH + 'Large_Green_0%d' % i for i in (1, 2, 3)], 1.6),
    'agave': ([BUSH + 'Small_Blue_0%d' % i for i in (1, 2, 3)], 0.9),
    'thistle': ([THISTLE + c for c in 'ABC'], 0.6),
}

REPORT = {'log': [], 'ruins': {}}
EAL = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
os.makedirs(OUT + 'shots', exist_ok=True)


def log(m):
    REPORT['log'].append(m)
    unreal.log_warning('SANDEDHOUSES ' + m)


def save_report():
    json.dump(REPORT, open(OUT + 'report.json', 'w'), indent=1)


def ruins():
    names = [n for n in ORDER if os.path.exists(SRC + 'SandedHouse_%s.json' % n)]
    return [n for n in names if not ONLY or n in ONLY]


# ---------------------------------------------------------------- assets
def make_sand_mi():
    path = MATS + '/' + SAND_MI
    if not EAL.does_asset_exist(path):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(SAND_MI, MATS, unreal.MaterialInstanceConstant,
                                                                unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    mel.set_material_instance_parent(mi, unreal.load_asset(BANK))
    mel.clear_all_material_instance_parameters(mi)
    # Same look as MI_BuriedArmour_Sand (user 2026-11-18: keep the drift close to the ground colour).
    vec = {'FreshTint': (1.32, 1.18, 0.98), 'FallbackColor': (0.42, 0.32, 0.20)}
    sca = {'FreshStrength': 1.0, 'FreshSoilAmount': 0.0, 'Roughness': 0.93, 'FreshRoughness': 0.9,
           'DetailNormalStrength': 0.6}
    for k, v in vec.items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    for k, v in sca.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    mel.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    return mi


def fbx_options():
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


def import_mesh(name, material):
    path = MESHES + '/' + name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)  # a reimport keeps old settings: delete, then import
    task = unreal.AssetImportTask()
    task.filename = SRC + name + '.fbx'
    task.destination_path = MESHES
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = fbx_options()
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if not mesh:
        raise RuntimeError('import failed ' + name)
    sms.set_nanite_settings(mesh, unreal.MeshNaniteSettings(enabled=False), apply_changes=True)
    mesh.set_material(0, material)
    sms.remove_collisions(mesh)
    body = mesh.get_editor_property('body_setup')
    if body:
        body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(mesh, False)
    bb = mesh.get_bounding_box()
    return mesh, [[round(bb.min.x), round(bb.min.y), round(bb.min.z)], [round(bb.max.x), round(bb.max.y),
                                                                          round(bb.max.z)]]


def plant_meshes():
    info = {}
    for kind, (paths, target) in PLANTS.items():
        rows = []
        for p in paths:
            m = unreal.load_asset(p)
            if not m:
                continue
            bb = m.get_bounding_box()
            rows.append((m, bb.max.z - bb.min.z, max(bb.max.x - bb.min.x, bb.max.y - bb.min.y), bb.min.z))
        info[kind] = (rows, target)
        REPORT.setdefault('plant_meshes', {})[kind] = [[r[0].get_name(), round(r[1]), round(r[2])] for r in rows]
    return info


# ---------------------------------------------------------------- blueprint
def _named(bp):
    out = {}
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDL.get_data(handle)
        template = SDL.get_object(data)
        if template is None or isinstance(template, unreal.Actor):
            out.setdefault('__actor__', (handle, template))
            continue
        out[str(SDL.get_variable_name(data))] = (handle, template)
        if isinstance(template, unreal.SceneComponent) and SDL.is_root_component(data):
            out['__root__'] = (handle, template)
    return out


def _add(bp, parent, cls, name):
    params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError('%s: adding %s failed: %s' % (bp.get_name(), name, fail))
    SDS.rename_subobject(handle, unreal.Text(name))
    return handle


def shell_collision(comp):
    comp.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    comp.set_collision_object_type(unreal.CollisionChannel.ECC_WORLD_STATIC)
    comp.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    ch = unreal.CollisionChannel
    comp.set_collision_response_to_channel(ch.ECC_BUILDING_PLACEMENT, unreal.CollisionResponseType.ECR_OVERLAP)
    for c in (ch.ECC_VISIBILITY, ch.ECC_PROJECTILE, ch.ECC_ENEMY, ch.ECC_PLAYER):  # not Camera: RTS cam pops
        comp.set_collision_response_to_channel(c, unreal.CollisionResponseType.ECR_BLOCK)
    comp.set_editor_property('can_ever_affect_navigation', False)


def to_ue(p):
    return unreal.Vector(p[0] * 100.0, -p[1] * 100.0, p[2] * 100.0)


def build_bp(name, house, sand, meta, plants):
    bp_name = 'BP_SandedHouse_' + name
    path = ROOT + '/' + bp_name
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)  # not placed anywhere yet: rebuild from scratch
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.Actor)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(bp_name, ROOT, unreal.Blueprint, factory)
    named = _named(bp)
    if '__root__' not in named:
        _add(bp, named['__actor__'][0], unreal.SceneComponent, 'DefaultSceneRoot')
        named = _named(bp)
    root = named['__root__'][0]
    _add(bp, root, unreal.StaticMeshComponent, 'House')
    if sand:
        _add(bp, root, unreal.StaticMeshComponent, 'Sand')
    for i in range(len(meta['plants'])):
        _add(bp, root, unreal.StaticMeshComponent, 'Plant_%02d' % i)
    for i in range(len(meta['blockers'])):
        _add(bp, root, unreal.BoxComponent, 'NavBlock_%02d' % i)
    named = _named(bp)
    named['__root__'][1].set_editor_property('mobility', unreal.ComponentMobility.STATIC)
    h = named['House'][1]
    h.set_editor_property('static_mesh', house)
    shell_collision(h)
    if sand:
        s = named['Sand'][1]
        s.set_editor_property('static_mesh', sand)
        shell_collision(s)
        s.set_editor_property('receives_decals', True)
    plant_rows = []
    for i, pl in enumerate(meta['plants']):
        rows, target = plants[pl['kind']]
        mesh, mh, mw, mz = rows[(i * 7 + len(name)) % len(rows)]
        sc = target * pl['scale'] * 100.0 / max(mh, 1.0)
        sxy = sc
        if pl['kind'] == 'saguaro':
            # SM_Desert_Cactus* are slim columns ~1-2 m tall: scaled uniformly to 4-9 m they turn into fat pickles
            # that overfill the roof slots. Width grows slower than height: 0.5 m at scale 1, ~0.85 m at scale 2.15.
            sxy = min(sc, 50.0 * pl['scale'] ** 0.7 / max(mw, 1.0))
        c = named['Plant_%02d' % i][1]
        c.set_editor_property('static_mesh', mesh)
        c.set_editor_property('relative_location', to_ue(pl['loc']) - unreal.Vector(0, 0, mz * sc))
        # Blender euler (X=tilt[1], Y=tilt[0], Z=yaw) mirrored in Y = R_x(-a) R_y(b) R_z(-yaw) in UE numbers; UE's
        # FRotationMatrix uses R_x(-roll) and R_y(-pitch), so roll = +tilt[1], pitch = -tilt[0], yaw = -yaw.
        c.set_editor_property('relative_rotation', unreal.Rotator(roll=pl['tilt'][1], pitch=-pl['tilt'][0],
                                                                  yaw=-pl['yaw']))
        c.set_editor_property('relative_scale3d', unreal.Vector(sxy, sxy, sc))
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property('can_ever_affect_navigation', False)
        plant_rows.append([pl['kind'], mesh.get_name(), round(sc, 3), round(mw * sxy), round(mh * sc)])
    for i, b in enumerate(meta['blockers']):
        c = named['NavBlock_%02d' % i][1]
        lo, hi = b['min'], b['max']
        centre = to_ue([(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2])
        c.set_editor_property('relative_location', centre)
        c.set_editor_property('relative_rotation', unreal.Rotator(yaw=-b.get('yaw', 0.0)))
        c.set_editor_property('box_extent', unreal.Vector((hi[0] - lo[0]) * 50.0, (hi[1] - lo[1]) * 50.0,
                                                          (hi[2] - lo[2]) * 50.0))
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property('generate_overlap_events', False)
        c.set_editor_property('can_ever_affect_navigation', bool(b.get('nav', True)))
        c.set_editor_property('dynamic_obstacle', True)
        c.set_editor_property('area_class_override', unreal.NavArea_Null)
        c.set_editor_property('use_system_default_obstacle_area_class', False)
        c.set_editor_property('visible', False)
        c.set_editor_property('hidden_in_game', True)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, False)
    return bp, plant_rows


def run_import():
    mi_sand = make_sand_mi()
    farm = unreal.load_asset(FARM_MI)
    plants = plant_meshes()
    out = {}
    for name in ruins():
        meta = json.load(open(SRC + 'SandedHouse_%s.json' % name))
        house, hb = import_mesh(meta['house_mesh'], farm)
        sand, sb = (import_mesh(meta['sand_mesh'], mi_sand) if meta.get('sand_mesh') else (None, None))
        bp, prow = build_bp(name, house, sand, meta, plants)
        blb = meta['stats'].get('house_bounds')
        info = {'bp': bp.get_path_name(), 'house_bounds_cm': hb, 'sand_bounds_cm': sb, 'plants': prow,
                'blockers': len(meta['blockers']), 'stats': meta['stats'],
                # axis check: UE min y must equal -(Blender max y) * 100
                'y_flip_ok': bool(blb and abs(hb[0][1] + blb[1][1] * 100) < 2 and abs(hb[1][1] + blb[0][1] * 100) < 2)}
        REPORT['ruins'][name] = info
        log(name + ' ' + json.dumps(info))
        out[name] = bp
    save_report()
    return out


# ---------------------------------------------------------------- proof shots on the open map
def ground(world, x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 50000.0), unreal.Vector(x, y, -50000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def flat_spot(world, origin, taken):
    for ring in range(0, 30):
        for k in range(max(1, ring * 8)):
            a = 2.0 * math.pi * k / max(1, ring * 8)
            cx, cy = origin.x + math.cos(a) * ring * 3000.0, origin.y + math.sin(a) * ring * 3000.0
            if any(math.hypot(cx - t.x, cy - t.y) < 5000.0 for t in taken):
                continue
            zs = [ground(world, cx + dx, cy + dy) for dx in (-1800, -900, 0, 900, 1800)
                  for dy in (-1500, -750, 0, 750, 1500)]
            if None in zs:
                continue
            if max(zs) - min(zs) < 120.0:
                return unreal.Vector(cx, cy, sorted(zs)[len(zs) // 2])
    return None


def setup_shots(bps):
    world = ues.get_editor_world()
    REPORT['map'] = world.get_path_name()
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    farm = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if 'FarmHouse' in a.get_class().get_name()]
    origin = farm[0].get_actor_location() if farm else unreal.Vector(0, 0, 0)
    log('origin %s (%d farm houses)' % (origin, len(farm)))
    taken = [a.get_actor_location() for a in farm]
    shots = []
    placed = {}
    if PLACED:  # shoot the ruins already saved in the map (labels SandedHouse_<Name>), spawn nothing
        for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
            n = a.get_actor_label()[len('SandedHouse_'):]
            if a.get_actor_label().startswith('SandedHouse_') and (not ONLY or n in ONLY):
                placed[n] = a.get_actor_location()
        log('placed in map: ' + ', '.join(placed))
    for name, bp in (placed.items() if PLACED else bps.items()):
        if PLACED:
            spot = bp
        else:
            spot = flat_spot(world, origin, taken)
            if spot is None:
                log('no flat spot for ' + name)
                continue
            taken.append(spot)
            actor = eas.spawn_actor_from_class(bp.generated_class(), spot, unreal.Rotator(0, 0, 0))
            actor.set_actor_label('SANDED_TEST_' + name)
            log('placed %s at %s' % (name, spot))
        # editor viewport FOV is 90 deg: closer than the Blender previews (34 deg) for the same framing
        for tag, yaw, pitch, dist in (('game', 112.0, 50.0, 24.0), ('back', -60.0, 38.0, 22.0),
                                      ('rts', 100.0, 62.0, 40.0), ('front', 90.0, 22.0, 17.0)):
            p, y = math.radians(pitch), math.radians(yaw)
            off = [-math.cos(p) * math.cos(y) * dist, -math.cos(p) * math.sin(y) * dist, math.sin(p) * dist]
            target = spot + unreal.Vector(0, 0, 200.0 if tag == 'front' else 300.0)
            eye = target + to_ue(off)
            shots.append(('%s_%s' % (name, tag), eye, target))
    return shots


ST = {'frames': 0, 'phase': 'wait', 'shots': [], 'busy': False, 't': time.time()}


def aim(eye, look):
    ues.set_level_viewport_camera_info(eye, unreal.MathLibrary.find_look_at_rotation(eye, look))


def tick(dt):
    if ST['busy']:
        return  # imports and screenshots pump Slate: ignore nested calls
    ST['frames'] += 1
    ST['busy'] = True
    try:
        step()
    except Exception:  # noqa: BLE001
        import traceback
        log('ERROR ' + traceback.format_exc())
        save_report()
        ST['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()
    finally:
        ST['busy'] = False


def step():
    ph = ST['phase']
    if ph == 'wait':
        if ST['frames'] < 120 or time.time() - ST['t'] < 20:
            return
        if SKIP_IMPORT:
            bps = {n: unreal.load_asset(ROOT + '/BP_SandedHouse_' + n) for n in ruins()}
            bps = {k: v for k, v in bps.items() if v}
        else:
            bps = run_import()
        if NO_SHOTS:
            ST['phase'], ST['frames'] = 'quit', 0
            return
        ST['shots'] = setup_shots(bps)
        save_report()
        ST['phase'], ST['frames'] = 'compile', 0
        return
    if ph == 'compile':
        if ST['frames'] < 600:  # shaders + RVT pages for the new meshes
            return
        ST['phase'] = 'aim'
        return
    if ph == 'aim':
        if not ST['shots']:
            ST['phase'], ST['frames'] = 'quit', 0
            return
        aim(ST['shots'][0][1], ST['shots'][0][2])
        ST['phase'], ST['frames'] = 'settle', 0
        return
    if ph == 'settle':
        if ST['frames'] < 150:
            if ST['frames'] % 10 == 0:
                aim(ST['shots'][0][1], ST['shots'][0][2])
            return
        name, _eye, _look = ST['shots'].pop(0)
        path = OUT + 'shots/%s.png' % name
        ST['task'] = unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, path)
        log('shot ' + path)
        ST['phase'], ST['frames'] = 'after', 0
        return
    if ph == 'after':
        task = ST.get('task')
        if ST['frames'] < 30 or (task is not None and not task.is_task_done() and ST['frames'] < 900):
            return
        ST['phase'] = 'aim'
        return
    if ph == 'quit':
        if ST['frames'] < 60:
            return
        save_report()
        ST['phase'] = 'done'
        unreal.SystemLibrary.quit_editor()


ST['handle'] = unreal.register_slate_post_tick_callback(tick)
