"""Brewery enclave POI for Ger_Berlin: textures, materials (with shell-hole variants), meshes and destructible Blueprints.

Blender source (generators, validator, report):
    D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinBreweryEnclave/Model/
Headless (never in the shared editor):
    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
        -DisablePlugins=Fab,BpGeneratorUltimate
Idempotent: re-run after a Blender rebuild. Report: Saved/Berlin/brewery/import_report.json.
Doc: Source/Docs/Environment/BerlinBreweryEnclave.md.

Everything lives in /Game/RTS_Survival/Blueprints/Environment/Berlin/BreweryEnclave/{Textures,Materials,Meshes,Blueprints}:
- T_BerlinBrewery_{Copper,Tile}_{D,N}: 2048 tileable, 1 tile = 4 m (UV0), D alpha = roughness.
- M_BerlinBrewery_Glaze: opaque master for the two signature looks; reads vertex colour R (soot) and B (dust) like
  M_BerlinRuin, so the copper and tile sit in the same grime as the brick. MI_BerlinBrewery_Copper / _Tile.
- Shell holes (WallChunkDamage.md "A + B"): MI_BerlinBrewery_{Copper,Tile}_WallHoles on the masked copy
  M_BerlinBrewery_Glaze_WallHoles (two-sided: kettles, roof sheet and tile linings are shells). Brick uses the kit's
  MI_BerlinRuin_Wall_Holes. Rubble and the worn steel (thin rafters, rails) take no holes.
- SM_Berlin_BreweryHall / SM_Berlin_BreweryVaults: legacy FBX import (UCX hulls), vertex colours REPLACE, lightmap UV1.
- BP_Berlin_BreweryHall / BP_Berlin_BreweryVaults: children of the ruin kit's BP_BerlinRuin_Base, built with the kit's
  own builder functions (ruinkit/ruin_blueprints_build.py): StaticMesh + WallHoleDamage + NavBlock boxes, and the death
  values the base graph reads. The death: a small kit-debris burst, a few brown dust puffs (NS_Artillery_StoneImpact),
  a short sink into the ground with a small random turn, then the actor stays as a solid dead remnant (no heap).
"""
import importlib.util
import json
import os

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/Berlin/BreweryEnclave'
TEX, MATS, MESHES, BPS = HOME + '/Textures', HOME + '/Materials', HOME + '/Meshes', HOME + '/Blueprints'
MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinBreweryEnclave/Model/'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/brewery/import_report.json'
RUIN = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/'
STEEL = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/M_BF_Crane_WornSteel'
HOLES_TOOL = 'D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_holes_material.py'
RUIN_BUILDER = 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/ruin_blueprints_build.py'
MASTER = 'M_BerlinBrewery_Glaze'
LOOKS = {   # MI name: (texture key, metallic, roughness multiplier, dust amount, soot amount)
    'MI_BerlinBrewery_Copper': ('Copper', 0.35, 1.0, 0.55, 0.6),
    'MI_BerlinBrewery_Tile': ('Tile', 0.0, 1.0, 0.5, 0.6),
}
# FBX slot name -> intact material
SLOT_MATERIAL = {
    'M_BerlinRuin_Wall': RUIN + 'MI_BerlinRuin_Wall',
    'M_BerlinRuin_Rubble': RUIN + 'MI_BerlinRuin_Rubble',
    'M_BerlinBrewery_Copper': MATS + '/MI_BerlinBrewery_Copper',
    'M_BerlinBrewery_Tile': MATS + '/MI_BerlinBrewery_Tile',
    'M_BF_Crane_WornSteel': STEEL,
}
# FBX slot name -> damaged (holes) material; None = this slot never shows a hole
SLOT_HOLES = {
    'M_BerlinRuin_Wall': RUIN + 'MI_BerlinRuin_Wall_Holes',
    'M_BerlinRuin_Rubble': None,
    'M_BerlinBrewery_Copper': MATS + '/MI_BerlinBrewery_Copper_WallHoles',
    'M_BerlinBrewery_Tile': MATS + '/MI_BerlinBrewery_Tile_WallHoles',
    'M_BF_Crane_WornSteel': None,
}
# Pieces: (mesh, Blueprint, hole/size class of the kit, health, damage reduction, death row)
PUFF = '/Game/RTS_Survival/ImpactSfxVfx/5Artillery/Stone/NS_Artillery_StoneImpact'
PIECES = {
    'BreweryHall': dict(kind='hero', health=6000, reduction=15, death=dict(
        sink=0.12, time=2.5, tilt=(1.0, 2.5), yaw=(1.0, 3.0), fx=PUFF, fx_scale=1.5, bursts=3, fx_speed=0.6,
        heap_fx=1.0, debris=20, debris_speed=0.6, heap=False)),
    'BreweryVaults': dict(kind='core', health=7000, reduction=20, death=dict(
        sink=0.18, time=1.8, tilt=(0.5, 1.5), yaw=(1.0, 3.0), fx=PUFF, fx_scale=1.3, bursts=3, fx_speed=0.55,
        heap_fx=1.0, debris=14, debris_speed=0.5, heap=False)),
}
# The vault field's hulls are wide half-cylinders, which the kit's aim-point rule reads as heaps; aim at the vault
# haunches on the east (+Y) side and at the fermentation wall instead (mesh cm).
VAULT_AIMS = [(0, -990, 170), (300, -230, 170), (-300, 530, 170), (200, 1290, 170), (-500, -1560, 260)]

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
REP = {'errors': []}


def log(message):
    unreal.log_warning('BERLIN_BREWERY ' + message)


def _load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _save(asset):
    if not EAL.save_loaded_asset(asset, False):
        raise RuntimeError('save failed ' + asset.get_path_name())


# ---------------------------------------------------------------- textures

def import_textures():
    tasks = []
    for key in ('Copper', 'Tile'):
        for kind in ('D', 'N'):
            name = f'T_BerlinBrewery_{key}_{kind}'
            task = unreal.AssetImportTask()
            task.filename = MODEL + f'Textures/{name}.png'
            task.destination_path = TEX
            task.destination_name = name
            task.automated = True
            task.replace_existing = True
            task.save = False
            tasks.append((task, name, kind))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t for t, _, _ in tasks])
    out = {}
    for _, name, kind in tasks:
        tex = unreal.load_asset(f'{TEX}/{name}')
        if tex is None:
            raise RuntimeError('texture import failed ' + name)
        if kind == 'N':
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('srgb', False)
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
        else:
            tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_BC7)
            tex.set_editor_property('srgb', True)
            tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
        _save(tex)
        out[name] = tex
    log(f'textures {sorted(out)}')
    return out


# ---------------------------------------------------------------- materials

def _expr(material, cls, x, y):
    return MEL.create_material_expression(material, cls, x, y)


def _param(material, cls, name, x, y, default=None):
    e = _expr(material, cls, x, y)
    e.set_editor_property('parameter_name', name)
    if default is not None:
        e.set_editor_property('default_value', default)
    return e


def build_master(textures):
    path = f'{MATS}/{MASTER}'
    if EAL.does_asset_exist(path):
        log('master exists, kept (delete it and its _WallHoles copy to rebuild the graph)')
        return unreal.load_asset(path)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(MASTER, MATS, unreal.Material,
                                                                         unreal.MaterialFactoryNew())
    base = _param(material, unreal.MaterialExpressionTextureSampleParameter2D, 'BaseTex', -1200, -200)
    base.set_editor_property('texture', textures['T_BerlinBrewery_Copper_D'])
    nrm = _param(material, unreal.MaterialExpressionTextureSampleParameter2D, 'NormalTex', -1200, 300)
    nrm.set_editor_property('texture', textures['T_BerlinBrewery_Copper_N'])
    nrm.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    vc = _expr(material, unreal.MaterialExpressionVertexColor, -1200, 700)
    soot_amt = _param(material, unreal.MaterialExpressionScalarParameter, 'SootAmount', -900, 600, 0.6)
    dust_amt = _param(material, unreal.MaterialExpressionScalarParameter, 'DustAmount', -900, 800, 0.5)
    soot_col = _param(material, unreal.MaterialExpressionVectorParameter, 'SootColor', -700, 0,
                      unreal.LinearColor(0.03, 0.025, 0.02, 1.0))
    dust_col = _param(material, unreal.MaterialExpressionVectorParameter, 'DustColor', -500, 100,
                      unreal.LinearColor(0.40, 0.37, 0.33, 1.0))
    metal = _param(material, unreal.MaterialExpressionScalarParameter, 'Metallic', -300, 400, 0.0)
    rough_mul = _param(material, unreal.MaterialExpressionScalarParameter, 'RoughnessMul', -900, 400, 1.0)
    soot_k = _expr(material, unreal.MaterialExpressionMultiply, -700, 600)
    MEL.connect_material_expressions(vc, 'R', soot_k, 'A')
    MEL.connect_material_expressions(soot_amt, '', soot_k, 'B')
    dust_k = _expr(material, unreal.MaterialExpressionMultiply, -700, 800)
    MEL.connect_material_expressions(vc, 'B', dust_k, 'A')
    MEL.connect_material_expressions(dust_amt, '', dust_k, 'B')
    l1 = _expr(material, unreal.MaterialExpressionLinearInterpolate, -450, -150)
    MEL.connect_material_expressions(base, 'RGB', l1, 'A')
    MEL.connect_material_expressions(soot_col, '', l1, 'B')
    MEL.connect_material_expressions(soot_k, '', l1, 'Alpha')
    l2 = _expr(material, unreal.MaterialExpressionLinearInterpolate, -250, -100)
    MEL.connect_material_expressions(l1, '', l2, 'A')
    MEL.connect_material_expressions(dust_col, '', l2, 'B')
    MEL.connect_material_expressions(dust_k, '', l2, 'Alpha')
    rough = _expr(material, unreal.MaterialExpressionMultiply, -700, 250)
    MEL.connect_material_expressions(base, 'A', rough, 'A')
    MEL.connect_material_expressions(rough_mul, '', rough, 'B')
    dust_rough = _expr(material, unreal.MaterialExpressionMultiply, -500, 900)
    MEL.connect_material_expressions(vc, 'B', dust_rough, 'A')
    dust_rough.set_editor_property('const_b', 0.9)
    rmax = _expr(material, unreal.MaterialExpressionMax, -300, 250)
    MEL.connect_material_expressions(rough, '', rmax, 'A')
    MEL.connect_material_expressions(dust_rough, '', rmax, 'B')
    MEL.connect_material_property(l2, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rmax, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(nrm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.recompile_material(material)
    _save(material)
    log('master built ' + path)
    return material


def build_instances(master, textures):
    out = {}
    for name, (key, metallic, rough_mul, dust, soot) in LOOKS.items():
        path = f'{MATS}/{name}'
        mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else \
            unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MATS, unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        MEL.set_material_instance_texture_parameter_value(mi, 'BaseTex', textures[f'T_BerlinBrewery_{key}_D'])
        MEL.set_material_instance_texture_parameter_value(mi, 'NormalTex', textures[f'T_BerlinBrewery_{key}_N'])
        MEL.set_material_instance_scalar_parameter_value(mi, 'Metallic', metallic)
        MEL.set_material_instance_scalar_parameter_value(mi, 'RoughnessMul', rough_mul)
        MEL.set_material_instance_scalar_parameter_value(mi, 'DustAmount', dust)
        MEL.set_material_instance_scalar_parameter_value(mi, 'SootAmount', soot)
        MEL.update_material_instance(mi)
        _save(mi)
        out[name] = mi
    return out


def build_hole_instances(any_mesh):
    """Masked, two-sided hole copies of the copper and tile instances (no grid: free holes, as on the ruin kit)."""
    tool = {}
    exec(open(HOLES_TOOL).read(), tool)
    out = {}
    for name in LOOKS:
        mi = tool['make_holes_instance'](f'{MATS}/{name}', MATS, any_mesh, grid_axis=None, two_sided=True)
        out[name] = mi.get_path_name()
    masked = unreal.load_asset(f'{MATS}/{MASTER}_WallHoles')
    try:
        live = tool['check_mask_live'](masked)
    except Exception as error:  # noqa: BLE001
        live = repr(error)
    REP['holes_mask_ps_instructions_masked_vs_opaque'] = live
    log(f'hole instances {out}; mask check (masked, opaque PS) {live}')
    return out


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
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = MESHES
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(f'{MESHES}/{name}')
    if mesh is None:
        raise RuntimeError('import made nothing for ' + name)
    slots = []
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in SLOT_MATERIAL if slot_name.startswith(k)), None)
        if key is None:
            raise RuntimeError(f'{name}: no material for slot {slot_name}')
        mesh.set_material(index, unreal.load_asset(SLOT_MATERIAL[key]))
        slots.append(key)
    sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if sms.get_num_uv_channels(mesh, 0) >= 2:
        mesh.set_editor_property('light_map_coordinate_index', 1)
    _save(mesh)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    box = mesh.get_bounding_box()
    info = dict(slots=slots, hulls=hulls, uv_channels=sms.get_num_uv_channels(mesh, 0),
                vertex_colors=bool(sms.has_vertex_colors(mesh)), verts=sms.get_number_verts(mesh, 0),
                bounds_cm=[round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)])
    log(f'{name}: {json.dumps(info)}')
    if hulls == 0:
        REP['errors'].append(f'{name}: no UCX hulls imported')
    return mesh, slots, info


# ---------------------------------------------------------------- Blueprints (the ruin kit's builder, our rows)

def build_blueprint(rb, base, suffix, mesh, slots, pieces):
    spec = PIECES[suffix]
    rb.DEATH_CLASS_OF[suffix] = 'brewery_' + suffix
    rb.DEATH['brewery_' + suffix] = spec['death']
    kind = spec['kind']
    name = 'BP_Berlin_' + suffix
    hulls = rb.hull_boxes(mesh)
    boxes = rb.nav_boxes(hulls)
    if suffix == 'BreweryVaults':
        aims = [unreal.Vector(*p) for p in VAULT_AIMS]
    else:
        aims = rb.aim_points(hulls, rb.CLASSES[kind]['aims']) if hulls else [unreal.Vector(0, 0, 300)]
    bp, created = rb.get_or_create_blueprint(name, base)
    named = rb.ensure_components(bp, len(boxes))
    rb.setup_mesh_template(named['StaticMesh'][1], mesh)
    holes = named['WallHoleDamage'][1]
    rb.setup_holes_template(holes, kind, False, pieces, True, suffix)
    damaged = [unreal.load_asset(SLOT_HOLES[s]) if SLOT_HOLES[s] else None for s in slots]
    holes.set_editor_property('DamagedMaterials', damaged)
    keep = rb.remnant_nav_flags(suffix, mesh, hulls, boxes)
    for index, box in enumerate(boxes):
        rb.setup_nav_box(named['NavBlock_%02d' % index][1], box, keep[index])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('health', float(spec['health']))
    cdo.set_editor_property('damage_reduction', float(spec['reduction']))
    cdo.set_editor_property('aim_offset_points', aims)
    values, death_info = rb.death_defaults(suffix, mesh)
    for key, value in values.items():
        cdo.set_editor_property(key, value)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    _save(bp)
    info = dict(blueprint=bp.get_path_name(), created=created, kind=kind, health=spec['health'],
                reduction=spec['reduction'], damaged_materials=[d.get_name() if d else None for d in damaged],
                nav_boxes=[[round(v) for v in b] for b in boxes], nav_kept_under_remnant='%d/%d' % (sum(keep), len(keep)),
                aims=[[a.x, a.y, a.z] for a in aims], death=death_info)
    log(f'{name}: {json.dumps(info, default=str)}')
    return info


def run():
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    textures = import_textures()
    master = build_master(textures)
    build_instances(master, textures)
    meshes = {}
    for suffix in PIECES:
        meshes[suffix] = import_mesh('SM_Berlin_' + suffix)
        REP.setdefault('meshes', {})[suffix] = meshes[suffix][2]
    REP['hole_instances'] = build_hole_instances(meshes['BreweryHall'][0])
    rb = _load_module(RUIN_BUILDER, 'ruin_blueprints_build')
    rb.BLUEPRINTS = BPS
    base = unreal.load_asset(f'/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/{rb.BASE_NAME}')
    if base is None or not rb.base_death_ready(base):
        raise RuntimeError('BP_BerlinRuin_Base missing or without its death graph variables')
    pieces = rb.debris_meshes()
    for suffix, (mesh, slots, _) in meshes.items():
        try:
            REP.setdefault('blueprints', {})[suffix] = build_blueprint(rb, base, suffix, mesh, slots, pieces)
        except Exception as error:  # noqa: BLE001
            REP['errors'].append(f'{suffix}: {error!r}')
            log(f'{suffix}: ERROR {error!r}')
    with open(REPORT, 'w') as handle:
        json.dump(REP, handle, indent=1, default=str)
    log('done; report ' + REPORT)
    return REP


if __name__ == '__main__':
    run()
