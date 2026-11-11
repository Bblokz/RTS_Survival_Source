"""Runs the editor half of a building's nomadic deploy through the shared editor (UECP bridge), in order:
  import    deploy_mesh.fbx -> <folder>/SM_<B>_Deploy (Nanite off, full precision UVs)
  textures  vat_offset.exr / vat_rot.exr -> T_<B>_DeployVat*
  masters   NomadicDeploy static switch into the building mesh's master materials (shared, idempotent)
  mis       per-slot deploy material instances on the deploy mesh
  data      DA_<B>_Deploy (smoke screen + heavy dust bursts) set on the truck blueprint
Usage: python ue_pipeline.py <building work dir> [step ...]     (no steps = all; also: export, probe)
Holds Saved/NomadicDeploy/editor.lock (a directory) for the whole run so parallel building agents take turns on the
shared editor. Content goes to /Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/<Building>/.
"""
import json
import re
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__)).replace('\\', '/')
sys.path.insert(0, HERE)
import locks  # noqa: E402
import uecp_call  # noqa: E402

LOCK = 'D:/UE5Projects/RTS_Survival/Saved/NomadicDeploy/editor.lock'
ROOT = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/'
ALL = ['import', 'textures', 'masters', 'mis', 'data', 'verify']
SAVING = ('import', 'textures', 'masters', 'mis', 'data')


def run(script, mode='py', **variables):
    path = HERE + '/' + script
    prelude = ''.join(f'{k} = {json.dumps(v)}\n' for k, v in variables.items())
    code = prelude + f"exec(open({json.dumps(path)}, encoding='utf-8').read(), globals())\n"
    action = 'execute_python' if mode == 'py' else 'execute_uecp_script'
    raw = uecp_call.call('call_action', {'action': action, 'params': {'code': code, 'description': script}})
    out = uecp_call.unwrap(raw)
    print(out)
    # an {"ok": false} reply (e.g. the editor is blocked by a modal dialog) means the step did not run
    if re.search(r'saved(?: deploy mesh)?[ =:]*False', out):
        raise SystemExit(f'step {script} did not save an asset (file locked?)')
    if 'Traceback' in out or 'ERROR' in out or '"ok": false' in raw.replace('":false', '": false'):
        raise SystemExit(f'step {script} failed')
    return out


def acquire():
    locks.acquire('editor.lock', 'ue_pipeline ' + ' '.join(sys.argv[1:]))


def release():
    locks.release('editor.lock')


def main(argv):
    work = os.path.abspath(argv[1]).replace('\\', '/')
    steps = argv[2:] or ALL
    cfg = json.load(open(work + '/config.json'))
    b = cfg['building']
    folder = ROOT + b
    mesh = f'{folder}/SM_{b}_Deploy'
    acquire()
    # saving steps also hold headless.lock: a running throwaway editor keeps shared files (masters, truck BPs) open
    # and a save then fails with a sharing violation
    saving = [s for s in steps if s in SAVING]
    if saving:
        locks.acquire('headless.lock', 'ue_pipeline ' + b + ' ' + ' '.join(saving), poll=2)
    try:
        for i, step in enumerate(steps):
            if saving and not any(s in SAVING for s in steps[i:]):
                locks.release('headless.lock')  # verify / probe after the last save do not need it
                saving = []
            print(f'== {b}: {step}', flush=True)
            if step == 'export':
                run('ue_export.py', SOURCE_MESH=cfg['source_mesh'], OUT=work + '/building.fbx')
            elif step == 'import':
                run('ue_import.py', FBX=work + '/deploy_mesh.fbx', FOLDER=folder, NAME=f'SM_{b}_Deploy',
                    SOURCE_MESH=cfg['source_mesh'])
            elif step == 'textures':
                run('ue_vat_textures.py', WORK=work, FOLDER=folder, BUILDING=b)
            elif step == 'masters':
                run('ue_patch_masters.py', MESH=cfg['source_mesh'])
            elif step == 'mis':
                run('ue_deploy_mis.py', BUILDING=b, SOURCE_MESH=cfg['source_mesh'], DEPLOY_MESH=mesh,
                    OFFSET_TEX=f'{folder}/T_{b}_DeployVatOffset', ROT_TEX=f'{folder}/T_{b}_DeployVatRot')
            elif step == 'data':
                run('ue_data_asset.py', BUILDING=b, WORK=work, FOLDER=folder, DEPLOY_MESH=mesh, TRUCK_BP=cfg['truck_bp'])
            elif step in ('verify', 'probe'):
                # wait for shader compiles (another agent's masters step may have left shared masters compiling)
                for attempt in range(120):
                    out = uecp_call.unwrap(uecp_call.call('call_action', {'action': 'execute_python', 'params': {
                        'code': f'DEPLOY_MESH = {json.dumps(mesh)}\n' + open(HERE + '/ue_verify.py').read(),
                        'description': 'ue_verify.py'}}))
                    if 'SUMMARY finished=1' in out:
                        break
                    if 'SUMMARY' not in out:  # the script itself failed: do not spin on it
                        raise SystemExit('verify script failed: ' + out)
                    time.sleep(5)
                print(out)
                if step == 'verify':
                    if 'ok=1' not in out:
                        raise SystemExit('verify failed: a deploy slot lacks uses_wpo=1 or has compile errors')
                    continue
                out = work + '/probe'
                os.makedirs(out, exist_ok=True)
                run('ue_probe.py', ACTION='spawn', MESH=mesh)
                for warm in (True, False):
                    for p in (0.0, 0.5, 1.0):
                        run('ue_probe.py', ACTION='set', PROGRESS=p)
                        run('ue_probe.py', ACTION='capture', TAG=('warm_' if warm else '') + f'p{int(p * 100):03d}', OUT=out)
                run('ue_probe.py', ACTION='cleanup')
            else:
                raise SystemExit('unknown step ' + step)
    finally:
        if saving:
            locks.release('headless.lock')
        release()


if __name__ == '__main__':
    main(sys.argv)
