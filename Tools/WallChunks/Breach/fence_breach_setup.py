"""Wire fence breaches: BP_Industrial_Dest_FenceWire on field-texture holes + UWallTankBreachComponent in AnyVehicle mode.
Every vehicle (light, medium, heavy) of every player drives through and the fence loses health for the wire it tore
(removed share of the face x max health); infantry go round (UNavArea_BreachableAnyVehicle). The breach component
replaces the Blueprint's old instant crush (SetupCrushDestructionOverlap returns early when one is present).
Run in a DebugGame commandlet (needs RTS.WallBreach.ConfigureRusWall with AnyVehicle):
    sh run_fence_breach_setup.sh

- Material: M_Fence_WallHolesField (field master of the already-masked M_Fence: the chain-link alpha is kept and
  multiplied by the hole field) and MI_SM_RTS_Fence_Wire_1_Fence_WallHolesField, a copy of the slot instance, no rebar.
- Wall line: the chain-link plane is at mesh x = -8 (verts at -15..0; the posts' outriggers lean to -80), along mesh Y
  from -7 to 358. Chain link is thin: Thickness 10.
- The Blueprint's own RTSNavCollisionEnvObstacle (RTS_NavArea_BlockCharLight) is switched off at play by the breach
  component and replaced by centre-line blocks in the any-vehicle area.
"""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
BP = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Industrial/BP_Industrial_Dest_FenceWire'
SOURCE_MASTER = '/Game/RTS_Survival/Meshes/Industrial/Fences/WireFence/M_Fence'
HOLES_DIR = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Industrial/Holes/'
HOLES_MI = HOLES_DIR + 'MI_SM_RTS_Fence_Wire_1_Fence_WallHoles'
FIELD_MI = HOLES_MI + 'Field'
DUST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX/NS_Deploy_HeavyDust'
SOUND = '/Game/RTS_Survival/Sounds/DestructionAndCollapse/CollapsingMetal/MetalObject/Cue_MetalObj_Destruction'
ATTENUATION = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
CONCURRENCY = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'
PATH = '-8,-7;-8,358'
ARGS = {
    'Thickness': 10, 'AnyVehicle': 1, 'NoDecal': 1, 'SideMargin': 25, 'VSlope': 0.15, 'EdgeBand': 30,
    'StubHeight': 4, 'NavHalfDepth': 60, 'RubblePieces': 8, 'DustScale': 0.35,
    # Wire costs little momentum: light vehicles lose a quarter of their speed, heavies a tenth.
    'LightShare': 0.75, 'MediumShare': 0.85, 'HeavyShare': 0.9,
    # Wire sags at once instead of hanging over the hull like a concrete slab.
    'TopDelay': 0.15, 'TopDuration': 0.25,
}


def _load_field_library():
    namespace = {'__name__': 'wall_field_library'}
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_field_holes_material.py').read(), namespace)
    return namespace


def ensure_field_material():
    library = _load_field_library()
    master = library['field_master_for'](SOURCE_MASTER)
    instance = unreal.load_asset(FIELD_MI) if EAL.does_asset_exist(FIELD_MI) else EAL.duplicate_asset(HOLES_MI, FIELD_MI)
    MEL.set_material_instance_parent(instance, master)
    MEL.set_material_instance_scalar_parameter_value(instance, library['REBAR'], 0.0)
    MEL.set_material_instance_scalar_parameter_value(instance, library['INNER_DARKEN'], 0.3)
    MEL.set_material_instance_vector_parameter_value(instance, library['INNER_COLOR'],
                                                     unreal.LinearColor(0.08, 0.07, 0.06, 1.0))
    overrides = instance.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_two_sided', True)
    overrides.set_editor_property('two_sided', True)
    instance.set_editor_property('base_property_overrides', overrides)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    unreal.log('FENCE_BREACH material ' + instance.get_path_name() + ' on ' + master.get_name() + ' ps=' +
               str(MEL.get_statistics(master).num_pixel_shader_instructions))
    return instance


def configure_fence():
    ensure_field_material()
    extra = ' '.join(f'{key}={value}' for key, value in ARGS.items())
    command = (f'RTS.WallBreach.ConfigureRusWall Blueprint={BP} Materials={FIELD_MI} Path={PATH} {extra} '
               f'Dust={DUST} Sound={SOUND} Attenuation={ATTENUATION} Concurrency={CONCURRENCY}')
    unreal.log('FENCE_BREACH ' + command)
    unreal.SystemLibrary.execute_console_command(None, command)


if __name__ != 'fence_breach_library':
    configure_fence()
