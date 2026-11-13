"""Lead step for the 2026-11-12 pit rollout: one shared NoGrid Soviet sandbag hole MI + dump of reference settings.

NoGrid instances are mesh-independent (WallGridColumns 0), so every Soviet sandbag slot shares one instance.
"""
import unreal

EAL = unreal.EditorAssetLibrary
SRC_MI = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Meshes/Holes/MI_SM_53KPit_Rus_SandBag_01_WallHoles'
DST_MI = '/Game/SandBagPack/Materials/Holes/MI_RusPit_SandBag_01_WallHoles'

if not EAL.does_asset_exist(DST_MI):
    EAL.duplicate_asset(SRC_MI, DST_MI)
    EAL.save_asset(DST_MI, only_if_is_dirty=False)
mi = unreal.load_asset(DST_MI)
print('shared MI', mi.get_path_name(), 'parent', mi.get_editor_property('parent').get_name(),
      'two_sided', mi.get_editor_property('base_property_overrides').get_editor_property('two_sided'),
      'columns', [s.parameter_value for s in mi.get_editor_property('scalar_parameter_values')
                  if str(s.parameter_info.name) == 'WallGridColumns'])

sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)


def dump(bp_path):
    bp = unreal.load_asset(bp_path)
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(sds.k2_find_subobject_data_from_handle(h))
        if obj is None or not isinstance(obj, unreal.WallHoleDamageComponent):
            continue
        print('==', bp_path.split('/')[-1])
        for prop in ('WallMeshComponentName',):
            print('  ', prop, obj.get_editor_property(prop))
        print('   DamagedMaterials', [m.get_path_name() if m else None for m in obj.get_editor_property('DamagedMaterials')])
        print('   DebrisMeshes', [m.get_name() if m else None for m in obj.get_editor_property('DebrisMeshes')])
        for sname in ('HoleSettings', 'GridSettings', 'CollapseSettings'):
            s = obj.get_editor_property(sname)
            print('  ', sname, s.export_text() if hasattr(s, 'export_text') else s)


dump('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit')
dump('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Pak40')
