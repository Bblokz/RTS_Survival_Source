"""Builds PCG_Kessel_Content, the content graph of SM_Kessel_01 (run in the editor).

One Survival Site Picker per kind of content, each feeding a Spawn Actor node that spawns by the ActorClass attribute. The
pools below are the design: what can turn up on the map. GROUND_OFFSETS comes from measure_pools() (lowest visible LOD0
vertex against the pivot), so an actor stands on the ground and not on its root.
"""
import unreal

GRAPH_FOLDER = "/Game/RTS_Survival/SurvivalMode/Maps/PCG"
GRAPH_NAME = "PCG_Kessel_Content"
GRAPH_PATH = GRAPH_FOLDER + "/" + GRAPH_NAME

ENV = "/Game/RTS_Survival/Blueprints/Environment/"
RES = "/Game/RTS_Survival/Blueprints/Resources/ChildBps/"
PITS = ENV + "CaptureObjects/GerPits/BP_CaptureGerPit_"
SCAV = ENV + "ScavengeObjects/"
ANIMALS = ENV + "Animated/FarmAnimals/"
SPECIALS = "/Game/RTS_Survival/SurvivalMode/Maps/Specials/"
RADIXITE = RES + "Radixite/Resource_Radixite_Crystal"
METAL = RES + "Metal/Resource_"

CAPTURE_POOL = [(PITS + "Flak37", 1.0, 0), (PITS + "Pak40", 1.0, 0), (PITS + "TwinMG42", 1.0, 0), (PITS + "Mortar", 1.0, 0),
                (PITS + "Puppchen", 1.0, 0), (PITS + "Panzerschreck", 1.0, 0), (PITS + "Flak88", 0.45, 1),
                (PITS + "LeFH18", 0.45, 1), (PITS + "SFH18", 0.3, 1),
                (ENV + "CaptureObjects/Bunkers/BP_CaptureBunker_37MM", 0.5, 1),
                (ENV + "CaptureObjects/Bunkers/BP_CaptureBunker_Turret", 0.5, 1),
                (ENV + "CaptureObjects/Vehicles/BP_CaptureBrummbar", 0.7, 2),
                (SPECIALS + "BP_SMD_CaptureLuchs", 0.5, 1), (SPECIALS + "BP_SMD_CaptureMarderDual", 0.5, 1)]
TANK_HULKS = [(SCAV + "DestroyedTanks/German/Light/PZ38T/ScavObj_Ger_Pz38T", 1.0, 0),
              (SCAV + "DestroyedTanks/German/Medium/Stug/ScavObj_Ger_Stug", 1.0, 0),
              (SCAV + "DestroyedTanks/German/Medium/PZIV/ScavObj_Ger_PZIV", 1.0, 0),
              (SCAV + "DestroyedTanks/German/Heavy/Tiger1/ScavObj_Ger_Tiger1", 0.3, 2)]
VEHICLE_PARTS = [(SCAV + "VehicleParts/ScavObj_VehicleParts%d" % i, 1.0, 0) for i in (1, 2, 3, 4)] + \
                [(SCAV + "VehicleParts/Cars/ScavObj_PostApo_Car_%d" % i, 0.5, 0) for i in (1, 2, 3, 4, 5)]
RADIXITE_CENTRE = [(RADIXITE + "_LargeWithGrowth", 1.0, 0)]
RADIXITE_MEMBERS = [(RADIXITE, 1.0, 0), (RADIXITE + "_SmallDecal", 1.0, 0), (RADIXITE + "_NoDecal", 0.6, 0)]
RADIXITE_BASE = [(RADIXITE, 1.0, 0), (RADIXITE + "_SmallDecal", 1.0, 0)]
METAL_CENTRE = [(METAL + "MetalTower", 1.0, 0), (METAL + "Metal_Ruin1", 0.6, 0), (METAL + "Metal_BrokenWaterTank", 0.5, 0)]
METAL_MEMBERS = [(METAL + "Metal_Barrels", 1.0, 0), (METAL + "Metal_Rails", 1.0, 0)]
METAL_BASE = [(METAL + "Metal_Barrels", 1.0, 0), (METAL + "Metal_Rails", 1.0, 0)]
HERDS = [(ANIMALS + "Sheep/BP_Wanderer_Sheep", 1.2, 0), (ANIMALS + "Cow/BP_Wanderer_Cow", 1.0, 0),
         (ANIMALS + "Goat/BP_Wanderer_Goat", 0.8, 0), (ANIMALS + "Pig/BP_Wanderer_Pig", 0.6, 0),
         (ANIMALS + "Chicken/BP_Wanderer_Chicken", 0.6, 0)]

# The rail bundle is 6 m of stiff steel: on bent ground it bridges the dip.
RAILS_REACH = 310.0
# Light props that the audit caught a few centimetres clear of slopes (kessel_map_build.step_generated_audit).
GROUND_OFFSETS = {METAL + "Metal_Rails": -10.0}
GROUND_OFFSETS.update({SCAV + "VehicleParts/Cars/ScavObj_PostApo_Car_%d" % i: -7.0 for i in (1, 2, 3, 4, 5)})

# name, category, sites min, max, ring, spacing, centre pool, centre yaw, member pool, members min, max, member spacing,
# centre clearance, herd
NODES = [
    ("Capture", "CAPTURE", 8, 10, 0.0, 100000.0, 6000.0, CAPTURE_POOL, "FACE_OUTWARD", [], 0, 0, 300.0, 0.0, False),
    ("RadixiteBase", "RADIXITE", 2, 2, 0.0, 7500.0, 3000.0, RADIXITE_BASE, "RANDOM", [], 0, 0, 300.0, 0.0, False),
    ("RadixiteNear", "RADIXITE", 3, 4, 7500.0, 18500.0, 8000.0, RADIXITE_CENTRE, "RANDOM", RADIXITE_MEMBERS, 5, 8, 320.0, 650.0, False),
    ("RadixiteFar", "RADIXITE", 3, 4, 18500.0, 100000.0, 9000.0, RADIXITE_CENTRE, "RANDOM", RADIXITE_MEMBERS, 5, 8, 320.0, 650.0, False),
    ("MetalBase", "METAL", 2, 2, 0.0, 7500.0, 3000.0, METAL_BASE, "RANDOM", [], 0, 0, 300.0, 0.0, False),
    ("MetalNear", "METAL", 3, 4, 7500.0, 18500.0, 8000.0, METAL_CENTRE, "RANDOM", METAL_MEMBERS, 2, 4, 700.0, 1000.0, False),
    ("MetalFar", "METAL", 3, 4, 18500.0, 100000.0, 9000.0, METAL_CENTRE, "RANDOM", METAL_MEMBERS, 2, 4, 700.0, 1000.0, False),
    ("TankHulks", "WRECK", 12, 16, 0.0, 100000.0, 2500.0, TANK_HULKS, "RANDOM", [], 0, 0, 300.0, 0.0, False),
    ("VehicleParts", "VEHICLE_PARTS", 7, 9, 0.0, 100000.0, 3000.0, VEHICLE_PARTS, "RANDOM", [], 0, 0, 300.0, 0.0, False),
    ("Herds", "ANIMALS", 5, 7, 0.0, 100000.0, 9000.0, [], "RANDOM", HERDS, 3, 6, 420.0, 0.0, True),
]


def blueprint_class(path):
    actor_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if actor_class is None:
        raise RuntimeError("no blueprint " + path)
    return actor_class


def all_paths():
    out = []
    for node in NODES:
        out += [entry[0] for entry in node[7]] + [entry[0] for entry in node[9]]
    return sorted(set(out))


def entries(pool, offsets):
    out = []
    for path, weight, limit in pool:
        entry = unreal.SurvivalSiteActorEntry()
        entry.set_editor_property("actor_class", blueprint_class(path))
        entry.set_editor_property("weight", weight)
        entry.set_editor_property("max_per_generation", limit)
        entry.set_editor_property("ground_offset", GROUND_OFFSETS.get(path, 0.0))
        if path == METAL + "Metal_Rails":
            entry.set_editor_property("even_ground_radius", RAILS_REACH)
        entry.set_editor_property("align_to_ground", any(path == row[0] for row in CAPTURE_POOL + TANK_HULKS + VEHICLE_PARTS + METAL_MEMBERS))
        out.append(entry)
    return out


def build_graph():
    library = unreal.EditorAssetLibrary
    if not library.does_directory_exist(GRAPH_FOLDER):
        library.make_directory(GRAPH_FOLDER)
    graph = unreal.load_asset(GRAPH_PATH) if library.does_asset_exist(GRAPH_PATH) else None
    if graph is None:
        graph = unreal.AssetToolsHelpers.get_asset_tools().create_asset(GRAPH_NAME, GRAPH_FOLDER, unreal.PCGGraph,
                                                                        unreal.PCGGraphFactory())
    for node in list(graph.get_editor_property("nodes")):
        graph.remove_node(node)
    offsets = {}
    yaw_modes = {"RANDOM": unreal.SurvivalSiteYawMode.RANDOM, "FACE_OUTWARD": unreal.SurvivalSiteYawMode.FACE_OUTWARD}
    categories = {"CAPTURE": unreal.SurvivalMapSiteCategory.CAPTURE, "RADIXITE": unreal.SurvivalMapSiteCategory.RADIXITE,
                  "METAL": unreal.SurvivalMapSiteCategory.METAL, "WRECK": unreal.SurvivalMapSiteCategory.WRECK,
                  "VEHICLE_PARTS": unreal.SurvivalMapSiteCategory.VEHICLE_PARTS, "ANIMALS": unreal.SurvivalMapSiteCategory.ANIMALS}
    for row, spec in enumerate(NODES):
        name, category, site_min, site_max, ring0, ring1, spacing, centre, yaw, members, member_min, member_max, member_gap, clearance, herd = spec
        picker_node, picker = graph.add_node_of_type(unreal.PCGSurvivalSitePickerSettings)
        picker.set_editor_property("site_category", categories[category])
        picker.set_editor_property("min_sites", site_min)
        picker.set_editor_property("max_sites", site_max)
        picker.set_editor_property("min_distance_from_centre", ring0)
        picker.set_editor_property("max_distance_from_centre", ring1)
        picker.set_editor_property("min_site_spacing", spacing)
        picker.set_editor_property("centre_pool", entries(centre, offsets))
        picker.set_editor_property("centre_yaw_mode", yaw_modes[yaw])
        picker.set_editor_property("member_pool", entries(members, offsets))
        picker.set_editor_property("min_members", member_min)
        picker.set_editor_property("max_members", member_max)
        picker.set_editor_property("member_spacing", member_gap)
        picker.set_editor_property("centre_clearance", clearance)
        picker.set_editor_property("members_share_one_class", herd)
        picker_node.set_editor_property("node_title", unreal.Name("Pick " + name))
        spawn_node, spawner = graph.add_node_of_type(unreal.PCGSpawnActorSettings)
        spawner.set_editor_property("option", unreal.PCGSpawnActorOption.NO_MERGING)
        spawner.set_editor_property("spawn_by_attribute", True)
        spawner.set_editor_property("spawn_attribute", unreal.Name("ActorClass"))
        spawner.set_editor_property("attach_options", unreal.PCGAttachOptions.IN_FOLDER)
        spawner.set_editor_property("warn_on_identical_spawn", False)
        spawn_node.set_editor_property("node_title", unreal.Name("Spawn " + name))
        picker_node.set_node_position(0, row * 220) if hasattr(picker_node, "set_node_position") else None
        spawn_node.set_node_position(420, row * 220) if hasattr(spawn_node, "set_node_position") else None
        graph.add_edge(picker_node, unreal.Name("Out"), spawn_node, unreal.Name("In"))
    graph.modify() if hasattr(graph, "modify") else None
    return unreal.EditorAssetLibrary.save_loaded_asset(graph, False)


def lowest_visible_point(actor):
    """Lowest LOD0 vertex of the visible static meshes in actor space; root sprites and collision do not count."""
    lowest = None
    origin_z = actor.get_actor_location().z
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        if isinstance(comp, unreal.InstancedStaticMeshComponent) or not comp.is_visible():
            continue
        mesh = comp.get_editor_property("static_mesh")
        if mesh is None:
            continue
        transform = comp.get_world_transform()
        for section in range(mesh.get_num_sections(0)):
            data = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
            for vertex in list(data[0])[::3]:
                z = transform.transform_location(vertex).z - origin_z
                lowest = z if lowest is None or z < lowest else lowest
    for comp in actor.get_components_by_class(unreal.SkeletalMeshComponent):
        if not comp.is_visible() or comp.get_editor_property("skeletal_mesh_asset") is None:
            continue
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(comp)
        bounds_min = origin.z - extent.z - origin_z
        lowest = bounds_min if lowest is None or bounds_min < lowest else lowest
    return lowest


def measure_pools(x=4000.0, y=0.0, z=1120.0):
    """Spawns each pool actor once on the plateau; writes the offset that puts its lowest visible point on the ground."""
    subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    result = {}
    for path in all_paths():
        actor = subsystem.spawn_actor_from_class(blueprint_class(path), unreal.Vector(x, y, z))
        if actor is None:
            result[path] = None
            continue
        low = lowest_visible_point(actor)
        result[path] = low
        subsystem.destroy_actor(actor)
    return result
