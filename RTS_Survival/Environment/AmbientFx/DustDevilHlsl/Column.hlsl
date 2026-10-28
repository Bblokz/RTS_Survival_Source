// The column: dust lifted at the foot spirals up, widens and thins out; the top leans with the drift and sways.
// DevilShape: x start height (0 to 1), y start angle (on one of three spiral arms), z size and puff variation,
// w radius variation. The arms twist with height, so the spin reads as a spiral from the RTS camera.

// @section Shape
Particles.Lifetime = lerp(2.6, 4.4, R2);
float Arm = floor(R1 * 3.0);
Particles.DevilShape = float4(R0 * R0 * 0.3, Arm * 2.0944 + (R1 * 3.0 - Arm - 0.5) * 0.9, R2, R3);

// @section Place
float U = saturate(Shape.x + (1.0 - Shape.x) * pow(T, 0.75));
float Height = 1400.0 * S * U;
float Radius = (70.0 + 480.0 * pow(U, 1.7)) * S * lerp(0.75, 1.25, Shape.w);
// World time, not particle age: every particle of an arm turns together, so the arms stay coherent spirals.
float Angle = Shape.y + (2.4 * U + (2.8 - 1.4 * U) * (Engine.Time + MO.y * 50.0)) * MO.z;
float Sway = sin(Engine.Time * 0.6 + MO.y * 6.2831853) * Height * 0.07;
float2 Offset = float2(cos(Angle), sin(Angle)) * Radius + Heading * Height * 0.22 + Side * Sway;
Particles.Position = float3(PS.xy + Offset, PS.z + Height);
float Alpha = MO.x * smoothstep(0.0, 0.12, T) * (1.0 - smoothstep(0.55, 1.0, T)) * lerp(0.55, 0.16, U);
Particles.Color = float4(DustColor.rgb * lerp(0.85, 1.1, Shape.z), Alpha);
Particles.SpriteSize = (130.0 + 380.0 * U) * S * lerp(0.8, 1.2, Shape.z) * float2(1.0, 1.0);
Particles.SpriteRotation = Shape.y * 57.29578 + AgeSeconds * 40.0 * MO.z;
