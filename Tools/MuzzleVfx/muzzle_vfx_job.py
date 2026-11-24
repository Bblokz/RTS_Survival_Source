"""Headless editor jobs for the small arms muzzle VFX (run by build_muzzle_vfx.sh; Docs/VFX/SmallArmsMuzzleVfx.md).

  muzzle_vfx_job.py build   creates the flash materials and NS_SmallArmsMuzzle when missing, then runs
                            RTS.MuzzleVfx.BuildNiagaraSystem (adds missing layer emitters, writes the HLSL, saves)
  muzzle_vfx_job.py patch   RTS.MuzzleVfx.PatchNiagaraSystem only (after editing MuzzleHlsl/*.hlsl)
"""
import sys

import unreal

FOLDER = "/Game/RTS_Survival/ImpactSfxVfx/MuzzleVfx"
FLASH_TEXTURE = "/Game/RTS_Survival/SpecialEffects/sA_ShootingVfxPack/Materials/Textures/T_Muzzle_1"
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(text):
    unreal.log("RTS_MUZZLE_VFX_JOB " + text)


def new_additive_material(name):
    material = TOOLS.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_niagara_sprites", True)
    material.set_editor_property("used_with_niagara_mesh_particles", True)
    return material


def build_flash_material():
    """Emissive = muzzle texture * particle colour * particle alpha; additive, so it fades without a dark edge."""
    material = new_additive_material("M_SmallArmsMuzzleFlash")
    color = MEL.create_material_expression(material, unreal.MaterialExpressionParticleColor, -700, 0)
    texture = MEL.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -700, -250)
    texture.set_editor_property("parameter_name", "FlashTexture")
    texture.set_editor_property("texture", unreal.load_asset(FLASH_TEXTURE))
    tinted = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, -150)
    MEL.connect_material_expressions(texture, "RGB", tinted, "A")
    MEL.connect_material_expressions(color, "RGB", tinted, "B")
    faded = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, -100)
    MEL.connect_material_expressions(tinted, "", faded, "A")
    MEL.connect_material_expressions(color, "A", faded, "B")
    MEL.connect_material_property(faded, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(material)
    return material


def build_glow_material():
    """Soft round falloff from the sprite centre: pow(saturate(1 - 2 * distance(uv, 0.5)), 2.5) * colour * alpha."""
    material = new_additive_material("M_SmallArmsMuzzleGlow")
    color = MEL.create_material_expression(material, unreal.MaterialExpressionParticleColor, -900, 200)
    uv = MEL.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1100, -100)
    centre = MEL.create_material_expression(material, unreal.MaterialExpressionConstant2Vector, -1100, 0)
    centre.set_editor_property("r", 0.5)
    centre.set_editor_property("g", 0.5)
    distance = MEL.create_material_expression(material, unreal.MaterialExpressionDistance, -900, -50)
    MEL.connect_material_expressions(uv, "", distance, "A")
    MEL.connect_material_expressions(centre, "", distance, "B")
    doubled = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -750, -50)
    doubled.set_editor_property("const_b", 2.0)
    MEL.connect_material_expressions(distance, "", doubled, "A")
    inverted = MEL.create_material_expression(material, unreal.MaterialExpressionOneMinus, -600, -50)
    MEL.connect_material_expressions(doubled, "", inverted, "")
    clamped = MEL.create_material_expression(material, unreal.MaterialExpressionSaturate, -480, -50)
    MEL.connect_material_expressions(inverted, "", clamped, "")
    falloff = MEL.create_material_expression(material, unreal.MaterialExpressionPower, -360, -50)
    falloff.set_editor_property("const_exponent", 2.5)
    MEL.connect_material_expressions(clamped, "", falloff, "Base")
    tinted = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -220, 50)
    MEL.connect_material_expressions(falloff, "", tinted, "A")
    MEL.connect_material_expressions(color, "RGB", tinted, "B")
    faded = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -80, 80)
    MEL.connect_material_expressions(tinted, "", faded, "A")
    MEL.connect_material_expressions(color, "A", faded, "B")
    MEL.connect_material_property(faded, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(material)
    return material


SMOKE_PUFFS = "/Game/RTS_Survival/Blueprints/Environment/AmbientFx/T_AmbientDust_Puffs"


def build_smoke_material():
    """Lit powder smoke on the 2x2 puff atlas of the dust devils (R = density, G = darker inner billow).
    Base colour = particle colour darkened by the billow; opacity = saturate(density * 1.8 * particle alpha), faded
    where the puff meets the ground."""
    material = TOOLS.create_asset("M_SmallArmsMuzzleSmoke", FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("translucency_lighting_mode",
                                 unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    material.set_editor_property("used_with_niagara_sprites", True)
    color = MEL.create_material_expression(material, unreal.MaterialExpressionParticleColor, -900, 0)
    puffs = MEL.create_material_expression(material, unreal.MaterialExpressionParticleSubUV, -900, -300)
    puffs.set_editor_property("texture", unreal.load_asset(SMOKE_PUFFS))
    puffs.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    shade = MEL.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -600, -200)
    shade.set_editor_property("const_a", 1.0)
    shade.set_editor_property("const_b", 0.7)
    MEL.connect_material_expressions(puffs, "G", shade, "Alpha")
    base = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, -150)
    MEL.connect_material_expressions(color, "RGB", base, "A")
    MEL.connect_material_expressions(shade, "", base, "B")
    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    density = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -600, 100)
    MEL.connect_material_expressions(puffs, "R", density, "A")
    MEL.connect_material_expressions(color, "A", density, "B")
    boosted = MEL.create_material_expression(material, unreal.MaterialExpressionMultiply, -450, 100)
    boosted.set_editor_property("const_b", 1.8)
    MEL.connect_material_expressions(density, "", boosted, "A")
    clamped = MEL.create_material_expression(material, unreal.MaterialExpressionSaturate, -320, 100)
    MEL.connect_material_expressions(boosted, "", clamped, "")
    fade = MEL.create_material_expression(material, unreal.MaterialExpressionDepthFade, -180, 100)
    fade.set_editor_property("fade_distance_default", 40.0)
    MEL.connect_material_expressions(clamped, "", fade, "Opacity")
    MEL.connect_material_property(fade, "", unreal.MaterialProperty.MP_OPACITY)
    rough = MEL.create_material_expression(material, unreal.MaterialExpressionConstant, -180, 250)
    rough.set_editor_property("r", 1.0)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(material)
    return material


def ensure_assets():
    for name, builder in (("M_SmallArmsMuzzleFlash", build_flash_material), ("M_SmallArmsMuzzleGlow", build_glow_material),
                          ("M_SmallArmsMuzzleSmoke", build_smoke_material)):
        path = FOLDER + "/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            log("exists " + path)
            continue
        material = builder()
        log("created %s saved=%s" % (path, unreal.EditorAssetLibrary.save_loaded_asset(material, False)))
    system_path = FOLDER + "/NS_SmallArmsMuzzle"
    if not unreal.EditorAssetLibrary.does_asset_exist(system_path):
        system = TOOLS.create_asset("NS_SmallArmsMuzzle", FOLDER, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
        log("created %s saved=%s" % (system_path, unreal.EditorAssetLibrary.save_loaded_asset(system, False)))


args = sys.argv[1:]
if not args or args[0] not in ("build", "patch"):
    raise SystemExit("usage: muzzle_vfx_job.py build | patch")
if args[0] == "build":
    ensure_assets()
    unreal.SystemLibrary.execute_console_command(None, "RTS.MuzzleVfx.BuildNiagaraSystem")
else:
    unreal.SystemLibrary.execute_console_command(None, "RTS.MuzzleVfx.PatchNiagaraSystem")
