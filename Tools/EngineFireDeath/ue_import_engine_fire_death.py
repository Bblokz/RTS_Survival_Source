"""Imports the engine deck debris and builds the two engine fire death materials.

Run headless (no editor needs to be open):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script="<this file>" -unattended -nop4
Creates in /Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck:
  SM_EngineDeck_* (box collision for the physics flight) with M_EngineDeckDebris (procedural burnt steel),
  M_WreckChar_Overlay (translucent unlit overlay: soot around the engine + embers that cool down;
  parameters CharCentre, CharRadius, EmberStartTime, EmberCoolDown are set by UWreckEngineFireDeathComponent).
"""
import os

import unreal

DEST = '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/EngineDeck'
EXPORT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Export')
PIECES = ['SM_EngineDeck_Plate_A', 'SM_EngineDeck_Plate_B', 'SM_EngineDeck_Grille', 'SM_EngineDeck_HatchLid',
          'SM_EngineDeck_Muffler', 'SM_EngineDeck_Fan']

mel = unreal.MaterialEditingLibrary
assets = unreal.AssetToolsHelpers.get_asset_tools()


def log(text):
    unreal.log_warning('EFD ' + text)


def fresh_material(name):
    path = DEST + '/' + name
    # Rebuild in place: the debris meshes and the C++ default keep pointing at the same asset.
    # Load by object path: an editor that was open while another process created the asset has no registry entry.
    material = unreal.load_asset(path + '.' + name)
    if material is not None:
        mel.delete_all_material_expressions(material)
        return material
    return assets.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def link(src, dst, dst_input, src_output=''):
    if not mel.connect_material_expressions(src, src_output, dst, dst_input):
        raise RuntimeError('link failed %s -> %s.%s' % (src.get_name(), dst.get_name(), dst_input))


def const3(material, rgb, x, y):
    return node(material, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(*rgb, 1.0))


def const(material, value, x, y):
    return node(material, unreal.MaterialExpressionConstant, x, y, r=value)


def noise(material, position, scale, x, y, out_min=0.0, out_max=1.0, levels=4):
    expression = node(material, unreal.MaterialExpressionNoise, x, y, scale=scale, levels=levels,
                      output_min=out_min, output_max=out_max, turbulence=True, quality=1)
    link(position, expression, '')
    return expression


def binary(material, cls, a, b, x, y):
    expression = node(material, cls, x, y)
    link(a, expression, 'A')
    link(b, expression, 'B')
    return expression


def lerp(material, a, b, alpha, x, y):
    expression = node(material, unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, expression, 'A')
    link(b, expression, 'B')
    link(alpha, expression, 'Alpha')
    return expression


def saturate(material, source, x, y):
    expression = node(material, unreal.MaterialExpressionSaturate, x, y)
    link(source, expression, '')
    return expression


def build_debris_material():
    material = fresh_material('M_EngineDeckDebris')
    material.set_editor_property('two_sided', True)
    # Local position: the pattern sticks to the piece while it tumbles.
    position = node(material, unreal.MaterialExpressionLocalPosition, -1400, 0)
    rust_noise = noise(material, position, 0.035, -1150, -200)
    paint_noise = noise(material, position, 0.02, -1150, 100)
    scorched = const3(material, (0.018, 0.016, 0.014), -900, -400)
    rust = const3(material, (0.17, 0.075, 0.03), -900, -300)
    paint = const3(material, (0.16, 0.15, 0.11), -900, 300)
    rust_mask = saturate(material, binary(material, unreal.MaterialExpressionMultiply,
                                          binary(material, unreal.MaterialExpressionSubtract, rust_noise,
                                                 const(material, 0.45, -1000, -100), -800, -150),
                                          const(material, 3.0, -1000, -50), -650, -150), -500, -150)
    paint_mask = saturate(material, binary(material, unreal.MaterialExpressionMultiply,
                                           binary(material, unreal.MaterialExpressionSubtract, paint_noise,
                                                  const(material, 0.62, -1000, 200), -800, 150),
                                           const(material, 4.0, -1000, 250), -650, 150), -500, 150)
    burnt = lerp(material, scorched, rust, rust_mask, -350, -250)
    base = lerp(material, burnt, paint, paint_mask, -150, 0)
    mel.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(const(material, 0.35, -150, 200), '', unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(const(material, 0.82, -150, 280), '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    log('built M_EngineDeckDebris')
    return material


def scalar_param(material, name, default, x, y):
    return node(material, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=default)


def build_char_overlay():
    material = fresh_material('M_WreckChar_Overlay')
    material.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)

    world = node(material, unreal.MaterialExpressionWorldPosition, -2200, 0)
    centre = node(material, unreal.MaterialExpressionVectorParameter, -2200, -250, parameter_name='CharCentre',
                  default_value=unreal.LinearColor(0, 0, 0, 0))
    centre_rgb = node(material, unreal.MaterialExpressionComponentMask, -2000, -250, r=True, g=True, b=True, a=False)
    link(centre, centre_rgb, '')
    radius = scalar_param(material, 'CharRadius', 300.0, -2000, -120)
    distance = binary(material, unreal.MaterialExpressionDistance, world, centre_rgb, -1800, -200)
    # 1 at the engine, 0 at CharRadius.
    falloff = node(material, unreal.MaterialExpressionOneMinus, -1450, -200)
    link(saturate(material, binary(material, unreal.MaterialExpressionDivide, distance, radius, -1650, -200),
                  -1550, -200), falloff, '')

    blotch = noise(material, world, 0.012, -1800, 100)
    fine = noise(material, world, 0.05, -1800, 350, levels=3)
    # Small specks for the embers, about a hand wide.
    speck_noise = noise(material, world, 0.16, -1800, 500, levels=2)
    # Heavy char: falloff pushed up and broken by the blotch noise.
    char_input = binary(material, unreal.MaterialExpressionAdd,
                        binary(material, unreal.MaterialExpressionMultiply, falloff,
                               const(material, 1.7, -1450, -80), -1250, -200),
                        binary(material, unreal.MaterialExpressionMultiply,
                               binary(material, unreal.MaterialExpressionSubtract, blotch,
                                      const(material, 0.6, -1550, 200), -1400, 100),
                               const(material, 0.9, -1450, 250), -1250, 100), -1050, -100)
    char_mask = saturate(material, char_input, -900, -100)
    # Light soot over the whole wreck so it reads as burnt out, not only around the engine.
    soot = binary(material, unreal.MaterialExpressionMultiply, fine, const(material, 0.45, -1450, 450), -1250, 350)
    char_opacity = binary(material, unreal.MaterialExpressionMax,
                          binary(material, unreal.MaterialExpressionMultiply, char_mask,
                                 const(material, 0.93, -900, 0), -750, -100), soot, -550, 0)

    # Heat: 1 at EmberStartTime, 0 after EmberCoolDown seconds, squared so the glow fades fast at first.
    time = node(material, unreal.MaterialExpressionTime, -1800, 650)
    start = scalar_param(material, 'EmberStartTime', 0.0, -1800, 750)
    cool = scalar_param(material, 'EmberCoolDown', 35.0, -1800, 850)
    elapsed = binary(material, unreal.MaterialExpressionSubtract, time, start, -1600, 700)
    remaining = node(material, unreal.MaterialExpressionOneMinus, -1250, 700)
    link(saturate(material, binary(material, unreal.MaterialExpressionDivide, elapsed, cool, -1450, 700), -1350, 700),
         remaining, '')
    heat = binary(material, unreal.MaterialExpressionMultiply, remaining, remaining, -1100, 700)
    # Embers: bright specks of the fine noise, only in the hot core near the engine.
    specks = saturate(material, binary(material, unreal.MaterialExpressionMultiply,
                                       binary(material, unreal.MaterialExpressionSubtract, speck_noise,
                                              const(material, 0.72, -1450, 550), -1250, 550),
                                       const(material, 9.0, -1250, 620), -1100, 550), -950, 550)
    # Hot core near the engine; the blotch noise breaks its edge so it reads as burnt patches, not a disc.
    core_ramp = binary(material, unreal.MaterialExpressionAdd,
                       binary(material, unreal.MaterialExpressionSubtract,
                              binary(material, unreal.MaterialExpressionMultiply, falloff,
                                     const(material, 2.6, -1100, 420), -950, 400),
                              const(material, 1.5, -1100, 470), -800, 420),
                       binary(material, unreal.MaterialExpressionMultiply,
                              binary(material, unreal.MaterialExpressionSubtract, blotch,
                                     const(material, 0.5, -1100, 330), -950, 320),
                              const(material, 1.4, -1100, 370), -800, 330), -680, 380)
    core_linear = saturate(material, core_ramp, -560, 380)
    core = binary(material, unreal.MaterialExpressionMultiply, core_linear, core_linear, -450, 420)
    ember_mask = binary(material, unreal.MaterialExpressionMultiply,
                        binary(material, unreal.MaterialExpressionMultiply, specks, core, -650, 550), heat, -500, 600)

    soot_colour = const3(material, (0.006, 0.005, 0.004), -500, -300)
    ember_colour = binary(material, unreal.MaterialExpressionMultiply,
                          const3(material, (1.0, 0.28, 0.05), -650, 750),
                          scalar_param(material, 'EmberStrength', 5.0, -650, 850), -450, 780)
    emissive = lerp(material, soot_colour, ember_colour, ember_mask, -250, 300)
    opacity = binary(material, unreal.MaterialExpressionMax, char_opacity, ember_mask, -250, 100)
    mel.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, False)
    log('built M_WreckChar_Overlay')


def import_pieces(debris_material):
    tasks = []
    for name in PIECES:
        task = unreal.AssetImportTask()
        task.filename = os.path.join(EXPORT_DIR, name + '.fbx')
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        options = unreal.FbxImportUI()
        options.import_mesh = True
        options.import_materials = False
        options.import_textures = False
        options.import_as_skeletal = False
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
        options.static_mesh_import_data.generate_lightmap_u_vs = False
        task.options = options
        tasks.append(task)
    assets.import_asset_tasks(tasks)
    for name in PIECES:
        path = DEST + '/' + name
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if mesh is None:
            log('MISSING ' + path)
            continue
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
        unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptingCollisionShapeType.BOX)
        mesh.set_material(0, debris_material)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        bounds = mesh.get_bounding_box()
        size = bounds.max - bounds.min
        log('imported %s tris=%d collision=%d size=%.0fx%.0fx%.0f' % (
            name, unreal.EditorStaticMeshLibrary.get_number_triangles(mesh, 0) if hasattr(
                unreal.EditorStaticMeshLibrary, 'get_number_triangles') else -1,
            unreal.EditorStaticMeshLibrary.get_simple_collision_count(mesh), size.x, size.y, size.z))


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.EditorAssetLibrary.make_directory(DEST)
    debris_material = build_debris_material()
    build_char_overlay()
    import_pieces(debris_material)
    for name in PIECES + ['M_EngineDeckDebris', 'M_WreckChar_Overlay']:
        path = DEST + '/' + name
        log('%s exists=%s' % (path, unreal.EditorAssetLibrary.does_asset_exist(path)))


main()
