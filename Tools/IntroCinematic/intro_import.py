"""Imports the Blender-made kits of the intro cinematic (Saved/IntroCinematic/import/<Kit>/) into
<ROOT>/Props and <ROOT>/UralShield: FBX static meshes, PBR textures, material instances, slot assignment.
Run inside the throwaway editor. Re-runnable (imports replace).
"""
import glob
import importlib
import os

import unreal

import intro_lib as lib
import intro_materials as mats

importlib.reload(mats)

SRC = "D:/UE5Projects/RTS_Survival/Saved/IntroCinematic/import"
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def _import(files, destination, options=None):
    tasks = []
    for path in files:
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = destination
        task.automated = True
        task.replace_existing = True
        task.save = False
        if options is not None:
            task.options = options
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    return ["%s/%s" % (destination, os.path.splitext(os.path.basename(path))[0]) for path in files]


def _fbx_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    data = options.static_mesh_import_data
    data.combine_meshes = True
    data.auto_generate_collision = False
    data.generate_lightmap_u_vs = False
    data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    data.remove_degenerates = False
    return options


def build_pbr_material():
    """M_Intro_PBR: BaseColor / Normal / ORM textures (R = AO, G = roughness, B = metallic), optional glow."""
    material = mats._new_material("M_Intro_PBR")

    def sample(name, y, sampler):
        node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("sampler_type", sampler)
        return node

    base = sample("BC", -200, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = sample("N", 50, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
    # The default must match the sampler (a masks texture), or the base material fails to compile in the game
    # process and every instance draws as the grey default material.
    orm = sample("ORM", 300, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    default_orm = unreal.load_asset(lib.ROOT + "/UralShield/Textures/T_UralShield_ORM")
    if default_orm is not None:
        orm.set_editor_property("texture", default_orm)
    tint = mats._vector(material, "Tint", (1.0, 1.0, 1.0, 1.0), -400)
    colour = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, -200)
    mel.connect_material_expressions(base, "RGB", colour, "A")
    mel.connect_material_expressions(tint, "", colour, "B")
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # NormalFlat pulls a too-busy normal map toward flat; RoughAdd takes the wet shine off.
    flat = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, 120)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    soft = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -350, 60)
    mel.connect_material_expressions(normal, "RGB", soft, "A")
    mel.connect_material_expressions(flat, "", soft, "B")
    mel.connect_material_expressions(mats._scalar(material, "NormalFlat", 0.0, 150), "", soft, "Alpha")
    mel.connect_material_property(soft, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -350, 320)
    mel.connect_material_expressions(orm, "G", rough, "A")
    mel.connect_material_expressions(mats._scalar(material, "RoughAdd", 0.0, 380), "", rough, "B")
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(orm, "B", unreal.MaterialProperty.MP_METALLIC)
    glow = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 500)
    mel.connect_material_expressions(base, "RGB", glow, "A")
    mel.connect_material_expressions(mats._scalar(material, "Glow", 0.0, 600), "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return mats._finish(material)


def _texture_set(folder, prefix):
    """(BC, N, ORM) texture assets named <prefix>_BC/_N/_ORM or <prefix>_BaseColor/_Normal/_ORM, or None."""
    found = []
    for suffixes in (("_BC", "_BaseColor"), ("_N", "_Normal"), ("_ORM",)):
        asset = None
        for suffix in suffixes:
            if eal.does_asset_exist(folder + "/" + prefix + suffix):
                asset = unreal.load_asset(folder + "/" + prefix + suffix)
        found.append(asset)
    return found if found[0] is not None else None


def _fix_texture_settings(folder):
    packages = []
    for data in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, False):
        if str(data.asset_class_path.asset_name) != "Texture2D":
            continue
        texture = data.get_asset()
        name = str(data.asset_name)
        if name.endswith("_N") or name.endswith("_Normal"):
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            texture.set_editor_property("srgb", False)
        elif name.endswith("_ORM"):
            texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            texture.set_editor_property("srgb", False)
        packages.append(texture.get_outermost())
    return packages


def import_kit(kit, destination, slot_rules):
    """slot_rules(mesh name, slot name) -> texture-set prefix, an existing material path, or None."""
    folder = SRC + "/" + kit
    textures = [p.replace("\\", "/") for p in glob.glob(folder + "/T_*.png")]
    meshes = [p.replace("\\", "/") for p in glob.glob(folder + "/SM_*.fbx")]
    _import(textures, destination + "/Textures")
    packages = _fix_texture_settings(destination + "/Textures")
    mesh_paths = _import(meshes, destination, _fbx_options())
    parent = unreal.load_asset(mats.MAT_DIR + "/M_Intro_PBR")
    for path in mesh_paths:
        mesh = unreal.load_asset(path)
        if mesh is None:
            print("  import failed:", path)
            continue
        name = path.rsplit("/", 1)[1]
        slots = [str(s.material_slot_name) for s in mesh.static_materials]
        for index, slot in enumerate(slots):
            rule = slot_rules(name, slot)
            if rule is None:
                continue
            if rule.startswith("/"):
                mesh.set_material(index, unreal.load_asset(rule))
                continue
            texture_set = _texture_set(destination + "/Textures", rule)
            if texture_set is None:
                print("  no textures for", name, slot, rule)
                continue
            textures_by_name = {"BC": texture_set[0]}
            if texture_set[1]:
                textures_by_name["N"] = texture_set[1]
            if texture_set[2]:
                textures_by_name["ORM"] = texture_set[2]
            mi = mats.instance("MI_" + rule[2:] if rule.startswith("T_") else "MI_" + rule, parent, textures=textures_by_name)
            mesh.set_material(index, mi)
        bounds = mesh.get_bounds()
        print("  %-28s slots %s  size %.0f x %.0f x %.0f  sockets %s" % (
            name, slots, bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2,
            [str(s.socket_name) for s in mesh.get_sockets_by_tag("")] if hasattr(mesh, "get_sockets_by_tag") else ""))
        packages.append(mesh.get_outermost())
    lib.safe_save(packages)


def _prop_rule(mesh, slot):
    prop = mesh.replace("SM_Intro_", "").replace("_Curled", "")
    if prop == "OilLamp" and slot == "Flame":
        return mats.MAT_DIR + "/MI_Intro_Flame"
    if prop == "PaperTag" and slot == "String":
        return "T_Intro_PaperTag_String"
    if prop == "Banner" and slot == "Pole":
        return "T_Intro_Banner_Pole"
    return "T_Intro_" + prop


def build_crack_material():
    """M_Intro_CrackGlow: the faces inside the crater's grooves. Glows with the MPC scalar Crystal (a little always)."""
    material = mats._new_material("M_Intro_CrackGlow")
    tint = mats._vector(material, "Tint", (1.0, 0.24, 0.02, 1.0), -200)
    glow = mats._scalar(material, "Glow", 7.0, 0)
    crystal = mats._collection(material, "Crystal", 200)
    level = mel.create_material_expression(material, unreal.MaterialExpressionAdd, -650, 150)
    mel.connect_material_expressions(crystal, "", level, "A")
    level.set_editor_property("const_b", 0.03)
    strength = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -500, 100)
    mel.connect_material_expressions(glow, "", strength, "A")
    mel.connect_material_expressions(level, "", strength, "B")
    colour = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 0)
    mel.connect_material_expressions(tint, "", colour, "A")
    mel.connect_material_expressions(strength, "", colour, "B")
    # Hot at the bottom of the groove, dark at its lips (the crater floor is at world z = 0): a crack, not a band.
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, 400)
    height = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -750, 400)
    height.set_editor_property("r", False); height.set_editor_property("g", False); height.set_editor_property("b", True)
    mel.connect_material_expressions(world, "", height, "")
    depth = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 400)
    mel.connect_material_expressions(height, "", depth, "A")
    depth.set_editor_property("const_b", -1.0 / 26.0)
    clamp = mel.create_material_expression(material, unreal.MaterialExpressionSaturate, -480, 400)
    mel.connect_material_expressions(depth, "", clamp, "")
    curve = mel.create_material_expression(material, unreal.MaterialExpressionPower, -360, 400)
    mel.connect_material_expressions(clamp, "", curve, "Base")
    curve.set_editor_property("const_exponent", 5.0)
    shaped = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -220, 200)
    mel.connect_material_expressions(colour, "", shaped, "A")
    mel.connect_material_expressions(curve, "", shaped, "B")
    mel.connect_material_property(shaped, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    black = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -350, -200)
    mel.connect_material_property(black, "", unreal.MaterialProperty.MP_BASE_COLOR)
    return mats._finish(material)


def _set_rule(mesh, slot):
    if slot == "CrackGlow":
        return mats.MAT_DIR + "/M_Intro_CrackGlow"
    if slot == "Ground":
        return "T_Intro_Crater_Ground"
    return "T_Intro_" + slot


def tune_set_materials():
    """Look of the trench kit in the engine (after import_sets, or alone after build_pbr_material)."""
    parent = unreal.load_asset(mats.MAT_DIR + "/M_Intro_PBR")
    mats.instance("MI_Intro_Earth", parent, scalars={"NormalFlat": 0.62, "RoughAdd": 0.1}, vectors={"Tint": (0.8, 0.76, 0.72, 1.0)})
    mats.instance("MI_Intro_Mould", parent, scalars={"NormalFlat": 0.15, "RoughAdd": 0.34}, vectors={"Tint": (0.8, 0.72, 0.85, 1.0)})
    mats.instance("MI_Intro_Sandbag", parent, scalars={"NormalFlat": 0.35})


def import_sets():
    build_crack_material()
    import_kit("Sets", lib.ROOT + "/Sets", _set_rule)


def import_all():
    build_pbr_material()
    import_kit("Props", lib.ROOT + "/Props", _prop_rule)
    import_kit("UralShield", lib.ROOT + "/UralShield", lambda mesh, slot: "T_UralShield")
