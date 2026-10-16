"""Places a gun pit's ready crate under the round the shell handler picks up (shared by the pit turret builders).

The drill bake gives the crate spot (where the round lies, in gun space) and the handler's pick-up yaw. A crate
mesh's pivot is seldom its footprint centre, so the mesh bounds are read through the editor bridge instead of being
written down here.

Use: sys.path gets this folder, then pit_crate.crate_transform(pit_crate.CRATE_MESH, carry['crate']).
"""
import math
from uecp import call_tool

# A crate small enough for the Pak pit's 4 m ring: at the CRATE_TOP height it is 51 x 69 cm, and its far corner stays
# about 3.9 m from the gun's pivot. SM_Ger_Ammobox at half size (83 x 109 cm) cannot cover the round inside that ring.
CRATE_MESH = '/Game/MillitaryBase/Meshes/SM_TankAmmoCrates_A'
# The drills lay the round on a crate top at this height (gerpit_crew.py / bs3_crew.py crate_top).
CRATE_TOP = 44.0
# The handler picks up from about 46 cm behind the round. A deep crate centred on the round would reach his feet, so
# it is pushed away from him until the round lies this far in from the crate's edge on his side.
ROUND_INSET_FROM_HANDLER_EDGE = 25.0


def read_mesh_bounds(mesh_path):
    """Returns (min, max) of the static mesh's bounding box as two (x, y, z) tuples."""
    code = f"""
import unreal
box = unreal.load_asset('{mesh_path}').get_bounding_box()
print('CRATEBOX', box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z, 'CRATEBOX_END')
"""
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': 'pit crate bounds'}})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '')
    if 'CRATEBOX ' not in text:
        raise RuntimeError(f'No bounds for {mesh_path}: {text[:400]}')
    values = [float(v) for v in text.split('CRATEBOX ')[1].split(' CRATEBOX_END')[0].split()]
    return tuple(values[:3]), tuple(values[3:])


def crate_transform(mesh_path, crate):
    """Transform of the ReadyCrate component on the pit-origin socket.

    crate: the drill's {'location': [x, y, z], 'yaw': pick-up yaw} (the round lies at location, the handler faces yaw).
    The crate is scaled so its top meets CRATE_TOP, turns its narrow side to the handler and covers the round.
    Returns (location, rotation as [pitch, yaw, roll] for the bridge, uniform scale).
    """
    (low_x, low_y, low_z), (high_x, high_y, high_z) = read_mesh_bounds(mesh_path)
    scale = CRATE_TOP / (high_z - low_z)
    # Turned to the handler's facing, the mesh's X runs along his line of reach: the short side must be along X.
    narrow_along_x = high_x - low_x < high_y - low_y
    yaw = crate['yaw'] if narrow_along_x else crate['yaw'] + 90.0
    depth = (high_x - low_x if narrow_along_x else high_y - low_y) * scale
    away_from_handler = max(0.0, depth * 0.5 - ROUND_INSET_FROM_HANDLER_EDGE)
    centre_x, centre_y = (low_x + high_x) * 0.5 * scale, (low_y + high_y) * 0.5 * scale
    a = math.radians(yaw)
    offset_x = centre_x * math.cos(a) - centre_y * math.sin(a)
    offset_y = centre_x * math.sin(a) + centre_y * math.cos(a)
    facing = math.radians(crate['yaw'])
    x, y, _ = crate['location']
    x += away_from_handler * math.cos(facing)
    y += away_from_handler * math.sin(facing)
    location = [round(x - offset_x, 3), round(y - offset_y, 3), round(-low_z * scale, 3)]
    far_corner = max(math.hypot(location[0] + px * math.cos(a) - py * math.sin(a),
                                location[1] + px * math.sin(a) + py * math.cos(a))
                     for px in (low_x * scale, high_x * scale) for py in (low_y * scale, high_y * scale))
    # The crate turns with the gun, so its far corner sets the clear radius the pit wall must keep.
    print(f'pit crate far corner {far_corner:.1f} cm from the pivot')
    return location, [0.0, yaw, 0.0], round(scale, 4)
