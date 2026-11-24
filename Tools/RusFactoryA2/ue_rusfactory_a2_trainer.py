"""Rus_Building_Factory (A2 crane, live): enemy AI training activity (Docs/EnemyAI/EnemyBuildingTraining.md).

Run headless (no UECP needed), safe to rerun:
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/RusFactoryA2/ue_rusfactory_a2_trainer.py"
      -unattended -nop4 -nosplash -DisablePlugins=Fab,BpGeneratorUltimate

  - EnemyBuildingTrainer (UEnemyBuildingTrainerComponent): Vehicles, IdleAnimation FinishCycle. Frame 0 of the 30 s
    A_RusFactoryA2_Work loop is the Idle pose (jib parked over the closed bay, hook up, hook turret hidden, the next
    turret on the lorry), so the crane finishes its cycle and rests there.
  - A2_Crane: tag AITrainingActivity, no longer plays on its own (saved_playing False).
  - AITrainingExit: in front of the gate on the +X face, between the wing walls (opening y -175..475 cm, gate face
    x ~1040, wing tips x ~1440; render mesh measured in Blender), ~4.8 m past the tips, +X out.
  - Work lights and welding sparks (NS_Sparks_Welding, the player cranes' sparks), tagged AITrainingActivity: a warm
    flood in the gate recess + sparks at the gate, a bluish weld light + sparks on the roof-bay coaming (seen when the
    turret goes down). Light intensity is a share of the Assembly Yard floods' 6000 cd (fixed, see main()).
    NS_Sparks_Welding also plays S_Welding (Niagara PlayAudio), so both spark emitters sound while training. They hang off the root, not A2_Machinery, so the construction / pack-up / death visibility
    nodes never switch them on; the trainer keeps them off while idle.
  - Roof beacons (user 2026-11-19: "flashing lights on the roof at a few places when training"): four amber warning
    lamps on the bare concrete beside the gate (GateLeft / GateRight) and on the back band (BackLeft / BackRight) -
    most of the roof is under camo nets - and a red obstruction lamp on a bracket off the stack's +X wall (rim 12.7 m).
    Each = housing (engine Cylinder, M_BF_Crane_WornSteel; engine Cube bracket on the stack) + lens (engine Sphere,
    MI_RusFactory_Beacon_*, tag AITrainingGlow) under A2_Machinery, so they hide with the crane during construction /
    pack-up / death, + a point light under the root (tag AITrainingActivity, hidden while idle) whose light function
    MI_RusFactory_BeaconLF_* blinks in step with its lens (same BlinkRate / BlinkPhase / BlinkSharpness, both on
    material Time). The amber lamps are a quarter cycle apart (a chase), so one is always near a flash.
    Seats: downward raycasts with the hit material on the A2 render mesh. Checks before the save: parenting
    (is_attached_to) and NoCollision on every beacon mesh.
    Materials: FactoryA2/AITraining (M_RusFactory_Beacon = surface, M_RusFactory_BeaconLF = light function, 5 MI pairs).
Mesh-space positions (the A2 FBX frame) are mapped through BuildingMeshComponent's relative transform.
"""
import json

import unreal

BP_PATH = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Building_Factory'
SPARKS = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Ger/Equipment/NS_Sparks_Welding'
TAG = 'AITrainingActivity'
SUBS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary
OUT = {'log': []}

# name: (class, mesh-space location cm, yaw, settings)
EXIT = ('AITrainingExit', unreal.SceneComponent, (1925.0, 150.0, 50.0), 0.0, {})
# (name, mesh-space location, colour, share of the reference flood's intensity, attenuation cm)
# The yard's floods hang ~10 m up; these sit 2-3 m from walls, so the full 6000 cd burnt the gate and bay white
# (2026-11-19 capture): 0.2 / 0.08 of it.
LIGHTS = [
    ('AI_GateWorkLight', (1250.0, 150.0, 420.0), unreal.Color(r=255, g=186, b=118, a=255), 0.2, 900.0),
    ('AI_BayWeldLight', (430.0, -230.0, 720.0), unreal.Color(r=176, g=206, b=255, a=255), 0.08, 450.0),
]
# Intensity reference: the Soviet Assembly Yard's floodlights (same kind of work light in the same scenes).
LIGHT_REFERENCES = ['/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/AssemblyYard/Rus_Building_AssemblyYard',
                    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_Bunker02_ZIS']
FALLBACK_LIGHT = (unreal.LightUnits.CANDELAS, 6000.0)  # = the yard floods on 2026-11-19 -> gate 1200 cd, bay 480 cd
EFFECTS = [
    ('AI_GateSparks', (1085.0, 40.0, 120.0)),
    ('AI_BaySparks', (430.0, -220.0, 668.0)),
]

# ---------------------------------------------------------------- roof beacons
FOLDER = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/FactoryA2/AITraining'
GLOW_TAG = 'AITrainingGlow'
HOUSING_MESH = '/Engine/BasicShapes/Cylinder'  # 100 cm dia x 100 cm, pivot at the centre
LENS_MESH = '/Engine/BasicShapes/Sphere'  # 100 cm dia, pivot at the centre
BRACKET_MESH = '/Engine/BasicShapes/Cube'  # 100 cm, pivot at the centre
# Tracked crane steel (MI_metal on the factory mesh lives in the gitignored /Game/Bunkers pack).
HOUSING_MATERIAL = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/BreakthroughShared/M_BF_Crane_WornSteel'
HOUSING_DIA, HOUSING_H, LENS_DIA, LENS_SINK = 30.0, 12.0, 34.0, 3.0  # cm
LIGHT_ABOVE_BASE = 100.0  # cm: 150 cd -> ~150 lux on the roof under it (48 cm gave ~650 lux, too hot)
AMBER = unreal.LinearColor(r=1.0, g=0.36, b=0.03, a=1.0)
RED = unreal.LinearColor(r=1.0, g=0.03, b=0.01, a=1.0)
AMBER_LIGHT = unreal.Color(r=255, g=150, b=40, a=255)
# group: (lens colour, BlinkRate Hz, BlinkPhase cycles, BlinkSharpness, light colour, light cd, attenuation cm)
# Four amber phases a quarter cycle apart: a chase round the roof; at any moment one lamp is >= 62 % and one ~0 %.
BLINK = {
    'Amber0': (AMBER, 1.1, 0.0, 3.0, AMBER_LIGHT, 150.0, 450.0),
    'Amber1': (AMBER, 1.1, 0.25, 3.0, AMBER_LIGHT, 150.0, 450.0),
    'Amber2': (AMBER, 1.1, 0.5, 3.0, AMBER_LIGHT, 150.0, 450.0),
    'Amber3': (AMBER, 1.1, 0.75, 3.0, AMBER_LIGHT, 150.0, 450.0),
    'Red': (RED, 0.6, 0.0, 4.0, unreal.Color(r=255, g=40, b=20, a=255), 200.0, 500.0),
}
# Most of the roof lies under two camo nets (M_Pattern_1, ~10 cm over the concrete), so the lamps stand on the bare
# concrete (6.35 m): the strips beside the crane bay and the back (-X) band. Downward raycasts with the hit material on
# the A2 render mesh, 2026-11-19. Bay rails: x 100 / 750 (upper 775), y -170..465; leaves x 85..775.
# (spot, mesh-space base centre cm, blink group, bracket: None or (centre, size) cm of a Cube under the housing)
BEACONS = [
    ('GateLeft', (760.0, -270.0, 635.0), 'Amber0', None),  # front strip, +X end: beside the gate, -Y of the bay
    ('GateRight', (810.0, 500.0, 635.0), 'Amber1', None),  # +Y strip end over the gate's +Y edge (pipe at y 550)
    ('BackRight', (-470.0, 940.0, 635.0), 'Amber2', None),  # back band, +Y end (pipe rail at x ~-420)
    ('BackLeft', (-500.0, -270.0, 635.0), 'Amber3', None),  # back band, -Y end (net edge at y ~-330)
    # Red obstruction lamp on a bracket off the stack's +X wall (pipe centre (-130, 165), r 83, outer wall x -47 at
    # y 169, rim 12.7 m); lens top ~13 cm over the rim.
    ('Stack', (-31.0, 169.0, 1240.0), 'Red', ((-33.0, 169.0, 1238.0), (36.0, 10.0, 4.0))),
]
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def log(msg):
    OUT['log'].append(msg)
    unreal.log('RUSA2_TRAINER ' + msg)


def gather(bp):
    rows = []
    for handle in SUBS.k2_gather_subobject_data_for_blueprint(bp):
        data = LIB.get_data(handle)
        rows.append({'handle': handle, 'data': data, 'name': str(LIB.get_variable_name(data)),
                     'obj': LIB.get_object_for_blueprint(data, bp)})
    return rows


def describe(bp, rows):
    tree = []
    for row in rows:
        obj, data = row['obj'], row['data']
        parent = ''
        for other in rows:
            if other is not row:
                try:
                    if LIB.is_attached_to(data, other['handle']):
                        parent = other['name']
                        break
                except Exception:
                    pass
        item = {'name': row['name'], 'class': obj.get_class().get_name() if obj else None, 'parent': parent,
                'root': bool(LIB.is_root_component(data)), 'inherited': bool(LIB.is_inherited_component(data))}
        if isinstance(obj, unreal.SceneComponent):
            item['loc'] = str(obj.get_editor_property('relative_location'))
            item['rot'] = str(obj.get_editor_property('relative_rotation'))
            item['tags'] = [str(t) for t in obj.get_editor_property('component_tags')]
        tree.append(item)
    return tree


def find(rows, name):
    return next((r for r in rows if r['name'] == name), None)


def add(bp, rows, parent_handle, cls, name):
    row = find(rows, name)
    if row:
        log('kept ' + name)
        return row['obj']
    params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=cls, blueprint_context=bp)
    result = SUBS.add_new_subobject(params)
    handle, fail = (result if isinstance(result, tuple) else (result, None))
    if fail is not None and not fail.is_empty():
        raise RuntimeError('add %s failed: %s' % (name, fail))
    if not SUBS.rename_subobject(handle, unreal.Text(name)):
        raise RuntimeError('rename to %s failed' % name)
    obj = LIB.get_object_for_blueprint(LIB.get_data(handle), bp)
    log('added %s (%s) as %s' % (name, cls.__name__, obj.get_name()))
    return obj


def place(comp, mesh_xf, loc, yaw=0.0):
    p = mesh_xf.transform_location(unreal.Vector(*loc))
    comp.set_editor_property('relative_location', p)
    comp.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0,
                                                                  yaw=yaw + mesh_xf.rotation.rotator().yaw))


def tag(comp, name=TAG):
    tags = [str(t) for t in comp.get_editor_property('component_tags')]
    if name not in tags:
        tags.append(name)
    comp.set_editor_property('component_tags', [unreal.Name(t) for t in tags])


def reference_light():
    """Units + intensity of the brightest point/spot light template in the reference Blueprints (read only)."""
    found = []
    for path in LIGHT_REFERENCES:
        ref = unreal.load_asset(path)
        if ref is None:
            continue
        for row in gather(ref):
            obj = row['obj']
            if isinstance(obj, (unreal.PointLightComponent, unreal.SpotLightComponent)):
                item = {'bp': path.rsplit('/', 1)[-1], 'name': row['name'], 'class': obj.get_class().get_name(),
                        'units': str(obj.get_editor_property('intensity_units')),
                        'intensity': obj.get_editor_property('intensity'),
                        'attenuation': obj.get_editor_property('attenuation_radius'),
                        'color': str(obj.get_editor_property('light_color')),
                        'visible': obj.get_editor_property('visible'),
                        'tags': [str(t) for t in obj.get_editor_property('component_tags')]}
                found.append((item, obj))
    OUT['reference_lights'] = [f[0] for f in found]
    if not found:
        return FALLBACK_LIGHT
    # prefer the Assembly Yard; within it the brightest
    yard = [f for f in found if f[0]['bp'] == 'Rus_Building_AssemblyYard'] or found
    best = max(yard, key=lambda f: f[0]['intensity'])[1]
    return best.get_editor_property('intensity_units'), best.get_editor_property('intensity')


def node(mat, cls, x, y, **props):
    expr = MEL.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


def wire(src, dst, pin):
    # The pin's display name can differ from the C++ member (Power's 'Exponent' input; single-input nodes like Sine may
    # use the blank name), so try the names the node itself reports that match the wanted pin.
    try:
        names = [str(n) for n in MEL.get_material_expression_input_names(dst)]
    except Exception:
        names = []
    want = pin.lower()
    candidates = [pin] + [n for n in names if n.lower() == want or want.startswith(n.lower()) or n.lower().startswith(want)]
    if pin == 'Input' or len(names) == 1:
        candidates.append('')
    for name in candidates:
        if MEL.connect_material_expressions(src, '', dst, name):
            return
    raise RuntimeError('connect %s -> %s.%s failed (node inputs %s)' % (src.get_name(), dst.get_name(), pin, names))


def binary(mat, cls, a, b, x, y):
    expr = node(mat, cls, x, y)
    wire(a, expr, 'A')
    wire(b, expr, 'B')
    return expr


def pulse_nodes(mat, x=-1200, y=0):
    """pow(0.5 + 0.5 * sin(2 pi (Time * BlinkRate + BlinkPhase)), BlinkSharpness): a flash once per cycle."""
    time = node(mat, unreal.MaterialExpressionTime, x, y)
    rate = node(mat, unreal.MaterialExpressionScalarParameter, x, y + 120, parameter_name='BlinkRate', default_value=1.1)
    phase = node(mat, unreal.MaterialExpressionScalarParameter, x, y + 240, parameter_name='BlinkPhase', default_value=0.0)
    sharp = node(mat, unreal.MaterialExpressionScalarParameter, x, y + 360, parameter_name='BlinkSharpness',
                 default_value=3.0)
    cycles = binary(mat, unreal.MaterialExpressionMultiply, time, rate, x + 220, y + 40)
    shifted = binary(mat, unreal.MaterialExpressionAdd, cycles, phase, x + 400, y + 120)
    sine = node(mat, unreal.MaterialExpressionSine, x + 580, y + 120, period=1.0)
    wire(shifted, sine, 'Input')
    unit = node(mat, unreal.MaterialExpressionConstantBiasScale, x + 760, y + 120, bias=1.0, scale=0.5)
    wire(sine, unit, 'Input')
    power = node(mat, unreal.MaterialExpressionPower, x + 960, y + 200)
    wire(unit, power, 'Base')
    wire(sharp, power, 'Exponent')
    return power


def fresh_material(name):
    path = FOLDER + '/' + name
    if EAL.does_asset_exist(path):
        mat = unreal.load_asset(path)
        MEL.delete_all_material_expressions(mat)
        log('rebuilt ' + name)
    else:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, FOLDER, unreal.Material,
                                                                      unreal.MaterialFactoryNew())
        log('created ' + name)
    return mat


def save(asset):
    if not EAL.save_loaded_asset(asset, False):
        raise RuntimeError('save failed: ' + asset.get_path_name())


def build_beacon_materials():
    """Lens material (surface) + light-function material, one MI pair per blink group; returns {group: (lens, lf)}."""
    lens = fresh_material('M_RusFactory_Beacon')
    lens.set_editor_property('material_domain', unreal.MaterialDomain.MD_SURFACE)
    pulse = pulse_nodes(lens)
    color = node(lens, unreal.MaterialExpressionVectorParameter, -400, -360, parameter_name='BeaconColor',
                 default_value=AMBER)
    glow = node(lens, unreal.MaterialExpressionScalarParameter, -400, -220, parameter_name='BeaconGlow',
                default_value=40.0)
    # glow between flashes while training; 0.06 (x40 = 2.4) saturated the lens in every training capture, so the lens
    # itself never looked like it flashed (2026-11-19 run factory_20261119_085449)
    ember = node(lens, unreal.MaterialExpressionScalarParameter, -400, 520, parameter_name='BeaconIdleGlow',
                 default_value=0.005)
    active = node(lens, unreal.MaterialExpressionScalarParameter, -400, 640, parameter_name='AITrainingActive',
                  default_value=0.0)  # set by UEnemyBuildingTrainerComponent on AITrainingGlow meshes
    tint = node(lens, unreal.MaterialExpressionScalarParameter, -400, -500, parameter_name='LensTint',
                default_value=0.35)
    lit = binary(lens, unreal.MaterialExpressionAdd, pulse, ember, 0, 300)
    hot = binary(lens, unreal.MaterialExpressionMultiply, color, glow, 0, -300)
    flash = binary(lens, unreal.MaterialExpressionMultiply, hot, lit, 200, 0)
    gated = binary(lens, unreal.MaterialExpressionMultiply, flash, active, 400, 100)
    base = binary(lens, unreal.MaterialExpressionMultiply, color, tint, 200, -500)
    rough = node(lens, unreal.MaterialExpressionConstant, 200, -200, r=0.25)
    MEL.connect_material_property(gated, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(lens)
    save(lens)

    lf = fresh_material('M_RusFactory_BeaconLF')
    lf.set_editor_property('material_domain', unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    MEL.connect_material_property(pulse_nodes(lf), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(lf)
    save(lf)

    tools = unreal.AssetToolsHelpers.get_asset_tools()

    def instance(name, parent, scalars, vectors):
        path = FOLDER + '/' + name
        if EAL.does_asset_exist(path):
            inst = unreal.load_asset(path)
        else:
            inst = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(inst, parent)
        for key, value in scalars.items():
            MEL.set_material_instance_scalar_parameter_value(inst, key, value)
        for key, value in vectors.items():
            MEL.set_material_instance_vector_parameter_value(inst, key, value)
        MEL.update_material_instance(inst)
        save(inst)
        return inst

    out = {}
    for group, (colour, rate, phase, sharp, _, _, _) in BLINK.items():
        blink = {'BlinkRate': rate, 'BlinkPhase': phase, 'BlinkSharpness': sharp}
        out[group] = (instance('MI_RusFactory_Beacon_' + group, lens, blink, {'BeaconColor': colour}),
                      instance('MI_RusFactory_BeaconLF_' + group, lf, blink, {}))
    log('beacon materials saved: ' + ', '.join(sorted(out)))
    return out


def setup_static(comp, mesh, material, scale, shadow):
    comp.set_editor_property('static_mesh', mesh)
    comp.set_editor_property('override_materials', [material])
    comp.set_editor_property('relative_scale3d', scale)
    comp.set_collision_profile_name('NoCollision')
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    try:  # not exposed as a method to python; harmless with NoCollision anyway
        comp.set_editor_property('generate_overlap_events', False)
    except Exception:
        pass
    try:
        comp.set_can_ever_affect_navigation(False)
    except Exception:
        comp.set_editor_property('can_ever_affect_navigation', False)
    comp.set_editor_property('cast_shadow', shadow)


def add_beacons(bp, rows, root_handle, mesh_xf):
    machinery = find(rows, 'A2_Machinery')
    if machinery is None:
        raise RuntimeError('A2_Machinery not found')
    rel = machinery['obj'].get_editor_property('relative_location')
    if abs(rel.x) + abs(rel.y) + abs(rel.z) > 0.01 or abs(machinery['obj'].get_editor_property('relative_rotation').yaw) > 0.01:
        raise RuntimeError('A2_Machinery is not at the identity; beacon positions assume it is')
    materials = build_beacon_materials()
    housing_mesh, lens_mesh = unreal.load_asset(HOUSING_MESH), unreal.load_asset(LENS_MESH)
    bracket_mesh, housing_mat = unreal.load_asset(BRACKET_MESH), unreal.load_asset(HOUSING_MATERIAL)
    if not (housing_mesh and lens_mesh and bracket_mesh and housing_mat):
        raise RuntimeError('missing beacon mesh or housing material')
    statics = []
    for spot, (x, y, z), group, bracket in BEACONS:
        lens_mi, lf_mi = materials[group]
        _, _, _, _, light_color, candela, radius = BLINK[group]
        housing = add(bp, rows, machinery['handle'], unreal.StaticMeshComponent, 'AI_Beacon_%s_Housing' % spot)
        place(housing, mesh_xf, (x, y, z + HOUSING_H / 2.0))
        setup_static(housing, housing_mesh, housing_mat,
                     unreal.Vector(HOUSING_DIA / 100.0, HOUSING_DIA / 100.0, HOUSING_H / 100.0), True)
        statics.append(housing)
        if bracket:
            (bx, by, bz), (sx, sy, sz) = bracket
            arm = add(bp, rows, machinery['handle'], unreal.StaticMeshComponent, 'AI_Beacon_%s_Bracket' % spot)
            place(arm, mesh_xf, (bx, by, bz))
            setup_static(arm, bracket_mesh, housing_mat, unreal.Vector(sx / 100.0, sy / 100.0, sz / 100.0), True)
            statics.append(arm)
        lens = add(bp, rows, machinery['handle'], unreal.StaticMeshComponent, 'AI_Beacon_%s_Lens' % spot)
        place(lens, mesh_xf, (x, y, z + HOUSING_H + LENS_DIA / 2.0 - LENS_SINK))
        setup_static(lens, lens_mesh, lens_mi, unreal.Vector(LENS_DIA / 100.0, LENS_DIA / 100.0, LENS_DIA / 100.0), False)
        tag(lens, GLOW_TAG)
        statics.append(lens)
        light = add(bp, rows, root_handle, unreal.PointLightComponent, 'AI_Beacon_%s_Light' % spot)
        place(light, mesh_xf, (x, y, z + LIGHT_ABOVE_BASE))
        light.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
        light.set_editor_property('intensity', candela)
        light.set_editor_property('light_color', light_color)
        light.set_editor_property('attenuation_radius', radius)
        light.set_editor_property('source_radius', 10.0)
        light.set_editor_property('cast_shadows', False)
        light.set_editor_property('light_function_material', lf_mi)
        light.set_editor_property('visible', False)
        tag(light)
    check_beacons(bp, statics)


def check_beacons(bp, statics):
    """Raise before the save if the beacons are not where the visibility nodes expect them, or still collide."""
    rows = gather(bp)
    machinery = find(rows, 'A2_Machinery')['handle']

    def under(name):
        return bool(LIB.is_attached_to(find(rows, name)['data'], machinery))  # ancestor test

    spot = BEACONS[0][0]
    got = {'A2_Crane': under('A2_Crane'), 'lens': under('AI_Beacon_%s_Lens' % spot),
           'housing': under('AI_Beacon_%s_Housing' % spot), 'light': under('AI_Beacon_%s_Light' % spot)}
    log('parent check (expect crane/lens/housing True, light False): %s' % got)
    if got != {'A2_Crane': True, 'lens': True, 'housing': True, 'light': False}:
        raise RuntimeError('beacon parenting unexpected: %s' % got)
    for comp in statics:
        profile = str(comp.get_collision_profile_name())
        enabled = comp.get_collision_enabled()
        if profile != 'NoCollision' or enabled != unreal.CollisionEnabled.NO_COLLISION:
            raise RuntimeError('%s still collides: %s %s' % (comp.get_name(), profile, enabled))
    log('collision check: %d beacon meshes NoCollision' % len(statics))


def main():
    # Fixed to the yard's values as read on 2026-11-19 (Candelas, 6000), so a rerun cannot drift with later yard edits;
    # reference_light() still logs what the yard has now.
    log('light reference now: units=%s intensity=%s (using CANDELAS 6000)' % reference_light())
    ref_units, ref_intensity = FALLBACK_LIGHT
    bp = unreal.load_asset(BP_PATH)
    rows = gather(bp)
    OUT['before'] = describe(bp, rows)
    root = next((r for r in rows if LIB.is_root_component(r['data'])), None)
    if root is None:
        raise RuntimeError('no root component')
    actor_handle = rows[0]['handle']
    mesh = find(rows, 'BuildingMeshComponent')['obj']
    mesh_xf = unreal.Transform(mesh.get_editor_property('relative_location'),
                               mesh.get_editor_property('relative_rotation'),
                               mesh.get_editor_property('relative_scale3d'))
    try:
        if not LIB.is_attached_to(find(rows, 'BuildingMeshComponent')['data'], root['handle']) and \
                root['name'] != 'BuildingMeshComponent':
            log('WARNING BuildingMeshComponent is not directly under the root; positions use its relative transform')
    except Exception as exc:
        log('attach check skipped: %r' % exc)
    if root['name'] == 'BuildingMeshComponent':
        mesh_xf = unreal.Transform()
    log('root=%s mesh_rel=%s' % (root['name'], mesh_xf))

    trainer = add(bp, rows, actor_handle, unreal.EnemyBuildingTrainerComponent, 'EnemyBuildingTrainer')
    trainer.set_editor_property('trainer_kind', unreal.EnemyBuildingTrainerKind.VEHICLES)
    trainer.set_editor_property('idle_animation', unreal.EnemyTrainerIdleAnimation.FINISH_CYCLE)
    trainer.set_editor_property('training_time_mlt', 1.0)
    trainer.set_editor_property('exit_point_component_name', unreal.Name('AITrainingExit'))

    crane = find(rows, 'A2_Crane')['obj']
    tag(crane)
    play = crane.get_editor_property('animation_data')
    play.set_editor_property('saved_playing', False)
    play.set_editor_property('saved_looping', True)
    play.set_editor_property('saved_position', 0.0)
    crane.set_editor_property('animation_data', play)

    name, cls, loc, yaw, _ = EXIT
    exit_comp = add(bp, rows, root['handle'], cls, name)
    place(exit_comp, mesh_xf, loc, yaw)

    for name, loc, color, share, radius in LIGHTS:
        light = add(bp, rows, root['handle'], unreal.PointLightComponent, name)
        place(light, mesh_xf, loc)
        light.set_editor_property('intensity_units', ref_units)
        light.set_editor_property('intensity', ref_intensity * share)
        light.set_editor_property('light_color', color)
        light.set_editor_property('attenuation_radius', radius)
        light.set_editor_property('source_radius', 20.0)
        light.set_editor_property('cast_shadows', False)
        light.set_editor_property('visible', False)
        tag(light)

    sparks = unreal.load_asset(SPARKS)
    if sparks is None:
        raise RuntimeError('missing ' + SPARKS)
    for name, loc in EFFECTS:
        fx = add(bp, rows, root['handle'], unreal.NiagaraComponent, name)
        place(fx, mesh_xf, loc)
        fx.set_editor_property('asset', sparks)
        fx.set_editor_property('auto_activate', False)
        tag(fx)

    add_beacons(bp, rows, root['handle'], mesh_xf)

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    OUT['after'] = describe(bp, gather(bp))
    if not unreal.EditorAssetLibrary.save_loaded_asset(bp, False):
        raise RuntimeError('save failed')
    dirty = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    OUT['dirty_after_save'] = dirty
    log('saved')


try:
    main()
    OUT['ok'] = True
except Exception as exc:  # report and keep the commandlet exit clean
    OUT['ok'] = False
    OUT['error'] = repr(exc)
    import traceback
    OUT['trace'] = traceback.format_exc()
unreal.log('RUSA2_TRAINER_RESULT ' + json.dumps(OUT, default=str))
