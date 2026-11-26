"""Editor Python: spawn the field hospital set (E5 shell + BP_BerlinFieldHospital + Dest_RedCrossFlag) on a test map
and frame it from the RTS angle, for take_viewport_screenshot. Not for Ger_Berlin (fh_place.py places it there)."""
import math
import unreal

FH = '/Game/RTS_Survival/Blueprints/Environment/Berlin/FieldHospital/'
SHELL = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/BP_BerlinRuin_E5_Shell1St_A'
FLAG_LOCAL = unreal.Vector(700, -30, 0)      # beside the door, left of the cellar shelf
ORIGIN, YAW = unreal.Vector(0, 0, 0), 0.0
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_actor_label().startswith('FHPREVIEW_'):
        a.destroy_actor()
hit = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(0, 0, 50000), unreal.Vector(0, 0, -50000),
                                             unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
z = hit.to_tuple()[4].z if hit else 0.0
base = unreal.Vector(ORIGIN.x, ORIGIN.y, z)
rot = unreal.Rotator(0, 0, YAW)
for label, path, local in (('FHPREVIEW_Shell', SHELL, unreal.Vector(0, 0, 0)),
                           ('FHPREVIEW_Hospital', FH + 'BP_BerlinFieldHospital', unreal.Vector(0, 0, 0)),
                           ('FHPREVIEW_Flag', FH + 'Dest_RedCrossFlag', FLAG_LOCAL)):
    cls = unreal.load_asset(path).generated_class()
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, base + unreal.MathLibrary.greater_greater_vector_rotator(local, rot), rot)
    a.set_actor_label(label)
print('ground z', z)
