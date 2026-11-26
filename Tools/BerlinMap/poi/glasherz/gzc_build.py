"""Glasherz: the ground-zero crystal of Ger_Berlin (owner pick A of Docs/Concepts/BerlinCraterCrystal/v01, 2026-11-26).
Runs in the throwaway -nullrhi editor of gzc_editor.sh (client gzc_cmd.py), one step per request:

    exec(open(r'.../gzc_build.py').read())
    probe()            # crater floor, what stands at the centre, Forge station direction, crater light units
    import_meshes()    # SM_Berlin_Glasherz_Crystal / _Collar, legacy FBX importer (keeps UCX), slots -> materials
    build_materials()  # MI_GH_Glass, MI_GH_Core (existing glass masters), M_GH_FrostDecal (new, deferred decal)
    place()            # GZC_* actors at the crater centre, +X of the collar toward the Forge station
    then fc-style nav wait (gzc_wait_nav.py) and save_map()

Meshes come from D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinCraterCrystal/Model
(gen_glasherz.py). Labels GZC_Glasherz_* in folder Berlin/GroundZeroCrystal (never the POI_ prefix: berlin_map_build
clears it). Rerunnable: place() deletes its own GZC_ actors first, imports replace the meshes.
"""
import math

import unreal

MODEL = "D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinCraterCrystal/Model/"
HOME = "/Game/RTS_Survival/Blueprints/Environment/Berlin/GroundZeroCrystal"
RUS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/"
FS = "/Game/RTS_Survival/Blueprints/Environment/Berlin/ForgeStation/"
GLASS_T = "/Game/RTS_Survival/Blueprints/Environment/FrozenLake/ProductionMaterials/M_FL_RadixiteGlass_Transparent"
GLASS_O = RUS + "GerBxps/Refinery/Kaskade/M_SD_RadixiteGlass"
SLOTS = {
    "SM_Berlin_Glasherz_Crystal": {"M_GH_Glass": HOME + "/MI_GH_Glass", "M_GH_Core": HOME + "/MI_GH_Core"},
    "SM_Berlin_Glasherz_Collar": {
        "M_GH_Concrete": RUS + "RusBunkers/Materials/MI_SovietConcrete_Tilling",
        "M_GH_CamoSteel": "/Game/RTS_Survival/Meshes/Buildings/Industrial/Textures/M_Nuclear_Tower_Soviet",
        "M_GH_Lagging": FS + "MI_FS_CryoInsulation",
        "M_GH_Steel": FS + "MI_FS_BareSteel",
        "M_GH_Lamp": RUS + "GerBxps/Bunkers/BreakthroughShared/M_BF_LampGlass",
    },
}
CENTRE = (-2167.0, -3000.0)        # crater region centre, Saved/Berlin/terrain/Ger_Berlin_layout.json (cm)
STATION = (363.0, 1382.0)          # Forge station pivot (BerlinForgeStation.md), used if no tagged actor is found
FOLDER = "Berlin/GroundZeroCrystal"
PREFIX = "GZC_Glasherz_"
MEL = unreal.MaterialEditingLibrary


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def _save(obj):
    if not unreal.EditorAssetLibrary.save_loaded_asset(obj, False):
        raise RuntimeError("save failed " + obj.get_path_name())


def _floor(x, y):
    """Landscape height (cm) under (x, y), plus the labels of anything else the ray passes."""
    hits = unreal.SystemLibrary.line_trace_multi(world(), unreal.Vector(x, y, 60000), unreal.Vector(x, y, -60000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                                 unreal.DrawDebugTrace.NONE, True) or []
    z, other = None, []
    for h in hits:
        t = h.to_tuple()
        a = t[9] if len(t) > 9 else None
        name = a.get_class().get_name() if a else "?"
        if "Landscape" in name and z is None:
            z = t[4].z
        elif a:
            other.append(a.get_actor_label())
    return z, other


def station_xy():
    for a in actors().get_all_level_actors():
        if a.actor_has_tag("ForgeStation") or a.get_class().get_name().startswith("BerlinForgeStation"):
            p = a.get_actor_location()
            return (p.x, p.y), a.get_actor_label()
    return STATION, "fallback"


def probe():
    cx, cy = CENTRE
    z0, other = _floor(cx, cy)
    print("centre floor z", z0, "other hits", other)
    ring = []
    for k in range(12):
        a = 2 * math.pi * k / 12
        for rr in (360, 600):
            z, _ = _floor(cx + math.cos(a) * rr, cy + math.sin(a) * rr)
            ring.append(z - z0 if z is not None and z0 is not None else None)
    print("floor dz at r 3.6/6 m (cm):", [round(v) if v is not None else None for v in ring])
    near = []
    for a in actors().get_all_level_actors():
        p = a.get_actor_location()
        d = math.hypot(p.x - cx, p.y - cy)
        if d < 2000:
            near.append((round(d), a.get_actor_label(), a.get_class().get_name()))
    for n in sorted(near)[:40]:
        print("near", n)
    (sx, sy), lab = station_xy()
    print("station", lab, sx, sy, "yaw from centre", round(math.degrees(math.atan2(sy - cy, sx - cx)), 1))
    for a in actors().get_all_level_actors():
        if a.get_actor_label() == "ATM_CraterLight":
            lc = a.light_component
            print("crater light", lc.get_editor_property("intensity"), lc.get_editor_property("intensity_units"),
                  lc.get_editor_property("attenuation_radius"), a.get_actor_location())
    m = unreal.load_asset(GLASS_T)
    for n in ("Roughness", "Specular", "GlowBase", "GlowRim", "GlowStrength"):
        print("glassT", n, MEL.get_material_default_scalar_parameter_value(m, n))
    for n in ("GlassColor", "GlowColor"):
        print("glassT", n, MEL.get_material_default_vector_parameter_value(m, n))
    return z0


# ---------------------------------------------------------------- meshes
def _import(name):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("import_animations", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = ui.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("one_convex_hull_per_ucx", True)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = MODEL + name + ".FBX"
    task.destination_path = HOME
    task.destination_name = name
    task.automated = True
    task.save = False
    task.replace_existing = True
    task.options = ui
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")
    mesh = unreal.load_asset(f"{HOME}/{name}")
    if mesh is None:
        raise RuntimeError("import failed " + name)
    mesh.set_editor_property("light_map_coordinate_index", 1)
    return mesh


def _assign(mesh):
    """Per slot via UStaticMesh.SetMaterial: writing the static_materials array back is silently replaced by
    WorldGridMaterial in the -nullrhi editor."""
    want = SLOTS[mesh.get_name()]
    out = []
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(sm.get_editor_property("material_slot_name"))
        key = next((k for k in want if slot.startswith(k)), None)
        if key is None:
            raise RuntimeError(f"unexpected slot {slot} on {mesh.get_name()}")
        mat = unreal.load_asset(want[key])
        if mat is None:
            raise RuntimeError("missing material " + want[key])
        mesh.set_material(i, mat)
        got = mesh.get_material(i)
        if got != mat:
            raise RuntimeError(f"slot {slot} kept {got}")
        out.append((slot, mat.get_name()))
    return out


def import_meshes():
    res = {}
    for name in SLOTS:
        mesh = _import(name)
        body = mesh.get_editor_property("body_setup")
        hulls = len(body.get_editor_property("agg_geom").get_editor_property("convex_elems")) if body else 0
        res[name] = dict(hulls=hulls, bounds=str(mesh.get_bounding_box()))
    return res


def assign_and_save():
    out = {}
    for name in SLOTS:
        mesh = unreal.load_asset(f"{HOME}/{name}")
        out[name] = _assign(mesh)
        _save(mesh)
    return out


# ---------------------------------------------------------------- materials
def _mi(name, parent, scalars, vectors):
    path = f"{HOME}/{name}"
    mi = unreal.load_asset(path)
    if mi is None:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, HOME, unreal.MaterialInstanceConstant,
                                                                     unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, unreal.load_asset(parent))
    for k, v in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in vectors.items():
        MEL.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v))
    MEL.update_material_instance(mi)
    _save(mi)
    return mi


GLASS = dict(scalars={"Roughness": 0.03, "Specular": 0.9, "GlowBase": 0.12, "GlowRim": 0.9, "GlowStrength": 1.6},
             vectors={"GlassColor": (0.25, 0.36, 0.48, 1), "GlowColor": (0.35, 0.68, 1.0, 1)})
CORE = dict(scalars={"Roughness": 0.2, "Specular": 0.6, "GlowBase": 1.0, "GlowRim": 1.0, "GlowStrength": 10.0},
            vectors={"GlassColor": (0.25, 0.55, 1.0, 1), "GlowColor": (0.45, 0.78, 1.0, 1)})


def _expr(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def frost_decal():
    """Deferred decal: pale cryo frost, opacity = radial falloff broken up by procedural noise."""
    name = "M_GH_FrostDecal"
    path = f"{HOME}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    m = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, HOME, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = _expr(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    c = _expr(m, unreal.MaterialExpressionConstant2Vector, -1200, 120)
    c.set_editor_property("r", 0.5)
    c.set_editor_property("g", 0.5)
    dist = _expr(m, unreal.MaterialExpressionDistance, -1000, 40)
    MEL.connect_material_expressions(uv, "", dist, "A")
    MEL.connect_material_expressions(c, "", dist, "B")
    r2 = _expr(m, unreal.MaterialExpressionMultiply, -850, 40)
    r2.set_editor_property("const_b", 2.0)
    MEL.connect_material_expressions(dist, "", r2, "A")
    inv = _expr(m, unreal.MaterialExpressionOneMinus, -700, 40)
    MEL.connect_material_expressions(r2, "", inv, "")
    fall = _expr(m, unreal.MaterialExpressionMultiply, -550, 40)
    fall.set_editor_property("const_b", 1.7)
    MEL.connect_material_expressions(inv, "", fall, "A")
    sc = _expr(m, unreal.MaterialExpressionMultiply, -1000, 260)
    sc.set_editor_property("const_b", 7.0)
    MEL.connect_material_expressions(uv, "", sc, "A")
    ap = _expr(m, unreal.MaterialExpressionAppendVector, -850, 260)
    zc = _expr(m, unreal.MaterialExpressionConstant, -1000, 380)
    MEL.connect_material_expressions(sc, "", ap, "A")
    MEL.connect_material_expressions(zc, "", ap, "B")
    nz = _expr(m, unreal.MaterialExpressionNoise, -700, 260)
    nz.set_editor_property("scale", 1.0)
    nz.set_editor_property("levels", 5)
    nz.set_editor_property("output_min", 0.0)
    nz.set_editor_property("output_max", 1.0)
    MEL.connect_material_expressions(ap, "", nz, "Position")
    add = _expr(m, unreal.MaterialExpressionAdd, -400, 120)
    MEL.connect_material_expressions(fall, "", add, "A")
    MEL.connect_material_expressions(nz, "", add, "B")
    sub = _expr(m, unreal.MaterialExpressionSubtract, -260, 120)
    sub.set_editor_property("const_b", 1.05)
    MEL.connect_material_expressions(add, "", sub, "A")
    gain = _expr(m, unreal.MaterialExpressionMultiply, -130, 120)
    gain.set_editor_property("const_b", 2.5)
    MEL.connect_material_expressions(sub, "", gain, "A")
    sat = _expr(m, unreal.MaterialExpressionSaturate, 0, 120)
    MEL.connect_material_expressions(gain, "", sat, "")
    col = _expr(m, unreal.MaterialExpressionConstant3Vector, -200, -150)
    col.set_editor_property("constant", unreal.LinearColor(0.72, 0.82, 0.92, 1))
    rough = _expr(m, unreal.MaterialExpressionConstant, -200, -40)
    rough.set_editor_property("r", 0.75)
    MEL.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(sat, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    _save(m)
    return m


def build_materials():
    _mi("MI_GH_Glass", GLASS_T, GLASS["scalars"], GLASS["vectors"])
    _mi("MI_GH_Core", GLASS_O, CORE["scalars"], CORE["vectors"])
    frost_decal()
    return [unreal.EditorAssetLibrary.does_asset_exist(f"{HOME}/{n}") for n in ("MI_GH_Glass", "MI_GH_Core",
                                                                               "M_GH_FrostDecal")]


# ---------------------------------------------------------------- placement
LIGHT = dict(intensity=3.0, radius=1400.0, colour=(0.45, 0.72, 1.0), height=420.0)


def clear():
    n = 0
    for a in actors().get_all_level_actors():
        if a.get_actor_label().startswith(PREFIX):
            actors().destroy_actor(a)
            n += 1
    return n


def _dup(label):
    """Copy an existing actor (spawn_actor_from_class asks the viewport for a placement point and divides by zero
    under -nullrhi)."""
    src = next(a for a in actors().get_all_level_actors() if a.get_actor_label() == label)
    d = actors().duplicate_actor(src, world(), unreal.Vector(0, 0, 0))
    d.tags = []
    d.set_actor_scale3d(unreal.Vector(1, 1, 1))
    return d


def place(light_units=None):
    removed = clear()
    cx, cy = CENTRE
    z0, other = _floor(cx, cy)
    if z0 is None:
        raise RuntimeError("no landscape under the crater centre")
    (sx, sy), lab = station_xy()
    yaw = math.degrees(math.atan2(sy - cy, sx - cx))
    loc = unreal.Vector(cx, cy, z0)
    rot = unreal.Rotator(0.0, 0.0, yaw)
    made = []
    for name in SLOTS:
        mesh = unreal.load_asset(f"{HOME}/{name}")
        a = _dup("ATM_Shimmer_0")
        a.set_actor_location_and_rotation(loc, rot, False, True)
        c = a.static_mesh_component
        c.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
        c.set_static_mesh(mesh)
        c.set_editor_property("override_materials", [])
        c.set_collision_profile_name("BlockAll")
        c.set_editor_property("can_ever_affect_navigation", True)
        c.set_editor_property("cast_shadow", True)
        c.set_editor_property("visible_in_scene_capture_only", False)
        c.set_editor_property("hidden_in_scene_capture", False)
        c.set_editor_property("render_in_main_pass", True)
        c.set_editor_property("affect_distance_field_lighting", True)
        c.set_editor_property("component_tags", [])          # the donor card carries the BerlinAtmosphere tag
        c.set_editor_property("visible_in_ray_tracing", True)
        c.set_editor_property("receives_decals", name.endswith("Collar"))
        c.set_visibility(True)
        a.set_actor_hidden_in_game(False)
        a.set_actor_label(PREFIX + name.split("_")[-1])
        a.set_folder_path(FOLDER)
        made.append(a.get_actor_label())
    src_decal = next(a.get_actor_label() for a in actors().get_all_level_actors()
                     if a.get_actor_label().startswith("DET_D_pan_debris_"))
    d = _dup(src_decal)
    d.set_actor_location_and_rotation(unreal.Vector(cx, cy, z0 + 50), unreal.Rotator(0, -90, yaw), False, True)
    d.decal.set_decal_material(unreal.load_asset(f"{HOME}/M_GH_FrostDecal"))
    d.decal.set_editor_property("decal_size", unreal.Vector(250, 950, 950))
    d.decal.set_editor_property("sort_order", 10)
    d.set_actor_label(PREFIX + "Frost")
    d.set_folder_path(FOLDER)
    made.append(d.get_actor_label())
    lt = _dup("ATM_CraterLight")
    lt.set_actor_location(unreal.Vector(cx, cy, z0 + LIGHT["height"]), False, True)
    lc = lt.light_component
    lc.set_editor_property("light_function_material", None)
    lc.set_editor_property("component_tags", [])            # donor = ATM_CraterLight (exponent falloff 2.4, unitless)
    if light_units is not None:
        lc.set_editor_property("intensity_units", light_units)
    lc.set_editor_property("intensity", LIGHT["intensity"])
    lc.set_editor_property("attenuation_radius", LIGHT["radius"])
    lc.set_editor_property("light_color", unreal.Color(r=int(LIGHT["colour"][0] * 255), g=int(LIGHT["colour"][1] * 255),
                                                       b=int(LIGHT["colour"][2] * 255), a=255))
    lc.set_editor_property("cast_shadows", False)
    lc.set_editor_property("source_radius", 150.0)
    lt.set_actor_label(PREFIX + "Light")
    lt.set_folder_path(FOLDER)
    made.append(lt.get_actor_label())
    return dict(removed=removed, made=made, floor_z=z0, yaw=round(yaw, 1), station=lab, other_hits=other)


def report():
    out = []
    for a in actors().get_all_level_actors():
        if not a.get_actor_label().startswith(PREFIX):
            continue
        p, r = a.get_actor_location(), a.get_actor_rotation()
        row = [a.get_actor_label(), a.get_class().get_name(), round(p.x), round(p.y), round(p.z), round(r.yaw, 1),
               a.get_folder_path(), list(a.tags)]
        if isinstance(a, unreal.StaticMeshActor):
            c = a.static_mesh_component
            row += [c.static_mesh.get_name(), str(c.get_collision_profile_name()),
                    [m.get_name() if m else None for m in c.get_materials()], c.get_editor_property("cast_shadow")]
        if isinstance(a, unreal.PointLight):
            lc = a.light_component
            row += [lc.get_editor_property("intensity"), lc.get_editor_property("light_function_material")]
        out.append(row)
    return out


def save_map():
    ok = unreal.EditorLoadingAndSavingUtils.save_packages([world().get_outermost()], False)
    print("saved", ok)
    return ok
