// Flash_Glow: a soft hot glow a little in front of the barrel, a camera-facing sprite with M_SmallArmsMuzzleGlow.
// It is what reads as the shot from the RTS camera, where the meshes are only a few pixels long.
// MuzzleShape: x size variation, y brightness variation, z and w unused.

// @section Shape
// At least one and a half frames: on a slow frame the flash would otherwise die before it is ever drawn.
Particles.Lifetime = max(lerp(0.05, 0.085, R0), Engine.DeltaTime * 1.5);
Particles.MuzzleShape = float4(R1, R2, R3, R0);

// @section Place
// Smoothstep fade over the last 85 % of the life.
float FadeT = saturate((T - 0.15) / 0.85);
float Pop = 1.0 - FadeT * FadeT * (3.0 - 2.0 * FadeT);
Particles.Position = OriginValue + Fwd * (14.0 * S);
float GlowSize = S * lerp(30.0, 42.0, Shape.x) * lerp(1.0, 0.75, T);
Particles.SpriteSize = float2(GlowSize, GlowSize);
Particles.SpriteRotation = 0.0;
Particles.Color = float4(float3(1.4, 0.48, 0.08) * lerp(0.75, 1.15, Shape.y), Pop);
