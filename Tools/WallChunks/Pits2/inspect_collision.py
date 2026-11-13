"""Collision trace flags + component collision profiles for the pit meshes (2026-11-12)."""
import unreal

MESHES = [
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Meshes/SM_GerPit_TwinMG42_Sandbags',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Meshes/SM_BS3Pit',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusMortarPit/Meshes/SM_Rus120MortarPit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/SM_B4Pit_Emplacement',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/SM_B4Pit_Sandbags',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/SM_B4ConcretePit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/SM_B4ConcretePit_Sandbags',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes/SM_B4Pit_SteppedSquare',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Meshes/SM_ML20Pit_TwinShoulders',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Meshes/SM_53KPit',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusAmpulometPit/Meshes/SM_AmpulometPit',
]
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for p in MESHES:
    sm = unreal.load_asset(p)
    if sm is None:
        print('MISSING', p)
        continue
    body = sm.get_editor_property('body_setup')
    flag = body.get_editor_property('collision_trace_flag') if body else None
    tris = sm.get_num_triangles(0)
    print('%-34s tris=%6d convex=%3d flag=%s nanite=%s' % (p.split('/')[-1], tris, sms.get_convex_collision_count(sm), flag,
          sm.get_editor_property('nanite_settings').enabled))

sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for bp_path, names in [
    ('/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerPit', ('Sandbags', 'BuildingMeshComponent')),
    ('/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerConcretePit', ('Sandbags', 'BuildingMeshComponent')),
    ('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusMortarPit/Rus_BXP_120MortarPit', ('BuildingMeshComponent',)),
    ('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/Rus53KPit/Rus_BXP_53KPit', ('BuildingMeshComponent',)),
]:
    bp = unreal.load_asset(bp_path)
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(sds.k2_find_subobject_data_from_handle(h))
        if obj is None or obj.get_name().replace('_GEN_VARIABLE', '') not in names:
            continue
        prof = obj.get_collision_profile_name()
        enabled = obj.get_collision_enabled()
        resp = obj.get_editor_property('body_instance').get_editor_property('collision_responses') if False else None
        print('%s.%s profile=%s enabled=%s obj=%s' % (bp_path.split('/')[-1], obj.get_name().replace('_GEN_VARIABLE', ''),
              prof, enabled, obj.get_collision_object_type()))
