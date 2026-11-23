"""Throwaway-editor dump for the buried 1945 armour sand mounds (read-only, saves nothing).
Spawns each vehicle BP in the editor world of a unit-test map, records every visible mesh component
(mesh path, transform relative to the vehicle actor, sockets) and exports each unique mesh to FBX, so Blender
can rebuild the exact in-game assembly.
  UnrealEditor.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py D:/UE5Projects/RTS_Survival/Saved/BuriedArmour/dump_vehicles.py" -unattended -nosplash
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
"""
import json
import os
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/BuriedArmour/dump/'
V = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/'
BPS = {
    'StuGIII': V + 'Ger_Medium/Stug/Ger_Vehicle_StugIII',
    'Wespe': V + 'Ger_Light/Wespe/Ger_Vehicle_Wespe',
    'FlakLuchs': V + 'Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs_FlakLuchs',
    'PzIV_F1_Assault': V + 'Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_F1_Assault',
    'Sdkfz251_MortarTW': V + 'Ger_ArmoredCar/Sdkfz251_MortarTW/Ger_ArmoredCar_251_MortarTW',
    'Sdkfz251_Transport': V + 'Ger_ArmoredCar/Sdkfz251_Transport/Ger_ArmoredCar_251_Transport',
}
S = {'t': 0.0, 'done': False, 'log': []}
os.makedirs(OUT + 'fbx', exist_ok=True)


def log(m):
    S['log'].append(m)
    unreal.log('BURIEDDUMP ' + m)


def tdict(t):
    q = t.rotation
    return {'loc': [t.translation.x, t.translation.y, t.translation.z], 'quat': [q.x, q.y, q.z, q.w],
            'scale': [t.scale3d.x, t.scale3d.y, t.scale3d.z]}


def export_mesh(mesh, exported):
    path = mesh.get_path_name().split('.')[0]
    if path in exported:
        return exported[path]
    fn = OUT + 'fbx/' + mesh.get_name() + '.fbx'
    task = unreal.AssetExportTask()
    task.object = mesh
    task.filename = fn
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    opts = unreal.FbxExportOption()
    opts.collision = False
    opts.level_of_detail = False
    opts.export_morph_targets = False
    opts.export_preview_mesh = False
    task.options = opts
    if isinstance(mesh, unreal.StaticMesh):
        task.exporter = unreal.StaticMeshExporterFBX()
    else:
        task.exporter = unreal.SkeletalMeshExporterFBX()
    ok = unreal.Exporter.run_asset_export_task(task)
    info = {'fbx': fn if ok else None, 'class': mesh.get_class().get_name()}
    try:
        b = mesh.get_bounding_box() if isinstance(mesh, unreal.StaticMesh) else mesh.get_imported_bounds().get_box()
        info['bbox'] = [[b.min.x, b.min.y, b.min.z], [b.max.x, b.max.y, b.max.z]]
    except Exception as e:  # noqa: BLE001
        info['bbox_err'] = str(e)
    exported[path] = info
    log(f'export {path} ok={ok}')
    return info


def gather(actor, root_inv, seen, rows, owner_chain):
    if actor in seen:
        return
    seen.add(actor)
    for c in actor.get_components_by_class(unreal.SceneComponent):
        row = {'actor': actor.get_class().get_name(), 'chain': owner_chain, 'comp': c.get_name(),
               'class': c.get_class().get_name()}
        rel = c.get_world_transform() * root_inv
        row['xf'] = tdict(rel)
        try:
            row['visible'] = bool(c.is_visible()) and not bool(c.get_editor_property('hidden_in_game'))
        except Exception:  # noqa: BLE001
            row['visible'] = True
        p = c.get_attach_parent()
        row['parent'] = p.get_name() if p else None
        row['socket'] = str(c.get_attach_socket_name())
        mesh = None
        if isinstance(c, unreal.StaticMeshComponent):
            mesh = c.static_mesh
        elif isinstance(c, unreal.SkeletalMeshComponent):
            mesh = c.get_skeletal_mesh_asset() if hasattr(c, 'get_skeletal_mesh_asset') else c.skeletal_mesh
        if mesh:
            row['mesh'] = mesh.get_path_name().split('.')[0]
            row['mats'] = [m.get_path_name().split('.')[0] if m else None for m in c.get_materials()]
        if isinstance(c, unreal.ChildActorComponent) and c.child_actor:
            row['child_actor'] = c.child_actor.get_class().get_name()
        rows.append(row)
        if isinstance(c, unreal.ChildActorComponent) and c.child_actor:
            gather(c.child_actor, root_inv, seen, rows, owner_chain + [c.get_name()])
    for a in actor.get_attached_actors():
        gather(a, root_inv, seen, rows, owner_chain + ['attached'])


def run():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    exported = {}
    result = {}
    for i, (key, path) in enumerate(BPS.items()):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
        if not cls:
            log(f'{key}: no class at {path}')
            continue
        loc = unreal.Vector(5000.0 * (i + 1), 30000.0, 2000.0)
        actor = eas.spawn_actor_from_class(cls, loc, unreal.Rotator(0, 0, 0))
        if not actor:
            log(f'{key}: spawn failed')
            continue
        root_inv = actor.get_actor_transform().inverse()
        rows = []
        gather(actor, root_inv, set(), rows, [])
        for r in rows:
            if r.get('mesh'):
                mesh = unreal.load_asset(r['mesh'])
                r['export'] = export_mesh(mesh, exported)
        result[key] = {'bp': path, 'components': rows}
        log(f'{key}: {len(rows)} components, {sum(1 for r in rows if r.get("mesh"))} meshes')
    json.dump({'vehicles': result, 'meshes': exported, 'log': S['log']}, open(OUT + 'vehicles.json', 'w'), indent=1)
    log('wrote vehicles.json')


def tick(dt):
    S['t'] += dt
    if S['done'] or S['t'] < 8.0:
        return
    S['done'] = True
    try:
        run()
    except Exception as e:  # noqa: BLE001
        import traceback
        log('ERROR ' + traceback.format_exc())
        json.dump({'log': S['log']}, open(OUT + 'error.json', 'w'), indent=1)
    unreal.SystemLibrary.quit_editor()


S['handle'] = unreal.register_slate_post_tick_callback(tick)
