"""Imports the Breakthrough HQ rail crane's Mechanized Depot actions (run with the UECP execute_uecp_script action).

Blender source and rebuild: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/HQ/Cranes/Model/README.md
(crane_build.py rail_depot writes SK_BF_HQ_RailCrane_Depot.FBX: the HQ rail crane's mesh and skeleton with only the depot actions).

Animation-only import onto the existing SK_BF_HQ_RailCrane_Skeleton, so the crane mesh and the HQ's own A_RailCrane_* stay as
they are. The legacy importer names the takes <file>_Anim_Armature_<action> and brings a mesh copy along, so it imports into a
scratch folder, moves the six sequences next to the HQ ones as A_RailCrane_Depot_* and deletes the rest.
On a rerun each new sequence replaces the old one in every reference (montage, AnimBP) before it takes its name.

Run with the UECP execute_uecp_script action. Then it makes, or refreshes, the depot's copies of the HQ crane's AnimBP and montage:
  ABP_BF_RailCrane_Depot      ABP_BF_RailCrane with A_RailCrane_Depot_Idle as the pose between montages.
  AM_RailCrane_Depot_Working  AM_RailCrane_Working's layout on the depot actions. The depot actions have the HQ frame
                              counts, so the sections keep the HQ times: Out 0, Work 13.43 s (Work twice), Stop 37.43 s.
Afterwards run ue_mech_depot_blueprints.py (the crane attachment and the truck's socket).
"""
import unreal

FBX = r'D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\GerBreakthroughBuildings\HQ\Cranes\Model\SK_BF_HQ_RailCrane_Depot.FBX'
RAIL = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughHQ/Cranes/Rail'
SKELETON = RAIL + '/SK_BF_HQ_RailCrane_Skeleton'
SCRATCH = '/Game/__RailCraneDepotImport'
ACTIONS = ('Idle', 'RollOut', 'Unstow', 'Work', 'Stow', 'RollIn')
HQ_ABP, ABP = RAIL + '/ABP_BF_RailCrane', RAIL + '/ABP_BF_RailCrane_Depot'
HQ_MONTAGE, MONTAGE = RAIL + '/AM_RailCrane_Working', RAIL + '/AM_RailCrane_Depot_Working'
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def save(path):
    if not ASSETS.save_loaded_asset(unreal.load_asset(path), False):
        raise RuntimeError('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):
        raise RuntimeError('not on disk ' + disk)


def legacy_import(task):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 1')


def import_actions():
    if ASSETS.does_directory_exist(SCRATCH):
        ASSETS.delete_directory(SCRATCH)
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_as_skeletal', True)
    ui.set_editor_property('import_animations', True)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_ANIMATION)
    ui.set_editor_property('original_import_type', unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    ui.set_editor_property('skeleton', unreal.load_asset(SKELETON))
    ui.set_editor_property('create_physics_asset', False)
    anim = ui.get_editor_property('anim_sequence_import_data')
    anim.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    anim.set_editor_property('remove_redundant_keys', False)
    task = unreal.AssetImportTask()
    task.filename = FBX
    task.destination_path = SCRATCH
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = ui
    legacy_import(task)
    found = {}
    for path in ASSETS.list_assets(SCRATCH, recursive=True):
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.AnimSequence) and '_A_RailCrane_Depot_' in asset.get_name():
            found[asset.get_name().split('_A_RailCrane_Depot_')[-1]] = asset
    if sorted(found) != sorted(ACTIONS):
        raise RuntimeError(f'expected {ACTIONS}, imported {sorted(found)}')
    for key in ACTIONS:
        new, target = found[key], f'{RAIL}/A_RailCrane_Depot_{key}'
        if ASSETS.does_asset_exist(target):
            old = unreal.load_asset(target)
            if not ASSETS.consolidate_assets(new, [old]):
                raise RuntimeError('could not replace ' + target)
            redirector = unreal.load_asset(target)
            if redirector is not None and ASSETS.does_asset_exist(target):
                TOOLS.fixup_referencers([redirector])
                if ASSETS.does_asset_exist(target):
                    ASSETS.delete_asset(target)
        if not ASSETS.rename_asset(new.get_path_name(), target):
            raise RuntimeError('could not move to ' + target)
        save(target)
        seq = unreal.load_asset(target)
        print(target, 'length', round(seq.get_play_length(), 3), 'skeleton', seq.get_editor_property('skeleton').get_name())
    ASSETS.delete_directory(SCRATCH)


def duplicate(source, target):
    if ASSETS.does_asset_exist(target):
        return unreal.load_asset(target)
    made = ASSETS.duplicate_asset(source, target)
    if made is None:
        raise RuntimeError(f'could not copy {source} to {target}')
    return made


def montage(uecp):
    """The HQ montage's layout on the depot actions: RollOut + Unstow (Out), Work twice (Work, loops), Stow + RollIn (Stop).
    Python can't edit montage segments, so this goes through the uecp.animation actions. A rerun keeps the existing montage:
    import_actions() already pointed its segments at the new sequences."""
    if not ASSETS.does_asset_exist(MONTAGE):
        uecp.animation.create_montage(source_animation=f'{RAIL}/A_RailCrane_Depot_RollOut', name=MONTAGE.split('/')[-1], save_path=RAIL)
        start = unreal.load_asset(f'{RAIL}/A_RailCrane_Depot_RollOut').get_play_length()
        for key in ('Unstow', 'Work', 'Work', 'Stow', 'RollIn'):
            start = uecp.animation.link_montage_slot(montage_path=MONTAGE, slot_name='DefaultSlot',
                                                     animation_path=f'{RAIL}/A_RailCrane_Depot_{key}', start_time=start)['play_length']
        hq = {s['name']: s['time'] for s in uecp.animation.get_montage_summary(montage_path=HQ_MONTAGE)['sections']}
        uecp.animation.rename_montage_section(montage_path=MONTAGE, old_name='Default', new_name='Out')
        uecp.animation.add_montage_section(montage_path=MONTAGE, section_name='Work', start_time=hq['Work'])
        uecp.animation.add_montage_section(montage_path=MONTAGE, section_name='Stop', start_time=hq['Stop'])
        uecp.animation.set_montage_section_link(montage_path=MONTAGE, section_name='Out', next_section_name='Work')
        uecp.animation.set_montage_section_link(montage_path=MONTAGE, section_name='Work', next_section_name='Work')
    save(MONTAGE)
    print('montage', uecp.animation.get_montage_summary(montage_path=MONTAGE)['sections'])


def anim_blueprint(uecp):
    """ABP_BF_RailCrane with the depot's stowed pose between montages, so the crane never jumps to the HQ's stow."""
    abp = duplicate(HQ_ABP, ABP)
    nodes = uecp.animation.get_anim_graph_nodes(anim_blueprint_path=ABP, include_nested=True)['nodes']
    player = next(n for n in nodes if n['node_class'] == 'AnimGraphNode_SequencePlayer')
    uecp.animation.set_anim_node_property(anim_blueprint_path=ABP, node_guid=player['node_guid'], property_name='Sequence',
                                          value=f'{RAIL}/A_RailCrane_Depot_Idle.A_RailCrane_Depot_Idle')
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    save(ABP)
    print('ABP idle', ABP)


import_actions()
UECP = globals().get('uecp')
if UECP is None:
    raise RuntimeError('sequences imported; run with execute_uecp_script to make the montage and the AnimBP')
anim_blueprint(UECP)
montage(UECP)
