"""Test drift continuity and rendered cloud/ray alignment without relying on wall-clock timing."""
import json
import math
import unreal

World = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
Actors = list(unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors())
Controller = next(Actor for Actor in Actors if Actor.get_actor_label() == 'Trains_SunbreakDrift')
Controller.set_actor_tick_enabled(False)
Collection = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/MPC_TrainsSunbreaks')
OriginalSpeed = next(Parameter.get_editor_property('default_value')
                     for Parameter in Collection.get_editor_property('scalar_parameters')
                     if str(Parameter.get_editor_property('parameter_name')) == 'CloudDriftScale')
Rays = [Actor for Actor in Actors if Actor.get_actor_label().startswith('FakeGodRay_Trains_')]

def SetSpeed(Speed):
    Parameters = list(Collection.get_editor_property('scalar_parameters'))
    for Index, Parameter in enumerate(Parameters):
        if str(Parameter.get_editor_property('parameter_name')) != 'CloudDriftScale':
            continue
        Parameter.set_editor_property('default_value', Speed)
        Parameters[Index] = Parameter
    Collection.set_editor_property('scalar_parameters', Parameters)
    Actual = unreal.MaterialLibrary.get_scalar_parameter_value(World, Collection, 'CloudDriftScale')
    assert abs(Actual - Speed) < 0.0001, (Actual, Speed)

def Offset():
    Value = unreal.MaterialLibrary.get_vector_parameter_value(World, Collection, 'CloudDriftOffset')
    return unreal.Vector(Value.r, Value.g, 0.0)

def Distance(First, Second):
    return math.hypot(First.x - Second.x, First.y - Second.y)

Probe = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/M_TrainsSunbreakProbe')
Position = unreal.load_object(None, Probe.get_path_name() + ':MaterialExpressionCustom_0')
Inputs = list(Position.get_editor_property('inputs'))
if not any(str(Input.get_editor_property('input_name')) == 'ProbeOffset' for Input in Inputs):
    ProbeInput = unreal.CustomInput()
    ProbeInput.set_editor_property('input_name', 'ProbeOffset')
    Inputs.append(ProbeInput)
    Position.set_editor_property('inputs', Inputs)
Position.set_editor_property('code', 'return float3(float2(-42000.0,-51000.0)+UV*float2(58000.0,89000.0)+ProbeOffset.xy,0.0);')
ProbeOffsetParameter = unreal.load_object(None, Probe.get_path_name() + ':MaterialExpressionVectorParameter_0')
if ProbeOffsetParameter is None:
    ProbeOffsetParameter = unreal.MaterialEditingLibrary.create_material_expression(
        Probe, unreal.MaterialExpressionVectorParameter, -600, 300)
ProbeOffsetParameter.set_editor_property('parameter_name', 'ProbeWorldOffset')
ProbeOffsetParameter.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
assert unreal.MaterialEditingLibrary.connect_material_expressions(ProbeOffsetParameter, '', Position, 'ProbeOffset')
unreal.MaterialEditingLibrary.recompile_material(Probe)
assert unreal.EditorAssetLibrary.save_loaded_asset(Probe)
print('DRIFT_TEST_PROBE_READY')

ProbeInstance = unreal.MaterialLibrary.create_dynamic_material_instance(World, Probe)
Target = unreal.RenderingLibrary.create_render_target2d(World, 128, 128)

def RenderValues():
    unreal.RenderingLibrary.draw_material_to_render_target(World, Target, ProbeInstance)
    return [Sample.r for Sample in unreal.RenderingLibrary.read_render_target_raw(World, Target, normalize=False)]

try:
    Controller.call_method('ResetSunbreakDrift')
    SetSpeed(0.32)
    GroundedRays = sum(Ray.get_editor_property('RayMaterial').get_scalar_parameter_value('CloudBoundaryFade') > 0.01 for Ray in Rays)
    assert GroundedRays >= 12, 'Terrain following unexpectedly hid the rays: ' + str(GroundedRays)
    InitialPositions = [Ray.get_actor_location() for Ray in Rays]
    BaselineValues = RenderValues()
    Controller.call_method('AdvanceSunbreakDrift', (1.0,))
    SlowOffset = Offset()
    assert abs(Distance(SlowOffset, unreal.Vector()) - 32.0) < 0.01
    RayMotionErrors = [Distance(Ray.get_actor_location(), unreal.Vector(Position.x + SlowOffset.x, Position.y + SlowOffset.y, 0.0))
                       for Ray, Position in zip(Rays, InitialPositions)]
    assert max(RayMotionErrors) < 0.02
    SetSpeed(32.0)
    assert Distance(Offset(), SlowOffset) < 0.001, 'Changing speed must not jump the current cloud phase.'
    Controller.call_method('AdvanceSunbreakDrift', (0.1,))
    FastOffset = Offset()
    assert abs(Distance(FastOffset, SlowOffset) - 320.0) < 0.02
    SetSpeed(0.0)
    Controller.call_method('AdvanceSunbreakDrift', (10.0,))
    assert Distance(Offset(), FastOffset) < 0.001
    SetSpeed(-0.32)
    Controller.call_method('AdvanceSunbreakDrift', (1.0,))
    assert abs(Distance(Offset(), FastOffset) - 32.0) < 0.02
    SetSpeed(0.32)
    Controller.call_method('AdvanceSunbreakDrift', (30.0,))
    FinalOffset = Offset()
    MovedValues = RenderValues()
    FixedGroundDifference = sum(abs(First - Second) for First, Second in zip(BaselineValues, MovedValues)) / len(BaselineValues)
    assert FixedGroundDifference > 0.005, FixedGroundDifference
    ProbeInstance.set_vector_parameter_value('ProbeWorldOffset', unreal.LinearColor(FinalOffset.x, FinalOffset.y, 0.0, 0.0))
    FollowingValues = RenderValues()
    FollowingDifference = max(abs(First - Second) for First, Second in zip(BaselineValues, FollowingValues))
    assert FollowingDifference < 0.01, FollowingDifference
    print('DRIFT_VERIFICATION_JSON', json.dumps({'rays': len(Rays), 'grounded_rays': GroundedRays, 'slow_speed_cm_per_second': 32.0,
          'fast_speed_cm_per_second': 3200.0, 'speed_change_phase_jump': 0.0,
          'max_ray_alignment_error_cm': max(RayMotionErrors), 'zero_speed_and_reverse': 'passed',
          'fixed_ground_mean_transmission_change': FixedGroundDifference,
          'following_cloud_max_transmission_error': FollowingDifference}))
finally:
    SetSpeed(OriginalSpeed)
    Controller.call_method('ResetSunbreakDrift')
    unreal.RenderingLibrary.release_render_target2d(Target)
    assert unreal.EditorAssetLibrary.save_loaded_asset(Collection)
    Controller.set_actor_tick_enabled(True)
    assert unreal.EditorLoadingAndSavingUtils.save_map(World, '/Game/RTS_Survival/Maps/GerMissions/Ger_Trains')
