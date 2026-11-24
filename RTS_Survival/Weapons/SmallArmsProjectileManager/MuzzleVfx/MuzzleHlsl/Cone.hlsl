// Flash_Cone: the forward tongue of flame, mesh SM_Muzzle_1 (a 36 cm cone along +X) with M_SmallArmsMuzzleFlash.
// Lives three to five frames; every shot gets its own length, width and roll, so a burst never repeats a flash.
// MuzzleShape: x length variation, y width variation, z roll, w brightness variation.

// @section Shape
// At least one and a half frames: on a slow frame the flash would otherwise die before it is ever drawn.
Particles.Lifetime = max(lerp(0.045, 0.075, R0), Engine.DeltaTime * 1.5);
Particles.MuzzleShape = float4(R0, R1, R2, R3);

// @section Place
// Smoothstep fade over the last 70 % of the life.
float FadeT = saturate((T - 0.3) / 0.7);
float Pop = 1.0 - FadeT * FadeT * (3.0 - 2.0 * FadeT);
float Grow = lerp(0.7, 1.0, saturate(T * 5.0));
float ConeLength = S * lerp(0.55, 1.15, Shape.x) * Grow;
float ConeWidth = S * lerp(0.45, 0.7, Shape.y) * Grow;
Particles.Position = OriginValue - Fwd * (2.0 * S);
Particles.Scale = float3(ConeLength, ConeWidth, ConeWidth);
float HalfRoll = Shape.z * 3.1415927;
float4 RollQuat = float4(sin(HalfRoll), 0.0, 0.0, cos(HalfRoll));
Particles.MeshOrientation = float4(
	ShotQuat.w * RollQuat.xyz + RollQuat.w * ShotQuat.xyz + cross(ShotQuat.xyz, RollQuat.xyz),
	ShotQuat.w * RollQuat.w - dot(ShotQuat.xyz, RollQuat.xyz));
Particles.Color = float4(float3(3.4, 1.05, 0.16) * lerp(0.75, 1.2, Shape.w), Pop);
