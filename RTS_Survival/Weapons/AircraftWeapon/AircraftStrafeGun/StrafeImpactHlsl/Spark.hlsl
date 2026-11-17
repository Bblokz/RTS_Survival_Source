// Unit_Spark: hot sparks where a round strikes a unit (armour, gun shields, vehicles). Additive glow sprites,
// one to two frames long, scattered a little around the hit.
// StrafeShape: x/y scatter, z size, w surface kind (unused).

// @section Shape
Particles.StrafeShape = float4(R0, R1, R2, SurfaceKind);
Particles.Lifetime = max(lerp(0.05, 0.11, R3), Engine.DeltaTime * 1.5);

// @section Place
float2 Scatter = (float2(Shape.x, Shape.y) - 0.5) * 2.0 * (22.0 * S);
Particles.Position = OriginValue + float3(Scatter.x, Scatter.y, 0.0) + GroundUp * (10.0 * S);
float SparkSize = S * lerp(22.0, 48.0, Shape.z) * lerp(1.0, 0.6, T);
Particles.SpriteSize = float2(SparkSize, SparkSize);
Particles.SpriteRotation = 0.0;
Particles.Color = float4(float3(1.6, 0.85, 0.3) * lerp(0.8, 1.2, Shape.z), 1.0 - T);
