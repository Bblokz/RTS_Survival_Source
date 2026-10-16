"""Imports the B-4 gun pit assets: the gun's pit animations, the crew animations and the pit props.

Run inside the editor after ue_b4_import.py:  import ue_b4_pit_import; ue_b4_pit_import.run_all()
Blender sources: b4_pit_animations.py, b4_pit_crew.py and b4_pit_props.py (AnimTeamWeapons/B4_203mm).

The game does not render Nanite: after (re)importing a pit or fieldwork mesh, cut it with
Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py and run Tools/GunPits/optimize_pit_meshes.py heavy, which
imports the cut mesh and turns Nanite off (see Docs/Units/GunPitMeshes.md).
"""
import json
import os

import unreal

import ue_b4_import as base

PIT_DIR = base.ROOT + '/Pit'
PIT_ANIM_DIR = PIT_DIR + '/Anims'
CREW_DIR = PIT_DIR + '/Crew'
PROP_DIR = PIT_DIR + '/Meshes'
INFANTRY_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton'
SOURCE_FBX_DIR = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/B4203MM'
CREW_COUNT = 4
LOADING_CURVE = 'B4_LoadingElevation'

PROP_MATERIALS = {
    'MI_B4Pit_Concrete': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/MI_Rus_BunkerConcrete_05',
    'MI_B4Pit_Earth': '/Game/RTS_Survival/Blueprints/Environment/GroundEnvActors/Desert/MI_RTS_DesertGround_Stones',
    'MI_B4Pit_Timber': '/Game/RTS_Survival/StarterContent/Materials/M_Wood_Pine',
    'MI_B4Pit_Steel': '/Game/RTS_Survival/StarterContent/Materials/M_Metal_Burnished_Steel',
    # The sandbags of the game's own Soviet sandbag walls.
    'MI_B4Pit_Sandbag': '/Game/SandBagPack/Materials/MI_Rus_SandBag_01',
}
eal = unreal.EditorAssetLibrary


def reimport_gun_rig():
    """The rig gained the rammer and crane cable bones; reimport in place so every reference stays valid."""
    filename = os.path.join(base.EXPORT_DIR, f'{base.MESH_NAME}.fbx')
    mesh = unreal.load_asset(f'{base.MESH_DIR}/{base.MESH_NAME}')
    options = base._skeletal_options(mesh.get_editor_property('skeleton'))
    options.create_physics_asset = False
    base._run_import(base._make_task(filename, base.MESH_DIR, base.MESH_NAME, options))
    mesh = unreal.load_asset(f'{base.MESH_DIR}/{base.MESH_NAME}')
    base.assign_materials(mesh)
    base.import_lods()
    for anim_name, folder in base.ANIM_FOLDERS.items():
        asset_name = f'Anim_B4_{anim_name}'
        filename = os.path.join(base.EXPORT_DIR, 'Anims', f'{asset_name}.fbx')
        base._run_import(base._make_task(filename, folder, asset_name, base._anim_options(mesh.get_editor_property('skeleton'))))
    base.setup_aim_poses()


def _import_anim(filename, folder, asset_name, skeleton):
    # Reimporting over a sequence of another length/rate fails; these assets are generated only, so start clean.
    if eal.does_asset_exist(f'{folder}/{asset_name}'):
        eal.delete_asset(f'{folder}/{asset_name}')
    return base._run_import(base._make_task(filename, folder, asset_name, base._anim_options(skeleton)))


def import_pit_anims():
    skeleton = unreal.load_asset(f'{base.MESH_DIR}/{base.MESH_NAME}').get_editor_property('skeleton')
    for name in ('Idle', 'Fire', 'Reload'):
        asset_name = f'Anim_B4Pit_{name}'
        _import_anim(os.path.join(base.EXPORT_DIR, 'PitAnims', asset_name + '.fbx'), PIT_ANIM_DIR, asset_name, skeleton)
    add_loading_curve()


def add_loading_curve():
    reload_anim = unreal.load_asset(f'{PIT_ANIM_DIR}/Anim_B4Pit_Reload')
    with open(os.path.join(base.EXPORT_DIR, 'b4_pit_curves.json')) as handle:
        keys = json.load(handle)[LOADING_CURVE]
    library = unreal.AnimationLibrary
    if library.does_curve_exist(reload_anim, LOADING_CURVE, unreal.RawCurveTrackTypes.RCT_FLOAT):
        library.remove_curve(reload_anim, LOADING_CURVE)
    library.add_curve(reload_anim, LOADING_CURVE, unreal.RawCurveTrackTypes.RCT_FLOAT)
    library.add_float_curve_keys(reload_anim, LOADING_CURVE, [k[0] for k in keys], [k[1] for k in keys])
    eal.save_loaded_asset(reload_anim)


def import_crew_anims():
    skeleton = unreal.load_asset(INFANTRY_SKELETON)
    for index in range(1, CREW_COUNT + 1):
        for kind in ('Idle', 'Reload'):
            asset_name = f'Anim_B4PitCrew{index}_{kind}'
            filename = os.path.join(base.EXPORT_DIR, 'Crew', 'Anims', asset_name + '.fbx')
            _import_anim(filename, CREW_DIR, asset_name, skeleton)
            sequence = unreal.load_asset(f'{CREW_DIR}/{asset_name}')
            # The crew walk around the pit through their root bone; the meshes are not characters.
            sequence.set_editor_property('enable_root_motion', False)
            eal.save_loaded_asset(sequence)


def _static_options():
    options = unreal.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.static_mesh_import_data.set_editor_property('combine_meshes', True)
    options.static_mesh_import_data.set_editor_property('auto_generate_collision', True)
    options.static_mesh_import_data.set_editor_property('generate_lightmap_u_vs', False)
    return options


def _assign_static_materials(mesh, mapping):
    # Editing the static_materials copy and writing it back did not persist; set_material marks the mesh dirty.
    for index, entry in enumerate(mesh.static_materials):
        target = mapping.get(str(entry.material_slot_name))
        if target:
            mesh.set_material(index, unreal.load_asset(target))
    eal.save_asset(mesh.get_path_name(), only_if_is_dirty=False)


def import_pit_prop(name):
    base._run_import(base._make_task(os.path.join(base.EXPORT_DIR, 'PitProps', name + '.fbx'), PROP_DIR, name,
                                     _static_options()))
    mesh = unreal.load_asset(f'{PROP_DIR}/{name}')
    if name == 'SM_B4Pit_Sandbags':
        # About 75k triangles of pack bags: thinned with distance by the large-prop LOD group.
        mesh.set_editor_property('lod_group', 'LargeProp')
    _assign_static_materials(mesh, PROP_MATERIALS)


def import_sandbags():
    """Reimports only the sandbag parapet, e.g. after changing build_sandbags in b4_pit_props.py."""
    import_pit_prop('SM_B4Pit_Sandbags')


def import_props():
    gun_materials = {f'MI_B4_{s}': f'{base.MATERIAL_DIR}/MI_B4_{s}' for s in base.MATERIAL_SETS}
    for name in ('SM_B4Pit_Emplacement', 'SM_B4Pit_Turntable', 'SM_B4Pit_ShellRack', 'SM_B4Pit_Rammer',
                 'SM_B4Pit_CartHandle', 'SM_B4Pit_Sandbags'):
        import_pit_prop(name)
    for name in ('SM_Cartridge', 'SM_Trolley', 'SM_Crane'):
        asset_name = name.replace('SM_', 'SM_B4_')
        base._run_import(base._make_task(os.path.join(SOURCE_FBX_DIR, name + '.FBX'), PROP_DIR, asset_name,
                                         _static_options()))
        _assign_static_materials(unreal.load_asset(f'{PROP_DIR}/{asset_name}'), gun_materials)


def run_all():
    reimport_gun_rig()
    import_pit_anims()
    import_crew_anims()
    import_props()
    unreal.log('B4 pit import done')
