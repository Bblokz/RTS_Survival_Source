"""German BXP QA: registers expansions missing from the async spawner (class + preview mesh maps). Headless.
Only adds missing keys; existing entries are left as they are."""
import unreal

COMM = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/CommunicationsCenter/BXPs/"
HQ = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/HQ/"
BUNKERS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/"
ENTRIES = [
    (unreal.BuildingExpansionType.BTX_GER_COMM_REQUISITION_OFFICE,
     COMM + "RequisitionOffice/Ger_Bxp_CommRequisitionOffice",
     COMM + "RequisitionOffice/SM_GerCommCenter_RequisitionOffice_PreviewMesh"),
    (unreal.BuildingExpansionType.BTX_GER_COMM_CIPHER_ROOM,
     COMM + "CipherRoom/Ger_Bxp_CommCipherRoom",
     COMM + "CipherRoom/SM_GerCommCenter_CipherRoom_PreviewMesh"),
    (unreal.BuildingExpansionType.BTX_GER_HQ_LASER,
     HQ + "HQLaser/Ger_Bxp_HQLaserPlatform",
     HQ + "HQLaser/Prev_Cons/SM_Ger_Bxp_HqLaser_PreviewMesh"),
    # Breakthrough concrete bunkers: field defenses previewed with their own building mesh, like the rail Pak bunker.
    (unreal.BuildingExpansionType.BTX_GER_FLAK88_BUNKER,
     BUNKERS + "Flak88Bunker/Ger_BXP_Flak88Bunker", BUNKERS + "Flak88Bunker/Mesh/SM_BF_Flak88Bunker"),
    (unreal.BuildingExpansionType.BTX_GER_FLAKVIERLING_BUNKER,
     BUNKERS + "FlakvierlingBunker/Ger_BXP_FlakvierlingBunker",
     BUNKERS + "FlakvierlingBunker/Mesh/SM_BF_FlakvierlingBunker"),
    (unreal.BuildingExpansionType.BTX_GER_PZ38T_BUNKER,
     BUNKERS + "PZ38TTurretBunker/Ger_BXP_PZ38TTurretBunker",
     BUNKERS + "PZ38TTurretBunker/Mesh/SM_BF_PZ38T_TurretBunker"),
]

spawner = unreal.load_asset("/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner")
cdo = unreal.get_default_object(spawner.generated_class())
classes = cdo.get_editor_property("BuildingExpansionMap")
previews = cdo.get_editor_property("BxpPreviewMeshMap")
before = (len(classes), len(previews))
for bxp_type, bp_path, preview_path in ENTRIES:
    name = bp_path.split("/")[-1]
    if bxp_type not in classes:
        classes[bxp_type] = unreal.load_class(None, bp_path + "." + name + "_C")
    if previews.get(bxp_type) is None:
        previews[bxp_type] = unreal.load_asset(preview_path)
spawner.modify()
cdo.set_editor_property("BuildingExpansionMap", classes)
cdo.set_editor_property("BxpPreviewMeshMap", previews)
ok = unreal.EditorLoadingAndSavingUtils.save_packages([spawner.get_outermost()], False)
cdo = unreal.get_default_object(spawner.generated_class())
after = (len(cdo.get_editor_property("BuildingExpansionMap")), len(cdo.get_editor_property("BxpPreviewMeshMap")))
unreal.log("BXPREG before %s after %s saved %s" % (before, after, ok))
for bxp_type, _, _ in ENTRIES:
    unreal.log("BXPREG %s -> %s / %s" % (bxp_type, cdo.get_editor_property("BuildingExpansionMap").get(bxp_type),
                                         cdo.get_editor_property("BxpPreviewMeshMap").get(bxp_type)))
