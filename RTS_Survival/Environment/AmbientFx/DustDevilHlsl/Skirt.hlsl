// The skirt: a low ring of dust rolling outward over the ground around the foot of the column.
// DevilShape: x start radius (0 to 1), y start angle, z colour and puff variation, w size variation.

// @section Shape
Particles.Lifetime = lerp(2.2, 3.6, R2);
Particles.DevilShape = float4(R0, R1 * 6.2831853, R2, R3);

// @section Place
float Height = (30.0 + 110.0 * T) * S;
float Radius = (110.0 + 330.0 * Shape.x + 260.0 * T) * S;
float Angle = Shape.y + 1.6 * MO.z * AgeSeconds;
Particles.Position = float3(PS.xy + float2(cos(Angle), sin(Angle)) * Radius, PS.z + Height);
float Alpha = MO.x * smoothstep(0.0, 0.15, T) * pow(1.0 - T, 1.5) * 0.4;
Particles.Color = float4(DustColor.rgb * lerp(0.9, 1.05, Shape.z), Alpha);
Particles.SpriteSize = (260.0 + 260.0 * T) * S * lerp(0.8, 1.2, Shape.w) * float2(1.0, 1.0);
Particles.SpriteRotation = Shape.y * 57.29578 + AgeSeconds * 25.0 * MO.z;
