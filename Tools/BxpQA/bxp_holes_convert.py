"""German BXP QA: put procedural wall holes (UWallHoleDamageComponent) on BXP Blueprints (headless).

Run: UnrealEditor-Cmd <uproject> -run=pythonscript -script="<this> --jobs Kaskade,HQRadar" (or --jobs all)
Per job: shared NoGrid two-sided hole instances per holeable slot material (GerBxps/SharedHoles; the pit set for
sandbag / camo concrete / ammo boxes), a WallHoleDamage component on BuildingMeshComponent with the German-pit tuning,
debris (the job's own cut set when imported, else the pit sandbag set) and aim points on the mesh's own bounds.
Health notify levels are kept: ABuildingExpansion::OnHealthChanged puts the building mesh back after the legacy
_At50/_At33 swaps, so their explosions still play. Saves only the Blueprint and the hole instances it creates.
"""
import json
import sys
import unreal

exec(open("D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_holes_material.py").read())

EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SLIB = unreal.SubobjectDataBlueprintFunctionLibrary
ROOT = "D:/UE5Projects/RTS_Survival/Source/Tools/BxpQA/"
GERBXPS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps"
SHARED_HOLES = GERBXPS + "/SharedHoles"
PIT_HOLES = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/Holes/"
PIT_DEBRIS = [PIT_HOLES + "SM_GerPit_Debris_%d" % i for i in range(8)]
DEBRIS_HOME = GERBXPS + "/SharedHoles/Debris/"

# Slot material name -> existing hole instance. Anything in HOLEABLE gets a shared instance made on first use.
EXISTING = {
    "M_GerSandBagScaffold_Atlas": PIT_HOLES + "MI_GerPit_GerSandBag_WallHoles",
    "M_GerCamoConcretePalet_Atlas": PIT_HOLES + "MI_GerPit_GerCamoConcrete_WallHoles",
    "M_Ger_AmmoBox": PIT_HOLES + "MI_GerPit_GerAmmoBox_WallHoles",
    "M_Flak88_Ammo": PIT_HOLES + "MI_GerPit_Flak88Ammo_WallHoles",
    "M_MB_ModuleAtlas": SHARED_HOLES + "/MI_M_MB_ModuleAtlas_WallHoles",
    "M_WeaponRack": SHARED_HOLES + "/MI_M_WeaponRack_WallHoles",
}
HOLEABLE = {"M_PantherII_Sheets", "M_GerRadar", "M_rusty_pipe_Radixite", "M_MB_PropsAtlas", "M_Old_Concrete_2",
            "M_TowerFrame_MetalVault", "M_SD_Firebrick", "M_Wood_Pine", "M_PakBunker", "MM_Ger_PropsAtlas",
            "MI_Rusty_Painted_Metal_Sheet_xhtnbhlg_2K", "M_Radixite_Solar_Pannel", "M_OpelBlitz",
            "M_RustedRefinery_Atlas", "T_ATent_D_Mat", "M_Switchboard",
            # Modelled but not offered yet (Breakthrough bunkers, capture bunkers, flak bunker, HQ laser).
            "MI_BF_Flak88Bunker_Concrete", "MI_BF_FlakvierlingBunker_Concrete", "M_BF_Trim",
            "M_BF_PZ38T_Bunker_Concrete", "M_BF_PZ38T_Bunker_WornSteel", "M_BF_PZ38T_Bunker_RustDark",
            "MI_GerFlakBunker", "M_PZII_NoAtlas"}

BXP = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
COMM = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/CommunicationsCenter/BXPs/"
JOBS = {
    "HQRadar": GERBXPS + "/HQ/HQRadar/Ger_Bxp_HQRadar",
    "HQPlatform": GERBXPS + "/HQ/HQFlakDefense/Ger_Bxp_HQPlatform",
    "HQHarvester": GERBXPS + "/HQ/HarvesterHub/Ger_Bxp_HQHarvesterHub",
    "HQRepairBay": GERBXPS + "/HQ/RepairHub/Ger_Bxp_HQRepairBay",
    "HQTower": GERBXPS + "/HQ/HQTower/Ger_Bxp_HQTower",
    "RefConverter": GERBXPS + "/Refinery/RadixConvert/Ger_Ref_Converter",
    "Kaskade": GERBXPS + "/Refinery/Kaskade/Ger_Bxp_RefineryKaskade",
    "FlakStand": GERBXPS + "/Refinery/FlakStand/Ger_Bxp_RefineryFlakStand",
    "BarracksReactor": GERBXPS + "/Barracks/RadixReactor/Ger_Bxp_BarracksReactorExpansion",
    "BarracksFuelCell": GERBXPS + "/Barracks/FuelCell/Ger_Bxp_BarracksFuelExpansion",
    "ForgeReactor": GERBXPS + "/Forge/RadixReactor/Ger_Bxp_LFactoryRadixiteReactor",
    "ForgeFuel": GERBXPS + "/Forge/RadixFuel/Ger_Bxp_LFactoryFuelExpansion",
    "SchreckFrame": GERBXPS + "/Forge/SchreckFrame/Ger_Bxp_LFactorySchreckFrame",
    "Smelter": GERBXPS + "/MetalVault/Smelter/Ger_Bxp_MetalVaultSmelter",
    "CommObservationPost": COMM + "ObservationPost/Ger_Bxp_CommObservationPost",
    "CommMG42Nests": COMM + "MG42Nests/Ger_Bxp_CommMG42Nests",
    "CommRequisitionOffice": COMM + "RequisitionOffice/Ger_Bxp_CommRequisitionOffice",
    "CommCipherRoom": COMM + "CipherRoom/Ger_Bxp_CommCipherRoom",
    "Flak37": BXP + "Childs/37mmFlak/RTS_BP_BXP_37mmFlak",
    "Flak88": BXP + "Childs/88mmFlak/RTS_BP_BXP_Flak88",
    "LeFH150": BXP + "Childs/LeFH105/RTS_BP_BXP_LeFH105mm",
    "SolarLarge": BXP + "Childs/Economic/BigSolarEXP/BXP_Solar_Big",
    "RailPakBunker": BXP + "GerPits/RailPakBunker/Ger_BXP_RailPakBunker",
    # Not offered yet; holes so the bunker is ready when it is (user ask 2026-11-20).
    "Pak38": BXP + "Childs/Pak38/RTS_BP_BXP_Pak38",
    # Second priority: in Unreal but not offered to the player yet.
    "HQLaser": GERBXPS + "/HQ/HQLaser/Ger_Bxp_HQLaserPlatform",
    "FlakBunker": GERBXPS + "/Bunkers/FlakBunker/Ger_BXP_FlakBunker",
    "Flak88Bunker": GERBXPS + "/Bunkers/Flak88Bunker/Ger_BXP_Flak88Bunker",
    "FlakvierlingBunker": GERBXPS + "/Bunkers/FlakvierlingBunker/Ger_BXP_FlakvierlingBunker",
    "PZ38TBunker": GERBXPS + "/Bunkers/PZ38TTurretBunker/Ger_BXP_PZ38TTurretBunker",
    "CaptureBunker37": GERBXPS + "/CaptureBunkers/Ger_Bxp_CaptureBunker37mm",
    "CaptureBunkerTurret": GERBXPS + "/CaptureBunkers/Ger_Bxp_CaptureBunkerTurret",
}


def log(msg):
    unreal.log("BXPHOLES " + msg)


def component(bp, name=None, cls=None):
    for handle in SDS.k2_gather_subobject_data_for_blueprint(bp):
        obj = SLIB.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)
        if obj is None:
            continue
        if name and obj.get_name().split("_GEN_VARIABLE")[0] == name:
            return obj
        if cls and isinstance(obj, cls):
            return obj
    return None


def ensure_holes_component(bp):
    existing = component(bp, cls=unreal.WallHoleDamageComponent)
    if existing is not None:
        return existing
    root = SDS.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.WallHoleDamageComponent,
                                          blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError("add WallHoleDamage failed: %s" % fail)
    SDS.rename_subobject(handle, unreal.Text("WallHoleDamage"))
    return SLIB.get_object_for_blueprint(SDS.k2_find_subobject_data_from_handle(handle), bp)


def hole_instance(slot_material, mesh):
    if slot_material is None:
        return None
    name = slot_material.get_name()
    if name in EXISTING:
        return unreal.load_asset(EXISTING[name])
    if name not in HOLEABLE:
        return None
    try:
        return make_holes_instance(slot_material.get_path_name().split(".")[0], SHARED_HOLES, mesh, grid_axis=None,
                                   two_sided=True)
    except Exception as error:
        log("hole instance skipped %s: %s" % (name, error))
        return None


def configure(component_obj, materials, debris):
    component_obj.set_editor_property("WallMeshComponentName", "BuildingMeshComponent")
    component_obj.set_editor_property("DamagedMaterials", materials)
    component_obj.set_editor_property("DebrisMeshes", debris)
    hole = unreal.WallChunkHoleSettings()
    hole.import_text("(MinCalibreMm=20,HoleRadiusPerCalibreMm=0.850000,HoleRadiusPerSqrtDamage=5.000000,"
                     "GuaranteedChunkRadius=45.000000,MaxHoleRadius=220.000000,MaxHiddenShareWhileAlive=0.650000)")
    component_obj.set_editor_property("HoleSettings", hole)
    grid = unreal.WallHoleGridSettings()
    grid.import_text("(GridAxis=NoGrid,CellSize=150.000000,CellBreakRadiusShare=0.750000,BakeReachShare=0.350000,"
                     "GrowRadiusShare=0.700000,MinHoleRadius=12.000000,HoleCentreOutsetShare=0.000000,"
                     "AreaPerDebrisPiece=3600.000000,MaxPiecesPerHit=5,CollapsePieceSpacing=110.000000,"
                     "MaxCollapsePieces=120)")
    component_obj.set_editor_property("GridSettings", grid)
    collapse = unreal.WallChunkCollapseSettings()
    collapse.import_text("(bCollapseIntoChunksOnDeath=False,DestroyOwnerDelaySeconds=4.500000,DeathSpeedScale=1.300000,"
                         'CollapseSound="/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue.Collapse_Cue",'
                         'CollapseAttenuation="/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse.Att_Collapse",'
                         'CollapseConcurrency="/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse.SC_Collapse")')
    component_obj.set_editor_property("CollapseSettings", collapse)


def holeable_vertices(mesh, materials):
    """Vertices of the sections whose slot gets a hole instance (all sections when none qualifies)."""
    sections = mesh.get_num_sections(0)
    chosen, every = [], []
    for section in range(sections):
        data = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
        vertices = list(data[0])
        every.extend(vertices)
        if section < len(materials) and materials[section] is not None:
            chosen.extend(vertices)
    return chosen or every


def aim_points(mesh_component, mesh, materials=()):
    """Five points on the expansion's own surfaces in actor space: targets at the centre and the four sides (60 % of
    the half extent) at 55 % height, each snapped to the nearest vertex of a holeable section, so shots aim at real
    geometry of the expansion (open frames and split nests have nothing at their bounds centre)."""
    box = mesh.get_bounding_box()
    centre = (box.min + box.max) * 0.5
    half = (box.max - box.min) * 0.5
    z = box.min.z + (box.max.z - box.min.z) * 0.55
    targets = [unreal.Vector(centre.x, centre.y, z)]
    for dx, dy in ((0.6, 0), (-0.6, 0), (0, 0.6), (0, -0.6)):
        targets.append(unreal.Vector(centre.x + half.x * dx, centre.y + half.y * dy, z))
    try:
        vertices = holeable_vertices(mesh, materials)
    except Exception as error:
        log("vertex snap skipped %s: %s" % (mesh.get_name(), error))
        vertices = []
    local = []
    for target in targets:
        if not vertices:
            local.append(target)
            continue
        best = min(vertices, key=lambda v: (v.x - target.x) ** 2 + (v.y - target.y) ** 2 + (v.z - target.z) ** 2)
        local.append(unreal.Vector(best.x, best.y, best.z))
    xf = unreal.Transform(mesh_component.get_editor_property("relative_location"),
                          mesh_component.get_editor_property("relative_rotation"),
                          mesh_component.get_editor_property("relative_scale3d"))
    return [unreal.MathLibrary.transform_location(xf, p) for p in local]


def debris_for(mesh):
    own = [DEBRIS_HOME + "%s/%s_Debris_%d" % (mesh.get_name(), mesh.get_name(), i) for i in range(6)]
    if all(EAL.does_asset_exist(p) for p in own):
        return [unreal.load_asset(p) for p in own], "own"
    return [unreal.load_asset(p) for p in PIT_DEBRIS], "pit-sandbag"


def convert(key):
    path = JOBS[key]
    bp = unreal.load_asset(path)
    if bp is None:
        log("%s MISSING %s" % (key, path))
        return None
    building = component(bp, name="BuildingMeshComponent")
    mesh = building.get_editor_property("static_mesh")
    slots = [s.get_editor_property("material_interface") for s in mesh.get_editor_property("static_materials")]
    overrides = list(building.get_editor_property("override_materials"))
    for i, o in enumerate(overrides):
        if o is not None and i < len(slots):
            slots[i] = o
    materials = [hole_instance(m, mesh) for m in slots]
    debris, debris_kind = debris_for(mesh)
    holes = ensure_holes_component(bp)
    configure(holes, materials, debris)
    cdo = unreal.get_default_object(bp.generated_class())
    bp.modify()
    points = aim_points(building, mesh, materials)
    cdo.set_editor_property("AimOffsetPoints", points)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = unreal.EditorLoadingAndSavingUtils.save_packages([bp.get_outermost()], False)
    result = {"bp": path, "mesh": mesh.get_name(), "saved": bool(saved), "debris": debris_kind,
              "slots": [[s.get_name() if s else None, m.get_name() if m else None] for s, m in zip(slots, materials)],
              "aim": [[round(p.x), round(p.y), round(p.z)] for p in points]}
    log("%s %s" % (key, json.dumps(result)))
    return result


def main():
    args = sys.argv[1:]
    jobs = list(JOBS) if "--jobs" not in args or args[args.index("--jobs") + 1] == "all" \
        else args[args.index("--jobs") + 1].split(",")
    results = {}
    for key in jobs:
        try:
            results[key] = convert(key)
        except Exception as error:
            log("%s FAILED %s" % (key, error))
            results[key] = {"error": str(error)}
    with open(ROOT + "bxp_holes_result.json", "w", encoding="utf-8") as f:
        json.dump(results, f, indent=1)
    log("done %d" % len(results))


main()
