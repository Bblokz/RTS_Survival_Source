"""German BXP QA: hole debris pieces fly as Niagara mesh particles, which draw each piece's own materials. A base
material without bUsedWithNiagaraMeshParticles renders as the default material in a cooked build (the editor sets the
flag silently), so this sets it on every base material used by the BXP debris sets and saves those masters (headless).
Also gives the flak bunker's debris the bunker's own instance (its mesh slot is empty; the Blueprint overrides it)."""
import unreal

EAL = unreal.EditorAssetLibrary
REG = unreal.AssetRegistryHelpers.get_asset_registry()
FOLDERS = [
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/SharedHoles/Debris",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/MechDepot/WurfrahmenShop/Meshes/Debris",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Barracks/SniperLoft/Meshes/Debris",
    "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/Holes",
]
FLAK_BUNKER_DEBRIS = FOLDERS[0] + "/SM_Ger_FlakBunker"
FLAK_BUNKER_MI = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/FlakBunker/Mesh/MI_GerFlakBunker"


def log(msg):
    unreal.log("BXPNIAGARA " + msg)


def fix_flak_bunker_debris():
    mi = unreal.load_asset(FLAK_BUNKER_MI)
    if mi is None:
        found = [a for a in REG.get_assets_by_path("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/FlakBunker", recursive=True)
                 if str(a.asset_name) == "MI_GerFlakBunker"]
        mi = unreal.load_asset(str(found[0].package_name)) if found else None
    if mi is None:
        log("MI_GerFlakBunker not found")
        return
    for a in REG.get_assets_by_path(FLAK_BUNKER_DEBRIS, recursive=False):
        piece = unreal.load_asset(str(a.package_name))
        if isinstance(piece, unreal.StaticMesh):
            piece.set_material(0, mi)
            EAL.save_loaded_asset(piece, False)
    log("flak bunker debris -> " + mi.get_name())


def main():
    fix_flak_bunker_debris()
    bases = {}
    for folder in FOLDERS:
        for a in REG.get_assets_by_path(folder, recursive=True):
            if str(a.asset_class_path.asset_name) != "StaticMesh":
                continue
            mesh = unreal.load_asset(str(a.package_name))
            for slot in mesh.get_editor_property("static_materials"):
                material = slot.get_editor_property("material_interface")
                base = material.get_base_material() if material else None
                if base is not None:
                    bases[base.get_path_name()] = base
    changed = []
    for path, base in sorted(bases.items()):
        if base.get_path_name().startswith("/Engine/"):
            log("engine material left alone " + path)
            continue
        if base.get_editor_property("used_with_niagara_mesh_particles"):
            continue
        base.set_editor_property("used_with_niagara_mesh_particles", True)
        unreal.MaterialEditingLibrary.recompile_material(base)
        ok = EAL.save_loaded_asset(base, False)
        changed.append("%s saved=%s" % (base.get_name(), ok))
    log("bases %d changed %d: %s" % (len(bases), len(changed), changed))


main()
