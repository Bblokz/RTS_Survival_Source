// Kopf & Augen: the Mosin rifleman's head nods and tilts a little on stressed syllables and his eyes blink (Custom node
// body of M_UI_MosinHeadNod). Port of option B from the browser preview (2026-11-12,
// Saved/MosinPortrait/mosin_talk_template.html, headMotion()).
// Nothing is lit or painted: the portrait is re-sampled through a small rotation + shift about the neck, weighted by a
// mask of everything that follows the head (helmet, face, scarf; the scarf's lower point and the body stay put). Blinks
// replace the eye with eyelid skin sampled just above it. Mixing happens in display space like the other portraits.
// Inputs: Tex (texture object = portrait), Mask (texture object; G = follows the head, R = scarf), UV, T (seconds),
// Aspect (= width / height), Neck (float2 pivot, height units p = (u * Aspect, v)), Eye0 / Eye1 (float4: centre xy,
// unused zw), Eye0Axes / Eye1Axes (float4: half-width axis xy, half-height axis zw pointing up), Voice (0..1),
// Nod (motion scale), Blink (0 = no blinks).
struct FMosinHead
{
    float Hash11(float N) { return frac(sin(N * 12.9898) * 43758.5453); }
    float Envelope(float Tm)
    {
        const float TAU = 6.2831853;
        float Syll = pow(saturate(sin(Tm * TAU * 3.3 + 1.7 * sin(Tm * 1.3)) * 0.5 + 0.5), 1.5);
        Syll = 0.35 + 0.65 * Syll;
        float Word = sin(Tm * 3.2) + 0.6 * sin(Tm * 1.376 + 1.0) + 0.3 * sin(Tm * 5.824 + 2.0);
        float G = saturate((Word + 1.05) / 0.45);
        G = G * G * (3.0 - 2.0 * G);
        return 0.05 + 0.95 * Syll * G;
    }
    // Lid closure 0..1: a blink every 4.1 s at a random moment in the slot, sometimes a double blink.
    float BlinkLid(float Tm)
    {
        const float PER = 4.1;
        float K = floor(Tm / PER);
        float S = Tm - K * PER - (0.5 + Hash11(K + 3.0) * 2.6);
        float One = smoothstep(-0.07, 0.0, S) - smoothstep(0.06, 0.16, S);
        float Dbl = step(0.72, Hash11(K + 9.0)) * (smoothstep(0.2, 0.26, S) - smoothstep(0.31, 0.42, S));
        return saturate(One + Dbl);
    }
};
FMosinHead H;

// Display-space sample of the portrait at a point in height units.
#define PORTRAIT(PP) pow(max(Texture2DSample(Tex, TexSampler, float2((PP).x / Aspect, (PP).y)).rgb, 0.0), 1.0 / 2.2)
#define MASK(PP) Texture2DSample(Mask, MaskSampler, float2((PP).x / Aspect, (PP).y))

float2 P = float2(UV.x * Aspect, UV.y);

// Motion: a slow sway plus small nods on stressed syllables.
float E = 0.5 * H.Envelope(T) + 0.3 * H.Envelope(T - 0.06) + 0.2 * H.Envelope(T - 0.13);
float Stress = smoothstep(0.55, 0.95, E) * Voice;
float Sway = sin(T * 0.73) * 0.6 + sin(T * 1.31 + 1.0) * 0.4;
float Ang = (0.010 * Sway + 0.006 * Stress * sin(T * 2.1)) * Nod;
float2 Shift = float2(0.0, 0.0035 * Stress + 0.0012 * sin(T * 0.9)) * Nod;

// Head weight: mask G feathered over a few pixels; the scarf follows less the lower it hangs below the neck.
float W = 0.0;
for (int I = 0; I < 8; I++)
{
    float A = I * 0.785398;
    W += MASK(P + float2(cos(A), sin(A)) * 0.006).g;
}
float4 M0 = MASK(P);
W = lerp(W / 8.0, M0.g, 0.3);
float Follow = 1.0 - smoothstep(Neck.y - 0.02, Neck.y + 0.12, P.y);
W *= lerp(1.0, Follow, smoothstep(0.3, 0.7, M0.r));

float2 R = P - Neck - Shift * W;
float Ca = cos(-Ang * W);
float Sa = sin(-Ang * W);
float2 PW = Neck + float2(Ca * R.x - Sa * R.y, Sa * R.x + Ca * R.y);
float3 C = PORTRAIT(PW);

// Blinks (in the head's moved frame, so the lids ride with the nod).
float Lid = H.BlinkLid(T) * Blink;
if (Lid > 0.01)
{
    const float3 LASH = float3(0.05, 0.035, 0.03);
    float Edge = 1.0 - 2.0 * Lid;                     // lid line moves from the top of the eye (+1) down
    for (int J = 0; J < 2; J++)
    {
        float2 Ec = (J == 0) ? Eye0.xy : Eye1.xy;
        float2 Ea = (J == 0) ? Eye0Axes.xy : Eye1Axes.xy;
        float2 Eb = (J == 0) ? Eye0Axes.zw : Eye1Axes.zw;
        float2 D = PW - Ec;
        float2 Q = float2(dot(D, Ea) / dot(Ea, Ea), dot(D, Eb) / dot(Eb, Eb));
        float Inside = 1.0 - smoothstep(0.85, 1.25, length(Q * float2(1.0, 0.9)));
        float Cover = smoothstep(Edge - 0.15, Edge + 0.15, Q.y) * Inside;
        // Eyelid colour: the skin right above the eye sits in the helmet brim's shadow (darker than the open eye), so
        // blend it with the lit skin just below the eye; the closed lid then reads as a shadowed lid, not a hole.
        float3 Skin = lerp(PORTRAIT(Ec + Ea * Q.x + Eb * 1.2), PORTRAIT(Ec + Ea * Q.x - Eb * 1.3), 0.6) * 0.9;
        // Thin, soft lash line on the lid edge, only within the eye opening (a wide dark band read as an empty socket).
        float LashLine = exp(-pow((Q.y - Edge) / 0.08, 2.0)) * (1.0 - smoothstep(0.7, 1.0, length(Q))) * 0.35;
        C = lerp(lerp(C, Skin, Cover), LASH, LashLine);
    }
}
#undef PORTRAIT
#undef MASK
return pow(max(C, 0.0), 2.2);
