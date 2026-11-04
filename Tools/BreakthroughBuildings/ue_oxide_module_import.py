"""Oxide Works expansion module M3 Gesenkhammer: imports its meshes and builds its two forge materials (run with the UECP
execute_python action; safe to rerun).

Blender source, frame, sockets and motion data: D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/
OxideWorks/Modules/Model/README.md. Copy the build's Motion_Params.json next to this script as
SM_BF_OxideWorks_Gesenkhammer_Motion.json first: the editor's Python may only open files inside the project.

  SM_BF_OxideWorks_Gesenkhammer          static: concrete + trim, 3 UCX, sockets Ram / Sparks / FurnaceMouth
  SM_BF_OxideWorks_Gesenkhammer_Moving   ram, flywheel, Scotch-yoke rod, chute links -> M_BF_OW_ModuleMotion
  SM_BF_OxideWorks_Gesenkhammer_Hot      billet, tundish melt, spout runs, furnace mouth -> M_BF_OW_HotMetal
  SM_BF_OxideWorks_Gesenkhammer_Parts    forged parts in the bin -> M_BF_RedOxidePrimer

Both forge materials take Time, Gate and Running from the faction's MF_BF_MotionClock (ue_bf_motion_materials.py), so the
module follows the WPO quality setting and stops with custom primitive data 16 (MotionStopped) = 1 like every other Breakthrough machine.
Period, blows per billet and squash come from MPC_BF_OW_Forge, which a Blueprint can read to fire the Sparks socket on the
same beat. The phase is not per object: the blow lands at Time = n * Period on every copy.

Run: exec this file; then wire World Position Offset on both materials with UECP connect_material_nodes (Python's
MaterialProperty has no WPO; the indices are printed), then exec again with FINISH = True to compile, save and assign.
"""
import importlib
import json
import sys

import unreal

TOOLS = 'D:/UE5Projects/RTS_Survival/Source/Tools/BreakthroughBuildings/'
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
import ue_bf_motion_materials as kit  # noqa: E402
importlib.reload(kit)

MODEL = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/OxideWorks/Modules/Model/'
NAME = 'SM_BF_OxideWorks_Gesenkhammer'
HOME = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/BreakthroughOxideWorks/Gesenkhammer'
SHARED, MOTION = kit.SHARED, kit.MOTION
MPC_PATH = MOTION + '/MPC_BF_OW_Forge'
MOVING_MAT = MOTION + '/M_BF_OW_ModuleMotion'
HOT_MAT = MOTION + '/M_BF_OW_HotMetal'
PRIMER_MAT = SHARED + '/M_BF_RedOxidePrimer'
SLOTS = dict(M_BF_SootedFortress_Concrete=SHARED + '/MI_BF_SootedFortress_UVTile', M_BF_Trim=SHARED + '/M_BF_Trim',
             M_BF_OW_ModuleMotion=MOVING_MAT, M_BF_OW_HotMetal=HOT_MAT, M_BF_RedOxidePrimer=PRIMER_MAT)
SOCKETS = ('Ram', 'Sparks', 'FurnaceMouth')
GROUP = 'BF Forge'
MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
FLOAT1, FLOAT3, FLOAT4 = (unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                          unreal.CustomMaterialOutputType.CMOT_FLOAT4)

CLOCK = '''
float t = DebugTime >= 0.0 ? DebugTime : T;
'''

# Moving: one flag per part in the vertex colour (R ram, G flywheel, B yoke, A chute link), pivots from the build.
FORGE_MOTION = CLOCK + '''
float p = frac(t / Period);
float blow = fmod(floor(t / Period), Strikes);
float sq = Squash * blow / max(Strikes - 1.0, 1.0);
// the blow lands at p = 0: down briefly, a slow lift, a wait at the top, then the drop
float fall = p < 0.08 ? 1.0 : (p < 0.6 ? 1.0 - smoothstep(0.08, 0.6, p) : (p < 0.88 ? 0.0 : pow((p - 0.88) / 0.12, 2.0)));
float a = 6.2831853 * p * Run;          // one turn of the flywheel per blow
float c = cos(a), s = sin(a);
float3 d = L - Pivot.xyz;
float3 pin = PinRest.xyz - Pivot.xyz;
float3 off = 0;
if (VC.r > 0.5) off = float3(0, 0, -(RamDrop + BilletH * sq) * fall * Run);
else if (VC.g > 0.5) off = float3(0, d.y * c - d.z * s, d.y * s + d.z * c) - float3(0, d.y, d.z);
else if (VC.b > 0.5)
{
    // the Scotch yoke follows the turned crank pin along the rod, so pin and slot can never part
    float3 moved = float3(pin.x, pin.y * c - pin.z * s, pin.y * s + pin.z * c) - pin;
    off = RodAxis.xyz * dot(moved, RodAxis.xyz);
}
else if (VC.a > 0.5) off = ChuteStep.xyz * smoothstep(0.1, 0.5, p) * Run;   // one pitch per blow; hood and cowl hide the wrap
return off;
'''

HOT_MOTION = CLOCK + '''
float blow = fmod(floor(t / Period), Strikes);
float sq = Squash * blow / max(Strikes - 1.0, 1.0);
float top = step(0.83, VC.a) * step(0.5, VC.g);      // the billet's top face sinks as it is forged
return float3(0, 0, -top * BilletH * sq * Run);
'''

# Hot: A = which part (0 furnace mouth, 1/3 tundish, 2/3 spout run, 1 billet). The melt is the building's BF_Slag, with
# its plates drifting the way each part's melt really moves.
HOT_SURFACE = CLOCK + '''
struct BFSlag
{
    float hash(float2 q)
    {
        float3 h = frac(float3(q.x, q.y, q.x) * 0.1031);
        h += dot(h, h.yzx + 33.33);
        return frac((h.x + h.y) * h.z);
    }
    float noise(float2 q)
    {
        float2 i = floor(q), f = frac(q);
        float2 u = f * f * (3.0 - 2.0 * f);
        return lerp(lerp(hash(i), hash(i + float2(1, 0)), u.x), lerp(hash(i + float2(0, 1)), hash(i + 1.0), u.x), u.y);
    }
    float2 hash2(float2 q)
    {
        return float2(hash(q), hash(q + 17.17));
    }
};
BFSlag f;
float kind = VC.a;
float ph = frac(t / Period);
float blow = fmod(floor(t / Period), Strikes);
if (kind > 0.83)
{
    // the billet: a white-yellow flash at each blow, a little cooler after every blow until the next billet comes in
    float flash = Run * pow(saturate(1.0 - ph / 0.3), 2.0);
    float heat = lerp(0.62, 0.40, blow / max(Strikes - 1.0, 1.0));
    float grain = f.noise(P.xy / 6.0 + P.z / 9.0 + t * 0.6);
    return float4(saturate(lerp(0.28, heat, Run) * (0.85 + 0.3 * grain)), 0.1, flash, 0.0);
}
float3 n = normalize(LN);
float3 an = abs(n);
float2 p;
float running = 1.0;
float crust = Crust;
if (kind > 0.5)
{
    float3 dir = normalize(VC.r > 0.5 ? Spout1.xyz : Spout0.xyz);
    float3 side = normalize(cross(float3(0, 0, 1), dir));
    p = float2(dot(P, side), dot(P, dir) - t * SpoutFlow * Run);
    running = 2.6;
}
else if (kind > 0.17)
{
    float a = t * TundishSwirl * Run;
    float2 d = P.xy - Centre.xy;
    float s = sin(a), c = cos(a);
    p = float2(c * d.x - s * d.y, s * d.x + c * d.y);
    crust = TundishCrust;
}
else
{
    p = (an.x > an.y) ? P.yz : P.xz;
    p.y += t * SpoutFlow * 0.3 * Run;
    running = 2.6;
}
p += Seed * 17.3;
float cell = max(CellCm, 5.0);
float2 q = p / cell + (float2(f.noise(p / (cell * 0.7)), f.noise(p / (cell * 0.7) + 3.1)) - 0.5) * 0.6;
float2 qi = floor(q), qf = frac(q);
float d1 = 9.0, d2 = 9.0;
float2 id = qi;
[unroll] for (int y = -1; y <= 1; y++)
{
    [unroll] for (int x = -1; x <= 1; x++)
    {
        float2 o = float2(x, y);
        float2 cc = o + f.hash2(qi + o + Seed) - qf;
        float dd = dot(cc, cc);
        if (dd < d1)
        {
            d2 = d1;
            d1 = dd;
            id = qi + o;
        }
        else
        {
            d2 = min(d2, dd);
        }
    }
}
float seam = sqrt(d2) - sqrt(d1);
float w = lerp(0.30, 0.04, saturate(crust)) * lerp(0.35, 1.5, f.noise(p / (cell * 3.0) + 7.7)) * running;
float molten = 1.0 - smoothstep(0.0, w, seam);
float hotPlate = step(f.hash(id + Seed), (1.0 - saturate(crust)) * 0.12 + 0.02) * 0.3;
float pulse = 0.85 + 0.15 * sin(t * 1.7 + f.hash(id + 3.0) * 6.283);
float temp = saturate(max(molten, hotPlate) * pulse + f.noise(p / (cell * 0.25) + t * 0.3) * 0.12 * molten);
if (kind < 0.17) temp *= 0.8 + 0.2 * sin(t * 7.3 + 1.1) * sin(t * 3.1);   // the furnace mouth flickers
return float4(temp * lerp(0.45, 1.0, Run), 1.0 - molten, 0.0, 0.0);
'''

HOT_EMISSIVE = '''
float t = saturate(H.x);
float3 hot = lerp(float3(0.55, 0.04, 0.0), float3(1.0, 0.28, 0.03), saturate(t * 1.6));
hot = lerp(hot, float3(1.0, 0.75, 0.35), saturate(t * 2.0 - 1.2));
return (hot * t * t + float3(1.0, 0.85, 0.55) * H.z * Flash) * Glow + float3(0.25, 0.02, 0.0) * Glow * Ember;
'''

failures = kit.failures


def params():
    return json.loads(open(TOOLS + NAME + '_Motion.json', encoding='utf-8').read())


def vec(values):
    return unreal.LinearColor(values[0], values[1], values[2], 0.0)


def collection():
    """MPC_BF_OW_Forge: the beat both materials and a Sparks timer share."""
    data = params()
    if not ASSETS.does_asset_exist(MPC_PATH):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset('MPC_BF_OW_Forge', MOTION, unreal.MaterialParameterCollection,
                                                                unreal.MaterialParameterCollectionFactoryNew())
    mpc = unreal.load_asset(MPC_PATH)
    rows = []
    for name, value in (('Period', data['Period']), ('StrikesPerBillet', data['StrikesPerBillet']), ('SquashFraction', data['SquashFraction'])):
        row = unreal.CollectionScalarParameter()
        row.set_editor_property('parameter_name', name)
        row.set_editor_property('default_value', float(value))
        rows.append(row)
    mpc.set_editor_property('scalar_parameters', rows)
    kit.save(mpc)
    return mpc


def fresh_material(path):
    if not ASSETS.does_asset_exist(path):
        folder, name = path.rsplit('/', 1)
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material = unreal.load_asset(path)
    for _ in range(20):
        nodes = kit.live_expressions(material)
        if not nodes:
            break
        for e in nodes:
            MEL.delete_material_expression(material, e)
    material.set_editor_property('used_with_static_lighting', True)
    return material


def scalar(g, name, value, x, y):
    return g.node('ScalarParameter', x, y, parameter_name=name, default_value=float(value), group=GROUP)


def vector(g, name, value, x, y):
    return g.node('VectorParameter', x, y, parameter_name=name, default_value=vec(value), group=GROUP)


def rgba(g, vc):
    """The vertex colour as float4: the VertexColor node's default pin is RGB only, and it has no RGBA pin."""
    e = g.node('AppendVector', vc.get_editor_property('material_expression_editor_x') + 150,
               vc.get_editor_property('material_expression_editor_y') + 60)
    g.link(vc, '', e, 'A')
    g.link(vc, 'A', e, 'B')
    return e


def beat(g, mpc, x, y):
    return [(name, g.node('CollectionParameter', x, y + 70 * i, collection=mpc, parameter_name=param), '')
            for i, (name, param) in enumerate((('Period', 'Period'), ('Strikes', 'StrikesPerBillet'), ('Squash', 'SquashFraction')))]


def trim_sources():
    """M_BF_Trim's texture nodes and which output feeds which property, so the moving parts wear the same paint."""
    trim = unreal.load_asset(SHARED + '/M_BF_Trim')
    out = {}
    for prop in ('MP_BASE_COLOR', 'MP_METALLIC', 'MP_ROUGHNESS', 'MP_NORMAL', 'MP_AMBIENT_OCCLUSION'):
        mp = getattr(unreal.MaterialProperty, prop)
        node = MEL.get_material_property_input_node(trim, mp)
        pin = MEL.get_material_property_input_node_output_name(trim, mp)
        out[prop] = (node.get_editor_property('texture'), node.get_editor_property('sampler_type'), pin)
    return out


def moving_material(mpc, clock_fn):
    data = params()
    m = fresh_material(MOVING_MAT)
    g = kit.Graph(m)
    src = trim_sources()
    samples = {}
    for i, (prop, (texture, sampler, pin)) in enumerate(src.items()):
        key = texture.get_name()
        if key not in samples:
            samples[key] = g.node('TextureSampleParameter2D', -900, -600 + 260 * len(samples), texture=texture, sampler_type=sampler,
                                  parameter_name='Trim_' + key, group=GROUP)
    base_tex, _, base_pin = src['MP_BASE_COLOR']
    lift = scalar(g, 'PaintLift', 0.45, -900, 300)
    # field-grey steel vanishes against the concrete from the RTS camera: lift the paint (AGENTS.md), keep primer on the links
    lifted = g.custom('return lerp(C * 3.0, float3(0.30, 0.31, 0.29), Lift);', FLOAT3, 'BF lifted steel paint',
                      [('C', samples[base_tex.get_name()], base_pin), ('Lift', lift, '')], -550, -600)
    vc = g.node('VertexColor', -900, 450)
    primer = vector(g, 'PrimerColor', (0.13, 0.028, 0.014), -900, 380)
    base = g.node('LinearInterpolate', -250, -600)
    g.link(lifted, '', base, 'A')
    g.link(primer, '', base, 'B')
    g.link(vc, 'A', base, 'Alpha')
    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for prop in ('MP_METALLIC', 'MP_ROUGHNESS', 'MP_AMBIENT_OCCLUSION', 'MP_NORMAL'):
        texture, _, pin = src[prop]
        source = samples[texture.get_name()]
        if prop == 'MP_METALLIC':      # painted steel: cut the metal with the lift, or it goes dark under a dim sky
            cut = g.custom('return M * (1.0 - Lift);', FLOAT1, 'BF paint cuts metallic',
                           [('M', source, pin), ('Lift', lift, '')], -550, -300)
            MEL.connect_material_property(cut, '', unreal.MaterialProperty.MP_METALLIC)
            continue
        MEL.connect_material_property(source, pin, getattr(unreal.MaterialProperty, prop))
    clock = g.call(clock_fn, -900, 700)
    inputs = [('L', g.node('LocalPosition', -900, 900), ''), ('VC', rgba(g, vc), ''), ('T', clock, 'Time'), ('Run', clock, 'Running'),
              ('DebugTime', scalar(g, 'DebugTime', -1.0, -900, 1000), '')] + beat(g, mpc, -900, 1080)
    inputs += [('RamDrop', scalar(g, 'RamDropCm', data['RamDrop'], -900, 1300), ''),
               ('BilletH', scalar(g, 'BilletHeightCm', data['BilletHeight'], -900, 1370), ''),
               ('Pivot', vector(g, 'WheelPivot', data['WheelPivot'], -900, 1440), ''),
               ('PinRest', vector(g, 'PinRest', data['PinRest'], -900, 1510), ''),
               ('RodAxis', vector(g, 'RodAxis', data['RodAxis'], -900, 1580), ''),
               ('ChuteStep', vector(g, 'ChuteStep', data['ChuteStep'], -900, 1650), '')]
    local = g.custom(FORGE_MOTION, FLOAT3, 'BF forge motion', inputs, -550, 900)
    wpo = g.gated(local, clock, -150, 900)
    return m, wpo


def hot_material(mpc, clock_fn):
    data = params()
    m = fresh_material(HOT_MAT)
    g = kit.Graph(m)
    clock = g.call(clock_fn, -1100, 600)
    vc = g.node('VertexColor', -1100, 400)
    world = g.node('WorldPosition', -1300, -300)
    local_p = g.node('TransformPosition', -1100, -300, transform_source_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD,
                     transform_type=unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    g.link(world, '', local_p, '')
    normal = g.node('VertexNormalWS', -1300, -200)
    local_n = g.node('Transform', -1100, -200, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD,
                     transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    g.link(normal, '', local_n, '')
    debug = scalar(g, 'DebugTime', -1.0, -1100, 700)
    common = [('VC', rgba(g, vc), ''), ('T', clock, 'Time'), ('Run', clock, 'Running'), ('DebugTime', debug, '')] + beat(g, mpc, -1100, 780)
    surface = g.custom(HOT_SURFACE, FLOAT4, 'BF forge hot metal',
                       [('P', local_p, ''), ('LN', local_n, '')] + common +
                       [('Crust', scalar(g, 'BF_SlagCrust', 0.55, -1300, 0), ''),
                        ('TundishCrust', scalar(g, 'TundishCrust', 0.75, -1300, 70), ''),
                        ('CellCm', scalar(g, 'BF_SlagCellCm', 18.0, -1300, 140), ''),
                        ('SpoutFlow', scalar(g, 'SpoutFlowCmPerSec', 22.0, -1300, 210), ''),
                        ('TundishSwirl', scalar(g, 'TundishSwirl', 0.35, -1300, 280), ''),
                        ('Seed', scalar(g, 'BF_SlagSeed', 5.0, -1300, 350), ''),
                        ('Centre', vector(g, 'TundishCentre', data['TundishCentre'], -1300, 420), ''),
                        ('Spout0', vector(g, 'SpoutDir0', data['SpoutDirs'][0], -1300, 490), ''),
                        ('Spout1', vector(g, 'SpoutDir1', data['SpoutDirs'][1], -1300, 560), '')], -700, -100)
    emissive = g.custom(HOT_EMISSIVE, FLOAT3, 'BF forge emissive',
                        [('H', surface, ''), ('Glow', scalar(g, 'BF_SlagGlow', 12.0, -700, 200), ''),
                         ('Ember', scalar(g, 'BF_SlagEmber', 0.015, -700, 270), ''), ('Flash', scalar(g, 'BlowFlash', 0.7, -700, 340), '')],
                        -350, -100)
    MEL.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    temp = g.node('ComponentMask', -350, 100, r=True, g=False, b=False, a=False)
    g.link(surface, '', temp, '')
    rough = g.node('LinearInterpolate', -150, 100)
    g.link(scalar(g, 'BF_SlagRoughCrust', 0.9, -350, 200), '', rough, 'A')
    g.link(scalar(g, 'BF_SlagRoughMelt', 0.45, -350, 270), '', rough, 'B')
    g.link(temp, '', rough, 'Alpha')
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(vector(g, 'BF_SlagCrustColor', (0.035, 0.028, 0.024), -150, -250), '', unreal.MaterialProperty.MP_BASE_COLOR)
    local = g.custom(HOT_MOTION, FLOAT3, 'BF billet squash', [('BilletH', scalar(g, 'BilletHeightCm', data['BilletHeight'], -1100, 1000), '')] +
                     common, -700, 700)
    wpo = g.gated(local, clock, -300, 700)
    return m, wpo


def primer_material():
    m = fresh_material(PRIMER_MAT)
    g = kit.Graph(m)
    MEL.connect_material_property(g.node('VectorParameter', -400, 0, parameter_name='PrimerColor',
                                         default_value=unreal.LinearColor(0.13, 0.028, 0.014, 1.0)), '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(g.node('ScalarParameter', -400, 200, parameter_name='Roughness', default_value=0.72), '',
                                  unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    kit.save(m)


def build_materials():
    """Step 1: the graphs. Returns the WPO source node of each material; wire them with UECP, then FINISH."""
    clock_fn = unreal.load_asset(MOTION + '/MF_BF_MotionClock')
    mpc = collection()
    primer_material()
    out = {}
    for path, make in ((MOVING_MAT, moving_material), (HOT_MAT, hot_material)):
        material, wpo = make(mpc, clock_fn)
        out[path] = wpo.get_name()
    return out


def import_mesh(name):
    """Legacy FBX (Interchange drops UCX hulls), vertex colours replaced from the file: they carry the motion flags."""
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
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + '.FBX'
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])


def finish_mesh(name):
    path = f'{HOME}/{name}'
    mesh = unreal.load_asset(path)
    for index, slot in enumerate(mesh.get_editor_property('static_materials')):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in SLOTS if slot_name.startswith(k)), None)
        if key is None:
            raise RuntimeError(f'no material for slot {slot_name} on {name}')
        mesh.set_material(index, unreal.load_asset(SLOTS[key]))
    mesh.set_editor_property('light_map_coordinate_index', 1)
    found = []
    for socket_name in SOCKETS if name == NAME else ():    # the Sockets array is protected from Python; look each one up
        socket = mesh.find_socket(socket_name)
        if socket is None:
            raise RuntimeError('missing socket ' + socket_name)
        socket.set_editor_property('relative_rotation', unreal.Rotator(0, 0, 0))   # FBX empties arrive tilted
        socket.set_editor_property('relative_scale', unreal.Vector(1, 1, 1))
        where = socket.get_editor_property('relative_location')
        found.append((socket_name, round(where.x, 1), round(where.y, 1), round(where.z, 1)))
    kit.save(mesh)
    body = mesh.get_editor_property('body_setup')
    hulls = len(body.get_editor_property('agg_geom').get_editor_property('convex_elems')) if body else 0
    print(name, 'tris', mesh.get_num_triangles(0), 'convex', hulls, 'sockets', found)
    return mesh


def import_all():
    unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
    try:
        for part in ('', '_Moving', '_Hot', '_Parts'):
            import_mesh(NAME + part)
    finally:
        unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')


def finish():
    """Step 2, after WPO is wired: compile and save the materials, then assign them on the meshes."""
    for path in (MOVING_MAT, HOT_MAT):
        m = unreal.load_asset(path)
        MEL.recompile_material(m)
        kit.save(m)
    for part in ('', '_Moving', '_Hot', '_Parts'):
        finish_mesh(NAME + part)


if globals().get('FINISH'):
    finish()
else:
    print('WPO sources', build_materials())
    import_all()
print('failures', failures)
