// Kopf & Fahrt: the Tiger loader's head nods on stressed syllables and blinks, the hull's engine shakes the frame, and
// the red battle lamp jitters on its cable (Custom node body of M_UI_TigerLoaderHeadDrive). Port of option C from the
// browser preview (2026-11-12, Saved/TigerLoaderPortrait/loader_talk_template.html, headDrive()).
// The portrait is re-sampled through a fine tremor of the whole frame plus a small nod/tilt about the neck, weighted by
// the head mask (head, side cap, headset). The lamp's light comes from rendered passes: the lamp swung -9 / +9 degrees
// (SwL / SwR, rendered with the lamp itself hidden from the camera) are cross-faded with the base by the jitter amount;
// the lamp at rest (mask B) is replaced by the lamp-free background and the one lamp sprite is drawn back rotated about
// the cable's roof pivot, so there is never a second, ghost lamp. Textures are sRGB, so samples are linear and mix like
// light; the result is linear.
// Inputs: Tex / SwL / SwR (texture objects: base, lamp light swung left, swung right), Mask (texture object; G = follows
// the head, B = the lamp at rest), UV, T (seconds), Aspect (= width / height), LampPivot (float4: lamp xy, cable pivot zw), LampSwing (float4:
// lamp swung left xy, swung right zw), Neck (float2), all in height units p = (u * Aspect, v), Eye0 / Eye1 (float4:
// centre xy), Eye0Axes / Eye1Axes (float4: half-width axis xy, half-height axis zw pointing up), Voice (0..1),
// Nod (head motion scale), Tremor (engine shake scale), Jitter (lamp jitter scale), Blink (0 = no blinks).
struct FLoaderDrive
{
    float Hash11(float N) { return frac(sin(N * 12.9898) * 43758.5453); }
    float Noise1(float X)
    {
        float I = floor(X);
        float F = frac(X);
        F = F * F * (3.0 - 2.0 * F);
        return lerp(Hash11(I), Hash11(I + 1.0), F);
    }
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
    float2 Rot(float2 V, float A)
    {
        float Ca = cos(A);
        float Sa = sin(A);
        return float2(Ca * V.x - Sa * V.y, Sa * V.x + Ca * V.y);
    }
    float Ang(float2 A, float2 B) { return atan2(A.x * B.y - A.y * B.x, dot(A, B)); }
};
FLoaderDrive H;

#define SMP(TX, PP) Texture2DSample(TX, TX##Sampler, float2((PP).x / Aspect, (PP).y)).rgb
#define MSK(PP) Texture2DSample(Mask, MaskSampler, float2((PP).x / Aspect, (PP).y))
// Lamp light at jitter LampS (-1..1 = the rendered -9..+9 degree passes); the lamp at rest swapped for the lamp-free
// background of that side's pass; then the lamp sprite from the base, rotated by Phi about the pivot.
#define SIDE(PP) (LampS < 0.0 ? SMP(SwL, PP) : SMP(SwR, PP))
#define ROTP(PP) (Pivot + H.Rot((PP) - Pivot, -Phi))
// The rest-position lamp is removed with a slightly grown mask, so its anti-aliased rim leaves no outline behind.
#define LAMPOUT(PP) max(max(MSK(PP).b, MSK((PP) + float2(0.004, 0.0)).b), max(max(MSK((PP) - float2(0.004, 0.0)).b, \
                        MSK((PP) + float2(0.0, 0.004)).b), MSK((PP) - float2(0.0, 0.004)).b))
#define SWUNG(PP) lerp(lerp(lerp(SMP(Tex, PP), SIDE(PP), abs(LampS)), SIDE(PP), LAMPOUT(PP)), \
                       SMP(Tex, ROTP(PP)), MSK(ROTP(PP)).b)

const float TAU = 6.2831853;
float2 Lamp = LampPivot.xy;
float2 Pivot = LampPivot.zw;
float2 P = float2(UV.x * Aspect, UV.y);

float E = 0.5 * H.Envelope(T) + 0.3 * H.Envelope(T - 0.06) + 0.2 * H.Envelope(T - 0.13);
float Stress = smoothstep(0.55, 0.95, E) * Voice;

// Engine tremor: a fine constant shiver of the whole frame, a jolt on stressed words.
float Tr = (0.0006 + 0.0012 * Stress) * Tremor;
float2 Q = P + Tr * float2(H.Noise1(T * 38.0) - 0.5, H.Noise1(T * 41.0 + 7.0) - 0.5) * 2.0;

// Head: slow sway plus small nods on stressed syllables, about the neck, weighted by the feathered head mask.
float Sway = sin(T * 0.73) * 0.6 + sin(T * 1.31 + 1.0) * 0.4;
float A = (0.009 * Sway + 0.006 * Stress * sin(T * 2.1)) * Nod;
float2 Shift = float2(0.0, 0.0035 * Stress + 0.0012 * sin(T * 0.9)) * Nod;
float W = 0.0;
for (int I = 0; I < 8; I++)
{
    float Aa = I * 0.785398;
    W += MSK(Q + float2(cos(Aa), sin(Aa)) * 0.006).g;
}
W = lerp(W / 8.0, MSK(Q).g, 0.3);
float2 PW = Neck + H.Rot(Q - Neck - Shift * W, -A * W);

// The lamp jitters on its cable in the tremor (and keeps a slow residual swing).
float LampS = clamp((0.18 * sin(T * 11.0) * (0.5 + Stress) + 0.12 * sin(T * TAU / 1.65)) * Jitter, -1.0, 1.0);
float PhiL = H.Ang(Lamp - Pivot, LampSwing.xy - Pivot);
float PhiR = H.Ang(Lamp - Pivot, LampSwing.zw - Pivot);
float Phi = LampS < 0.0 ? -LampS * PhiL : LampS * PhiR;
float3 C = SWUNG(PW);

// Blinks (in the head's moved frame, so the lids ride with the nod).
float Lid = H.BlinkLid(T) * Blink;
if (Lid > 0.01)
{
    for (int J = 0; J < 2; J++)
    {
        float2 Ec = (J == 0) ? Eye0.xy : Eye1.xy;
        float2 Ea = (J == 0) ? Eye0Axes.xy : Eye1Axes.xy;
        float2 Eb = (J == 0) ? Eye0Axes.zw : Eye1Axes.zw;
        float2 D = PW - Ec;
        float2 Qe = float2(dot(D, Ea) / dot(Ea, Ea), dot(D, Eb) / dot(Eb, Eb));
        float Inside = 1.0 - smoothstep(0.85, 1.25, length(Qe * float2(1.0, 0.9)));
        float Edge = 1.0 - 2.0 * Lid;
        float Cover = smoothstep(Edge - 0.15, Edge + 0.15, Qe.y) * Inside;
        // Eyelid colour: mostly the lit skin below the eye, some from above (the brow side is darker).
        float3 Skin = lerp(SWUNG(Ec + Ea * Qe.x + Eb * 1.2), SWUNG(Ec + Ea * Qe.x - Eb * 1.3), 0.6) * 0.9;
        float LashLine = exp(-pow((Qe.y - Edge) / 0.08, 2.0)) * (1.0 - smoothstep(0.7, 1.0, length(Qe))) * 0.35;
        C = lerp(lerp(C, Skin, Cover), float3(0.003, 0.003, 0.003), LashLine);
    }
}
#undef SWUNG
#undef ROTP
#undef LAMPOUT
#undef SIDE
#undef SMP
#undef MSK
return max(C, 0.0);
