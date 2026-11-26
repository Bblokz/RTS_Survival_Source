"""Headless: package dependencies (textures) of the pit materials."""
import unreal
registry = unreal.AssetRegistryHelpers.get_asset_registry()
options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True)
for package in ["/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/M_GerCamoConcretePalet_Atlas",
                "/Game/RTS_Survival/Meshes/Props/Ammo/Materials/M_Ger_AmmoBox",
                "/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas"]:
    for dependency in registry.get_dependencies(package, options) or []:
        unreal.log("BATTERYDEP %s -> %s" % (package.split("/")[-1], dependency))
