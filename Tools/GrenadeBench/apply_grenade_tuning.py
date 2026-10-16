"""Writes the benchmark-tuned grenade settings into the squad Blueprints, adds the missing grenade socket to the
heavily armored German skeleton and updates the grenade card texts. Run inside an editor that has the
FuseSecondsAfterLanding / bStickToVehicles build (through the UECP bridge's execute_python).
Docs: Source/Docs/Abilities/GrenadeQuality.md"""
import unreal

SQUADS = "/Game/RTS_Survival/Blueprints/Squads/"
# Squad Blueprint -> settings to write on its GrenadeComponent.
TUNING = {
    SQUADS + "Ger/Barracks_Infantry/SteelFistAssaultSquad/Squad_Ger_Faust": {
        "FuseSecondsAfterLanding": 1.0, "bStickToVehicles": True},
    SQUADS + "Ger/ArmoryUnlocked/Squad_Ger_HazmatSniperTeam": {
        "FuseSecondsAfterLanding": 1.0, "bStickToVehicles": True},
    SQUADS + "Ger/Barracks_Infantry/JagerTrupp/Squad_Ger_JagerTrupp": {
        "FuseSecondsAfterLanding": 1.5, "AoeRange": 250.0, "Damage": 90.0, "AoeDamageExponentReduction": 1.0},
    SQUADS + "Ger/EliteInfantry/LightBringers/Squad_Ger_LightBringers": {
        "FuseSecondsAfterLanding": 1.5, "AoeRange": 200.0, "Damage": 60.0, "AoeDamageExponentReduction": 1.0},
}
SOCKET_SOURCE_MESH = "/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/GermanSoldier/Meshes/Armored/SK_Ger_Armored"
SOCKET_TARGET_SOLDIER = SQUADS + "Ger/Barracks_Infantry/SteelFistAssaultSquad/Ger_Inf_Faust"
BUTTONS_TABLE = "/Game/RTS_Survival/Blueprints/GameUI/ActionUI/Data/Buttons_DataTableb"

sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary


def grenade_component(bp):
    for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj and obj.get_class().get_name() == "GrenadeComponent":
            return obj
    return None


for path, values in TUNING.items():
    bp = unreal.load_asset(path)
    comp = grenade_component(bp)
    if comp is None:
        print("NO GRENADE COMPONENT", path)
        continue
    settings = comp.get_editor_property("M_Settings")
    for key, value in values.items():
        settings.set_editor_property(key, value)
    comp.modify()
    comp.set_editor_property("M_Settings", settings)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    readback = grenade_component(bp).get_editor_property("M_Settings")
    print("tuned", path.split("/")[-1], "saved=%s" % saved,
          {key: readback.get_editor_property(key) for key in values})


def soldier_mesh(soldier_bp_path):
    name = soldier_bp_path.split("/")[-1]
    cls = unreal.load_object(None, soldier_bp_path + "." + name + "_C")
    return unreal.get_default_object(cls).get_editor_property("mesh").get_editor_property("skeletal_mesh_asset")


target_mesh = soldier_mesh(SOCKET_TARGET_SOLDIER)
source_mesh = unreal.load_asset(SOCKET_SOURCE_MESH) if unreal.EditorAssetLibrary.does_asset_exist(SOCKET_SOURCE_MESH) \
    else soldier_mesh(SQUADS + "Ger/Barracks_Infantry/JagerTrupp/Ger_Inf_Kar")
source_socket = source_mesh.find_socket("grenade")
if target_mesh.find_socket("grenade") is not None:
    print("grenade socket already on", target_mesh.get_name())
elif source_socket is None:
    print("NO SOURCE SOCKET on", source_mesh.get_name())
else:
    socket = unreal.SkeletalMeshSocket(target_mesh)
    socket.set_editor_property("socket_name", "grenade")
    for prop in ("bone_name", "relative_location", "relative_rotation", "relative_scale"):
        socket.set_editor_property(prop, source_socket.get_editor_property(prop))
    # On the skeleton, so every mesh of that skeleton gets it.
    target_mesh.add_socket(socket, True)
    skeleton = target_mesh.get_editor_property("skeleton")
    print("socket added: bone", source_socket.get_editor_property("bone_name"),
          "mesh saved", unreal.EditorAssetLibrary.save_loaded_asset(target_mesh, False),
          "skeleton saved", unreal.EditorAssetLibrary.save_loaded_asset(skeleton, False))

# Card texts quote the damage and radius; key 1 = AT, 2 = stick, 3 = small grenade.
table = unreal.load_asset(BUTTONS_TABLE)
print("buttons table loaded", table is not None)
