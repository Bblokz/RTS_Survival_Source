"""Choose sparse bright landscape anchors and place the Trains ray variant."""
import json
import math
import unreal

EditorWorld = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ActorSubsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert EditorWorld.get_name() == 'Ger_Trains'
Actors = list(ActorSubsystem.get_all_level_actors())
ExistingRays = {Actor.get_actor_label(): Actor for Actor in Actors if Actor.get_actor_label().startswith('FakeGodRay_Trains_')}
Probe = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/M_TrainsSunbreakProbe')
Collection = unreal.load_asset('/Game/RTS_Survival/Materials/Sky/MPC_TrainsSunbreaks')
for Parameter in Collection.get_editor_property('scalar_parameters'):
    unreal.MaterialLibrary.set_scalar_parameter_value(EditorWorld, Collection,
                                                       Parameter.get_editor_property('parameter_name'),
                                                       Parameter.get_editor_property('default_value'))
Resolution = 128
RenderTarget = unreal.RenderingLibrary.create_render_target2d(EditorWorld, Resolution, Resolution)
unreal.RenderingLibrary.draw_material_to_render_target(EditorWorld, RenderTarget, Probe)
Samples = unreal.RenderingLibrary.read_render_target_raw(EditorWorld, RenderTarget, normalize=False)
assert len(Samples) == Resolution * Resolution
print('PROBE_RANGE', min(Sample.r for Sample in Samples), max(Sample.r for Sample in Samples))

Candidates = []
for Row in range(3, Resolution - 3, 3):
    for Column in range(3, Resolution - 3, 3):
        Transmission = Samples[Row * Resolution + Column].r
        if Transmission < 0.76:
            continue
        WorldX = -42000.0 + (Column + 0.5) / Resolution * 58000.0
        WorldY = -51000.0 + (Row + 0.5) / Resolution * 89000.0
        Candidates.append((Transmission, WorldX, WorldY))
Candidates.sort(reverse=True)
BuildingAnchors = [Actor.get_actor_location() for Actor in Actors
                   if any(Word in Actor.get_class().get_name().lower() for Word in ['outpost', 'building', 'bunker', 'factory', 'forge'])]
Selected = []
MinimumSpacing = 11000.0
BuildingClearance = 6000.0
for Transmission, WorldX, WorldY in Candidates:
    if any(math.hypot(WorldX - Position.x, WorldY - Position.y) < MinimumSpacing for Position, _ in Selected):
        continue
    if any(math.hypot(WorldX - Position.x, WorldY - Position.y) < BuildingClearance for Position in BuildingAnchors):
        continue
    Hit = unreal.SystemLibrary.line_trace_single(EditorWorld, unreal.Vector(WorldX, WorldY, 15000.0),
                                                 unreal.Vector(WorldX, WorldY, -10000.0),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE)
    if not Hit:
        continue
    HitData = Hit.to_tuple()
    if not HitData[0] or not isinstance(HitData[9], unreal.LandscapeProxy):
        continue
    Selected.append((HitData[5], Transmission))
    if len(Selected) >= 14:
        break
assert len(Selected) >= 8, 'Not enough suitable bright terrain anchors: ' + str(len(Selected))

RayClass = unreal.load_class(None, '/Game/RTS_Survival/SpecialEffects/Atmosphere/BP_FakeGodRayTrains.BP_FakeGodRayTrains_C')
Sun = next(Actor for Actor in Actors if isinstance(Actor, unreal.DirectionalLight))
SunYaw = Sun.get_actor_rotation().yaw
Placements = []
for Index, (GroundPosition, Transmission) in enumerate(Selected):
    Position = unreal.Vector(GroundPosition.x, GroundPosition.y, GroundPosition.z - 100.0)
    Rotation = unreal.Rotator(pitch=23.0 + (Index % 3 - 1) * 3.0, yaw=SunYaw + 180.0 + (Index % 5 - 2) * 3.0)
    Label = 'FakeGodRay_Trains_' + str(Index + 1).zfill(2)
    Ray = ExistingRays.get(Label)
    if Ray:
        Ray.set_actor_location(Position, False, False)
        Ray.set_actor_rotation(Rotation, False)
    else:
        Ray = ActorSubsystem.spawn_actor_from_class(RayClass, Position, Rotation)
    assert Ray
    Ray.set_actor_label(Label)
    Ray.set_folder_path('Lighting/FakeGodRays_Trains')
    Settings = {
        'BeamLength': 5000.0 + (Index % 4) * 380.0,
        'BeamWidth': 1500.0 + (Index % 4) * 180.0,
        'EmissiveStrength': 4.0 + (Index % 3) * 0.35,
        'RayOpacity': 0.085,
        'RayColor': unreal.LinearColor(0.56 + (Index % 3) * 0.025, 0.38 + (Index % 4) * 0.025, 1.0, 1.0),
        'Seed': 13.7 + Index * 7.31,
    }
    for Name, Value in Settings.items():
        Ray.set_editor_property(Name, Value)
    Ray.call_method('RefreshFakeGodRays')
    Placements.append({'actor': Label, 'location': [Position.x, Position.y, Position.z],
                       'transmission_at_placement': Transmission, 'width': Settings['BeamWidth']})
PlacedLabels = {Placement['actor'] for Placement in Placements}
for Label, Ray in ExistingRays.items():
    if Label not in PlacedLabels:
        assert ActorSubsystem.destroy_actor(Ray)
unreal.RenderingLibrary.release_render_target2d(RenderTarget)
print('PLACEMENTS_JSON', json.dumps(Placements))
print('PLACED', len(Selected), 'MIN_SPACING', MinimumSpacing)
