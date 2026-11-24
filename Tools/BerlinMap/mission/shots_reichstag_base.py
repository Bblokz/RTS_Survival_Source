"""Low-angle viewport shots of each RT_Base_* building's base (and overviews) in my editor.
Usage: python shots_reichstag_base.py <port> <out_dir> [label_filter]"""
import math
import sys
import time

from uecp_client import action

port, out = int(sys.argv[1]), sys.argv[2]
flt = sys.argv[3] if len(sys.argv) > 3 else ''
CODE_LIST = """
import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
    if a.get_actor_label().startswith('RT_Base_'):
        o, e = a.get_actor_bounds(True)
        print('ACT', a.get_actor_label(), o.x, o.y, e.x, e.y, e.z, a.get_actor_location().z)
"""
r = action(port, 'execute_python', {'code': CODE_LIST, 'description': 'list base actors'})
rows = []
for item in r.get('output', []):
    for line in item['message'].splitlines():
        if line.startswith('ACT '):
            p = line.split()
            rows.append((p[1], *map(float, p[2:])))
shots = []
for label, ox, oy, ex, ey, ez, z in rows:
    if flt and flt not in label:
        continue
    radius = max(ex, ey)
    for k, ang in enumerate((35, 215)):
        d = radius + 900
        cx, cy = ox + d * math.cos(math.radians(ang)), oy + d * math.sin(math.radians(ang))
        yaw = math.degrees(math.atan2(oy - cy, ox - cx))
        pitch = -math.degrees(math.atan2(250, d))
        shots.append((f'{label}_{k}', (cx, cy, z + 250), (pitch, yaw)))
if not flt:
    shots.append(('overview_se', (16500, -4500, 4200), (-28, -126)))
    shots.append(('overview_top', (6000, -16000, 16000), (-89.9, 0)))
for name, loc, (pitch, yaw) in shots:
    code = ("import unreal\nunreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info("
            f"unreal.Vector({loc[0]}, {loc[1]}, {loc[2]}), unreal.Rotator(roll=0, pitch={pitch}, yaw={yaw}))\n")
    action(port, 'execute_python', {'code': code, 'description': 'move camera'})
    time.sleep(3)
    res = action(port, 'take_viewport_screenshot', {'file_path': f'{out}/{name}.png', 'max_dimension': 1600})
    print(name, res.get('ok') if isinstance(res, dict) else str(res)[:200])
