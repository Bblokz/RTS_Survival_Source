"""Lamps for Breakthrough buildings that have their own import scripts (run with the UECP execute_python action).

For each building in BUILDINGS:
- **Concrete:** gives it its own concrete instance with its lamp pools on, `MI_BF_<Name>_Concrete`, a child of the faction's
  MI_BF_SootedFortress_UVTile, and points its main mesh's concrete slot at it.
- **Lamps mesh:** imports `<Mesh>_Lamps` from the Blender kit (SharedParts/bf_lamps.py) next to the main mesh.
- **Heat, soot and camo** (HQ, 2026-11-03): where BUILDINGS gives them, the same instance gets its soot spots from
  `<Mesh>_HeatSources.json` and the painted-camo layer with its mask.
- **Baked fields** (HQ, 2026-11-04): where BUILDINGS gives `fields`, the instance reads its heat, soot and lamp sources from
  one baked texture instead of looping over them (switch BF_UseBakedFields; bake with SharedParts/bf_fields.py from this
  instance's own source values, so bake after the heat and lamps are set, then run this again).
Copy `<Mesh>_Lamps.json` (and `<Mesh>_HeatSources.json`, `T_BF_<Name>_Fields.json`) from the Blender model folder next to this script first: the editor's Python only opens files inside the project.
If a building's own import script runs again, it puts the plain UVTile instance back on the concrete slot: run this after it.

    BUILDINGS_TO_RUN = ['HQ']; exec(open(r'<this file>').read())      # default: all
"""
import importlib
import json
import sys

import unreal

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/'
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import bf_ue_lamps  # noqa: E402
importlib.reload(bf_ue_lamps)     # the editor's Python keeps modules between runs

BLEND = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/'
SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
CHILD = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/'
MASKS = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/FlakBunker/Material/'
# Optional per building, on the same concrete instance:
# - heat: <Mesh>_HeatSources.json next to this script (soot over the firing slits, exhaust soot) plus the layer's scalars;
# - camo: the painted-camo layer's mask (any T_BF_CamoMask_*, see ProceduralSource/make_camo_masks.py) and scalars.
BUILDINGS = {
    'HQ': dict(home=CHILD + 'BreakthroughHQ/Mesh', mesh='SM_BF_HQ', model=BLEND + 'HQ/Model/',
               # muzzle soot rather than fire: faint heat, short reach, a plume that climbs about two metres, no firelight
               heat=dict(BF_HeatAmount=0.2, BF_HeatReachCm=50, BF_SootRiseCm=220, BF_SootAmount=0.85, BF_HeatGlow=0.0),
               camo=dict(mask='T_BF_CamoMask_Splitter', BF_CamoScaleCm=2400, BF_CamoSoftness=0.02, BF_CamoAmount=0.85),
               fields='T_BF_HQ_Fields',
               # user 2026-11-04: no crack lines on the HQ; the rebar reads the mesh's curvature UVs (SharedParts/bf_curvature.py)
               switches=dict(BF_UseCracks=False, BF_UseMeshCurvature=True)),
    'MechDepot': dict(home=CHILD + 'BreakthroughMechDepot/Mesh', mesh='SM_BF_MechDepot', model=BLEND + 'MechanizedDepot/Model/'),
    'ExperimentalFactory': dict(home=CHILD + 'BreakthroughExperimentalFactory/Mesh', mesh='SM_BF_ExperimentalFactory',
                                model=BLEND + 'ExperimentalUnitsFactory/Model/'),
}
CONCRETE_SLOT = 'M_BF_SootedFortress_Concrete'
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EDIT = unreal.MaterialEditingLibrary


def save(path):
    obj = unreal.load_asset(path)
    obj.modify()
    # save_asset and save_loaded_asset report False on some of these packages; save_packages doesn't
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + path)


def apply_heat(mi, json_path, scalars):
    data = json.loads(open(json_path, encoding='utf-8').read())
    EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseHeatSoot', True)
    for kind, count, param in (('heat', 8, 'BF_HeatSource'), ('soot', 2, 'BF_SootSource')):
        rows = data[kind]
        for k in range(count):
            row = rows[k] if k < len(rows) else dict(pos=[0, 0, 0], radius=0)
            EDIT.set_material_instance_vector_parameter_value(mi, f'{param}{k}', unreal.LinearColor(*row['pos'], row['radius']))
    for name, value in scalars.items():
        EDIT.set_material_instance_scalar_parameter_value(mi, name, value)
    print('heat sources', len(data['heat']), 'soot sources', len(data['soot']))


def apply_camo(mi, settings):
    EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseCamo', True)
    EDIT.set_material_instance_texture_parameter_value(mi, 'BF_CamoMask', unreal.load_asset(MASKS + settings['mask']))
    for name, value in settings.items():
        if name != 'mask':
            EDIT.set_material_instance_scalar_parameter_value(mi, name, value)
    print('camo', settings['mask'])


def apply_fields(mi, b):
    """The baked hot-spot fields (SharedParts/bf_fields.py): one texture read in place of the heat, soot and lamp loops.
    Bake them again whenever this instance's sources, BF_SootRiseCm or the mesh's LightMapUV change."""
    name = b['fields']
    task = unreal.AssetImportTask()
    task.filename = b['model'] + name + '.png'
    task.destination_path = b['home']
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    ASSET_TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f"{b['home']}/{name}")
    # exact 8-bit values, no mips (a lower mip would average neighbouring charts), clamped, never streamed
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    tex.set_editor_property('filter', unreal.TextureFilter.TF_BILINEAR)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    tex.set_editor_property('never_stream', True)
    save(f"{b['home']}/{name}")
    meta = json.loads(open(f'{TOOLS}{name}.json', encoding='utf-8').read())
    EDIT.set_material_instance_static_switch_parameter_value(mi, 'BF_UseBakedFields', True)
    EDIT.set_material_instance_texture_parameter_value(mi, 'BF_FieldsMap', tex)
    EDIT.set_material_instance_scalar_parameter_value(mi, 'BF_FieldsLampScale', meta['lamp_scale'])
    print('fields', name, 'lamp scale', meta['lamp_scale'])


def concrete_instance(name, b):
    path = f"{b['home']}/MI_BF_{name}_Concrete"
    if unreal.load_asset(path) is None:
        ASSET_TOOLS.create_asset(path.rsplit('/', 1)[1], b['home'], unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi = unreal.load_asset(path)
    EDIT.set_material_instance_parent(mi, unreal.load_asset(SHARED + '/MI_BF_SootedFortress_UVTile'))
    pools = bf_ue_lamps.apply_lamps(mi, f"{TOOLS}{b['mesh']}_Lamps.json")
    if 'heat' in b:
        apply_heat(mi, f"{TOOLS}{b['mesh']}_HeatSources.json", b['heat'])
    if 'camo' in b:
        apply_camo(mi, b['camo'])
    if 'fields' in b:
        apply_fields(mi, b)
    for switch, value in b.get('switches', {}).items():
        EDIT.set_material_instance_static_switch_parameter_value(mi, switch, value)
    EDIT.update_material_instance(mi)
    save(path)
    mesh = unreal.load_asset(f"{b['home']}/{b['mesh']}")
    slots = [i for i, s in enumerate(mesh.get_editor_property('static_materials'))
             if str(s.get_editor_property('material_slot_name')).startswith(CONCRETE_SLOT)]
    if not slots:
        raise RuntimeError(f"{b['mesh']} has no {CONCRETE_SLOT} slot")
    for i in slots:
        mesh.set_material(i, mi)     # setting it on the struct only changes a copy
    save(f"{b['home']}/{b['mesh']}")
    print(name, 'concrete instance', pools, 'lamp pools, slots', slots)


def import_lamps(b):
    name = b['mesh'] + '_Lamps'
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = b['model'] + name + '.FBX'
    task.destination_path = b['home']
    task.destination_name = name
    task.automated = True
    task.save = False    # AssetImportTask(save=True) silently misses the disk here
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        ASSET_TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(f"{b['home']}/{name}")
    lookup = dict(M_BF_Trim=SHARED + '/M_BF_Trim', **bf_ue_lamps.MATERIALS)
    for i, s in enumerate(mesh.get_editor_property('static_materials')):
        slot = str(s.get_editor_property('material_slot_name'))
        key = next(k for k in lookup if slot.startswith(k))
        mesh.set_material(i, unreal.load_asset(lookup[key]))
    mesh.set_editor_property('light_map_coordinate_index', 1)
    save(f"{b['home']}/{name}")
    print(name, 'tris', mesh.get_num_triangles(0))


for building in globals().get('BUILDINGS_TO_RUN', list(BUILDINGS)):
    concrete_instance(building, BUILDINGS[building])
    import_lamps(BUILDINGS[building])
