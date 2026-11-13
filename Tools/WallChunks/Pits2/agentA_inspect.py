"""Agent A: inspect the 5 assigned pit BXPs: components, meshes, slots, collision, aim points (2026-11-12)."""
import unreal

PITS = [
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusMortarPit/Rus_BXP_120MortarPit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerSteppedSquarePit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Rus_BXP_ML20TwinShouldersPit',
]
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
out = []
for path in PITS:
    bp = unreal.load_asset(path)
    out.append('=== ' + path.split('/')[-1])
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(sds.k2_find_subobject_data_from_handle(h))
        if obj is None:
            continue
        line = '  %-30s %s' % (obj.get_name().replace('_GEN_VARIABLE', ''), obj.get_class().get_name())
        if isinstance(obj, unreal.StaticMeshComponent):
            sm = obj.get_editor_property('static_mesh')
            line += ' mesh=%s' % (sm.get_path_name() if sm else None)
            ov = [m.get_name() if m else None for m in obj.get_editor_property('override_materials')]
            if any(ov):
                line += ' override=%s' % ov
            line += ' profile=%s' % obj.get_collision_profile_name()
            if sm:
                body = sm.get_editor_property('body_setup')
                line += ' flag=%s convex=%d' % (body.get_editor_property('collision_trace_flag'), sms.get_convex_collision_count(sm))
        out.append(line)
    cdo = unreal.get_default_object(bp.generated_class())
    out.append('  Aim CDO: %s' % [(round(v.x), round(v.y), round(v.z)) for v in cdo.get_editor_property('AimOffsetPoints')])
print('\n'.join(out))
