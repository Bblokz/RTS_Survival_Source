"""Salt pan decal for Salt Flats: no landscape layer is bright enough, so the pan is a decal with a generated mask.

The generator writes T_SM_SaltPanDecal.png (R = opacity with a sharp cracked edge, G = brightness) next to the layout.
`ensure_salt_assets` imports it and builds the decal material once; `step_salt_decal` places the decal.
"""
import unreal

import survival_map_build as smb

ROOT = "/Game/RTS_Survival/SurvivalMode/Maps/Materials"
TEXTURE = ROOT + "/T_SM_SaltPanDecal"
MATERIAL = ROOT + "/M_SM_SaltPanDecal"


def import_texture(png_path):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", png_path)
    task.set_editor_property("destination_path", ROOT)
    task.set_editor_property("destination_name", "T_SM_SaltPanDecal")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(TEXTURE)
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("never_stream", True)  # low mips of a mask make a stair of blocks at the pan's edge
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)
    return texture


def build_material(texture):
    lib = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(MATERIAL):
        material = unreal.load_asset(MATERIAL)
        lib.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_SM_SaltPanDecal", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    sample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -700, 0)
    sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    colour = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -700, -300)
    colour.set_editor_property("constant", unreal.LinearColor(0.30, 0.12, 0.058, 1.0))  # round 7: a crust about 10 % paler than the sand round it
    multiply = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, -200)
    lib.connect_material_expressions(colour, "", multiply, "A")
    lib.connect_material_expressions(sample, "G", multiply, "B")
    lib.connect_material_property(multiply, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(sample, "R", unreal.MaterialProperty.MP_OPACITY)
    rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -400, 100)
    rough.set_editor_property("r", 1.0)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def ensure_salt_assets(png_path):
    return build_material(import_texture(png_path))


def step_salt_decal():
    smb.require_map()
    info = smb.LAYOUT["salt_decal"]
    z = 480.0
    actor = smb.actors_sub().spawn_actor_from_class(unreal.DecalActor, unreal.Vector(info["centre"][0], info["centre"][1], z),
                                                    unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
    decal = actor.get_component_by_class(unreal.DecalComponent)
    decal.set_decal_material(unreal.load_asset(MATERIAL))
    half = info["side"] / 2.0
    decal.set_editor_property("decal_size", unreal.Vector(700.0, half, half))
    actor.set_actor_label(smb.DRESS_PREFIX + "SaltPanDecal")
    actor.set_folder_path(smb.DRESS_FOLDER + "/Identity")
    return actor


def ensure_oasis_water(normal_texture_path):
    """Still pool: dark teal, glossy, slow ripple, and clear at the shore through depth fade."""
    lib = unreal.MaterialEditingLibrary
    path = ROOT + "/M_SM_OasisWater"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        lib.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_SM_OasisWater", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    def constant(value, x, y):
        node = lib.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
        node.set_editor_property("r", value)
        return node

    def colour(r, g, b, x, y):
        node = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
        node.set_editor_property("constant", unreal.LinearColor(r, g, b, 1.0))
        return node

    def depth(distance, y):
        fade = lib.create_material_expression(material, unreal.MaterialExpressionDepthFade, -900, y)
        lib.connect_material_expressions(constant(distance, -1100, y), "", fade, "FadeDistance")
        return fade

    # Colour by depth: sandy green at the edge to deep teal, so the pool has form even straight from above.
    tint = lib.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -500, -300)
    lib.connect_material_expressions(colour(0.17, 0.19, 0.12, -760, -420), "", tint, "A")
    lib.connect_material_expressions(colour(0.012, 0.055, 0.06, -760, -320), "", tint, "B")
    lib.connect_material_expressions(depth(130.0, -220), "", tint, "Alpha")
    lib.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # A cap on the mirror: moderate specular and some roughness keep it water at grazing angles, not a white sky sheet.
    lib.connect_material_property(constant(0.2, -500, -150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(constant(0.35, -500, -60), "", unreal.MaterialProperty.MP_SPECULAR)
    body = lib.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -500, 60)
    lib.connect_material_expressions(constant(0.45, -760, 20), "", body, "A")
    lib.connect_material_expressions(constant(0.96, -760, 100), "", body, "B")
    lib.connect_material_expressions(depth(110.0, 160), "", body, "Alpha")
    shore = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 80)
    lib.connect_material_expressions(body, "", shore, "A")
    lib.connect_material_expressions(depth(16.0, 260), "", shore, "B")
    lib.connect_material_property(shore, "", unreal.MaterialProperty.MP_OPACITY)
    texture = unreal.load_asset(normal_texture_path)
    if texture is not None:
        world = lib.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1200, 420)
        mask = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1050, 420)
        mask.set_editor_property("r", True)
        mask.set_editor_property("g", True)
        mask.set_editor_property("b", False)
        mask.set_editor_property("a", False)
        lib.connect_material_expressions(world, "", mask, "")
        tile = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -900, 420)
        lib.connect_material_expressions(mask, "", tile, "A")
        lib.connect_material_expressions(constant(0.0042, -1050, 540), "", tile, "B")  # a pond's ripples, not sea chop
        pan = lib.create_material_expression(material, unreal.MaterialExpressionPanner, -740, 420)
        pan.set_editor_property("speed_x", 0.008)
        pan.set_editor_property("speed_y", 0.005)
        lib.connect_material_expressions(tile, "", pan, "Coordinate")
        sample = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -560, 420)
        sample.set_editor_property("texture", texture)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        lib.connect_material_expressions(pan, "", sample, "UVs")
        flat = lib.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -300, 420)
        lib.connect_material_expressions(colour(0.0, 0.0, 1.0, -560, 340), "", flat, "A")
        lib.connect_material_expressions(sample, "RGB", flat, "B")
        lib.connect_material_expressions(constant(0.28, -560, 560), "", flat, "Alpha")
        lib.connect_material_property(flat, "", unreal.MaterialProperty.MP_NORMAL)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


def ensure_minimap_water():
    """Opaque stand-in for the minimap capture only; rough and dull, so it stays near sand brightness with post off."""
    lib = unreal.MaterialEditingLibrary
    path = ROOT + "/M_SM_OasisMinimapWater"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        material = unreal.load_asset(path)
        lib.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_SM_OasisMinimapWater", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    colour = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, -200)
    colour.set_editor_property("constant", unreal.LinearColor(0.03, 0.16, 0.17, 1.0))
    lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 0)
    rough.set_editor_property("r", 1.0)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = lib.create_material_expression(material, unreal.MaterialExpressionConstant, -500, 100)
    spec.set_editor_property("r", 0.0)
    lib.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material


SKIRT_MATERIAL = ROOT + "/M_SM_HorizonSkirt"
SKIRT_SAND = "/Game/RTS_Survival/Materials/ground/Landscape/Textures/05_Thai_Rippled_Sand_2x2_M_td1hcimn/td1hcimn_8K_"


def ensure_skirt_material():
    """Sand past the landscape edge: the landscape's ThaiSand branch (same albedo, tint and 400 cm tiling) on world XY."""
    lib = unreal.MaterialEditingLibrary
    if unreal.EditorAssetLibrary.does_asset_exist(SKIRT_MATERIAL):
        material = unreal.load_asset(SKIRT_MATERIAL)
        lib.delete_all_material_expressions(material)
    else:
        material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "M_SM_HorizonSkirt", ROOT, unreal.Material, unreal.MaterialFactoryNew())
    world = lib.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1100, 0)
    mask = lib.create_material_expression(material, unreal.MaterialExpressionComponentMask, -900, 0)
    mask.set_editor_property("r", True)
    mask.set_editor_property("g", True)
    mask.set_editor_property("b", False)
    mask.set_editor_property("a", False)
    lib.connect_material_expressions(world, "", mask, "")
    tile = lib.create_material_expression(material, unreal.MaterialExpressionDivide, -700, 0)
    tile.set_editor_property("const_b", 400.0)
    lib.connect_material_expressions(mask, "", tile, "A")
    samples = {}
    for k, (name, y) in enumerate((("Albedo", -300), ("Roughness", 0), ("Normal", 300))):
        s = lib.create_material_expression(material, unreal.MaterialExpressionTextureSample, -450, y)
        s.set_editor_property("texture", unreal.load_asset(SKIRT_SAND + name))
        if name == "Normal":
            s.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif name == "Roughness":
            s.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
        lib.connect_material_expressions(tile, "", s, "UVs")
        samples[name] = s
    tint = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -450, -500)
    tint.set_editor_property("constant", unreal.LinearColor(1.0, 0.564, 0.444, 1.0))
    mul = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, -300)
    lib.connect_material_expressions(samples["Albedo"], "RGB", mul, "A")
    lib.connect_material_expressions(tint, "", mul, "B")
    lib.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lib.connect_material_property(samples["Roughness"], "R", unreal.MaterialProperty.MP_ROUGHNESS)
    lib.connect_material_property(samples["Normal"], "RGB", unreal.MaterialProperty.MP_NORMAL)
    lib.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    return material
