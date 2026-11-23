"""Read-only: dump material instance parameters (commandlet)."""
import unreal
P = ['/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Materials/MI_OW_Wood',
     '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/MI_Phytophage_BlackPlum_Solid',
     '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/MI_Phytophage_BlackPlum',
     '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/MI_Phytophage_AshMauve',
     '/Game/RTS_Survival/Blueprints/Environment/OreMountains/OreClues/MI_OreClueDecal_Words',
     '/Game/Modular_MedievalTown_WI/Materials/Materialnstances/MI_GrainBag_1_Grain',
     '/Game/Modular_MedievalTown_WI/Materials/Materialnstances/MI_GrainBag_2_Cloth',
     '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Map/MI_OderWarthe_AlgaeWater',
     '/Game/MillitaryBase/Materials/MI_TentTextil', '/Game/Greek_island/Materials/MI_fabric_SSS1']
def L(*a): unreal.log_warning('MATDUMP ' + ' '.join(str(x) for x in a))
for p in P:
    mi = unreal.load_asset(p)
    if not mi:
        L('missing', p); continue
    par = mi.get_editor_property('parent') if isinstance(mi, unreal.MaterialInstance) else None
    L('==', p, 'parent', par.get_path_name() if par else None, 'base', mi.get_base_material().get_path_name())
    if isinstance(mi, unreal.MaterialInstance):
        for v in mi.get_editor_property('scalar_parameter_values'):
            L('  s', v.parameter_info.name, round(v.parameter_value, 3))
        for v in mi.get_editor_property('vector_parameter_values'):
            c = v.parameter_value; L('  v', v.parameter_info.name, round(c.r, 3), round(c.g, 3), round(c.b, 3), round(c.a, 3))
        for v in mi.get_editor_property('texture_parameter_values'):
            L('  t', v.parameter_info.name, v.parameter_value.get_path_name() if v.parameter_value else None)
    base = mi.get_base_material()
    mel = unreal.MaterialEditingLibrary
    L('  all_scalars', [str(n) for n in mel.get_scalar_parameter_names(base)])
    L('  all_vectors', [str(n) for n in mel.get_vector_parameter_names(base)])
    L('  all_textures', [str(n) for n in mel.get_texture_parameter_names(base)])
    L('  blend', base.get_editor_property('blend_mode'), 'domain', base.get_editor_property('material_domain'))
