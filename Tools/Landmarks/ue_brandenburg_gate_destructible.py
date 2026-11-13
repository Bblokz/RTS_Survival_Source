"""Destructible Brandenburg Gate ruin (2026-11-12): procedural shell holes on the stone + BP_BrandenburgGateRuin.

Run inside the editor (UECP execute_python), after ue_brandenburg_gate_import.py:
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/Landmarks/ue_brandenburg_gate_destructible.py').read())
    run()                  # or run(('holes', 'debris', 'blueprint'))

- holes:     M_BG_Sandstone_Procedural_WallHoles (masked copy, Docs/Environment/WallChunkDamage.md A + B) and the two-sided
             NoGrid instances MI_BG_Sandstone_WallHoles / MI_BG_SandstoneBroken_WallHoles (same overrides as the intact ones).
- debris:    MI_BG_SandstoneDebris_WallHoles (no LightMapUV maps, no rebar: the chunk meshes have no gate UV1) and the 8
             wall chunk pieces copied as Debris/SM_BG_Gate_Debris_0..7 in it.
- blueprint: BP_BrandenburgGateRuin, a copy of BP_Dest_PostApo_HighRise1 (WallHoleDamage + CollapseIntoPieces +
             VerticalDestruction) with the gate mesh and these materials and debris. Its graphs are rebuilt separately
             through UECP build_blueprint_graph (health 10000, damage reduction 20, sink with heavy smoke).
The intact instances, the master and the BF master are not changed.
"""
import unreal

exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_holes_material.py').read())


class _PieSafeAssets:
    """EditorAssetLibrary refuses duplicate/save while the editor plays (PIE); AssetTools and the package saver do not.
    The library's functions look EAL up at call time, so this stand-in serves them too."""
    does_asset_exist = staticmethod(unreal.EditorAssetLibrary.does_asset_exist)

    @staticmethod
    def duplicate_asset(source_path, target_path):
        folder, name = target_path.rsplit('/', 1)
        return unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, folder, unreal.load_asset(source_path))

    @staticmethod
    def save_loaded_asset(asset, only_if_dirty=False):
        return unreal.EditorLoadingAndSavingUtils.save_packages([asset.get_outermost()], only_if_dirty)


EAL = _PieSafeAssets

H = '/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/'
MESH = H + 'SM_BrandenburgGate_Ruin'
TEMPLATE_BP = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/BP_Dest_PostApo_HighRise1'
GATE_BP = H + 'BP_BrandenburgGateRuin'
DEBRIS_SWITCHES_OFF = ('BF_UseCurvatureMap', 'BF_UseBakedFields', 'BF_UseRebar')


def _two_sided(instance):
    overrides = instance.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_two_sided', True)
    overrides.set_editor_property('two_sided', True)
    instance.set_editor_property('base_property_overrides', overrides)


def _child_of(source_path, parent, target_path):
    """Copy of an instance (keeps its overrides and static switches) under a new parent."""
    instance = unreal.load_asset(target_path) if EAL.does_asset_exist(target_path) else \
        EAL.duplicate_asset(source_path, target_path)
    MEL.set_material_instance_parent(instance, parent)
    _two_sided(instance)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    return instance


def holes():
    mesh = unreal.load_asset(MESH)
    stone = make_holes_instance(H + 'MI_BG_Sandstone', H + 'Holes', mesh, grid_axis=None, two_sided=True)
    broken = _child_of(H + 'MI_BG_SandstoneBroken', stone, H + 'Holes/MI_BG_SandstoneBroken_WallHoles')
    master = unreal.load_asset(H + 'M_BG_Sandstone_Procedural_WallHoles')
    masked, opaque = check_mask_live(master)
    stats = {}
    for mi in (stone, broken):
        s = MEL.get_statistics(mi)
        stats[mi.get_name()] = s.num_pixel_shader_instructions
    print('GATE_HOLES master PS', masked, 'vs', opaque, stats)
    return stone, broken


def debris():
    broken = unreal.load_asset(H + 'Holes/MI_BG_SandstoneBroken_WallHoles')
    mi = _child_of(H + 'Holes/MI_BG_SandstoneBroken_WallHoles', broken, H + 'Holes/MI_BG_SandstoneDebris_WallHoles')
    for switch in DEBRIS_SWITCHES_OFF:
        MEL.set_material_instance_static_switch_parameter_value(mi, switch, False)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    pieces = make_debris_pieces(H + 'Debris', 'SM_BG_Gate', mi)
    print('GATE_DEBRIS', mi.get_name(), [p.get_name() for p in pieces])
    return pieces


def _templates(bp):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    return [lib.get_object(lib.get_data(h)) for h in sds.k2_gather_subobject_data_for_blueprint(bp)]


def blueprint():
    bp = unreal.load_asset(GATE_BP) if EAL.does_asset_exist(GATE_BP) else EAL.duplicate_asset(TEMPLATE_BP, GATE_BP)
    stone = unreal.load_asset(H + 'Holes/MI_BG_Sandstone_WallHoles')
    broken = unreal.load_asset(H + 'Holes/MI_BG_SandstoneBroken_WallHoles')
    pieces = [unreal.load_asset(f'{H}Debris/SM_BG_Gate_Debris_{i}') for i in range(DEBRIS_PIECES)]
    for comp in _templates(bp):
        if isinstance(comp, unreal.StaticMeshComponent) and comp.get_name().startswith('StaticMesh'):
            comp.set_static_mesh(unreal.load_asset(MESH))
            comp.set_editor_property('override_materials', [])
        elif isinstance(comp, unreal.WallHoleDamageComponent):
            # slots: 0 stone, 1 broken stone / rubble, 2 bronze (no holes), 3 rebar (no holes)
            comp.set_editor_property('damaged_materials', [stone, broken, None, None])
            comp.set_editor_property('debris_meshes', pieces)
            hs = comp.get_editor_property('hole_settings')
            hs.set_editor_property('hole_radius_per_calibre_mm', 1.1)
            hs.set_editor_property('max_hole_radius', 260.0)
            comp.set_editor_property('hole_settings', hs)
            gs = comp.get_editor_property('grid_settings')
            gs.set_editor_property('area_per_debris_piece', 3600.0)
            gs.set_editor_property('max_pieces_per_hit', 6)
            gs.set_editor_property('max_collapse_pieces', 160)
            comp.set_editor_property('grid_settings', gs)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    EAL.save_loaded_asset(bp, False)
    print('GATE_BP', bp.get_path_name())
    return bp


def run(stages=('holes', 'debris', 'blueprint')):
    for stage in stages:
        globals()[stage]()
