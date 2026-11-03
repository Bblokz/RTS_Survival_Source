"""Lets the crew soldier materials draw on instanced static meshes, which battlefield corpses are.
Without the usage flag the engine draws corpses with its default material.

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
"""
import unreal

LOOKS = [
    "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Material/M_ScavengerNoCamo",
    "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/ScavengerVariations/MI_Inf_Ger_ScavengerVar1",
    "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/ScavengerVariations/MI_Inf_Ger_ScavengerVeteran",
    "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/Atlas/M_RusSoldier",
]

masters = {}
for path in LOOKS:
    look = unreal.load_asset(path)
    material = look
    while isinstance(material, unreal.MaterialInstance):
        material = material.get_editor_property("parent")
    print("RTS_RMU look", path.split("/")[-1], "master", material.get_path_name())
    masters[material.get_path_name().split(".")[0]] = material

for path, material in masters.items():
    before = material.get_editor_property("used_with_instanced_static_meshes")
    if not before:
        material.set_editor_property("used_with_instanced_static_meshes", True)
        unreal.MaterialEditingLibrary.recompile_material(material)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    print("RTS_RMU master", path.split("/")[-1], "was", before, "now", True, "saved", saved)
