// The grit: small dark specks (dry grass, pebbles) whirling fast inside the lower column; they make the spin
// readable from the RTS camera, where the soft dust alone reads as a haze.
// DevilShape: x height (0 to 1), y start angle, z size and puff variation, w radius variation.

// @section Shape
Particles.Lifetime = lerp(1.8, 3.0, R2);
float Arm = floor(R1 * 3.0);
Particles.DevilShape = float4(0.05 + R0 * 0.55, Arm * 2.0944 + (R1 * 3.0 - Arm - 0.5) * 0.6, R2, R3);

// @section Place
float U = saturate(Shape.x + 0.25 * T);
float Height = 1400.0 * S * U;
float Radius = (60.0 + 420.0 * pow(U, 1.7)) * S * lerp(0.6, 1.0, Shape.w);
float Angle = Shape.y + (2.4 * U + (5.0 - 2.5 * U) * (Engine.Time + MO.y * 50.0)) * MO.z;
float2 Offset = float2(cos(Angle), sin(Angle)) * Radius + Heading * Height * 0.22;
Particles.Position = float3(PS.xy + Offset, PS.z + Height);
float Alpha = MO.x * smoothstep(0.0, 0.1, T) * (1.0 - smoothstep(0.7, 1.0, T)) * 0.9;
Particles.Color = float4(DustColor.rgb * 0.25, Alpha);
Particles.SpriteSize = lerp(35.0, 75.0, Shape.z) * S * float2(1.0, 1.0);
Particles.SpriteRotation = Shape.y * 57.29578 + AgeSeconds * 360.0 * MO.z;
