"""Surface look options for the Reichstag ruin kit (run with the UECP execute_python action in the shared editor).

Makes the option instances under ReichstagRuin/Materials/Looks; nothing is assigned to the meshes or the Blueprint here.
The throwaway capture run (ue_reichstag_looks_capture.py) swaps them onto the placed ruin in UT_ReichstagRuin per option.

Options (slot RT_Stone / RT_Rubble / RT_Steel):
- A Sandstein:   the Brandenburg Gate's stone (M_BG_Sandstone_Procedural, same tint), so the two landmarks match.
- B Brandruine:  the same master, burnt: darker, heavy streaks and grime, little dust (the 1945 photos).
- C Staubgrau:   the same master, cool grey under heavy bomb dust, with the brick core showing in the rubble.
  (The map's own DBK ruin materials were tried first and fail on the kit: they expect DBK's UV atlas and show paint bands.)
The kit has no per-mesh maps, so A/B use the Breakthrough "UVTile" switch set (no curvature map, no baked fields, no soot
sources). Rebar is off: the walls are masonry. The kit's instances also switch on BG_PerInstanceVariation (added to the
Gate master here, off by default) so ISM copies of one piece get different damage.
"""
import unreal

ROOT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/ReichstagRuin'
LOOKS = ROOT + '/Materials/Looks'
GATE = '/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/'
MASTER = GATE + 'M_BG_Sandstone_Procedural'
GATE_STONE = GATE + 'MI_BG_Sandstone'
RUST = GATE + 'MI_BG_RebarRust'
EDIT = unreal.MaterialEditingLibrary
ASSET_TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

KIT_SWITCHES = dict(BF_UseBakedSurface=True, BF_UseCurvatureMap=False, BF_UseMeshCurvature=False, BF_UseBakedFields=False,
                    BF_UseHeatSoot=False, BF_UseHeat=False, BF_UseRebar=False, BF_UseDamage=True, BF_UseCracks=True,
                    BF_UseWeathering=True, BF_UseCamo=False, BF_UseCamoMap=False, BF_UseLampPools=False)
# name: (scalar overrides, tint); scalars start from the Gate's stone instance
STONES = {
    'MI_RT_LookA_Stone': (dict(BF_WearSeed=41.0), (1.50, 1.12, 0.70)),
    'MI_RT_LookA_Rubble': (dict(BF_WearAmount=0.65, BF_SpallAmount=0.8, BF_DustAmount=0.75, BF_StreakAmount=0.25,
                                BF_WearSeed=57.0), (1.70, 1.32, 0.90)),
    'MI_RT_LookB_Stone': (dict(BF_WearAmount=0.65, BF_SpallAmount=0.75, BF_ImpactAmount=0.7, BF_DustAmount=0.3,
                               BF_StreakAmount=1.0, BF_GroundGrimeAmount=1.0, BF_GroundGrimeHeightCm=250.0,
                               BF_OilAmount=0.4, BF_WearSeed=43.0), (0.95, 0.80, 0.60)),
    'MI_RT_LookB_Rubble': (dict(BF_WearAmount=0.7, BF_SpallAmount=0.8, BF_DustAmount=0.5, BF_StreakAmount=0.4,
                                BF_WearSeed=59.0), (1.20, 0.98, 0.72)),
    'MI_RT_LookC_Stone': (dict(BF_WearAmount=0.55, BF_DustAmount=0.95, BF_StreakAmount=0.55, BF_GroundGrimeAmount=0.6,
                               BF_WearSeed=47.0), (1.10, 1.04, 0.95)),
    'MI_RT_LookC_Rubble': (dict(BF_WearAmount=0.7, BF_SpallAmount=0.8, BF_DustAmount=0.85, BF_StreakAmount=0.2,
                                BF_WearSeed=61.0), (1.55, 0.90, 0.66)),
}
# option -> slot -> material path
OPTIONS = {
    'A_Sandstein': {'RT_Stone': f'{LOOKS}/MI_RT_LookA_Stone', 'RT_Rubble': f'{LOOKS}/MI_RT_LookA_Rubble', 'RT_Steel': RUST},
    'B_Brandruine': {'RT_Stone': f'{LOOKS}/MI_RT_LookB_Stone', 'RT_Rubble': f'{LOOKS}/MI_RT_LookB_Rubble', 'RT_Steel': RUST},
    'C_Staubgrau': {'RT_Stone': f'{LOOKS}/MI_RT_LookC_Stone', 'RT_Rubble': f'{LOOKS}/MI_RT_LookC_Rubble', 'RT_Steel': RUST},
}


def save(obj):
    obj.modify()
    if not unreal.EditorLoadingAndSavingUtils.save_packages([obj.get_outermost()], False):
        raise RuntimeError('save failed ' + obj.get_path_name())


VARIATION = 'BG_PerInstanceVariation'
VARIATION_HLSL = """// Per-instance shift of the object-space position the BF layers read (damage, spalls, casting, streaks), so copies of
// one kit piece do not repeat. XY only: z stays 0 at grade for the weathering. Up to 250 m keeps the hashes decorrelated.
return float3(frac(Seed * 1.0) * 25000.0, frac(Seed * 17.31 + 0.37) * 25000.0, 0.0);"""


def walk_graph(mat):
    """Every expression reachable from the material outputs (the 5.5 Python API cannot list a graph directly)."""
    seen, todo = [], []
    for prop in unreal.MaterialProperty:
        try:
            node = EDIT.get_material_property_input_node(mat, prop)
        except Exception:
            node = None
        if node is not None:
            todo.append(node)
    while todo:
        node = todo.pop()
        if node in seen:
            continue
        seen.append(node)
        todo.extend(n for n in EDIT.get_inputs_for_material_expression(mat, node) if n is not None)
    return seen


def add_instance_variation():
    """Adds static switch BG_PerInstanceVariation (default off, so the Brandenburg Gate's permutation is unchanged) between
    the master's local-position transform and its vertex interpolator. Idempotent; incremental on the Gate's own duplicate
    (the BF master itself is never touched). Rerun after the Gate's import script remakes the master."""
    mat = unreal.load_asset(MASTER)
    nodes = walk_graph(mat)
    if any(isinstance(n, unreal.MaterialExpressionStaticSwitchParameter) and str(n.get_editor_property('parameter_name')) == VARIATION
           for n in nodes):
        print('variation already in', MASTER)
        return
    interp = [n for n in nodes if isinstance(n, unreal.MaterialExpressionVertexInterpolator)
              and any(isinstance(i, unreal.MaterialExpressionTransformPosition) for i in EDIT.get_inputs_for_material_expression(mat, n))]
    if len(interp) != 1:
        raise RuntimeError(f'expected one local-position interpolator, found {len(interp)}')
    interp = interp[0]
    local = next(i for i in EDIT.get_inputs_for_material_expression(mat, interp) if isinstance(i, unreal.MaterialExpressionTransformPosition))
    x, y = local.get_editor_property('material_expression_editor_x'), local.get_editor_property('material_expression_editor_y')
    rnd = EDIT.create_material_expression(mat, unreal.MaterialExpressionPerInstanceRandom, x, y + 160)
    code = EDIT.create_material_expression(mat, unreal.MaterialExpressionCustom, x + 220, y + 160)
    code.set_editor_property('code', VARIATION_HLSL)
    code.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    code.set_editor_property('description', 'BG_InstanceShift')
    seed = unreal.CustomInput()
    seed.set_editor_property('input_name', 'Seed')
    code.set_editor_property('inputs', [seed])
    add = EDIT.create_material_expression(mat, unreal.MaterialExpressionAdd, x + 480, y + 80)
    sw = EDIT.create_material_expression(mat, unreal.MaterialExpressionStaticSwitchParameter, x + 640, y)
    sw.set_editor_property('parameter_name', VARIATION)
    sw.set_editor_property('group', '00_Layers')
    sw.set_editor_property('default_value', False)
    sw.set_editor_property('desc', 'Reichstag kit: per-instance shift of the object-space pattern (ISM copies of one piece)')
    assert EDIT.connect_material_expressions(rnd, '', code, 'Seed'), 'seed'
    assert EDIT.connect_material_expressions(local, '', add, 'A'), 'add A'
    assert EDIT.connect_material_expressions(code, '', add, 'B'), 'add B'
    assert EDIT.connect_material_expressions(add, '', sw, 'True'), 'switch true'
    assert EDIT.connect_material_expressions(local, '', sw, 'False'), 'switch false'
    assert EDIT.connect_material_expressions(sw, '', interp, ''), 'interpolator'
    EDIT.recompile_material(mat)
    save(mat)
    print('variation added to', MASTER)


def make_stones():
    gate = unreal.load_asset(GATE_STONE)
    base = {str(n): EDIT.get_material_instance_scalar_parameter_value(gate, n) for n in EDIT.get_scalar_parameter_names(gate)
            if str(n).startswith('BF_')}
    master = unreal.load_asset(MASTER)
    for name, (scalars, tint) in STONES.items():
        mi = unreal.load_asset(f'{LOOKS}/{name}')
        if mi is None:
            mi = ASSET_TOOLS.create_asset(name, LOOKS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        EDIT.set_material_instance_parent(mi, master)
        for k, v in {**KIT_SWITCHES, VARIATION: True}.items():
            EDIT.set_material_instance_static_switch_parameter_value(mi, k, v)
        for k, v in {**base, **scalars}.items():
            EDIT.set_material_instance_scalar_parameter_value(mi, k, v)
        EDIT.set_material_instance_vector_parameter_value(mi, 'BG_StoneTint', unreal.LinearColor(*tint, 1.0))
        EDIT.update_material_instance(mi)
        save(mi)
        print('LOOK', name, 'tint', tint)


if globals().get('RT_LOOKS_BUILD', True):   # the capture script execs this file with RT_LOOKS_BUILD = False for OPTIONS
    add_instance_variation()
    make_stones()
    for opt, slots in OPTIONS.items():
        print('OPTION', opt, {k: bool(unreal.load_asset(v)) for k, v in slots.items()})
