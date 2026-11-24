"""Act 1 clue spots on Ger_2ndOreMountains (mission doc: "Act 1 clues"). Run inside the throwaway editor (ore_cmd.py).

    import ore_clues; ore_clues.build_all()        # decal assets + both spots + triggers (re-runnable)

A  Fossil roots (5 gully spots): fuller blackened root masses, a shattered stump on each root plate and a few standing
   dead trunks right at the roots, so "the forest died standing, even the roots turned to stone" reads from the RTS
   camera. One trigger actor covers all five gullies; the Scavenger line plays once, at the first gully reached.
B  Miners' shelter at adit 3 (east shaft): a timbered portal (Tuerstock) with a plank bulkhead and a door left ajar,
   painted GLUECK AUF / SCHUTZRAUM 1946, the miners' tally marks on the rock, a dim warm light in the doorway, stores,
   a tipped mine cart and a row of graves on the apron for those who went up. The announcer line plays once when a
   player unit reaches the apron.
No purple anywhere, no crystals near either spot (lore rules in OreMountainsMission.md). Labels ORE_ClueA_* / ORE_ClueB_*.
"""
import math
import os
import random

import unreal

import ore_map_build as omb

HOME = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/ClueSpots"
DECAL_SRC = "D:/UE5Projects/RTS_Survival/Saved/OreMountains/clues/decals"
CLIFF_DECALS = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/CliffKit/Decals/"
TRIGGER_BP = "/Game/RTS_Survival/Blueprints/Missions/TriggerAreas/BP_ScriptTrigger_PortraitLine"
KIT = "/Game/RTS_Survival/Blueprints/Environment/OreMountains/DeadForest/"
WW1 = "/Game/RBP_WW1_Trench/Meshes/"
STONES = "/Game/RTS_Survival/Blueprints/Environment/GroundStones/Meshes/"
FOLDER_A = omb.FOLDER + "Clues/A_FossilRoots"
FOLDER_B = omb.FOLDER + "Clues/B_MinersShelter"

ROOT_SPOTS = [(-7102.7, 3953.0), (-7826.9, -6666.5), (5513.4, 10519.0), (5794.2, -9916.0), (-7035.0, -24790.0)]
ADIT = {"x": -23328.6, "y": 12700.0, "z": 1815.0, "yaw": -8.13}
TRIGGER_RADIUS_A = 1500.0
TRIGGER_RADIUS_B = 1300.0


# ---- helpers ------------------------------------------------------------------------------------------------------
def _mesh(path, label, loc, rot, scale=(1.0, 1.0, 1.0), folder=FOLDER_B):
    actor = omb.actors_sub().spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(*rot))
    comp = actor.static_mesh_component
    comp.set_static_mesh(unreal.load_asset(path))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    comp.set_editor_property("can_ever_affect_navigation", False)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def _decal(mi_path, label, loc, rot, size, sort=0, folder=FOLDER_B):
    actor = omb.actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(*loc), unreal.Rotator(*rot))
    comp = actor.decal
    comp.set_decal_material(unreal.load_asset(mi_path))
    comp.set_editor_property("decal_size", unreal.Vector(*size))
    comp.set_editor_property("sort_order", sort)
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def _ground_hit(x, y):
    """Top surface under (x, y) including meshes (rock pieces, kit roots); falls back to the landscape height."""
    world = omb.world()
    start, end = unreal.Vector(x, y, 20000.0), unreal.Vector(x, y, -5000.0)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is not None:
        loc = hit.to_tuple()[4]
        if loc.z > -4000.0:
            return loc.z
    return omb.ground_z(x, y)


def _face_distance(x, y, z, dx, dy, reach=3000.0):
    """Distance from (x, y, z) to the first blocking surface along (dx, dy); None when nothing is hit."""
    world = omb.world()
    start = unreal.Vector(x, y, z)
    end = unreal.Vector(x + dx * reach, y + dy * reach, z)
    hit = unreal.SystemLibrary.line_trace_single(world, start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    loc = hit.to_tuple()[4]
    return math.hypot(loc.x - x, loc.y - y)


# ---- decal assets (painted words, tally) ------------------------------------------------------------------------
def build_decal_assets():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    parent = unreal.load_asset(CLIFF_DECALS + "Materials/M_OreCliffDecal")
    flat_n = unreal.load_asset(CLIFF_DECALS + "Textures/T_OreCliffDecalV2_Flat_N")
    made = []
    for name in ("Words", "Tally"):
        textures = {}
        for kind in ("BCA", "RMAO"):
            tex_name = "T_OreClue_%s_%s" % (name, kind)
            task = unreal.AssetImportTask()
            task.filename = os.path.join(DECAL_SRC, tex_name + ".png")
            task.destination_path = HOME + "/Textures"
            task.destination_name = tex_name
            task.automated = True
            task.replace_existing = True
            task.save = False
            tools.import_asset_tasks([task])
            tex = unreal.load_asset("%s/Textures/%s" % (HOME, tex_name))
            if kind == "BCA":
                tex.set_editor_property("srgb", True)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_BC7)
            else:
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
            unreal.EditorAssetLibrary.save_asset(tex.get_path_name(), only_if_is_dirty=False)
            textures[kind] = tex
        mi_name = "MI_OreClueDecal_" + name
        mi_path = "%s/%s" % (HOME, mi_name)
        mi = unreal.load_asset(mi_path)
        if mi is None:
            mi = tools.create_asset(mi_name, HOME, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel = unreal.MaterialEditingLibrary
        mel.set_material_instance_parent(mi, parent)
        mel.set_material_instance_texture_parameter_value(mi, "BaseColour_Opacity", textures["BCA"])
        mel.set_material_instance_texture_parameter_value(mi, "Normal", flat_n)
        mel.set_material_instance_texture_parameter_value(mi, "RMAO", textures["RMAO"])
        mel.set_material_instance_scalar_parameter_value(mi, "OpacityLerpB", 1.0)
        mel.set_material_instance_scalar_parameter_value(mi, "BaseColour_OpacityScale", 1.45)
        mel.set_material_instance_scalar_parameter_value(mi, "BaseColour_Brightness", 0.9)
        mel.set_material_instance_scalar_parameter_value(mi, "AngleFadeAmount", 0.0)
        mel.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_asset(mi_path, only_if_is_dirty=False)
        made.append(mi_path)
    made.append(build_fossil_material())
    omb.log("clue decal assets %s" % made)
    return made


FOSSIL_MI = HOME + "/MI_OreClue_FossilRoots"


def build_fossil_material():
    """Charcoal-dark copy of the dead-forest wood (same atlas, albedo darkened and desaturated in gen step): the clue
    roots read as blackened and stone-hard next to the grey standing trunks."""
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    task = unreal.AssetImportTask()
    task.filename = os.path.join(DECAL_SRC, "T_OreClue_FossilRoots_BC.png")
    task.destination_path = HOME + "/Textures"
    task.destination_name = "T_OreClue_FossilRoots_BC"
    task.automated = True
    task.replace_existing = True
    task.save = False
    tools.import_asset_tasks([task])
    tex = unreal.load_asset(HOME + "/Textures/T_OreClue_FossilRoots_BC")
    unreal.EditorAssetLibrary.save_asset(tex.get_path_name(), only_if_is_dirty=False)
    mi = unreal.load_asset(FOSSIL_MI)
    if mi is None:
        mi = tools.create_asset("MI_OreClue_FossilRoots", HOME, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_parent(mi, unreal.load_asset(KIT + "Materials/MI_OreDeadForest_Wood_Still"))
    mel.set_material_instance_texture_parameter_value(mi, "Albedo", tex)
    mel.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_asset(FOSSIL_MI, only_if_is_dirty=False)
    return FOSSIL_MI


def _fossilise(actor):
    actor.static_mesh_component.set_material(0, unreal.load_asset(FOSSIL_MI))


# ---- A: fossil roots ----------------------------------------------------------------------------------------------
def build_roots():
    labels = omb.by_label()
    rng = random.Random(1976)
    count = 0
    for n, (sx, sy) in enumerate(ROOT_SPOTS, start=1):
        plate = labels.get("ORE_Roots_Plate_%d" % n)
        ribs = labels.get("ORE_Roots_Ribs_%d" % n)
        if plate is None or ribs is None:
            omb.log("roots %d: kit pieces missing, skipped" % n)
            continue
        p, r = plate.get_actor_location(), ribs.get_actor_location()
        ribs_rot = ribs.get_actor_rotation()
        tag = "ORE_ClueA_%d_" % n
        # 0. the kit's own roots at this spot turn charcoal-black too
        _fossilise(ribs)
        _fossilise(plate)
        # 1. the root mass reads as a mass: a second, larger rib set crossing the first, and one half-buried
        _fossilise(_mesh(KIT + "Meshes/SM_OreDead_Roots_R2_Ribs", tag + "RibsCross", (r.x + 60.0, r.y - 40.0, r.z - 25.0),
                         (ribs_rot.roll, ribs_rot.pitch, ribs_rot.yaw + 38.0), (1.25, 1.15, 1.05), FOLDER_A))
        mid = ((p.x + r.x) * 0.5, (p.y + r.y) * 0.5)
        yaw_to_plate = math.degrees(math.atan2(p.y - r.y, p.x - r.x))
        _fossilise(_mesh(KIT + "Meshes/SM_OreDead_Roots_R2_Ribs", tag + "RibsRun", (mid[0], mid[1], _ground_hit(*mid) - 45.0),
                         (0.0, 0.0, yaw_to_plate + 90.0), (0.7, 1.35, 0.8), FOLDER_A))
        # a sooty black stain under the whole mass, so it reads from the RTS camera
        _decal("/Game/Crater_Decals/Decals/Scorched/MI_Decal_Crater_Scorched_Scatter_01", tag + "Stain", (r.x, r.y, r.z),
               (0.0, -90.0, yaw_to_plate), (300.0, 520.0, 380.0), 0, FOLDER_A)
        # 2. the tree the roots belonged to: a shattered stump standing on the root plate
        _mesh(KIT + "Meshes/SM_OreDead_Stump_Shattered", tag + "Stump", (p.x, p.y, p.z - 15.0),
              (0.0, 0.0, rng.uniform(0.0, 360.0)), (1.15, 1.15, 1.25), FOLDER_A)
        # 3. "died standing": three blackened trunks right at the gully, roots running down into the cut
        for k, (bp, dist, ang) in enumerate((("BP_OreDeadSpruce_Skeleton_11m", 480.0, 70.0),
                                              ("BP_OreDeadSpruce_Black_8m", 430.0, 200.0),
                                              ("BP_OreDeadSpruce_Skeleton_9m", 520.0, 300.0))):
            a = math.radians(yaw_to_plate + ang)
            tx, ty = p.x + math.cos(a) * dist, p.y + math.sin(a) * dist
            tree = omb._spawn_bp(KIT + bp, tag + "Trunk_%d" % k, tx, ty, rng.uniform(0.0, 360.0), FOLDER_A)
            tree.set_actor_location(unreal.Vector(tx, ty, _ground_hit(tx, ty) - 20.0), False, False)
            _mesh(KIT + "Meshes/SM_OreDead_Roots_R1_Plate", tag + "TrunkRoots_%d" % k, (tx, ty, _ground_hit(tx, ty) - 30.0),
                  (0.0, 0.0, math.degrees(math.atan2(r.y - ty, r.x - tx))), (0.75, 0.75, 0.9), FOLDER_A)
        count += 1
    omb.log("clue A: %d root spots dressed" % count)
    return count


# ---- B: miners' shelter -------------------------------------------------------------------------------------------
# Where the rock face of adit 3 stands along the outward normal, relative to the adit point (traced once on the clean
# map, 2026-11-16: 1563 cm from a point 1500 cm out). Later traces drift once props stand on the apron.
FACE_BACK_CM = -63.0


def adit_frame(retrace=False):
    """Portal origin on the rock face of adit 3 and the outward / along-face unit vectors."""
    yaw = math.radians(ADIT["yaw"])
    out = (math.cos(yaw), math.sin(yaw))
    side = (-math.sin(yaw), math.cos(yaw))
    apron_x, apron_y = ADIT["x"] + out[0] * 1500.0, ADIT["y"] + out[1] * 1500.0
    dist = 1500.0 - FACE_BACK_CM
    if retrace:
        floor = omb.ground_z(apron_x, apron_y)
        dist = _face_distance(apron_x, apron_y, floor + 150.0, -out[0], -out[1]) or dist
    fx, fy = apron_x - out[0] * dist, apron_y - out[1] * dist
    return {"x": fx, "y": fy, "z": omb.ground_z(fx + out[0] * 200.0, fy + out[1] * 200.0), "out": out, "side": side,
            "yaw": ADIT["yaw"], "traced": dist}


def build_shelter():
    f = adit_frame()
    out, side, yaw = f["out"], f["side"], f["yaw"]

    def at(u, s, dz=0.0, ground=True):
        x = f["x"] + out[0] * u + side[0] * s
        y = f["y"] + out[1] * u + side[1] * s
        z = (omb.ground_z(x, y) if ground else f["z"]) + dz
        return (x, y, z)

    T = "ORE_ClueB_"
    # adit 3 gets its portal from this spot: the grey placeholder box and its note go
    for label in ("ORE_PH_adit_3", "ORE_Note_PH_adit_3"):
        old = omb.by_label().get(label)
        if old is not None:
            omb.actors_sub().destroy_actor(old)
    trunk = KIT + "Meshes/SM_OreDead_FallenTrunk_9m"       # 9 m along X, ~0.9 m thick: old spruce timber
    cabin = "/Game/LowPolyForest/Models/Cabin/Walls/SM_Cabin_Wall_8th_"
    # bulkhead of planks (three 2 m panels, the middle one with a doorway), 3 m high, 1 m in front of the rock
    for k, (s, piece) in enumerate(((-200.0, "01"), (0.0, "Door_01"), (200.0, "01"))):
        _mesh(cabin + piece, T + "Bulkhead_%d" % k, at(100.0, s, -5.0), (0.0, 0.0, yaw + 90.0))
    # portal frame (Tuerstock) round it: two spruce posts and a cap
    for k, s in enumerate((-325.0, 325.0)):
        _mesh(trunk, T + "Post_%d" % k, at(115.0, s, -10.0), (0.0, 90.0, yaw), (0.37, 0.42, 0.42))
    cx, cy, _ = at(115.0, 0.0)
    _mesh(trunk, T + "Cap", (cx, cy, at(115.0, 0.0)[2] + 330.0), (0.0, 0.0, yaw + 90.0 + 180.0), (0.8, 0.45, 0.45))
    cap = omb.by_label()[T + "Cap"]
    b = cap.get_actor_bounds(False)
    loc = cap.get_actor_location()
    cap.set_actor_location(unreal.Vector(loc.x + (cx - b[0].x), loc.y + (cy - b[0].y), loc.z), False, False)
    # the steel air door of the shelter stands open; darkness behind the doorway
    _mesh(WW1 + "SM_Bunker_Door", T + "Door", at(118.0, -55.0), (0.0, 0.0, yaw + 60.0), (1.2, 1.2, 1.25))
    _decal(CLIFF_DECALS + "MI_OreCliffDecal_SootCurtain", T + "DoorDark", at(40.0, 0.0, 120.0),
           (90.0, 0.0, yaw + 180.0), (200.0, 140.0, 70.0), 1)
    # GLUECK AUF / SCHUTZRAUM 1946 on the right panel, the miners' tally on the left one (size: depth, h/2, w/2)
    _decal(HOME + "/MI_OreClueDecal_Words", T + "Words", at(100.0, 200.0, 190.0), (90.0, 0.0, yaw + 180.0),
           (60.0, 48.0, 96.0), 3)
    _decal(HOME + "/MI_OreClueDecal_Tally", T + "Tally", at(100.0, -200.0, 165.0), (90.0, 0.0, yaw + 180.0),
           (60.0, 72.0, 72.0), 3)
    # a dim warm lamp just inside the doorway: somebody still keeps a light down there
    light = omb.actors_sub().spawn_actor_from_class(unreal.PointLight, unreal.Vector(*at(60.0, 0.0, 90.0)),
                                                    unreal.Rotator(0.0, 0.0, 0.0))
    lc = light.point_light_component
    lc.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    lc.set_editor_property("intensity", 6.0)
    lc.set_editor_property("use_temperature", True)
    lc.set_editor_property("temperature", 1900.0)
    lc.set_editor_property("attenuation_radius", 420.0)
    lc.set_editor_property("cast_shadows", False)
    light.set_actor_label(T + "DoorLamp")
    light.set_folder_path(FOLDER_B)
    # stores and work gear on the apron
    _mesh(WW1 + "SM_Sack_Barricade_01", T + "Sandbags", at(260.0, -380.0), (0.0, 0.0, yaw + 90.0 + 12.0), (0.9, 1.0, 1.0))
    _mesh(WW1 + "SM_Boxes_and_crates", T + "Stores", at(220.0, 430.0), (0.0, 0.0, yaw + 80.0))
    _mesh(WW1 + "SM_First_Aid_Box", T + "FirstAid", at(150.0, 140.0), (0.0, 0.0, yaw + 20.0))
    _mesh(WW1 + "SM_Oil_Barrel", T + "Barrel_0", at(170.0, 600.0), (0.0, 0.0, 0.0))
    _mesh(WW1 + "SM_Oil_Barrel", T + "Barrel_1", at(240.0, 650.0), (0.0, 84.0, yaw + 30.0), (1.0, 1.0, 1.0))
    _mesh(WW1 + "SM_Shoval", T + "Shovel", at(70.0, 330.0, 5.0), (0.0, -14.0, yaw + 180.0))
    _mesh("/Game/Megascans/3D_Assets/Mine_Cart_ufmodhpfa/S_Mine_Cart_ufmodhpfa_lod5_Var1", T + "MineCart",
          at(420.0, -560.0, 25.0), (102.0, 0.0, yaw + 20.0), (1.6, 1.6, 1.6))
    # the graves of those who went up: a row of six beside the apron, stone-rimmed, plank crosses
    rng = random.Random(46)
    for g in range(6):
        u, s = 1400.0 + rng.uniform(-25.0, 25.0), -560.0 + g * 230.0
        cx, cy, cz = at(u, s)
        _decal("/Game/Crater_Decals/Decals/Dirt/MI_Decal_Dirt_Scatter_0%d" % (1 + g % 3), T + "Grave_%d_Earth" % g,
               (cx, cy, cz), (0.0, -90.0, yaw), (120.0, 85.0, 140.0), 2)
        # a low cairn of field stones over the grave (2.1 x 0.9 m), long axis pointing at the portal
        _mesh(STONES + "SM_GS_Cluster_A", T + "Grave_%d_Cairn" % g, (cx, cy, cz - 4.0), (0.0, 0.0, yaw),
              (1.75, 0.95, 1.6))
        for k in range(8):
            a = k / 8.0 * math.tau
            px, py, pz = at(u + math.cos(a) * 112.0, s + math.sin(a) * 55.0)
            _mesh(STONES + "SM_GS_Chunk_" + "ABCD"[(g + k) % 4], T + "Grave_%d_Stone_%d" % (g, k), (px, py, pz - 6.0),
                  (0.0, 0.0, rng.uniform(0.0, 360.0)), (0.7, 0.7, 0.45))
        lean = rng.uniform(-9.0, 9.0)
        hx, hy, hz = at(u + 125.0, s)
        _mesh(WW1 + "SM_Broken_Wood_Plank_01", T + "Grave_%d_CrossPost" % g, (hx, hy, hz + 60.0),
              (0.0, 90.0 + lean, yaw + 90.0), (0.95, 1.5, 2.2))
        _mesh(WW1 + "SM_Broken_Wood_Plank_03", T + "Grave_%d_CrossBar" % g, (hx, hy, hz + 100.0),
              (lean * 0.4, 0.0, yaw + 90.0), (0.42, 1.4, 2.2))
    omb.log("clue B: shelter at face (%.0f, %.0f), traced %.0f" % (f["x"], f["y"], f["traced"]))
    return f


# ---- triggers -----------------------------------------------------------------------------------------------------
def _set_portrait(actor, member):
    try:
        actor.set_editor_property("PortraitType", getattr(unreal.RTSPortraitTypes, member))
        return "enum"
    except Exception as error:
        omb.log("PortraitType via enum failed (%s), trying text" % error)
    return "text"


def build_triggers():
    cls = unreal.EditorAssetLibrary.load_blueprint_class(TRIGGER_BP)
    sx, sy = ROOT_SPOTS[0]
    a = omb.actors_sub().spawn_actor_from_class(cls, unreal.Vector(sx, sy, omb.ground_z(sx, sy) + 100.0), unreal.Rotator(0, 0, 0))
    a.set_actor_label("ORE_ClueA_Trigger_FossilRoots")
    a.set_folder_path(FOLDER_A)
    pts = [unreal.Vector(x - sx, y - sy, omb.ground_z(x, y) - omb.ground_z(sx, sy)) for x, y in ROOT_SPOTS]
    a.set_editor_property("TriggerPoints", pts)
    a.set_editor_property("TriggerRadius", TRIGGER_RADIUS_A)
    mode_a = _set_portrait(a, "GERMAN_SCAVENGERS")
    f = adit_frame()
    bx, by = f["x"] + f["out"][0] * 700.0, f["y"] + f["out"][1] * 700.0
    b = omb.actors_sub().spawn_actor_from_class(cls, unreal.Vector(bx, by, omb.ground_z(bx, by) + 100.0), unreal.Rotator(0, 0, 0))
    b.set_actor_label("ORE_ClueB_Trigger_MinersShelter")
    b.set_folder_path(FOLDER_B)
    b.set_editor_property("TriggerRadius", TRIGGER_RADIUS_B)
    mode_b = _set_portrait(b, "GERMAN_ANNOUNCER")
    omb.log("clue triggers placed (portrait set by %s / %s)" % (mode_a, mode_b))
    return a, b


# ---- intercepted Soviet radio (9 spots, SovietMosin portrait, Russian with English subtitles) ----------------------
# (spot, x, y, radius cm): the trigger centre stands where the player first meets the site, not inside its defences
INTERCEPTS = [
    ("Checkpoint", 133.0, 7705.0, 2000.0),        # weighbridge checkpoint on the road
    ("BaseA", 2010.0, 24120.0, 2500.0),           # Wismut yard, convoy staging (east)
    ("BaseB", 7705.0, -6030.0, 2200.0),           # below the knoll's AT guns (Base B overwatch)
    ("Battery", 18760.0, 13400.0, 2500.0),        # artillery battery (NE)
    ("Viaduct", 3685.0, -2948.0, 1800.0),         # broken A4 viaduct gap
    ("SurveyPost", -5628.0, 11658.0, 1800.0),     # Forge survey post by the Radixite field
    ("AmmoRoute", 9045.0, 22445.0, 1800.0),       # ammo trucks' road up to the battery
    ("ConvoyRoad", -2814.0, 1842.0, 2000.0),      # ambush corridor below the ridge
    ("PeatBog", -2211.0, -24120.0, 1800.0),       # smouldering peat bog (west)
]
INTERCEPT_LINES = "/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OreMountains/"
FOLDER_I = omb.FOLDER + "Clues/Intercepts"
# owner's picks 2026-11-16; a spot with two lines gets a second trigger further in (see intercept_triggers)
INTERCEPT_PICKS = {
    "Checkpoint": ["OreMountains_Intercept_Checkpoint", "OreMountains_Intercept_Checkpoint_B"],
    "BaseA": ["OreMountains_Intercept_BaseA", "OreMountains_Intercept_BaseA_B"],
    "BaseB": "OreMountains_Intercept_BaseB",
    "Battery": "OreMountains_Intercept_Battery",
    "Viaduct": "OreMountains_Intercept_Viaduct",
    "SurveyPost": "OreMountains_Intercept_SurveyPost",
    "AmmoRoute": "OreMountains_Intercept_AmmoRoute",
    "ConvoyRoad": "OreMountains_Intercept_ConvoyRoad",
    "PeatBog": "OreMountains_Intercept_PeatBog",
}
# the second line's sphere is this share of the spot radius, so it fires once the player pushes into the site; the
# portrait manager queues it behind the first line if that is still playing
SECOND_LINE_RADIUS_SHARE = 0.5


def intercept_triggers(picks=None):
    """[(label, x, y, radius, line or None)] for every intercept trigger: one per spot, plus <Spot>_2 for a 2nd line."""
    out = []
    for spot, x, y, radius in INTERCEPTS:
        lines = (picks or {}).get(spot)
        lines = lines if isinstance(lines, (list, tuple)) else [lines]
        out.append(("ORE_Intercept_" + spot, x, y, radius, lines[0]))
        for k, line in enumerate(lines[1:], start=2):
            out.append(("ORE_Intercept_%s_%d" % (spot, k), x, y, radius * SECOND_LINE_RADIUS_SHARE ** (k - 1), line))
    return out


def build_intercepts(picks=None):
    """One BP_ScriptTrigger_PortraitLine per spot (SovietMosin). picks = {spot: SoundWave name or [names]}; spots
    without a pick get no line (the trigger then fires silently). Labels ORE_Intercept_<Spot>[_2]. Re-runnable."""
    omb.require_map()
    removed = omb.clear_prefix("ORE_Intercept_")
    cls = unreal.EditorAssetLibrary.load_blueprint_class(TRIGGER_BP)
    placed = []
    for label, x, y, radius, line in intercept_triggers(picks):
        a = omb.actors_sub().spawn_actor_from_class(cls, unreal.Vector(x, y, omb.ground_z(x, y) + 100.0), unreal.Rotator(0, 0, 0))
        a.set_actor_label(label)
        a.set_folder_path(FOLDER_I)
        a.set_editor_property("TriggerRadius", radius)
        a.set_editor_property("PortraitType", unreal.RTSPortraitTypes.SOVIET_MOSIN)
        if line:
            a.set_editor_property("VoiceLine", unreal.load_asset(INTERCEPT_LINES + line))
        placed.append((label, line))
    omb.log("intercepts: removed %d, placed %s" % (removed, placed))
    return placed


def clear():
    return omb.clear_prefix("ORE_ClueA_") + omb.clear_prefix("ORE_ClueB_")


def build_all(assets=True):
    omb.require_map()
    omb.load_layout()
    removed = clear()
    if assets:
        build_decal_assets()
    build_roots()
    build_shelter()
    build_triggers()
    omb.log("clues rebuilt (removed %d old actors)" % removed)
