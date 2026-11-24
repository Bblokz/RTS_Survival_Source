"""Runs Phytophage/import_unreal.py into /Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage, then strips
collision from the ten meshes (decoration: no collision, no navigation) and reports bounds/LODs.

    python pripyat_cmd.py -c "exec(open(r'<this file>').read())"
"""
import runpy

import unreal

PHYTO_SRC = r'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Phytophage/import_unreal.py'
PHYTO_DEST = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage'

_phyto = runpy.run_path(PHYTO_SRC, run_name='phytophage_kit')
_phyto['main'].__globals__['DESTINATION'] = PHYTO_DEST
_phyto['main']()
_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
PHYTO_RESULT = {}
for _n in ('Curtain_01', 'Curtain_02', 'Corner_01', 'Corner_02', 'Wrap_01', 'Wrap_02', 'Mat_01', 'Mat_02', 'Vein',
           'Pustules'):
    _m = unreal.load_asset(PHYTO_DEST + '/SM_Phytophage_' + _n)
    if _m is None:
        PHYTO_RESULT[_n] = 'MISSING'
        continue
    _sub.remove_collisions(_m)
    _body = _m.get_editor_property('body_setup')
    if _body is not None:
        _body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    _ns = _m.get_editor_property('nanite_settings')
    _ns.enabled = False
    _m.set_editor_property('nanite_settings', _ns)
    unreal.EditorAssetLibrary.save_loaded_asset(_m, False)
    _b = _m.get_bounding_box()
    PHYTO_RESULT[_n] = dict(lods=_m.get_num_lods(), tris=[_m.get_num_triangles(i) for i in range(_m.get_num_lods())],
                            min=[round(_b.min.x), round(_b.min.y), round(_b.min.z)],
                            max=[round(_b.max.x), round(_b.max.y), round(_b.max.z)],
                            material=_m.get_material(0).get_name() if _m.get_material(0) else None)
for _k, _v in PHYTO_RESULT.items():
    print(_k, _v)
