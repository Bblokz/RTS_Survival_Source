"""Connect the compiled drift controller to the Trains cloud field and shafts."""
import unreal

SkyFolder = '/Game/RTS_Survival/Materials/Sky/'
RayFolder = '/Game/RTS_Survival/SpecialEffects/Atmosphere/'
World = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert World.get_name() == 'Ger_Trains'
Collection = unreal.load_asset(SkyFolder + 'MPC_TrainsSunbreaks')
VectorParameters = list(Collection.get_editor_property('vector_parameters'))
OffsetParameter = next((Parameter for Parameter in VectorParameters
                        if str(Parameter.get_editor_property('parameter_name')) == 'CloudDriftOffset'), None)
if OffsetParameter is None:
    OffsetParameter = unreal.CollectionVectorParameter()
    OffsetParameter.set_editor_property('parameter_name', 'CloudDriftOffset')
    OffsetParameter.set_editor_property('default_value', unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    VectorParameters.append(OffsetParameter)
    Collection.set_editor_property('vector_parameters', VectorParameters)
assert unreal.EditorAssetLibrary.save_loaded_asset(Collection)

Field = unreal.load_asset(SkyFolder + 'MF_TrainsSunbreakField')
Custom = unreal.load_object(None, Field.get_path_name() + ':MaterialExpressionCustom_0')
Code = Custom.get_editor_property('code')
Code = Code.replace('float2 Wind = float2(1.0, 0.28) * RayDetailDrift * CloudDriftScale;\nfloat2 Ground = Position.xy - Time * Wind;',
                    'float2 Ground = Position.xy - CloudOffset.xy;')
Code = Code.replace('(Position.xy - Time * float2(RayDetailDrift, RayDetailDrift * 0.28))', 'Ground')
assert 'Time *' not in Code, 'All cloud layers must use the accumulated world offset.'
Custom.set_editor_property('code', Code)
Inputs = list(Custom.get_editor_property('inputs'))
if not any(str(Input.get_editor_property('input_name')) == 'CloudOffset' for Input in Inputs):
    OffsetInput = unreal.CustomInput()
    OffsetInput.set_editor_property('input_name', 'CloudOffset')
    Inputs.append(OffsetInput)
    Custom.set_editor_property('inputs', Inputs)
OffsetExpression = unreal.load_object(None, Field.get_path_name() + ':MaterialExpressionCollectionParameter_11')
if OffsetExpression is None:
    OffsetExpression = unreal.MaterialEditingLibrary.create_material_expression_in_function(
        Field, unreal.MaterialExpressionCollectionParameter, -600, 1800)
OffsetExpression.set_editor_property('collection', Collection)
OffsetExpression.set_editor_property('parameter_name', 'CloudDriftOffset')
assert unreal.MaterialEditingLibrary.connect_material_expressions(OffsetExpression, '', Custom, 'CloudOffset')
unreal.MaterialEditingLibrary.update_material_function(Field)
assert unreal.EditorAssetLibrary.save_loaded_asset(Field)

RayMaterial = unreal.load_asset(RayFolder + 'M_FakeGodRay_Trains')
Gate = unreal.load_object(None, RayMaterial.get_path_name() + ':MaterialExpressionCustom_1')
assert 'Transmission' in Gate.get_editor_property('code')
GateInputs = list(Gate.get_editor_property('inputs'))
if not any(str(Input.get_editor_property('input_name')) == 'BoundaryFade' for Input in GateInputs):
    BoundaryInput = unreal.CustomInput()
    BoundaryInput.set_editor_property('input_name', 'BoundaryFade')
    GateInputs.append(BoundaryInput)
    Gate.set_editor_property('inputs', GateInputs)
Gate.set_editor_property('code', 'return smoothstep(0.60,0.88,Transmission) * saturate(BoundaryFade);')
BoundaryExpression = unreal.load_object(None, RayMaterial.get_path_name() + ':MaterialExpressionScalarParameter_14')
if BoundaryExpression is None:
    BoundaryExpression = unreal.MaterialEditingLibrary.create_material_expression(
        RayMaterial, unreal.MaterialExpressionScalarParameter, -700, 2200)
BoundaryExpression.set_editor_property('parameter_name', 'CloudBoundaryFade')
BoundaryExpression.set_editor_property('default_value', 1.0)
BoundaryExpression.set_editor_property('desc', 'Soft recycling at map edges; independent of the artist RayFade control')
assert unreal.MaterialEditingLibrary.connect_material_expressions(BoundaryExpression, '', Gate, 'BoundaryFade')

for Path in [SkyFolder + 'M_LF_TrainsSunbreaks', SkyFolder + 'M_TrainsSunbreakProbe', RayFolder + 'M_FakeGodRay_Trains']:
    Material = unreal.load_asset(Path)
    unreal.MaterialEditingLibrary.recompile_material(Material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(Material)

ActorSubsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
Actors = list(ActorSubsystem.get_all_level_actors())
Controller = next((Actor for Actor in Actors if Actor.get_actor_label() == 'Trains_SunbreakDrift'), None)
if Controller is None:
    ControllerClass = unreal.load_class(None, '/Script/RTS_Survival.TrainsSunbreakDrift')
    assert ControllerClass
    Controller = ActorSubsystem.spawn_actor_from_class(ControllerClass, unreal.Vector(0.0, 0.0, 0.0))
Controller.set_actor_tick_enabled(False)
Controller.set_actor_label('Trains_SunbreakDrift')
Controller.set_folder_path('Lighting/FakeGodRays_Trains')
Rays = [Actor for Actor in Actors if Actor.get_actor_label().startswith('FakeGodRay_Trains_')]
assert len(Rays) == 14
Controller.call_method('ConfigureSunbreakDrift', (Collection, Rays))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_set_viewport_realtime(True)
assert unreal.EditorLoadingAndSavingUtils.save_map(World, '/Game/RTS_Survival/Maps/GerMissions/Ger_Trains')
print('DRIFT_CONTROLLER_AND_MATERIALS_READY', len(Rays))
