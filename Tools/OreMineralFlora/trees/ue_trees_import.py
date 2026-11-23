"""Imports the eight Ore Mountains mineral-flora trees into Unreal, headless. Safe to rerun after a Blender rebuild.

  powershell -File run_ue_cmd.ps1 -Script <this file> [-ScriptArgs "nobp" | "mat"] -NullRhi
    (no arg) textures, materials, meshes, Blueprints;  nobp: no Blueprints;  mat: textures + materials only

Creates, only under /Game/RTS_Survival/Blueprints/Environment/OreMountains/MineralFlora/Trees:
  Textures/T_OreFloraTrees_BC, _N, _RMA          one 2048 atlas for the eight pieces
  Materials/MI_OreFloraTrees_Wood, _Cards         INSTANCES of the Wpo masters M_Trunks_01 / M_Foilage_00 (wind on)
  Materials/MI_OreFloraTrees_Wood_Still, _Cards_Still   the same with wind off (the fallen log)
  Meshes/SM_OreFlora_*                            LODs from the Blender build, Nanite off, UCX trunk hulls
  BP_OreFlora_<Piece>                             standing pieces: duplicates of BP_OreDeadSpruce_Survivor_9m (AWpoTree:
                                                  tips over when crushed); the log: a duplicate of BP_OreCrushRock_Base
                                                  (sinks when crushed, NavBlock box), because a tip-over turns a lying
                                                  log on end.
Opens no map, edits no existing asset (duplicates only). Report: Saved/OreMountains/flora/trees/ue_import_report.json.
"""
import json
import math
import sys

import unreal

SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreMineralFlora/Trees/'
HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/MineralFlora/Trees'
WPO = '/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage'
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/ue_import_report.json'
TREE_SOURCE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/BP_OreDeadSpruce_Survivor_9m'
LOG_SOURCE = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Blueprints/BP_OreCrushRock_Base'
NAV_AREAS = '/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/'
SPLINTERS = ['/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckHoles/SM_WoodHoleSplinter_%d' % i for i in range(4)]
TREE_DEATH_CUE = '/Game/RTS_Survival/Sounds/DestructionAndCollapse/CollapsingTree/Cue_Collapse_Tree'
COLLAPSE_ATT = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
COLLAPSE_CONC = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'
LOD_SCREEN = [1.0, 0.45, 0.06]       # as the dead spruce kit: the default game frame shows LOD1
HEALTH = 300.0                        # as the dead spruce (user decision 2026-11-17: one shell kills a tree)
# Foliage card instance: the kit's values, Opacity_Instensity 3.0 (keeps the needle/leaf mass in the low mips),
# SubSurface_Colour warm instead of the kit's grey so back-lit cards stay red / yellow.
CARD = {'Roughness': 0.9, 'Specular': 0.0, 'SS_Intensity': 0.08, 'Brightness': 1.0, 'Variation': 0.0, 'Desaturation': 0.0, 'Opacity_Instensity': 3.0}
SSS = {'SubSurface_Colour': (0.32, 0.15, 0.05)}
SWAY = {'Trunk_Sway_Height': 1.5, 'Trunk_Sway_Intensity': 1.0, 'Trunk_Sway_Speed': 2.0, 'Trunk_Sway_Strength': 0.003}
STILL = {'Trunk_Sway_Height': 1.5, 'Trunk_Sway_Intensity': 0.0, 'Trunk_Sway_Speed': 2.0, 'Trunk_Sway_Strength': 0.0, 'Wind_intensity': 0.0, 'Additional_WPO': 0.0}
LOG = dict(sink_time=2.6, tip_speed=0.02, nav_pad=25.0, nav_min_top=80.0)

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
failures = []
report = dict(meshes={}, materials={}, blueprints={}, failures=failures)


def log(msg):
    unreal.log('FLORA_TREES ' + msg)


def save(obj):
    if not EAL.save_loaded_asset(obj, False):
        failures.append('save failed ' + obj.get_path_name())


def import_texture(name, srgb, compression, alpha_cover=False):
    task = unreal.AssetImportTask()
    task.filename = SRC + 'Model/Textures/' + name + '.png'
    task.destination_path = HOME + '/Textures'
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset('%s/Textures/%s' % (HOME, name))
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('compression_settings', compression)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    if alpha_cover:
        tex.set_editor_property('alpha_coverage_thresholds', unreal.Vector4(0.0, 0.0, 0.0, 0.3333))
        tex.set_editor_property('do_scale_mips_for_alpha_coverage', True)
    save(tex)
    return tex


def instance(name, parent_path, textures, scalars, vectors=None):
    path = '%s/Materials/%s' % (HOME, name)
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if mi is None:
        mi = TOOLS.create_asset(name, HOME + '/Materials', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    parent = unreal.load_asset(parent_path)
    mel.set_material_instance_parent(mi, parent)
    for k, v in textures.items():
        mel.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in scalars.items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1.0))
    mel.update_material_instance(mi)
    save(mi)
    for k, v in scalars.items():
        got = mel.get_material_instance_scalar_parameter_value(mi, k)
        if abs(got - v) > 1e-4:
            failures.append('%s: scalar %s reads back %s, wanted %s' % (name, k, got, v))
    report['materials'][name] = dict(asset=mi.get_path_name(), parent=parent.get_path_name(), scalars=scalars, vectors=vectors or {})
    return mi


def materials():
    C = unreal.TextureCompressionSettings
    bc = import_texture('T_OreFloraTrees_BC', True, C.TC_DEFAULT, alpha_cover=True)
    nr = import_texture('T_OreFloraTrees_N', False, C.TC_NORMALMAP)
    rma = import_texture('T_OreFloraTrees_RMA', False, C.TC_MASKS)
    return dict(
        wood=instance('MI_OreFloraTrees_Wood', WPO + '/MasterMats/M_Trunks_01', {'Albedo': bc, 'RMA': rma, 'Normal': nr}, dict(SWAY, Wind_intensity=0.03)),
        cards=instance('MI_OreFloraTrees_Cards', WPO + '/MasterMats/M_Foilage_00', {'Albedo': bc, 'Normal': nr}, dict(SWAY, Wind_intensity=0.05, **CARD), SSS),
        wood_still=instance('MI_OreFloraTrees_Wood_Still', WPO + '/MasterMats/M_Trunks_01', {'Albedo': bc, 'RMA': rma, 'Normal': nr}, STILL),
        cards_still=instance('MI_OreFloraTrees_Cards_Still', WPO + '/MasterMats/M_Foilage_00', {'Albedo': bc, 'Normal': nr}, dict(STILL, **CARD), SSS))


def fbx_options():
    ui = unreal.FbxImportUI()
    ui.set_editor_property('import_mesh', True)
    ui.set_editor_property('import_as_skeletal', False)
    ui.set_editor_property('import_materials', False)
    ui.set_editor_property('import_textures', False)
    ui.set_editor_property('import_animations', False)
    data = ui.get_editor_property('static_mesh_import_data')
    data.set_editor_property('combine_meshes', True)
    data.set_editor_property('auto_generate_collision', False)
    data.set_editor_property('generate_lightmap_u_vs', False)
    data.set_editor_property('build_nanite', False)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
    data.set_editor_property('one_convex_hull_per_ucx', True)
    return ui


def import_mesh(key, info, mats):
    name = info['mesh']
    task = unreal.AssetImportTask()
    task.filename = '%sModel/FBX/%s.fbx' % (SRC, name)
    task.destination_path = HOME + '/Meshes'
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = fbx_options()
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset('%s/Meshes/%s' % (HOME, name))
    if mesh is None:
        failures.append('import failed ' + name)
        return None
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    for lod in range(1, len(info['triangles'])):
        got = sub.import_lod(mesh, lod, '%sModel/FBX/%s_LOD%d.fbx' % (SRC, name, lod))
        if got != lod:
            failures.append('%s LOD%d import returned %s' % (name, lod, got))
    sub.set_lod_screen_sizes(mesh, LOD_SCREEN[:mesh.get_num_lods()])
    still = info['role'] == 'fallen'
    slots = []
    for i, sm in enumerate(mesh.static_materials):
        slot = str(sm.material_slot_name)
        kind = 'cards' if 'Card' in slot else 'wood'
        mesh.set_material(i, mats[kind + ('_still' if still else '')])
        slots.append(slot)
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    save(mesh)
    box = mesh.get_bounding_box()
    geom = body.get_editor_property('agg_geom')
    prims = {k: len(geom.get_editor_property(k)) for k in ('box_elems', 'convex_elems', 'sphere_elems', 'sphyl_elems')}
    got = dict(min=[round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)], max=[round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)])
    want = info['bounds_cm']
    err = max(abs(got[k][i] - want[k][i]) for k in ('min', 'max') for i in range(3))
    report['meshes'][key] = dict(asset=mesh.get_path_name(), bounds_cm=got, bounds_error_cm=round(err, 1),
                                 lod_triangles=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())], collision=prims, slots=slots,
                                 materials=[m.material_interface.get_path_name() for m in mesh.static_materials])
    if err > 3.0:
        failures.append('%s bounds differ from the Blender build by %.1f cm' % (key, err))
    if prims['convex_elems'] != info['collision_hulls']:
        failures.append('%s has %d convex hulls, the build wrote %d' % (key, prims['convex_elems'], info['collision_hulls']))
    return mesh


# ---------------------------------------------------------------- Blueprints
def components(bp):
    """name -> template; the gather lists an overridden inherited component more than once (dedupe by path)."""
    out, seen = {}, set()
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        data = SDL.get_data(h)
        o = SDL.get_object_for_blueprint(data, bp)
        if o is None or not isinstance(o, unreal.ActorComponent) or o.get_path_name() in seen:
            continue
        seen.add(o.get_path_name())
        out[o.get_name().replace('_GEN_VARIABLE', '')] = o
    return out


def area_class(name):
    cls = unreal.load_class(None, '%s%s.%s_C' % (NAV_AREAS, name, name))
    if cls is None:
        raise RuntimeError('nav area missing ' + name)
    return cls


def get_or_duplicate(name, source):
    path = '%s/%s' % (HOME, name)
    if EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    bp = EAL.duplicate_asset(source, path)
    if bp is None:
        raise RuntimeError('duplicate failed ' + path)
    return bp


def make_tree(key, info, mesh):
    name = 'BP_OreFlora_' + key
    bp = get_or_duplicate(name, TREE_SOURCE)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('TreeMesh', mesh)
    cdo.set_editor_property('TankDestructionType', getattr(unreal.CrushDestructionType, info['crush']))
    cdo.set_editor_property('Health', HEALTH)
    comps = components(bp)
    if 'Tree' not in comps:
        raise RuntimeError(name + ' has no Tree component: ' + str(list(comps)))
    comps['Tree'].set_editor_property('static_mesh', mesh)
    comps['Tree'].set_editor_property('override_materials', [])
    if 'Litter' in comps:
        comps['Litter'].set_editor_property('static_mesh', None)
    navs = [c for c in comps.values() if isinstance(c, unreal.NavModifierComponent)]
    if not navs:
        raise RuntimeError(name + ' has no nav modifier')
    for c in navs:
        c.set_editor_property('area_class', area_class(info['nav_area']))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    return bp


def make_log(key, info, mesh):
    name = 'BP_OreFlora_' + key
    bp = get_or_duplicate(name, LOG_SOURCE)
    comps = components(bp)
    for need in ('MainMesh', 'HealthComp', 'NavBlock', 'WallHoleDamage'):
        if need not in comps:
            raise RuntimeError('%s lacks %s: %s' % (name, need, list(comps)))
    mm = comps['MainMesh']
    mm.set_editor_property('static_mesh', mesh)
    mm.set_editor_property('override_materials', [])
    mm.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    mm.set_editor_property('can_ever_affect_navigation', False)
    mm.set_editor_property('generate_overlap_events', True)
    mm.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS)
    mm.set_collision_object_type(unreal.CollisionChannel.ECC_DESTRUCTIBLE)
    mm.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
    ch = unreal.CollisionChannel
    for c in (ch.ECC_BUILDING_PLACEMENT, ch.ECC_PLAYER_OBJ, ch.ECC_ENEMY_OBJ):
        mm.set_collision_response_to_channel(c, unreal.CollisionResponseType.ECR_OVERLAP)
    for c in (ch.ECC_VISIBILITY, ch.ECC_PROJECTILE, ch.ECC_ENEMY, ch.ECC_PLAYER):
        mm.set_collision_response_to_channel(c, unreal.CollisionResponseType.ECR_BLOCK)
    hc = comps['HealthComp']
    hc.set_editor_property('MaxHealth', HEALTH)
    hc.set_editor_property('CurrentHealth', HEALTH)
    red = hc.get_editor_property('DamageReductionSettings')
    red.set_editor_property('DamageReduction', 0.0)
    hc.set_editor_property('DamageReductionSettings', red)
    wh = comps['WallHoleDamage']
    wh.set_editor_property('WallMeshComponentName', 'MainMesh')
    wh.set_editor_property('DamagedMaterials', [])
    wh.set_editor_property('DebrisMeshes', [unreal.load_asset(p) for p in SPLINTERS])
    grid = wh.get_editor_property('GridSettings')
    grid.set_editor_property('max_pieces_per_hit', 2)
    grid.set_editor_property('max_collapse_pieces', 14)
    wh.set_editor_property('GridSettings', grid)
    col = wh.get_editor_property('CollapseSettings')
    col.set_editor_property('collapse_into_chunks_on_death', False)
    col.set_editor_property('destroy_owner_delay_seconds', 3.0)
    col.set_editor_property('collapse_sound', unreal.load_asset(TREE_DEATH_CUE))
    col.set_editor_property('collapse_attenuation', unreal.load_asset(COLLAPSE_ATT))
    col.set_editor_property('collapse_concurrency', unreal.load_asset(COLLAPSE_CONC))
    wh.set_editor_property('CollapseSettings', col)
    # nav box over the trunk (the hull) padded; the crown end's branches stay walkable for nobody but the crushers
    lo, hi = info['bounds_cm']['min'], info['bounds_cm']['max']
    trunk_len = lo[0] + (hi[0] - lo[0])
    nb = comps['NavBlock']
    hx = (info['hull_x_cm'][1] - info['hull_x_cm'][0]) / 2 + LOG['nav_pad']
    cx = (info['hull_x_cm'][1] + info['hull_x_cm'][0]) / 2
    hy = info['hull_halfwidth_cm'] + LOG['nav_pad']
    top = max(LOG['nav_min_top'], info['hull_top_cm'])
    nb.set_editor_property('relative_location', unreal.Vector(cx, 0.0, top / 2))
    nb.set_editor_property('relative_rotation', unreal.Rotator(roll=0, pitch=0, yaw=0))
    nb.set_editor_property('box_extent', unreal.Vector(hx, hy, top / 2))
    nb.set_editor_property('dynamic_obstacle', True)
    nb.set_editor_property('can_ever_affect_navigation', True)
    nb.set_editor_property('area_class_override', area_class(info['nav_area']))
    nb.set_editor_property('use_system_default_obstacle_area_class', False)
    nb.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    nb.set_editor_property('hidden_in_game', True)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    sink = round(hi[2] + 0.02 * trunk_len + 30.0)
    cdo.set_editor_property('Crushable', True)
    cdo.set_editor_property('Overlap Tanks', True)
    cdo.set_editor_property('CrushDestructionType', getattr(unreal.CrushDestructionType, info['crush']))
    cdo.set_editor_property('CrushTipSpeedScale', LOG['tip_speed'])
    cdo.set_editor_property('CrushSinkDeltaZ', float(sink))
    cdo.set_editor_property('CrushSinkTime', LOG['sink_time'])
    cdo.set_editor_property('health', HEALTH)
    cdo.set_editor_property('aim_offset_points', [unreal.Vector(x, 0.0, 30.0) for x in (100.0, 300.0, 500.0)])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(bp)
    report['log_nav_box'] = dict(cx=cx, hx=hx, hy=hy, top=top, sink=sink)
    return bp


def readback(key, info):
    path = '%s/BP_OreFlora_%s' % (HOME, key)
    bp = unreal.load_asset(path)
    if bp is None:
        return 'missing'
    cdo = unreal.get_default_object(bp.generated_class())
    d = dict(asset=path)
    for n in ('TankDestructionType', 'CrushDestructionType', 'Health', 'TreeMesh', 'Crushable', 'Overlap Tanks', 'CrushSinkDeltaZ', 'CrushTipSpeedScale'):
        try:
            d[n] = str(cdo.get_editor_property(n))[:160]
        except Exception:
            pass
    comps = components(bp)
    d['components'] = sorted(comps)
    for n, c in comps.items():
        if isinstance(c, unreal.StaticMeshComponent):
            sm = c.get_editor_property('static_mesh')
            d[n] = sm.get_name() if sm else None
        if isinstance(c, unreal.NavModifierComponent):
            d['nav_' + n] = c.get_editor_property('area_class').get_name()
        if isinstance(c, unreal.BoxComponent):
            ac = c.get_editor_property('area_class_override')
            d['nav_' + n] = (ac.get_name() if ac else None, str(c.get_editor_property('box_extent')))
    return d


# rev2 renamed four pieces to their new heights; the old-named assets (created by this script, referenced by nothing
# else, never placed) are deleted so no orphans remain.
RENAMED = ['RustSpruce_13m', 'OchreLarch_12m', 'RedBroadleaf_10m', 'OchreSnag_4m']


def delete_renamed():
    for key in RENAMED:
        for path in ('%s/BP_OreFlora_%s' % (HOME, key), '%s/Meshes/SM_OreFlora_%s' % (HOME, key)):
            if EAL.does_asset_exist(path):
                ok = EAL.delete_asset(path)
                report.setdefault('deleted', []).append((path, bool(ok)))
                if not ok:
                    failures.append('could not delete ' + path)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    pieces = json.load(open(SRC + 'OreFloraTrees_pieces.json'))['pieces']
    if 'mat' not in args:
        delete_renamed()
    mats = materials()
    if 'mat' in args:
        log('materials only')
    else:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
        meshes = {}
        try:
            for key, info in pieces.items():
                meshes[key] = import_mesh(key, info, mats)
        finally:
            unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
        if 'nobp' not in args:
            for key, info in pieces.items():
                if meshes.get(key) is None:
                    continue
                try:
                    if info['role'] == 'fallen':
                        make_log(key, info, meshes[key])
                    else:
                        make_tree(key, info, meshes[key])
                    report['blueprints'][key] = readback(key, info)
                except Exception as exc:  # noqa: BLE001
                    failures.append('%s blueprint: %r' % (key, exc))
    with open(REPORT, 'w') as f:
        json.dump(report, f, indent=1, default=str)
    log('done: %d meshes, %d blueprints, %d failures' % (len(report['meshes']), len(report['blueprints']), len(failures)))
    for line in failures:
        unreal.log_warning('FLORA_TREES failure: ' + line)


main()
