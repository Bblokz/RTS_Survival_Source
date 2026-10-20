"""Gives the Sd.Kfz. 231 the flare aim ability: adds a UFlareAimAbilityComponent ("FlareAbility") to
Ger_ArmoredCar_Sdkfz_231 with its settings and looks, compiles and saves the Blueprint. Idempotent.
Needs an editor built with UFlareAimAbilityComponent. Run through the UECP bridge:
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/FlareAbility/ue_wire_flare_231.py").read())
The turret mesh SK_sdkfz_231_Turret carries the FlareLaunch socket (on its own skeleton); the component attaches the
discharger rack (LauncherMesh) to it at BeginPlay, so the turret Blueprint is not touched."""
import unreal

BLUEPRINT = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz231/Ger_ArmoredCar_Sdkfz_231"
FLARE = "/Game/RTS_Survival/SpecialEffects/Flare/"
LAUNCHER = "/Game/RTS_Survival/Blueprints/Turrets/German/Light/Sdkfz_231Turret/Meshes/SM_FlareDischarger_231"
LAUNCH_SOUND = "/Game/RTS_Survival/Sounds/WeaponSounds/Artillery/Mortars/MortarGrenadeWp/Cue_MortarGrenadeWeapon_Launch"
COMPONENT_NAME = "FlareAbility"

blueprint = unreal.load_asset(BLUEPRINT)
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
library = unreal.SubobjectDataBlueprintFunctionLibrary
handles = subsystem.k2_gather_subobject_data_for_blueprint(blueprint)


def template_of(handle):
    return library.get_object(library.get_data(handle))


template = None
for handle in handles:
    obj = template_of(handle)
    if obj is not None and isinstance(obj, unreal.FlareAimAbilityComponent):
        template = obj
        print("existing component", obj.get_name())
        break
if template is None:
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.FlareAimAbilityComponent,
                                          blueprint_context=blueprint)
    new_handle, fail_reason = subsystem.add_new_subobject(params)
    if not str(fail_reason):
        subsystem.rename_subobject(new_handle, unreal.Text(COMPONENT_NAME))
    template = template_of(new_handle)
    print("added component", template.get_name() if template else None, "fail:", fail_reason)

aim = template.get_editor_property("m_aim_ability_settings")
aim.set_editor_property("aim_ability_type", unreal.AimAbilityType.FLARE_LAUNCH)
aim.set_editor_property("preferred_ability_index", 11)
aim.set_editor_property("cooldown", 60)
aim.set_editor_property("aim_assist_type", unreal.PlayerAimAbilityTypes.LARGE_CROSSHAIR)
aim.set_editor_property("range_percentage", 10)
template.set_editor_property("m_aim_ability_settings", aim)

flare = template.get_editor_property("m_flare_settings")
flare.set_editor_property("fire_range", 7000.0)
flare.set_editor_property("apex_height", 1200.0)
flare.set_editor_property("hang_time_seconds", 20.0)
flare.set_editor_property("vision_radius", 3000.0)
flare.set_editor_property("ascent_speed", 3000.0)
flare.set_editor_property("launch_socket_name", "FlareLaunch")
visuals = flare.get_editor_property("visuals")
visuals.set_editor_property("launcher_mesh", unreal.load_asset(LAUNCHER))
visuals.set_editor_property("flare_mesh", unreal.load_asset(FLARE + "SM_Flare_Cartridge"))
visuals.set_editor_property("parachute_mesh", unreal.load_asset(FLARE + "SM_Flare_Parachute"))
visuals.set_editor_property("launch_effect", unreal.load_asset(FLARE + "NS_Flare_Launch"))
visuals.set_editor_property("trail_effect", unreal.load_asset(FLARE + "NS_Flare_Trail"))
visuals.set_editor_property("burn_effect", unreal.load_asset(FLARE + "NS_Flare_Burn"))
visuals.set_editor_property("launch_sound", unreal.load_asset(LAUNCH_SOUND))
# the real cartridge and canopy are too small to read from the RTS camera
visuals.set_editor_property("mesh_scale", 1.5)
flare.set_editor_property("visuals", visuals)
template.set_editor_property("m_flare_settings", flare)

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
print("SAVED", unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False))
check = template.get_editor_property("m_flare_settings")
print("CHECK type", template.get_editor_property("m_aim_ability_settings").get_editor_property("aim_ability_type"),
      "range", check.get_editor_property("fire_range"), "apex", check.get_editor_property("apex_height"),
      "hang", check.get_editor_property("hang_time_seconds"), "vision", check.get_editor_property("vision_radius"),
      "burn", check.get_editor_property("visuals").get_editor_property("burn_effect"))
