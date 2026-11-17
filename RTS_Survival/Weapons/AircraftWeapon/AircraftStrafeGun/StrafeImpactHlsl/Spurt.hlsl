// Ground_Spurt: the tall dirt geyser a round throws up, the look of a strafe. A lit sprite with the smoke puff
// atlas, aligned to world up (renderer alignment Custom, Particles.SpriteAlignment = up) so the column stays upright
// from any camera. Three particles per impact stack into one column: the lower one is thick and short-lived, the
// upper ones are thin, thrown higher and hang longer.
// StrafeShape: x column part (0 bottom .. 1 top), y size variation, z lean and spin, w surface kind.

// @section Shape
Particles.StrafeShape = float4(R0, R1, R2, SurfaceKind);
Particles.Lifetime = lerp(0.9, 1.7, R0) * lerp(0.9, 1.2, R1) * sqrt(CalibreScale);
Particles.SubImageIndex = floor(R3 * 3.999);
Particles.SpriteAlignment = float3(0.0, 0.0, 1.0);

// @section Place
// Thrown up fast, slows under gravity and drag; the top part is thrown highest.
float ThrowHeight = S * lerp(140.0, 380.0, Shape.x) * lerp(0.8, 1.2, Shape.y);
float Rise = ThrowHeight * (1.0 - exp(-AgeSeconds * lerp(9.0, 5.0, Shape.x)));
float Sag = 60.0 * S * AgeSeconds * AgeSeconds;
float Height = max(Rise - Sag, 0.0);
float2 Lean = (float2(Shape.z, Shape.y) - 0.5) * 2.0 * (12.0 * S);
Particles.Position = OriginValue + GroundUp * (Height * 0.55) + float3(Lean.x, Lean.y, 0.0) * saturate(AgeSeconds * 3.0);
// Wide enough to read from the RTS camera (30-50 m away): a column of a few soldiers' width, widening as it rises.
float Width = S * lerp(75.0, 40.0, Shape.x) * lerp(0.85, 1.25, Shape.y) * (1.0 + AgeSeconds * 1.2);
float Tall = max(Height * 1.15, Width * 1.4);
Particles.SpriteSize = float2(Width, Tall);
Particles.SpriteRotation = (Shape.z - 0.5) * 16.0;
// Pops in within a frame or two, thins out as it rises and falls back.
float FadeInT = saturate(T / 0.04);
float Alpha = FadeInT * pow(1.0 - T, 1.6) * lerp(0.95, 0.7, Shape.x);
Particles.Color = float4(DirtColour * lerp(0.85, 1.1, Shape.y), Alpha);
