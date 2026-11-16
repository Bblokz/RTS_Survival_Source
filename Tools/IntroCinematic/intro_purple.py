"""The Purple Death in the 1945 trench (shots 5.2 / 5.3): M_Intro_PurpleDeath on the mould mats, pustules and veins.

The old look was smooth pink plastic that grew by scaling the meshes. Now:
- necrotic plum-black flesh, bruised violet mottling and dry mauve spore crust, with a branching vein network drawn
  over it (ridged multifractal vessels, projected on the surface from three sides); thin sharp glowing vessel lines
  beat slowly like a pulse;
- the vein meshes (Mode 1) are dark wet tubes, plum-black at the root, with a thin glowing core (where the tube
  faces the camera) along which light pumps outward from the root;
- the growth is revealed by the MPC scalar Mould (0..1): the mats and pustules spread out from their pivot with a
  ragged front (Reach = cm at Mould 1), the veins crawl outward along their length (U = metres from the root,
  MaxLen = the longest vein). The advancing front is fresher: wetter and brighter.
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

CORE = PD_LIB + """
float3 an = pow(abs(N), 4.0); an /= (an.x + an.y + an.z);
float3 p = WP / 14.0 + Seed * 3.0;
float v = PD.veins(p.yz) * an.x + PD.veins(p.xz) * an.y + PD.veins(p.xy) * an.z;
float m1 = PD.fbm(p.yz * 1.3) * an.x + PD.fbm(p.xz * 1.3) * an.y + PD.fbm(p.xy * 1.3) * an.z;
float mf = PD.fbm(p.yz * 7.0) * an.x + PD.fbm(p.xz * 7.0) * an.y + PD.fbm(p.xy * 7.0) * an.z;
// Growth: from the pivot (mats, pustules) or along the vein from its root (veins); ragged front.
float vein = step(0.5, Mode);
float d = lerp(length(LP), UV.x * 100.0, vein);
float reach = lerp(Reach, MaxLen * 100.0, vein);
// ragged, but not so ragged that islands of growth appear far ahead of the body (they read as floating petals)
float front = Mould * reach + (m1 - 0.5) * lerp(0.16, 0.08, vein) * reach - d;
float shown = step(0.0, front) * step(0.004, Mould);
float fresh = 1.0 - smoothstep(0.0, lerp(0.16, 0.05, vein) * reach, front);
float beat = 0.5 + 0.5 * sin(Time * 1.6 + m1 * 7.0);
float wave = frac(UV.x * 1.1 - Time * 0.45);
float pump = smoothstep(0.55, 0.9, wave) * smoothstep(1.0, 0.92, wave) * vein;
// Colour: necrotic plum-black, bruised violet, dry mauve spore crust; dark livid veins. Only the thick veins glow,
// faintly, beating; light pumps along the vein meshes. Scary is dark and wet, not neon.
float vb = PD.big(p.yz) * an.x + PD.big(p.xz) * an.y + PD.big(p.xy) * an.z;
vb = smoothstep(0.6, 1.0, vb); vb *= vb;                    // thin, sharp vessel lines, not soft smears
float3 col = lerp(float3(0.004, 0.001, 0.005), float3(0.02, 0.0025, 0.028), smoothstep(0.4, 0.8, m1));
col = lerp(col, float3(0.045, 0.035, 0.046), smoothstep(0.64, 0.8, mf) * 0.45 * (1.0 - vein));
col = lerp(col, float3(0.024, 0.002, 0.034), saturate(v * 0.6) * (1.0 - vein));
// The vein tubes are dark wet vessels, plum-black at the root and a little more livid toward the tips, with a
// thin glowing core: lit where the tube faces the camera, so it reads round and translucent rather than a flat
// pink ribbon (albedo stays ~0.02: this set's fixed exposure turns anything brighter into neon).
float facing = saturate(dot(N, V));
float core = pow(facing, 7.0);
float along = smoothstep(0.0, 1.6, UV.x * 100.0 / 100.0);
float3 tube = lerp(float3(0.008, 0.001, 0.011), float3(0.026, 0.003, 0.036), along) * lerp(0.45, 1.0, facing);
col = lerp(col, tube, vein);
// wet and glistening in the veins and at the fresh front; the flesh between is dull and spongy
float rough = saturate(lerp(0.82, 0.3, saturate(v * 0.8 + vein)) - fresh * 0.15 + (mf - 0.5) * 0.2 * (1.0 - vein));
float spec = lerp(0.18, 0.55, vein);
float3 glow = (vb * (0.1 + 0.9 * beat) * 0.035 * (1.0 - vein) + vein * core * (0.05 + pump * 0.22))
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
    inputs = [("WP", world, ""), ("LP", local, ""), ("N", normal, ""), ("UV", coords, ""), ("Time", time, ""),
              ("V", view, ""),
              ("Mould", mats._collection(material, "Mould", 0), ""), ("Mode", mats._scalar(material, "Mode", 0.0, 100), ""),
              ("Reach", mats._scalar(material, "Reach", 150.0, 200), ""),
              ("MaxLen", mats._scalar(material, "MaxLen", 3.8, 300), ""), ("Seed", mats._scalar(material, "Seed", 0.0, 400), ""),
              ("Glow", mats._scalar(material, "Glow", 1.0, 500), "")]
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


def build_instances(meshes):
    """One instance per trench mould mesh: Reach from the mesh bounds around its pivot. Returns {mesh: MI path}."""
    parent = unreal.load_asset(M + "M_Intro_PurpleDeath")
    out = {}
    for index, name in enumerate(meshes):
        mesh = unreal.load_asset(lib.ROOT + "/Sets/" + name)
        bounds = mesh.get_bounds()
        o, e = bounds.origin, bounds.box_extent
        reach = max(((o.x + sx * e.x) ** 2 + (o.y + sy * e.y) ** 2) ** 0.5 for sx in (-1, 1) for sy in (-1, 1))
        veins = "Veins" in name
        scalars = {"Mode": 1.0 if veins else 0.0, "Reach": reach * 0.95, "Seed": index * 0.37}
        if veins:
            scalars["MaxLen"] = 3.8
        mats.instance("MI_Intro_PD_" + name.replace("SM_Intro_Mould_", ""), parent, scalars=scalars)
        out[name] = M + "MI_Intro_PD_" + name.replace("SM_Intro_Mould_", "")
        print("purple death", name, "reach %.0f cm" % reach)
    return out
