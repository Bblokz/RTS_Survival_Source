"""Final read-only checks: spruce BP dependencies, LOD material slots, BP compile of every manifest Blueprint,
dirty packages outside /Prypyat/. exec in the server namespace (saves nothing)."""
import json

import unreal

_AR = unreal.AssetRegistryHelpers.get_asset_registry()
_opt = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
_deps = _AR.get_dependencies('/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces/BP_PolesieDeadSpruce_01_TallSentinel', _opt)
print('spruce deps', [str(d) for d in _deps])
_sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
_lib = unreal.SubobjectDataBlueprintFunctionLibrary
_bp = unreal.load_asset('/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces/BP_PolesieDeadSpruce_01_TallSentinel')
for _h in _sds.k2_gather_subobject_data_for_blueprint(_bp):
    _o = _lib.get_object_for_blueprint(_lib.get_data(_h), _bp)
    if isinstance(_o, unreal.WallHoleDamageComponent):
        print('spruce holes', [m.get_path_name() for m in _o.get_editor_property('DamagedMaterials')],
              [m.get_name() for m in _o.get_editor_property('DebrisMeshes')])
    elif isinstance(_o, unreal.StaticMeshComponent):
        _sm = _o.get_editor_property('static_mesh')
        print('spruce comp', _o.get_name(), _sm.get_name() if _sm else None,
              [m.get_name() if m else None for m in _o.get_editor_property('override_materials')])
_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
_d = json.load(open('D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/pripyat_assets.json', encoding='utf-8'))
_bad_lod = []
for _a in _d['assets']:
    if _a['kind'] != 'static_mesh' or _a['kit'] in ('Existing', 'Airfield'):
        continue
    _m = unreal.load_asset(_a['asset_path'])
    _n = len(_m.get_editor_property('static_materials'))
    for _l in range(_m.get_num_lods()):
        for _s in range(_m.get_num_sections(_l)):
            if _sub.get_lod_material_slot(_m, _l, _s) >= _n or _m.get_material(_sub.get_lod_material_slot(_m, _l, _s)) is None:
                _bad_lod.append((_a['id'], _l, _s))
    if _m.get_num_lods() > 1:
        print('lods', _a['id'], _n, [[_sub.get_lod_material_slot(_m, _l, _s) for _s in range(_m.get_num_sections(_l))]
                                    for _l in range(_m.get_num_lods())])
print('bad lod slots', _bad_lod)
for _p in ['/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces/Meshes/SM_PolesieDeadSpruce_02_BogLean']:
    _m = unreal.load_asset(_p)
    print('spruce lods', [[_sub.get_lod_material_slot(_m, _l, _s) for _s in range(_m.get_num_sections(_l))]
                          for _l in range(_m.get_num_lods())], len(_m.get_editor_property('static_materials')))
_bps = 0
for _a in _d['assets']:
    if _a['kind'] == 'blueprint':
        _b = unreal.load_asset(_a['asset_path'])
        unreal.BlueprintEditorLibrary.compile_blueprint(_b)
        _bps += 1 if _b.generated_class() is not None else 0
print('blueprints compiled with a class', _bps)
_dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
print('dirty outside Prypyat', [x for x in _dirty if '/Prypyat/' not in x])
