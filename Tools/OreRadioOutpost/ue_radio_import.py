"""Imports the radio outpost model (concept 5A Funkbude) into Unreal, headless. Safe to rerun after a Blender rebuild.

  sh run_ue_py.sh ue_radio_build.py <log> -nullrhi        (imports, then builds the hut Blueprint)

Creates, only under /Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/RadioOutpost:
  Textures/T_OreRadio_Case_BC     case atlas (dark green, red seal bands, yellow BERLIN stencil), from orp_stencil.py
  Materials/MI_OreRadio_<slot>    TarRoof, Mast, RedPaint, Tarp, Case: instances of the adit master M_OreAdit_Surface
                                  with existing project textures (the other slots reuse existing instances, MATS)
  Meshes/SM_OreRadio_*            Funkbude, Mast, CaseLeanTo, CaseStack: legacy FBX import (keeps the UCX hulls),
                                  LODs, Nanite off, lightmap UVs generated
Opens no map. Writes Saved/OreMountains/radio/ue_import_report.json.
"""
import json
import os

import unreal

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreRadioOutpost/Model/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/RadioOutpost'
ORE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
MASTER = ORE + 'AditPortals/Materials/M_OreAdit_Surface'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/radio/ue_import_report.json'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
failures = []
report = dict(meshes={}, materials={}, failures=failures)

MS = '/Game/Megascans/Surfaces/'
GI = '/Game/Greek_island/Textures/'
TEX = {
    'sheet': (MS + 'Rusty_Painted_Metal_Sheet_vckjca2/T_Rusty_Painted_Metal_Sheet_vckjca2_2K_D',
              MS + 'Rusty_Painted_Metal_Sheet_vckjca2/T_Rusty_Painted_Metal_Sheet_vckjca2_2K_N',
              MS + 'Rusty_Painted_Metal_Sheet_vckjca2/T_RustyPaintedMetalSheet_vckjca2_2K_ORDp'),
    'fabric': (GI + 'T_fabric/T_fabric_D', GI + 'T_fabric/T_fabric_N', GI + 'T_fabric/T_fabric_AORM'),
}
# new instances: slot -> (texture set or 'case', scalars, vectors). Mesh UVs are 2 m per tile (cases: 0-1 atlas).
NEW_MI = {
    'TarRoof': ('fabric', dict(UVScale=2.0, Desaturate=1.0, RoughScale=1.0), dict(Tint=(0.085, 0.085, 0.08))),
    'Mast': ('sheet', dict(UVScale=2.0, Desaturate=0.85, Metallic=0.3, RoughScale=1.0), dict(Tint=(0.62, 0.64, 0.65))),
    'RedPaint': ('sheet', dict(UVScale=2.0, Desaturate=1.0, Metallic=0.15, RoughScale=1.0), dict(Tint=(0.95, 0.16, 0.1))),
    'Tarp': ('fabric', dict(UVScale=2.0, Desaturate=1.0, RoughScale=1.0), dict(Tint=(0.33, 0.36, 0.2))),
    'Case': ('case', dict(UVScale=1.0, NormalStrength=0.0, Metallic=0.25, RoughScale=0.9), dict(Tint=(1.0, 1.0, 1.0))),
}
# Blender slot -> material instance
MATS = {
    'M_OreRadio_Timber': ORE + 'AditPortals/Materials/MI_OreAdit_Timber',
    'M_OreRadio_Steel': ORE + 'AditPortals/Materials/MI_OreAdit_Steel',
    'M_OreRadio_White': ORE + 'AditPortals/Materials/MI_OreAdit_Paint',
    'M_OreRadio_Wire': ORE + 'AditPortals/Materials/MI_OreAdit_Throat',
    'M_OreRadio_Concrete': ORE + 'A4Viaduct/MI_A4_Concrete',
    'M_OreRadio_Roof': HOME + '/Materials/MI_OreRadio_TarRoof',
    'M_OreRadio_Mast': HOME + '/Materials/MI_OreRadio_Mast',
    'M_OreRadio_Red': HOME + '/Materials/MI_OreRadio_RedPaint',
    'M_OreRadio_Tarp': HOME + '/Materials/MI_OreRadio_Tarp',
    'M_OreRadio_Case': HOME + '/Materials/MI_OreRadio_Case',
}
PIECES = ['SM_OreRadio_Funkbude', 'SM_OreRadio_Mast', 'SM_OreRadio_CaseLeanTo', 'SM_OreRadio_CaseStack']
# screen size ~ 1.78 * bounds radius / distance (FOV 90, 16:9); the default camera is ~49 m from the site, so the hut
# (r 5.4 m) is ~0.2 there, the lean-to ~0.15, the stack ~0.05, the mast ~0.8. LOD1 only well beyond the game camera;
# the case meshes keep LOD0 (reduction would wreck the atlas UVs on box-built cases).
LODS = {'SM_OreRadio_Funkbude': ((1.0, 1.0), (0.5, 0.07), (0.25, 0.035)),
        'SM_OreRadio_Mast': ((1.0, 1.0), (0.5, 0.2)),
        'SM_OreRadio_CaseLeanTo': ((1.0, 1.0),),
        'SM_OreRadio_CaseStack': ((1.0, 1.0),)}


def log(*a):
    unreal.log_warning('ORE_RADIO_IMPORT ' + ' '.join(str(x) for x in a))


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        failures.append('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'
    if not os.path.isfile(disk):
        failures.append('not on disk ' + disk)


def import_texture():
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'Textures/T_OreRadio_Case_BC.png'
    task.destination_path = HOME + '/Textures'
    task.destination_name = 'T_OreRadio_Case_BC'
    task.automated = True
    task.save = False
    task.replace_existing = True
    TOOLS.import_asset_tasks([task])
    path = HOME + '/Textures/T_OreRadio_Case_BC'
    tex = unreal.load_asset(path)
    if tex is None:
        failures.append('texture import failed')
        return None
    tex.set_editor_property('srgb', True)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    save(path)
    return tex


def instance(slot, master, master_pk, case_tex):
    key, scalars, vectors = NEW_MI[slot]
    name = 'MI_OreRadio_' + slot
    path = '%s/Materials/%s' % (HOME, name)
    mi = unreal.load_asset(path)
    if mi is None:
        mi = TOOLS.create_asset(name, HOME + '/Materials', unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.clear_all_material_instance_parameters(mi)
    if key == 'case':
        mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTex', case_tex)
    else:
        bc, nr, pk = (unreal.load_asset(p) for p in TEX[key])
        if None in (bc, nr, pk):
            failures.append('%s: missing texture of set %s' % (slot, key))
            return None
        mel.set_material_instance_texture_parameter_value(mi, 'BaseColorTex', bc)
        mel.set_material_instance_texture_parameter_value(mi, 'NormalTex', nr)
        cs = str(pk.get_editor_property('compression_settings'))
        if 'MASKS' in cs.upper():
            mel.set_material_instance_texture_parameter_value(mi, 'PackedMasksTex', pk)
            mel.set_material_instance_scalar_parameter_value(mi, 'UseMasksPacked', 1.0)
        elif cs == str(master_pk.get_editor_property('compression_settings')) and \
                pk.get_editor_property('srgb') == master_pk.get_editor_property('srgb'):
            mel.set_material_instance_texture_parameter_value(mi, 'PackedTex', pk)
        else:
            failures.append('%s: packed texture %s matches neither packed parameter' % (slot, pk.get_name()))
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in vectors.items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    mel.update_material_instance(mi)
    save(path)
    bc_now = mel.get_material_instance_texture_parameter_value(mi, 'BaseColorTex')
    report['materials'][slot] = dict(asset=mi.get_path_name(), base_color=bc_now.get_name() if bc_now else None)
    log('MI', slot, 'BaseColorTex =', bc_now.get_name() if bc_now else None)
    if key == 'case' and (bc_now is None or bc_now.get_name() != 'T_OreRadio_Case_BC'):
        failures.append('MI_OreRadio_Case does not carry the stencil atlas')
    return mi


def fbx_ui():
    ui = unreal.FbxImportUI()
    for k, v in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                     import_animations=False).items():
        ui.set_editor_property(k, v)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property('static_mesh_import_data')
    for k, v in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                     generate_lightmap_u_vs=True, build_nanite=False, remove_degenerates=True).items():
        d.set_editor_property(k, v)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return ui


def import_mesh(name, expect, want_hulls):
    task = unreal.AssetImportTask()
    task.filename = MODEL + 'FBX/%s.fbx' % name
    task.destination_path = HOME + '/Meshes'
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = fbx_ui()
    TOOLS.import_asset_tasks([task])
    path = '%s/Meshes/%s' % (HOME, name)
    mesh = unreal.load_asset(path)
    if mesh is None:
        failures.append('import failed ' + name)
        return
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    opts = unreal.StaticMeshReductionOptions()
    opts.auto_compute_lod_screen_size = False
    red = []
    for pct, screen in LODS[name]:
        r = unreal.StaticMeshReductionSettings()
        r.percent_triangles = pct
        r.screen_size = screen
        red.append(r)
    opts.reduction_settings = red
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_lods(mesh, opts)
    slots = mesh.get_editor_property('static_materials')
    names = []
    for i, sm in enumerate(slots):
        sname = str(sm.get_editor_property('material_slot_name'))
        names.append(sname)
        mi = unreal.load_asset(MATS[sname]) if sname in MATS else None
        if mi is None:
            failures.append('%s slot %s has no material' % (name, sname))
            continue
        mesh.set_material(i, mi)
    save(path)
    box = mesh.get_bounding_box()
    got = dict(min=[round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
               max=[round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)])
    err = max(abs(got[k][i] - expect[k][i]) for k in ('min', 'max') for i in range(3))
    body = mesh.get_editor_property('body_setup')
    geom = body.get_editor_property('agg_geom') if body else None
    convex = len(geom.get_editor_property('convex_elems')) if geom else 0
    report['meshes'][name] = dict(asset=mesh.get_path_name(), bounds_cm=got, expected_cm=expect, bounds_error_cm=round(err, 1),
                                  lod_triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                                  convex_hulls=convex, slots=names,
                                  materials=[mesh.get_material(i).get_name() if mesh.get_material(i) else None
                                             for i in range(len(slots))])
    if err > 3.0:
        failures.append('%s bounds differ from the Blender build by %.1f cm (axis or scale wrong)' % (name, err))
    if convex != want_hulls:
        failures.append('%s: %d convex hulls, Blender wrote %d' % (name, convex, want_hulls))


def main():
    model = json.load(open(MODEL + 'OreRadioOutpost_model.json'))
    master = unreal.load_asset(MASTER)
    master_pk = unreal.load_asset(TEX['fabric'][2])        # the master's PackedTex default is the fabric AORM map
    case_tex = import_texture()
    for slot in NEW_MI:
        instance(slot, master, master_pk, case_tex)
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for name in PIECES:
            p = model['pieces'][name]
            lo, hi = p['bounds_m']['min'], p['bounds_m']['max']
            # design frame (x east, y north) -> Unreal cm (X north, Y east)
            expect = dict(min=[round(lo[1] * 100, 1), round(lo[0] * 100, 1), round(lo[2] * 100, 1)],
                          max=[round(hi[1] * 100, 1), round(hi[0] * 100, 1), round(hi[2] * 100, 1)])
            import_mesh(name, expect, p['ucx'])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    os.makedirs(os.path.dirname(REPORT), exist_ok=True)
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1)
    log('failures=%d %s' % (len(failures), json.dumps(failures)))
    for k, v in report['meshes'].items():
        log('%s err=%.1fcm hulls=%d lods=%s mats=%s' % (k, v['bounds_error_cm'], v['convex_hulls'], v['lod_triangles'],
                                                        v['materials']))


main()
