// Ground_Dust: the haze a strafe leaves hanging over the band; grows slowly, drifts with the wind and fades.
// One lit puff per impact, larger and longer than the spurt, so the band stays visible for a few seconds.
// StrafeShape: x size, y drift, z spin, w surface kind.

// @section Shape
Particles.StrafeShape = float4(R0, R1, R2, SurfaceKind);
Particles.Lifetime = lerp(2.0, 3.4, R3) * sqrt(CalibreScale);
Particles.SubImageIndex = floor(R2 * 3.999);

// @section Place
float Growth = 1.0 - exp(-AgeSeconds * 1.4);
float3 Drift = float3(14.0, 8.0, 0.0) * AgeSeconds * lerp(0.6, 1.4, Shape.y);
Particles.Position = OriginValue + GroundUp * (25.0 * S + 30.0 * S * Growth) + Drift;
float PuffSize = S * lerp(70.0, 230.0, Growth) * lerp(0.8, 1.25, Shape.x);
Particles.SpriteSize = float2(PuffSize, PuffSize);
Particles.SpriteRotation = Shape.z * 360.0 + AgeSeconds * lerp(-20.0, 20.0, Shape.y);
float FadeInT = saturate(T / 0.08);
float Alpha = FadeInT * pow(1.0 - T, 1.3) * 0.5;
Particles.Color = float4(DirtColour * 1.15, Alpha);
