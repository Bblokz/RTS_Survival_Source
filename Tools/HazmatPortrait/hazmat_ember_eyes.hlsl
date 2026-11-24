// Ember Breath: red glow behind the SovietHazmat gas-mask lenses (Custom node body of M_UI_HazmatEmberEyes).
// Port of option A from the browser preview (2026-11-10). The preview drew the decoded (linear) texel straight to an
// sRGB screen, so its glow is in display units on a darker portrait. Here the same glow is added in display space to the
// correctly encoded portrait, then converted back to linear for the UI emissive output.
// Inputs: Base (float3, linear texel), UV, T (seconds), Aspect (= texture width / height), LCenter/LAxisA/LAxisB and
// RCenter/RAxisA/RAxisB (lens ellipses in height units: p = (u * Aspect, v); AxisA = major semi-axis vector, AxisB = minor),
// Voice (0..1), BreathHz, Intensity, Spill, SpillRadius.
const float TAU = 6.2831853;
const float3 RED = float3(1.0, 0.07, 0.035);
const float3 HOT = float3(1.0, 0.55, 0.35);

float3 b = max(Base, 0.0);
float3 s = pow(b, 1.0 / 2.2);

// Talking envelope, identical to announcer_trace.envelope() (the advisor's oscillograph).
float syll = pow(saturate(sin(T * TAU * 3.3 + 1.7 * sin(T * 1.3)) * 0.5 + 0.5), 1.5);
syll = 0.35 + 0.65 * syll;
float word = sin(T * 3.2) + 0.6 * sin(T * 1.376 + 1.0) + 0.3 * sin(T * 5.824 + 2.0);
float gate = saturate((word + 1.05) / 0.45);
gate = gate * gate * (3.0 - 2.0 * gate);
float e = (0.05 + 0.95 * syll * gate) * Voice;

// Slow respirator breath under the speech rhythm.
float breath = 0.5 + 0.5 * sin(T * TAU * BreathHz - 1.5708);
breath *= breath;
float level = (0.38 + 0.22 * breath + 0.6 * e) * Intensity;

float2 p = float2(UV.x * Aspect, UV.y);
float glass = 0.55 + 1.6 * dot(b, float3(0.3, 0.59, 0.11));   // grime and reflections on the glass stay visible
float3 add = float3(0.0, 0.0, 0.0);
[unroll] for (int i = 0; i < 2; i++)
{
    float2 c = (i == 0) ? LCenter : RCenter;
    float2 A = (i == 0) ? LAxisA : RAxisA;
    float2 B = (i == 0) ? LAxisB : RAxisB;
    float2 d = p - c;
    float2 q = float2(dot(d, A) / dot(A, A), dot(d, B) / dot(B, B));
    float r = length(q);
    float inside = 1.0 - smoothstep(0.86, 1.0, r);
    float core = 1.0 - smoothstep(0.0, 1.0, r);
    float fill = inside * (0.45 + 0.75 * core) * glass * level;
    float rim = max(r - 1.0, 0.0);
    float halo = (1.0 - inside) * exp(-rim * rim * 9.0) * 0.32 * level;
    add += lerp(RED, HOT, clamp(fill * fill * 0.5, 0.0, 0.6)) * (fill * 1.25 + halo);
}

// Red light falling on the brass rims, mask and hood.
float2 mid = (LCenter + RCenter) * 0.5;
float dm = length((p - mid) * float2(1.0, 0.85));
add += b * RED * exp(-dm * dm / (SpillRadius * SpillRadius)) * Spill * level;

float3 D = s + add;
D = D / (1.0 + max(D - 1.0, 0.0));   // soft knee so the hot cores do not clip hard
return pow(D, 2.2);
