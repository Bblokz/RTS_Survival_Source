"""Fence breaches: the destructible fences on field-texture holes + UWallTankBreachComponent (no owner side).
- Wire fences (BP_Industrial_Dest_FenceWire, BP_Airport_Dest_FenceWire): AnyVehicle, every vehicle (light, medium,
  heavy) of every player drives through; infantry go round (UNavArea_BreachableAnyVehicle).
- Concrete fences (Military ConcreteFence_1/_2, PostApo ConcreteFence/Corner/Gate/Long): AnySide, medium and heavy
  vehicles of every player break them; light vehicles and infantry go round (UNavArea_BreachableAnySideMediumHeavy),
  as the old BlockCharLight area and MediumOrHeavy crush had it.
A vehicle tears a gap and the fence loses health for the share of its face removed (removed share x max health); the
breach component replaces the Blueprints' old instant crush (SetupCrushDestructionOverlap returns early when one is
present). Shell holes keep working, now in field mode (no slot merging).
Run in a DebugGame commandlet (needs RTS.WallBreach.ConfigureRusWall with AnyVehicle/AnySide/AddHoles):
    sh run_fence_breach_setup.sh

Wall lines (mesh space, measured from the vertices 2026-11-18):
- SM_RTS_Fence_Wire_1: chain-link plane at x = -8 (posts' outriggers lean to -80), y -7..358, thin.
- SM_Army_Fence_Wired (airport): along x -3..373 at y = 0 (wires and posts spread y -40..40). No holes before: the
  setup adds a WallHoleDamage (OwnDeath: the BP keeps its destroyed-mesh swap; the hole materials come off first).
- SM_Scorch_MilitaryWalls1: along x -326..326 at y = 0, 92 cm thick, 6.3 m tall; slot 1 is the barbed wire on top.
- SM_Scorch_Fence_Concrete: along y -170..170 at x = 0, 20 cm; the corner is an L (x = 0 from y -170 to 0, then
  y = 0 to x 175). SM_Container_FenceGate: along y -3..338 at x = -5, sheet metal.
Scaled templates (ConcreteFence_2 X x3, ConcreteFenceLong Y x3) use the mesh-space line; the breach follows the scale.
"""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
ENV = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/'
CONCRETE = ENV + 'Military/ConcreteWall/'
DUST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX/NS_Deploy_HeavyDust'
METAL_SOUND = '/Game/RTS_Survival/Sounds/DestructionAndCollapse/CollapsingMetal/MetalObject/Cue_MetalObj_Destruction'
CONCRETE_SOUND = '/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue'
ATTENUATION = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
CONCRETE_ATTENUATION = ATTENUATION
CONCRETE_CONCURRENCY = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'
FENCE_DEBRIS = '|'.join(f'{ENV}Industrial/Holes/Debris/SM_RTS_Fence_Wire_1_Debris_{i}' for i in range(8))

WIRE_ARGS = {
    'AnyVehicle': 1, 'NoDecal': 1, 'SideMargin': 25, 'VSlope': 0.15, 'EdgeBand': 30, 'StubHeight': 4,
    'NavHalfDepth': 60, 'RubblePieces': 8, 'DustScale': 0.35,
    # Wire costs little momentum: light vehicles lose a quarter of their speed, heavies a tenth.
    'LightShare': 0.75, 'MediumShare': 0.85, 'HeavyShare': 0.9,
    # Wire sags at once instead of hanging over the hull like a concrete slab.
    'TopDelay': 0.15, 'TopDuration': 0.25,
    'Sound': METAL_SOUND,
}
# Concrete fences break like the Rus walls (their defaults), any side, medium and heavy only.
CONCRETE_ARGS = {'AnySide': 1, 'NavHalfDepth': 120, 'Sound': CONCRETE_SOUND}
GATE_ARGS = dict(CONCRETE_ARGS, Sound=METAL_SOUND, EdgeBand=40, StubHeight=6, VSlope=0.2)

# Field instance: (slot instance to copy, source master of the field master, rebar, inner darken, inner colour).
FIELD_INSTANCES = {
    'IndustrialWire': (ENV + 'Industrial/Holes/MI_SM_RTS_Fence_Wire_1_Fence_WallHoles',
                       '/Game/RTS_Survival/Meshes/Industrial/Fences/WireFence/M_Fence', 0.0, 0.3, (0.08, 0.07, 0.06)),
    'AirportWire': ('/Game/ApocalypticCity/Materials/Fence_02_M_Inst', '/Game/ApocalypticCity/Materials/Fence_02_M',
                    0.0, 0.3, (0.08, 0.07, 0.06)),
    'MilitaryConcrete': (CONCRETE + 'Holes/MI_SM_Scorch_MilitaryWalls1_ScorchAtlas_WallHoles',
                         '/Game/RTS_Survival/Meshes/Debris/ScorchedCity/Materials/M_ScorchAtlas', 1.0, 0.55,
                         (0.18, 0.17, 0.16)),
    'MilitaryBarbed': ('/Game/ResearchMegaPack/SecretBase/Materials/M_Barbed_1', None, 0.0, 0.0, (0.1, 0.1, 0.1)),
    'PostApoConcrete': (CONCRETE + 'Holes/MI_SM_Scorch_Fence_Concrete_ScorchAtlas_WallHoles',
                        '/Game/RTS_Survival/Meshes/Debris/ScorchedCity/Materials/M_ScorchAtlas', 1.0, 0.55,
                        (0.18, 0.17, 0.16)),
    'PostApoCorner': (CONCRETE + 'Holes/MI_SM_Scorch_Fence_Concrete_Corner_ScorchAtlas_WallHoles',
                      '/Game/RTS_Survival/Meshes/Debris/ScorchedCity/Materials/M_ScorchAtlas', 1.0, 0.55,
                      (0.18, 0.17, 0.16)),
    'ContainerGate': (CONCRETE + 'Holes/MI_SM_Container_FenceGate_ContainerBlue_WallHoles',
                      '/Game/RTS_Survival/Meshes/Industrial/Container/MeshesMat/M_container_blue', 0.0, 0.4,
                      (0.06, 0.07, 0.08)),
}
FIELD_FOLDERS = {
    'IndustrialWire': ENV + 'Industrial/Holes/', 'AirportWire': ENV + 'Airport/Holes/',
    'MilitaryBarbed': CONCRETE + 'Holes/',
}
# Blueprint: (field instances per slot, path, thickness, args)
FENCES = {
    ENV + 'Industrial/BP_Industrial_Dest_FenceWire': (['IndustrialWire'], '-8,-7;-8,358', 10, WIRE_ARGS),
    ENV + 'Airport/BP_Airport_Dest_FenceWire': (
        ['AirportWire'], '-3,0;373,0', 20,
        dict(WIRE_ARGS, AddHoles='StaticMesh', Debris=FENCE_DEBRIS, OwnDeath=1)),
    CONCRETE + 'BP_Dest_Military_ConcreteFence_1': (['MilitaryConcrete', 'MilitaryBarbed'], '-326,0;326,0', 92,
                                                    CONCRETE_ARGS),
    CONCRETE + 'BP_Dest_Military_ConcreteFence_2': (['MilitaryConcrete', 'MilitaryBarbed'], '-326,0;326,0', 92,
                                                    CONCRETE_ARGS),
    CONCRETE + 'BP_Dest_PostApo_ConcreteFence': (['PostApoConcrete'], '0,-170;0,170', 20, CONCRETE_ARGS),
    CONCRETE + 'BP_Dest_PostApo_ConcreteFenceLong': (['PostApoConcrete'], '0,-170;0,170', 20, CONCRETE_ARGS),
    CONCRETE + 'BP_Dest_PostApo_ConcreteFenceCorner': (['PostApoCorner'], '0,-170;0,0;175,0', 20, CONCRETE_ARGS),
    CONCRETE + 'BP_Dest_PostApo_ConcreteFenceGate': (['ContainerGate'], '-5,-3;-5,338', 10, GATE_ARGS),
}


def _load_field_library():
    namespace = {'__name__': 'wall_field_library'}
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_field_holes_material.py').read(), namespace)
    return namespace


def _field_path(key):
    source = FIELD_INSTANCES[key][0]
    folder = FIELD_FOLDERS.get(key, source.rsplit('/', 1)[0] + '/')
    name = source.rsplit('/', 1)[1]
    if not name.startswith('MI_'):
        name = 'MI_' + name
    return folder + name + ('Field' if name.endswith('_WallHoles') else '_WallHolesField')


def ensure_field_material(key, library):
    source, master_source, rebar, darken, colour = FIELD_INSTANCES[key]
    target = _field_path(key)
    source_asset = unreal.load_asset(source)
    if master_source is None:
        # The source is a master itself (barbed wire): its field copy, and an instance of that.
        master = library['field_master_for'](source)
        instance = unreal.load_asset(target) if EAL.does_asset_exist(target) else \
            unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                target.rsplit('/', 1)[1], target.rsplit('/', 1)[0], unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
    else:
        master = library['field_master_for'](master_source)
        instance = unreal.load_asset(target) if EAL.does_asset_exist(target) else EAL.duplicate_asset(source, target)
    MEL.set_material_instance_parent(instance, master)
    MEL.set_material_instance_scalar_parameter_value(instance, library['REBAR'], float(rebar))
    MEL.set_material_instance_scalar_parameter_value(instance, library['INNER_DARKEN'], float(darken))
    MEL.set_material_instance_vector_parameter_value(instance, library['INNER_COLOR'], unreal.LinearColor(*colour, 1.0))
    overrides = instance.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_two_sided', True)
    overrides.set_editor_property('two_sided', True)
    instance.set_editor_property('base_property_overrides', overrides)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    unreal.log(f'FENCE_BREACH material {key}: {instance.get_path_name()} on {master.get_name()} (from {source_asset.get_name()})')
    return instance.get_path_name().split('.')[0]


def configure_fence(only=None, save=True):
    library = _load_field_library()
    paths = {}
    for blueprint, (slots, path, thickness, args) in FENCES.items():
        if only and not any(o in blueprint for o in only):
            continue
        materials = []
        for key in slots:
            if key not in paths:
                paths[key] = ensure_field_material(key, library)
            materials.append(paths[key])
        extra = ' '.join(f'{k}={v}' for k, v in args.items())
        attenuation = ATTENUATION if args is WIRE_ARGS or args.get('AnyVehicle') else CONCRETE_ATTENUATION
        command = (f'RTS.WallBreach.ConfigureRusWall Blueprint={blueprint} Materials={"|".join(materials)} '
                   f'Path={path} Thickness={thickness} {extra} Dust={DUST} Attenuation={attenuation} '
                   f'Concurrency={CONCRETE_CONCURRENCY}' + ('' if save else ' NoSave=1'))
        unreal.log('FENCE_BREACH ' + command)
        unreal.SystemLibrary.execute_console_command(None, command)


if __name__ != 'fence_breach_library':
    configure_fence()
