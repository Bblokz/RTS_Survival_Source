"""Wreck holes rollout (Docs/Environment/WreckHoleDamage.md). Editor Python, run in steps:

    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WreckHoles/build_wreck_holes.py', encoding='utf-8').read())
    report = build_materials()          # masked _WreckHoles copies for every wreck slot material (no C++ needed)
    patch_char_overlay()                # hole cut on M_WreckChar_Overlay
    build_debris()                      # metal shard meshes for the hole debris
    register_all()                      # DA_WreckHoleMaterials (needs UWreckHoleMaterialMap in the editor DLL)
    setup_parent_blueprints()           # WreckHoleDamage component on the wreck family roots

A new wreck that derives from one of PARENT_BLUEPRINTS only needs `build_materials(['/Game/.../Env_Dest_New'])` and
`register_all()`: the inherited component finds its meshes and looks their materials up in the map.
"""
import json
import unreal

exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WreckHoles/make_wreck_holes_material.py', encoding='utf-8').read())

WRECK_PREFIXES = ('Env_Dest', 'ScavObj_')
PARENT_BLUEPRINTS = [
    '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/BP_DestroyedTank_Scavengable',
    '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/DestroyedTanks/Bp_ScavengableObject_Tank',
    '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts1',
    '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts2',
    '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts3',
    '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts4',
    '/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/Cars/ScavObj_PostApo_Car_1',
]
COMPONENT_NAME = 'WreckHoleDamage'
DEBRIS_FOLDER = '/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckHoles'
# Bent plates and small chunks, re-skinned in burnt metal (a Niagara-usable material).
DEBRIS_SOURCES = [
    '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_Plate_A',
    '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck/SM_EngineDeck_Plate_B',
    '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/SM_Derbis_1',
    '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/SM_Derbis_2',
]
SHARD_MATERIAL_PARENT = '/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/M_WreckDebrisChunk'
SHARD_MATERIAL = DEBRIS_FOLDER + '/MI_WreckHoleShard'
REPORT = unreal.Paths.project_saved_dir() + 'WreckHoles/build_report.json'


def wreck_blueprints():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    found = registry.get_assets(unreal.ARFilter(
        package_paths=['/Game/RTS_Survival'], recursive_paths=True,
        class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'Blueprint')]))
    return sorted(str(a.package_name) for a in found if str(a.asset_name).startswith(WRECK_PREFIXES))


def slot_materials(blueprint_path):
    """Every material a static mesh component of the Blueprint shows (its override, else the mesh's)."""
    blueprint = unreal.load_asset(blueprint_path)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    materials = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)
        if not isinstance(component, unreal.StaticMeshComponent):
            continue
        mesh = component.get_editor_property('static_mesh')
        if mesh is None:
            continue
        overrides = component.get_editor_property('override_materials')
        for index, static_material in enumerate(mesh.get_editor_property('static_materials')):
            material = overrides[index] if index < len(overrides) and overrides[index] else \
                static_material.get_editor_property('material_interface')
            if material is not None:
                materials.append(material)
    return materials


def _allow_bulk_loads():
    # The UECP bridge refuses scripts that load more than 40 assets unless told it is a bulk edit.
    try:
        import uecp
        uecp.allow_bulk_loads()
    except Exception:
        pass


def build_materials(blueprints=None):
    """Masked copies for every slot material of the wrecks. Returns and writes a report."""
    _allow_bulk_loads()
    blueprints = blueprints or wreck_blueprints()
    unique = {}
    for path in blueprints:
        for material in slot_materials(path):
            unique.setdefault(material.get_path_name(), material)
    report = {'built': {}, 'skipped': {}}
    for path, material in sorted(unique.items()):
        ok, reason = can_take_holes(material)
        if not ok:
            report['skipped'][path] = reason
            continue
        damaged = make_damaged_material(material)
        if damaged is None:
            report['skipped'][path] = 'build failed'
            continue
        report['built'][path] = damaged.get_path_name()
    unreal.EditorAssetLibrary.make_directory('/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckHoles')
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    print(f'WRECK_HOLES materials built={len(report["built"])} skipped={len(report["skipped"])}')
    return report


def register_all():
    """Registers every (source, damaged) pair from the build report in DA_WreckHoleMaterials."""
    _allow_bulk_loads()
    with open(REPORT) as handle:
        report = json.load(handle)
    pairs = [(unreal.load_asset(source.split('.')[0]), unreal.load_asset(damaged.split('.')[0]))
             for source, damaged in report['built'].items()]
    count = register(pairs)
    print(f'WRECK_HOLES map pairs={count}')
    return count


def build_debris():
    """Shard meshes in burnt metal; returns them in DebrisMeshes order."""
    EAL = unreal.EditorAssetLibrary
    if EAL.does_asset_exist(SHARD_MATERIAL):
        shard_material = unreal.load_asset(SHARD_MATERIAL)
    else:
        shard_material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'MI_WreckHoleShard', DEBRIS_FOLDER, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(shard_material, unreal.load_asset(SHARD_MATERIAL_PARENT))
        MEL.set_material_instance_scalar_parameter_value(shard_material, 'BurntDarkening', 1.0)
        EAL.save_loaded_asset(shard_material, False)
    shards = []
    for index, source in enumerate(DEBRIS_SOURCES):
        target = f'{DEBRIS_FOLDER}/SM_WreckHoleShard_{index}'
        mesh = unreal.load_asset(target) if EAL.does_asset_exist(target) else EAL.duplicate_asset(source, target)
        for slot in range(len(mesh.get_editor_property('static_materials'))):
            mesh.set_material(slot, shard_material)
        body = mesh.get_editor_property('body_setup')
        if body is not None:
            body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
        EAL.save_loaded_asset(mesh, False)
        shards.append(mesh)
    return shards


def _component_template(blueprint):
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        component = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, blueprint)
        if component is not None and component.get_class().get_name() == 'WreckHoleDamageComponent':
            return component
    return None


def setup_blueprint(blueprint_path, shards):
    """Adds the WreckHoleDamage component (once per hierarchy) and sets its map and shards. Returns a status."""
    blueprint = unreal.load_asset(blueprint_path)
    component = _component_template(blueprint)
    status = 'existing'
    if component is None:
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        root = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)[0]
        params = unreal.AddNewSubobjectParams(
            parent_handle=root, new_class=unreal.load_class(None, '/Script/RTS_Survival.WreckHoleDamageComponent'),
            blueprint_context=blueprint)
        handle, fail_reason = subsystem.add_new_subobject(params)
        if not fail_reason.is_empty():
            return 'add failed: ' + str(fail_reason)
        subsystem.rename_subobject(handle, unreal.Text(COMPONENT_NAME))
        component = _component_template(blueprint)
        status = 'added'
    component.set_editor_property('material_map', material_map())
    component.set_editor_property('debris_meshes', shards)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    return status


def setup_parent_blueprints():
    shards = build_debris()
    return {path: setup_blueprint(path, shards) for path in PARENT_BLUEPRINTS}
