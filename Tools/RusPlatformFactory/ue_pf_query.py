"""Read-only query for the platform factory trolley build (no asset is saved).

Headless: UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this> ...
Dumps the T-34 hull socket / bounds / materials, the T-34/76 turret meshes and how the turret Blueprint attaches its gun,
and exports the turret base, gun and hull meshes as FBX for the Blender build (Saved/EnemyBuildingTrainer/platform_query).
"""
import json
import os

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/EnemyBuildingTrainer/platform_query/'
HULL = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Meshes/SM_T34_Hull'
TRACKS = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Meshes/SK_T34_Tracks'
TURRET = '/Game/RTS_Survival/Blueprints/Turrets/Russian/Medium/T34Turrets/Meshes/SM_T34_76_TurretBaseMesh'
GUN = '/Game/RTS_Survival/Blueprints/Turrets/Russian/Medium/T34Turrets/T34_76/Meshes/SK_34_76WeaponSystem'
BPS = ['/Game/RTS_Survival/Blueprints/Turrets/Russian/Medium/T34Turrets/T34_76/Rus_Turret_T34_76',
       '/Game/RTS_Survival/Blueprints/Turrets/Russian/Medium/T34Turrets/T34_76/Rus_Turret_T34_76_Soviet',
       '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476_Soviet',
       '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_PlatformFactory']
OUT = {'log': []}
SUBS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary


def vec(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


def rot(r):
    return [round(r.pitch, 3), round(r.yaw, 3), round(r.roll, 3)]


def static_info(path):
    mesh = unreal.load_asset(path)
    box = mesh.get_bounding_box()
    sockets = []
    # StaticMesh.sockets is protected from python (memory headless-mesh-socket-python): list by an empty tag.
    for s in mesh.get_sockets_by_tag('') or []:
        sockets.append({'name': str(s.get_editor_property('socket_name')),
                        'loc': vec(s.get_editor_property('relative_location')),
                        'rot': rot(s.get_editor_property('relative_rotation')),
                        'scale': vec(s.get_editor_property('relative_scale'))})
    mats = []
    for slot in mesh.get_editor_property('static_materials'):
        mi = slot.get_editor_property('material_interface')
        mats.append({'slot': str(slot.get_editor_property('material_slot_name')), 'mat': mi.get_path_name() if mi else None})
    return {'bounds': [vec(box.min), vec(box.max)], 'sockets': sockets, 'materials': mats, 'tris': mesh.get_num_triangles(0)}


def skeletal_info(path):
    mesh = unreal.load_asset(path)
    box = mesh.get_bounds().box_extent
    origin = mesh.get_bounds().origin
    mats = []
    for slot in mesh.get_editor_property('materials'):
        mi = slot.get_editor_property('material_interface')
        mats.append({'slot': str(slot.get_editor_property('material_slot_name')), 'mat': mi.get_path_name() if mi else None})
    skeleton = mesh.get_editor_property('skeleton')
    bones = []
    try:
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        for name in unreal.AnimPoseExtensions.get_bone_names(pose):
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)
            bones.append({'bone': str(name), 'loc': vec(t.translation), 'rot': rot(t.rotation.rotator())})
    except Exception as exc:
        bones.append({'error': repr(exc)})
    sockets = []
    try:
        for i in range(mesh.num_sockets()):
            s = mesh.get_socket_by_index(i)
            sockets.append({'name': str(s.get_editor_property('socket_name')), 'bone': str(s.get_editor_property('bone_name')),
                            'loc': vec(s.get_editor_property('relative_location')),
                            'rot': rot(s.get_editor_property('relative_rotation'))})
    except Exception as exc:
        sockets.append({'error': repr(exc)})
    return {'bounds_origin': vec(origin), 'bounds_extent': vec(box), 'materials': mats, 'bones': bones, 'sockets': sockets}


def bp_info(path):
    bp = unreal.load_asset(path)
    if bp is None:
        return {'missing': True}
    rows = []
    handles = SUBS.k2_gather_subobject_data_for_blueprint(bp)
    datas = [(h, LIB.get_data(h)) for h in handles]
    for h, d in datas:
        obj = LIB.get_object_for_blueprint(d, bp)
        item = {'name': str(LIB.get_variable_name(d)), 'class': obj.get_class().get_name() if obj else None,
                'inherited': bool(LIB.is_inherited_component(d))}
        parent = ''
        for h2, d2 in datas:
            if h2 != h:
                try:
                    if LIB.is_attached_to(d, h2):
                        parent = str(LIB.get_variable_name(d2))
                        break
                except Exception:
                    pass
        item['parent'] = parent
        if isinstance(obj, unreal.SceneComponent):
            item['loc'] = vec(obj.get_editor_property('relative_location'))
            item['rot'] = rot(obj.get_editor_property('relative_rotation'))
            item['scale'] = vec(obj.get_editor_property('relative_scale3d'))
            try:
                item['socket'] = str(obj.get_attach_socket_name())
            except Exception as exc:
                item['socket'] = repr(exc)
        if isinstance(obj, unreal.StaticMeshComponent):
            sm = obj.get_editor_property('static_mesh')
            item['mesh'] = sm.get_path_name() if sm else None
            item['override'] = [m.get_path_name() if m else None for m in obj.get_editor_property('override_materials')]
        if isinstance(obj, unreal.SkeletalMeshComponent):
            sk = obj.get_editor_property('skeletal_mesh_asset')
            item['mesh'] = sk.get_path_name() if sk else None
            item['override'] = [m.get_path_name() if m else None for m in obj.get_editor_property('override_materials')]
        if isinstance(obj, unreal.ChildActorComponent):
            cls = obj.get_editor_property('child_actor_class')
            item['child_class'] = cls.get_path_name() if cls else None
        rows.append(item)
    return rows


def export(path, filename):
    task = unreal.AssetExportTask()
    task.object = unreal.load_asset(path)
    task.filename = OUT_DIR + filename
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    opts = unreal.FbxExportOption()
    opts.set_editor_property('collision', False)
    opts.set_editor_property('level_of_detail', False)
    opts.set_editor_property('vertex_color', False)
    task.options = opts
    ok = unreal.Exporter.run_asset_export_task(task)
    OUT['log'].append('export %s -> %s ok=%s exists=%s' % (path, filename, ok, os.path.exists(OUT_DIR + filename)))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    OUT['hull'] = static_info(HULL)
    OUT['turret'] = static_info(TURRET)
    OUT['gun'] = skeletal_info(GUN)
    OUT['tracks'] = skeletal_info(TRACKS)
    OUT['bps'] = {p.rsplit('/', 1)[-1]: bp_info(p) for p in BPS}
    open(OUT_DIR + 'query.json', 'w', encoding='utf-8').write(json.dumps(OUT, default=str))
    # Static meshes only: the skeletal FBX exporter asserts in a commandlet (CPU skinning has no MeshObject, 2026-11-19).
    for p, f in ((TURRET, 'SM_T34_76_TurretBaseMesh.fbx'), (HULL, 'SM_T34_Hull.fbx')):
        try:
            export(p, f)
        except Exception as exc:
            OUT['log'].append('export failed %s %r' % (p, exc))


try:
    main()
    OUT['ok'] = True
except Exception as exc:
    import traceback
    OUT['ok'] = False
    OUT['error'] = repr(exc)
    OUT['trace'] = traceback.format_exc()
text = json.dumps(OUT, default=str)
try:
    open(OUT_DIR + 'query.json', 'w', encoding='utf-8').write(text)
except Exception:
    pass
unreal.log('PF_QUERY_RESULT ' + text)
