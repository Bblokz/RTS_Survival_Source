"""Lead read-back of the WallHoleDamage templates on the 10 rollout pits (2026-11-12)."""
import unreal

B4 = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit/'
ML = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/'
BX = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
PITS = [BX + 'GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42', BX + 'RusBS3Pit/Rus_BXP_BS3Pit',
        BX + 'RusMortarPit/Rus_BXP_120MortarPit', B4 + 'Rus_BXP_B4HowitzerSteppedSquarePit',
        ML + 'Rus_BXP_ML20TwinShouldersPit', B4 + 'Rus_BXP_B4HowitzerPit', B4 + 'Ger_BXP_CapturedB4Pit',
        B4 + 'Rus_BXP_B4HowitzerConcretePit', ML + 'Rus_BXP_ML20Pit', ML + 'Rus_BXP_ML20ConcretePit']
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for path in PITS:
    bp = unreal.load_asset(path)
    found = []
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(sds.k2_find_subobject_data_from_handle(h))
        if isinstance(obj, unreal.WallHoleDamageComponent):
            g = obj.get_editor_property('GridSettings')
            c = obj.get_editor_property('CollapseSettings')
            found.append('%s mats=%s debris=%d grid=%s area=%.0f/%d collapse=%s r=%.2f' % (
                obj.get_editor_property('WallMeshComponentName'),
                [m.get_name() if m else None for m in obj.get_editor_property('DamagedMaterials')],
                len(obj.get_editor_property('DebrisMeshes')), g.get_editor_property('GridAxis'),
                g.get_editor_property('AreaPerDebrisPiece'), g.get_editor_property('MaxPiecesPerHit'),
                c.get_editor_property('bCollapseIntoChunksOnDeath'),
                obj.get_editor_property('HoleSettings').get_editor_property('HoleRadiusPerCalibreMm')))
        elif isinstance(obj, unreal.StaticMeshComponent) and obj.get_name().startswith('Sandbags'):
            found.append('Sandbags coll=%s nav=%s' % (obj.get_collision_enabled(),
                                                     obj.get_editor_property('can_ever_affect_navigation')))
    print(path.split('/')[-1], '|', ' || '.join(found) if found else 'NO COMPONENT')
