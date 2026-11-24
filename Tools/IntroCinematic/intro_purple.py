"""The Purple Death in the 1945 trench (shots 5.2 / 5.3): M_Intro_PurpleDeath on the mould mats, pustules and veins.

The old look was smooth pink plastic that grew by scaling the meshes. Now:
- necrotic plum-black flesh, bruised violet mottling and dry mauve spore crust, with a branching vein network drawn
  over it (ridged multifractal vessels, projected on the surface from three sides); thin sharp glowing vessel lines
  beat slowly like a pulse;
- the vein meshes (Mode 1) are dark wet tubes, plum-black at the root, with a thin glowing core (where the tube
  faces the camera) along which light pumps outward from the root;
- the growth is six baked stages per mesh (blender/sets/build_mould_stages.py), one shown at a time from the MPC
  scalar Mould (0..1) with a short patchy dissolve between them; the mats carry real raised vessels and nodules and
  sample the same field (T_Intro_MouldRelief_ORM) for their vessel colour.
Run inside the throwaway editor.
"""
import importlib

import unreal

import intro_lib as lib
import intro_materials as mats

importlib.reload(mats)

mel = unreal.MaterialEditingLibrary
M = mats.MAT_DIR + "/"

PD_LIB = mats.PD_VEINS

# Growth by baked stages (blender/sets/build_mould_stages.py): every mould mesh exists at six stages of growth; each
# stage's instance shows it while the MPC Mould clock is in [StageLo, StageHi]. The old reveal mask cut hard slabs,
# spikes and specks wherever its edge crossed geometry standing off the ground, and cut the veins off blunt.
# stage n shows from Mould = its size on (blender STAGES), the first from 0
STAGE_LO = (0.0, 0.0625, 0.075, 0.0875, 0.1, 0.1125, 0.125, 0.1375, 0.15, 0.1625, 0.175, 0.1875, 0.2, 0.2125, 0.22, 0.2469, 0.2738, 0.3007, 0.3276, 0.3545, 0.3814, 0.4083, 0.4352, 0.4621, 0.489, 0.5159, 0.5428, 0.5697, 0.5966, 0.6234, 0.6503, 0.6772, 0.7041, 0.731, 0.7579, 0.7848, 0.8117, 0.8386, 0.8655, 0.8924, 0.9193, 0.9462, 0.9731, 1)

CORE = PD_LIB + """
// Mode 0 (mats) blends its three projections softly: a hard hand-over turned the projected vessel lines into sharp L
// corners on the curved flesh; soft, they fade across it.
float3 an = pow(abs(N), 4.0 - 2.5 * step(Mode, 0.5)); an /= (an.x + an.y + an.z);
float3 p = WP / 14.0 + Seed * 3.0;
// Mode 0: mats (vessels from the baked relief, T_Intro_MouldRelief_ORM, on the mats' UVs: the same field the mesh
// raised its vessels from), 1: vein tubes, 2: pustules.
float mat = 1.0 - step(0.5, Mode);
float vein = step(0.5, Mode) * (1.0 - step(1.5, Mode));
float4 rel = Texture2DSample(Relief, ReliefSampler, float2((UV.x + 4.0) / 16.0, ((1.0 - UV.y) + 4.0) / 8.0));
float vproc = PD.veins(p.yz) * an.x + PD.veins(p.xz) * an.y + PD.veins(p.xy) * an.z;
float v = max(vproc, rel.r * mat);                       // the network everywhere, the raised vessels on the mats
float m1 = PD.fbm(p.yz * 1.3) * an.x + PD.fbm(p.xz * 1.3) * an.y + PD.fbm(p.xy * 1.3) * an.z;
float mf = PD.fbm(p.yz * 7.0) * an.x + PD.fbm(p.xz * 7.0) * an.y + PD.fbm(p.xy * 7.0) * an.z;
// This stage shows while Mould is in [StageLo, StageHi): a hard switch between small steps (a dissolve showed part
// of the next, larger shell over the current one: a mask edge off the ground again, read as tongues and stipple).
float shown = step(StageLo, Mould) * (1.0 - step(StageHi, Mould)) * step(0.004, Mould);
float beat = 0.5 + 0.5 * sin(Time * 1.6 + m1 * 7.0);
float wave = frac(UV.x * 1.1 - Time * 0.45);
float pump = smoothstep(0.55, 0.9, wave) * smoothstep(1.0, 0.92, wave) * vein;
// Colour: necrotic plum-black, bruised violet, dry mauve spore crust; dark livid veins. Only the thick veins glow,
// faintly, beating; light pumps along the vein meshes. Scary is dark and wet, not neon.
float vb = PD.big(p.yz) * an.x + PD.big(p.xz) * an.y + PD.big(p.xy) * an.z;
vb = max(smoothstep(0.6, 1.0, vb) * smoothstep(0.6, 1.0, vb), rel.r * rel.r * mat);
float3 col = lerp(float3(0.006, 0.0014, 0.008), float3(0.026, 0.0035, 0.034), smoothstep(0.4, 0.8, m1));
col = lerp(col, float3(0.045, 0.035, 0.046), smoothstep(0.64, 0.8, mf) * 0.45 * (1.0 - vein));
col = lerp(col, float3(0.03, 0.007, 0.036), rel.g * 0.4 * mat);                       // swollen nodules
col = lerp(col, float3(0.024, 0.002, 0.034), saturate(v * lerp(0.6, 0.9, mat)) * (1.0 - vein));
// The vein tubes are dark wet vessels, plum-black at the root and a little more livid toward the tips, with a
// thin glowing core: lit where the tube faces the camera, so it reads round and translucent rather than a flat
// pink ribbon (albedo stays ~0.02: this set's fixed exposure turns anything brighter into neon).
float facing = saturate(dot(N, V));
float core = pow(facing, 7.0);
float along = smoothstep(0.0, 1.6, UV.x * 100.0 / 100.0);
float3 tube = lerp(float3(0.008, 0.001, 0.011), float3(0.026, 0.003, 0.036), along) * lerp(0.45, 1.0, facing);
col = lerp(col, tube * lerp(0.45, 1.0, smoothstep(0.03, 0.32, UV.x)), vein);
// wet in the veins, but not a glossy hose; the flesh between is dull and spongy
float rough = saturate(lerp(0.92, 0.3, vein) - v * 0.25 * (1.0 - vein) + (mf - 0.5) * 0.15 * (1.0 - vein));
// Almost no specular on the flesh: on the smooth, subdivided stage meshes the trench's raking sun laid a broad
// sheen over the dark mould under this set's fixed exposure and it read as pale lilac upholstery (as the helmets
// read as clay). The vessels and nodules carry the form in the diffuse instead.
float spec = lerp(0.04, 0.55, vein);
// A vein emerges dark from the mould it grew out of and only lights up further out: a bright core right at the root
// made the line where the mould sheet crosses the vein read as a sliced-off end.
float rootDark = smoothstep(0.03, 0.32, UV.x);
float3 glow = (vb * (0.1 + 0.9 * beat) * 0.03 * (1.0 - vein) + vein * core * (0.05 + pump * 0.22) * rootDark)
            * float3(0.32, 0.015, 0.48) * Glow;
"""


def build_material():
    # Default lit: a subsurface model let the trench's low sun shine through the flesh and turned it hot pink.
    material = mats._new_material("M_Intro_PurpleDeath", blend=unreal.BlendMode.BLEND_MASKED,
                                  shading=unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1400, -600)
    local = mel.create_material_expression(material, unreal.MaterialExpressionTransformPosition, -1200, -500)
    local.set_editor_property("transform_source_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property("transform_type", unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_LOCAL)
    mel.connect_material_expressions(world, "", local, "")
    normal = mel.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1400, -400)
    coords = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -1400, -300)
    time = mel.create_material_expression(material, unreal.MaterialExpressionTime, -1400, -200)
    view = mel.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -1400, -100)
    relief = mel.create_material_expression(material, unreal.MaterialExpressionTextureObject, -1400, 0)
    relief.set_editor_property("texture", unreal.load_asset(lib.ROOT + "/Sets/Textures/T_Intro_MouldRelief_ORM"))
    inputs = [("WP", world, ""), ("LP", local, ""), ("N", normal, ""), ("UV", coords, ""), ("Time", time, ""),
              ("V", view, ""),
              ("Mould", mats._collection(material, "Mould", 0), ""), ("Mode", mats._scalar(material, "Mode", 0.0, 100), ""),
              ("StageLo", mats._scalar(material, "StageLo", 0.0, 200), ""),
              ("StageHi", mats._scalar(material, "StageHi", 9.0, 300), ""), ("Seed", mats._scalar(material, "Seed", 0.0, 400), ""),
              ("Glow", mats._scalar(material, "Glow", 1.0, 500), ""), ("Relief", relief, "")]
    surface = mats._custom(material, CORE + "return float4(col, shown);", inputs, y=0)
    light = mats._custom(material, CORE + "return float4(glow, rough);", inputs, y=300)
    mel.connect_material_property(mats._mask(material, surface, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(mats._mask(material, surface, "a", y=100), "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.connect_material_property(mats._mask(material, light, "rgb", y=300), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(mats._mask(material, light, "a", y=400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    # Low specular on the flesh (a full sheen under the trench's low sun washed it out to lavender); the vein tubes
    # are wet and catch it.
    specular = mats._custom(material, CORE + "return spec;", inputs, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1, y=600)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)

    return mats._finish(material)


STEEL = PD_LIB + """
float3 an = pow(abs(N), 4.0); an /= (an.x + an.y + an.z);
float3 p = WP / 9.0;
float rust = PD.fbm(p.yz * 2.0) * an.x + PD.fbm(p.xz * 2.0) * an.y + PD.fbm(p.xy * 2.0) * an.z;
float spot = PD.fbm(p.yz * 4.5 + 7.0) * an.x + PD.fbm(p.xz * 4.5 + 7.0) * an.y + PD.fbm(p.xy * 4.5 + 7.0) * an.z;
float v = PD.veins(p.yz * 0.7) * an.x + PD.veins(p.xz * 0.7) * an.y + PD.veins(p.xy * 0.7) * an.z;
// worn field-grey paint, rust bleeding through, the Purple Death taking it in plum-black spots and fine vessels
float3 col = float3(0.009, 0.0105, 0.0085) * (0.8 + 0.4 * PD.fbm(p.xy * 9.0));
col = lerp(col, float3(0.045, 0.016, 0.005), smoothstep(0.45, 0.62, rust));
float mould = smoothstep(0.5, 0.6, spot);
col = lerp(col, float3(0.014, 0.0015, 0.02), mould);
col = lerp(col, float3(0.026, 0.003, 0.036), v * 0.7 * smoothstep(0.45, 0.6, spot));
"""


def build_steel_material():
    """M_Intro_MouldSteel on the trench props' Steel slot (helmets, mess tins): worn field-grey with rust and the
    Purple Death spotting it; no specular and fully rough: under the trench's raking sun and fixed exposure any sheen
    washed the dark steel out to pale clay (a red test colour came back pink-white at every edge)."""
    material = mats._new_material("M_Intro_MouldSteel", shading=unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1400, -600)
    normal = mel.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1400, -400)
    surface = mats._custom(material, STEEL + "return float4(col, 1.0);", [("WP", world, ""), ("N", normal, "")], y=0)
    mel.connect_material_property(mats._mask(material, surface, "rgb"), "", unreal.MaterialProperty.MP_BASE_COLOR)
    specular = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 300)
    specular.set_editor_property("r", 0.0)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    material.set_editor_property("fully_rough", True)
    mats._finish(material)
    mesh = unreal.load_asset(lib.ROOT + "/Sets/SM_Intro_TrenchProps")
    for index, slot in enumerate(mesh.static_materials):
        if str(slot.material_slot_name) == "Steel":
            mesh.set_material(index, material)
    lib.safe_save([mesh.get_outermost()])


def build_encrusted():
    """MI_Intro_PD_Encrusted: the Purple Death already caked on the trench's steel and the dead (pustule mode, always
    shown). Bare, they read as grey clay heads next to the mould under this set's fixed exposure."""
    parent = unreal.load_asset(M + "M_Intro_PurpleDeath")
    mi = mats.instance("MI_Intro_PD_Encrusted", parent, scalars={"Mode": 2.0, "Seed": 1.3, "StageLo": 0.0, "StageHi": 9.0})
    mesh = unreal.load_asset(lib.ROOT + "/Sets/SM_Intro_TrenchProps")
    for index, slot in enumerate(mesh.static_materials):
        if str(slot.material_slot_name) == "Steel":
            mesh.set_material(index, mi)
    lib.safe_save([mesh.get_outermost()])
    return mi


def build_instances(meshes):
    """One instance per growth stage of each trench mould mesh (SM_<piece>_S<n>): Mode, Seed and its Mould window.
    Returns {mesh: MI path prefix} (a stage's instance is the prefix + "_S<n>")."""
    parent = unreal.load_asset(M + "M_Intro_PurpleDeath")
    out = {}
    for index, name in enumerate(meshes):
        mode = 1.0 if "Veins" in name else (2.0 if "Pustules" in name else 0.0)
        prefix = "MI_Intro_PD_" + name.replace("SM_Intro_Mould_", "")
        for stage, lo in enumerate(STAGE_LO):
            hi = STAGE_LO[stage + 1] if stage + 1 < len(STAGE_LO) else 9.0
            mats.instance("%s_S%d" % (prefix, stage), parent,
                          scalars={"Mode": mode, "Seed": index * 0.37, "StageLo": lo, "StageHi": hi})
        out[name] = M + prefix
    return out


# Shot 5.1 (2026-11-21): the aerosol that pours out of the opened canisters. A sphere with a soft limb and billowing
# noise (camera-independent, so no billboard keys are needed); it thins out as it grows (ObjectRadius against
# RefRadius), so a puff keyed from small to large reads as gas spreading into cloud.
GAS_CODE = mats.VNOISE + """
float3 p = (WorldPos - ObjPos) / max(Radius, 1.0);
float limb = saturate(dot(normalize(Normal), normalize(CamVec)));
float t = Time * Drift + Seed;
float d = 0.0; float amp = 0.5;
float2 q1 = p.xy * 2.4 + float2(t, Seed * 1.7); float2 q2 = p.yz * 2.4 + float2(Seed * 0.9, -t);
for (int i = 0; i < 5; i++) { d += amp * (vnoise(q1) * 0.5 + vnoise(q2) * 0.5); q1 = q1 * 2.03 + 7.0; q2 = q2 * 2.03 + 3.0; amp *= 0.5; }
float body = smoothstep(0.05, 0.75, limb);
float thin = saturate(pow(RefRadius / max(Radius, 1.0), 0.7));
// billows with gaps between them, dense and lit at their cores, dark and torn at their edges
float alpha = saturate(body * smoothstep(0.40, 0.74, d + limb * 0.14)) * Density * thin;
float3 col = lerp(float3(0.025, 0.006, 0.04), float3(0.48, 0.09, 0.80), saturate(d * 2.2 - 0.85));
col *= Glow * (0.35 + 1.1 * limb * limb * d);
return float4(col, alpha);
"""


def build_gas_material():
    material = mats._new_material("M_Intro_PDGas", blend=unreal.BlendMode.BLEND_TRANSLUCENT,
                                  shading=unreal.MaterialShadingModel.MSM_UNLIT)
    inputs = [
        ("WorldPos", mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -900, -400), ""),
        ("ObjPos", mel.create_material_expression(material, unreal.MaterialExpressionObjectPositionWS, -900, -320), ""),
        ("Radius", mel.create_material_expression(material, unreal.MaterialExpressionObjectRadius, -900, -240), ""),
        ("CamVec", mel.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -900, -160), ""),
        ("Normal", mel.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -900, -80), ""),
        ("Time", mel.create_material_expression(material, unreal.MaterialExpressionTime, -900, 0), ""),
        ("Seed", mats._scalar(material, "Seed", 0.0, 100), ""),
        ("Drift", mats._scalar(material, "Drift", 0.12, 200), ""),
        ("Density", mats._scalar(material, "Density", 0.75, 300), ""),
        ("Glow", mats._scalar(material, "Glow", 1.35, 400), ""),
        ("RefRadius", mats._scalar(material, "RefRadius", 420.0, 500), "")]
    custom = mats._custom(material, GAS_CODE, inputs)
    mel.connect_material_property(mats._mask(material, custom, "rgb"), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(mats._mask(material, custom, "a"), "", unreal.MaterialProperty.MP_OPACITY)
    mats._finish(material)
    for index in range(4):
        mats.instance("MI_Intro_PDGas_%d" % index, material, scalars={"Seed": 3.1 + index * 7.3})
