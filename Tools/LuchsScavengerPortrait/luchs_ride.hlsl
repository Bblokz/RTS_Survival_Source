// Ruetteln ("the ride"): the Scavenger stands in the hatch of a Panzer II Luchs going flat out. Tank and crewman jolt
// over random bumps, the steppe and dust behind him stream away into the distance, he nods on stressed syllables and
// the rags on his hood flutter (Custom node body of M_UI_LuchsScavengerRide). Port of option C from the browser
// preview (2026-11-14, Saved/LightTankPortrait/fahrtwind_talk_template.html, ride()), plus option B's filter breath
// (filterBreath()): each syllable puffs vapour from the two cheek filters and the head wind smears it toward the rags;
// slow breaths keep a faint puff going between words.
// Foreground (crewman + tank) is re-sampled through the jolt, the head nod (mask G) and the rag flutter (mask R); the
// background (mask B) is a flow of the render toward the vanishing point behind him, blurred along the flow, two
// phases cross-faded so the scroll never jumps. Textures are sRGB, so samples are linear; the result is linear.
// Inputs: Tex (texture object = portrait), Mask (texture object; R = rags, G = follows the head, B = background), UV,
// T (seconds), Aspect (= width / height), NeckVP (float4: neck xy, vanishing point zw), Rag (float4: rag root xy,
// rag tip zw), Filters (float4: left filter xy, right filter zw), all in height units p = (u * Aspect, v), Voice (0..1),
// Jolt (bump scale), Nod (head scale), Flow (background flow scale), Flutter (rag scale), Breath (vapour scale).
struct FLuchsRide
{
    float Hash11(float N) { return frac(sin(N * 12.9898) * 43758.5453); }
    float Noise1(float X)
    {
        float I = floor(X);
        float F = frac(X);
        F = F * F * (3.0 - 2.0 * F);
        return lerp(Hash11(I), Hash11(I + 1.0), F);
    }
    float Hash21(float2 Q)
    {
        Q = frac(Q * float2(123.34, 456.21));
        Q += dot(Q, Q + 45.32);
        return frac(Q.x * Q.y);
    }
    float Noise2(float2 Q)
    {
        float2 I = floor(Q);
        float2 F = frac(Q);
        float2 U = F * F * (3.0 - 2.0 * F);
        return lerp(lerp(Hash21(I), Hash21(I + float2(1.0, 0.0)), U.x),
                    lerp(Hash21(I + float2(0.0, 1.0)), Hash21(I + float2(1.0, 1.0)), U.x), U.y);
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
    // Sharp decaying knocks at irregular moments (the tracks hitting ruts) over a fine rumble.
    float Bumps(float Tm)
    {
        float K = floor(Tm * 2.2);
        float F = Tm * 2.2 - K;
        return exp(-F * 8.0) * sin(F * 34.0) * (0.4 + 0.6 * Hash11(K)) + 0.25 * (Noise1(Tm * 9.0) - 0.5);
    }
    float2 Rot(float2 V, float A)
    {
        float Ca = cos(A);
        float Sa = sin(A);
        return float2(Ca * V.x - Sa * V.y, Sa * V.x + Ca * V.y);
    }
};
FLuchsRide H;

#define SMP(PP) Texture2DSample(Tex, TexSampler, float2((PP).x / Aspect, (PP).y)).rgb
#define MSK(PP) Texture2DSample(Mask, MaskSampler, float2((PP).x / Aspect, (PP).y))

float2 P = float2(UV.x * Aspect, UV.y);
float2 Neck = NeckVP.xy;
float2 VP = NeckVP.zw;
float E = 0.5 * H.Envelope(T) + 0.3 * H.Envelope(T - 0.06) + 0.2 * H.Envelope(T - 0.13);
float Stress = smoothstep(0.55, 0.95, E) * Voice;

// Tank and crewman jolt together.
float JoltY = (H.Bumps(T) * 0.0045 + 0.003 * Stress * sin(T * 2.1)) * Jolt;
float2 PF = P - float2(0.0, JoltY);

// Head nod about the neck (feathered head mask).
float W = 0.0;
for (int I = 0; I < 8; I++)
{
    float Aa = I * 0.785398;
    W += MSK(PF + float2(cos(Aa), sin(Aa)) * 0.006).g;
}
W = lerp(W / 8.0, MSK(PF).g, 0.3);
float A = (0.006 * sin(T * 0.8) + 0.008 * Stress * sin(T * 2.3)) * W * Nod;
float2 PH = Neck + H.Rot(PF - Neck, -A) - float2(0.0, 0.003 * Stress * W * Nod);

// Rags flutter: a wave runs out along the rag axis (feathered rag mask).
float WR = 0.0;
for (int J = 0; J < 8; J++)
{
    float Ab = J * 0.785398;
    WR += MSK(PH + float2(cos(Ab), sin(Ab)) * 0.012).r;
}
WR = max(WR / 8.0, MSK(PH).r);
float2 Ax = Rag.zw - Rag.xy;
float RagLen = length(Ax);
float2 RagDir = Ax / RagLen;
float2 RagN = float2(-RagDir.y, RagDir.x);
float S = clamp(dot(PH - Rag.xy, RagDir) / RagLen, 0.0, 1.2);
float Wave = sin(S * 15.0 - T * 17.0) + 0.5 * sin(S * 27.0 - T * 29.0 + 1.3);
PH -= RagN * 0.005 * Flutter * Wave * smoothstep(0.05, 0.45, S) * WR;
float3 Fg = SMP(PH);

// Background streams into the distance behind him: two cross-faded phases, each blurred along the flow.
float2 FlowDir = normalize(VP - P);
float FlowLen = (0.035 * length(P - VP) + 0.004) * Flow;
float3 Bg = float3(0.0, 0.0, 0.0);
float WSum = 0.0;
for (int K = 0; K < 2; K++)
{
    float Ph = frac(T * 1.2 + K * 0.5);
    float Wt = 1.0 - abs(2.0 * Ph - 1.0);
    float3 Acc = float3(0.0, 0.0, 0.0);
    for (int L = 0; L < 5; L++)
    {
        float2 Q = P - FlowDir * FlowLen * (Ph - 0.5 + L * 0.06);
        Acc += (MSK(Q).b > 0.5) ? SMP(Q) : SMP(P);   // never drag the crewman or the tank into the background
    }
    Bg += Acc / 5.0 * Wt;
    WSum += Wt;
}
Bg /= max(WSum, 0.001);
float3 C = lerp(Fg, Bg, MSK(PF).b);

// Filter breath: four puffs per filter, born on the syllables (slow breaths between words), torn off along the wind
// (the rag axis) while they grow and fade. The filters ride along with the jolt and the nod (W = 1 at the mask).
float NodA = (0.006 * sin(T * 0.8) + 0.008 * Stress * sin(T * 2.3)) * Nod;
float Vap = 0.0;
for (int Fi = 0; Fi < 2; Fi++)
{
    float2 Src = (Fi == 0) ? Filters.xy : Filters.zw;
    Src = Neck + H.Rot(Src - Neck + float2(0.0, 0.003 * Stress * Nod), NodA) + float2(0.0, JoltY);
    float Seed = Fi * 0.37;
    for (int Kp = 0; Kp < 4; Kp++)
    {
        float Age = frac(T * 1.6 + Kp * 0.25 + Seed);
        float Born = T - Age / 1.6;
        float Puff = max(H.Envelope(Born) * Voice, 0.3 * pow(max(sin(Born * 1.5707963), 0.0), 3.0));
        float2 Cn = Src + RagDir * (0.012 + 0.2 * Age) + float2(0.0, -0.015 * Age);
        float R = 0.014 + 0.06 * Age;
        float2 D = P - Cn;
        float N = H.Noise2(D * 90.0 + float2(T * 3.0, Seed * 17.0));
        Vap += exp(-dot(D, D) / (R * R)) * (0.55 + 0.6 * N) * Puff * (1.0 - Age) * 1.6;
    }
}
float3 Vapour = float3(0.85, 0.84, 0.8) * (0.7 + 0.3 * dot(C, float3(0.33, 0.33, 0.33)));
C = lerp(C, Vapour + C * 0.3, saturate(Vap * Breath) * 0.85);
#undef SMP
#undef MSK
return max(C, 0.0);
