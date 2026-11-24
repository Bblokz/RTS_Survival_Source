"""Build / update the Frozen Lake ice materials (Unreal editor python, throwaway editor only).

Everything lives under /Game/RTS_Survival/Blueprints/Environment/FrozenLake/Ice:
  Textures/T_FrozenLakeIce_Noise          packed tiling noise (R clouds, G crack distance, B roots, A bubbles), BC7
  Textures/T_FrozenLakeIce_StateNeutral   4x4 default for the StateMask parameter (medium, nothing else)
  M_FrozenLakeIce                         opaque master, one Custom node "FrozenLakeIce"
  MI_FrozenLakeIce_Heavy / _Medium / _Infantry / _Infantry_Vein   one type per plane (opaque)
  MI_FrozenLakeIce_Lake                   one lake mesh: UseStateMask on, blend mode overridden to Masked (holes)
  M_FrozenLakeWater + MI_FrozenLakeWater  translucent open water under the ice, seen through the Masked holes
  Test/...                                test lake MI, test state mask, black-ice bracket MIs (test only)

SAFE TO RE-RUN, NEVER DESTRUCTIVE: since 2026-11-17 the map (Ger_FrozenLake, MI_FL_IceLake) references the master.
  - An existing master is updated IN PLACE: the Custom node's code and parameter defaults change, new parameter nodes
    are added, no node is ever deleted (delete_all on a used master killed the editor: ue-python-material-gotchas).
    Before it changes anything it checks the graph (every Custom input connected); on any problem it raises and saves
    nothing.
  - The noise and neutral textures are only imported when missing (the map uses them); only the test mask re-imports.
  - Still: run it only in a throwaway editor, never while an editor has Ger_FrozenLake open.
The map-facing contract never changes: UseStateMask, StateMask, LakeOrigin, LakeSize and the RGBA channel meaning.
Used by ice_headless_test.py. Writes Saved/FrozenLake/ice/build_report.json.
"""
import json
import os

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLake/Ice'
HOME = '/Game/RTS_Survival/Blueprints/Environment/FrozenLake/Ice'
TEX = HOME + '/Textures'
TEST = HOME + '/Test'
MASTER = HOME + '/M_FrozenLakeIce'
WATER = HOME + '/M_FrozenLakeWater'
REPORT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/FrozenLake/ice/'
TEST_LAKE_SIZE_CM = 58600.0
GROUP_STATE = '1 State'
GROUP_LOOK = '2 Look'
GROUP_BLACK = '2b Look black ice'
GROUP_TILING = '3 Tiling'
GROUP_VEIN = '4 Vein (black ice)'
GROUP_WARN = '5 Warn (cryo failing)'
GROUP_WATER = '6 Open water'
GROUP_TRACKS = '7 Vehicle tracks'
FIXED_INPUTS = ('WP', 'V', 'Time', 'MaskIn', 'N')
# vehicle tracks: UTankTrailSubsystem's landscape-wide R8 render target, world-mapped through MPC_TankTrails
TRAILS_RT = '/Game/RTS_Survival/Blueprints/Environment/TankTrails/RT_TankTrails'
TRAILS_MPC = '/Game/RTS_Survival/Blueprints/Environment/TankTrails/MPC_TankTrails'

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
failures = []
report = {'failures': failures}

# Radixite colour = the crystals' own glow: MM_Crystal_Emissive's DiffuseMap is Cristal_3A_Yellow_Cristal_03_Emissive,
# average sRGB (0.87, 0.66, 0.0) = linear (0.76, 0.41, 0.0); normalised here (VeinEmissive sets the strength).
# (The minimap draws radixite green, 0.17/0.98/0.12; the world crystals are amber with red-orange edges.)
VEIN_COLOR = (1.0, 0.54, 0.0)

# Parameters renamed in round 2 (old name -> new name). Nothing overrode the old names.
RENAMES = {'TransitionNoise': 'BorderWobble', 'BandSoftness': 'BorderWidth'}

# (name, default, group) - order is the Custom node input order
SCALARS = [
    ('BaseThickness', 0.5, GROUP_STATE), ('WarnGlow', 0.0, GROUP_STATE), ('OpenWater', 0.0, GROUP_STATE),
    ('VeinAmount', 0.0, GROUP_STATE),
    ('MacroScale', 17300.0, GROUP_TILING), ('MidScale', 3700.0, GROUP_TILING), ('FineScale', 640.0, GROUP_TILING),
    ('DriftLength', 5300.0, GROUP_TILING), ('DriftWidth', 3000.0, GROUP_TILING), ('WindAngle', 35.0, GROUP_TILING),
    ('BorderWobble', 70.0, GROUP_LOOK), ('BorderWidth', 250.0, GROUP_LOOK), ('RimWidth', 70.0, GROUP_LOOK),
    ('RimStrength', 0.8, GROUP_LOOK), ('MacroVariation', 0.12, GROUP_LOOK),
    ('SnowRoughness', 0.85, GROUP_LOOK), ('GreyRoughness', 0.42, GROUP_LOOK), ('BlackRoughness', 0.34, GROUP_BLACK),
    ('CrackWidth', 0.09, GROUP_LOOK), ('CrackStrengthGrey', 0.55, GROUP_LOOK), ('CrackStrengthBlack', 0.45, GROUP_BLACK),
    ('BubbleDepth', 35.0, GROUP_BLACK), ('BubbleAmount', 0.5, GROUP_BLACK),
    ('RootDepth', 300.0, GROUP_VEIN), ('RootScale', 5300.0, GROUP_VEIN), ('RootSharpness', 9.0, GROUP_VEIN),
    ('VeinEmissive', 2.0, GROUP_VEIN), ('VeinHalo', 0.25, GROUP_VEIN), ('VeinPulseSpeed', 0.25, GROUP_VEIN),
    ('WarnEmissive', 1.6, GROUP_WARN), ('WarnPulseSpeed', 0.8, GROUP_WARN),
    ('WaterRoughness', 0.06, GROUP_WATER), ('HoleEdgeNoise', 0.3, GROUP_WATER),
    # round 2
    ('DustAmount', 0.2, GROUP_LOOK),
    ('SkyReflect', 0.35, GROUP_BLACK), ('FrostAmount', 0.22, GROUP_BLACK), ('ScratchStrength', 0.45, GROUP_BLACK),
    ('ScratchLength', 1100.0, GROUP_TILING), ('ScratchWidth', 140.0, GROUP_TILING),
    ('WarnRimWidth', 80.0, GROUP_WARN), ('WarnWetness', 0.6, GROUP_WARN),
    # round 4: vehicle tracks (heavy values match the landscape snow ruts, ls_frozenlake_snowruts.py)
    ('TrackStrength', 1.0, GROUP_TRACKS), ('TrackRutLow', 0.05, GROUP_TRACKS), ('TrackRutHigh', 0.35, GROUP_TRACKS),
    ('TrackReachTexels', 2.5, GROUP_TRACKS), ('TrackNormalHeavy', 5.5, GROUP_TRACKS),
    ('TrackBermHeavy', 0.3, GROUP_TRACKS), ('TrackExposeHeavy', 0.6, GROUP_TRACKS),
    ('TrackScuffMedium', 0.45, GROUP_TRACKS), ('TrackNormalMedium', 1.0, GROUP_TRACKS),
    ('TrackScratchBlack', 0.55, GROUP_TRACKS), ('TrackNormalBlack', 0.3, GROUP_TRACKS),
]
VECTORS = [
    ('SnowColor', (0.9, 0.93, 0.97), GROUP_LOOK), ('ScouredIceColor', (0.68, 0.76, 0.85), GROUP_LOOK),
    ('GreyIceColor', (0.24, 0.28, 0.31), GROUP_LOOK), ('GreySnowColor', (0.52, 0.55, 0.58), GROUP_LOOK),
    ('BlackIceColor', (0.045, 0.058, 0.07), GROUP_BLACK), ('CrackColor', (0.75, 0.82, 0.88), GROUP_LOOK),
    ('VeinColor', VEIN_COLOR, GROUP_VEIN), ('WarnColor', (0.3, 0.82, 1.0), GROUP_WARN),
    ('WaterColor', (0.012, 0.026, 0.034), GROUP_WATER), ('SlushColor', (0.6, 0.65, 0.68), GROUP_WATER),
    # round 2
    ('SkyReflectColor', (0.15, 0.19, 0.24), GROUP_BLACK), ('FrostColor', (0.5, 0.56, 0.62), GROUP_BLACK),
    ('RidgeColor', (0.93, 0.96, 1.0), GROUP_LOOK), ('SeamColor', (0.035, 0.045, 0.05), GROUP_LOOK),
    ('WetCrackColor', (0.012, 0.018, 0.022), GROUP_WARN),
    ('TrackSnowTint', (0.30, 0.34, 0.44), GROUP_TRACKS), ('TrackScuffColor', (0.6, 0.64, 0.68), GROUP_TRACKS),
]

HLSL = r"""
// Frozen Lake ice. One continuous thickness T (1 heavy/white, 0.5 medium/grey, 0 infantry/black) drives the look.
// State: from the world-mapped StateMask when UseStateMask is on (MaskIn.x >= 0), else from the scalar parameters.
float2 p = WP.xy;
float3 vdir = normalize(V);
bool useMask = MaskIn.x >= 0.0;
float T = useMask ? MaskIn.x : BaseThickness;
float warn = max(useMask ? MaskIn.y : 0.0, WarnGlow);
float open = max(useMask ? MaskIn.z : 0.0, OpenWater);
float vein = useMask ? MaskIn.w : VeinAmount;

// ---- world-space noise at non-integer scale ratios, rotated, so no repeat lines up over a 400 m lake ----
const float2x2 R1 = float2x2(0.8, -0.6, 0.6, 0.8);
const float2x2 R2 = float2x2(0.28, 0.96, -0.96, 0.28);
float4 nMac = Texture2DSample(N, NSampler, mul(R1, p) / MacroScale + float2(0.13, 0.71));
float4 nMid = Texture2DSample(N, NSampler, p / MidScale + float2(0.41, 0.07));
float4 nFin = Texture2DSample(N, NSampler, mul(R2, p) / FineScale);
float wa = radians(WindAngle);
float2 wdir = float2(cos(wa), sin(wa));
float2 pd = float2(dot(p, wdir) / DriftLength, dot(p, float2(-wdir.y, wdir.x)) / DriftWidth);
float4 nDr = Texture2DSample(N, NSampler, pd + float2(0.57, 0.29));
float macro = lerp(1.0 - MacroVariation, 1.0 + MacroVariation, nMac.r);
float2 par = -vdir.xy / max(vdir.z, 0.25);
float4 nSh = Texture2DSample(N, NSampler, (p + par * BubbleDepth) / (FineScale * 0.7) + float2(0.31, 0.83));

// ---- world distance (cm) to the type borders, from the mask's own gradient: the border width is BorderWidth
//      whatever the mask's blur, and the 0.75 / 0.25 crossings of the mask (the gameplay truth) stay the centre ----
float3 dpx = ddx(WP);
float3 dpy = ddy(WP);
float px = max(max(length(dpx), length(dpy)), 0.01);
float det = dpx.x * dpy.y - dpx.y * dpy.x;
det = det >= 0.0 ? max(det, 1e-4) : min(det, -1e-4);
float dTx = ddx(T);
float dTy = ddy(T);
float2 gT = float2(dTx * dpy.y - dTy * dpx.y, dTy * dpx.x - dTx * dpy.x) / det;
float invT = 1.0 / max(length(gT), 1e-6);
float wob = (nMid.r - 0.5) * BorderWobble + (nFin.r - 0.5) * BorderWobble * 0.4;
float dH = (T - 0.75) * invT + wob;   // > 0 on the heavy side
float dB = (T - 0.25) * invT + wob;   // > 0 on the medium-or-heavy side
float hw = max(0.5 * BorderWidth, 1.0);
float wHeavy = smoothstep(-hw, hw, dH);
float wMed = smoothstep(-hw, hw, dB);

// ---- cracks (G = distance to crack, 0 on the crack) ----
float crackF = 1.0 - smoothstep(CrackWidth * 0.5, CrackWidth, nFin.g);
float crackM = 1.0 - smoothstep(CrackWidth * 0.6, CrackWidth * 1.2, nMid.g);

// ---- heavy: wind-drifted snow, scoured pale-blue ice between the drifts ----
// big soft drifts led by the macro noise; the wind sample only shapes them (alone it reads as diagonal flecks)
float drift = smoothstep(0.25, 0.55, nDr.r * 0.3 + nMac.r * 0.45 + nMid.r * 0.25);
float3 scoured = ScouredIceColor * lerp(0.92, 1.06, nFin.r);
scoured = lerp(scoured, CrackColor, crackM * 0.6);
float3 snow = SnowColor * (0.97 + nFin.a * 0.05);
float3 heavyC = lerp(scoured, snow, drift) * lerp(1.0, macro, 0.5);
float heavyR = lerp(0.5, SnowRoughness, drift);

// ---- medium: grey ice, white cracks, soft unstretched snow dust (no wind streaks: they read as grain from above) ----
float3 medC = GreyIceColor * lerp(0.85, 1.15, nMid.r) * macro;
medC = lerp(medC, CrackColor, max(crackM, crackF * 0.3) * CrackStrengthGrey);
float dust = smoothstep(0.62, 0.88, nMid.r * 0.55 + nMac.r * 0.45) * DustAmount;
medC = lerp(medC, GreySnowColor, dust);
float medR = lerp(GreyRoughness, 0.8, dust);

// ---- black: clear dark blue-grey ice with depth, sky sheen, frost and scratches (units must stay readable) ----
float2 rootUV = mul(R1, p + par * RootDepth) / RootScale;
float4 nRt = Texture2DSample(N, NSampler, rootUV);
float4 nRtB = Texture2DSampleBias(N, NSampler, rootUV + float2(0.003, 0.002), 3.5);
float bub = smoothstep(0.35, 0.8, nSh.a) * BubbleAmount;
float crackS = 1.0 - smoothstep(CrackWidth * 0.4, CrackWidth * 0.8, nSh.g);
float crackBig = 1.0 - smoothstep(CrackWidth * 0.25, CrackWidth * 0.5, nMac.g);
float blackCr = max(max(crackBig, crackM * 0.3), crackS * 0.15);
float3 blackC = BlackIceColor * lerp(0.85, 1.2, nMac.r);
float sky = SkyReflect * (0.5 + 0.5 * (nMid.r * 0.6 + nMac.r * 0.4)) * (0.8 + 0.4 * (1.0 - vdir.z));
blackC = lerp(blackC, SkyReflectColor, saturate(sky));
blackC = lerp(blackC, CrackColor * 0.5, bub * 0.5);
float frost = smoothstep(0.6, 0.9, nDr.r) * smoothstep(0.4, 0.7, nMac.r) * FrostAmount;
float sa = radians(WindAngle + 70.0);
float2 sdir = float2(cos(sa), sin(sa));
float2 ps = float2(dot(p, sdir) / ScratchLength, dot(p, float2(-sdir.y, sdir.x)) / ScratchWidth);
float4 nSc = Texture2DSample(N, NSampler, ps + float2(0.23, 0.61));
float scratch = (1.0 - smoothstep(0.08, 0.2, nSc.g)) * smoothstep(0.35, 0.65, nMid.r) * ScratchStrength;
blackC = lerp(blackC, FrostColor, saturate(frost + scratch));
blackC = lerp(blackC, CrackColor * 0.85, blackCr * CrackStrengthBlack);
float blackR = lerp(BlackRoughness, 0.7, saturate(blackCr * 0.7 + bub * 0.3 + frost + scratch * 0.6));

// ---- radixite roots below the ice ----
// roots come and go in patches (second, larger root sample gates the first) so the network never reads as a grid
float4 nRtG = Texture2DSample(N, NSampler, mul(R2, p) / (RootScale * 3.7) + float2(0.71, 0.19));
float gate = smoothstep(0.25, 0.75, nRtG.r);
float roots = pow(saturate(nRt.b), RootSharpness) * gate;
float halo = nRtB.b * nRtB.b * nRtB.b * gate;
float pulse = 0.6 + 0.4 * sin(Time * VeinPulseSpeed * 6.2832 + nRt.r * 6.2832);
blackC += VeinColor * halo * 0.05 * vein;

// ---- vehicle tracks: UTankTrailSubsystem's RT_TankTrails (R = rut coverage), mapped by MPC_TankTrails. Off in the
//      editor (TrailEnabled 0) and outside the RT: no fetch. Shared clamped sampler, so no sampler slot. ----
float tC = 0.0;
float2 tG = float2(0.0, 0.0);
float tBerm = 0.0;
[branch] if (TrailOn > 0.5 && TrackStrength > 0.0)
{
    float2 tuv = (WP.xy - TrailMin.xy) * TrailInv.xy;
    if (all(tuv > 0.0) && all(tuv < 1.0))
    {
        float2 ts = TrailInv.zw * TrackReachTexels;
        tC = TrailRT.SampleLevel(View.SharedBilinearClampedSampler, tuv, 0).r;
        float tPX = TrailRT.SampleLevel(View.SharedBilinearClampedSampler, tuv + float2(ts.x, 0.0), 0).r;
        float tNX = TrailRT.SampleLevel(View.SharedBilinearClampedSampler, tuv - float2(ts.x, 0.0), 0).r;
        float tPY = TrailRT.SampleLevel(View.SharedBilinearClampedSampler, tuv + float2(0.0, ts.y), 0).r;
        float tNY = TrailRT.SampleLevel(View.SharedBilinearClampedSampler, tuv - float2(0.0, ts.y), 0).r;
        float tRing = max(max(tPX, tNX), max(tPY, tNY));
        // the RT is binary per 16 cm texel: blend the centre with its neighbours so rut edges don't staircase
        float tSoft = (tC * 2.0 + tPX + tNX + tPY + tNY) / 6.0;
        tBerm = saturate((tRing - tSoft) * 3.0) * (1.0 - smoothstep(TrackRutLow, TrackRutHigh, tSoft));
        tG = float2(tPX - tNX, tPY - tNY) * 0.5;
        tC = tSoft;
    }
}
tC *= TrackStrength;
tBerm *= TrackStrength;
tG *= TrackStrength;
// heavy (snow over ice): a compacted, shadowed trough like the shore snow ruts, scoured pale ice at the deepest part
float rutH = smoothstep(TrackRutLow, TrackRutHigh, tC);
float deepH = smoothstep(0.75, 1.0, rutH) * TrackExposeHeavy;
heavyC = lerp(heavyC, heavyC * TrackSnowTint, rutH);
heavyC = lerp(heavyC, ScouredIceColor * 0.85, deepH);
heavyC = lerp(heavyC, float3(1.0, 1.0, 1.0), tBerm * TrackBermHeavy);
heavyR = lerp(lerp(lerp(heavyR, 0.55, rutH), 0.3, deepH), 0.95, tBerm);
// medium (grey ice): whitish abrasion scuffs along the track, fine crack lines along the rut edges
float tw = max(fwidth(tC), 1e-3);
float scuff = smoothstep(0.15, 0.6, tC) * (0.6 + 0.4 * nFin.r);
float edgeCr = (1.0 - saturate(abs(tC - 0.2) / (tw * 1.5))) * smoothstep(0.35, 0.6, nSh.r);
medC = lerp(medC, TrackScuffColor, scuff * TrackScuffMedium);
medC = lerp(medC, CrackColor, edgeCr * 0.7);
medR = lerp(medR, 0.6, scuff * 0.6);
// black (clear ice): faint white scratch lines along the track and a little frost powder; roots stay visible
float scr = max(1.0 - saturate(abs(tC - 0.3) / (tw * 1.2)), (1.0 - saturate(abs(tC - 0.7) / (tw * 1.2))) * 0.7);
scr *= smoothstep(0.3, 0.6, nFin.r * 0.5 + nSh.g * 0.5);
float powder = smoothstep(0.4, 0.95, tC) * 0.25;
blackC = lerp(blackC, FrostColor, saturate(powder + scr * TrackScratchBlack));
blackR = lerp(blackR, 0.6, saturate(powder + scr * 0.5));

// ---- combine the three looks ----
float3 col = lerp(blackC, medC, wMed);
col = lerp(col, heavyC, wHeavy);
float rough = lerp(lerp(blackR, medR, wMed), heavyR, wHeavy);

// ---- border lines: heavy edge = white pressure ridge of broken blocks just inside + dark wet seam just outside;
//      black edge = pale shear crack on the medium side + dark seam on the black side ----
float rj = (nSh.r - 0.5) * RimWidth * 0.6;
float rh = RimWidth * 0.5;
float lineFade = saturate(RimWidth / (2.0 * px)) * RimStrength;
float ridge = 1.0 - smoothstep(rh - px, rh + px, abs(dH - rh * 1.2 + rj));
float seamH = 1.0 - smoothstep(rh * 0.5 - px, rh * 0.5 + px, abs(dH + rh * 0.6 + rj * 0.5));
float shear = 1.0 - smoothstep(rh * 0.3 - px, rh * 0.3 + px, abs(dB - rh * 0.4 + rj * 0.5));
float seamB = 1.0 - smoothstep(rh * 0.5 - px, rh * 0.5 + px, abs(dB + rh * 0.6 + rj * 0.5));
float blocks = smoothstep(0.3, 0.6, nFin.a * 0.5 + nSh.b * 0.5);
float seam = max(seamH, seamB) * lineFade;
col = lerp(col, SeamColor, seam);
rough = lerp(rough, 0.15, seam);
float rdg = ridge * lerp(0.6, 1.0, blocks) * lineFade;
col = lerp(col, RidgeColor, rdg);
rough = lerp(rough, 0.85, rdg);
col = lerp(col, CrackColor, shear * lineFade * 0.9);

// ---- warn: the cryo is failing. Cracks widen and go dark and wet, the surface gets wet, a pale cyan rim pulses at
//      the edge of the area (warn = 0.3), cyan light leaks along the cracks. Leaves the centre calm for steam VFX. ----
float dWx = ddx(warn);
float dWy = ddy(warn);
float2 gW = float2(dWx * dpy.y - dWy * dpx.y, dWy * dpx.x - dWx * dpy.x) / det;
float dW = (warn - 0.3) / max(length(gW), 1e-6) + (nFin.r - 0.5) * WarnRimWidth;
float wpulse = 0.5 + 0.5 * sin(Time * WarnPulseSpeed * 6.2832);
float wr = 0.5 * WarnRimWidth;
float warnRim = (1.0 - smoothstep(wr - px, wr + px, abs(dW - wr))) * saturate(WarnRimWidth / (2.0 * px));
float wk = 1.0 + 1.5 * warn;
float wetM = 1.0 - smoothstep(CrackWidth * 0.5 * wk, CrackWidth * 1.0 * wk, nMid.g);
float wetF = 1.0 - smoothstep(CrackWidth * 0.4 * wk, CrackWidth * 0.8 * wk, nFin.g);
float wetCr = max(max(wetM, wetF * 0.7), blackCr * (1.0 - wMed)) * smoothstep(0.05, 0.4, warn);
float haloCr = saturate((1.0 - smoothstep(CrackWidth * 1.0 * wk, CrackWidth * 2.0 * wk, nMid.g)) - wetM);
float wet = saturate(warn) * WarnWetness;
col *= lerp(1.0, 0.62, wet * lerp(0.6, 1.0, wHeavy));
col = lerp(col, WetCrackColor, wetCr * 0.9);
rough = lerp(rough, 0.08, max(wet * 0.8, wetCr));
col = lerp(col, WarnColor * 0.5, warnRim * 0.25);
float3 warnE = WarnColor * WarnEmissive * (0.3 + 0.7 * wpulse) * (warnRim + haloCr * 0.4 * smoothstep(0.1, 0.6, warn));

// ---- open water (holes, melted zones): jagged edge, slush rim, floes near the edge (kept as ice when Masked) ----
float o = open + (nFin.r - 0.5) * HoleEdgeNoise;
float water = smoothstep(0.5, 0.56, o);
float slush = smoothstep(0.3, 0.5, o) * (1.0 - water);
float floe = smoothstep(0.6, 0.85, nFin.a) * smoothstep(0.9, 0.55, o) * water;
col = lerp(col, SlushColor, slush * 0.85);
col = lerp(col, WaterColor, water);
col = lerp(col, SlushColor * 0.9, floe);
rough = lerp(lerp(rough, 0.7, slush), WaterRoughness, water);
rough = lerp(rough, 0.6, floe);

// ---- emissive: roots under black ice / open water, cyan warn rim + crack light ----
float veinVis = vein * max((1.0 - wMed) * (1.0 - slush), water * 0.6);
float3 veinE = VeinColor * VeinEmissive * (roots * pulse + halo * VeinHalo * (0.7 + 0.3 * pulse)) * veinVis;

float tk = TrackNormalHeavy * wHeavy + TrackNormalMedium * (wMed - wHeavy) + TrackNormalBlack * (1.0 - wMed);
Normal = normalize(float3(tG * tk * (1.0 - water), 1.0));   // world space (TangentSpaceNormal off)
Emissive = veinE + warnE * (1.0 - water);
Roughness = rough;
Opacity = 1.0 - water + floe;
return col;
"""

WATER_FIXED = ('WP', 'V', 'Time', 'N', 'SD', 'PD')
WATER_SCALARS = [
    ('FadeDepth', 350.0, GROUP_WATER), ('ClearOpacity', 0.3, GROUP_WATER), ('DeepOpacity', 0.92, GROUP_WATER),
    ('WaterRoughness', 0.04, GROUP_WATER), ('CrumbScale', 700.0, GROUP_WATER), ('CrumbAmount', 0.6, GROUP_WATER),
    ('DriftSpeed', 6.0, GROUP_WATER), ('SkySheen', 0.35, GROUP_WATER),
]
WATER_VECTORS = [
    ('ShallowColor', (0.05, 0.1, 0.11), GROUP_WATER), ('DeepColor', (0.02, 0.042, 0.05), GROUP_WATER),
    ('CrumbColor', (0.55, 0.6, 0.63), GROUP_WATER), ('SkySheenColor', (0.2, 0.26, 0.3), GROUP_WATER),
]
WATER_HLSL = r"""
// Frozen Lake open water, a plane ~20 cm under the ice, seen only through the Masked holes of the lake ice.
// Depth fade from the scene behind (lakebed, sunk hulls): shallow = see-through, deep = dark.
float3 vdir = normalize(V);
float rayDepth = max(SD - PD, 0.0);
float vert = rayDepth * max(abs(vdir.z), 0.2);
float f = saturate(vert / FadeDepth);
float3 col = lerp(ShallowColor, DeepColor, f);
float op = lerp(ClearOpacity, DeepOpacity, f);
float2 drift = float2(Time * DriftSpeed, Time * DriftSpeed * 0.37);
float4 n1 = Texture2DSample(N, NSampler, (WP.xy + drift) / CrumbScale + float2(0.17, 0.53));
float4 n2 = Texture2DSample(N, NSampler, (WP.xy - drift * 0.6) / (CrumbScale * 3.1) + float2(0.61, 0.11));
// soft overcast-sky sheen so deep water reads as water, not as a black hole (stronger at grazing angles)
float sheen = SkySheen * (0.7 + 0.3 * n2.r) * (0.6 + 0.4 * (1.0 - abs(vdir.z)));
col = lerp(col, SkySheenColor, saturate(sheen));
// crumbs of broken ice drifting slowly on the surface
float crumb = smoothstep(0.75, 0.88, n1.a) * smoothstep(0.35, 0.65, n2.r) * CrumbAmount;
col = lerp(col, CrumbColor, crumb);
op = lerp(op, 1.0, crumb);
Roughness = lerp(WaterRoughness, 0.6, crumb);
Opacity = op;
return col;
"""


def log(msg):
    unreal.log('FROZENICE ' + str(msg))
    report.setdefault('log', []).append(str(msg))


def save(obj):
    if not EAL.save_loaded_asset(obj, False):
        failures.append('save ' + obj.get_path_name())


def N(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, getattr(unreal, 'MaterialExpression' + cls), x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def C(a, a_out, b, b_in):
    if not MEL.connect_material_expressions(a, a_out, b, b_in):
        failures.append('%s.%s -> %s.%s' % (a.get_name(), a_out, b.get_name(), b_in))


def import_texture(src_png, dest_path, name, compression, address_clamp=False, mips=True, only_if_missing=False):
    path = dest_path + '/' + name
    if only_if_missing and EAL.does_asset_exist(path):
        return unreal.load_asset(path)
    task = unreal.AssetImportTask()
    task.filename = src_png
    task.destination_path = dest_path
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    if tex is None:
        raise RuntimeError('texture import failed: ' + src_png)
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', compression)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_WORLD)
    if address_clamp:
        tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
        tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
    if not mips:
        tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    save(tex)
    return tex


def _custom_outputs(custom, names_kinds):
    outs = []
    for name, kind in names_kinds:
        co = unreal.CustomOutput()
        co.set_editor_property('output_name', name)
        co.set_editor_property('output_type', kind)
        outs.append(co)
    custom.set_editor_property('additional_outputs', outs)


def _set_custom_inputs(custom, spec):
    ins = []
    for name, _, _ in spec:
        ci = unreal.CustomInput()
        ci.set_editor_property('input_name', name)
        ins.append(ci)
    custom.set_editor_property('inputs', ins)
    for name, node, out in spec:
        C(node, out, custom, name)


def _param_nodes(mat, scalars, vectors, existing, x, y):
    """Reuse (and retune) existing parameter nodes by name, create the missing ones. Returns the Custom input spec."""
    inv = {new: old for old, new in RENAMES.items()}
    spec, created = [], []
    for name, value, group in scalars:
        node = existing.get(name) or existing.get(inv.get(name, ''))
        if node is None:
            node = N(mat, 'ScalarParameter', x, y, parameter_name=name, default_value=value, group=group)
            y += 70
            created.append(name)
        else:
            node.set_editor_property('parameter_name', name)
            node.set_editor_property('default_value', value)
            node.set_editor_property('group', group)
        spec.append((name, node, ''))
    for name, rgb, group in vectors:
        colour = unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)
        node = existing.get(name) or existing.get(inv.get(name, ''))
        if node is None:
            node = N(mat, 'VectorParameter', x, y, parameter_name=name, group=group, default_value=colour)
            y += 120
            created.append(name)
        else:
            node.set_editor_property('parameter_name', name)
            node.set_editor_property('default_value', colour)
            node.set_editor_property('group', group)
        spec.append((name, node, ''))
    return spec, created


def update_in_place(mat, code, fixed, scalars, vectors, extra=(), outputs=None, links=None):
    """Change a live master without deleting anything: new code, retuned/added parameters, reconnected inputs.
    Validates the graph first and raises (nothing changed, nothing saved) when it is not what this script built.
    extra: (input name, factory(mat) -> node) for non-parameter inputs, created only when the node is missing.
    outputs / links: the Custom node's additional outputs and (material property, output name) to (re)connect."""
    custom = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
    if custom is None or not isinstance(custom, unreal.MaterialExpressionCustom):
        raise RuntimeError('%s: BaseColor is not fed by a Custom node; refusing to touch it' % mat.get_name())
    names = [str(ci.get_editor_property('input_name')) for ci in custom.get_editor_property('inputs')]
    nodes = list(MEL.get_inputs_for_material_expression(mat, custom))
    if len(nodes) != len(names) or any(n is None for n in nodes):
        raise RuntimeError('%s: Custom inputs %d names / %d nodes, unconnected %s; refusing to touch it' % (
            mat.get_name(), len(names), len(nodes), [nm for nm, nd in zip(names, nodes) if nd is None]))
    existing = dict(zip(names, nodes))
    missing = [f for f in fixed if f not in existing]
    if missing:
        raise RuntimeError('%s: fixed inputs missing %s; refusing to touch it' % (mat.get_name(), missing))
    spec = [(f, existing[f], '') for f in fixed]
    created = []
    for name, factory in extra:
        node = existing.get(name)
        if node is None:
            node = factory(mat)
            created.append(name)
        spec.append((name, node, ''))
    pspec, created_p = _param_nodes(mat, scalars, vectors, existing, -1350, 6000)
    created += created_p
    spec += pspec
    used = set(nm for nm, _, _ in spec) | set(RENAMES)
    orphans = [nm for nm in names if nm not in used]
    before = len(failures)
    _set_custom_inputs(custom, spec)  # inputs first: setting the code triggers a compile, which needs them connected
    if len(failures) != before:
        raise RuntimeError('%s: reconnect failed %s; NOT saved' % (mat.get_name(), failures[before:]))
    if outputs is not None:
        _custom_outputs(custom, outputs)
        for prop, out in links or ():
            if not MEL.connect_material_property(custom, out, prop):
                failures.append('custom.%s -> %s' % (out, prop))
        if len(failures) != before:
            raise RuntimeError('%s: output links failed %s; NOT saved' % (mat.get_name(), failures[before:]))
    custom.set_editor_property('code', code)
    MEL.recompile_material(mat)
    save(mat)
    log('%s updated in place: %d inputs, created %s, orphaned (left alone) %s' % (
        mat.get_name(), len(spec), created, orphans))
    return mat


def _collection(name, y):
    def make(mat):
        e = N(mat, 'CollectionParameter', -2600, y, collection=unreal.load_asset(TRAILS_MPC))
        e.set_editor_property('parameter_name', name)
        return e
    return make


def _trails_rt(mat):
    return N(mat, 'TextureObject', -2600, 1000, texture=unreal.load_asset(TRAILS_RT),
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)


TRAIL_INPUTS = (('TrailMin', _collection('TrailBoundsMin', 700)), ('TrailInv', _collection('TrailBoundsInvSize', 800)),
                ('TrailOn', _collection('TrailEnabled', 900)), ('TrailRT', _trails_rt))
_O = unreal.CustomMaterialOutputType
ICE_OUTPUTS = (('Emissive', _O.CMOT_FLOAT3), ('Roughness', _O.CMOT_FLOAT1), ('Opacity', _O.CMOT_FLOAT1),
               ('Normal', _O.CMOT_FLOAT3))
_P = unreal.MaterialProperty
ICE_LINKS = ((_P.MP_BASE_COLOR, ''), (_P.MP_EMISSIVE_COLOR, 'Emissive'), (_P.MP_ROUGHNESS, 'Roughness'),
             (_P.MP_OPACITY_MASK, 'Opacity'), (_P.MP_NORMAL, 'Normal'))


def create_master(noise, neutral):
    """First build only (the master does not exist yet)."""
    mat = TOOLS.create_asset('M_FrozenLakeIce', HOME, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('two_sided', False)
    mat.set_editor_property('used_with_static_lighting', False)
    mat.set_editor_property('opacity_mask_clip_value', 0.5)

    T = unreal.MaterialSamplerType
    wp = N(mat, 'WorldPosition', -2600, -400)
    cam = N(mat, 'CameraVectorWS', -2600, -300)
    time = N(mat, 'Time', -2600, -200)
    noise_obj = N(mat, 'TextureObject', -2600, -100, texture=noise, sampler_type=T.SAMPLERTYPE_LINEAR_COLOR)

    # world-mapped state mask (later: the ice mechanic's render target)
    wxy = N(mat, 'ComponentMask', -2400, 100, r=True, g=True, b=False, a=False)
    C(wp, '', wxy, '')
    origin = N(mat, 'VectorParameter', -2600, 200, parameter_name='LakeOrigin', group=GROUP_STATE,
               default_value=unreal.LinearColor(0.0, 0.0, 0.0, 0.0))
    oxy = N(mat, 'ComponentMask', -2400, 200, r=True, g=True, b=False, a=False)
    C(origin, '', oxy, '')
    sub = N(mat, 'Subtract', -2250, 120)
    C(wxy, '', sub, 'A')
    C(oxy, '', sub, 'B')
    size = N(mat, 'ScalarParameter', -2600, 320, parameter_name='LakeSize', default_value=58600.0, group=GROUP_STATE)
    div = N(mat, 'Divide', -2100, 150)
    C(sub, '', div, 'A')
    C(size, '', div, 'B')
    mask = N(mat, 'TextureSampleParameter2D', -1950, 150, parameter_name='StateMask', texture=neutral,
             sampler_type=T.SAMPLERTYPE_LINEAR_COLOR, group=GROUP_STATE)
    C(div, '', mask, 'UVs')
    off = N(mat, 'Constant4Vector', -1950, 420, constant=unreal.LinearColor(-1.0, 0.0, 0.0, 0.0))
    switch = N(mat, 'StaticSwitchParameter', -1650, 200, parameter_name='UseStateMask', default_value=False,
               group=GROUP_STATE)
    C(mask, 'RGBA', switch, 'True')
    C(off, '', switch, 'False')

    custom = N(mat, 'Custom', -900, 0, code=HLSL, description='FrozenLakeIce',
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    spec = [('WP', wp, ''), ('V', cam, ''), ('Time', time, ''), ('MaskIn', switch, ''), ('N', noise_obj, '')]
    spec += [(name, factory(mat), '') for name, factory in TRAIL_INPUTS]
    pspec, _ = _param_nodes(mat, SCALARS, VECTORS, {}, -1350, -400)
    mat.set_editor_property('tangent_space_normal', False)
    mat.set_editor_property('used_with_nanite', True)
    _custom_outputs(custom, ICE_OUTPUTS)
    _set_custom_inputs(custom, spec + pspec)
    for prop, out in ICE_LINKS:
        if not MEL.connect_material_property(custom, out, prop):
            failures.append('custom.%s -> %s' % (out, prop))
    MEL.recompile_material(mat)
    save(mat)
    return mat


def build_water(noise):
    """Translucent open water under the ice. Created once; later runs update it in place like the ice master."""
    if EAL.does_asset_exist(WATER):
        return update_in_place(unreal.load_asset(WATER), WATER_HLSL, WATER_FIXED, WATER_SCALARS, WATER_VECTORS)
    mat = TOOLS.create_asset('M_FrozenLakeWater', HOME, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('translucency_lighting_mode',
                            unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    mat.set_editor_property('two_sided', False)
    mat.set_editor_property('used_with_static_lighting', False)
    wp = N(mat, 'WorldPosition', -1800, -300)
    cam = N(mat, 'CameraVectorWS', -1800, -200)
    time = N(mat, 'Time', -1800, -100)
    noise_obj = N(mat, 'TextureObject', -1800, 0, texture=noise,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    sd = N(mat, 'SceneDepth', -1800, 100)
    pdp = N(mat, 'PixelDepth', -1800, 200)
    custom = N(mat, 'Custom', -700, 0, code=WATER_HLSL, description='FrozenLakeWater',
               output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    spec = [('WP', wp, ''), ('V', cam, ''), ('Time', time, ''), ('N', noise_obj, ''), ('SD', sd, ''), ('PD', pdp, '')]
    pspec, _ = _param_nodes(mat, WATER_SCALARS, WATER_VECTORS, {}, -1200, -300)
    _custom_outputs(custom, (('Roughness', unreal.CustomMaterialOutputType.CMOT_FLOAT1),
                             ('Opacity', unreal.CustomMaterialOutputType.CMOT_FLOAT1)))
    _set_custom_inputs(custom, spec + pspec)
    P = unreal.MaterialProperty
    for prop, out in ((P.MP_BASE_COLOR, ''), (P.MP_ROUGHNESS, 'Roughness'), (P.MP_OPACITY, 'Opacity')):
        if not MEL.connect_material_property(custom, out, prop):
            failures.append('water custom.%s -> %s' % (out, prop))
    MEL.recompile_material(mat)
    save(mat)
    log('created %s' % mat.get_path_name())
    return mat


def make_mi(name, folder, parent, scalars=None, switches=None, textures=None, vectors=None, blend_mode=None):
    path = folder + '/' + name
    mi = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
    if mi is None:
        mi = TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant,
                                unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        if isinstance(v, tuple):
            v = unreal.LinearColor(v[0], v[1], v[2], 1.0)
        MEL.set_material_instance_vector_parameter_value(mi, k, v)
    for k, v in (textures or {}).items():
        MEL.set_material_instance_texture_parameter_value(mi, k, v)
    for k, v in (switches or {}).items():
        MEL.set_material_instance_static_switch_parameter_value(mi, k, v)
    if blend_mode is not None:
        ov = mi.get_editor_property('base_property_overrides')
        ov.set_editor_property('override_blend_mode', True)
        ov.set_editor_property('blend_mode', blend_mode)
        mi.set_editor_property('base_property_overrides', ov)
    MEL.update_material_instance(mi)
    save(mi)
    return mi


def blend_of(mi):
    try:
        return str(mi.get_blend_mode())
    except Exception:  # noqa: BLE001
        try:
            ov = mi.get_editor_property('base_property_overrides')
            return 'override=%s %s' % (ov.get_editor_property('override_blend_mode'),
                                       ov.get_editor_property('blend_mode'))
        except Exception as exc:  # noqa: BLE001
            return repr(exc)


def stats(mat):
    try:
        st = MEL.get_statistics(mat)  # Material or MaterialInstance (the instance's static-switch permutation)
        return {'pixel_shader_instructions': st.num_pixel_shader_instructions,
                'vertex_shader_instructions': st.num_vertex_shader_instructions,
                'samplers': st.num_samplers, 'pixel_texture_samples': st.num_pixel_texture_samples,
                'vertex_texture_samples': st.num_vertex_texture_samples,
                'uv_scalars': st.num_uv_scalars, 'interpolator_scalars': st.num_interpolator_scalars}
    except Exception as exc:  # noqa: BLE001
        return {'error': repr(exc)}


def build(test_origin=None):
    """test_origin: world (x, y) of the test lake's min corner in cm, for MI_FrozenLakeIce_TestLake."""
    os.makedirs(REPORT_DIR, exist_ok=True)
    TC = unreal.TextureCompressionSettings
    noise = import_texture(HERE + '/textures/T_FrozenLakeIce_Noise.png', TEX, 'T_FrozenLakeIce_Noise', TC.TC_BC7,
                           only_if_missing=True)
    neutral = import_texture(HERE + '/textures/T_FrozenLakeIce_StateNeutral.png', TEX, 'T_FrozenLakeIce_StateNeutral',
                             TC.TC_VECTOR_DISPLACEMENTMAP, address_clamp=True, mips=False, only_if_missing=True)
    test_state = import_texture(HERE + '/textures/T_FrozenLakeIce_TestState.png', TEST, 'T_FrozenLakeIce_TestState',
                                TC.TC_VECTOR_DISPLACEMENTMAP, address_clamp=True)
    if EAL.does_asset_exist(MASTER):
        live = unreal.load_asset(MASTER)
        # the map's ice mesh (SM_FL_IceLake) is Nanite; set the usage flag here so no editor dirties the master later
        live.set_editor_property('used_with_nanite', True)
        # the track normal is world space: the ice is world-mapped and the meshes' tangents are not to be trusted
        live.set_editor_property('tangent_space_normal', False)
        mat = update_in_place(live, HLSL, FIXED_INPUTS, SCALARS, VECTORS, extra=TRAIL_INPUTS, outputs=ICE_OUTPUTS,
                              links=ICE_LINKS)
    else:
        mat = create_master(noise, neutral)
    water = build_water(noise)
    mis = {
        'Heavy': make_mi('MI_FrozenLakeIce_Heavy', HOME, mat, scalars={'BaseThickness': 1.0}),
        'Medium': make_mi('MI_FrozenLakeIce_Medium', HOME, mat, scalars={'BaseThickness': 0.5}),
        'Infantry': make_mi('MI_FrozenLakeIce_Infantry', HOME, mat, scalars={'BaseThickness': 0.0}),
        # black ice over the Radixite vein (the reveal): same ice, roots on
        'InfantryVein': make_mi('MI_FrozenLakeIce_Infantry_Vein', HOME, mat,
                                scalars={'BaseThickness': 0.0, 'VeinAmount': 1.0}),
        # one lake mesh, state from the mask; Masked so open water / holes show the water plane and what lies below
        'Lake': make_mi('MI_FrozenLakeIce_Lake', HOME, mat, switches={'UseStateMask': True},
                        textures={'StateMask': neutral}, blend_mode=unreal.BlendMode.BLEND_MASKED),
        'Water': make_mi('MI_FrozenLakeWater', HOME, water),
    }
    ox, oy = test_origin or (0.0, 0.0)
    mis['TestLake'] = make_mi('MI_FrozenLakeIce_TestLake', TEST, mis['Lake'],
                              textures={'StateMask': test_state},
                              scalars={'LakeSize': TEST_LAKE_SIZE_CM},
                              vectors={'LakeOrigin': unreal.LinearColor(ox, oy, 0.0, 0.0)})
    # black-ice readability bracket (test only, scalar/vector overrides = no new shader permutation)
    mis['BlackDarker'] = make_mi('MI_FrozenLakeIce_Black_Darker', TEST, mat, scalars={
        'BaseThickness': 0.0, 'SkyReflect': 0.22, 'FrostAmount': 0.2, 'ScratchStrength': 0.3},
        vectors={'BlackIceColor': (0.026, 0.034, 0.042)})
    mis['BlackLighter'] = make_mi('MI_FrozenLakeIce_Black_Lighter', TEST, mat, scalars={
        'BaseThickness': 0.0, 'SkyReflect': 0.45, 'FrostAmount': 0.45, 'ScratchStrength': 0.6},
        vectors={'BlackIceColor': (0.07, 0.088, 0.105)})
    report['blend_modes'] = {k: blend_of(v) for k, v in mis.items()}
    log('blend modes %s' % report['blend_modes'])
    report['stats_master'] = stats(mat)
    report['expressions'] = MEL.get_num_material_expressions(mat)
    log('master stats %s' % report['stats_master'])
    json.dump(report, open(REPORT_DIR + 'build_report.json', 'w'), indent=1)
    if failures:
        log('FAILURES %s' % failures)
    return mat, mis


if __name__ == '__main__':
    build()
