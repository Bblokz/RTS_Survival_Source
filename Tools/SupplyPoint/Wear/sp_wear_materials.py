"""Supply point wear pass (2026-11-17, user: "too not post apocalyptic ... make it PBR and ensure it shows damage and
wear"). Supply-point-only materials; the shared Breakthrough M_BF_* assets stay untouched.

    M_SupplyPoint_WornSteel   crane steel master (Base slot 1, UpperWorks, Trolley, HookBlock; Rope via its own MI)
        faded field-grey paint, chipped through to rust (Megascans chipped-paint mask), rust patches that grow toward
        the ground and on top faces, rust streaks running down every side face (mesh space, so they ride with the
        slewing jib), black scratches, top dust and ground grime. All four PBR channels change with the masks.
        Roughness reuses the HQ buildings' map T_ScratchedPaintedMetal_tduwej0c_4K_MR (G = roughness, R = the
        scratches), and its scratches go black in the base colour the way M_Building_PostGray does it (2026-11-17).
    MI_SupplyPoint_CraneSteel (CRANE values, tuned in game 2026-11-17), MI_SupplyPoint_Rope
    M_SupplyPoint_Timber -> MI_SupplyPoint_Timber      Base slot 0, on T_SupplyPoint_Trim_* (compose_trim.py: weathered board row, rusted
        drum row) plus per-area tone, damp lower timber, mud splash from the ground, moss and dust on the tops.

Used two ways (exec this file, then call the functions):
  * preview in a throwaway editor (ue_preview_wear.py): HOME = a scratch folder that is never saved;
  * final build in the shared editor (ue_build_wear.py): HOME = the supply point Materials folder, saved.
Never rebuild a master in place once meshes use it (delete_all_material_expressions on a live master has killed the
shared editor): build() refuses to touch an existing master unless REBUILD_EXISTING is set.
"""
import unreal

WEAR_TEXTURES = 'D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/Textures/'
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty

SRC = {
    'ChipMask': '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_xhtnbhlg/T_RustyPaintedMetalSheet_xhtnbhlg_2K_ORDp',
    'ChipNormal': '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_xhtnbhlg/T_Rusty_Painted_Metal_Sheet_xhtnbhlg_2K_N',
    'RustColour': '/Game/QuarryEnv/Textures/MeshTextures/T_Rust_BC',
    'RustNormal': '/Game/Megascans/Surfaces/Rusty_Metal_vmghfhro/T_Rusty_Metal_vmghfhro_4K_N',
    'Scratches': '/Game/RTS_Survival/Materials/Buildings/HQ/Textures/T_ScratchedPaintedMetal_tduwej0c_4K_MR',
    'Leaks': '/Game/UVB80/Assets/UVB80/Buildings/Textures/T_WindowDirtLeaksGrunge',
    'Grunge': '/Game/Greek_island/Textures/T_Mask/T_grunge_mask',
}
TRIM = ['T_SupplyPoint_Trim_BaseColor', 'T_SupplyPoint_Trim_NormalDX', 'T_SupplyPoint_Trim_ORM']


def lin(r, g, b):
    """sRGB 0-255 -> linear colour."""
    def f(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(f(r), f(g), f(b), 1.0)


# =====================================================================================================================
# graph helpers
def node(m, cls, x, y):
    return MEL.create_material_expression(m, cls, x, y)


def scalar(m, name, value, x, y, group):
    n = node(m, unreal.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    n.set_editor_property('group', group)
    return n


def vector(m, name, value, x, y, group):
    n = node(m, unreal.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('default_value', value)
    n.set_editor_property('group', group)
    return n


def sampler_for(texture):
    comp = texture.get_editor_property('compression_settings')
    srgb = texture.get_editor_property('srgb')
    T, S = unreal.TextureCompressionSettings, unreal.MaterialSamplerType
    if comp == T.TC_NORMALMAP:
        return S.SAMPLERTYPE_NORMAL
    if comp == T.TC_MASKS:
        return S.SAMPLERTYPE_MASKS
    if comp == T.TC_GRAYSCALE:
        return S.SAMPLERTYPE_GRAYSCALE if srgb else S.SAMPLERTYPE_LINEAR_GRAYSCALE
    if comp == T.TC_ALPHA:
        return S.SAMPLERTYPE_ALPHA
    return S.SAMPLERTYPE_COLOR if srgb else S.SAMPLERTYPE_LINEAR_COLOR


def tex(m, name, texture, x, y, group='Textures'):
    n = node(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    n.set_editor_property('parameter_name', name)
    n.set_editor_property('texture', texture)
    n.set_editor_property('sampler_type', sampler_for(texture))
    n.set_editor_property('sampler_source', unreal.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    n.set_editor_property('group', group)
    return n


def custom(m, code, inputs, out_type, x, y, desc):
    n = node(m, unreal.MaterialExpressionCustom, x, y)
    n.set_editor_property('code', code)
    n.set_editor_property('output_type', out_type)
    n.set_editor_property('description', desc)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property('input_name', name)
        pins.append(pin)
    n.set_editor_property('inputs', pins)
    return n


def wire(m, target, feeds):
    for pin, (source, output) in feeds.items():
        if not MEL.connect_material_expressions(source, output, target, pin):
            raise RuntimeError(f'could not wire {pin} on {target.get_editor_property("description")}')


def frame_inputs(m, x, y):
    """Shared position inputs: mesh-space position + normal (patterns ride along with moving parts) and the height
    above the actor root (the supply point stands on its pivot, so this is height above the ground)."""
    wp = node(m, unreal.MaterialExpressionWorldPosition, x - 300, y)
    local = node(m, unreal.MaterialExpressionTransformPosition, x, y)
    local.set_editor_property('transform_source_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type', unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    MEL.connect_material_expressions(wp, '', local, '')
    actor = node(m, unreal.MaterialExpressionActorPositionWS, x - 300, y + 90)
    rel = node(m, unreal.MaterialExpressionSubtract, x, y + 90)
    MEL.connect_material_expressions(wp, '', rel, 'A')
    MEL.connect_material_expressions(actor, '', rel, 'B')
    height = node(m, unreal.MaterialExpressionComponentMask, x + 150, y + 90)
    for ch, on in (('r', False), ('g', False), ('b', True), ('a', False)):
        height.set_editor_property(ch, on)
    MEL.connect_material_expressions(rel, '', height, '')
    wn = node(m, unreal.MaterialExpressionVertexNormalWS, x - 300, y + 180)
    ln = node(m, unreal.MaterialExpressionTransform, x, y + 180)
    ln.set_editor_property('transform_source_type', unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD)
    ln.set_editor_property('transform_type', unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    MEL.connect_material_expressions(wn, '', ln, '')
    return local, height, wn, ln


def new_material(home, name):
    path = f'{home}/{name}'
    if EAL.does_asset_exist(path):
        m = unreal.load_asset(path)
        if not globals().get('REBUILD_EXISTING'):
            raise RuntimeError(f'{path} exists; set REBUILD_EXISTING only for an unassigned scratch copy')
        while MEL.get_num_material_expressions(m):
            before = MEL.get_num_material_expressions(m)
            MEL.delete_all_material_expressions(m)
            if MEL.get_num_material_expressions(m) == before:
                raise RuntimeError('could not clear ' + path)
        return m
    return TOOLS.create_asset(name, home, unreal.Material, unreal.MaterialFactoryNew())


# =====================================================================================================================
# HLSL (Custom nodes). Inputs that are not textures are plain floats/float3s.
PLANE_UV = r'''
// mesh-space planar UV on the face's dominant axis; side faces keep Z as V so streaks always run down
float3 An = abs(LocalNormal);
float2 P = An.z > max(An.x, An.y) ? Local.xy : float2(An.x > An.y ? Local.y : Local.x, -Local.z);
return P / max(Scale, 1.0);
'''

STREAK_UV = r'''
float3 An = abs(LocalNormal);
float H = An.x > An.y ? Local.y : Local.x;
return float2(H / max(ScaleH, 1.0), -Local.z / max(ScaleV, 1.0));
'''

STEEL_MASKS = r'''
float Up = saturate(NormalZ);
float Low = saturate(1.0 - Height / max(RustLowHeight, 1.0));
float Drive = RustAmount + RustLowBoost * Low * Low + TopRust * Up * Up + (Grunge - 0.5) * MacroContrast;
// the chipped-paint scan: rough = rusted chip; mixed with the macro grunge so patches merge into big rust areas
float Signal = saturate(ChipSignal) * 0.7 + Grunge * 0.3;
float Rust = smoothstep(1.0 - Drive - ChipSoftness, 1.0 - Drive + ChipSoftness, Signal);
float Side = 1.0 - Up;
float Streak = saturate((Leak - (1.0 - StreakAmount)) * 2.5) * Side * saturate(0.35 + Drive);
float Bare = saturate(Scratch * 2.2 - 0.25) * ScratchAmount * (1.0 - Rust);
float Dust = Up * Up * DustAmount * saturate(Grunge * 1.6);
return float4(Rust, Bare, Dust, Streak);
'''

STEEL_COLOUR = r'''
float3 Paint = PaintColour * (0.8 + 0.4 * Grunge);
// sun-faded, chalky paint in broad patches
Paint = lerp(Paint, Paint * 1.4 + 0.025, saturate((Grunge - 0.5) * 2.0) * PaintFade);
float3 C = lerp(Paint, StreakColour * (0.75 + 0.5 * Grunge), Masks.w * 0.65);
C = lerp(C, RustColour * RustTint, Masks.x);
C = lerp(C, ScratchColour, Masks.y);    // black scratches, as on the HQ buildings (M_Building_PostGray)
C = lerp(C, DustColour, Masks.z);
float Low = saturate(1.0 - Height / max(GrimeHeight, 1.0));
C = lerp(C, GrimeColour, Low * Low * GrimeAmount * (0.55 + 0.45 * Grunge));
return C;
'''

STEEL_ROUGH = r'''
// RoughnessMap = G of T_ScratchedPaintedMetal_tduwej0c_4K_MR, the roughness map of the HQ buildings
// (M_Building_PostGray) and the Panzer I hull, reused here as the paint's roughness detail
float Paint = saturate(PaintRoughness + (RoughnessMap - 0.5) * RoughnessMapStrength);
float R = lerp(Paint, RustRoughness, Masks.x);
R = lerp(R, max(R, 0.82), Masks.w * 0.6);
R = lerp(R, saturate(max(RoughnessMap, ScratchRoughness)), Masks.y);
return lerp(R, 0.95, Masks.z);
'''

STEEL_METAL = r'''
return saturate(PaintMetallic * (1.0 - Masks.x) * (1.0 - Masks.z));
'''

BLEND_NORMAL = r'''
float3 N = normalize(lerp(PaintNormal, RustNormal, Masks.x));
return normalize(lerp(float3(0, 0, 1), N, NormalStrength));
'''

TIMBER_COLOUR = r'''
float V = UV.y - floor(UV.y);
float WoodRow = step(0.1, V) * step(V, 0.3);
float Up = saturate(NormalZ);
float3 C = BaseColour * lerp(1.0 - ToneVariation, 1.0 + ToneVariation, Grunge);
float Low = saturate(1.0 - Height / max(DampHeight, 1.0));
C = lerp(C, C * DampTint, Low * Low);
float Moss = smoothstep(0.75, 0.9, Grunge2 * 0.8 + Up * 0.25) * Up * Up * MossAmount * WoodRow;
C = lerp(C, MossColour * (0.7 + 0.6 * Grunge), Moss);
C = lerp(C, DustColour, Up * Up * DustAmount * Grunge);
float MudField = (1.0 - Height / max(MudHeight, 1.0)) * 1.5 - Grunge2 * 0.8;
float Mud = smoothstep(0.15, 0.4, MudField) * MudAmount;
C = lerp(C, MudColour * (0.8 + 0.4 * Grunge), Mud);
return C;
'''

TIMBER_MASKS = r'''
// x = mud, y = moss, z = damp (the colour node repeats the same maths; kept in step by hand)
float V = UV.y - floor(UV.y);
float WoodRow = step(0.1, V) * step(V, 0.3);
float Up = saturate(NormalZ);
float Low = saturate(1.0 - Height / max(DampHeight, 1.0));
float Moss = smoothstep(0.75, 0.9, Grunge2 * 0.8 + Up * 0.25) * Up * Up * MossAmount * WoodRow;
float MudField = (1.0 - Height / max(MudHeight, 1.0)) * 1.5 - Grunge2 * 0.8;
float Mud = smoothstep(0.15, 0.4, MudField) * MudAmount;
return float3(Mud, Moss, Low * Low);
'''

TIMBER_ROUGH = r'''
float R = lerp(Rough, Rough * 0.85, Masks.z);
R = lerp(R, 0.97, Masks.y);
return lerp(R, MudRoughness, Masks.x);
'''

TIMBER_NORMAL = r'''
float3 N = lerp(Normal, float3(0, 0, 1), saturate(Masks.x * 0.8 + Masks.y * 0.5));
return normalize(N);
'''


# =====================================================================================================================
ROUGH_PINS = ('Masks', 'RoughnessMap', 'RoughnessMapStrength', 'PaintRoughness', 'RustRoughness', 'ScratchRoughness')


def rough_feeds(m, masks, scratch, x=-1000, y=20):
    """Feeds of the roughness node; also used by ue_apply_scratch_roughness.py on the live master."""
    return {'Masks': (masks, ''), 'RoughnessMap': (scratch, 'G'),
            'RoughnessMapStrength': (scalar(m, 'RoughnessMapStrength', 1.0, x, y, 'Surface'), ''),
            'PaintRoughness': (scalar(m, 'PaintRoughness', 0.72, x, y + 70, 'Surface'), ''),
            'RustRoughness': (scalar(m, 'RustRoughness', 0.9, x, y + 140, 'Surface'), ''),
            'ScratchRoughness': (scalar(m, 'ScratchRoughness', 0.85, x, y + 210, 'Scratches'), '')}


def build_steel(home):
    m = new_material(home, 'M_SupplyPoint_WornSteel')
    load = {k: unreal.load_asset(v) for k, v in SRC.items()}
    missing = [k for k, v in load.items() if v is None]
    if missing:
        raise RuntimeError(f'missing source textures {missing}')
    local, height, wn, ln = frame_inputs(m, -2600, 600)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -2600, -400)

    # UV0 tiles every 1.6 m on the crane parts (generate_supply_point.py), the chip scan is 2 m x 1 m
    chip_uv = custom(m, 'return UV * float2(0.8, 1.6) * Scale;', ['UV', 'Scale'],
                     unreal.CustomMaterialOutputType.CMOT_FLOAT2, -2200, -500, 'chip UV')
    wire(m, chip_uv, {'UV': (uv, ''), 'Scale': (scalar(m, 'ChipScale', 0.5, -2450, -440, 'Chips'), '')})
    rust_uv = custom(m, 'return UV * Scale;', ['UV', 'Scale'], unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                     -2200, -300, 'rust UV')
    wire(m, rust_uv, {'UV': (uv, ''), 'Scale': (scalar(m, 'RustScale', 0.6, -2450, -280, 'Rust'), '')})
    scratch_uv = custom(m, 'return UV * Scale;', ['UV', 'Scale'], unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                        -2200, -150, 'scratch UV')
    wire(m, scratch_uv, {'UV': (uv, ''), 'Scale': (scalar(m, 'ScratchScale', 0.8, -2450, -130, 'Scratches'), '')})
    streak_uv = custom(m, STREAK_UV, ['Local', 'LocalNormal', 'ScaleH', 'ScaleV'],
                       unreal.CustomMaterialOutputType.CMOT_FLOAT2, -2200, 0, 'streak UV (mesh space)')
    wire(m, streak_uv, {'Local': (local, ''), 'LocalNormal': (ln, ''),
                        'ScaleH': (scalar(m, 'StreakScaleH', 180.0, -2450, 20, 'Streaks'), ''),
                        'ScaleV': (scalar(m, 'StreakScaleV', 320.0, -2450, 90, 'Streaks'), '')})
    macro_uv = custom(m, PLANE_UV, ['Local', 'LocalNormal', 'Scale'], unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                      -2200, 200, 'macro UV (mesh space)')
    wire(m, macro_uv, {'Local': (local, ''), 'LocalNormal': (ln, ''),
                       'Scale': (scalar(m, 'MacroScale', 420.0, -2450, 220, 'Rust'), '')})

    chip = tex(m, 'ChipMask', load['ChipMask'], -1900, -560)
    chip_n = tex(m, 'ChipNormal', load['ChipNormal'], -1900, -380)
    rust_c = tex(m, 'RustColour', load['RustColour'], -1900, -200)
    rust_n = tex(m, 'RustNormal', load['RustNormal'], -1900, -20)
    scratch = tex(m, 'Scratches', load['Scratches'], -1900, 160)
    leaks = tex(m, 'Leaks', load['Leaks'], -1900, 340)
    grunge = tex(m, 'Grunge', load['Grunge'], -1900, 520)
    for t, u in ((chip, chip_uv), (chip_n, chip_uv), (rust_c, rust_uv), (rust_n, rust_uv), (scratch, scratch_uv),
                 (leaks, streak_uv), (grunge, macro_uv)):
        MEL.connect_material_expressions(u, '', t, 'UVs')
    nz = node(m, unreal.MaterialExpressionComponentMask, -2300, 820)
    for ch, on in (('r', False), ('g', False), ('b', True), ('a', False)):
        nz.set_editor_property(ch, on)
    MEL.connect_material_expressions(wn, '', nz, '')

    masks = custom(m, STEEL_MASKS, ['ChipSignal', 'Grunge', 'Leak', 'Scratch', 'Height', 'NormalZ', 'RustAmount',
                                    'RustLowBoost', 'RustLowHeight', 'TopRust', 'MacroContrast', 'ChipSoftness',
                                    'StreakAmount', 'ScratchAmount', 'DustAmount'],
                   unreal.CustomMaterialOutputType.CMOT_FLOAT4, -1300, -300, 'steel masks: rust, bare, dust, streak')
    wire(m, masks, {
        'ChipSignal': (chip, 'G'), 'Grunge': (grunge, 'R'), 'Leak': (leaks, 'R'), 'Scratch': (scratch, 'R'),
        'Height': (height, ''), 'NormalZ': (nz, ''),
        'RustAmount': (scalar(m, 'RustAmount', 0.38, -1600, -900, 'Rust'), ''),
        'RustLowBoost': (scalar(m, 'RustLowBoost', 0.35, -1600, -830, 'Rust'), ''),
        'RustLowHeight': (scalar(m, 'RustLowHeight', 250.0, -1600, -760, 'Rust'), ''),
        'TopRust': (scalar(m, 'TopRust', 0.18, -1600, -690, 'Rust'), ''),
        'MacroContrast': (scalar(m, 'MacroContrast', 0.6, -1600, -620, 'Rust'), ''),
        'ChipSoftness': (scalar(m, 'ChipSoftness', 0.04, -1600, -550, 'Chips'), ''),
        'StreakAmount': (scalar(m, 'StreakAmount', 0.45, -1600, -480, 'Streaks'), ''),
        'ScratchAmount': (scalar(m, 'ScratchAmount', 0.7, -1600, -410, 'Scratches'), ''),
        'DustAmount': (scalar(m, 'DustAmount', 0.35, -1600, -340, 'Weathering'), ''),
    })

    colour = custom(m, STEEL_COLOUR, ['Masks', 'Grunge', 'Height', 'PaintColour', 'PaintFade', 'StreakColour',
                                      'RustColour', 'RustTint', 'ScratchColour', 'DustColour', 'GrimeColour',
                                      'GrimeHeight', 'GrimeAmount'],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, -700, -500, 'steel colour')
    wire(m, colour, {
        'Masks': (masks, ''), 'Grunge': (grunge, 'R'), 'Height': (height, ''),
        'PaintColour': (vector(m, 'PaintColour', lin(84, 86, 80), -1000, -1100, 'Paint'), ''),
        'PaintFade': (scalar(m, 'PaintFade', 0.35, -1000, -1030, 'Paint'), ''),
        'StreakColour': (vector(m, 'StreakColour', lin(84, 62, 44), -1000, -960, 'Streaks'), ''),
        'RustColour': (rust_c, 'RGB'),
        'RustTint': (vector(m, 'RustTint', unreal.LinearColor(1.2, 1.15, 0.8, 1.0), -1000, -890, 'Rust'), ''),
        'ScratchColour': (vector(m, 'ScratchColour', lin(8, 8, 8), -1000, -820, 'Scratches'), ''),
        'DustColour': (vector(m, 'DustColour', lin(150, 138, 118), -1000, -750, 'Weathering'), ''),
        'GrimeColour': (vector(m, 'GrimeColour', lin(48, 42, 34), -1000, -680, 'Weathering'), ''),
        'GrimeHeight': (scalar(m, 'GrimeHeight', 110.0, -1000, -610, 'Weathering'), ''),
        'GrimeAmount': (scalar(m, 'GrimeAmount', 0.6, -1000, -540, 'Weathering'), ''),
    })
    rough = custom(m, STEEL_ROUGH, list(ROUGH_PINS), unreal.CustomMaterialOutputType.CMOT_FLOAT1, -700, 0,
                   'steel roughness')
    wire(m, rough, rough_feeds(m, masks, scratch))
    metal = custom(m, STEEL_METAL, ['Masks', 'PaintMetallic'], unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                   -700, 250, 'steel metallic')
    wire(m, metal, {'Masks': (masks, ''), 'PaintMetallic': (scalar(m, 'PaintMetallic', 0.0, -1000, 260, 'Surface'), '')})
    normal = custom(m, BLEND_NORMAL, ['PaintNormal', 'RustNormal', 'Masks', 'NormalStrength'],
                    unreal.CustomMaterialOutputType.CMOT_FLOAT3, -700, 400, 'steel normal')
    wire(m, normal, {'PaintNormal': (chip_n, 'RGB'), 'RustNormal': (rust_n, 'RGB'), 'Masks': (masks, ''),
                     'NormalStrength': (scalar(m, 'NormalStrength', 1.0, -1000, 420, 'Surface'), '')})

    MEL.connect_material_property(colour, '', MP.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', MP.MP_ROUGHNESS)
    MEL.connect_material_property(metal, '', MP.MP_METALLIC)
    MEL.connect_material_property(normal, '', MP.MP_NORMAL)
    MEL.recompile_material(m)
    return m


def import_trim(home):
    """Imports the compose_trim.py PNGs into <home>/Textures (replacing), with the right compression."""
    tasks = []
    for name in TRIM:
        task = unreal.AssetImportTask()
        task.filename = WEAR_TEXTURES + name + '.png'
        task.destination_path = home + '/Textures'
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = False
        tasks.append(task)
    TOOLS.import_asset_tasks(tasks)
    out = {}
    for name in TRIM:
        t = unreal.load_asset(f'{home}/Textures/{name}')
        if t is None:
            raise RuntimeError('trim import failed: ' + name)
        if name.endswith('NormalDX'):
            t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property('srgb', False)
        elif name.endswith('ORM'):
            t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
            t.set_editor_property('srgb', False)
        out[name] = t
    return out


def build_timber(home, trim):
    m = new_material(home, 'M_SupplyPoint_Timber')
    grunge_tex = unreal.load_asset(SRC['Grunge'])
    local, height, wn, ln = frame_inputs(m, -2200, 600)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate, -2200, -400)
    bc = tex(m, 'BaseColor', trim['T_SupplyPoint_Trim_BaseColor'], -1700, -500)
    nrm = tex(m, 'Normal', trim['T_SupplyPoint_Trim_NormalDX'], -1700, -320)
    orm = tex(m, 'ORM', trim['T_SupplyPoint_Trim_ORM'], -1700, -140)
    for t in (bc, nrm, orm):
        MEL.connect_material_expressions(uv, '', t, 'UVs')
    macro_uv = custom(m, PLANE_UV, ['Local', 'LocalNormal', 'Scale'], unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                      -2000, 100, 'tone UV (mesh space)')
    wire(m, macro_uv, {'Local': (local, ''), 'LocalNormal': (ln, ''),
                       'Scale': (scalar(m, 'ToneScale', 380.0, -2250, 120, 'Weathering'), '')})
    mud_uv = custom(m, PLANE_UV, ['Local', 'LocalNormal', 'Scale'], unreal.CustomMaterialOutputType.CMOT_FLOAT2,
                    -2000, 260, 'mud/moss UV (mesh space)')
    wire(m, mud_uv, {'Local': (local, ''), 'LocalNormal': (ln, ''),
                     'Scale': (scalar(m, 'MudScale', 120.0, -2250, 280, 'Weathering'), '')})
    grunge = tex(m, 'Grunge', grunge_tex, -1700, 60)
    grunge2 = tex(m, 'Grunge', grunge_tex, -1700, 240)
    MEL.connect_material_expressions(macro_uv, '', grunge, 'UVs')
    MEL.connect_material_expressions(mud_uv, '', grunge2, 'UVs')
    nz = node(m, unreal.MaterialExpressionComponentMask, -1900, 820)
    for ch, on in (('r', False), ('g', False), ('b', True), ('a', False)):
        nz.set_editor_property(ch, on)
    MEL.connect_material_expressions(wn, '', nz, '')

    shared = {'UV': (uv, ''), 'Grunge2': (grunge2, 'R'), 'Height': (height, ''), 'NormalZ': (nz, ''),
              'DampHeight': (scalar(m, 'DampHeight', 90.0, -1300, -1000, 'Weathering'), ''),
              'MossAmount': (scalar(m, 'MossAmount', 0.6, -1300, -930, 'Weathering'), ''),
              'MudHeight': (scalar(m, 'MudHeight', 45.0, -1300, -860, 'Weathering'), ''),
              'MudAmount': (scalar(m, 'MudAmount', 0.85, -1300, -790, 'Weathering'), '')}
    masks = custom(m, TIMBER_MASKS, list(shared.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT3, -900, 200,
                   'timber masks: mud, moss, damp')
    wire(m, masks, shared)
    colour_feeds = dict(shared)
    colour_feeds.update({
        'BaseColour': (bc, 'RGB'), 'Grunge': (grunge, 'R'),
        'ToneVariation': (scalar(m, 'ToneVariation', 0.2, -1300, -720, 'Weathering'), ''),
        'DampTint': (vector(m, 'DampTint', unreal.LinearColor(0.74, 0.70, 0.64, 1.0), -1300, -650, 'Weathering'), ''),
        'MossColour': (vector(m, 'MossColour', lin(70, 78, 48), -1300, -580, 'Weathering'), ''),
        'DustColour': (vector(m, 'DustColour', lin(140, 128, 108), -1300, -510, 'Weathering'), ''),
        'DustAmount': (scalar(m, 'DustAmount', 0.3, -1300, -440, 'Weathering'), ''),
        'MudColour': (vector(m, 'MudColour', lin(58, 49, 38), -1300, -370, 'Weathering'), ''),
    })
    colour = custom(m, TIMBER_COLOUR, list(colour_feeds.keys()), unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                    -600, -400, 'timber colour')
    wire(m, colour, colour_feeds)
    rough = custom(m, TIMBER_ROUGH, ['Masks', 'Rough', 'MudRoughness'], unreal.CustomMaterialOutputType.CMOT_FLOAT1,
                   -600, 100, 'timber roughness')
    wire(m, rough, {'Masks': (masks, ''), 'Rough': (orm, 'G'),
                    'MudRoughness': (scalar(m, 'MudRoughness', 0.8, -900, 120, 'Weathering'), '')})
    normal = custom(m, TIMBER_NORMAL, ['Normal', 'Masks'], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -600, 300,
                    'timber normal')
    wire(m, normal, {'Normal': (nrm, 'RGB'), 'Masks': (masks, '')})

    MEL.connect_material_property(colour, '', MP.MP_BASE_COLOR)
    MEL.connect_material_property(rough, '', MP.MP_ROUGHNESS)
    MEL.connect_material_property(orm, 'B', MP.MP_METALLIC)
    MEL.connect_material_property(orm, 'R', MP.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(normal, '', MP.MP_NORMAL)
    MEL.recompile_material(m)
    return m


def make_instance(home, name, parent, scalars=None, vectors=None):
    path = f'{home}/{name}'
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else TOOLS.create_asset(
        name, home, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    return mi


# MI_SupplyPoint_CraneSteel: master defaults plus the in-game tuning (more rust on up-facing steel, which is what
# the top-down RTS camera sees; bigger scratches so they survive at game distance)
CRANE = dict(scalars={'RustAmount': 0.42, 'TopRust': 0.32, 'StreakAmount': 0.5, 'ScratchScale': 0.25,
                      'ScratchAmount': 0.9})
ROPE = dict(scalars={'PaintMetallic': 0.65, 'PaintRoughness': 0.42, 'RustAmount': 0.2, 'RustLowBoost': 0.0,
                     'TopRust': 0.0, 'StreakAmount': 0.0, 'ScratchAmount': 0.0, 'DustAmount': 0.1, 'PaintFade': 0.0,
                     'GrimeAmount': 0.0, 'ChipScale': 2.0},
            vectors={'PaintColour': lin(40, 38, 35)})


def build(home):
    """Every supply point wear asset under home; returns {name: asset}."""
    steel = build_steel(home)
    trim = import_trim(home)
    timber = build_timber(home, trim)
    out = {'M_SupplyPoint_WornSteel': steel, 'M_SupplyPoint_Timber': timber}
    out['MI_SupplyPoint_CraneSteel'] = make_instance(home, 'MI_SupplyPoint_CraneSteel', steel, **CRANE)
    out['MI_SupplyPoint_Timber'] = make_instance(home, 'MI_SupplyPoint_Timber', timber)
    out['MI_SupplyPoint_Rope'] = make_instance(home, 'MI_SupplyPoint_Rope', steel, **ROPE)
    out.update(trim)
    return out


# which slot of which supply point mesh gets what (slot names from the FBX = the old material names)
ASSIGN = {
    'SM_SupplyPoint_Base': {'M_BF_Trim': 'MI_SupplyPoint_Timber', 'M_BF_Crane_PaintedSteel': 'MI_SupplyPoint_CraneSteel'},
    'SM_SupplyPoint_UpperWorks': {'M_BF_Crane_PaintedSteel': 'MI_SupplyPoint_CraneSteel'},
    'SM_SupplyPoint_Trolley': {'M_BF_Crane_PaintedSteel': 'MI_SupplyPoint_CraneSteel'},
    'SM_SupplyPoint_HookBlock': {'M_BF_Crane_PaintedSteel': 'MI_SupplyPoint_CraneSteel'},
    'SM_SupplyPoint_Rope': {'M_BF_Crane_WornSteel': 'MI_SupplyPoint_Rope'},
}
