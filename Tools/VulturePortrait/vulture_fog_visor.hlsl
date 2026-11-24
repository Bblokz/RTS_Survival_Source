// Atemhauch: breath fog on the inside of the Vulture's visor (Custom node body of M_UI_VultureFogVisor).
// Port of option A from the browser preview (2026-11-12, CommanderPortraits/Tools/vulture_talk_template.html, fogVisor()).
// The preview worked on display-space (sRGB-encoded) texels; here every sampled texel is encoded to display space, the
// fog is mixed in display space, and the result is decoded back to linear for the UI emissive output. Pixels outside the
// visor mask therefore come out exactly as the portrait texture.
// Inputs: Base (float3, linear texel at UV), Tex (texture object = the portrait, for the blur taps; TexSampler is its
// sampler), Mask (float, visor mask 0..1, linear), UV, T (seconds), Aspect (= width / height), Mouth (float2, height
// units p = (u * Aspect, v)), VisH (visor height, height units), Voice (0..1), Strength (0..1 fog opacity scale),
// Spread (fog reach multiplier).
struct FVultureFog
{
    float Hash(float2 P)
    {
        P = frac(P * float2(123.34, 456.21));
        P += dot(P, P + 45.32);
        return frac(P.x * P.y);
    }
    float Noise(float2 P)
    {
        float2 I = floor(P);
        float2 F = frac(P);
        float2 U = F * F * (3.0 - 2.0 * F);
        return lerp(lerp(Hash(I), Hash(I + float2(1, 0)), U.x), lerp(Hash(I + float2(0, 1)), Hash(I + float2(1, 1)), U.x), U.y);
    }
    float Fbm(float2 P)
    {
        float S = 0.0;
        float A = 0.5;
        for (int I = 0; I < 4; I++)
        {
            S += A * Noise(P);
            P *= 2.03;
            A *= 0.5;
        }
        return S;
    }
    // Talking envelope, identical to announcer_trace.envelope() (the advisor's oscillograph and the hazmat's eyes).
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
};
FVultureFog V;
const float TAU = 6.2831853;

// Fog amount: leaky sum of the recent envelope (stateless), with a small breath puff every ~4 s.
float Sum = 0.0;
float Norm = 0.0;
float W = 1.0;
for (int K = 0; K < 14; K++)
{
    Sum += W * V.Envelope(T - K * 0.1);
    Norm += W;
    W *= 0.84;
}
float Breath = pow(max(sin(T * TAU * 0.24), 0.0), 4.0) * 0.32;
float F = max(Sum / Norm * Voice * 1.25, Breath);

float3 B = pow(max(Base, 0.0), 1.0 / 2.2);   // display space
float2 P = float2(UV.x * Aspect, UV.y);
float2 D = (P - Mouth) / VisH;
D.y = D.y * 1.05 + 0.08 * F;                  // spreads up and out from the mouth
float Dist = length(D * float2(0.85, 1.0));
float Edge = (0.12 + 0.72 * F) * Spread;
float N = V.Fbm(P * 30.0 + float2(0.0, -T * 0.04));
float Dens = 1.0 - smoothstep(Edge * 0.35, Edge, Dist + (N - 0.5) * 0.26);
Dens *= 0.45 + 0.55 * smoothstep(0.0, 0.6, F);
float Drops = smoothstep(0.7, 0.92, V.Noise(P * 300.0)) * 0.5;

// Soften the visor reflections under the mist: 8 taps on a ring.
float3 Blur = 0.0;
for (int J = 0; J < 8; J++)
{
    float A = J * 0.785398;
    float2 O = float2(cos(A) / Aspect, sin(A)) * 0.012;
    Blur += pow(max(Texture2DSample(Tex, TexSampler, UV + O).rgb, 0.0), 1.0 / 2.2);
}
Blur = lerp(Blur / 8.0, B, 0.25);
float3 Mist = float3(0.6, 0.64, 0.65) * (0.5 + 0.7 * dot(Blur, float3(0.3, 0.59, 0.11))) + Drops * 0.25;
float Amount = clamp(Dens * 0.8, 0.0, 0.92) * saturate(Mask) * Strength;
float3 C = lerp(B, lerp(Blur, Mist, 0.7), Amount);
C = C / (1.0 + max(C - 1.0, 0.0));            // soft knee
return pow(C, 2.2);
