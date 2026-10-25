"""Verify the dedicated Trains lighting assets and save the finished map."""
import json
import math
import unreal

EditorWorld = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert EditorWorld.get_name() == 'Ger_Trains'
ActorSubsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
Rays = [Actor for Actor in ActorSubsystem.get_all_level_actors() if Actor.get_actor_label().startswith('FakeGodRay_Trains_')]
assert len(Rays) == 14
MaterialPaths = set()
for Ray in Rays:
    Material = Ray.get_editor_property('RayMaterial')
    assert Material
    assert Material.get_editor_property('parent').get_name() == 'M_FakeGodRay_Trains'
    MaterialPaths.add(Material.get_path_name())
    assert not Ray.is_actor_tick_enabled()
    Components = Ray.get_components_by_class(unreal.StaticMeshComponent)
    assert len(Components) == 3
    for Component in Components:
        assert Component.get_collision_enabled() == unreal.CollisionEnabled.NO_COLLISION
        assert not Component.get_editor_property('cast_shadow')
assert len(MaterialPaths) == len(Rays), 'Each ray must own its material instance.'
ClosestPair = min(math.hypot(First.get_actor_location().x - Second.get_actor_location().x,
                            First.get_actor_location().y - Second.get_actor_location().y)
                  for Index, First in enumerate(Rays) for Second in Rays[Index + 1:])
assert ClosestPair >= 11000.0

Ray = Rays[0]
OriginalFade = Ray.get_editor_property('RayFade')
OriginalEmissive = Ray.get_editor_property('EmissiveStrength')
try:
    Ray.call_method('SetRayFade', (0.0,))
    assert Ray.get_editor_property('RayMaterial').get_scalar_parameter_value('RayFade') == 0.0
    Ray.call_method('SetEmissiveStrength', (0.0,))
    assert Ray.get_editor_property('RayMaterial').get_scalar_parameter_value('EmissiveStrength') == 0.0
finally:
    Ray.call_method('SetRayFade', (OriginalFade,))
    Ray.call_method('SetEmissiveStrength', (OriginalEmissive,))

Collection = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/MPC_TrainsSunbreaks')
Defaults = {str(Parameter.get_editor_property('parameter_name')): Parameter.get_editor_property('default_value')
            for Parameter in Collection.get_editor_property('scalar_parameters')}
for Name, Value in Defaults.items():
    unreal.MaterialLibrary.set_scalar_parameter_value(EditorWorld, Collection, Name, Value)
Probe = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/M_TrainsSunbreakProbe')
Resolution = 128
RenderTarget = unreal.RenderingLibrary.create_render_target2d(EditorWorld, Resolution, Resolution)
try:
    unreal.RenderingLibrary.draw_material_to_render_target(EditorWorld, RenderTarget, Probe)
    Samples = unreal.RenderingLibrary.read_render_target_raw(EditorWorld, RenderTarget, normalize=False)
    Values = sorted(Sample.r for Sample in Samples)
    Quantiles = [round(Values[int(Quantile * (len(Values) - 1))], 3) for Quantile in [0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0]]
    ShadedFraction = sum(Value < 0.65 for Value in Values) / len(Values)
    for ParameterName in ['SunTransmission', 'ShadeTransmission']:
        unreal.MaterialLibrary.set_scalar_parameter_value(EditorWorld, Collection, ParameterName, 0.0)
    unreal.RenderingLibrary.draw_material_to_render_target(EditorWorld, RenderTarget, Probe)
    OffSamples = unreal.RenderingLibrary.read_render_target_raw(EditorWorld, RenderTarget, normalize=False)
    assert max(Sample.r for Sample in OffSamples) < 0.001, 'Shared transmission zero must extinguish the field.'
finally:
    for Name, Value in Defaults.items():
        unreal.MaterialLibrary.set_scalar_parameter_value(EditorWorld, Collection, Name, Value)
    unreal.RenderingLibrary.release_render_target2d(RenderTarget)

AssetPaths = [
    '/Game/RTS_Survival/Materials/Sky/MPC_TrainsSunbreaks',
    '/Game/RTS_Survival/Materials/Sky/MF_TrainsSunbreakField',
    '/Game/RTS_Survival/Materials/Sky/M_LF_TrainsSunbreaks',
    '/Game/RTS_Survival/Materials/Sky/M_TrainsSunbreakProbe',
    '/Game/RTS_Survival/SpecialEffects/Atmosphere/M_FakeGodRay_Trains',
    '/Game/RTS_Survival/SpecialEffects/Atmosphere/BP_FakeGodRayTrains',
]
for AssetPath in AssetPaths:
    assert unreal.EditorAssetLibrary.save_loaded_asset(unreal.load_asset(AssetPath)), AssetPath
assert unreal.EditorLoadingAndSavingUtils.save_map(EditorWorld, '/Game/RTS_Survival/Maps/GerMissions/Ger_Trains')
print('VERIFIED_JSON', json.dumps({'ray_count': len(Rays), 'closest_pair_cm': ClosestPair,
                                 'mask_quantiles': Quantiles, 'fraction_below_0_65': ShadedFraction,
                                 'independent_material_instances': True, 'fade_and_emissive_setters': 'passed',
                                 'zero_transmission_probe': 'passed', 'assets_and_map_saved': True,
                                 'controls': Defaults}))
