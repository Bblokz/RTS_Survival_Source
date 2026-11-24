"""Inspect the 9 pit BXPs still missing WallHoleDamage: components, meshes, slots, bounds (2026-11-12)."""
import unreal

PITS = [
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBS3Pit/Rus_BXP_BS3Pit',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusMortarPit/Rus_BXP_120MortarPit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerPit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerConcretePit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Rus_BXP_B4HowitzerSteppedSquarePit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Ger_BXP_CapturedB4Pit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Rus_BXP_ML20Pit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Rus_BXP_ML20ConcretePit',
    '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Rus_BXP_ML20TwinShouldersPit',
]

sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
out = []
for path in PITS:
    bp = unreal.load_asset(path)
    gen = bp.generated_class()
    parent = unreal.BlueprintEditorLibrary.get_blueprint_asset(bp) if False else None
    out.append('=== %s  (parent %s)' % (path.split('/')[-1], unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_name() if hasattr(unreal.BlueprintEditorLibrary,'get_blueprint_parent_class') else '?'))
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    for h in handles:
        d = sds.k2_find_subobject_data_from_handle(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
        if obj is None:
            continue
        name = obj.get_name().replace('_GEN_VARIABLE', '')
        cls = obj.get_class().get_name()
        line = '  %-34s %s' % (name, cls)
        if isinstance(obj, unreal.StaticMeshComponent):
            sm = obj.get_editor_property('static_mesh')
            line += '  mesh=%s' % (sm.get_name() if sm else None)
            ov = [m.get_name() if m else None for m in obj.get_editor_property('override_materials')]
            if any(ov):
                line += ' override=%s' % ov
            rel = obj.get_editor_property('relative_location')
            sc = obj.get_editor_property('relative_scale3d')
            line += ' loc=(%.0f,%.0f,%.0f) scale=(%.2f,%.2f,%.2f)' % (rel.x, rel.y, rel.z, sc.x, sc.y, sc.z)
            if sm:
                b = sm.get_bounding_box()
                line += '\n      bounds min(%.0f,%.0f,%.0f) max(%.0f,%.0f,%.0f)' % (b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z)
                for i, s in enumerate(sm.static_materials):
                    mi = s.material_interface
                    line += '\n      slot %d %-22s %s' % (i, s.material_slot_name, mi.get_path_name() if mi else None)
                try:
                    line += '\n      collision: convex=%d simple=%d' % (unreal.EditorStaticMeshLibrary.get_convex_collision_count(sm), unreal.EditorStaticMeshLibrary.get_simple_collision_count(sm))
                except Exception as e:
                    line += '\n      collision: ? %s' % e
        out.append(line)
    cdo = unreal.get_default_object(gen)
    try:
        out.append('  AimOffsetPoints CDO: %s' % [(round(v.x), round(v.y), round(v.z)) for v in cdo.get_editor_property('AimOffsetPoints')])
    except Exception as e:
        out.append('  AimOffsetPoints: %s' % e)
print('\n'.join(out))
