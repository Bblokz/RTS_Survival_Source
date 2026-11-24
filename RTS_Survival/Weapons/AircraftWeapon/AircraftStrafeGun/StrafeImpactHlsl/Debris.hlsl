// Ground_Debris: dark clods and grit flung out of the hole, ballistic, landing again around the impact.
// Small camera-facing sprites with the smoke puff atlas at full density so they read as solid specks.
// StrafeShape: x throw direction, y throw distance and size, z throw speed, w surface kind.

// @section Shape
Particles.StrafeShape = float4(R0, R1, R2, SurfaceKind);
Particles.Lifetime = lerp(0.55, 1.1, R3);
Particles.SubImageIndex = floor(R1 * 3.999);

// @section Place
float Angle = Shape.x * 6.2831853;
float Spread = lerp(60.0, 230.0, Shape.y) * S;
float UpSpeed = lerp(380.0, 820.0, Shape.z) * sqrt(S);
float3 Throw = float3(cos(Angle) * Spread, sin(Angle) * Spread, 0.0);
float3 Flight = Throw * AgeSeconds + GroundUp * (UpSpeed * AgeSeconds - 490.0 * AgeSeconds * AgeSeconds);
// Stops where it lands: never below the impact's ground plane.
float AboveGround = max(dot(Flight, GroundUp), 0.0);
Particles.Position = OriginValue + (Flight - GroundUp * dot(Flight, GroundUp)) + GroundUp * AboveGround;
float SpeckSize = S * lerp(9.0, 18.0, Shape.y);
Particles.SpriteSize = float2(SpeckSize, SpeckSize);
Particles.SpriteRotation = Shape.z * 360.0 + AgeSeconds * 400.0;
Particles.Color = float4(DirtColour * 0.42, saturate((1.0 - T) * 3.0));
