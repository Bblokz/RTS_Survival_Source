// Scavenger talking portrait (Custom node body of M_UI_ScavengerBreathVapour), two effects on one clock:
//  * Atemdampf: breath vapour puffing out of the two cheek filters (option C of the 2026-11-12 browser preview,
//    Saved/ScavengerPortrait/scavenger_talk_template.html, vapourFrom()).
//  * Radix-Ampullen: the two green vials on the strap glow and bubble with the voice, light falling on the strap,
//    plate and hose (option B of the same preview, vialGlow(); added later on 2026-11-12 at the user's request).
// The preview worked on display-space (sRGB-encoded) texels; here the texel is encoded to display space, the vial light
// is added, the vapour screened over, and the result decoded back to linear for the UI emissive output.
// Inputs: Base (float3, linear texel at UV), UV, T (seconds), Aspect (= width / height), FilL / FilR (float3: filter
// centre in height units p = (u * Aspect, v), z = side * filter radius, left negative), Voice (0..1), Strength (0..1,
// vapour), Rise (vertical drift multiplier), V1A / V1B / V2A / V2B (float2: vial capsule end centres in height units,
// A = lower end, bubbles rise toward B), V1R / V2R (capsule radii, height units), VialStrength (0..1, vial light).
struct FScavVapour
{
    float Hash11(float N) { return frac(sin(N * 12.9898) * 43758.5453); }
    float Hash21(float2 P)
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
        return lerp(lerp(Hash21(I), Hash21(I + float2(1, 0)), U.x), lerp(Hash21(I + float2(0, 1)), Hash21(I + float2(1, 1)), U.x), U.y);
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
    // Talking envelope, identical to announcer_trace.envelope() (shared by every speaker portrait).
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
    float Breath(float Tm) { return pow(max(sin(Tm * 6.2831853 * 0.24), 0.0), 4.0); }
    // Vapour cover 0..1 from one filter: a puff is born every 0.11 s with the loudness at its birth, drifts outward and
    // up, grows, breaks into wisps and fades over 1.55 s.
    float FromFilter(float2 P, float3 Fil, float Tm, float Voice, float Rise)
    {
        float Side = sign(Fil.z);
        float R = abs(Fil.z);
        const float DT = 0.11;
        const float LIFE = 1.55;
        float N0 = floor(Tm / DT);
        float Keep = 1.0;
        for (int K = 0; K < 15; K++)
        {
            float Tb = (N0 - K) * DT;
            float Age = Tm - Tb;
            float Str = max(pow(Envelope(Tb), 1.6) * Voice, Breath(Tb) * 0.55);
            if (Age > LIFE || Str < 0.02)
            {
                continue;
            }
            float J = Hash11(Tb * 13.1 + Side);
            float2 Vel = float2(Side * (0.045 + 0.03 * J), (-0.035 - 0.03 * Hash11(Tb * 5.3 + Side)) * Rise);
            float2 Pos = Fil.xy + float2(Side * R * 0.75, R * 0.1) + Vel * Age + float2(0.0, 0.012 * Age * Age);
            float Rad = R * (0.45 + 0.35 * J) + 0.035 * Age;
            float D = length((P - Pos) * float2(1.0, 1.15)) / Rad;
            float Wisp = Fbm((P - Pos) * 70.0 + float2(Tb * 9.0, -Age * 2.0));
            float A = exp(-D * D * 1.6) * smoothstep(0.25, 0.75, Wisp + 0.25 * (1.0 - Age / LIFE)) * Str
                    * pow(1.0 - Age / LIFE, 1.4) * 1.6;
            Keep *= 1.0 - clamp(A * 0.55, 0.0, 0.9);
        }
        return 1.0 - Keep;
    }
    // Vial light level: a slightly lagged envelope (the liquid answers a little behind the voice), dim rest glow.
    float VialLevel(float Tm, float Voice)
    {
        float E = 0.45 * Envelope(Tm) + 0.3 * Envelope(Tm - 0.08) + 0.25 * Envelope(Tm - 0.18);
        return 0.14 + 0.86 * E * Voice;
    }
    // Light one vial adds in display space: glow in the liquid (keeps the glass texture), rising bubbles, a thin halo and
    // green spill on nearby surfaces. Bubbles rise at a fixed speed; only their brightness follows the voice (a
    // voice-driven speed makes the phase jump once the material clock is large).
    float3 VialGlow(float2 P, float3 B, float2 A, float2 Bt, float R, float Level, float Seed, float Tm)
    {
        const float3 GREEN = float3(0.55, 1.0, 0.28);
        float2 AB = Bt - A;
        float Len = length(AB);
        float H = saturate(dot(P - A, AB) / dot(AB, AB));
        float D = length(P - A - AB * H) - R;
        float Inside = 1.0 - smoothstep(-R * 0.12, R * 0.05, D);
        float Core = 1.0 - smoothstep(-R, 0.0, D);
        float2 Ax = AB / Len;
        float2 Nx = float2(-Ax.y, Ax.x);
        float U = dot(P - A, Ax) / Len;
        float W = dot(P - A, Nx) / R;
        float Bub = 0.0;
        for (int K = 0; K < 7; K++)
        {
            float Ph = frac(Hash11(K + Seed) + Tm * 0.75 * (0.6 + 0.5 * Hash11(K * 3.1 + Seed)));
            float2 Bc = float2(Ph * 1.1 - 0.05, (Hash11(K * 7.7 + Seed) - 0.5) * 1.2);
            float2 Dv = float2((U - Bc.x) * Len / R, W - Bc.y);
            float Br = 0.13 + 0.1 * Hash11(K * 1.9 + Seed);
            Bub += (1.0 - smoothstep(Br * 0.4, Br, length(Dv))) * (1.0 - smoothstep(0.8, 1.05, Ph));
        }
        float Glass = 0.6 + 1.4 * dot(B, float3(0.3, 0.59, 0.11));
        float3 Em = GREEN * (0.12 + 0.5 * Core) * Glass * Level * Inside;
        Em += GREEN * Bub * Inside * (0.3 + 0.6 * Level);
        float Out = max(D, 0.0);
        float Halo = exp(-Out * Out / (R * R * 0.9)) * (1.0 - Inside) * 0.16 * Level;
        float Spill = exp(-Out * Out / (R * R * 12.0)) * 0.9 * Level;
        return Em + B * GREEN * 0.9 * Inside * Level + GREEN * Halo + B * GREEN * Spill;
    }
};
FScavVapour V;

float2 P = float2(UV.x * Aspect, UV.y);
float3 B = pow(max(Base, 0.0), 1.0 / 2.2);       // display space

// Vials first (they sit on the chest, under the vapour), with a soft knee so the liquid never clips hard.
float Lv = V.VialLevel(T, Voice) * VialStrength;
float3 C = B + V.VialGlow(P, B, V1A, V1B, V1R, Lv, 1.0, T) + V.VialGlow(P, B, V2A, V2B, V2R, Lv * 0.92, 7.0, T);
C = C / (1.0 + max(C - 1.0, 0.0));

// Vapour screened over everything.
float Cover = 1.0 - (1.0 - V.FromFilter(P, FilL, T, Voice, Rise)) * (1.0 - V.FromFilter(P, FilR, T, Voice, Rise));
Cover = clamp(Cover * Strength, 0.0, 0.9);
const float3 VAPOUR = float3(0.8, 0.74, 0.64);    // warm, dusk-lit vapour
C = 1.0 - (1.0 - C) * (1.0 - VAPOUR * Cover);
return pow(C, 2.2);
