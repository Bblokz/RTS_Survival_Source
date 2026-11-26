"""Headless read-only dump of candidate source meshes for the airfield build site piece.
Log: Saved/WorldBuildSitePieces/airfield_dump.txt
"""
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "WorldBuildSitePieces")
os.makedirs(OUT, exist_ok=True)
LINES = []

CANDIDATES = [
    "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Meshes/SK_Stuka_VTO",
    "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Ju87G/SK_Stuka_VTO_Ju87G",
    "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/ME410/Meshes/SK_Ger_Aircraft_Me410",
    "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/FW_Recon/SK_Ger_Aircraft_FW_Recon",
    "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Wrecks/SM_PrypyatDownedJU87",
    "/Game/RTS_Survival/WorldCampaign/MapObjects/Enemy/Airfield/Mesh/SM_Global_SovietAirbase",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Airfield/Meshes/SM_SovietAirfield",
    "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/meshes/SM_AirportHangar",
    "/Game/MilitaryAirport/Meshes/Buildings/SM_Hangar2",
    "/Game/MilitaryAirport/Meshes/Buildings/SM_Watchtower",
    "/Game/MilitaryAirport/Meshes/Buildings/SM_FuelSilo_1",
    "/Game/MilitaryAirport/Meshes/Props/SM_FuelTank",
    "/Game/MilitaryAirport/Meshes/Props/SM_Barrel_1_Gasoline",
    "/Game/MilitaryAirport/Meshes/Props/SM_Carpark_Asphalt",
    "/Game/MilitaryAirport/Meshes/Props/SM_Crate_Stack_04",
    "/Game/MilitaryBaseCamp/Meshes/SM_MilitaryTent",
    "/Game/MilitaryBaseCamp/Meshes/SM_OpenTent",
    "/Game/MillitaryBase/Meshes/SM_StorageTent1",
    "/Game/MillitaryBase/Meshes/SM_Tent",
    "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Meshes/SM_Prypyat_FuelDrums",
    "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Meshes/SM_SovietAirfield_Prypyat",
    "/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Meshes/SM_Prypyat_ServiceCrates",
    "/Game/RTS_Survival/WorldCampaign/MapObjects/Missions/Story/Pripyat/SM_WorldPiece_Pripyat",
]


def log(text):
    LINES.append(str(text))


def describe_material(material):
    if material is None:
        return "None"
    text = material.get_path_name()
    if isinstance(material, unreal.MaterialInstance):
        for parameter in material.get_editor_property("texture_parameter_values"):
            tex = parameter.parameter_value
            text += "\n        tex %s = %s" % (parameter.parameter_info.name, tex.get_path_name() if tex else None)
        for parameter in material.get_editor_property("vector_parameter_values"):
            text += "\n        vec %s = %s" % (parameter.parameter_info.name, parameter.parameter_value)
    return text


for path in CANDIDATES:
    asset = unreal.load_asset(path)
    if asset is None:
        log("MISSING " + path)
        continue
    if isinstance(asset, unreal.StaticMesh):
        bounds = asset.get_bounds()
        log("SM %s tris=%d lods=%d nanite=%s origin=(%.1f %.1f %.1f) extent=(%.1f %.1f %.1f)" % (
            path, asset.get_num_triangles(0), asset.get_num_lods(), asset.get_editor_property("nanite_settings").enabled,
            bounds.origin.x, bounds.origin.y, bounds.origin.z,
            bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z))
        for index, static_material in enumerate(asset.static_materials):
            log("    slot %d %s -> %s" % (index, static_material.material_slot_name,
                                         describe_material(static_material.material_interface)))
    elif isinstance(asset, unreal.SkeletalMesh):
        bounds = asset.get_bounds()
        log("SK %s origin=(%.1f %.1f %.1f) extent=(%.1f %.1f %.1f)" % (
            path, bounds.origin.x, bounds.origin.y, bounds.origin.z,
            bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z))
        for index, skeletal_material in enumerate(asset.materials):
            log("    slot %d %s -> %s" % (index, skeletal_material.material_slot_name,
                                         describe_material(skeletal_material.material_interface)))
    else:
        log("OTHER %s %s" % (path, type(asset)))

log("--- geometry script functions")
for name in ("GeometryScript_AssetUtils", "GeometryScript_MeshSimplify", "GeometryScript_MeshTransforms",
             "GeometryScript_MeshBasicEdits", "GeometryScript_MeshNormals", "GeometryScript_Primitives"):
    library = getattr(unreal, name, None)
    if library is None:
        log("NO " + name)
        continue
    log(name + ": " + ", ".join(sorted(entry for entry in dir(library) if not entry.startswith("_")
                                        and entry not in dir(unreal.Object))))

with open(os.path.join(OUT, "airfield_dump.txt"), "w", encoding="utf-8") as handle:
    handle.write("\n".join(LINES))
