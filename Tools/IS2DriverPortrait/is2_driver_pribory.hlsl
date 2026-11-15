// Pribory ("the instruments"): the IS-2 driver (mekhvod) at his station, fists on the steering levers, a band of
// daylight from the vision slit across his eyes (Custom node body of M_UI_IS2DriverPribory). Port of option B from the
// browser preview (2026-11-15, Saved/IS2Portrait/driver_talk_template.html, pribory()), with his head locked: the user
// wants the face to stay put while everything around him moves.
// While he talks the engine revs with his voice: the amber panel light pulses on his face (lighting only), the dials
// wobble about their centres, and a fine engine tremor runs through the station and his fists on the levers. A canteen
// hangs on its strap left of his head (his right) and swings with the hull, harder and with an engine jiggle as the
// revs rise (added 2026-11-15 so the motion reads at the in-game size). His head never moves: no nod, and the band of
// light holds still across his eyes.
// The render is re-sampled through the tremor (mask G = tremor weight, 0 on the man and forced to 0 on his head) and the
// dial wobble (mask R = gauge faces), then the panel light is scaled by adding (render - panel-off render) * (I - 1).
// The canteen is its own layer (Swing, premultiplied in linear light), drawn over the plate rotated about its hook.
// Textures are sRGB, so samples are linear; the result is linear.
// Inputs: Tex (texture object = portrait), PanelOff (texture object = the render with the panel unlit), Mask (texture
// object; R = gauge faces, G = tremor weight), Swing (texture object = the canteen layer, premultiplied RGBA), UV,
// T (seconds), Aspect (= width / height), G01 (float4: gauge 0 xy, gauge 1 zw), G23 (float4: gauge 2 xy, gauge 3 zw),
// G4 (float4: gauge 4 xy), SwingPivot (float4: hook xy, canteen centre zw), all in height units p = (u * Aspect, v),
// Voice (0..1), Tremor (tremor scale), Needles (dial wobble scale), Glow (panel light pulse scale), Sway (canteen swing
// scale).
struct FIS2Pribory
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
    float2 Rot(float2 V, float A)
    {
        float Ca = cos(A);
        float Sa = sin(A);
        return float2(Ca * V.x - Sa * V.y, Sa * V.x + Ca * V.y);
    }
};
FIS2Pribory H;

#define SMP(PP) Texture2DSample(Tex, TexSampler, float2((PP).x / Aspect, (PP).y)).rgb
#define OFF(PP) Texture2DSample(PanelOff, PanelOffSampler, float2((PP).x / Aspect, (PP).y)).rgb
#define MSK(PP) Texture2DSample(Mask, MaskSampler, float2((PP).x / Aspect, (PP).y))

float2 P = float2(UV.x * Aspect, UV.y);
float E = 0.5 * H.Envelope(T) + 0.3 * H.Envelope(T - 0.06) + 0.2 * H.Envelope(T - 0.13);
// Engine revs: idle when silent, following the voice when he talks.
float Rev = lerp(0.35, E, Voice);

// Engine tremor on the station and his fists only (mask G is 0 on the rest of him and on his head).
float2 Trem = float2(H.Noise1(T * 47.0) - 0.5, H.Noise1(T * 53.0 + 9.0) - 0.5) * (0.0010 + 0.0014 * Rev) * Tremor;
float2 Q = P - Trem * MSK(P).g;

// Dials: the whole face of the nearest gauge wobbles about its centre, which reads as the needle shaking.
float GaugeW = MSK(Q).r;
if (GaugeW > 0.02)
{
    float2 Gs[5] = { G01.xy, G01.zw, G23.xy, G23.zw, G4.xy };
    float Best = 1e9;
    float2 C = Q;
    float Id = 0.0;
    for (int I = 0; I < 5; I++)
    {
        float D = distance(Q, Gs[I]);
        if (D < Best)
        {
            Best = D;
            C = Gs[I];
            Id = float(I);
        }
    }
    float A = Needles * (0.03 + 0.07 * Rev) * (H.Noise1(T * 26.0 + Id * 7.3) - 0.5) * 2.0;
    Q = C + H.Rot(Q - C, A * GaugeW);
}

// Panel light: the render minus the panel-off render is the panel's own light; scale it with the revs.
float3 B = SMP(Q);
float3 Kp = max(B - OFF(Q), 0.0);
float PanelI = 1.0 + Glow * (0.75 + 0.45 * Rev + 0.06 * sin(T * 37.0) * Rev - 1.0);
float3 Col = B + Kp * (PanelI - 1.0);

// Canteen: a pendulum about the hook (about 1 Hz for its 26 cm strap), wider and with an engine jiggle as the revs rise,
// a small bounce along the strap, and the hook itself riding the station tremor.
float Ang = (sin(T * 6.1 + 0.8 * sin(T * 0.7)) * (0.035 + 0.05 * Rev)
             + 0.03 * (H.Noise1(T * 31.0) - 0.5) * Rev) * Sway;
float2 Ps = SwingPivot.xy + H.Rot(P - Trem - SwingPivot.xy, -Ang);
Ps.y -= 0.004 * Rev * Sway * (H.Noise1(T * 23.0 + 4.0) - 0.5);
float4 Lay = Texture2DSample(Swing, SwingSampler, float2(Ps.x / Aspect, Ps.y));
return Col * (1.0 - Lay.a) + Lay.rgb;
