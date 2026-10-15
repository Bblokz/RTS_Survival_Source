"""Sets the ML-20 skeleton's sockets through the editor bridge (Python cannot set a socket's transform).

Launch and Out sit on B_Gun at the muzzle and the breech (ml20_rig_info.json, Blender gun frame); Unreal bone space
is (x, z, y) of the Blender offset from B_Gun's head, with roll -90 like the B-4's sockets. PitOrigin, ShellMount,
CartMount and RammerMount sit at their bones' origins, as on the B-4, so the B-4 pit turret's props fit unchanged.

Run: python ml20_sockets.py   (with the scratchpad uecp.py importable and the bridge on 127.0.0.1:9855)
"""
import json

from uecp import call_tool

SKELETON = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Mesh/SK_TW_ML20_Skeleton'
MUZZLE = (358.4, 0.0, 167.66)
BREECH = (-103.8, 0.0, 167.16)
GUN_BONE_HEAD = (-9.398, 0.0, 160.02)
SOCKET_ROLL_DEGREES = -90.0
# (socket, bone, Blender gun-frame point or None for the bone origin)
SOCKETS = [('Launch', 'B_Gun', MUZZLE), ('Out', 'B_Gun', BREECH), ('PitOrigin', 'B_Root', None),
           ('ShellMount', 'B_Shell', None), ('CartMount', 'B_AmmoCart', None), ('RammerMount', 'B_Rammer', None)]


def bone_space_location(point):
    if point is None:
        return [0.0, 0.0, 0.0]
    offset = [point[i] - GUN_BONE_HEAD[i] for i in range(3)]
    return [round(offset[0], 2), round(offset[2], 2), round(offset[1], 2)]


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    print(action, params.get('socket_name', ''), text[:200])
    return json.loads(text, strict=False)


def main():
    existing = act('get_skeleton_sockets', skeleton_path=SKELETON)
    names = {socket.get('name') or socket.get('socket_name') for socket in existing.get('sockets', [])}
    for name, bone, point in SOCKETS:
        if name not in names:
            act('add_skeleton_socket', skeleton_path=SKELETON, socket_name=name, bone_name=bone)
        act('set_skeleton_socket_transform', skeleton_path=SKELETON, socket_name=name,
            relative_location=bone_space_location(point), relative_rotation=[0.0, 0.0, SOCKET_ROLL_DEGREES],
            relative_scale=[1.0, 1.0, 1.0])
    act('save_asset', asset_path=SKELETON)


if __name__ == '__main__':
    main()
