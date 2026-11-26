"""python shot.py <port> <out.png> <tx> <ty> <tz> <pitch> <yaw> <dist>: frame the editor viewport and screenshot it."""
import os
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'mission'))
from uecp_client import action, call  # noqa: E402

port, out = int(sys.argv[1]), sys.argv[2]
tx, ty, tz, pitch, yaw, dist = (float(v) for v in sys.argv[3:9])
code = f"""import unreal
r = unreal.Rotator(0, {pitch}, {yaw})
loc = unreal.Vector({tx}, {ty}, {tz}) - r.get_forward_vector() * {dist}
unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(loc, r)
"""
call(port, 'execute_python', {'code': code, 'description': 'frame shot'})
print(action(port, 'take_viewport_screenshot', {'file_path': out}))
