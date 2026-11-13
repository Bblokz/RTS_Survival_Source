"""Agent A, throwaway editor: switch SM_BS3Pit to complex-as-simple (its 7 convex hulls sat 35 cm median / up to 1 m
outside the stepped bag faces and stood above the bags in places), verify traces now land on the render surface,
save the mesh. The 7 hulls stay (used only for physics bodies). Backup: Saved/WallChunks/Pits2/agentA_SM_BS3Pit_collision_backup.txt.
Run with agentA_run_headless.sh while the shared editor does not hold the package."""
import json
import unreal

exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Pits2/agentA_collision_measure.py').read().split('def tick(delta):')[0])
PATH = MESHES['BS3']
S2 = {'t': 0.0, 'phase': 0}


def finish2():
    with open(ROOT + 'agentA_bs3_complex_log.json', 'w') as handle:
        json.dump(S['log'], handle, indent=1)
    unreal.SystemLibrary.quit_editor()
    S2['phase'] = 99


def tick2(delta):
    S2['t'] += delta
    try:
        if S2['phase'] == 0 and S2['t'] > 15.0:
            unreal.load_asset(PATH)
            S2['phase'], S2['mark'] = 1, S2['t']
        elif S2['phase'] == 1 and S2['t'] - S2['mark'] > 20.0:
            mesh = unreal.load_asset(PATH)
            body = mesh.get_editor_property('body_setup')
            log('before flag %s' % body.get_editor_property('collision_trace_flag'))
            mesh.modify()
            body.modify()
            body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            log('after flag %s' % body.get_editor_property('collision_trace_flag'))
            S2['phase'], S2['mark'] = 2, S2['t']
        elif S2['phase'] == 2 and S2['t'] - S2['mark'] > 10.0:
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            mesh = unreal.load_asset(PATH)
            rays = make_rays(mesh)
            actor = spawn(mesh)
            hits = trace_all(world, rays)
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(actor)
            count = sum(1 for h in hits if h is not None)
            log('complex hits %d/%d' % (count, len(rays)))
            path = ROOT + 'agentA_BS3_coll.json'
            data = json.load(open(path))
            data['meshes']['complex'] = hits
            json.dump(data, open(path, 'w'))
            if count == 0:
                log('ERROR no hits with complex-as-simple: not saving')
            else:
                log('saved %s' % unreal.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=False))
            finish2()
        elif S2['t'] > 200.0 and S2['phase'] != 99:
            log('ERROR timeout')
            finish2()
    except Exception as error:  # noqa: BLE001
        log('ERROR %r' % (error,))
        finish2()


unreal.register_slate_post_tick_callback(tick2)
