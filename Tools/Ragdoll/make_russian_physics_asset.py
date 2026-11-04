"""Gives SK_RussianSoldier its own physics asset, copied from the Scavenger's (same skeleton, bodies that fit a
geared soldier). The shared SK_InfantryMaster_PhysicsAsset is far slimmer than the Mosin soldier's mesh, so his
ragdolls sank up to 25 cm into the ground.

Run headless: UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
"""
import unreal

SOURCE = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/PostApocalyptic/Mesh/SK_PostApoSoldier_PhysicsAsset"
TARGET = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier_PhysicsAsset"
MESH = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier"

library = unreal.EditorAssetLibrary
if library.does_asset_exist(TARGET):
    physics_asset = unreal.load_asset(TARGET)
    print("RTS_RPA reuse", TARGET)
else:
    physics_asset = library.duplicate_asset(SOURCE, TARGET)
    print("RTS_RPA duplicated", SOURCE, "->", TARGET, physics_asset is not None)

mesh = unreal.load_asset(MESH)
before = mesh.get_editor_property("physics_asset")
mesh.set_editor_property("physics_asset", physics_asset)
print("RTS_RPA mesh physics asset", before.get_name() if before else None, "->", physics_asset.get_name())
print("RTS_RPA saved asset", library.save_asset(TARGET, only_if_is_dirty=False))
print("RTS_RPA saved mesh", library.save_asset(MESH, only_if_is_dirty=False))
