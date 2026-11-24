"""Read-only survey for the 2026-11-17 SD BXP round (radio van, Wurfrahmen shop, sniper loft). Headless:
UnrealEditor-Cmd RTS_Survival.uproject -run=pythonscript -script=<this> ; writes Saved/SDBxpRound2/dump.txt"""
import unreal

OUT = []
BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
NOMADIC = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/'
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary


def log(*a):
    OUT.append(' '.join(str(x) for x in a))


def mesh_info(path):
    mesh = unreal.load_asset(path)
    if mesh is None:
        log('MISSING', path)
        return
    log('MESH', path, 'tris', mesh.get_num_triangles(0))
    for i, m in enumerate(mesh.get_editor_property('static_materials')):
        mi = m.get_editor_property('material_interface')
        log('   slot', i, m.get_editor_property('material_slot_name'), mi.get_path_name() if mi else None)
    for name in ('Turret', 'Train', 'Way', 'Aerial', 'Van', 'Roof', 'Rockets', 'Jig', 'BXP_Origin'):
        s = mesh.find_socket(name)
        if s:
            log('   socket', name, s.get_editor_property('relative_location'), s.get_editor_property('relative_rotation'))
    b = mesh.get_bounding_box()
    log('   bounds', b.min, b.max)
    try:
        cc = mesh.get_editor_property('complex_collision_mesh')
        log('   complex', cc.get_path_name() if cc else None)
    except Exception as e:
        log('   complex ?', e)


INTERESTING = ('static_mesh', 'child_actor_class', 'skeletal_mesh_asset', 'WallMeshComponentName', 'DamagedMaterials',
               'DebrisMeshes', 'HoleSettings', 'GridSettings', 'CollapseSettings', 'HealthLevelsToNotifyOn',
               'attach_parent', 'relative_location', 'relative_rotation')


def bp_info(path):
    bp = unreal.load_asset(path)
    if bp is None:
        log('MISSING BP', path)
        return
    gen = bp.generated_class()
    log('BP', path, 'parent', bp.get_editor_property('parent_class').get_path_name() if hasattr(bp, 'parent_class') else '')
    try:
        log('   parent_class', unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp) if hasattr(unreal.BlueprintEditorLibrary, 'get_blueprint_parent_class') else '')
    except Exception:
        pass
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = sds.k2_find_subobject_data_from_handle(handle)
        obj = LIB.get_object_for_blueprint(data, bp)
        if obj is None:
            continue
        log('   comp', obj.get_name(), obj.get_class().get_name())
        for prop in INTERESTING:
            try:
                v = obj.get_editor_property(prop)
            except Exception:
                continue
            if hasattr(v, 'export_text'):
                v = v.export_text()
            elif isinstance(v, unreal.Object):
                v = v.get_path_name()
            elif isinstance(v, unreal.Array):
                v = [x.get_path_name() if isinstance(x, unreal.Object) else str(x) for x in v]
            log('      ', prop, '=', v)
        try:
            log('       attach_socket', obj.get_attach_socket_name(), 'parent', obj.get_attach_parent().get_name() if obj.get_attach_parent() else None)
        except Exception:
            pass
    cdo = unreal.get_default_object(gen)
    for prop in ('AimOffsetPoints', 'bIsStandAlone'):
        try:
            log('   CDO', prop, cdo.get_editor_property(prop))
        except Exception as e:
            pass


def main():
    for p in (BXP + 'GerBxps/Forge/SchreckFrame/Meshes/SM_GerForge_SchreckFrame',
              NOMADIC + 'GerT1/MechanizedDepot/BuildingMeshes/SM_MechanizedDepot_BuildingMesh',
              NOMADIC + 'GerT1/OpelBlitz_Barracks/BuildingMeshes/SM_Ger_Barracks_BuildingMesh'):
        mesh_info(p)
    for p in (BXP + 'GerBxps/Forge/SchreckFrame/Ger_Bxp_LFactorySchreckFrame',
              BXP + 'GerPits/Turrets/RTS_BP_EmbTurret_GerPit_Panzerschreck',
              NOMADIC + 'GerT2/CommunicationsCenter/BXPs/MG42Nests/Ger_Bxp_CommMG42Nests',
              BXP + 'GerPits/TwinMG42Pit/Turrets/Ger_BP_EmbTurret_TwinMG42',
              BXP + 'GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42'):
        bp_info(p)
    # Asset registry listing of relevant folders.
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for folder in (BXP + 'GerBxps/Forge/SchreckFrame', NOMADIC + 'GerT1/MechanizedDepot', NOMADIC + 'GerT1/OpelBlitz_Barracks'):
        for a in reg.get_assets_by_path(folder, recursive=True):
            log('ASSET', a.package_name, a.asset_class_path.asset_name)
    unreal.log('DUMP_DONE')
    with open('D:/UE5Projects/RTS_Survival/Saved/SDBxpRound2/dump.txt', 'w', encoding='utf-8') as f:
        f.write('\n'.join(OUT))


main()
