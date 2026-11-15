"""Phytophage-infested drowned trees -> /Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/Trees.

Source: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Phytophage/Trees (generate_phyto_trees.py builds the three
stages on SM_RTS_DeadTree_1 as exported from UE; generate_phyto_tree_textures.py the tileable textures).

Run headless only (no level loaded; the clean re-import force-deletes the old meshes, wrappers and master, which is
only safe when nothing in an open level or undo buffer references them):
    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
        -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Order: phyto_trees_export_source.py (once) -> Blender generate_phyto_tree_textures.py, generate_phyto_trees.py,
validate_phyto_trees.py -> this file -> manifest_phyto_trees.py. PHYTO_REBUILD_MATERIAL = False keeps the master.

Creates
    T_PhytoTree_Masks (R wet, G mould, B cavity, A hair; linear, Masks), T_PhytoTree_Normal (DirectX normal)
    M_PhytoTree        opaque two-sided master: wet gloss map + macro wetness, panning normal blended over the base
                       normal, Fresnel dry fungal hairs on grazing edges, very slow sine WPO weighted by vertex colour R
                       (0 on the wood), slow vein / pod pulse along the normal (vertex colour G), Radixite shards
                       (vertex colour B), necrotic pods (vertex colour A). ISM/HISM usage on.
    MI_PhytoTree       the instance the meshes use (all tints / speeds exposed)
    SM_PhytoTree_Creeping / _Shrouded / _Cocooned   slot 0 "Tree" = MI_Env_DeadTree_1 (shared, unchanged), slot 1
                       = MI_PhytoTree; 3 LODs at the source's screen sizes; the source's 6 convex hulls,
                       simple-as-complex; Nanite off; lightmap UV 2 at 64 like the source.
    BP_PhytoTree_<Stage> crushable wrappers: duplicates of BP_PolesieDeadSpruce_02_BogLean (AWpoTree; the map's own tree
                       kit pattern) with TreeMesh / Tree component set to the variant. The source tree's own wrapper
                       (BP_Scorch_Dest_DeadTree) picks a random mesh of trees 1-3 in its construction script, so a
                       mesh-swapped copy of it would not keep the variant.
"""
import json
import os

import unreal

PT_SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Phytophage/Trees/'
PT_HOME = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/Trees'
PT_BARK = '/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/Materials/MI_Env_DeadTree_1'
PT_SOURCE_MESH = '/Game/RTS_Survival/Meshes/Nature/Trees/DeadTrees/SM_RTS_DeadTree_1'
PT_BP_SOURCE = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces/BP_PolesieDeadSpruce_02_BogLean'
PT_BLEND_NORMALS = '/Engine/Functions/Engine_MaterialFunctions02/Utility/BlendAngleCorrectedNormals'
PT_STAGES = ['Creeping', 'Shrouded', 'Cocooned']
PT_LOD_SCREEN = [1.0, 0.45, 0.26]
PT_REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/phyto_trees/import_report.json'
# linear tints, at or under MI_Pripyat_Phytophage_Dull (the map's wall growth): deep dirty purple, the gloss does the work
PT_VECTORS = dict(DarkTint=(0.010, 0.0035, 0.013), MidTint=(0.05, 0.015, 0.063), MouldTint=(0.075, 0.062, 0.068),
                  PodTint=(0.014, 0.003, 0.018), HairTint=(0.13, 0.11, 0.14), CrystalTint=(0.42, 0.11, 0.015))
PT_SCALARS = dict(Tiling=1.0, MacroScale=0.19, MacroDark=0.35, MouldAmount=1.3, PodDarkness=0.85, HairExponent=3.0,
                  HairAmount=1.2, DryRoughness=0.62, WetRoughness=0.1, NormalStrength=0.85, CrystalGlow=0.12,
                  WobbleSpeed=0.45, WobbleAmp=5.0, PulseSpeed=1.1, PulseAmp=1.6)
PT_PAN = (0.006, 0.013)          # UV per second: the slime film creeps, it does not flow

_EAL = unreal.EditorAssetLibrary
_EDIT = unreal.MaterialEditingLibrary
_SUB = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PT_LOG = []


def _log(*a):
    line = ' '.join(str(x) for x in a)
    PT_LOG.append(line)
    print(line)
    unreal.log_warning('PHYTO_TREES ' + line)


# ------------------------------------------------------------------------------------------------------------ textures
def _texture(png, name, normal):
    path = PT_HOME + '/' + name
    task = unreal.AssetImportTask()
    task.filename = PT_SRC + png
    task.destination_path = PT_HOME
    task.destination_name = name
    task.automated, task.save, task.replace_existing = True, False, True
    _TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if normal
                            else unreal.TextureCompressionSettings.TC_MASKS)
    if normal:
        tex.set_editor_property('flip_green_channel', False)
    _EAL.save_loaded_asset(tex, False)
    return tex


# ------------------------------------------------------------------------------------------------------------ material
class _Graph:
    def __init__(self, material):
        self.m = material

    def node(self, cls, **props):
        n = _EDIT.create_material_expression(self.m, cls)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(self, src, dst, pin):
        out = ''
        if isinstance(src, tuple):
            src, out = src
        if not _EDIT.connect_material_expressions(src, out, dst, pin):
            raise RuntimeError('link failed %s.%s -> %s.%s' % (src.get_name(), out, dst.get_name(), pin))

    def scalar(self, name):
        return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=PT_SCALARS[name])

    def vector(self, name):
        r, g, b = PT_VECTORS[name]
        return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name,
                         default_value=unreal.LinearColor(r, g, b, 1.0))

    def const(self, v):
        return self.node(unreal.MaterialExpressionConstant, r=v)

    def op(self, cls, a, b):
        n = self.node(cls)
        self.link(a, n, 'A')
        self.link(b, n, 'B')
        return n

    def mul(self, a, b):
        return self.op(unreal.MaterialExpressionMultiply, a, b)

    def add(self, a, b):
        return self.op(unreal.MaterialExpressionAdd, a, b)

    def lerp(self, a, b, alpha):
        n = self.node(unreal.MaterialExpressionLinearInterpolate)
        self.link(a, n, 'A')
        self.link(b, n, 'B')
        self.link(alpha, n, 'Alpha')
        return n

    def sat(self, a):
        n = self.node(unreal.MaterialExpressionSaturate)
        self.link(a, n, '')
        return n

    def tex(self, name, texture, uv, normal):
        n = self.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=name, texture=texture,
                      sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal
                      else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        self.link(uv, n, 'UVs')
        return n

    def out(self, src, prop):
        o = ''
        if isinstance(src, tuple):
            src, o = src
        if not _EDIT.connect_material_property(src, o, prop):
            raise RuntimeError('output failed %s' % prop)


_WPO_HLSL = '''
float ph = dot(WP, float3(0.0041, 0.0033, 0.0019));
float t = T * Speed;
float3 sway = float3(sin(t + ph) * 0.35, cos(t * 0.8 + ph * 1.3) * 0.35, sin(t * 0.7 + ph * 0.9)) * Amp * R;
float pulse = (0.5 + 0.5 * sin(T * PulseSpeed - WP.z * 0.012)) * PulseAmp * G;
return sway + N * pulse;
'''


def _master(masks, normal):
    path = PT_HOME + '/M_PhytoTree'
    if _EAL.does_asset_exist(path):
        if not globals().get('PHYTO_REBUILD_MATERIAL', True):
            return unreal.load_asset(path)
        _EAL.delete_asset(path)
    m = _TOOLS.create_asset('M_PhytoTree', PT_HOME, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('two_sided', True)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    g = _Graph(m)
    uv = g.mul(g.node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0), g.scalar('Tiling'))
    uv_macro = g.mul(uv, g.scalar('MacroScale'))
    tm = g.tex('Masks', masks, uv, False)
    tmm = g.tex('MasksMacro', masks, uv_macro, False)
    n1 = g.tex('Normal', normal, uv, True)
    pan = g.node(unreal.MaterialExpressionPanner, speed_x=PT_PAN[0], speed_y=PT_PAN[1])
    g.link(g.mul(uv, g.const(1.6)), pan, 'Coordinate')
    n2 = g.tex('NormalPan', normal, pan, True)
    blend = g.node(unreal.MaterialExpressionMaterialFunctionCall)
    blend.set_editor_property('material_function', unreal.load_asset(PT_BLEND_NORMALS))
    g.link((n1, 'RGB'), blend, 'BaseNormal')
    g.link((n2, 'RGB'), blend, 'AdditionalNormal')
    flat = g.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 1, 1))
    g.out(g.lerp(flat, blend, g.scalar('NormalStrength')), unreal.MaterialProperty.MP_NORMAL)
    vc = g.node(unreal.MaterialExpressionVertexColor)
    # base colour
    c = g.lerp(g.vector('DarkTint'), g.vector('MidTint'), (tm, 'B'))
    c = g.mul(c, g.lerp(g.scalar('MacroDark'), g.const(1.2), (tmm, 'B')))
    c = g.lerp(c, g.vector('MouldTint'), g.sat(g.mul((tm, 'G'), g.scalar('MouldAmount'))))
    c = g.lerp(c, g.vector('PodTint'), g.mul((vc, 'A'), g.scalar('PodDarkness')))
    fres = g.node(unreal.MaterialExpressionFresnel, base_reflect_fraction=0.0)
    g.link(g.scalar('HairExponent'), fres, 'ExponentIn')
    hair = g.sat(g.mul(g.mul(fres, (tm, 'A')), g.scalar('HairAmount')))
    c = g.lerp(c, g.vector('HairTint'), hair)
    crystal = g.vector('CrystalTint')
    c = g.lerp(c, crystal, (vc, 'B'))
    g.out(c, unreal.MaterialProperty.MP_BASE_COLOR)
    # wetness gloss: fine wet film in the folds + big slick patches from the macro sample
    wet = g.sat(g.add(g.add(g.mul((tm, 'R'), g.const(0.75)), g.mul((tmm, 'R'), g.const(0.5))), g.const(-0.15)))
    r = g.lerp(g.scalar('DryRoughness'), g.scalar('WetRoughness'), wet)
    r = g.lerp(r, g.const(0.12), (vc, 'G'))
    r = g.lerp(r, g.const(0.95), hair)
    r = g.lerp(r, g.const(0.28), (vc, 'B'))
    g.out(r, unreal.MaterialProperty.MP_ROUGHNESS)
    g.out(g.mul(g.mul(crystal, (vc, 'B')), g.scalar('CrystalGlow')), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # WPO: very slow sway of the free growth (R = 0 on the wood) + vein / pod pulse travelling up (G)
    cust = g.node(unreal.MaterialExpressionCustom, code=_WPO_HLSL,
                  output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3, description='PhytoWPO')
    names = ['R', 'G', 'WP', 'N', 'T', 'Speed', 'Amp', 'PulseSpeed', 'PulseAmp']
    ins = []
    for nm in names:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', nm)
        ins.append(ci)
    cust.set_editor_property('inputs', ins)
    g.link((vc, 'R'), cust, 'R')
    g.link((vc, 'G'), cust, 'G')
    g.link(g.node(unreal.MaterialExpressionWorldPosition), cust, 'WP')
    g.link(g.node(unreal.MaterialExpressionVertexNormalWS), cust, 'N')
    g.link(g.node(unreal.MaterialExpressionTime), cust, 'T')
    g.link(g.scalar('WobbleSpeed'), cust, 'Speed')
    g.link(g.scalar('WobbleAmp'), cust, 'Amp')
    g.link(g.scalar('PulseSpeed'), cust, 'PulseSpeed')
    g.link(g.scalar('PulseAmp'), cust, 'PulseAmp')
    # MaterialProperty has no WPO entry in Python: the project's editor library wires it (as the Berlin/Rus-gate tools)
    if not unreal.NomadicDeployEditorLibrary.connect_world_position_offset(cust, ''):
        raise RuntimeError('WPO connection failed')
    _EDIT.layout_material_expressions(m)
    _EDIT.recompile_material(m)
    _EAL.save_loaded_asset(m, False)
    _log('master', path, 'expressions', _EDIT.get_num_material_expressions(m))
    return m


def _instance(master):
    path = PT_HOME + '/MI_PhytoTree'
    mi = unreal.load_asset(path) if _EAL.does_asset_exist(path) else _TOOLS.create_asset(
        'MI_PhytoTree', PT_HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    _EDIT.set_material_instance_parent(mi, master)
    for k, (r, g, b) in PT_VECTORS.items():
        _EDIT.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(r, g, b, 1.0))
    for k, v in PT_SCALARS.items():
        _EDIT.set_material_instance_scalar_parameter_value(mi, k, v)
    _EDIT.update_material_instance(mi)
    _EAL.save_loaded_asset(mi, False)
    return mi


# --------------------------------------------------------------------------------------------------------------- meshes
def _fbx_ui():
    ui = unreal.FbxImportUI()
    for k, v in dict(import_mesh=True, import_as_skeletal=False, import_materials=False, import_textures=False,
                     import_animations=False).items():
        ui.set_editor_property(k, v)
    ui.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
    d = ui.get_editor_property('static_mesh_import_data')
    # the FBX carries the source's six hulls as UCX_<name>_00..05, one convex each
    for k, v in dict(combine_meshes=True, auto_generate_collision=False, one_convex_hull_per_ucx=True,
                     generate_lightmap_u_vs=False, build_nanite=False, remove_degenerates=True).items():
        d.set_editor_property(k, v)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    return ui


def _mesh(stage, bark_mi, phyto_mi):
    name = 'SM_PhytoTree_' + stage
    for old in (PT_HOME + '/BP_PhytoTree_' + stage, PT_HOME + '/' + name):
        if _EAL.does_asset_exist(old):  # a re-import into the old asset keeps its hulls and slot names: start clean
            _EAL.delete_asset(old)
    task = unreal.AssetImportTask()
    task.filename = PT_SRC + name + '_LOD0.FBX'
    task.destination_path = PT_HOME
    task.destination_name = name
    task.automated, task.save, task.replace_existing, task.options = True, False, True, _fbx_ui()
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        _TOOLS.import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
    mesh = unreal.load_asset(PT_HOME + '/' + name)
    if mesh is None:
        raise RuntimeError('import failed ' + name)
    for lod in (1, 2):
        got = _SUB.import_lod(mesh, lod, PT_SRC + '%s_LOD%d.FBX' % (name, lod))
        if got != lod:
            _log('WARN', name, 'LOD%d import returned' % lod, got)
    _SUB.set_lod_screen_sizes(mesh, PT_LOD_SCREEN[:mesh.get_num_lods()])
    ns = mesh.get_editor_property('nanite_settings')
    ns.enabled = False
    mesh.set_editor_property('nanite_settings', ns)
    statics = mesh.get_editor_property('static_materials')
    for sm_ in statics:
        phyto = 'Phyto' in str(sm_.get_editor_property('material_slot_name'))
        sm_.set_editor_property('material_slot_name', 'Phytophage' if phyto else 'Tree')
        sm_.set_editor_property('material_interface', phyto_mi if phyto else bark_mi)
    mesh.set_editor_property('static_materials', statics)
    slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
    for i, sn in enumerate(slots):
        mesh.set_material(i, phyto_mi if sn == 'Phytophage' else bark_mi)
    mesh.set_editor_property('light_map_coordinate_index', 2)
    mesh.set_editor_property('light_map_resolution', 64)
    body = mesh.get_editor_property('body_setup')
    body.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_SIMPLE_AS_COMPLEX)
    _EAL.save_loaded_asset(mesh, False)
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems'))
    b = mesh.get_bounding_box()
    info = dict(asset=PT_HOME + '/' + name, lods=mesh.get_num_lods(),
                tris=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
                sections=[_SUB.get_number_materials(mesh)] if hasattr(_SUB, 'get_number_materials') else None,
                slots=slots, materials=[mesh.get_material(i).get_name() for i in range(len(slots))], hulls=hulls,
                min=[round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)],
                max=[round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)],
                uv_channels=_SUB.get_num_uv_channels(mesh, 0))
    try:
        desc = mesh.get_static_mesh_description(0)
        info['vertex_colour_attr'] = bool(desc)
    except Exception:            # noqa: BLE001 - reading only, optional
        pass
    _log(name, json.dumps(info))
    return mesh, info


def _components(bp):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    out = {}
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        o = lib.get_object_for_blueprint(lib.get_data(h), bp)
        if o is not None and isinstance(o, unreal.ActorComponent):
            out[o.get_name().replace('_GEN_VARIABLE', '')] = o
    return out


def _wrapper(stage, mesh):
    path = PT_HOME + '/BP_PhytoTree_' + stage
    bp = unreal.load_asset(path) if _EAL.does_asset_exist(path) else _EAL.duplicate_asset(PT_BP_SOURCE, path)
    if bp is None:
        raise RuntimeError('could not create ' + path)
    cdo = unreal.get_default_object(bp.generated_class())
    cur = cdo.get_editor_property('TreeMesh')
    cdo.set_editor_property('TreeMesh', [mesh] if isinstance(cur, (list, unreal.Array)) else mesh)
    comps = _components(bp)
    tree = comps.get('Tree')
    if tree is None:
        raise RuntimeError(path + ': no Tree component, have %s' % list(comps))
    tree.set_editor_property('static_mesh', mesh)
    tree.set_editor_property('override_materials', [])
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    _EAL.save_loaded_asset(bp, False)
    cdo = unreal.get_default_object(bp.generated_class())
    detail = {}
    for k, comp in comps.items():
        extra = comp.get_class().get_name()
        if isinstance(comp, unreal.StaticMeshComponent):
            sm = comp.get_editor_property('static_mesh')
            extra += ' mesh=%s visible=%s' % (sm.get_name() if sm else None, comp.get_editor_property('visible'))
        detail[k] = extra
    info = dict(bp=path, components=detail, tank=str(cdo.get_editor_property('TankDestructionType')),
                tree_mesh=str(cdo.get_editor_property('TreeMesh'))[-70:])
    _log('wrapper', json.dumps(info))
    return info


def main():
    _EAL.make_directory(PT_HOME)
    masks = _texture('T_PhytoTree_Masks.png', 'T_PhytoTree_Masks', False)
    normal = _texture('T_PhytoTree_NormalDX.png', 'T_PhytoTree_Normal', True)
    master = _master(masks, normal)
    mi = _instance(master)
    bark = unreal.load_asset(PT_BARK)
    report = dict(textures=[masks.get_path_name(), normal.get_path_name()], master=master.get_path_name(),
                  instance=mi.get_path_name(), meshes={}, wrappers={})
    src = unreal.load_asset(PT_SOURCE_MESH)
    sb = src.get_bounding_box()
    report['source'] = dict(asset=PT_SOURCE_MESH, tris=[src.get_num_triangles(i) for i in range(src.get_num_lods())],
                            min=[round(sb.min.x, 1), round(sb.min.y, 1), round(sb.min.z, 1)],
                            max=[round(sb.max.x, 1), round(sb.max.y, 1), round(sb.max.z, 1)])
    for stage in globals().get('PHYTO_STAGES') or PT_STAGES:
        mesh, info = _mesh(stage, bark, mi)
        report['meshes'][stage] = info
        if globals().get('PHYTO_WRAPPERS', True):
            report['wrappers'][stage] = _wrapper(stage, mesh)
    os.makedirs(os.path.dirname(PT_REPORT), exist_ok=True)
    with open(PT_REPORT, 'w', encoding='utf-8') as handle:
        json.dump(report, handle, indent=1)
    _log('PHYTO_TREES_DONE')


main()
