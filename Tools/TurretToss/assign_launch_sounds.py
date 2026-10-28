"""Assign the turret-launch Sound Cues to the cook-off behaviour Blueprints, one per weight class.

Run inside an editor that has the UAmmoCookOffBehaviour::M_TurretLaunchSound property (UECP execute_python or the
editor's Python console). Saves only the four behaviour Blueprints.
Super-heavy tanks use the heavy cook-off behaviour (UVehicleModuleSubsystem::GetAmmoCookOffBehaviourClass).
"""
import unreal

BEHAVIOURS = "/Game/RTS_Survival/Blueprints/GroundVehicles/ModuleSystem/AmmoDet/"
CUES = "/Game/RTS_Survival/Sounds/Explosions/TurretLaunch/"
ASSIGNMENT = {
    "Beh_AmmoCookArm": "Cue_TurretLaunch_Light",
    "Beh_AmmoCookLight": "Cue_TurretLaunch_Light",
    "Beh_AmmoCookMed": "Cue_TurretLaunch_Medium",
    "Beh_AmmoCookHeavy": "Cue_TurretLaunch_Heavy",
}

for behaviour_name, cue_name in ASSIGNMENT.items():
    blueprint = unreal.load_asset(BEHAVIOURS + behaviour_name)
    cue = unreal.load_asset(CUES + cue_name)
    defaults = unreal.get_default_object(blueprint.generated_class())
    defaults.set_editor_property("m_turret_launch_sound", cue)
    blueprint.modify()
    saved = unreal.EditorAssetLibrary.save_asset(BEHAVIOURS + behaviour_name, only_if_is_dirty=False)
    print(behaviour_name, "->", defaults.get_editor_property("m_turret_launch_sound").get_name(), "saved", saved)
