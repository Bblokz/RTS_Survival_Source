"""Breakthrough building motion kit: four world-position-offset functions and the instances that use them
(run with the UECP execute_python action; safe to rerun).

  MF_BF_MotionClock  shared clock: Time (times MotionTimeScale, 1), a per-object Phase, Gate (MPC_RTSWPO.WPOIntensity, the
                     trees' wind setting, clamped to 0-1) and Running = 1 - MotionStopped (custom primitive data 16; an
                     unset slot reads 0, so machines run unless a building writes 1; wind is never stopped).
  MF_BF_Flutter      cloth in the wind. UV channel 2 holds the weights (x sway, y lift), channel 3 the sway direction in
                     the mesh's local axes. The FBX importer stores V as 1 - v, so the shader flips both y values back;
                     an unweighted vertex then reads 0 and does not move. Sway only pushes outwards, so a
                     net against a wall never goes through it.
  MF_BF_Spin         turns the mesh about its local Z through its pivot (fans).
  MF_BF_Rattle       lifts the mesh's local +X edge about its local Y axis in puffs (exhaust rain flaps).
  MF_BF_Sway         pendulum about the pivot, weighted by the Weight input (hooks, chains, antennas; not used yet).

M_BF_CamoNet and M_BF_Trim get static switches MotionFlutter / MotionSpin / MotionRattle, all off, so every mesh
already on them is unchanged; the instances in Motion/ switch one on. Building meshes keep motion on their own
small meshes or behind those weights. See Source/Docs/Rendering/BreakthroughMotion.md.

Run: exec this file, then build_functions(), build_base_materials(...), wire World Position Offset (UECP
connect_material_nodes to_input='WorldPositionOffset'), compile and save both materials, then build_instances().
"""
import unreal

SHARED = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared'
MOTION = SHARED + '/Motion'
MPC = '/Game/RTS_Survival/Blueprints/Environment/WpoTreeFoliage/MasterMats/MPC_RTSWPO'
RUNNING_PRIMITIVE_DATA = 16     # 0 is the faction flag, 4-7 the portrait team colour, 8-12 the old cars
GROUP = 'BF Motion'
TAG = 'BFMotion'                # desc on every node this script adds to the two base materials
MEL = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
FLOAT3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
FLOAT1 = unreal.CustomMaterialOutputType.CMOT_FLOAT1
failures = []

PHASE = 'return frac(sin(dot(O.xy, float2(0.1291, 0.7823))) * 43758.5453) * 6.2831853;'

FLUTTER = '''
W.y = 1.0 - W.y;
D.y = 1.0 - D.y;
float gust = 0.62 + 0.38 * sin(T * 0.41 + Ph) * sin(T * 0.23 + Ph * 1.7);
float s1 = dot(P.xy, float2(0.0105, 0.0071));
float s2 = dot(P.xy, float2(-0.0173, 0.0219));
float t = T * Speed;
float outwards = 0.5 + 0.5 * (0.65 * sin(t * 1.9 - s1 + Ph) + 0.35 * sin(t * 3.3 - s2));
float up = 0.7 * sin(t * 1.3 - s1 * 0.8 + Ph) + 0.3 * sin(t * 2.9 - s2 * 1.3);
return float3(D * (W.x * Sway * outwards * gust), W.y * Lift * up * gust);
'''

SPIN = '''
float a = frac(T * Rate + Ph * 0.1591549) * 6.2831853 * Run;
float c = cos(a);
float s = sin(a);
return float3(L.x * c - L.y * s, L.x * s + L.y * c, L.z) - L;
'''

RATTLE = '''
float puff = pow(saturate(sin(T * Rate * 6.2831853 + Ph)), 4.0);
float chatter = 2.5 * puff * (0.5 + 0.5 * sin(T * 31.0 + Ph * 3.1));
float a = radians(Run * (Rest + Lift * puff * (0.6 + 0.4 * sin(T * 1.7 + Ph * 2.3)) + chatter));
float c = cos(a);
float s = sin(a);
return float3(L.x * c - L.z * s, L.y, L.x * s + L.z * c) - L;
'''

SWAY = '''
float ax = radians(Amp) * Weight * sin(T * Rate * 6.2831853 + Ph);
float ay = radians(Amp) * 0.7 * Weight * sin(T * Rate * 8.6 + Ph * 1.9);
float3 p = float3(L.x, L.y * cos(ax) - L.z * sin(ax), L.y * sin(ax) + L.z * cos(ax));
p = float3(p.x * cos(ay) + p.z * sin(ay), p.y, -p.x * sin(ay) + p.z * cos(ay));
return p - L;
'''


class Graph:
    """Adds nodes to a material or a material function and wires them; failed links are collected in failures."""

    def __init__(self, owner, tag=''):
        self.owner, self.tag = owner, tag

    def node(self, cls, x, y, **props):
        expression_class = getattr(unreal, 'MaterialExpression' + cls)
        if isinstance(self.owner, unreal.MaterialFunction):
            e = MEL.create_material_expression_in_function(self.owner, expression_class, x, y)
        else:
            e = MEL.create_material_expression(self.owner, expression_class, x, y)
        if self.tag:
            e.set_editor_property('desc', self.tag)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, a_out, b, b_in):
        if not MEL.connect_material_expressions(a, a_out, b, b_in):
            failures.append(f'{self.owner.get_name()}: {a.get_name()}.{a_out} -> {b.get_name()}.{b_in}')

    def scalar(self, name, value, x, y, **props):
        return self.node('ScalarParameter', x, y, parameter_name=name, default_value=value, group=GROUP, **props)

    def custom(self, code, output_type, description, inputs, x, y):
        """Custom HLSL node; inputs is [(name, node, node output)], wired in that order."""
        e = self.node('Custom', x, y, code=code, output_type=output_type, description=description)
        pins = []
        for name, _source, _out in inputs:
            pin = unreal.CustomInput()
            pin.set_editor_property('input_name', name)
            pins.append(pin)
        e.set_editor_property('inputs', pins)
        for name, source, out in inputs:
            self.link(source, out, e, name)
        return e

    def call(self, function, x, y):
        e = self.node('MaterialFunctionCall', x, y)
        e.set_material_function(function)
        return e

    def local_to_world(self, source, x, y):
        e = self.node('Transform', x, y, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                      transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
        self.link(source, '', e, '')
        return e

    def gated(self, source, clock, x, y):
        """Local offset to world, times the clock's Gate: the whole kit stops when the wind setting is 0."""
        e = self.node('Multiply', x, y)
        self.link(self.local_to_world(source, x - 220, y), '', e, 'A')
        self.link(clock, 'Gate', e, 'B')
        return e


def live_expressions(owner):
    return [e for e in unreal.ObjectIterator(unreal.MaterialExpression) if e.get_outer() == owner and unreal.SystemLibrary.is_valid(e)]


def function_asset(name, description):
    """The function, created on first use, emptied for a rebuild (one delete pass only removes every other node)."""
    path = f'{MOTION}/{name}'
    if not ASSETS.does_asset_exist(path):
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MOTION, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
    function = unreal.load_asset(path)      # a just-created function rejects nodes until it is loaded again
    for _ in range(20):
        nodes = live_expressions(function)
        if not nodes:
            break
        for e in nodes:
            MEL.delete_material_expression_in_function(function, e)
    function.set_editor_property('description', description)
    function.set_editor_property('expose_to_library', True)
    function.set_editor_property('library_categories_text', ['RTS Breakthrough'])
    return function


def finish_function(function):
    MEL.update_material_function(function)
    save(function)
    return function


def save(asset):
    path = asset.get_path_name().split('.')[0]
    if not ASSETS.save_loaded_asset(asset, False):
        failures.append('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + path[len('/Game/'):] + '.uasset'
    if not unreal.Paths.file_exists(disk):      # a save can report success and still miss the disk
        failures.append('not on disk ' + disk)


def output(g, name, source, priority, x, y, description=''):
    e = g.node('FunctionOutput', x, y, output_name=name, sort_priority=priority, description=description)
    g.link(source, '', e, '')


def time_scaled(g, time):
    """Time times MotionTimeScale (default 1): an instance set to 0.1 films the motion in slow motion."""
    e = g.node('Multiply', -600, 0)
    g.link(time, '', e, 'A')
    g.link(g.scalar('MotionTimeScale', 1.0, -800, -100), '', e, 'B')
    return e


def running_from_stopped(g):
    """Running = 1 - MotionStopped. An unset custom primitive data slot reads 0 on a component (measured 2026-11-03), so the
    slot says "stopped": a component nobody touches keeps moving, and a building writes 1 to stop its machines."""
    stopped = g.scalar('MotionStopped', 0.0, -800, 450, use_custom_primitive_data=True, primitive_data_index=RUNNING_PRIMITIVE_DATA)
    e = g.node('OneMinus', -600, 450)
    g.link(stopped, '', e, '')
    return e


def build_clock():
    f = function_asset('MF_BF_MotionClock', 'Shared clock for the Breakthrough motion functions: Time, a phase per object '
                                            'so neighbours never move in step, Gate from the wind setting, Running from '
                                            f'custom primitive data {RUNNING_PRIMITIVE_DATA} (default 1).')
    g = Graph(f)
    time = time_scaled(g, g.node('Time', -800, 0))
    obj = g.node('ObjectPositionWS', -600, 150)
    phase = g.custom(PHASE, FLOAT1, 'BF object phase', [('O', obj, '')], -350, 150)
    collection = g.node('CollectionParameter', -600, 300, collection=unreal.load_asset(MPC), parameter_name='WPOIntensity')
    gate = g.node('Saturate', -350, 300)
    g.link(collection, '', gate, '')
    running = running_from_stopped(g)
    output(g, 'Time', time, 0, 0, 0)
    output(g, 'Phase', phase, 1, 0, 150, 'Radians, from the object position')
    output(g, 'Gate', gate, 2, 0, 300, 'MPC_RTSWPO.WPOIntensity clamped to 0-1')
    output(g, 'Running', running, 3, 0, 450, f'1 - custom primitive data {RUNNING_PRIMITIVE_DATA} (MotionStopped); 1 unless the building writes 1 there')
    return finish_function(f)


def build_flutter(clock_function):
    f = function_asset('MF_BF_Flutter', 'Cloth in the wind on baked weights: UV2 (sway, lift), UV3 sway direction in local '
                                        'XY. Returns a world position offset.')
    g = Graph(f)
    clock = g.call(clock_function, -900, 0)
    inputs = [('W', g.node('TextureCoordinate', -900, 250, coordinate_index=2), ''),
              ('D', g.node('TextureCoordinate', -900, 350, coordinate_index=3), ''),
              ('P', g.node('WorldPosition', -900, 450, world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS), ''),
              ('T', clock, 'Time'), ('Ph', clock, 'Phase'),
              ('Sway', g.scalar('FlutterSwayCm', 22.0, -900, 550), ''),
              ('Lift', g.scalar('FlutterLiftCm', 5.0, -900, 650), ''),
              ('Speed', g.scalar('FlutterSpeed', 1.0, -900, 750), '')]
    local = g.custom(FLUTTER, FLOAT3, 'BF flutter', inputs, -550, 300)
    output(g, 'WPO', g.gated(local, clock, -150, 300), 0, 100, 300)
    return finish_function(f)


def build_rigid(name, description, code, label, params, clock_function, extra_inputs=()):
    """Spin, Rattle and Sway: a rotation of the local vertex position about the mesh's pivot."""
    f = function_asset(name, description)
    g = Graph(f)
    clock = g.call(clock_function, -900, 0)
    inputs = [('L', g.node('LocalPosition', -900, 250), ''), ('T', clock, 'Time'), ('Ph', clock, 'Phase')]
    inputs += [(pin, g.scalar(param, value, -900, 350 + 100 * i), '') for i, (pin, param, value) in enumerate(params)]
    for pin, make in extra_inputs:
        inputs.append((pin, make(g), ''))
    if 'Run' in code:
        inputs.append(('Run', clock, 'Running'))
    local = g.custom(code, FLOAT3, label, inputs, -550, 300)
    output(g, 'WPO', g.gated(local, clock, -150, 300), 0, 100, 300)
    return finish_function(f)


def weight_input(g):
    one = g.node('Constant', -1100, 900, r=1.0)
    e = g.node('FunctionInput', -900, 900, input_name='Weight', input_type=unreal.FunctionInputType.FUNCTION_INPUT_SCALAR,
               use_preview_value_as_default=True, description='0 holds a vertex still, 1 swings it fully; 1 when unconnected')
    g.link(one, '', e, 'Preview')
    return e


def base_material_switches(path, motions):
    """Static switches, default off, choosing each motion function's offset or zero, summed into World Position Offset."""
    material = unreal.load_asset(path)
    for e in live_expressions(material):
        if e.get_editor_property('desc') == TAG:
            MEL.delete_material_expression(material, e)
    g = Graph(material, TAG)
    total = None
    for i, (switch_name, function) in enumerate(motions):
        y = 1400 + 260 * i
        zero = g.node('Constant3Vector', -900, y + 90, constant=unreal.LinearColor(0, 0, 0, 0))
        switch = g.node('StaticSwitchParameter', -600, y, parameter_name=switch_name, default_value=False, group=GROUP)
        g.link(g.call(function, -900, y), 'WPO', switch, 'True')
        g.link(zero, '', switch, 'False')
        if total is None:
            total = switch
            continue
        add = g.node('Add', -350, y)
        g.link(total, '', add, 'A')
        g.link(switch, '', add, 'B')
        total = add
    return material, total


def instance(name, parent, switch):
    path = f'{MOTION}/{name}'
    if not ASSETS.does_asset_exist(path):
        factory = unreal.MaterialInstanceConstantFactoryNew()
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, MOTION, unreal.MaterialInstanceConstant, factory)
    mi = unreal.load_asset(path)
    mi.set_editor_property('parent', unreal.load_asset(parent))
    MEL.set_material_instance_static_switch_parameter_value(mi, switch, True)
    MEL.update_material_instance(mi)
    save(mi)
    return mi


def build_functions():
    if not ASSETS.does_directory_exist(MOTION):
        ASSETS.make_directory(MOTION)
    clock = build_clock()
    flutter = build_flutter(clock)
    spin = build_rigid('MF_BF_Spin', 'Turns the mesh about its local Z axis through its pivot. Running 0 stops it.', SPIN, 'BF spin',
                       [('Rate', 'SpinTurnsPerSecond', 2.2)], clock)
    rattle = build_rigid('MF_BF_Rattle', 'Lifts the local +X edge about the local Y axis through the pivot, in puffs: an exhaust '
                                         'rain flap. Running 0 closes it.', RATTLE, 'BF rattle',
                         [('Lift', 'RattleLiftDeg', 28.0), ('Rest', 'RattleRestDeg', 5.0), ('Rate', 'RattlePuffsPerSecond', 0.85)], clock)
    sway = build_rigid('MF_BF_Sway', 'Pendulum about the pivot, scaled by Weight (vertex weight or 1).', SWAY, 'BF sway',
                       [('Amp', 'SwayAmpDeg', 3.0), ('Rate', 'SwayRate', 0.35)], clock, [('Weight', weight_input)])
    return dict(clock=clock, flutter=flutter, spin=spin, rattle=rattle, sway=sway)


def build_base_materials(functions):
    """Returns {material path: (material, node to wire into World Position Offset)}; the wiring is a separate step because
    Python's MaterialProperty has no World Position Offset."""
    net = base_material_switches(SHARED + '/M_BF_CamoNet', [('MotionFlutter', functions['flutter'])])
    trim = base_material_switches(SHARED + '/M_BF_Trim', [('MotionFlutter', functions['flutter']), ('MotionSpin', functions['spin']),
                                                          ('MotionRattle', functions['rattle'])])
    return {SHARED + '/M_BF_CamoNet': net, SHARED + '/M_BF_Trim': trim}


def build_instances():
    return [instance('MI_BF_CamoNet_Flutter', SHARED + '/M_BF_CamoNet', 'MotionFlutter'),
            instance('MI_BF_Trim_Flutter', SHARED + '/M_BF_Trim', 'MotionFlutter'),
            instance('MI_BF_Trim_Spin', SHARED + '/M_BF_Trim', 'MotionSpin'),
            instance('MI_BF_Trim_Rattle', SHARED + '/M_BF_Trim', 'MotionRattle')]
