// Smoke_Puff: the powder smoke, a lit sprite with M_AmbientDust (2x2 puff atlas). The gas is blown out of the
// barrel, slows within a fraction of a second, then grows, rises and drifts with the wind while it fades.
// Each smoke shot spawns a few puffs; one stays near the barrel, others are blown further out.
// MuzzleShape: x how far it is blown, y/w sideways spread and size variation, z rise and spin variation.

// @section Shape
Particles.Lifetime = lerp(1.3, 2.4, R0) * LifetimeMlt;
Particles.MuzzleShape = float4(R1, R2, R3, R0);
Particles.SubImageIndex = floor(R3 * 3.999);

// @section Place
float Blow = 1.0 - exp(-AgeSeconds * 6.0);
float BlowDistance = (30.0 + 120.0 * Shape.x) * S;
float2 Lateral = (float2(Shape.y, Shape.w) - 0.5) * 2.0;
float3 Spread = (Right * Lateral.x + Up * Lateral.y) * (16.0 * S);
float3 Rise = float3(0.0, 0.0, (10.0 + 12.0 * Shape.z) * AgeSeconds);
float3 Wind = float3(10.0, 6.0, 0.0) * AgeSeconds;
Particles.Position = OriginValue + (Fwd * BlowDistance + Spread) * Blow + Rise + Wind;
float Growth = pow(saturate(1.0 - exp(-AgeSeconds * 1.6)), 0.8);
float PuffSize = S * lerp(28.0, 125.0, Growth) * lerp(0.8, 1.25, Shape.w);
Particles.SpriteSize = float2(PuffSize, PuffSize);
Particles.SpriteRotation = Shape.z * 360.0 + AgeSeconds * lerp(-30.0, 30.0, Shape.y);
// Smoothstep fade-in over the first 5 % of the life, then a slow fade-out.
float FadeInT = saturate(T / 0.05);
float Alpha = FadeInT * FadeInT * (3.0 - 2.0 * FadeInT) * pow(1.0 - T, 1.2) * 0.85;
Particles.Color = float4(float3(0.56, 0.55, 0.52) * lerp(0.9, 1.08, Shape.x), Alpha);
