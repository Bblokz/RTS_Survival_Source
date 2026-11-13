"""Berlin apartment furniture (16 crushable pieces): textures, the one shared material, its shell-hole variant, meshes.

Headless (never in the shared editor):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
      -DisablePlugins=Fab,BpGeneratorUltimate
Idempotent: re-imports textures and FBX over the assets, rebuilds the master graph in place, re-makes the hole master.
Report: Saved/Berlin/furniture/import_report.json. Doc: Source/Docs/Environment/BerlinFurniture.md.
Then run furniture_blueprints_build.py.

Source (the modelling contract): D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinFurniture/
  FBX/SM_BerlinFurn_<NN>_<Name>.FBX (UCX hulls, one slot M_BerlinFurniture, pivot on the ground at the body footprint
  centre, +X front), Textures/T_BerlinFurniture_D / _N / _ORM.png (one 2048 atlas), furniture_table.json, READY.txt
  (written last: nothing is imported before it exists).

Makes, under /Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/:
  Textures/T_BerlinFurniture_D (sRGB), _N (normal map, DirectX green, not flipped), _ORM (linear masks: R AO, G roughness,
           B metallic)
  Materials/M_BerlinFurniture            opaque, default lit, 3 samples; FurnitureTint (colour multiplier)
  Materials/MI_BerlinFurniture           the instance every piece (and the debris) uses
  Materials/M_BerlinFurniture_WallHoles  masked hole copy, Tools/WallChunks/make_wall_holes_material.masked_master_for
  Materials/MI_BerlinFurniture_Holes     make_holes_instance(MI_BerlinFurniture, NoGrid, two-sided, inner tint): what
                                         UWallHoleDamageComponent swaps in on the first hit (DamagedMaterials)
  Meshes/SM_BerlinFurn_<NN>_<Name>       legacy FBX import (Interchange drops UCX), MI_BerlinFurniture on slot 0
  Meshes/SM_BerlinFurn_Debris_0..5       copies of the ruin kit debris with MI_BerlinFurniture (splinters for holes/death)
"""
import json
import os
import sys
import time

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.normpath(os.path.join(HERE, '..', '..', 'WallChunks')))
import make_wall_holes_material as WH  # noqa: E402

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinFurniture/'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture'
TEXTURES = ROOT + '/Textures'
MATERIALS = ROOT + '/Materials'
MESHES = ROOT + '/Meshes'
MASTER = MATERIALS + '/M_BerlinFurniture'
INSTANCE = MATERIALS + '/MI_BerlinFurniture'
HOLES_MASTER = MASTER + WH.SUFFIX
HOLES_INSTANCE_NAME = 'MI_BerlinFurniture_Holes'
SLOT = 'M_BerlinFurniture'
KIT_DEBRIS = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Debris/SM_BerlinRuin_Debris_%d'
DEBRIS_COUNT = 6
PLACEHOLDER_MESH = '/Engine/BasicShapes/Cube'   # NoGrid: the hole instance only needs bounds for unused grid params
# Charred wood seen through a hole in a hollow shell (wardrobe, piano); the library default.
HOLES_INNER_DARKEN = 0.75
HOLES_INNER_COLOR = (0.14, 0.085, 0.05)
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/furniture/import_report.json'
PROJECT_CONTENT = 'D:/UE5Projects/RTS_Survival/Content'

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
PROP = unreal.MaterialProperty
TEXTURE_SETUP = {
    'T_BerlinFurniture_D': dict(srgb=True, compression=unreal.TextureCompressionSettings.TC_DEFAULT),
    'T_BerlinFurniture_N': dict(srgb=False, compression=unreal.TextureCompressionSettings.TC_NORMALMAP),
    'T_BerlinFurniture_ORM': dict(srgb=False, compression=unreal.TextureCompressionSettings.TC_MASKS),
}


def log(message):
    unreal.log_warning('BERLIN_FURN ' + message)


def _save(asset):
    if not EAL.save_loaded_asset(asset, False):
        raise RuntimeError('save failed (Error moving file? asset open elsewhere): ' + asset.get_path_name())


def table():
    if not os.path.isfile(SOURCE + 'READY.txt'):
        raise RuntimeError('READY.txt missing in ' + SOURCE + ': the models are not final, nothing imported')
    return json.load(open(SOURCE + 'furniture_table.json'))


# ---------------------------------------------------------------- textures
def import_textures(info):
    if 'DirectX' not in info.get('normal_map_convention', ''):
        raise RuntimeError('normal map convention is not DirectX: ' + info.get('normal_map_convention', '?'))
    tasks = []
    for name in TEXTURE_SETUP:
        path = SOURCE + 'Textures/' + name + '.png'
        if not os.path.isfile(path):
            raise RuntimeError('missing texture ' + path)
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = TEXTURES
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    out = {}
    for name, cfg in TEXTURE_SETUP.items():
        tex = unreal.load_asset(TEXTURES + '/' + name)
        tex.set_editor_property('compression_settings', cfg['compression'])
        tex.set_editor_property('srgb', cfg['srgb'])
        tex.set_editor_property('flip_green_channel', False)
        tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        _save(tex)
        out[name] = tex
    log('textures imported: ' + ', '.join(out))
    return out


# ---------------------------------------------------------------- material
def build_master(textures):
    if EAL.does_asset_exist(MASTER):
        material = unreal.load_asset(MASTER)
        for _ in range(30):   # delete_all_material_expressions only removes about half per call
            if MEL.get_num_material_expressions(material) == 0:
                break
            MEL.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_BerlinFurniture', MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property('two_sided', False)
    material.set_editor_property('used_with_static_lighting', True)
    # The debris copies fly as Niagara mesh particles with the opaque instance.
    material.set_editor_property('used_with_niagara_mesh_particles', True)
    failed = []

    def sample(name, sampler, y):
        node = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSample, -700, y)
        node.set_editor_property('texture', textures[name])
        node.set_editor_property('sampler_type', sampler)
        return node

    def link(ok, what):
        if not ok:
            failed.append(what)

    colour = sample('T_BerlinFurniture_D', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -300)
    normal = sample('T_BerlinFurniture_N', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 0)
    orm = sample('T_BerlinFurniture_ORM', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, 300)
    tint = MEL.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -700, -520)
    tint.set_editor_property('parameter_name', 'FurnitureTint')
    tint.set_editor_property('default_value', unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    tinted = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, -400)
    link(MEL.connect_material_expressions(colour, 'RGB', tinted, 'A'), 'colour->tint')
    link(MEL.connect_material_expressions(tint, '', tinted, 'B'), 'tint->mul')
    link(MEL.connect_material_property(tinted, '', PROP.MP_BASE_COLOR), 'base colour')
    link(MEL.connect_material_property(normal, 'RGB', PROP.MP_NORMAL), 'normal')
    link(MEL.connect_material_property(orm, 'R', PROP.MP_AMBIENT_OCCLUSION), 'ao')
    link(MEL.connect_material_property(orm, 'G', PROP.MP_ROUGHNESS), 'roughness')
    link(MEL.connect_material_property(orm, 'B', PROP.MP_METALLIC), 'metallic')
    MEL.recompile_material(material)
    _save(material)
    if failed:
        raise RuntimeError('M_BerlinFurniture: failed links ' + str(failed))
    log('master built, %d expressions' % MEL.get_num_material_expressions(material))
    return material


def build_instance(master):
    if EAL.does_asset_exist(INSTANCE):
        mi = unreal.load_asset(INSTANCE)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'MI_BerlinFurniture', MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    MEL.update_material_instance(mi)
    _save(mi)
    return mi


def build_holes():
    """The masked hole variant, exactly by the shared library, as ruin_material_build.build_holes: the stale hole
    master is deleted and re-made so it never lags behind M_BerlinFurniture; the instance asset is kept."""
    holes_path = MATERIALS + '/' + HOLES_INSTANCE_NAME
    if EAL.does_asset_exist(HOLES_MASTER):
        if EAL.delete_asset(HOLES_MASTER):
            log('holes: deleted stale M_BerlinFurniture_WallHoles, instance kept')
        else:
            log('holes: FALLBACK, deleting the holes instance too (Blueprint references to it break until rebuilt)')
            EAL.delete_asset(holes_path)
            if not EAL.delete_asset(HOLES_MASTER):
                raise RuntimeError('could not delete stale ' + HOLES_MASTER)
    mi = WH.make_holes_instance(INSTANCE, MATERIALS, unreal.load_asset(PLACEHOLDER_MESH), grid_axis=None,
                                two_sided=True, name=HOLES_INSTANCE_NAME, inner_tint=True,
                                inner_darken=HOLES_INNER_DARKEN, inner_color=HOLES_INNER_COLOR)
    _save(mi)
    master = unreal.load_asset(HOLES_MASTER)
    names = sorted(str(n) for n in MEL.get_scalar_parameter_names(master) if str(n).startswith(('WallHole', 'WallGrid')))
    info = dict(master=master.get_name(), blend=str(master.get_editor_property('blend_mode')),
                instance_parent=mi.get_editor_property('parent').get_name(),
                two_sided=bool(mi.get_editor_property('base_property_overrides').get_editor_property('two_sided')),
                hole_parameters=len(names))
    log('holes: ' + json.dumps(info))
    return info


# ---------------------------------------------------------------- meshes
def import_mesh(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.IGNORE)
    task = unreal.AssetImportTask()
    task.filename = SOURCE + 'FBX/' + name + '.FBX'
    task.destination_path = MESHES
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def finish_mesh(piece, instance):
    name = piece['name']
    mesh = unreal.load_asset(MESHES + '/' + name)
    if mesh is None:
        raise RuntimeError('import made nothing for ' + name)
    slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    if slots != [SLOT]:
        raise RuntimeError(f'{name}: slots {slots}, expected [{SLOT}]')
    mesh.set_material(0, instance)
    mesh.set_editor_property('light_map_coordinate_index', 1)
    mesh.modify()
    unreal.EditorLoadingAndSavingUtils.save_packages([mesh.get_outermost()], False)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    box = mesh.get_bounding_box()
    size = [round(box.max.x - box.min.x, 1), round(box.max.y - box.min.y, 1), round(box.max.z - box.min.z, 1)]
    tris = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).get_number_verts(mesh, 0)
    row = dict(hulls=hulls, table_hulls=piece['hulls'], size=size, table_size=piece['size_cm'], lod0_verts=tris,
               bounds_min=[round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
               table_bounds_min=piece['bounds_min_cm'], material=mesh.get_material(0).get_name())
    problems = []
    if hulls != piece['hulls']:
        problems.append(f'hulls {hulls} != table {piece["hulls"]}')
    if any(abs(a - b) > 1.5 for a, b in zip(size, piece['size_cm'])):
        problems.append(f'size {size} != table {piece["size_cm"]}')
    if any(abs(a - b) > 1.5 for a, b in zip(row['bounds_min'], piece['bounds_min_cm'])):
        problems.append(f'bounds min {row["bounds_min"]} != table {piece["bounds_min_cm"]} (mirrored?)')
    row['problems'] = problems
    return row


def make_debris(instance):
    out = []
    for index in range(DEBRIS_COUNT):
        target = f'{MESHES}/SM_BerlinFurn_Debris_{index}'
        mesh = unreal.load_asset(target) if EAL.does_asset_exist(target) else EAL.duplicate_asset(KIT_DEBRIS % index, target)
        if mesh is None:
            raise RuntimeError('could not copy ' + KIT_DEBRIS % index)
        for slot in range(len(mesh.get_editor_property('static_materials'))):
            mesh.set_material(slot, instance)
        _save(mesh)
        out.append(mesh.get_name())
    return out


def on_disk(package_path, since):
    path = PROJECT_CONTENT + package_path[len('/Game'):] + '.uasset'
    return os.path.isfile(path) and os.path.getmtime(path) >= since - 2


def run():
    start = time.time()
    info = table()
    report = {'errors': [], 'meshes': {}}
    textures = import_textures(info)
    master = build_master(textures)
    instance = build_instance(master)
    report['holes'] = build_holes()
    stats = MEL.get_statistics(master)
    report['master_stats'] = dict(samplers=stats.num_samplers, pixel_texture_samples=stats.num_pixel_texture_samples)
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for piece in info['pieces']:
            import_mesh(piece['name'])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    for piece in info['pieces']:
        try:
            row = finish_mesh(piece, instance)
            report['meshes'][piece['name']] = row
            if row['problems']:
                report['errors'].append(f'{piece["name"]}: {row["problems"]}')
        except Exception as error:  # noqa: BLE001
            report['errors'].append(f'{piece["name"]}: {error!r}')
    report['debris'] = make_debris(instance)
    written = [TEXTURES + '/' + n for n in TEXTURE_SETUP] + [MASTER, INSTANCE, HOLES_MASTER,
                                                              MATERIALS + '/' + HOLES_INSTANCE_NAME]
    written += [MESHES + '/' + p['name'] for p in info['pieces']] + [f'{MESHES}/{n}' for n in report['debris']]
    missing = [p for p in written if not on_disk(p, start)]
    report['on_disk'] = f'{len(written) - len(missing)}/{len(written)}'
    if missing:
        report['errors'].append('not written this run: ' + str(missing))
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as handle:
        json.dump(report, handle, indent=1)
    log(f'done: {len(report["meshes"])} meshes, on disk {report["on_disk"]}, errors {report["errors"]}, report {REPORT}')
    return report


if __name__ == '__main__':
    run()
