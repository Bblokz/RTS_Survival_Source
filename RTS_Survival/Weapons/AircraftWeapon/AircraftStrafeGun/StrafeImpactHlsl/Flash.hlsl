// Ground_Flash: the bright kick where a round strikes the ground, at the base of its spurt. An additive glow
// sprite (M_SmallArmsMuzzleGlow), one to two frames long; about half the impacts flash, so the band sparkles.
// StrafeShape: x flashes or not, y size, z brightness, w surface kind.

// @section Shape
Particles.StrafeShape = float4(R0, R1, R2, SurfaceKind);
// At least one and a half frames, or a slow frame never draws it; the other half of the impacts do not flash.
Particles.Lifetime = R0 < 0.55 ? max(lerp(0.05, 0.09, R3), Engine.DeltaTime * 1.5) : 0.0;

// @section Place
Particles.Position = OriginValue + GroundUp * (10.0 * S);
float FlashSize = S * lerp(45.0, 85.0, Shape.y) * lerp(1.0, 0.6, T);
Particles.SpriteSize = float2(FlashSize, FlashSize);
Particles.SpriteRotation = 0.0;
Particles.Color = float4(float3(1.9, 1.15, 0.55) * lerp(0.8, 1.25, Shape.z), 1.0 - T);
