"""One-off edit of the LIVE M_SupplyPoint_WornSteel (2026-11-17, user: reuse a roughness map other objects use and make
the roughness scratches black). Mirrors sp_wear_materials.py after that change without rebuilding the master (a
delete_all on a live master has killed the shared editor): new colour / roughness / metallic Custom nodes are added
and wired from the old nodes' sources, the old three are deleted. Run once in the shared editor (UECP execute_python):
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/ue_apply_scratch_roughness.py').read())
Result in Saved/SupplyPointWear/scratch_apply.txt.
"""
import unreal

exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/sp_wear_materials.py').read())
HOME = '/Game/RTS_Survival/Blueprints/Environment/SupplyPoint/Materials/'
COLOUR_PINS = ['Masks', 'Grunge', 'Height', 'PaintColour', 'PaintFade', 'StreakColour', 'RustColour', 'RustTint',
               'BareSteel', 'DustColour', 'GrimeColour', 'GrimeHeight', 'GrimeAmount']
OUTPUT_OF = {'Grunge': 'R', 'RustColour': 'RGB'}
LOG = []


def note(msg):
    LOG.append(str(msg))


def inputs_of(m, expr):
    return list(MEL.get_inputs_for_material_expression(m, expr))


def apply():
    m = unreal.load_asset(HOME + 'M_SupplyPoint_WornSteel')
    colour_old = MEL.get_material_property_input_node(m, MP.MP_BASE_COLOR)
    rough_old = MEL.get_material_property_input_node(m, MP.MP_ROUGHNESS)
    metal_old = MEL.get_material_property_input_node(m, MP.MP_METALLIC)
    for name, node_ in (('colour', colour_old), ('rough', rough_old), ('metal', metal_old)):
        if not isinstance(node_, unreal.MaterialExpressionCustom):
            raise RuntimeError(f'{name} output is not a Custom node: {node_}')
        note(f'old {name}: {node_.get_editor_property("description")} pins '
             f'{[str(p.get_editor_property("input_name")) for p in node_.get_editor_property("inputs")]}')
    colour_in = inputs_of(m, colour_old)
    pins = [str(p.get_editor_property('input_name')) for p in colour_old.get_editor_property('inputs')]
    if pins != COLOUR_PINS:
        raise RuntimeError(f'colour pins changed: {pins}')
    masks = colour_in[0]
    scratch = inputs_of(m, masks)[3]
    if not (isinstance(scratch, unreal.MaterialExpressionTextureSampleParameter2D)
            and str(scratch.get_editor_property('parameter_name')) == 'Scratches'):
        raise RuntimeError(f'masks pin 3 is not the Scratches sampler: {scratch}')
    note(f'scratch texture {scratch.get_editor_property("texture").get_name()}')

    # colour: same sources, the BareSteel parameter becomes a black ScratchColour
    scratch_colour = colour_in[COLOUR_PINS.index('BareSteel')]
    scratch_colour.set_editor_property('parameter_name', 'ScratchColour')
    scratch_colour.set_editor_property('default_value', lin(8, 8, 8))
    new_pins = [('ScratchColour' if p == 'BareSteel' else p) for p in COLOUR_PINS]
    colour = custom(m, STEEL_COLOUR, new_pins, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -700, -650, 'steel colour')
    wire(m, colour, {pin: (src, OUTPUT_OF.get(pin, '')) for pin, src in zip(new_pins, colour_in)})

    # roughness: the HQ roughness map (Scratches sampler, G) + the existing paint / rust parameters
    rough_in = inputs_of(m, rough_old)          # Masks, PaintRoughness, RustRoughness, BareRoughness
    rough = custom(m, STEEL_ROUGH, list(ROUGH_PINS), unreal.CustomMaterialOutputType.CMOT_FLOAT1, -700, -50,
                   'steel roughness')
    wire(m, rough, {'Masks': (masks, ''), 'RoughnessMap': (scratch, 'G'),
                    'RoughnessMapStrength': (scalar(m, 'RoughnessMapStrength', 1.0, -1000, 230, 'Surface'), ''),
                    'PaintRoughness': (rough_in[1], ''), 'RustRoughness': (rough_in[2], ''),
                    'ScratchRoughness': (scalar(m, 'ScratchRoughness', 0.85, -1000, 300, 'Scratches'), '')})
    bare_roughness = rough_in[3]

    # metallic: black scratches are not bare metal any more
    metal_in = inputs_of(m, metal_old)          # Masks, PaintMetallic
    metal = custom(m, STEEL_METAL, ['Masks', 'PaintMetallic'], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -700, 350,
                   'steel metallic')
    wire(m, metal, {'Masks': (masks, ''), 'PaintMetallic': (metal_in[1], '')})

    MEL.connect_material_property(colour, '', MP.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', MP.MP_ROUGHNESS)
    MEL.connect_material_property(metal, '', MP.MP_METALLIC)
    for old in (colour_old, rough_old, metal_old, bare_roughness):
        MEL.delete_material_expression(m, old)
    MEL.recompile_material(m)
    s = MEL.get_statistics(m)
    note(f'recompiled: PS {s.num_pixel_shader_instructions} samplers {s.num_samplers}')
    note(f'save master {EAL.save_loaded_asset(m, False)}')

    mi = unreal.load_asset(HOME + 'MI_SupplyPoint_CraneSteel')
    for k, v in CRANE['scalars'].items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    note(f'save crane MI {EAL.save_loaded_asset(mi, False)} ScratchAmount '
         f'{MEL.get_material_instance_scalar_parameter_value(mi, "ScratchAmount")}')


try:
    apply()
except Exception as error:  # noqa: BLE001
    import traceback
    note('FAILED ' + traceback.format_exc())
finally:
    open(unreal.Paths.project_saved_dir() + 'SupplyPointWear/scratch_apply.txt', 'w').write('\n'.join(LOG))
