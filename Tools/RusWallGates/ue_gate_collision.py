"""Unreal editor python: simple box collision for the gate meshes (closed pose) and VAT bounds.

Boxes are (centre xyz, size xyz) in mesh cm. The leaf collision is the CLOSED leaf; AWallGate turns it off once open.
The A leaf swings into +Y (inside) up to ~3.4 m, so its bounds get a positive Y extension (VAT moves vertices only on
the GPU; without it the open leaves would be culled with the closed bounds).
"""
import unreal

EAL = unreal.EditorAssetLibrary
SMS = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
MESHES = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/Meshes'


def box(lo, hi):
    return ((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2), (hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2])


BOXES = {
    'SM_RusWallGateA_Frame': [box((-120, -72, 0), (10, 52, 690)), box((-930, -72, 0), (-800, 52, 690))],
    'SM_RusWallGateA_Leaf': [box((-800, -17, 15), (-120, -2, 455))],
    'SM_RusWallGateC_Frame': [box((-180, -112, 0), (15, 132, 1140)), box((-1085, -112, 0), (-890, 132, 1140)),
                              box((-910, -92, 1000), (-160, 112, 1125))],
    'SM_RusWallGateC_Leaf': [box((-903, -32, 0), (-167, 12, 525))],
}
EXTENSION = {'SM_RusWallGateA_Leaf': (unreal.Vector(0, 350, 0), unreal.Vector(0, 0, 0))}

for name, boxes in BOXES.items():
    mesh = unreal.load_asset(MESHES + '/' + name)
    SMS.remove_collisions(mesh)
    bs = mesh.get_editor_property('body_setup')
    agg = bs.get_editor_property('agg_geom')
    elems = []
    for (c, s) in boxes:
        e = unreal.KBoxElem()
        e.set_editor_property('center', unreal.Vector(*c))
        e.set_editor_property('x', s[0])
        e.set_editor_property('y', s[1])
        e.set_editor_property('z', s[2])
        elems.append(e)
    agg.set_editor_property('box_elems', elems)
    bs.set_editor_property('agg_geom', agg)
    bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    mesh.set_editor_property('body_setup', bs)
    if name in EXTENSION:
        mesh.set_editor_property('positive_bounds_extension', EXTENSION[name][0])
        mesh.set_editor_property('negative_bounds_extension', EXTENSION[name][1])
    print(name, 'simple collisions', SMS.get_simple_collision_count(mesh), 'bounds', mesh.get_bounds().box_extent,
          'saved', EAL.save_loaded_asset(mesh, False))
